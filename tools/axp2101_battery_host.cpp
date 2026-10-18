#include "Axp2101Battery.h"

#include <array>
#include <cassert>

namespace {
std::array<uint8_t, 256> registers{};
bool present = true, addFails = false, writeFails = false;
int readFails = -1;
size_t writes = 0, removes = 0, adds = 0;
}

esp_err_t i2c_master_probe(void*, uint16_t address, int timeout) {
    assert(address == 0x34 && timeout <= 20);
    return present ? ESP_OK : ESP_FAIL;
}
esp_err_t i2c_master_bus_add_device(void*, const i2c_device_config_t* config,
                                   void** handle) {
    assert(config->device_address == 0x34 && config->scl_speed_hz == 400000);
    if (addFails) return ESP_FAIL;
    ++adds;
    *handle = reinterpret_cast<void*>(1);
    return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(void*) {
    ++removes;
    return ESP_OK;
}
esp_err_t i2c_master_transmit(void*, const uint8_t* bytes, size_t size, int timeout) {
    assert(size == 2 && timeout <= 20);
    // Initialization may only enable detection and the gauge, preserving all
    // charger, watchdog, voltage and battery parameter settings.
    assert(bytes[0] == 0x18 || bytes[0] == 0x68);
    const uint8_t mask = bytes[0] == 0x18 ? 8 : 1;
    assert(bytes[1] == (registers[bytes[0]] | mask));
    ++writes;
    if (writeFails) return ESP_FAIL;
    registers[bytes[0]] = bytes[1];
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(void*, const uint8_t* tx, size_t txSize,
                                     uint8_t* rx, size_t size, int timeout) {
    assert(txSize == 1 && size == 1 && timeout <= 20);
    if (*tx == readFails) return ESP_FAIL;
    *rx = registers[*tx];
    return ESP_OK;
}

int main() {
    void* bus = reinterpret_cast<void*>(1);
    Stickmon::Axp2101Battery battery;
    assert(!battery.available() && battery.level() == -1);
    assert(!battery.begin(nullptr));
    present = false;
    assert(!battery.begin(bus));
    present = true;
    addFails = true;
    assert(!battery.begin(bus));
    addFails = false;
    assert(!battery.begin(bus) && !battery.available() && writes == 0);
    registers[3] = 0x4A;
    registers[0x18] = 0x07;
    registers[0x68] = 0xA0;
    readFails = 0x68;
    assert(!battery.begin(bus) && !battery.available());
    readFails = -1;
    writeFails = true;
    assert(!battery.begin(bus) && !battery.available());
    writeFails = false;
    assert(battery.begin(bus) && battery.available());
    assert(registers[0x18] == 0x0F && registers[0x68] == 0xA1);
    const auto initialWrites = writes, initialAdds = adds;
    assert(battery.begin(bus) && writes == initialWrites && adds == initialAdds);
    registers[0xA4] = 100;
    assert(battery.level() == -1);  // USB power without a battery.
    registers[0] = 0x08;
    for (uint8_t percent : {0, 1, 19, 20, 50, 99, 100}) {
        registers[0xA4] = percent;
        assert(battery.level() == percent);
    }
    for (uint8_t invalid : {101, 127, 128, 255}) {
        registers[0xA4] = invalid;
        assert(battery.level() == -1);
    }
    registers[0xA4] = 65;
    for (int reg : {0x00, 0x18, 0xA4}) {
        readFails = reg;
        assert(battery.level() == -1);
    }
    readFails = -1;
    assert(battery.level() == 65);  // Recover after transient bus failures.
    registers[0x18] &= ~8U;
    assert(battery.level() == -1);
    assert(writes == initialWrites && removes == adds - 1);
}
