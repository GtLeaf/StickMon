#pragma once

#include <cstdint>
#include <cstring>

namespace Platform {

inline uint8_t readProgramByte(const uint8_t* address) {
    uint8_t value = 0;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

inline uint16_t readProgramWord(const uint16_t* address) {
    uint16_t value = 0;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

}  // namespace Platform
