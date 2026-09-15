#pragma once

#include <cstdint>

namespace EspNowRadioConfig {

// ESP-NOW sessions own the 2.4 GHz radio and use one channel on every board.
constexpr uint8_t CHANNEL = 6;

}  // namespace EspNowRadioConfig
