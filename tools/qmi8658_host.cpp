#include "Qmi8658.h"

#include <array>
#include <cassert>
#include <cstring>

namespace {
std::array<uint8_t, 256> registers{};
uint8_t foundAddress = 0x6A;
bool wrongIdAt6A = false;
int failRegister = -1;
size_t reads = 0, writes = 0, removes = 0;
}

esp_err_t i2c_master_probe(i2c_master_bus_handle_t, uint16_t address, int timeout) {
    assert(timeout <= 20);
    return address == foundAddress || wrongIdAt6A ? ESP_OK : ESP_FAIL;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,
                                   const i2c_device_config_t* config,
                                   i2c_master_dev_handle_t* handle) {
    assert(config->dev_addr_length == I2C_ADDR_BIT_LEN_7);
    assert(config->scl_speed_hz == 400000);
    *handle = reinterpret_cast<void*>(uintptr_t(config->device_address));
    return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t) {
    ++removes;
    return ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t, const uint8_t* bytes,
                             size_t size, int timeout) {
    assert(size == 2 && timeout <= 20);
    ++writes;
    if (bytes[0] == failRegister) return ESP_FAIL;
    registers[bytes[0]] = bytes[1];
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t device,
                                     const uint8_t* tx, size_t txSize,
                                     uint8_t* rx, size_t size, int timeout) {
    assert(txSize == 1 && timeout <= 20);
    ++reads;
    if (*tx == failRegister) return ESP_FAIL;
    if (*tx == 0 && wrongIdAt6A && device == reinterpret_cast<void*>(0x6A)) {
        *rx = 0;
    } else {
        std::memcpy(rx, registers.data() + *tx, size);
    }
    return ESP_OK;
}

int main() {
    registers[0] = 0x05;
    Stickmon::Qmi8658 imu;
    assert(!imu.begin(nullptr));
    assert(imu.begin(reinterpret_cast<void*>(1)));
    assert(registers[8] == 0 && (registers[2] & 1));
    float x = 9, y = 9, z = 9;
    const auto offReads = reads;
    assert(!imu.readAcceleration(x, y, z));
    assert(x == 0 && y == 0 && z == 0 && reads == offReads);
    assert(imu.setAccelerationEnabled(true));
    assert(registers[8] == 1 && registers[2] == 0x40 && registers[3] == 0x27);
    registers[0x2E] = 0;
    assert(!imu.readAcceleration(x, y, z));
    registers[0x2E] = 1;
    // +/-8g scale: little-endian signed 4096 counts per g.
    const uint8_t axes[] = {0, 0x10, 0, 0xF0, 0, 0x40};
    std::memcpy(registers.data() + 0x35, axes, sizeof(axes));
    assert(imu.readAcceleration(x, y, z));
    assert(x == 1 && y == -1 && z == 4);
    failRegister = 0x35;
    assert(!imu.readAcceleration(x, y, z) && x == 0 && y == 0 && z == 0);
    failRegister = 8;
    assert(!imu.setAccelerationEnabled(false));
    failRegister = -1;
    assert(imu.setAccelerationEnabled(false));
    const auto offWrites = writes;
    assert(imu.setAccelerationEnabled(false) && writes == offWrites);
    failRegister = 3;
    assert(!imu.setAccelerationEnabled(true));
    assert(registers[8] == 0 && (registers[2] & 1));
    failRegister = -1;
    assert(imu.setAccelerationEnabled(true));
    assert(imu.setAccelerationEnabled(false));
    foundAddress = 0x6B;
    wrongIdAt6A = true;
    Stickmon::Qmi8658 alternate;
    assert(alternate.begin(reinterpret_cast<void*>(1)) && removes == 1);
    registers[0] = 0;
    Stickmon::Qmi8658 absent;
    assert(!absent.begin(reinterpret_cast<void*>(1)));
    assert(!absent.setAccelerationEnabled(true));
}
