#pragma once

#include <Arduino.h>
#include <cstdint>

namespace PokemonSprites {

enum class SpriteKind : uint8_t {
    ICON_0,
    ICON_1,
    FRONT,
    BACK,
};

struct SpriteFrame {
    uint16_t speciesId;
    uint8_t kind;
    uint8_t width;
    uint8_t height;
    uint32_t offset;
    uint32_t length;
};

extern const uint16_t SPRITE_FRAME_COUNT;
extern const SpriteFrame SPRITE_FRAMES[] PROGMEM;
extern const SpriteFrame EGG_FRAME PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind);

}  // namespace PokemonSprites
