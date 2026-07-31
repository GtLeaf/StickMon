#pragma once

#include "platform/api/PlatformServices.h"

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class DesktopPlatform final : public Platform::IPlatformLifecycle,
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
    explicit DesktopPlatform(std::string resourceRoot = ".");
    Platform::Services& serviceBundle() { return services_; }

    bool begin() override;
    uint32_t millis() const override;
    uint32_t micros() const override { return nowMs_ * 1000U; }
    void sleepMs(uint32_t durationMs) override;
    void advanceMs(uint32_t durationMs) { nowMs_ += durationMs; }

    void write(const char* text, size_t length) override;
    void flush() override {}

    void update() override {}
    bool pressed(Platform::InputButton button) const override;
    void setPressed(Platform::InputButton button, bool value);

    Platform::FrameBuffer565 frameBuffer() override;
    void present() override { ++presentCount_; }
    void setBrightness(uint8_t value) override { brightness_ = value; }
    uint8_t brightness() const override { return brightness_; }
    void sleep() override { displaySleeping_ = true; }

    void setVolume(uint8_t percent) override;
    uint8_t volume() const override { return volume_; }
    bool playPcmU8(const uint8_t* data, size_t sampleCount,
                   uint32_t sampleRate) override;
    bool playing() const override { return playing_; }
    void stop() override { playing_ = false; }
    bool beginMicrophone() override;
    void endMicrophone() override { microphoneActive_ = false; }
    bool recordMicrophone(int16_t* data, size_t sampleCount,
                          uint32_t sampleRate) override;
    bool microphoneRecording() const override { return false; }
    bool microphoneActive() const override { return microphoneActive_; }

    bool readAcceleration(float& x, float& y, float& z) override;
    int batteryLevel() override { return batteryLevel_; }
    Platform::WakeReason wakeReason() const override {
        return Platform::WakeReason::NORMAL;
    }
    uint32_t hardwareRandom() override;
    size_t externalMemorySize() const override { return 0; }
    size_t externalMemoryFree() const override { return 0; }
    void restart() override { restartRequested_ = true; }
    void enterDeepSleep(uint64_t, bool) override { deepSleepRequested_ = true; }

    bool initialize() override { return true; }
    size_t blobSize(const char* nameSpace, const char* key) override;
    bool readBlob(const char* nameSpace, const char* key,
                  void* output, size_t length) override;
    bool writeBlob(const char* nameSpace, const char* key,
                   const void* data, size_t length) override;
    bool removeBlob(const char* nameSpace, const char* key) override;
    bool clearNamespace(const char* nameSpace) override;

    bool mount() override;
    size_t totalBytes() const override { return resourceBytes_; }
    size_t usedBytes() const override { return resourceBytes_; }
    Platform::ResourceFile open(const char* path) override;

    void* allocate(size_t bytes, bool preferExternal) override;
    void release(void* memory) override;
    size_t externalFree() const override { return 0; }

    bool enable() override;
    void end() override;
    bool active() const override { return peersActive_; }
    bool send(const uint8_t destination[6], const void* data,
              size_t length) override;
    bool receive(Platform::PeerPacket& packet) override;

    size_t presentCount() const { return presentCount_; }
    size_t audioPlayCount() const { return audioPlayCount_; }
    const std::vector<uint8_t>& lastAudio() const { return lastAudio_; }
    const std::string& logs() const { return logs_; }

private:
    static std::string blobKey(const char* nameSpace, const char* key);

    Platform::Services services_;
    std::string resourceRoot_;
    std::vector<uint16_t> frameBuffer_;
    std::array<bool, static_cast<size_t>(Platform::InputButton::COUNT)> buttons_{};
    std::unordered_map<std::string, std::vector<uint8_t>> blobs_;
    std::deque<Platform::PeerPacket> peerQueue_;
    std::vector<uint8_t> lastAudio_;
    std::string logs_;
    uint32_t nowMs_ = 0;
    uint32_t randomState_ = 0x12345678UL;
    size_t resourceBytes_ = 0;
    size_t presentCount_ = 0;
    size_t audioPlayCount_ = 0;
    uint8_t brightness_ = 128;
    uint8_t volume_ = 50;
    int batteryLevel_ = 100;
    bool initialized_ = false;
    bool resourcesMounted_ = false;
    bool displaySleeping_ = false;
    bool playing_ = false;
    bool microphoneActive_ = false;
    bool restartRequested_ = false;
    bool deepSleepRequested_ = false;
    bool peersActive_ = false;
};
