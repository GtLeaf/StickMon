#include "platform/m5stick_s3/M5StickS3Platform.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <driver/rtc_io.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <new>

namespace {

LGFX_Sprite gFrameBuffer;

constexpr uint8_t hardwareVolumeForPercent(uint8_t percent) {
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(percent) * 255U + 50U) / 100U);
}

static_assert(hardwareVolumeForPercent(0) == 0,
              "Muted volume must map to zero");
static_assert(hardwareVolumeForPercent(50) == 128,
              "Half volume mapping changed");
static_assert(hardwareVolumeForPercent(100) == 255,
              "Full volume must map to 255");

constexpr gpio_num_t SECONDARY_WAKE_GPIO = GPIO_NUM_12;
constexpr const char* RESOURCE_BASE_PATH = "/assets";
constexpr const char* RESOURCE_PARTITION = "littlefs";
constexpr uint8_t RESOURCE_MAX_OPEN_FILES = 8;

class LittleResourceFile final : public Platform::IResourceFile {
public:
    explicit LittleResourceFile(fs::File file) : file_(std::move(file)) {}
    bool valid() const override { return file_ && !file_.isDirectory(); }
    size_t size() const override { return valid() ? file_.size() : 0; }
    size_t position() const override { return valid() ? file_.position() : 0; }
    size_t read(void* output, size_t length) override {
        return valid() && output
            ? file_.read(static_cast<uint8_t*>(output), length)
            : 0;
    }
    bool seek(size_t position) override {
        return valid() && file_.seek(position, fs::SeekSet);
    }
    void close() override { file_.close(); }

private:
    mutable fs::File file_;
};

constexpr uint8_t PEER_QUEUE_CAPACITY = 8;
Platform::PeerPacket gPeerQueue[PEER_QUEUE_CAPACITY];
volatile uint8_t gPeerQueueHead = 0;
volatile uint8_t gPeerQueueTail = 0;
portMUX_TYPE gPeerQueueMux = portMUX_INITIALIZER_UNLOCKED;

void receivePeerPacket(const uint8_t* mac, const uint8_t* data, int length) {
    if (!mac || !data || length <= 0 ||
        static_cast<size_t>(length) > Platform::PeerPacket::MAX_PAYLOAD_BYTES) {
        return;
    }
    portENTER_CRITICAL(&gPeerQueueMux);
    uint8_t next = static_cast<uint8_t>((gPeerQueueHead + 1) % PEER_QUEUE_CAPACITY);
    if (next != gPeerQueueTail) {
        Platform::PeerPacket& packet = gPeerQueue[gPeerQueueHead];
        memcpy(packet.source, mac, sizeof(packet.source));
        memcpy(packet.payload, data, static_cast<size_t>(length));
        packet.length = static_cast<size_t>(length);
        gPeerQueueHead = next;
    }
    portEXIT_CRITICAL(&gPeerQueueMux);
}

}  // namespace

M5StickS3Platform::M5StickS3Platform()
    : services_{*this, *this, *this, *this, *this, *this, *this, *this,
                *this, *this, *this, *this} {}

M5StickS3Platform& M5StickS3Platform::instance() {
    static M5StickS3Platform platform;
    return platform;
}

Platform::Services& M5StickS3Platform::serviceBundle() {
    return services_;
}

void bindM5StickS3Platform() {
    Platform::bind(M5StickS3Platform::instance().serviceBundle());
}

bool M5StickS3Platform::begin() {
    if (initialized_) return true;

    auto cfg = M5.config();
    // StickMon targets M5StickS3 only. Pin the board so M5GFX does not run
    // broad hardware autodetection during startup.
    cfg.fallback_board = m5::board_t::board_M5StickS3;
    cfg.internal_spk = true;
    cfg.internal_imu = true;
    cfg.internal_mic = true;
    cfg.external_rtc = false;
    M5.begin(cfg);

    M5.Mic.end();
    if (!M5.Speaker.isRunning()) M5.Speaker.begin();
    WiFi.mode(WIFI_OFF);
    btStop();

    M5.Display.setRotation(1);
    M5.Display.setBrightness(128);

    gFrameBuffer.setColorDepth(16);
    gFrameBuffer.setPsram(psramFound());
    if (!gFrameBuffer.createSprite(Platform::LOGICAL_DISPLAY_W,
                                   Platform::LOGICAL_DISPLAY_H)) {
        Serial.println("[Platform] ERROR: createSprite failed");
        return false;
    }
    gFrameBuffer.setSwapBytes(true);
    setVolume(audioVolume_);
    initialized_ = true;
    Serial.println("[Platform] M5StickS3 ready");
    return true;
}

uint32_t M5StickS3Platform::millis() const {
    return M5.millis();
}

uint32_t M5StickS3Platform::micros() const {
    return ::micros();
}

void M5StickS3Platform::sleepMs(uint32_t durationMs) {
    delay(durationMs);
}

void M5StickS3Platform::write(const char* text, size_t length) {
    if (text && length > 0) Serial.write(
        reinterpret_cast<const uint8_t*>(text), length);
}

void M5StickS3Platform::flush() {
    Serial.flush();
}

void M5StickS3Platform::update() {
    M5.update();
}

bool M5StickS3Platform::pressed(Platform::InputButton button) const {
    switch (button) {
    case Platform::InputButton::PRIMARY:
        return M5.BtnA.isPressed();
    case Platform::InputButton::SECONDARY:
        return M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
    default:
        return false;
    }
}

Platform::FrameBuffer565 M5StickS3Platform::frameBuffer() {
    Platform::FrameBuffer565 result;
    result.pixels = static_cast<uint16_t*>(gFrameBuffer.getBuffer());
    result.width = Platform::LOGICAL_DISPLAY_W;
    result.height = Platform::LOGICAL_DISPLAY_H;
    result.byteSwapped = true;
    return result;
}

void M5StickS3Platform::present() {
    gFrameBuffer.pushSprite(&M5.Display, 0, 0);
}

void M5StickS3Platform::setBrightness(uint8_t value) {
    M5.Display.setBrightness(value);
}

uint8_t M5StickS3Platform::brightness() const {
    return M5.Display.getBrightness();
}

void M5StickS3Platform::sleep() {
    M5.Display.sleep();
    M5.Display.waitDisplay();
}

void M5StickS3Platform::setVolume(uint8_t percent) {
    audioVolume_ = percent > 100 ? 100 : percent;
    M5.Speaker.setVolume(hardwareVolumeForPercent(audioVolume_));
}

uint8_t M5StickS3Platform::volume() const {
    return audioVolume_;
}

bool M5StickS3Platform::playPcmU8(const uint8_t* data, size_t sampleCount,
                                  uint32_t sampleRate) {
    return playPcmU8Channel(data, sampleCount, sampleRate, 0, true);
}

bool M5StickS3Platform::playPcmU8Channel(const uint8_t* data,
                                         size_t sampleCount,
                                         uint32_t sampleRate,
                                         uint8_t channel,
                                         bool stopCurrent) {
    if (!initialized_ || audioVolume_ == 0 || !data || sampleCount == 0 ||
        sampleRate == 0 || channel >= Platform::IAudioDevice::CHANNEL_COUNT ||
        microphoneMode_ || !M5.Speaker.isEnabled()) {
        return false;
    }
    return M5.Speaker.playRaw(
        data, sampleCount, sampleRate, false, 1, channel, stopCurrent);
}

void M5StickS3Platform::setChannelVolume(uint8_t channel, uint8_t percent) {
    if (!initialized_ || channel >= Platform::IAudioDevice::CHANNEL_COUNT) return;
    uint8_t clamped = percent > 100 ? 100 : percent;
    M5.Speaker.setChannelVolume(
        channel, static_cast<uint8_t>((clamped * 255U + 50U) / 100U));
}

bool M5StickS3Platform::playing() const {
    return initialized_ && M5.Speaker.isPlaying();
}

uint8_t M5StickS3Platform::queuedPcm(uint8_t channel) const {
    return initialized_ && channel < Platform::IAudioDevice::CHANNEL_COUNT
        ? static_cast<uint8_t>(M5.Speaker.isPlaying(channel))
        : 0;
}

void M5StickS3Platform::stop() {
    if (!initialized_) return;
    M5.Speaker.stop();
    while (M5.Speaker.isPlaying()) {
        delay(1);
    }
}

void M5StickS3Platform::stopChannel(uint8_t channel) {
    if (initialized_ && channel < Platform::IAudioDevice::CHANNEL_COUNT) {
        M5.Speaker.stop(channel);
        while (M5.Speaker.isPlaying(channel) != 0) {
            delay(1);
        }
    }
}

bool M5StickS3Platform::beginMicrophone() {
    if (!initialized_) return false;
    if (microphoneMode_) return M5.Mic.isRunning();
    M5.Speaker.stop(0);
    M5.Speaker.end();
    delay(2);
    if (!M5.Mic.begin()) {
        M5.Speaker.begin();
        setVolume(audioVolume_);
        return false;
    }
    microphoneMode_ = true;
    Serial.println("[Audio] mode=microphone");
    return true;
}

void M5StickS3Platform::endMicrophone() {
    if (!initialized_ || !microphoneMode_) return;
    while (static_cast<int32_t>(millis() - microphoneRecordingUntilMs_) < 0 ||
           M5.Mic.isRecording()) {
        delay(1);
    }
    M5.Mic.end();
    delay(2);
    M5.Speaker.begin();
    setVolume(audioVolume_);
    microphoneMode_ = false;
    microphoneRecordingUntilMs_ = 0;
    Serial.println("[Audio] mode=speaker");
}

bool M5StickS3Platform::recordMicrophone(
    int16_t* data, size_t sampleCount, uint32_t sampleRate) {
    if (!microphoneMode_ || !data || sampleCount == 0 || sampleRate == 0 ||
        !M5.Mic.record(data, sampleCount, sampleRate, false)) {
        return false;
    }
    uint32_t durationMs = static_cast<uint32_t>(
        (static_cast<uint64_t>(sampleCount) * 1000ULL + sampleRate - 1) /
        sampleRate);
    microphoneRecordingUntilMs_ = millis() + durationMs;
    return true;
}

bool M5StickS3Platform::microphoneRecording() const {
    return initialized_ && microphoneMode_ && M5.Mic.isRecording() != 0;
}

bool M5StickS3Platform::microphoneActive() const {
    return microphoneMode_;
}

bool M5StickS3Platform::readAcceleration(float& x, float& y, float& z) {
    return initialized_ && M5.Imu.getAccel(&x, &y, &z);
}

int M5StickS3Platform::batteryLevel() {
    return M5.Power.getBatteryLevel();
}

Platform::WakeReason M5StickS3Platform::wakeReason() const {
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER:
        return Platform::WakeReason::TIMER;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        return Platform::WakeReason::NORMAL;
    default:
        return Platform::WakeReason::EXTERNAL_SIGNAL;
    }
}

uint32_t M5StickS3Platform::hardwareRandom() {
    return esp_random();
}

size_t M5StickS3Platform::externalMemorySize() const {
    return ESP.getPsramSize();
}

size_t M5StickS3Platform::externalMemoryFree() const {
    return ESP.getFreePsram();
}

void M5StickS3Platform::restart() {
    ESP.restart();
}

void M5StickS3Platform::enterDeepSleep(
    uint64_t timerWakeUs, bool wakeOnSecondaryButton) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    if (wakeOnSecondaryButton) {
        rtc_gpio_pullup_en(SECONDARY_WAKE_GPIO);
        rtc_gpio_pulldown_dis(SECONDARY_WAKE_GPIO);
        esp_sleep_enable_ext1_wakeup(
            1ULL << static_cast<uint8_t>(SECONDARY_WAKE_GPIO),
            ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if (timerWakeUs > 0) {
        esp_sleep_enable_timer_wakeup(timerWakeUs);
    }
    esp_deep_sleep_start();
}

bool M5StickS3Platform::initialize() {
    Preferences preferences;
    bool ready = preferences.begin("stickmon", false);
    preferences.end();
    return ready;
}

size_t M5StickS3Platform::blobSize(const char* nameSpace, const char* key) {
    Preferences preferences;
    if (!nameSpace || !key || !preferences.begin(nameSpace, true)) return 0;
    // getBytesLength() error-logs through the Arduino printf path for missing
    // keys; that printf has a large stack footprint and once overflowed the
    // 8KB loopTask during legacy save migration. Probe the key type first so
    // absent slots stay silent.
    if (preferences.getType(key) != PT_BLOB) {
        preferences.end();
        return 0;
    }
    size_t result = preferences.getBytesLength(key);
    preferences.end();
    return result;
}

bool M5StickS3Platform::readBlob(const char* nameSpace, const char* key,
                                 void* output, size_t length) {
    Preferences preferences;
    if (!nameSpace || !key || !output || length == 0 ||
        !preferences.begin(nameSpace, true)) {
        return false;
    }
    bool result = preferences.getBytesLength(key) == length &&
                  preferences.getBytes(key, output, length) == length;
    preferences.end();
    return result;
}

bool M5StickS3Platform::writeBlob(const char* nameSpace, const char* key,
                                  const void* data, size_t length) {
    Preferences preferences;
    if (!nameSpace || !key || !data || length == 0 ||
        !preferences.begin(nameSpace, false)) {
        return false;
    }
    bool result = preferences.putBytes(key, data, length) == length;
    preferences.end();
    return result;
}

bool M5StickS3Platform::removeBlob(const char* nameSpace, const char* key) {
    Preferences preferences;
    if (!nameSpace || !key || !preferences.begin(nameSpace, false)) return false;
    bool result = !preferences.isKey(key) || preferences.remove(key);
    preferences.end();
    return result;
}

bool M5StickS3Platform::clearNamespace(const char* nameSpace) {
    Preferences preferences;
    if (!nameSpace || !preferences.begin(nameSpace, false)) return false;
    bool result = preferences.clear();
    preferences.end();
    return result;
}

bool M5StickS3Platform::mount() {
    if (resourceStoreReady_) return true;
    resourceStoreReady_ = LittleFS.begin(
        false, RESOURCE_BASE_PATH, RESOURCE_MAX_OPEN_FILES, RESOURCE_PARTITION);
    return resourceStoreReady_;
}

size_t M5StickS3Platform::totalBytes() const {
    return resourceStoreReady_ ? LittleFS.totalBytes() : 0;
}

size_t M5StickS3Platform::usedBytes() const {
    return resourceStoreReady_ ? LittleFS.usedBytes() : 0;
}

Platform::ResourceFile M5StickS3Platform::open(const char* path) {
    if (!resourceStoreReady_ || !path) return {};
    fs::File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory()) return {};
    auto* implementation = new (std::nothrow) LittleResourceFile(std::move(file));
    return Platform::ResourceFile(implementation);
}

void* M5StickS3Platform::allocate(size_t bytes, bool preferExternal) {
    if (bytes == 0) return nullptr;
    if (preferExternal && psramFound()) {
        if (void* memory = ps_malloc(bytes)) return memory;
    }
    return malloc(bytes);
}

void M5StickS3Platform::release(void* memory) {
    free(memory);
}

size_t M5StickS3Platform::externalFree() const {
    return ESP.getFreePsram();
}

bool M5StickS3Platform::enable() {
    if (peerTransportActive_) return true;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (esp_now_init() != ESP_OK ||
        esp_now_register_recv_cb(receivePeerPacket) != ESP_OK) {
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);
        return false;
    }
    portENTER_CRITICAL(&gPeerQueueMux);
    gPeerQueueHead = 0;
    gPeerQueueTail = 0;
    portEXIT_CRITICAL(&gPeerQueueMux);
    peerTransportActive_ = true;
    return true;
}

void M5StickS3Platform::end() {
    if (!peerTransportActive_) return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    WiFi.mode(WIFI_OFF);
    peerTransportActive_ = false;
}

bool M5StickS3Platform::active() const {
    return peerTransportActive_;
}

bool M5StickS3Platform::send(const uint8_t destination[6], const void* data,
                             size_t length) {
    if (!peerTransportActive_ || !destination || !data || length == 0 ||
        length > Platform::PeerPacket::MAX_PAYLOAD_BYTES) {
        return false;
    }
    if (!esp_now_is_peer_exist(destination)) {
        esp_now_peer_info_t peer{};
        memcpy(peer.peer_addr, destination, sizeof(peer.peer_addr));
        peer.channel = 0;
        peer.encrypt = false;
        if (esp_now_add_peer(&peer) != ESP_OK) return false;
    }
    return esp_now_send(destination, static_cast<const uint8_t*>(data), length) ==
           ESP_OK;
}

bool M5StickS3Platform::receive(Platform::PeerPacket& packet) {
    if (!peerTransportActive_) return false;
    portENTER_CRITICAL(&gPeerQueueMux);
    if (gPeerQueueTail == gPeerQueueHead) {
        portEXIT_CRITICAL(&gPeerQueueMux);
        return false;
    }
    packet = gPeerQueue[gPeerQueueTail];
    gPeerQueueTail = static_cast<uint8_t>(
        (gPeerQueueTail + 1) % PEER_QUEUE_CAPACITY);
    portEXIT_CRITICAL(&gPeerQueueMux);
    return true;
}
