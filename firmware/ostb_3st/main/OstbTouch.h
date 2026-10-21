#pragma once

#include "driver/i2c_master.h"
#include "platform/api/PlatformServices.h"
#include "TouchCalibration.h"

namespace Ostb {
class OstbTouch {
public:
    using Event = Platform::IInputDevice::TouchEvent;
    using Type = Platform::IInputDevice::TouchType;
    bool begin(i2c_master_bus_handle_t bus, bool forceSetup);
    void update(uint32_t now, bool blocked);
    bool poll(Event& event);
    bool available() const { return device_ && calibrated_ && !skipped_; }
    bool setupActive() const { return device_ && !calibrated_ && !skipped_; }
    void drawSetup();
    void skipSetup() { skipped_ = true; pending_ = false; }
private:
    void emit(Type type, uint32_t now);
    i2c_master_dev_handle_t device_ = nullptr;
    Stickmon::TouchCalibration calibration_;
    Stickmon::TouchCalibrationSetup setup_;
    bool calibrated_ = false, skipped_ = false, dirty_ = true;
    bool down_ = false, blockedUntilRelease_ = true, pending_ = false;
    bool setupDown_ = false, setupMoved_ = false;
    const char* setupHint_ = nullptr;
    uint32_t lastPoll_ = 0, setupDownAt_ = 0;
    int16_t rawX_ = 0, rawY_ = 0, setupX_ = 0, setupY_ = 0;
    Event event_;
};
}  // namespace Ostb
