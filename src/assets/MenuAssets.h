#pragma once
#include <Arduino.h>
#include <cstdint>

namespace MenuAssets {

static constexpr uint8_t FRAME_W = 32;
static constexpr uint8_t FRAME_H = 32;
static constexpr uint8_t MAIN_ICON_COUNT = 7;
static constexpr uint8_t BOX_ICON_COUNT = 7;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const RleFrame MAIN_ICON_FRAMES[] PROGMEM;
extern const uint16_t MAIN_ICON_RLE[] PROGMEM;
extern const RleFrame BOX_ICON_FRAMES[] PROGMEM;
extern const uint16_t BOX_ICON_RLE[] PROGMEM;

}
