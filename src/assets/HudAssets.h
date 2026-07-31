#pragma once
#include <cstdint>
#include "platform/api/FlashStorage.h"

namespace HudAssets {

static constexpr uint8_t HUNGER_ICON_W = 14;
static constexpr uint8_t HUNGER_ICON_H = 14;
static constexpr uint16_t HUNGER_ICON_RLE_LEN = 223;

extern const uint16_t HUNGER_ICON_RLE[] STICKMON_FLASH_DATA;

}
