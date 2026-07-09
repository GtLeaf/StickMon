#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {

struct RoomPoint {
    int16_t x;
    int16_t y;
};

static constexpr uint16_t STANDARD_ROOM_W = 240;
static constexpr uint16_t STANDARD_ROOM_H = 163;
static constexpr int16_t STANDARD_ROOM_Y = 0;
static constexpr uint32_t STANDARD_ROOM_DAY_RLE_LEN = 39122;
static constexpr uint32_t STANDARD_ROOM_NIGHT_RLE_LEN = 39122;

static constexpr uint8_t ROOM_WALK_POLYGON_COUNT = 12;
static constexpr int16_t ROOM_WALK_MIN_X = 38;
static constexpr int16_t ROOM_WALK_MIN_Y = 67;
static constexpr int16_t ROOM_WALK_MAX_X = 232;
static constexpr int16_t ROOM_WALK_MAX_Y = 154;

static constexpr int16_t ROOM_FOOD_X = 45;
static constexpr int16_t ROOM_FOOD_Y = 112;

static constexpr uint8_t ROOM_BED_POLYGON_COUNT = 12;
static constexpr int16_t ROOM_BED_MIN_X = 40;
static constexpr int16_t ROOM_BED_MIN_Y = 87;
static constexpr int16_t ROOM_BED_MAX_X = 108;
static constexpr int16_t ROOM_BED_MAX_Y = 112;
static constexpr int16_t ROOM_BED_X = 74;
static constexpr int16_t ROOM_BED_Y = 99;

extern const uint16_t STANDARD_ROOM_DAY_RLE[] PROGMEM;
extern const uint16_t STANDARD_ROOM_NIGHT_RLE[] PROGMEM;
extern const RoomPoint ROOM_WALK_POLYGON[] PROGMEM;
extern const RoomPoint ROOM_BED_POLYGON[] PROGMEM;

}
