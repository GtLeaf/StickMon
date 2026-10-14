#include "Qmi8658.h"

namespace Stickmon {
namespace {
// QMI8658C datasheet, rev 0.6, tables 23/24/26/27 and standby mode (p42).
// https://files.waveshare.com/wiki/common/QMI8658C.pdf
constexpr uint8_t WHO_AM_I = 0x00;
constexpr uint8_t CTRL1 = 0x02;
constexpr uint8_t CTRL2 = 0x03;
constexpr uint8_t CTRL5 = 0x06;
constexpr uint8_t CTRL7 = 0x08;
constexpr uint8_t STATUS0 = 0x2E;
constexpr uint8_t AX_L = 0x35;
constexpr int TIMEOUT_MS = 20;
}

bool Qmi8658::writeRegister(uint8_t reg, uint8_t value) {
    const uint8_t bytes[] = {reg, value};
    return device_ && i2c_master_transmit(device_, bytes, sizeof(bytes), TIMEOUT_MS) == ESP_OK;
}

bool Qmi8658::readRegisters(uint8_t reg, uint8_t* data, size_t size) {
    return device_ && i2c_master_transmit_receive(
        device_, &reg, 1, data, size, TIMEOUT_MS) == ESP_OK;
}

bool Qmi8658::begin(i2c_master_bus_handle_t bus) {
    if (!bus) return false;
    if (!device_) {
        constexpr uint8_t addresses[] = {0x6A, 0x6B};
        for (uint8_t address : addresses) {
            if (i2c_master_probe(bus, address, TIMEOUT_MS) != ESP_OK) continue;
            i2c_device_config_t config{};
            config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            config.device_address = address;
            config.scl_speed_hz = 400000;
            if (i2c_master_bus_add_device(bus, &config, &device_) != ESP_OK) return false;
            uint8_t id = 0;
            if (readRegisters(WHO_AM_I, &id, 1) && id == 0x05) break;
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
        }
    }
    return device_ && setAccelerationEnabled(false);
}

bool Qmi8658::setAccelerationEnabled(bool enabled) {
    if (!device_) return false;
    if (enabled == enabled_ && (enabled || standby_)) return true;
    if (!enabled) {
        // Disable all sensors before stopping the internal oscillator.
        const bool sensorsOff = writeRegister(CTRL7, 0x00);
        const bool oscillatorOff = writeRegister(CTRL1, 0x41);
        if (sensorsOff || oscillatorOff) enabled_ = false;
        standby_ = sensorsOff && oscillatorOff;
        return standby_;
    }
    standby_ = false;
    // Auto-increment, little endian, +/-8g, 62.5Hz, no gyro/AE/FIFO.
    if (!writeRegister(CTRL1, 0x40) || !writeRegister(CTRL7, 0x00) ||
        !writeRegister(CTRL2, 0x27) || !writeRegister(CTRL5, 0x00) ||
        !writeRegister(CTRL7, 0x01)) {
        setAccelerationEnabled(false);
        return false;
    }
    enabled_ = true;
    return true;
}

bool Qmi8658::readAcceleration(float& x, float& y, float& z) {
    x = y = z = 0.0f;
    uint8_t status = 0, bytes[6] = {};
    if (!enabled_ || !readRegisters(STATUS0, &status, 1) || !(status & 0x01) ||
        !readRegisters(AX_L, bytes, sizeof(bytes))) return false;
    auto axis = [&bytes](int offset) {
        const uint16_t raw = bytes[offset] | (uint16_t(bytes[offset + 1]) << 8);
        const int32_t signedRaw = raw >= 0x8000 ? int32_t(raw) - 0x10000 : raw;
        return signedRaw / 4096.0f;
    };
    x = axis(0); y = axis(2); z = axis(4);
    return true;
}

}  // namespace Stickmon
