#pragma once

#include <M5Unified.h>

class PixelRenderer {
public:
    static void bind(LGFX_Sprite* target);
    static LGFX_Sprite& canvas();

    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);
    static void clear(uint16_t color);
    static void text(int x, int y, const char* value, uint16_t color, uint8_t size = 1);
    static void bar(int x, int y, int w, int h, uint8_t value, uint16_t fill, uint16_t bg);
};
