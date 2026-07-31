#pragma once

#include <cstdint>

#if defined(ARDUINO_ARCH_ESP32)
#include <pgmspace.h>
#define STICKMON_FLASH_DATA PROGMEM
#else
#define STICKMON_FLASH_DATA
#endif

namespace FlashStorage {

template <typename T>
inline uint8_t readByte(const T* address) {
#if defined(ARDUINO_ARCH_ESP32)
    return pgm_read_byte(address);
#else
    return static_cast<uint8_t>(*address);
#endif
}

template <typename T>
inline uint16_t readWord(const T* address) {
#if defined(ARDUINO_ARCH_ESP32)
    return pgm_read_word(address);
#else
    return static_cast<uint16_t>(*address);
#endif
}

template <typename T>
inline uint32_t readDword(const T* address) {
#if defined(ARDUINO_ARCH_ESP32)
    return pgm_read_dword(address);
#else
    return static_cast<uint32_t>(*address);
#endif
}

}  // namespace FlashStorage
