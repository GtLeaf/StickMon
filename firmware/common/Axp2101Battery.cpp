#include "Axp2101Battery.h"

namespace Stickmon {
namespace {
// AXP2101 datasheet: REG 00, 18, 68 and A4.
// https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf
// Device address/identity also match XPowersLib's AXP2101Constants.h.
constexpr uint8_t ADDRESS = 0x34;
constexpr uint8_t CHIP_ID = 0x03;
constexpr uint8_t STATUS = 0x00;
constexpr uint8_t CHARGE_STATUS = 0x01;
constexpr uint8_t COMMON_CONFIG = 0x10;
constexpr uint8_t POWER_KEY_TIMING = 0x27;
constexpr uint8_t KEY_IRQ_ENABLE = 0x41;
constexpr uint8_t KEY_IRQ_STATUS = 0x49;
constexpr uint8_t SHORT_PRESS = 1U << 3;
constexpr uint8_t KEY_DOWN = 1U << 1;
constexpr uint8_t KEY_EVENTS = SHORT_PRESS | KEY_DOWN;
constexpr uint8_t GAUGE_CONTROL = 0x18;
constexpr uint8_t BATTERY_DETECTION = 0x68;
constexpr uint8_t ADC_CHANNEL_CONTROL = 0x30;
constexpr uint8_t PERCENT = 0xA4;
constexpr uint8_t GAUGE_ENABLE = 1U << 3;
constexpr uint8_t BATTERY_PRESENT = 1U << 3;
constexpr uint8_t BATTERY_VOLTAGE_MEASURE = 1U;
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
    return writeRegister(reg, static_cast<uint8_t>(value | mask));
}

bool Axp2101Battery::writeRegister(uint8_t reg, uint8_t value) {
    const uint8_t bytes[] = {reg, value};
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
        !enableBit(ADC_CHANNEL_CONTROL, BATTERY_VOLTAGE_MEASURE) ||
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

bool Axp2101Battery::charging() {
    uint8_t status = 0, charge = 0;
    return readRegister(STATUS, status) && (status & BATTERY_PRESENT) &&
           readRegister(CHARGE_STATUS, charge) && (charge >> 5) == 1;
}

bool Axp2101Battery::configurePowerButton() {
    // REG27[1:0]=0 selects a 128ms power-on press. Preserve the hardware
    // long-hold recovery, IRQ timing, charger configuration and other IRQs.
    uint8_t timing = 0;
    const bool ready = readRegister(POWER_KEY_TIMING, timing) &&
        ((timing & 3U) == 0 || writeRegister(POWER_KEY_TIMING, timing & ~3U)) &&
        writeRegister(KEY_IRQ_STATUS, KEY_EVENTS) &&
        enableBit(KEY_IRQ_ENABLE, KEY_EVENTS);
    if (ready) powerButtonDownSeen_ = false;
    return ready;
}

bool Axp2101Battery::takePowerButtonPress() {
    uint8_t status = 0;
    // IRQ status is write-one-to-clear. Require a fresh key-down as well as
    // the click, so releasing a held power-on key never shuts down the app.
    if (!readRegister(KEY_IRQ_STATUS, status)) return false;
    const uint8_t events = status & KEY_EVENTS;
    if (!events || !writeRegister(KEY_IRQ_STATUS, events)) return false;
    const bool downSeen = powerButtonDownSeen_ || (events & KEY_DOWN);
    powerButtonDownSeen_ = (events & SHORT_PRESS) ? false : downSeen;
    return (events & SHORT_PRESS) && downSeen;
}

bool Axp2101Battery::powerOff() {
    return enableBit(COMMON_CONFIG, 1U);
}

}  // namespace Stickmon
