#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {

static constexpr uint16_t STANDARD_ROOM_W = 240;
static constexpr uint16_t STANDARD_ROOM_H = 124;
static constexpr int16_t STANDARD_ROOM_Y = 5;
static constexpr uint32_t STANDARD_ROOM_RLE_LEN = 29761;

extern const uint16_t STANDARD_ROOM_RLE[] PROGMEM;

}
