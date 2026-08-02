#pragma once

#include "platform/api/PlatformServices.h"

class M5StickS3Platform final : public Platform::IPlatformLifecycle,
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
    static M5StickS3Platform& instance();
    Platform::Services& serviceBundle();

    bool begin() override;

    uint32_t millis() const override;
    uint32_t micros() const override;
    void sleepMs(uint32_t durationMs) override;

    void write(const char* text, size_t length) override;
    void flush() override;

    void update() override;
    bool pressed(Platform::InputButton button) const override;

    Platform::FrameBuffer565 frameBuffer() override;
    void present() override;
    void setBrightness(uint8_t value) override;
    uint8_t brightness() const override;
    void sleep() override;

    void setVolume(uint8_t percent) override;
    uint8_t volume() const override;
    bool playPcmU8(const uint8_t* data, size_t sampleCount,
                   uint32_t sampleRate) override;
    bool playPcmU8Channel(const uint8_t* data, size_t sampleCount,
                          uint32_t sampleRate, uint8_t channel,
                          bool stopCurrent) override;
    bool playing() const override;
    uint8_t queuedPcm(uint8_t channel) const override;
    void stop() override;
    void stopChannel(uint8_t channel) override;
    bool beginMicrophone() override;
    void endMicrophone() override;
    bool recordMicrophone(int16_t* data, size_t sampleCount,
                          uint32_t sampleRate) override;
    bool microphoneRecording() const override;
    bool microphoneActive() const override;

    bool readAcceleration(float& x, float& y, float& z) override;

    int batteryLevel() override;
    Platform::WakeReason wakeReason() const override;
    uint32_t hardwareRandom() override;
    size_t externalMemorySize() const override;
    size_t externalMemoryFree() const override;
    void restart() override;
    void enterDeepSleep(uint64_t timerWakeUs,
                        bool wakeOnSecondaryButton) override;

    bool initialize() override;
    size_t blobSize(const char* nameSpace, const char* key) override;
    bool readBlob(const char* nameSpace, const char* key,
                  void* output, size_t length) override;
    bool writeBlob(const char* nameSpace, const char* key,
                   const void* data, size_t length) override;
    bool removeBlob(const char* nameSpace, const char* key) override;
    bool clearNamespace(const char* nameSpace) override;

    bool mount() override;
    size_t totalBytes() const override;
    size_t usedBytes() const override;
    Platform::ResourceFile open(const char* path) override;

    void* allocate(size_t bytes, bool preferExternal) override;
    void release(void* memory) override;
    size_t externalFree() const override;

    bool enable() override;
    void end() override;
    bool active() const override;
    bool send(const uint8_t destination[6], const void* data,
              size_t length) override;
    bool receive(Platform::PeerPacket& packet) override;

private:
    M5StickS3Platform();

    Platform::Services services_;
    bool initialized_ = false;
    uint8_t audioVolume_ = 50;
    bool microphoneMode_ = false;
    uint32_t microphoneRecordingUntilMs_ = 0;
    bool resourceStoreReady_ = false;
    bool peerTransportActive_ = false;
};

void bindM5StickS3Platform();
