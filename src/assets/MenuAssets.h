#pragma once
#include <cstdint>
#include "platform/api/FlashStorage.h"

#ifndef STICKMON_EXTERNAL_MENU_ICONS
#define STICKMON_EXTERNAL_MENU_ICONS 0
#endif

namespace MenuAssets {

static constexpr uint8_t FRAME_W = 40;
static constexpr uint8_t FRAME_H = 40;
static constexpr uint8_t MAIN_ICON_COUNT = 9;
static constexpr uint8_t BOX_ICON_COUNT = 7;

struct RleFrame {
    uint16_t offset;
    uint16_t length;
};

// External builds must preload successfully before entering gameplay. Accessors
// never allocate, open files, or lazily initialize; they return null until ready.
bool begin();
const RleFrame* mainIconFrames();
const uint16_t* mainIconRle();

#if !STICKMON_EXTERNAL_MENU_ICONS
extern const RleFrame MAIN_ICON_FRAMES[] STICKMON_FLASH_DATA;
extern const uint16_t MAIN_ICON_RLE[] STICKMON_FLASH_DATA;
#endif
extern const RleFrame BOX_ICON_FRAMES[] STICKMON_FLASH_DATA;
extern const uint16_t BOX_ICON_RLE[] STICKMON_FLASH_DATA;

}
