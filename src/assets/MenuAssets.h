#pragma once
#include <cstdint>
#include "platform/api/FlashStorage.h"

namespace MenuAssets {

static constexpr uint8_t FRAME_W = 40;
static constexpr uint8_t FRAME_H = 40;
static constexpr uint8_t MAIN_ICON_COUNT = 9;
static constexpr uint8_t BOX_ICON_COUNT = 7;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

extern const RleFrame MAIN_ICON_FRAMES[] STICKMON_FLASH_DATA;
extern const uint16_t MAIN_ICON_RLE[] STICKMON_FLASH_DATA;
extern const RleFrame BOX_ICON_FRAMES[] STICKMON_FLASH_DATA;
extern const uint16_t BOX_ICON_RLE[] STICKMON_FLASH_DATA;

}
