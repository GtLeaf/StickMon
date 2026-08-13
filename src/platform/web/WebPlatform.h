#pragma once

#include "platform/api/PlatformServices.h"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class WebPlatform final : public Platform::IPlatformLifecycle,
                          public Platform::IClock,
                          public Platform::ILogger,
                          public Platform::IInputDevice,
                          public Platform::IDisplayDevice,
                          public Platform::IAudioDevice,
                          public Platform::IImuDevice,
                          public Platform::IPowerDevice,
                          public Platform::IBlobStore,
                          public Platform::IResourceStore,
                          public Platform::IMemoryAllocator,
                          public Platform::IPeerTransport {
public:
    WebPlatform();
    Platform::Services& serviceBundle() { return services_; }

    // IPlatformLifecycle
    bool begin() override;

    // IClock — real time via emscripten_get_now()
    uint32_t millis() const override;
    uint32_t micros() const override;
    void sleepMs(uint32_t durationMs) override;

    // ILogger
    void write(const char* text, size_t length) override;
    void flush() override;

    // IInputDevice — driven by JS keyboard/touch events
    void update() override {}
    bool pressed(Platform::InputButton button) const override;
    void setPressed(Platform::InputButton button, bool value);

    // IDisplayDevice — RGB565 framebuffer, presented to HTML5 Canvas
    Platform::FrameBuffer565 frameBuffer() override;
    void present() override;
    void setBrightness(uint8_t value) override { brightness_ = value; }
    uint8_t brightness() const override { return brightness_; }
    void sleep() override { displaySleeping_ = true; }

    // IAudioDevice — Web Audio API via EM_JS
    void setVolume(uint8_t percent) override;
    uint8_t volume() const override { return volume_; }
    bool playPcmU8(const uint8_t* data, size_t sampleCount,
                   uint32_t sampleRate) override;
    bool playPcmU8Channel(const uint8_t* data, size_t sampleCount,
                          uint32_t sampleRate, uint8_t channel,
                          bool stopCurrent) override;
    void setChannelVolume(uint8_t channel, uint8_t percent) override;
    bool playing() const override;
    uint8_t queuedPcm(uint8_t channel) const override;
    void stop() override;
    void stopChannel(uint8_t channel) override;
    bool beginMicrophone() override { return false; }
    void endMicrophone() override {}
    bool recordMicrophone(int16_t*, size_t, uint32_t) override { return false; }
    bool microphoneRecording() const override { return false; }
    bool microphoneActive() const override { return false; }

    // IImuDevice
    bool readAcceleration(float& x, float& y, float& z) override;
    void setAcceleration(float x, float y, float z);

    // IPowerDevice
    int batteryLevel() override { return 100; }
    Platform::WakeReason wakeReason() const override {
        return Platform::WakeReason::NORMAL;
    }
    uint32_t hardwareRandom() override;
    // 浏览器堆内存充足，向游戏层报告与真机 PSRAM 等量的外部内存，
    // 使 RoomRenderer 等按 externalMemorySize()>0 门控的缓冲分配路径生效
    size_t externalMemorySize() const override { return kExternalMemSize; }
    size_t externalMemoryFree() const override;
    void restart() override {}
    void enterDeepSleep(uint64_t, bool) override {}

    // IBlobStore — localStorage via EM_JS
    bool initialize() override { return true; }
    size_t blobSize(const char* nameSpace, const char* key) override;
    bool readBlob(const char* nameSpace, const char* key,
                  void* output, size_t length) override;
    bool writeBlob(const char* nameSpace, const char* key,
                   const void* data, size_t length) override;
    bool removeBlob(const char* nameSpace, const char* key) override;
    bool clearNamespace(const char* nameSpace) override;

    // IResourceStore — Emscripten virtual FS (preloaded files)
    bool mount() override;
    size_t totalBytes() const override { return resourceBytes_; }
    size_t usedBytes() const override { return resourceBytes_; }
    Platform::ResourceFile open(const char* path) override;

    // IMemoryAllocator
    void* allocate(size_t bytes, bool preferExternal) override;
    void release(void* memory) override;
    size_t externalFree() const override { return 0; }

    // IPeerTransport — stub (no ESP-NOW in browser)
    bool enable() override { return true; }
    void end() override {}
    bool active() const override { return false; }
    bool send(const uint8_t[6], const void*, size_t) override { return false; }
    bool receive(Platform::PeerPacket&) override { return false; }

private:
    static constexpr size_t kExternalMemSize = 8u * 1024u * 1024u;
    static std::string blobKey(const char* nameSpace, const char* key);

    Platform::Services services_;
    std::vector<uint16_t> frameBuffer_;
    std::array<bool, static_cast<size_t>(Platform::InputButton::COUNT)> buttons_{};
    std::unordered_map<std::string, std::vector<uint8_t>> blobs_;
    std::unordered_map<void*, size_t> externalAllocs_;
    size_t externalUsed_ = 0;
    size_t resourceBytes_ = 0;
    uint8_t brightness_ = 255;
    uint8_t volume_ = 50;
    float accelerationX_ = 0.0f;
    float accelerationY_ = 0.0f;
    float accelerationZ_ = 1.0f;
    bool initialized_ = false;
    bool resourcesMounted_ = false;
    bool displaySleeping_ = false;
    std::array<uint8_t, Platform::IAudioDevice::CHANNEL_COUNT>
        audioQueueDepth_{};
    std::array<uint8_t, Platform::IAudioDevice::CHANNEL_COUNT>
        audioChannelVolume_{{100, 100, 100}};
};
