#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HudAssets {

static constexpr uint8_t HUNGER_ICON_W = 14;
static constexpr uint8_t HUNGER_ICON_H = 14;
static constexpr uint16_t HUNGER_ICON_RLE_LEN = 223;

extern const uint16_t HUNGER_ICON_RLE[] PROGMEM;

}
