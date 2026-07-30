#pragma once

#include "platform/api/PlatformServices.h"

class M5StickS3Platform final : public Platform::IPlatformLifecycle,
                                public Platform::IClock,
                                public Platform::IInputDevice,
                                public Platform::IDisplayDevice,
                                public Platform::IAudioDevice,
                                public Platform::IImuDevice,
                                public Platform::IPowerDevice {
public:
    static M5StickS3Platform& instance();
    Platform::Services& serviceBundle();

    bool begin() override;

    uint32_t millis() const override;
    void sleepMs(uint32_t durationMs) override;

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
    bool playing() const override;
    void stop() override;
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

private:
    M5StickS3Platform();

    Platform::Services services_;
    bool initialized_ = false;
    uint8_t audioVolume_ = 50;
    bool microphoneMode_ = false;
    uint32_t microphoneRecordingUntilMs_ = 0;
};

void bindM5StickS3Platform();
