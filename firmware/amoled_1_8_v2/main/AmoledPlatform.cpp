#include "AmoledPlatform.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace AmoledV2 {
namespace {

// The shared game model stores brightness as an 8-bit level, while the
// Waveshare BSP accepts a percentage.
int brightnessPercent(uint8_t value) {
    return (static_cast<unsigned>(value) * 100U + 127U) / 255U;
}

constexpr char RESOURCE_BASE_PATH[] = "/assets";
constexpr char RESOURCE_PARTITION[] = "resources";
constexpr gpio_num_t TOUCH_WAKE_GPIO = GPIO_NUM_21;
constexpr uint8_t PEER_QUEUE_CAPACITY = 8;
constexpr uint8_t AUDIO_QUEUE_CAPACITY = 4;

Platform::PeerPacket peerQueue[PEER_QUEUE_CAPACITY];
volatile uint8_t peerQueueHead = 0;
volatile uint8_t peerQueueTail = 0;
portMUX_TYPE peerQueueMux = portMUX_INITIALIZER_UNLOCKED;

void receivePeerPacket(const esp_now_recv_info_t* info,
                       const uint8_t* data,
                       int length) {
    if (!info || !info->src_addr || !data || length <= 0 ||
        static_cast<size_t>(length) > Platform::PeerPacket::MAX_PAYLOAD_BYTES) {
        return;
    }
    portENTER_CRITICAL(&peerQueueMux);
    uint8_t next = static_cast<uint8_t>(
        (peerQueueHead + 1U) % PEER_QUEUE_CAPACITY);
    if (next != peerQueueTail) {
        Platform::PeerPacket& packet = peerQueue[peerQueueHead];
        std::memcpy(packet.source, info->src_addr, sizeof(packet.source));
        std::memcpy(packet.payload, data, static_cast<size_t>(length));
        packet.length = static_cast<size_t>(length);
        peerQueueHead = next;
    }
    portEXIT_CRITICAL(&peerQueueMux);
}

void resetPeerQueue() {
    portENTER_CRITICAL(&peerQueueMux);
    peerQueueHead = 0;
    peerQueueTail = 0;
    portEXIT_CRITICAL(&peerQueueMux);
}

class SpiffsResourceFile final : public Platform::IResourceFile {
public:
    explicit SpiffsResourceFile(FILE* file) : file_(file) {
        if (!file_) return;
        long original = std::ftell(file_);
        if (original < 0 || std::fseek(file_, 0, SEEK_END) != 0) return;
        long length = std::ftell(file_);
        if (length >= 0) size_ = static_cast<size_t>(length);
        std::fseek(file_, original, SEEK_SET);
    }

    bool valid() const override { return file_ != nullptr; }
    size_t size() const override { return valid() ? size_ : 0; }
    size_t position() const override {
        if (!valid()) return 0;
        long value = std::ftell(file_);
        return value >= 0 ? static_cast<size_t>(value) : 0;
    }
    size_t read(void* output, size_t length) override {
        return valid() && output ? std::fread(output, 1, length, file_) : 0;
    }
    bool seek(size_t position) override {
        return valid() && position <= size_ &&
               std::fseek(file_, static_cast<long>(position), SEEK_SET) == 0;
    }
    void close() override {
        if (!file_) return;
        std::fclose(file_);
        file_ = nullptr;
    }

private:
    FILE* file_ = nullptr;
    size_t size_ = 0;
};

}  // namespace

AmoledPlatform::AmoledPlatform()
    : services_{*this, *this, *this, *this, *this, *this, *this, *this,
                *this, *this, *this, *this} {}

AmoledPlatform& AmoledPlatform::instance() {
    static AmoledPlatform platform;
    return platform;
}

Platform::Services& AmoledPlatform::serviceBundle() {
    return services_;
}

void bindAmoledPlatform() {
    Platform::bind(AmoledPlatform::instance().serviceBundle());
}

bool AmoledPlatform::begin() {
    if (initialized_) return true;
    if (bsp_i2c_init() != ESP_OK) return false;
    audioQueue_ = xQueueCreate(AUDIO_QUEUE_CAPACITY, sizeof(AudioChunk*));
    audioMutex_ = xSemaphoreCreateMutex();
    if (!audioQueue_ || !audioMutex_ ||
        xTaskCreatePinnedToCore(audioTaskEntry, "amoled_audio", 4096, this, 2,
                                nullptr, 1) != pdPASS) {
        if (audioQueue_) vQueueDelete(audioQueue_);
        if (audioMutex_) vSemaphoreDelete(audioMutex_);
        audioQueue_ = nullptr;
        audioMutex_ = nullptr;
        return false;
    }
    initialized_ = true;
    return true;
}

void AmoledPlatform::audioTaskEntry(void* context) {
    static_cast<AmoledPlatform*>(context)->audioTask();
    vTaskDelete(nullptr);
}

void AmoledPlatform::audioTask() {
    AudioChunk* chunk = nullptr;
    int16_t pcm[256];
    while (true) {
        // Codec setup and writes stay on this task so UI-side audio submission
        // never waits for an active PCM chunk to finish.
        if (xQueueReceive(audioQueue_, &chunk, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        xSemaphoreTake(audioMutex_, portMAX_DELAY);
        if (chunk->generation == audioGeneration_.load() &&
            !microphoneMode_) {
            if (!speakerCodec_) speakerCodec_ = bsp_audio_codec_speaker_init();
            if (speakerOpen_ && speakerSampleRate_ != chunk->sampleRate) {
                esp_codec_dev_close(speakerCodec_);
                speakerOpen_ = false;
            }
            if (speakerCodec_ && !speakerOpen_) {
                esp_codec_dev_sample_info_t sampleInfo = {
                    .bits_per_sample = 16,
                    .channel = 1,
                    .channel_mask = 0,
                    .sample_rate = chunk->sampleRate,
                    .mclk_multiple = 0,
                };
                speakerOpen_ =
                    esp_codec_dev_open(speakerCodec_, &sampleInfo) ==
                    ESP_CODEC_DEV_OK;
                if (speakerOpen_) speakerSampleRate_ = chunk->sampleRate;
            }
        }
        if (chunk->generation == audioGeneration_.load() &&
            speakerCodec_ && speakerOpen_ && !microphoneMode_) {
            audioPlaying_ = true;
            applySpeakerVolumeLocked(chunk->channel);
            size_t offset = 0;
            while (offset < chunk->sampleCount &&
                   chunk->generation == audioGeneration_.load()) {
                size_t count = chunk->sampleCount - offset;
                if (count > sizeof(pcm) / sizeof(pcm[0])) {
                    count = sizeof(pcm) / sizeof(pcm[0]);
                }
                for (size_t index = 0; index < count; ++index) {
                    pcm[index] = static_cast<int16_t>(
                        (static_cast<int>(chunk->samples[offset + index]) -
                         128) << 8);
                }
                if (esp_codec_dev_write(
                        speakerCodec_, pcm,
                        static_cast<int>(count * sizeof(pcm[0]))) < 0) {
                    break;
                }
                offset += count;
                // The codec write can complete without blocking when DMA has
                // room. Always give CPU1's idle task a scheduling point.
                vTaskDelay(1);
            }
            audioPlaying_ = false;
        }
        if (chunk->channel < Platform::IAudioDevice::CHANNEL_COUNT &&
            queuedPcm_[chunk->channel].load() > 0) {
            queuedPcm_[chunk->channel].fetch_sub(1);
        }
        heap_caps_free(chunk->samples);
        heap_caps_free(chunk);
        chunk = nullptr;
        xSemaphoreGive(audioMutex_);
    }
}

void AmoledPlatform::clearAudioQueue() {
    if (!audioQueue_) return;
    AudioChunk* chunk = nullptr;
    while (xQueueReceive(audioQueue_, &chunk, 0) == pdTRUE) {
        if (chunk->channel < Platform::IAudioDevice::CHANNEL_COUNT &&
            queuedPcm_[chunk->channel].load() > 0) {
            queuedPcm_[chunk->channel].fetch_sub(1);
        }
        heap_caps_free(chunk->samples);
        heap_caps_free(chunk);
        chunk = nullptr;
    }
}

void AmoledPlatform::applySpeakerVolumeLocked(uint8_t channel) {
    if (!speakerCodec_ || !speakerOpen_) return;
    uint32_t channelVolume = channel < Platform::IAudioDevice::CHANNEL_COUNT
        ? channelVolumes_[channel] : 100;
    uint8_t effective = static_cast<uint8_t>(
        (static_cast<uint32_t>(volume_) * channelVolume) / 100U);
    esp_codec_dev_set_out_vol(speakerCodec_, effective);
}

uint32_t AmoledPlatform::millis() const {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

uint32_t AmoledPlatform::micros() const {
    return static_cast<uint32_t>(esp_timer_get_time());
}

void AmoledPlatform::sleepMs(uint32_t durationMs) {
    vTaskDelay(pdMS_TO_TICKS(durationMs));
}

void AmoledPlatform::write(const char* text, size_t length) {
    if (text && length > 0) std::fwrite(text, 1, length, stdout);
}

void AmoledPlatform::flush() { std::fflush(stdout); }
void AmoledPlatform::update() {}
bool AmoledPlatform::pressed(Platform::InputButton) const { return false; }
Platform::FrameBuffer565 AmoledPlatform::frameBuffer() { return {}; }
void AmoledPlatform::present() {}
void AmoledPlatform::setBrightness(uint8_t value) {
    brightness_ = value;
    if (initialized_) {
        bsp_display_brightness_set(brightnessPercent(value));
    }
}
uint8_t AmoledPlatform::brightness() const { return brightness_; }
void AmoledPlatform::sleep() {}

void AmoledPlatform::setVolume(uint8_t percent) {
    volume_ = percent > 100 ? 100 : percent;
    if (audioMutex_) {
        xSemaphoreTake(audioMutex_, portMAX_DELAY);
        applySpeakerVolumeLocked(0);
        xSemaphoreGive(audioMutex_);
    }
}
uint8_t AmoledPlatform::volume() const { return volume_; }
bool AmoledPlatform::playPcmU8(const uint8_t* data, size_t sampleCount,
                               uint32_t sampleRate) {
    return playPcmU8Channel(data, sampleCount, sampleRate, 0, true);
}
bool AmoledPlatform::playPcmU8Channel(const uint8_t* data, size_t sampleCount,
                                      uint32_t sampleRate, uint8_t channel,
                                      bool stopCurrent) {
    if (!initialized_ || !data || sampleCount == 0 || sampleRate == 0 ||
        channel >= Platform::IAudioDevice::CHANNEL_COUNT || volume_ == 0 ||
        !audioQueue_ || !audioMutex_) {
        return false;
    }
    if (microphoneMode_) endMicrophone();
    AudioChunk* chunk = static_cast<AudioChunk*>(
        heap_caps_calloc(1, sizeof(AudioChunk), MALLOC_CAP_8BIT));
    if (!chunk) return false;
    chunk->samples = static_cast<uint8_t*>(
        heap_caps_malloc(sampleCount, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!chunk->samples) {
        heap_caps_free(chunk);
        return false;
    }
    std::memcpy(chunk->samples, data, sampleCount);
    chunk->channel = channel;
    chunk->sampleRate = sampleRate;
    chunk->sampleCount = sampleCount;
    if (stopCurrent) {
        chunk->generation = audioGeneration_.fetch_add(1) + 1;
        clearAudioQueue();
    } else {
        chunk->generation = audioGeneration_.load();
    }
    queuedPcm_[channel].fetch_add(1);
    if (xQueueSend(audioQueue_, &chunk, 0) != pdTRUE) {
        queuedPcm_[channel].fetch_sub(1);
        heap_caps_free(chunk->samples);
        heap_caps_free(chunk);
        return false;
    }
    return true;
}
void AmoledPlatform::setChannelVolume(uint8_t channel, uint8_t percent) {
    if (channel >= Platform::IAudioDevice::CHANNEL_COUNT) return;
    channelVolumes_[channel] = percent > 100 ? 100 : percent;
}
bool AmoledPlatform::playing() const {
    return audioPlaying_.load() || queuedPcm_[0].load() > 0 ||
           queuedPcm_[1].load() > 0 || queuedPcm_[2].load() > 0;
}
uint8_t AmoledPlatform::queuedPcm(uint8_t channel) const {
    return channel < Platform::IAudioDevice::CHANNEL_COUNT
        ? queuedPcm_[channel].load() : 0;
}
void AmoledPlatform::stop() {
    if (!audioMutex_) return;
    audioGeneration_.fetch_add(1);
    clearAudioQueue();
    xSemaphoreTake(audioMutex_, portMAX_DELAY);
    if (speakerCodec_ && speakerOpen_) esp_codec_dev_close(speakerCodec_);
    speakerOpen_ = false;
    audioPlaying_.store(false);
    xSemaphoreGive(audioMutex_);
}
void AmoledPlatform::stopChannel(uint8_t channel) {
    if (channel >= Platform::IAudioDevice::CHANNEL_COUNT) return;
    audioGeneration_.fetch_add(1);
    clearAudioQueue();
}
bool AmoledPlatform::beginMicrophone() {
    if (!initialized_) return false;
    stop();
    if (!microphoneCodec_) {
        microphoneCodec_ = bsp_audio_codec_microphone_init();
    }
    if (!microphoneCodec_) return false;
    if (!microphoneOpen_) {
        esp_codec_dev_sample_info_t sampleInfo = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = 16000,
            .mclk_multiple = 0,
        };
        if (esp_codec_dev_open(microphoneCodec_, &sampleInfo) != ESP_CODEC_DEV_OK) {
            return false;
        }
        microphoneSampleRate_ = 16000;
        microphoneOpen_ = true;
    }
    microphoneMode_ = true;
    return true;
}
void AmoledPlatform::endMicrophone() {
    microphoneMode_ = false;
    if (microphoneCodec_ && microphoneOpen_) {
        esp_codec_dev_close(microphoneCodec_);
    }
    microphoneOpen_ = false;
}
bool AmoledPlatform::recordMicrophone(int16_t* data, size_t sampleCount,
                                      uint32_t sampleRate) {
    if (!microphoneMode_ || !microphoneCodec_ || !data || sampleCount == 0 ||
        sampleRate == 0) return false;
    if (microphoneOpen_ && microphoneSampleRate_ != sampleRate) {
        esp_codec_dev_close(microphoneCodec_);
        microphoneOpen_ = false;
    }
    if (!microphoneOpen_) {
        esp_codec_dev_sample_info_t sampleInfo = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = sampleRate,
            .mclk_multiple = 0,
        };
        if (esp_codec_dev_open(microphoneCodec_, &sampleInfo) != ESP_CODEC_DEV_OK) {
            return false;
        }
        microphoneSampleRate_ = sampleRate;
        microphoneOpen_ = true;
    }
    return esp_codec_dev_read(
               microphoneCodec_, data,
               static_cast<int>(sampleCount * sizeof(int16_t))) >= 0;
}
bool AmoledPlatform::microphoneRecording() const { return microphoneMode_; }
bool AmoledPlatform::microphoneActive() const { return microphoneMode_; }

bool AmoledPlatform::readAcceleration(float& x, float& y, float& z) {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    return false;
}

int AmoledPlatform::batteryLevel() {
    // The V2 BSP does not expose a calibrated PMU/ADC channel through the
    // common board API yet. Keep this unknown until the driver is integrated.
    return -1;
}
Platform::PowerCapabilities AmoledPlatform::capabilities() const {
    // V1 has the power hardware, but this firmware does not yet ship a
    // calibrated AXP2101/RTC driver. Report only the sleep path we can verify.
    return {false, false, true, true};
}
Platform::WakeReason AmoledPlatform::wakeReason() const {
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: return Platform::WakeReason::TIMER;
    case ESP_SLEEP_WAKEUP_UNDEFINED: return Platform::WakeReason::NORMAL;
    default: return Platform::WakeReason::EXTERNAL_SIGNAL;
    }
}
uint32_t AmoledPlatform::hardwareRandom() { return esp_random(); }
size_t AmoledPlatform::externalMemorySize() const { return esp_psram_get_size(); }
size_t AmoledPlatform::externalMemoryFree() const {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
void AmoledPlatform::restart() { esp_restart(); }
void AmoledPlatform::enterDeepSleep(uint64_t timerWakeUs,
                                    bool wakeOnSecondaryButton) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (wakeOnSecondaryButton) {
        gpio_set_direction(TOUCH_WAKE_GPIO, GPIO_MODE_INPUT);
        gpio_set_pull_mode(TOUCH_WAKE_GPIO, GPIO_PULLUP_ONLY);
        esp_sleep_enable_ext1_wakeup(
            1ULL << static_cast<uint8_t>(TOUCH_WAKE_GPIO),
            ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if (timerWakeUs > 0) esp_sleep_enable_timer_wakeup(timerWakeUs);
    esp_deep_sleep_start();
}

bool AmoledPlatform::initialize() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) return false;
        result = nvs_flash_init();
    }
    return result == ESP_OK;
}

size_t AmoledPlatform::blobSize(const char* nameSpace, const char* key) {
    if (!nameSpace || !key) return 0;
    nvs_handle_t handle = 0;
    if (nvs_open(nameSpace, NVS_READONLY, &handle) != ESP_OK) return 0;
    size_t length = 0;
    esp_err_t result = nvs_get_blob(handle, key, nullptr, &length);
    nvs_close(handle);
    return result == ESP_OK ? length : 0;
}

bool AmoledPlatform::readBlob(const char* nameSpace, const char* key,
                              void* output, size_t length) {
    if (!nameSpace || !key || !output || length == 0) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(nameSpace, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t storedLength = length;
    esp_err_t result = nvs_get_blob(handle, key, output, &storedLength);
    nvs_close(handle);
    return result == ESP_OK && storedLength == length;
}

bool AmoledPlatform::writeBlob(const char* nameSpace, const char* key,
                               const void* data, size_t length) {
    if (!nameSpace || !key || !data || length == 0) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(nameSpace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_set_blob(handle, key, data, length);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool AmoledPlatform::removeBlob(const char* nameSpace, const char* key) {
    if (!nameSpace || !key) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(nameSpace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_erase_key(handle, key);
    if (result == ESP_ERR_NVS_NOT_FOUND) result = ESP_OK;
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool AmoledPlatform::clearNamespace(const char* nameSpace) {
    if (!nameSpace) return false;
    nvs_handle_t handle = 0;
    if (nvs_open(nameSpace, NVS_READWRITE, &handle) != ESP_OK) return false;
    esp_err_t result = nvs_erase_all(handle);
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result == ESP_OK;
}

bool AmoledPlatform::mount() {
    if (resourcesMounted_) return true;
    esp_vfs_spiffs_conf_t config{};
    config.base_path = RESOURCE_BASE_PATH;
    config.partition_label = RESOURCE_PARTITION;
    config.max_files = 8;
    config.format_if_mount_failed = false;
    resourcesMounted_ = esp_vfs_spiffs_register(&config) == ESP_OK;
    return resourcesMounted_;
}

size_t AmoledPlatform::totalBytes() const {
    size_t total = 0;
    size_t used = 0;
    return resourcesMounted_ &&
                   esp_spiffs_info(RESOURCE_PARTITION, &total, &used) == ESP_OK
               ? total
               : 0;
}

size_t AmoledPlatform::usedBytes() const {
    size_t total = 0;
    size_t used = 0;
    return resourcesMounted_ &&
                   esp_spiffs_info(RESOURCE_PARTITION, &total, &used) == ESP_OK
               ? used
               : 0;
}

Platform::ResourceFile AmoledPlatform::open(const char* path) {
    if (!resourcesMounted_ || !path || path[0] != '/' ||
        std::strstr(path, "..") != nullptr) {
        return {};
    }
    char fullPath[192];
    int written = std::snprintf(fullPath, sizeof(fullPath), "%s%s",
                                RESOURCE_BASE_PATH, path);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(fullPath)) return {};
    FILE* file = std::fopen(fullPath, "rb");
    if (!file) return {};
    auto* implementation = new (std::nothrow) SpiffsResourceFile(file);
    if (!implementation) {
        std::fclose(file);
        return {};
    }
    return Platform::ResourceFile(implementation);
}

void* AmoledPlatform::allocate(size_t bytes, bool preferExternal) {
    if (bytes == 0) return nullptr;
    if (preferExternal) {
        void* memory = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory) return memory;
    }
    return heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
}

void AmoledPlatform::release(void* memory) { heap_caps_free(memory); }
size_t AmoledPlatform::externalFree() const { return externalMemoryFree(); }

bool AmoledPlatform::enable() {
    if (peerTransportActive_) return true;
    if (!wifiInitialized_) {
        if (esp_netif_init() != ESP_OK &&
            esp_netif_init() != ESP_ERR_INVALID_STATE) return false;
        esp_err_t eventResult = esp_event_loop_create_default();
        if (eventResult != ESP_OK && eventResult != ESP_ERR_INVALID_STATE) return false;
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        esp_err_t wifiInit = esp_wifi_init(&config);
        if (wifiInit != ESP_OK && wifiInit != ESP_ERR_WIFI_STATE) return false;
        wifiInitialized_ = true;
    }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_start() != ESP_OK ||
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM) != ESP_OK) {
        return false;
    }
    if (esp_now_init() != ESP_OK ||
        esp_now_register_recv_cb(receivePeerPacket) != ESP_OK) {
        esp_now_deinit();
        return false;
    }
    resetPeerQueue();
    peerTransportActive_ = true;
    return true;
}
void AmoledPlatform::end() {
    if (!peerTransportActive_) return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    // Wi-Fi is shared with the remote ESP-Claw runtime. ESP-NOW may be
    // disabled here, but the station must remain alive for remote chat.
    peerTransportActive_ = false;
    resetPeerQueue();
}
bool AmoledPlatform::active() const { return peerTransportActive_; }
bool AmoledPlatform::send(const uint8_t destination[6], const void* data,
                          size_t length) {
    if (!peerTransportActive_ || !destination || !data || length == 0 ||
        length > Platform::PeerPacket::MAX_PAYLOAD_BYTES) return false;
    if (!esp_now_is_peer_exist(destination)) {
        esp_now_peer_info_t peer{};
        std::memcpy(peer.peer_addr, destination, sizeof(peer.peer_addr));
        peer.channel = 0;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        if (esp_now_add_peer(&peer) != ESP_OK) return false;
    }
    return esp_now_send(destination, static_cast<const uint8_t*>(data), length) ==
           ESP_OK;
}
bool AmoledPlatform::receive(Platform::PeerPacket& packet) {
    if (!peerTransportActive_) return false;
    portENTER_CRITICAL(&peerQueueMux);
    if (peerQueueTail == peerQueueHead) {
        portEXIT_CRITICAL(&peerQueueMux);
        return false;
    }
    packet = peerQueue[peerQueueTail];
    peerQueueTail = static_cast<uint8_t>(
        (peerQueueTail + 1U) % PEER_QUEUE_CAPACITY);
    portEXIT_CRITICAL(&peerQueueMux);
    return true;
}

}  // namespace AmoledV2
