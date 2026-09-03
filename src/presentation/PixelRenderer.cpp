#include "presentation/PixelRenderer.h"
#include <algorithm>
#include <cmath>
#include "assets/FontFallbackCN.h"
#include "core/FontResource.h"
#include "platform/api/ProgramMemory.h"

namespace {
Canvas565 gCanvas;
static constexpr uint8_t MAX_TEXT_OUTLINE_WIDTH = 2;
static constexpr uint8_t NATIVE_TEXT_HEIGHT = 16;
static constexpr uint8_t ASCII_CELL_WIDTH = 8;
static constexpr int MAX_OUTLINE_GLYPH_WIDTH = 16;
static constexpr int MAX_OUTLINE_MASK_WIDTH =
    MAX_OUTLINE_GLYPH_WIDTH + MAX_TEXT_OUTLINE_WIDTH * 2;
static constexpr int MAX_OUTLINE_MASK_HEIGHT =
    NATIVE_TEXT_HEIGHT + MAX_TEXT_OUTLINE_WIDTH * 2;
static constexpr int MAX_OUTLINE_MASK_PIXELS =
    MAX_OUTLINE_MASK_WIDTH * MAX_OUTLINE_MASK_HEIGHT;

bool isUtf8Continuation(uint8_t value) {
    return (value & 0xC0) == 0x80;
}

uint32_t readUtf8(const char*& p) {
    const uint8_t* s = reinterpret_cast<const uint8_t*>(p);
    if (s[0] < 0x80) {
        p += 1;
        return s[0];
    }
    if (s[0] >= 0xC2 && s[0] <= 0xDF &&
        s[1] != 0 && isUtf8Continuation(s[1])) {
        uint32_t cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        p += 2;
        return cp;
    }
    if (s[0] >= 0xE0 && s[0] <= 0xEF &&
        s[1] != 0 && isUtf8Continuation(s[1]) &&
        s[2] != 0 && isUtf8Continuation(s[2])) {
        uint32_t cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        if (cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF)) {
            p += 3;
            return cp;
        }
    }
    if (s[0] >= 0xF0 && s[0] <= 0xF4 &&
        s[1] != 0 && isUtf8Continuation(s[1]) &&
        s[2] != 0 && isUtf8Continuation(s[2]) &&
        s[3] != 0 && isUtf8Continuation(s[3])) {
        uint32_t cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                      ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        if (cp >= 0x10000 && cp <= 0x10FFFF) {
            p += 4;
            return cp;
        }
    }
    p += 1;
    return '?';
}

bool mapDownscaledCoordinate(int source, float scale, int outputSize, int& output) {
    output = static_cast<int>(ceilf(source * scale));
    return output >= 0 && output < outputSize &&
           static_cast<int>(output / scale) == source;
}

uint16_t readGlyphRow(const uint8_t* bitmap, int row, bool progmem) {
    const uint8_t left = progmem
        ? Platform::readProgramByte(&bitmap[row * 2])
        : bitmap[row * 2];
    const uint8_t right = progmem
        ? Platform::readProgramByte(&bitmap[row * 2 + 1])
        : bitmap[row * 2 + 1];
    return static_cast<uint16_t>(left) << 8 | right;
}

void drawGlyphBitmap(Canvas565& canvas, int x, int y, const uint8_t* bitmap,
                     uint16_t color, bool progmem, int glyphWidth) {
    for (int row = 0; row < NATIVE_TEXT_HEIGHT; ++row) {
        const uint16_t bits = readGlyphRow(bitmap, row, progmem);
        for (int col = 0; col < glyphWidth; ++col) {
            if (bits & (1U << (15 - col))) canvas.drawPixel(x + col, y + row, color);
        }
    }
}

void drawOuterOutline(Canvas565& canvas, int x, int y,
                      const uint32_t* glyphRows, int glyphWidth, int glyphHeight,
                      uint16_t color, uint8_t width) {
    static constexpr int PAD = MAX_TEXT_OUTLINE_WIDTH;
    uint32_t originalRows[MAX_OUTLINE_MASK_HEIGHT] = {};
    uint32_t expandedRows[MAX_OUTLINE_MASK_HEIGHT] = {};
    uint32_t exteriorRows[MAX_OUTLINE_MASK_HEIGHT] = {};
    uint16_t queue[MAX_OUTLINE_MASK_PIXELS];
    const int maskWidth = glyphWidth + PAD * 2;
    const int maskHeight = glyphHeight + PAD * 2;
    const uint32_t validColumns = (1UL << maskWidth) - 1;

    for (int row = 0; row < glyphHeight; ++row) {
        const uint32_t positioned = (glyphRows[row] << PAD) & validColumns;
        originalRows[row + PAD] = positioned;

        for (int dy = -width; dy <= width; ++dy) {
            const uint8_t horizontalWidth = width - std::abs(dy);
            uint32_t horizontal = positioned;
            for (uint8_t offset = 1; offset <= horizontalWidth; ++offset) {
                horizontal |= positioned << offset;
                horizontal |= positioned >> offset;
            }
            expandedRows[row + PAD + dy] |= horizontal & validColumns;
        }
    }

    uint16_t head = 0;
    uint16_t tail = 0;
    auto pushExterior = [&](int row, int col) {
        const uint32_t bit = 1UL << col;
        if ((originalRows[row] & bit) != 0 || (exteriorRows[row] & bit) != 0) return;
        exteriorRows[row] |= bit;
        queue[tail++] = static_cast<uint16_t>((row << 5) | col);
    };

    for (int col = 0; col < maskWidth; ++col) {
        pushExterior(0, col);
        pushExterior(maskHeight - 1, col);
    }
    for (int row = 1; row + 1 < maskHeight; ++row) {
        pushExterior(row, 0);
        pushExterior(row, maskWidth - 1);
    }

    static constexpr int8_t NEIGHBOR_X[] = {-1, 1, 0, 0};
    static constexpr int8_t NEIGHBOR_Y[] = {0, 0, -1, 1};
    while (head < tail) {
        const uint16_t packed = queue[head++];
        const int row = packed >> 5;
        const int col = packed & 0x1F;
        for (uint8_t i = 0; i < 4; ++i) {
            const int nextRow = row + NEIGHBOR_Y[i];
            const int nextCol = col + NEIGHBOR_X[i];
            if (nextRow < 0 || nextRow >= maskHeight ||
                nextCol < 0 || nextCol >= maskWidth) {
                continue;
            }
            pushExterior(nextRow, nextCol);
        }
    }

    for (int row = 0; row < maskHeight; ++row) {
        const uint32_t visibleOutline = expandedRows[row] & exteriorRows[row];
        for (int col = 0; col < maskWidth; ++col) {
            if (visibleOutline & (1UL << col)) {
                canvas.drawPixel(x + col - PAD, y + row - PAD, color);
            }
        }
    }
}

void drawGlyphOutline(Canvas565& canvas, int x, int y, const uint8_t* bitmap,
                      uint16_t color, bool progmem, int glyphWidth,
                      uint8_t width) {
    uint32_t glyphRows[NATIVE_TEXT_HEIGHT] = {};
    for (int row = 0; row < NATIVE_TEXT_HEIGHT; ++row) {
        const uint16_t bits = readGlyphRow(bitmap, row, progmem);
        for (int col = 0; col < glyphWidth; ++col) {
            if (bits & (1U << (15 - col))) glyphRows[row] |= 1UL << col;
        }
    }
    drawOuterOutline(canvas, x, y, glyphRows, glyphWidth,
                     NATIVE_TEXT_HEIGHT, color, width);
}

void drawGlyphPass(Canvas565& canvas, int x, int y, const uint8_t* bitmap,
                   uint16_t color, bool progmem, int glyphWidth,
                   uint8_t outlineWidth) {
    if (outlineWidth > 0) {
        drawGlyphOutline(canvas, x, y, bitmap, color, progmem, glyphWidth,
                         outlineWidth);
    } else {
        drawGlyphBitmap(canvas, x, y, bitmap, color, progmem, glyphWidth);
    }
}

bool usesUnscii(uint32_t codepoint) {
    return codepoint < 0x80 || codepoint == 0x2640 || codepoint == 0x2642;
}

int glyphAdvance(uint32_t codepoint) {
    return usesUnscii(codepoint) ? ASCII_CELL_WIDTH : NATIVE_TEXT_HEIGHT;
}

void drawTextPass(Canvas565& target, int x, int y, const char* value,
                  uint16_t color,
                  uint8_t outlineWidth = 0) {
    int cursor = x;
    const char* p = value;
    while (*p) {
        const uint32_t codepoint = readUtf8(p);
        if (codepoint == '\n') {
            cursor = x;
            y += NATIVE_TEXT_HEIGHT;
            continue;
        }
        if (codepoint == ' ') {
            cursor += glyphAdvance(codepoint);
            continue;
        }

        const bool unscii = usesUnscii(codepoint);
        const int glyphWidth = unscii ? ASCII_CELL_WIDTH : NATIVE_TEXT_HEIGHT;
        const FontFace face = unscii
            ? FontFace::UNSCII_ASCII
            : FontFace::SARASA_CJK;
        const uint8_t* bitmap =
            FontResource::ins().findGlyphBitmap(codepoint, face);
        if (bitmap) {
            drawGlyphPass(target, cursor, y, bitmap, color,
                          false, glyphWidth, outlineWidth);
        } else if (!unscii) {
            const FontFallbackCNGlyph* glyph = findFontFallbackCNGlyph(codepoint);
            if (glyph) {
                drawGlyphPass(target, cursor, y, glyph->bitmap,
                              color, true, glyphWidth, outlineWidth);
            } else {
                target.drawRect(cursor + 1, y + 2, 12, 12, color);
            }
        } else {
            static constexpr int boxWidth = 6;
            static constexpr int boxHeight = 12;
            static constexpr int boxY = 2;
            if (outlineWidth > 0) {
                for (int offset = -outlineWidth; offset <= outlineWidth; ++offset) {
                    target.drawRect(cursor + 1 + offset, y + boxY,
                                                     boxWidth, boxHeight, color);
                    target.drawRect(cursor + 1, y + boxY + offset,
                                                     boxWidth, boxHeight, color);
                }
            } else {
                target.drawRect(cursor + 1, y + boxY,
                                                 boxWidth, boxHeight, color);
            }
        }
        cursor += glyphAdvance(codepoint);
    }
}

uint32_t readLargeGlyphRow(const uint8_t* bitmap, int row) {
    const uint8_t* bytes = bitmap + row * 4;
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           bytes[3];
}

void drawNativeGlyphBitmap(Canvas565& canvas, int x, int y,
                           const uint8_t* bitmap, uint16_t color,
                           int glyphWidth) {
    if (!bitmap) return;
    const int originX = x * canvas.coordinateScale();
    const int originY = y * canvas.coordinateScale();
    for (int row = 0; row < FontResource::LARGE_GLYPH_H; ++row) {
        const uint32_t bits = readLargeGlyphRow(bitmap, row);
        for (int col = 0; col < glyphWidth; ++col) {
            if (bits & (1UL << (31 - col))) {
                canvas.drawPhysicalPixel(originX + col, originY + row, color);
            }
        }
    }
}

void drawNativeLegacyGlyph(Canvas565& canvas, int x, int y,
                           const uint8_t* bitmap, uint16_t color,
                           int glyphWidth) {
    if (!bitmap) return;
    const int scale = canvas.coordinateScale();
    const int originX = x * scale;
    const int originY = y * scale;
    for (int row = 0; row < NATIVE_TEXT_HEIGHT; ++row) {
        const uint16_t bits = readGlyphRow(bitmap, row, false);
        for (int col = 0; col < glyphWidth; ++col) {
            if ((bits & (1U << (15 - col))) == 0) continue;
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    canvas.drawPhysicalPixel(originX + col * scale + dx,
                                             originY + row * scale + dy,
                                             color);
                }
            }
        }
    }
}

void drawNativeFallbackBox(Canvas565& canvas, int x, int y,
                           uint16_t color, int width, int height) {
    const int scale = canvas.coordinateScale();
    const int originX = x * scale;
    const int originY = y * scale;
    for (int row = 0; row < height * scale; ++row) {
        for (int col = 0; col < width * scale; ++col) {
            if (row == 0 || row == height * scale - 1 ||
                col == 0 || col == width * scale - 1) {
                canvas.drawPhysicalPixel(originX + col, originY + row, color);
            }
        }
    }
}

void drawNativeTextPass(Canvas565& target, int x, int y, const char* value,
                        uint16_t color, int offsetX = 0, int offsetY = 0) {
    int cursor = x;
    const char* p = value;
    while (*p) {
        const uint32_t codepoint = readUtf8(p);
        if (codepoint == '\n') {
            cursor = x;
            y += NATIVE_TEXT_HEIGHT;
            continue;
        }
        if (codepoint == ' ') {
            cursor += glyphAdvance(codepoint);
            continue;
        }

        const bool unscii = usesUnscii(codepoint);
        const int glyphWidth = unscii ? ASCII_CELL_WIDTH * 2
                                      : FontResource::LARGE_GLYPH_W;
        const FontFace face = unscii
            ? FontFace::UNSCII_ASCII
            : FontFace::SARASA_CJK;
        const uint8_t* bitmap = FontResource::ins().findLargeGlyphBitmap(
            codepoint, face);
        if (bitmap) {
            drawNativeGlyphBitmap(target, cursor + offsetX, y + offsetY,
                                  bitmap, color, glyphWidth);
        } else if (!unscii) {
            const FontFallbackCNGlyph* fallback =
                findFontFallbackCNGlyph(codepoint);
            if (fallback) {
                drawNativeLegacyGlyph(target, cursor + offsetX, y + offsetY,
                                      fallback->bitmap, color, NATIVE_TEXT_HEIGHT);
            } else {
                drawNativeFallbackBox(target, cursor + offsetX + 1,
                                      y + offsetY + 2, color, 12, 12);
            }
        } else {
            drawNativeFallbackBox(target, cursor + offsetX + 1,
                                  y + offsetY + 2, color, 6, 12);
        }
        cursor += glyphAdvance(codepoint);
    }
}

uint8_t rgb565R(uint16_t color) {
    return static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
}

uint8_t rgb565G(uint16_t color) {
    return static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
}

uint8_t rgb565B(uint16_t color) {
    return static_cast<uint8_t>((color & 0x1F) * 255 / 31);
}

uint16_t blendRgb565(uint16_t dst, uint16_t src, uint8_t alpha) {
    const uint8_t inv = static_cast<uint8_t>(255 - alpha);
    const uint8_t red = static_cast<uint8_t>(
        (rgb565R(src) * alpha + rgb565R(dst) * inv) / 255);
    const uint8_t green = static_cast<uint8_t>(
        (rgb565G(src) * alpha + rgb565G(dst) * inv) / 255);
    const uint8_t blue = static_cast<uint8_t>(
        (rgb565B(src) * alpha + rgb565B(dst) * inv) / 255);
    return PixelRenderer::rgb(red, green, blue);
}
}

void PixelRenderer::bind(const Platform::FrameBuffer565& target) {
    gCanvas.attach(target);
}

void PixelRenderer::setCoordinateScale(uint8_t scale) {
    gCanvas.setCoordinateScale(scale);
}

Canvas565& PixelRenderer::canvas() {
    return gCanvas;
}

uint16_t PixelRenderer::rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void PixelRenderer::clear(uint16_t color) {
    canvas().fillSprite(color);
}

void PixelRenderer::darken(uint8_t amount) {
    if (!gCanvas.attached() || amount == 0) return;
    if (amount == 255) {
        gCanvas.fillSprite(0);
        return;
    }

    uint16_t* pixels = gCanvas.rawPixels();
    if (!pixels) return;

    const uint16_t keep = 255 - amount;
    const uint32_t count = gCanvas.pixelCount();
    for (uint32_t i = 0; i < count; ++i) {
        const uint16_t color = gCanvas.decodeColor(pixels[i]);
        const uint16_t red = static_cast<uint16_t>((((color >> 11) & 0x1F) * keep) / 255);
        const uint16_t green = static_cast<uint16_t>((((color >> 5) & 0x3F) * keep) / 255);
        const uint16_t blue = static_cast<uint16_t>(((color & 0x1F) * keep) / 255);
        const uint16_t darkened = static_cast<uint16_t>((red << 11) | (green << 5) | blue);
        pixels[i] = gCanvas.encodeColor(darkened);
    }
}

void PixelRenderer::fillRectAlpha(int x, int y, int w, int h,
                                  uint16_t color, uint8_t alpha) {
    if (!gCanvas.attached() || w <= 0 || h <= 0 || alpha == 0) return;
    if (alpha == 255) {
        gCanvas.fillRect(x, y, w, h, color);
        return;
    }

    const int left = x < 0 ? 0 : x;
    const int top = y < 0 ? 0 : y;
    const int right = x + w > gCanvas.width() ? gCanvas.width() : x + w;
    const int bottom = y + h > gCanvas.height() ? gCanvas.height() : y + h;
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            const uint16_t background = gCanvas.readPixel(px, py);
            gCanvas.drawPixel(px, py, blendRgb565(background, color, alpha));
        }
    }
}

void PixelRenderer::text(int x, int y, const char* value, uint16_t color,
                         uint8_t size) {
    (void)size;
    if (gCanvas.coordinateScale() >= 2) {
        drawNativeTextPass(gCanvas, x, y, value, color);
    } else {
        drawTextPass(gCanvas, x, y, value, color);
    }
}

void PixelRenderer::text(Canvas565& target, int x, int y, const char* value,
                         uint16_t color, uint8_t size) {
    (void)size;
    if (target.coordinateScale() >= 2) {
        drawNativeTextPass(target, x, y, value, color);
    } else {
        drawTextPass(target, x, y, value, color);
    }
}

void PixelRenderer::textOutlined(int x, int y, const char* value, uint16_t color,
                                 uint16_t outline, uint8_t outlineWidth,
                                 uint8_t size) {
    (void)size;
    if (gCanvas.coordinateScale() >= 2) {
        if (outlineWidth == 0) {
            drawNativeTextPass(gCanvas, x, y, value, color);
            return;
        }
        if (outlineWidth > MAX_TEXT_OUTLINE_WIDTH) {
            outlineWidth = MAX_TEXT_OUTLINE_WIDTH;
        }
        for (int offsetY = -outlineWidth; offsetY <= outlineWidth; ++offsetY) {
            for (int offsetX = -outlineWidth; offsetX <= outlineWidth; ++offsetX) {
                if (offsetX == 0 && offsetY == 0) continue;
                drawNativeTextPass(gCanvas, x, y, value, outline,
                                   offsetX, offsetY);
            }
        }
        drawNativeTextPass(gCanvas, x, y, value, color);
        return;
    }
    if (outlineWidth == 0) {
        drawTextPass(gCanvas, x, y, value, color);
        return;
    }
    if (outlineWidth > MAX_TEXT_OUTLINE_WIDTH) outlineWidth = MAX_TEXT_OUTLINE_WIDTH;
    drawTextPass(gCanvas, x, y, value, outline, outlineWidth);
    drawTextPass(gCanvas, x, y, value, color);
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
    gCanvas.drawRgb565Rle(x, y, w, h, data, offset, length, flipX);
}

void PixelRenderer::drawIndexed4Rle(int x, int y, int w, int h,
                                    const uint16_t* data, uint32_t offset,
                                    uint32_t length, const uint16_t* palette,
                                    uint32_t paletteOffset, uint8_t paletteSize,
                                    bool flipX) {
    if (!gCanvas.attached() || !data || !palette || w <= 0 || h <= 0 ||
        paletteSize == 0) return;

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token =
            Platform::readProgramWord(&data[offset + idx++]);
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
                packed =
                    Platform::readProgramWord(&data[offset + idx++]);
            }
            uint8_t paletteIndex = (packed >> ((i & 0x03) * 4)) & 0x0F;
            if (paletteIndex >= paletteSize) continue;
            uint16_t color = Platform::readProgramWord(
                &palette[paletteOffset + paletteIndex]);
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            gCanvas.drawPixel(x + col, y + row, color);
        }
    }
}

void PixelRenderer::drawRgb565RleSolid(int x, int y, int w, int h,
                                       const uint16_t* data, uint32_t offset,
                                       uint32_t length, uint16_t color,
                                       bool flipX) {
    if (!gCanvas.attached() || !data || w <= 0 || h <= 0) return;

    const uint32_t total = static_cast<uint32_t>(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token =
            Platform::readProgramWord(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;
        if (token & 0x8000) {
            pixel = std::min<uint32_t>(total, pixel + run);
            continue;
        }

        for (uint16_t i = 0;
             i < run && idx < length && pixel < total;
             ++i, ++idx, ++pixel) {
            int col = static_cast<int>(pixel % w);
            int row = static_cast<int>(pixel / w);
            if (flipX) col = w - 1 - col;
            gCanvas.drawPixel(x + col, y + row, color);
        }
    }
}

void PixelRenderer::drawIndexed4RleSolid(int x, int y, int w, int h,
                                         const uint16_t* data, uint32_t offset,
                                         uint32_t length, uint16_t color,
                                         bool flipX) {
    if (!gCanvas.attached() || !data || w <= 0 || h <= 0) return;

    const uint32_t total = static_cast<uint32_t>(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token =
            Platform::readProgramWord(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;
        if (token & 0x8000) {
            pixel = std::min<uint32_t>(total, pixel + run);
            continue;
        }

        uint16_t packedPixels = static_cast<uint16_t>((run + 3U) / 4U);
        if (idx + packedPixels > length) return;
        for (uint16_t i = 0; i < run && pixel < total; ++i, ++pixel) {
            int col = static_cast<int>(pixel % w);
            int row = static_cast<int>(pixel / w);
            if (flipX) col = w - 1 - col;
            gCanvas.drawPixel(x + col, y + row, color);
        }
        idx += packedPixels;
    }
}

void PixelRenderer::drawIndexed4RleScaled(int x, int y, int w, int h,
                                          const uint16_t* data, uint32_t offset,
                                          uint32_t length, const uint16_t* palette,
                                          uint32_t paletteOffset, uint8_t paletteSize,
                                          float scale, bool flipX, uint8_t alpha) {
    if (!gCanvas.attached() || !data || !palette || w <= 0 || h <= 0 ||
        paletteSize == 0 || scale <= 0.0f || alpha == 0) {
        return;
    }
    if (scale == 1.0f && alpha == 255) {
        drawIndexed4Rle(x, y, w, h, data, offset, length, palette, paletteOffset, paletteSize, flipX);
        return;
    }
    if (scale < 1.0f) {
        int outW = std::max<int>(1, static_cast<int>(w * scale));
        int outH = std::max<int>(1, static_cast<int>(h * scale));
        const uint32_t total = static_cast<uint32_t>(w * h);
        uint32_t idx = 0;
        uint32_t pixel = 0;
        while (idx < length && pixel < total) {
            uint16_t token =
                Platform::readProgramWord(&data[offset + idx++]);
            uint16_t run = token & 0x7FFF;
            if (run == 0) continue;
            if (token & 0x8000) {
                pixel = std::min<uint32_t>(total, pixel + run);
                continue;
            }

            uint16_t packed = 0;
            for (uint16_t i = 0; i < run && pixel < total; ++i, ++pixel) {
                if ((i & 0x03) == 0) {
                    if (idx >= length) return;
                    packed =
                        Platform::readProgramWord(&data[offset + idx++]);
                }
                uint8_t paletteIndex = (packed >> ((i & 0x03) * 4)) & 0x0F;
                if (paletteIndex >= paletteSize) continue;

                int sourceCol = static_cast<int>(pixel % w);
                int sourceRow = static_cast<int>(pixel / w);
                int sampledCol = flipX ? w - 1 - sourceCol : sourceCol;
                int drawCol = 0;
                int drawRow = 0;
                if (!mapDownscaledCoordinate(sampledCol, scale, outW, drawCol) ||
                    !mapDownscaledCoordinate(sourceRow, scale, outH, drawRow)) {
                    continue;
                }
                uint16_t color = Platform::readProgramWord(
                    &palette[paletteOffset + paletteIndex]);
                int drawX = x + drawCol;
                int drawY = y + drawRow;
                if (alpha == 255) {
                    gCanvas.drawPixel(drawX, drawY, color);
                } else if (drawX >= 0 && drawY >= 0 &&
                           drawX < gCanvas.width() && drawY < gCanvas.height()) {
                    uint16_t background =
                        gCanvas.readPixel(drawX, drawY);
                    gCanvas.drawPixel(
                        drawX, drawY, blendRgb565(background, color, alpha));
                }
            }
        }
        return;
    }

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token =
            Platform::readProgramWord(&data[offset + idx++]);
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
                packed =
                    Platform::readProgramWord(&data[offset + idx++]);
            }
            uint8_t paletteIndex = (packed >> ((i & 0x03) * 4)) & 0x0F;
            if (paletteIndex >= paletteSize) continue;
            uint16_t color = Platform::readProgramWord(
                &palette[paletteOffset + paletteIndex]);
            int col = pixel % w;
            int row = pixel / w;
            if (flipX) col = w - 1 - col;
            // Map each source pixel to its exact destination interval. Using
            // ceil(scale) for every pixel makes 1.66x backgrounds expand to
            // 2x and breaks the intended crop on the portrait display.
            int drawX = x + static_cast<int>(std::floor(col * scale));
            int drawY = y + static_cast<int>(std::floor(row * scale));
            int nextX = x + static_cast<int>(std::ceil((col + 1) * scale));
            int nextY = y + static_cast<int>(std::ceil((row + 1) * scale));
            int drawW = std::max(1, nextX - drawX);
            int drawH = std::max(1, nextY - drawY);
            if (alpha == 255) {
                gCanvas.fillRect(drawX, drawY, drawW, drawH, color);
            } else {
                fillRectAlpha(drawX, drawY, drawW, drawH, color, alpha);
            }
        }
    }
}

void PixelRenderer::drawRgb565RleScaled(int x, int y, int w, int h,
                                        const uint16_t* data, uint32_t offset,
                                        uint32_t length, float scale, bool flipX) {
    if (!gCanvas.attached() || !data || w <= 0 || h <= 0 ||
        scale <= 0.0f) return;
    if (scale == 1.0f) {
        drawRgb565Rle(x, y, w, h, data, offset, length, flipX);
        return;
    }

    if (scale > 1.0f) {
        const uint32_t total = (uint32_t)(w * h);
        uint32_t idx = 0;
        uint32_t pixel = 0;
        while (idx < length && pixel < total) {
            uint16_t token =
                Platform::readProgramWord(&data[offset + idx++]);
            uint16_t run = token & 0x7FFF;
            if (run == 0) continue;

            if (token & 0x8000) {
                pixel += run;
                if (pixel > total) pixel = total;
                continue;
            }

            for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
                uint16_t color =
                    Platform::readProgramWord(&data[offset + idx++]);
                int col = pixel % w;
                int row = pixel / w;
                if (flipX) col = w - 1 - col;
                int drawX = (int)(x + col * scale);
                int drawY = (int)(y + row * scale);
                int drawW = (int)ceilf(scale);
                int drawH = (int)ceilf(scale);
                gCanvas.fillRect(drawX, drawY, drawW, drawH, color);
            }
        }
        return;
    }

    int outW = (int)(w * scale);
    int outH = (int)(h * scale);
    if (outW <= 0) outW = 1;
    if (outH <= 0) outH = 1;

    const uint32_t total = (uint32_t)(w * h);
    uint32_t idx = 0;
    uint32_t pixel = 0;
    while (idx < length && pixel < total) {
        uint16_t token =
            Platform::readProgramWord(&data[offset + idx++]);
        uint16_t run = token & 0x7FFF;
        if (run == 0) continue;

        if (token & 0x8000) {
            pixel = std::min<uint32_t>(total, pixel + run);
            continue;
        }

        for (uint16_t i = 0; i < run && idx < length && pixel < total; ++i, ++pixel) {
            uint16_t color =
                Platform::readProgramWord(&data[offset + idx++]);
            int sourceCol = static_cast<int>(pixel % w);
            int sourceRow = static_cast<int>(pixel / w);
            int sampledCol = flipX ? w - 1 - sourceCol : sourceCol;
            int drawCol = 0;
            int drawRow = 0;
            if (mapDownscaledCoordinate(sampledCol, scale, outW, drawCol) &&
                mapDownscaledCoordinate(sourceRow, scale, outH, drawRow)) {
                gCanvas.drawPixel(x + drawCol, y + drawRow, color);
            }
        }
    }
}
