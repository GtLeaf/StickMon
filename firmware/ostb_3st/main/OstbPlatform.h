#pragma once

#include <array>
#include <atomic>

#include "esp_codec_dev.h"
#include "esp_adc/adc_oneshot.h"
#include "core/PcmMixer.h"
#include "PowerButton.h"
#include "OstbTouch.h"
#include "esp_lcd_panel_io.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "platform/api/PlatformServices.h"

namespace Ostb {

class OstbPlatform final : public Platform::IPlatformLifecycle,
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
    static OstbPlatform& instance();
    Platform::Services& serviceBundle();

    bool begin() override;
    uint32_t millis() const override;
    uint32_t micros() const override;
    void sleepMs(uint32_t durationMs) override;
    void write(const char* text, size_t length) override;
    void flush() override;
    void update() override;
    bool pressed(Platform::InputButton button) const override;
    bool touchAvailable() const override { return touch_.available(); }
    bool pollTouch(Platform::IInputDevice::TouchEvent& event) override { return touch_.poll(event); }
    bool touchSetupActive() const { return touch_.setupActive(); }
    void drawTouchSetup() { touch_.drawSetup(); }
    Platform::FrameBuffer565 frameBuffer() override;
    void present() override;
    void setBrightness(uint8_t value) override;
    uint8_t brightness() const override;
    void sleep() override;
    bool locked() const override { return locked_; }
    bool takeShutdownRequest() { bool v = shutdown_; shutdown_ = false; return v; }
    bool takeUnlockRequest() { bool v = unlocked_; unlocked_ = false; return v; }
    bool takeSaveRequest() { bool v = saveRequested_; saveRequested_ = false; return v; }
    bool powerKeyHeld() const { return powerButton_.down(); }

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
    bool setAccelerationEnabled(bool enabled) override;
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
    OstbPlatform();
    bool beginDisplay();
    bool beginAudio();
    bool beginI2c();
    bool beginBattery();
    void applyBacklight();
    static void audioTaskEntry(void* context);
    void audioTask();
    Platform::Services services_;
    bool resourcesMounted_ = false, initialized_ = false;
    bool peerTransportActive_ = false, wifiInitialized_ = false;
    bool locked_ = false, shutdown_ = false, unlocked_ = false;
    bool firstFramePresented_ = false;
    bool saveRequested_ = false;
    bool blocked_[2] = {true, true};
    bool buttons_[2] = {};
    Stickmon::PowerButton powerButton_;
    i2c_master_bus_handle_t i2cBus_ = nullptr;
    OstbTouch touch_;
    adc_oneshot_unit_handle_t batteryAdc_ = nullptr;
    uint32_t batterySampleMs_ = 0;
    int batteryRaw_ = 0, batteryPercent_ = -1;
    bool batterySampled_ = false;
    uint16_t* frame_ = nullptr;
    uint16_t* strips_[2] = {};
    esp_lcd_panel_io_handle_t io_ = nullptr;
    SemaphoreHandle_t completion_ = nullptr, audioMutex_ = nullptr;
    PcmMixer mixer_;
    esp_codec_dev_handle_t speakerCodec_ = nullptr;
    uint8_t brightness_ = 72, volume_ = 35;
};
void bindOstbPlatform();
}  // namespace Ostb
