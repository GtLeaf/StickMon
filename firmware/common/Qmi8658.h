#pragma once

#include "driver/i2c_master.h"

namespace Stickmon {

// Shares the BSP bus with touch/audio. It never deletes or resets that bus.
class Qmi8658 {
public:
    bool begin(i2c_master_bus_handle_t bus);
    bool setAccelerationEnabled(bool enabled);
    bool readAcceleration(float& x, float& y, float& z);

private:
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegisters(uint8_t reg, uint8_t* data, size_t size);
    i2c_master_dev_handle_t device_ = nullptr;
    bool enabled_ = false;
    bool standby_ = false;
};

}  // namespace Stickmon
