#pragma once

#include <cstdint>

struct FontFallbackCNGlyph {
    uint32_t codepoint;
    uint8_t bitmap[32];
};

extern const uint16_t FONT_FALLBACK_CN_COUNT;
const FontFallbackCNGlyph* findFontFallbackCNGlyph(uint32_t codepoint);
