#pragma once

#include <cstdint>

#include "driver/i2c_master.h"

namespace Stickmon {

// Battery telemetry only; charging parameters and power rails remain untouched.
class Axp2101Battery {
public:
    bool begin(i2c_master_bus_handle_t bus);
    bool available() const { return device_ != nullptr; }
    // Returns the PMU's state of charge (0..100), or -1 when unavailable.
    int level();

private:
    bool readRegister(uint8_t reg, uint8_t& value);
    bool enableBit(uint8_t reg, uint8_t mask);
    i2c_master_dev_handle_t device_ = nullptr;
};

}  // namespace Stickmon
