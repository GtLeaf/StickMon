#pragma once

#include <cstdint>

struct Font16CNGlyph {
    uint32_t codepoint;
    uint8_t bitmap[32];
};

extern const uint16_t FONT16CN_COUNT;
const Font16CNGlyph* findFont16CNGlyph(uint32_t codepoint);
