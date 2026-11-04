#pragma once

#include "presentation/Canvas565.h"

class PixelRenderer {
public:
    static void bind(const Platform::FrameBuffer565& target);
    static void setCoordinateScale(uint8_t scale);
    static Canvas565& canvas();

    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);
    static void clear(uint16_t color);
    static void darken(uint8_t amount);
    static void fillRectAlpha(int x, int y, int w, int h,
                              uint16_t color, uint8_t alpha);
    // Stick and desktop targets use the 16px font path. AMOLED targets use a
    // dedicated physical 32px font when the canvas layout scale is 2.
    // The final argument is retained for compatibility with existing call
    // sites and does not control the physical font size.
    static void text(int x, int y, const char* value, uint16_t color,
                     uint8_t size = 1);
    static void text(Canvas565& target, int x, int y, const char* value,
                     uint16_t color, uint8_t size = 1);
    // Opt-in coverage text; 16/20px are native, 17-19px are animation only.
    // Missing optional glyphs fall back to the original 16px bitmap.
    static void textSmooth(int x, int y, const char* value, uint16_t color,
                           uint8_t height);
    static void textOutlined(int x, int y, const char* value, uint16_t color,
                             uint16_t outline, uint8_t outlineWidth = 1,
                             uint8_t size = 1);
    static void bar(int x, int y, int w, int h, uint8_t value, uint16_t fill, uint16_t bg);
    static void drawRgb565Rle(int x, int y, int w, int h,
                              const uint16_t* data, uint32_t offset,
                              uint32_t length, bool flipX = false);
    static void drawIndexed4Rle(int x, int y, int w, int h,
                                const uint16_t* data, uint32_t offset,
                                uint32_t length, const uint16_t* palette,
                                uint32_t paletteOffset, uint8_t paletteSize,
                                bool flipX = false);
    static void drawRgb565RleSolid(int x, int y, int w, int h,
                                   const uint16_t* data, uint32_t offset,
                                   uint32_t length, uint16_t color,
                                   bool flipX = false);
    static void drawIndexed4RleSolid(int x, int y, int w, int h,
                                     const uint16_t* data, uint32_t offset,
                                     uint32_t length, uint16_t color,
                                     bool flipX = false);
    // Opt-in nearest sampling gives fractional exploration zoom nonoverlapping
    // pixel intervals; existing callers retain their original coverage rules.
    static void drawIndexed4RleScaled(int x, int y, int w, int h,
                                      const uint16_t* data, uint32_t offset,
                                      uint32_t length, const uint16_t* palette,
                                      uint32_t paletteOffset, uint8_t paletteSize,
                                      float scale, bool flipX = false,
                                      uint8_t alpha = 255, bool nearest = false);
    static void drawRgb565RleScaled(int x, int y, int w, int h,
                                    const uint16_t* data, uint32_t offset,
                                    uint32_t length, float scale, bool flipX = false,
                                    bool nearest = false);
};
