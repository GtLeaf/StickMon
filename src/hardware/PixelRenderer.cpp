#include "hardware/PixelRenderer.h"
#include <Arduino.h>
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
