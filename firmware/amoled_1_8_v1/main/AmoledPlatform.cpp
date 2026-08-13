#include "AmoledPlatform.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace AmoledV1 {
namespace {

constexpr char RESOURCE_BASE_PATH[] = "/assets";
constexpr char RESOURCE_PARTITION[] = "resources";

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

bool AmoledPlatform::begin() { return true; }

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
void AmoledPlatform::setBrightness(uint8_t value) { brightness_ = value; }
uint8_t AmoledPlatform::brightness() const { return brightness_; }
void AmoledPlatform::sleep() {}

void AmoledPlatform::setVolume(uint8_t percent) {
    volume_ = percent > 100 ? 100 : percent;
}
uint8_t AmoledPlatform::volume() const { return volume_; }
bool AmoledPlatform::playPcmU8(const uint8_t*, size_t, uint32_t) { return false; }
bool AmoledPlatform::playPcmU8Channel(const uint8_t*, size_t, uint32_t,
                                      uint8_t, bool) { return false; }
void AmoledPlatform::setChannelVolume(uint8_t, uint8_t) {}
bool AmoledPlatform::playing() const { return false; }
uint8_t AmoledPlatform::queuedPcm(uint8_t) const { return 0; }
void AmoledPlatform::stop() {}
void AmoledPlatform::stopChannel(uint8_t) {}
bool AmoledPlatform::beginMicrophone() { return false; }
void AmoledPlatform::endMicrophone() {}
bool AmoledPlatform::recordMicrophone(int16_t*, size_t, uint32_t) { return false; }
bool AmoledPlatform::microphoneRecording() const { return false; }
bool AmoledPlatform::microphoneActive() const { return false; }

bool AmoledPlatform::readAcceleration(float& x, float& y, float& z) {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    return false;
}

int AmoledPlatform::batteryLevel() { return -1; }
Platform::WakeReason AmoledPlatform::wakeReason() const {
    return Platform::WakeReason::NORMAL;
}
uint32_t AmoledPlatform::hardwareRandom() { return esp_random(); }
size_t AmoledPlatform::externalMemorySize() const { return esp_psram_get_size(); }
size_t AmoledPlatform::externalMemoryFree() const {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
void AmoledPlatform::restart() { esp_restart(); }
void AmoledPlatform::enterDeepSleep(uint64_t timerWakeUs, bool) {
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

bool AmoledPlatform::enable() { return false; }
void AmoledPlatform::end() {}
bool AmoledPlatform::active() const { return false; }
bool AmoledPlatform::send(const uint8_t[6], const void*, size_t) { return false; }
bool AmoledPlatform::receive(Platform::PeerPacket&) { return false; }

}  // namespace AmoledV1
