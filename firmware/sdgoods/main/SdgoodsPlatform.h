// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ResourceArchive.h"
#include "Audio.h"
#include "Battery.h"
#include "PeerRadio.h"
#include "PowerButton.h"
#include "Qmi8658.h"
#include "Cst816TouchState.h"
#include "NativePanel.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace Sdgoods {
class SdgoodsPlatform final : public Platform::IPlatformLifecycle, public Platform::IClock,
    public Platform::ILogger, public Platform::IInputDevice, public Platform::IDisplayDevice,
    public Platform::IImuDevice, public Platform::IPowerDevice,
    public Platform::IBlobStore, public Platform::IMemoryAllocator {
public:
    SdgoodsPlatform();
    Platform::Services& serviceBundle() { return services_; }
    bool begin() override;
    uint32_t millis() const override;
    uint32_t micros() const override;
    void sleepMs(uint32_t ms) override;
    void write(const char* text, size_t length) override;
    void flush() override;
    // The application samples before simulation to persist a power press first.
    void update() override {}
    void pollInputs();
    bool pressed(Platform::InputButton) const override { return false; }
    bool touchAvailable() const override { return true; }
    bool touchCalibrationSupported() const override { return true; }
    void bypassTouchCalibration(bool value) override;
    bool saveTouchCalibration(const Platform::TouchTransform& transform) override;
    bool pollTouch(TouchEvent& event) override {
        if (cancelPending_) {
            cancelPending_ = false;
            event = {TouchType::CANCEL, 0, 0, millis()};
            return true;
        }
        return touch_.poll(event);
    }
    bool takeSaveRequest() { const bool result = saveRequested_; saveRequested_ = false; return result; }
    Platform::FrameBuffer565 frameBuffer() override { return {frame_, 360, 360, true}; }
    void present() override;
    void presentRegion(int x, int y, int width, int height) override;
    void setBrightness(uint8_t value) override;
    uint8_t brightness() const override { return brightness_; }
    void sleep() override;
    bool locked() const override { return locked_; }
    bool setAccelerationEnabled(bool enabled) override { return imu_.setAccelerationEnabled(enabled); }
    bool readAcceleration(float& x, float& y, float& z) override { return imu_.readAcceleration(x, y, z); }
    int batteryLevel() override { return battery_.level(millis()); }
    bool batteryCharging() override { return battery_.chargingEstimate(millis()); }
    Platform::PowerCapabilities capabilities() const override { return {true, false, true, true}; }
    bool powerKeyHeld() const { return powerButton_.down(); }
    bool takeUnlockRequest() { const bool v = unlocked_; unlocked_ = false; return v; }
    bool takeShutdownRequest() { const bool v = shutdown_; shutdown_ = false; return v; }
    void powerOff();
    Platform::WakeReason wakeReason() const override;
    uint32_t hardwareRandom() override;
    size_t externalMemorySize() const override;
    size_t externalMemoryFree() const override;
    void restart() override;
    void enterDeepSleep(uint64_t, bool) override;
    bool initialize() override;
    size_t blobSize(const char*, const char*) override;
    bool readBlob(const char*, const char*, void*, size_t) override;
    bool writeBlob(const char*, const char*, const void*, size_t) override;
    bool removeBlob(const char*, const char*) override;
    bool clearNamespace(const char*) override;
    void* allocate(size_t bytes, bool external) override;
    void release(void* memory) override;
    size_t externalFree() const override { return externalMemoryFree(); }
private:
    ResourceArchive resources_;
    Audio audio_;
    Battery battery_;
    PeerRadio radio_;
    Stickmon::Qmi8658 imu_;
    Stickmon::PowerButton powerButton_;
    Platform::Services services_;
    bool beginDisplay();
    bool beginTouch();
    void applyBacklight();
    void setLocked(bool value);
    void prepareSleep();
    uint16_t* frame_ = nullptr;
    uint16_t* strips_[2]{};
    SemaphoreHandle_t completion_ = nullptr;
    esp_lcd_panel_io_handle_t io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    i2c_master_bus_handle_t i2cBus_ = nullptr;
    i2c_master_dev_handle_t touchDevice_ = nullptr;
    Cst816TouchState touch_;
    uint32_t sampledAtMs_ = 0;
    uint8_t brightness_ = 128;
    bool initialized_ = false, storageReady_ = false, firstFrame_ = false;
    bool locked_ = false, unlocked_ = false, shutdown_ = false, saveRequested_ = false;
    bool touchBlocked_ = true, cancelPending_ = false;
};
}  // namespace Sdgoods
