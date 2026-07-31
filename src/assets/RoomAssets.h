#pragma once
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

}
