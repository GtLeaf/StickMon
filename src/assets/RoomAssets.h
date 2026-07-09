#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {

struct RoomPoint {
    int16_t x;
    int16_t y;
};

struct RoomPatchRun {
    uint16_t y;
    uint16_t x;
    uint16_t len;
    uint32_t colorOffset;
};

static constexpr uint16_t STANDARD_ROOM_W = 240;
static constexpr uint16_t STANDARD_ROOM_H = 161;
static constexpr int16_t STANDARD_ROOM_Y = 0;
static constexpr uint32_t STANDARD_ROOM_BASE_RAW_BYTES = 77280;
static constexpr uint32_t STANDARD_ROOM_BASE_COMPRESSED_LEN = 29110;
static constexpr uint32_t STANDARD_ROOM_NIGHT_PATCH_RUN_COUNT = 204;
static constexpr uint32_t STANDARD_ROOM_NIGHT_PATCH_PIXEL_COUNT = 2054;
static constexpr bool STANDARD_ROOM_SHARED_RLE = false;

static constexpr uint8_t ROOM_WALK_POLYGON_COUNT = 12;
static constexpr int16_t ROOM_WALK_MIN_X = 37;
static constexpr int16_t ROOM_WALK_MIN_Y = 64;
static constexpr int16_t ROOM_WALK_MAX_X = 234;
static constexpr int16_t ROOM_WALK_MAX_Y = 155;

static constexpr int16_t ROOM_FOOD_X = 45;
static constexpr int16_t ROOM_FOOD_Y = 112;

static constexpr uint8_t ROOM_BED_POLYGON_COUNT = 12;
static constexpr int16_t ROOM_BED_MIN_X = 40;
static constexpr int16_t ROOM_BED_MIN_Y = 86;
static constexpr int16_t ROOM_BED_MAX_X = 108;
static constexpr int16_t ROOM_BED_MAX_Y = 114;
static constexpr int16_t ROOM_BED_X = 74;
static constexpr int16_t ROOM_BED_Y = 100;

extern const uint8_t STANDARD_ROOM_BASE_COMPRESSED[] PROGMEM;
extern const RoomPatchRun STANDARD_ROOM_NIGHT_PATCH_RUNS[] PROGMEM;
extern const uint16_t STANDARD_ROOM_NIGHT_PATCH_PIXELS[] PROGMEM;
extern const RoomPoint ROOM_WALK_POLYGON[] PROGMEM;
extern const RoomPoint ROOM_BED_POLYGON[] PROGMEM;

}
