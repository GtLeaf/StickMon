#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HudAssets {

static constexpr uint8_t HUNGER_ICON_W = 18;
static constexpr uint8_t HUNGER_ICON_H = 18;
static constexpr uint16_t HUNGER_ICON_RLE_LEN = 369;

extern const uint16_t HUNGER_ICON_RLE[] PROGMEM;

}
