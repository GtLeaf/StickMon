#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "platform/api/FlashStorage.h"
#include "presentation/Canvas565.h"

// Opt-in nearest sampling for animated art. Color and silhouette deliberately
// share one geometry; transparent RLE skips never become opaque (black can be).
namespace Ui::TintedSpriteRle {
struct Source {
    const uint16_t* data;
    uint32_t offset, length;
    const uint16_t* palette;
    uint32_t paletteOffset;
    uint8_t paletteSize;
    int width, height;
    bool indexed;
};
inline uint16_t blend(uint16_t a, uint16_t b, uint8_t t) {
    return uint16_t((((((a >> 11) & 31) * (255-t) + ((b >> 11) & 31) * t + 127) / 255) << 11) |
        (((((a >> 5) & 63) * (255-t) + ((b >> 5) & 63) * t + 127) / 255) << 5) |
        (((a & 31) * (255-t) + (b & 31) * t + 127) / 255));
}
inline bool draw(Canvas565& canvas, const Source& s, int x, int y,
                 float scale, uint16_t tint, uint8_t amount, bool flipX = false) {
    if (!canvas.attached() || !s.data || !s.length || s.width <= 0 || s.height <= 0 ||
        s.width > 255 || s.height > 255 || !std::isfinite(scale) || scale <= 0 ||
        s.width * scale > 32767 || s.height * scale > 32767 ||
        (s.indexed && (!s.palette || !s.paletteSize || s.paletteSize > 16))) return false;
    const int dw = std::max(1, int(std::lround(s.width * scale)));
    const int dh = std::max(1, int(std::lround(s.height * scale)));
    // Clip bounds are logical canvas coordinates; fillAssetRect applies
    // assetScale here and the canvas applies its raster/layout scale later.
    const int raster = canvas.assetScale();
    if ((int64_t(x) + dw) * raster <= canvas.clipLeft() ||
        int64_t(x) * raster >= canvas.clipRight() ||
        (int64_t(y) + dh) * raster <= canvas.clipTop() ||
        int64_t(y) * raster >= canvas.clipBottom()) return true;
    // Invert the destination's nearest sampling once, including rounded last
    // pixels and tiny scales. Fixed-size maps, no raster or heap allocation.
    int16_t xs[256], ys[256];
    int dest = 0;
    for (int i = 0; i <= s.width; ++i) {
        while (dest < dw && std::min(s.width-1, int(dest / scale)) < i) ++dest;
        xs[i] = int16_t(dest);
    }
    dest = 0;
    for (int i = 0; i <= s.height; ++i) {
        while (dest < dh && std::min(s.height-1, int(dest / scale)) < i) ++dest;
        ys[i] = int16_t(dest);
    }
    uint16_t colors[16]{};
    if (s.indexed) for (int i = 0; i < s.paletteSize; ++i)
        colors[i] = blend(FlashStorage::readWord(s.palette + s.paletteOffset + i), tint, amount);
    uint32_t cursor = 0, pixel = 0;
    const uint32_t count = uint32_t(s.width * s.height);
    while (cursor < s.length && pixel < count) {
        const uint16_t token = FlashStorage::readWord(s.data + s.offset + cursor++);
        const uint32_t run = token & 0x7fffU;
        if (!run || run > count - pixel) return false;
        if (token & 0x8000U) { pixel += run; continue; }
        const uint32_t words = s.indexed ? (run + 3) / 4 : run;
        if (words > s.length - cursor) return false;
        for (uint32_t i = 0; i < run; ++i, ++pixel) {
            const int row = int(pixel / s.width), col = int(pixel % s.width);
            const int dc = flipX ? s.width - 1 - col : col;
            const int left = x + xs[dc], top = y + ys[row];
            const int w = xs[dc+1] - xs[dc], h = ys[row+1] - ys[row];
            if (!w || !h || (int64_t(top) + h)*raster <= canvas.clipTop() ||
                int64_t(top)*raster >= canvas.clipBottom() ||
                (int64_t(left) + w)*raster <= canvas.clipLeft() ||
                int64_t(left)*raster >= canvas.clipRight()) continue;
            const uint16_t word = FlashStorage::readWord(s.data + s.offset + cursor + (s.indexed ? i/4 : i));
            const unsigned index = (word >> ((i%4)*4)) & 15U;
            if (s.indexed && index >= s.paletteSize) return false;
            canvas.fillAssetRect(left, top, w, h,
                s.indexed ? colors[index] : blend(word, tint, amount));
        }
        cursor += words;
    }
    return pixel == count;
}
} // namespace Ui::TintedSpriteRle
