#pragma once

#include <atomic>

#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "platform/api/PlatformServices.h"

namespace AmoledV2 {

class AmoledPlatform final : public Platform::IPlatformLifecycle,
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
    static AmoledPlatform& instance();
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
    bool playPcmU8(const uint8_t*, size_t, uint32_t) override;
    bool playPcmU8Channel(const uint8_t*, size_t, uint32_t, uint8_t, bool) override;
    void setChannelVolume(uint8_t, uint8_t) override;
    bool playing() const override;
    uint8_t queuedPcm(uint8_t) const override;
    void stop() override;
    void stopChannel(uint8_t) override;
    bool beginMicrophone() override;
    void endMicrophone() override;
    bool recordMicrophone(int16_t*, size_t, uint32_t) override;
    bool microphoneRecording() const override;
    bool microphoneActive() const override;
    bool readAcceleration(float& x, float& y, float& z) override;

    int batteryLevel() override;
    Platform::PowerCapabilities capabilities() const override;
    Platform::WakeReason wakeReason() const override;
    uint32_t hardwareRandom() override;
    size_t externalMemorySize() const override;
    size_t externalMemoryFree() const override;
    void restart() override;
    void enterDeepSleep(uint64_t timerWakeUs, bool wakeOnSecondaryButton) override;

    bool initialize() override;
    size_t blobSize(const char*, const char*) override;
    bool readBlob(const char*, const char*, void*, size_t) override;
    bool writeBlob(const char*, const char*, const void*, size_t) override;
    bool removeBlob(const char*, const char*) override;
    bool clearNamespace(const char*) override;

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
    bool send(const uint8_t destination[6], const void* data, size_t length) override;
    bool receive(Platform::PeerPacket& packet) override;

private:
    struct AudioChunk {
        uint8_t channel = 0;
        uint32_t sampleRate = 0;
        uint32_t generation = 0;
        size_t sampleCount = 0;
        uint8_t* samples = nullptr;
    };

    AmoledPlatform();

    static void audioTaskEntry(void* context);
    void audioTask();
    void clearAudioQueue();
    void applySpeakerVolumeLocked(uint8_t channel);

    Platform::Services services_;
    bool resourcesMounted_ = false;
    bool initialized_ = false;
    bool peerTransportActive_ = false;
    bool wifiInitialized_ = false;
    bool speakerOpen_ = false;
    bool microphoneOpen_ = false;
    bool microphoneMode_ = false;
    std::atomic<bool> audioPlaying_{false};
    std::atomic<uint32_t> audioGeneration_{1};
    std::atomic<uint8_t>
        queuedPcm_[Platform::IAudioDevice::CHANNEL_COUNT]{};
    QueueHandle_t audioQueue_ = nullptr;
    SemaphoreHandle_t audioMutex_ = nullptr;
    uint32_t speakerSampleRate_ = 0;
    uint32_t microphoneSampleRate_ = 0;
    esp_codec_dev_handle_t speakerCodec_ = nullptr;
    esp_codec_dev_handle_t microphoneCodec_ = nullptr;
    uint8_t brightness_ = 72;
    uint8_t volume_ = 50;
    uint8_t channelVolumes_[Platform::IAudioDevice::CHANNEL_COUNT] = {
        100, 100, 100};
};

void bindAmoledPlatform();

}  // namespace AmoledV2
