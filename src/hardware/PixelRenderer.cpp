#include "hardware/PixelRenderer.h"
#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include "assets/Font16CN.h"

namespace {
LGFX_Sprite* gCanvas = nullptr;

uint32_t readUtf8(const char*& p) {
    const uint8_t* s = reinterpret_cast<const uint8_t*>(p);
    if (s[0] < 0x80) {
        p += 1;
        return s[0];
    }
    if ((s[0] & 0xE0) == 0xC0) {
        uint32_t cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        p += 2;
        return cp;
    }
    if ((s[0] & 0xF0) == 0xE0) {
        uint32_t cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        p += 3;
        return cp;
    }
    p += 1;
    return '?';
}

void drawCnGlyph(LGFX_Sprite& canvas, int x, int y, const Font16CNGlyph* glyph, uint16_t color) {
    for (int row = 0; row < 16; ++row) {
        uint8_t left = pgm_read_byte(&glyph->bitmap[row * 2]);
        uint8_t right = pgm_read_byte(&glyph->bitmap[row * 2 + 1]);
        for (int col = 0; col < 8; ++col) {
            if (left & (1 << (7 - col))) canvas.drawPixel(x + col, y + row, color);
        }
        for (int col = 0; col < 8; ++col) {
            if (right & (1 << (7 - col))) canvas.drawPixel(x + 8 + col, y + row, color);
        }
    }
}
}

void PixelRenderer::bind(LGFX_Sprite* target) {
    gCanvas = target;
}

LGFX_Sprite& PixelRenderer::canvas() {
    return *gCanvas;
}

uint16_t PixelRenderer::rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void PixelRenderer::clear(uint16_t color) {
    canvas().fillSprite(color);
}

void PixelRenderer::text(int x, int y, const char* value, uint16_t color, uint8_t size) {
    (void)size;
    canvas().setFont(&fonts::AsciiFont8x16);
    canvas().setTextSize(1);
    canvas().setTextColor(color);
    canvas().setTextDatum(top_left);

    int cursor = x;
    const char* p = value;
    while (*p) {
        uint32_t cp = readUtf8(p);
        if (cp == '\n') {
            cursor = x;
            y += 16;
            continue;
        }
        if (cp < 0x80) {
            if (cp == ' ') {
                cursor += 5;
            } else {
                char buf[2] = {(char)cp, '\0'};
                canvas().drawString(buf, cursor, y);
                cursor += 8;
            }
            continue;
        }

        const Font16CNGlyph* glyph = findFont16CNGlyph(cp);
        if (glyph) {
            drawCnGlyph(canvas(), cursor, y, glyph, color);
        } else {
            canvas().drawRect(cursor + 2, y + 2, 12, 12, color);
        }
        cursor += 16;
    }
}

void PixelRenderer::bar(int x, int y, int w, int h, uint8_t value, uint16_t fill, uint16_t bg) {
    canvas().fillRoundRect(x, y, w, h, 2, bg);
    int inner = ((w - 2) * value) / 100;
    if (inner > 0) {
        canvas().fillRoundRect(x + 1, y + 1, inner, h - 2, 1, fill);
    }
    canvas().drawRoundRect(x, y, w, h, 2, rgb(45, 48, 56));
}

void PixelRenderer::drawRgb565Rle(int x, int y, int w, int h,
                                  const uint16_t* data, uint32_t offset,
                                  uint32_t length, bool flipX) {
    if (!gCanvas || !data || w <= 0 || h <= 0) return;

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
            uint16_t color = pgm_read_word(&data[offset + idx++]);
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            gCanvas->drawPixel(x + col, y + row, color);
        }
    }
}

void PixelRenderer::drawIndexed4Rle(int x, int y, int w, int h,
                                    const uint16_t* data, uint32_t offset,
                                    uint32_t length, const uint16_t* palette,
                                    uint32_t paletteOffset, uint8_t paletteSize,
                                    bool flipX) {
    if (!gCanvas || !data || !palette || w <= 0 || h <= 0 || paletteSize == 0) return;

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        uint16_t packed = 0;
        for (uint16_t i = 0; i < run && pixel < total; ++i, ++pixel) {
            if ((i & 0x03) == 0) {
                if (idx >= length) return;
                packed = pgm_read_word(&data[offset + idx++]);
            }
            uint8_t paletteIndex = (packed >> ((i & 0x03) * 4)) & 0x0F;
            if (paletteIndex >= paletteSize) continue;
            uint16_t color = pgm_read_word(&palette[paletteOffset + paletteIndex]);
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            gCanvas->drawPixel(x + col, y + row, color);
        }
    }
}

void PixelRenderer::drawIndexed4RleScaled(int x, int y, int w, int h,
                                          const uint16_t* data, uint32_t offset,
                                          uint32_t length, const uint16_t* palette,
                                          uint32_t paletteOffset, uint8_t paletteSize,
                                          float scale, bool flipX) {
    if (!gCanvas || !data || !palette || w <= 0 || h <= 0 || paletteSize == 0 || scale <= 0.0f) return;
    if (scale == 1.0f) {
        drawIndexed4Rle(x, y, w, h, data, offset, length, palette, paletteOffset, paletteSize, flipX);
        return;
    }
    if (scale < 1.0f) {
        drawIndexed4Rle(x, y, w, h, data, offset, length, palette, paletteOffset, paletteSize, flipX);
        return;
    }

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    int drawW = (int)ceilf(scale);
    int drawH = (int)ceilf(scale);
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        uint16_t packed = 0;
        for (uint16_t i = 0; i < run && pixel < total; ++i, ++pixel) {
            if ((i & 0x03) == 0) {
                if (idx >= length) return;
                packed = pgm_read_word(&data[offset + idx++]);
            }
            uint8_t paletteIndex = (packed >> ((i & 0x03) * 4)) & 0x0F;
            if (paletteIndex >= paletteSize) continue;
            uint16_t color = pgm_read_word(&palette[paletteOffset + paletteIndex]);
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            int drawX = (int)(x + col * scale);
            int drawY = (int)(y + row * scale);
            gCanvas->fillRect(drawX, drawY, drawW, drawH, color);
        }
    }
}

void PixelRenderer::drawRgb565RleScaled(int x, int y, int w, int h,
                                        const uint16_t* data, uint32_t offset,
                                        uint32_t length, float scale, bool flipX) {
    if (!gCanvas || !data || w <= 0 || h <= 0 || scale <= 0.0f) return;
    if (scale == 1.0f) {
        drawRgb565Rle(x, y, w, h, data, offset, length, flipX);
        return;
    }

    if (scale > 1.0f) {
        const uint32_t total = (uint32_t)(w * h);
        uint32_t idx = 0;
        uint32_t pixel = 0;
        while (idx < length && pixel < total) {
            uint16_t token = pgm_read_word(&data[offset + idx++]);
            uint16_t run = token & 0x7FFF;
            if (run == 0) continue;

            if (token & 0x8000) {
                pixel += run;
                if (pixel > total) pixel = total;
                continue;
            }

            for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
                uint16_t color = pgm_read_word(&data[offset + idx++]);
                int col = pixel % w;
                int row = pixel / w;
                if (flipX) col = w - 1 - col;
                int drawX = (int)(x + col * scale);
                int drawY = (int)(y + row * scale);
                int drawW = (int)ceilf(scale);
                int drawH = (int)ceilf(scale);
                gCanvas->fillRect(drawX, drawY, drawW, drawH, color);
            }
        }
        return;
    }

    int outW = (int)(w * scale);
    int outH = (int)(h * scale);
    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    size_t pixelCount = (size_t)w * h;
    uint16_t* buf = (uint16_t*)malloc(pixelCount * sizeof(uint16_t));
    uint8_t* opaque = (uint8_t*)calloc(pixelCount, sizeof(uint8_t));
    if (!buf || !opaque) {
        if (buf) free(buf);
        if (opaque) free(opaque);
        drawRgb565Rle(x, y, w, h, data, offset, length, flipX);
        return;
    }

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token = pgm_read_word(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel += run;
            if (pixel > total) pixel = total;
            continue;
        }

        for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
            buf[pixel] = pgm_read_word(&data[offset + idx++]);
            opaque[pixel] = 1;
        }
    }

    for (int row = 0; row < outH; ++row) {
        int srcRow = (int)(row / scale);
        if (srcRow >= h) srcRow = h - 1;
        for (int col = 0; col < outW; ++col) {
            int srcCol = (int)(col / scale);
            if (srcCol >= w) srcCol = w - 1;
            int finalCol = flipX ? (w - 1 - srcCol) : srcCol;
            size_t srcIndex = (size_t)srcRow * w + finalCol;
            if (opaque[srcIndex]) {
                gCanvas->drawPixel(x + col, y + row, buf[srcIndex]);
            }
        }
    }

    free(opaque);
    free(buf);
}
