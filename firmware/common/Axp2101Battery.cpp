#include "Axp2101Battery.h"

namespace Stickmon {
namespace {
// AXP2101 datasheet: REG 00, 18, 68 and A4.
// https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf
// Device address/identity also match XPowersLib's AXP2101Constants.h.
constexpr uint8_t ADDRESS = 0x34;
constexpr uint8_t CHIP_ID = 0x03;
constexpr uint8_t STATUS = 0x00;
constexpr uint8_t GAUGE_CONTROL = 0x18;
constexpr uint8_t BATTERY_DETECTION = 0x68;
constexpr uint8_t PERCENT = 0xA4;
constexpr uint8_t GAUGE_ENABLE = 1U << 3;
constexpr uint8_t BATTERY_PRESENT = 1U << 3;
constexpr int TIMEOUT_MS = 20;
}

bool Axp2101Battery::readRegister(uint8_t reg, uint8_t& value) {
    return device_ && i2c_master_transmit_receive(
        device_, &reg, 1, &value, 1, TIMEOUT_MS) == ESP_OK;
}

bool Axp2101Battery::enableBit(uint8_t reg, uint8_t mask) {
    uint8_t value = 0;
    if (!readRegister(reg, value)) return false;
    if ((value & mask) == mask) return true;
    const uint8_t bytes[] = {reg, static_cast<uint8_t>(value | mask)};
    return i2c_master_transmit(device_, bytes, sizeof(bytes), TIMEOUT_MS) == ESP_OK;
}

bool Axp2101Battery::begin(i2c_master_bus_handle_t bus) {
    if (device_) return true;
    if (!bus || i2c_master_probe(bus, ADDRESS, TIMEOUT_MS) != ESP_OK) return false;
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = ADDRESS;
    config.scl_speed_hz = 400000;
    if (i2c_master_bus_add_device(bus, &config, &device_) != ESP_OK) return false;
    uint8_t id = 0;
    if (!readRegister(CHIP_ID, id) || id != 0x4A ||
        !enableBit(BATTERY_DETECTION, 1U) ||
        !enableBit(GAUGE_CONTROL, GAUGE_ENABLE)) {
        i2c_master_bus_rm_device(device_);
        device_ = nullptr;
        return false;
    }
    return true;
}

int Axp2101Battery::level() {
    uint8_t status = 0, control = 0, percent = 0;
    if (!readRegister(STATUS, status) || !(status & BATTERY_PRESENT) ||
        !readRegister(GAUGE_CONTROL, control) || !(control & GAUGE_ENABLE) ||
        !readRegister(PERCENT, percent) || percent > 100) return -1;
    return percent;
}

}  // namespace Stickmon
