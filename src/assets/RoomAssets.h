#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {

struct RoomPoint {
    int16_t x;
    int16_t y;
};

static constexpr uint16_t STANDARD_ROOM_W = 240;
static constexpr uint16_t STANDARD_ROOM_H = 162;
static constexpr int16_t STANDARD_ROOM_Y = 0;
static constexpr uint32_t STANDARD_ROOM_RLE_LEN = 38882;

static constexpr uint8_t ROOM_WALK_POLYGON_COUNT = 10;
static constexpr int16_t ROOM_WALK_MIN_X = 30;
static constexpr int16_t ROOM_WALK_MIN_Y = 63;
static constexpr int16_t ROOM_WALK_MAX_X = 234;
static constexpr int16_t ROOM_WALK_MAX_Y = 155;

extern const uint16_t STANDARD_ROOM_RLE[] PROGMEM;
extern const RoomPoint ROOM_WALK_POLYGON[] PROGMEM;

}
