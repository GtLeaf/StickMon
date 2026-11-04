#include "Axp2101Battery.h"
#include "AmoledPowerButton.h"

#include <array>
#include <cassert>

namespace {
std::array<uint8_t, 256> registers{};
bool present = true, addFails = false, writeFails = false;
bool allowPowerWrites = false;
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
    // Verify the exact writable bits; charger and rail settings are untouched.
    if (allowPowerWrites && bytes[0] == 0x27) {
        assert(bytes[1] == (registers[0x27] & ~3U));
    } else if (allowPowerWrites && bytes[0] == 0x49) {
        assert(bytes[1] && !(bytes[1] & ~0x0AU));
    } else {
        assert(bytes[0] == 0x18 || bytes[0] == 0x30 || bytes[0] == 0x68 ||
               (allowPowerWrites && (bytes[0] == 0x41 || bytes[0] == 0x10)));
        const uint8_t mask = bytes[0] == 0x41 ? 0x0A : bytes[0] == 0x18 ? 8 : 1;
        assert(bytes[1] == (registers[bytes[0]] | mask));
    }
    ++writes;
    if (writeFails) return ESP_FAIL;
    if (bytes[0] == 0x49) registers[0x49] &= ~bytes[1];
    else registers[bytes[0]] = bytes[1];
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

    registers[0] = 0x08;
    for (uint8_t state : {0x00, 0x20, 0x40, 0x60, 0x80, 0xA0}) {
        registers[1] = state;
        assert(battery.charging() == (state == 0x20));
    }
    registers[1] = 0x20;
    registers[0] = 0;
    assert(!battery.charging()); // USB without a battery is not charging.
    registers[0] = 0x08;
    for (int reg : {0x00, 0x01}) {
        readFails = reg;
        assert(!battery.charging());
    }
    readFails = -1;
    assert(battery.charging());

    allowPowerWrites = true;
    registers[0x27] = 0xFF;
    registers[0x41] = 0xA5;
    registers[0x49] = 0xFF;
    assert(battery.configurePowerButton());
    assert(registers[0x27] == 0xFC && registers[0x41] == 0xAF);
    assert(registers[0x49] == 0xF5); // Preserve all non-owned IRQs.
    assert(!battery.takePowerButtonPress());
    // A boot key held beyond the startup window has no fresh key-down.
    registers[0x49] |= 8;
    assert(!battery.takePowerButtonPress());
    registers[0x49] |= 0x0A;
    writeFails = true;
    assert(!battery.takePowerButtonPress() && registers[0x49] == 0xFF);
    writeFails = false;
    assert(battery.takePowerButtonPress() && registers[0x49] == 0xF5);
    assert(!battery.takePowerButtonPress());
    registers[0x49] |= 2;
    assert(!battery.takePowerButtonPress());
    registers[0x49] |= 8;
    assert(battery.takePowerButtonPress()); // Press/release in separate polls.
    for (int reg : {0x27, 0x41}) {
        readFails = reg;
        assert(!battery.configurePowerButton());
    }
    readFails = 0x49;
    assert(!battery.takePowerButtonPress());
    readFails = 0x10;
    assert(!battery.powerOff());
    readFails = -1;
    registers[0x10] = 0xA4;
    writeFails = true;
    assert(!battery.powerOff() && registers[0x10] == 0xA4);
    writeFails = false;
    assert(battery.powerOff() && registers[0x10] == 0xA5);

    // The boot click is drained, including events during the startup grace.
    Stickmon::Axp2101Battery pmu;
    assert(pmu.begin(bus));
    Amoled::PowerButton key;
    assert(!key.poll(pmu, 0));
    registers[0x49] |= 0x0A;
    assert(!key.poll(pmu, 500));
    assert(!key.poll(pmu, 1000));
    registers[0x49] |= 0x0A;
    assert(key.poll(pmu, 1050));
    assert(!key.poll(pmu, 1100));
    registers[0x49] |= 0x0A;
    assert(!key.poll(pmu, 1101)); // Polling is bounded, IRQ remains pending.
    assert(key.poll(pmu, 1150));
    Amoled::PowerButton wrapped;
    assert(!wrapped.poll(pmu, 0xFFFFFF00U));
    registers[0x49] |= 0x0A;
    assert(wrapped.poll(pmu, 0xFFFFFF00U + 1000U));
}
