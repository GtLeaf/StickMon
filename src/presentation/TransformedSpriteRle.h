#pragma once

#include "presentation/TintedSpriteRle.h"

// A cropped image retains the sampling grid and paint order of its original
// transparent canvas. In particular a mirrored stored pose must be visited in
// logical source order before applying the caller's whole-canvas mirror: the
// legacy fractional non-nearest intervals overlap and paint order is visible.
namespace Ui::TransformedSpriteRle {
struct Geometry {
    int width, height;
    int offsetX, offsetY;
    bool imageFlipX;
};
enum class Mode : uint8_t { Normal, Solid, Tinted };

inline bool downsample(int source, float scale, int size, int& result) {
    result = int(std::ceil(source * scale));
    return result >= 0 && result < size && int(result / scale) == source;
}

inline void interval(int origin, int source, int count, float scale,
                     bool indexed, bool nearest, int& start, int& size) {
    if (nearest) {
        start = origin + int(std::ceil(source * scale));
        size = std::min(int(std::ceil((source + 1) * scale)),
                        int(std::lround(count * scale))) - (start - origin);
    } else if (indexed) {
        start = origin + int(std::floor(source * scale));
        size = std::max(1, origin + int(std::ceil((source + 1) * scale)) - start);
    } else {
        start = int(origin + source * scale);
        size = int(std::ceil(scale));
    }
}

inline bool draw(Canvas565& canvas, const TintedSpriteRle::Source& s,
                 const Geometry& g, int x, int y, float scale, bool flipX,
                 bool nearest, Mode mode = Mode::Normal,
                 uint16_t color = 0, uint8_t amount = 0) {
    if (!s.data || !s.length || s.width <= 0 || s.height <= 0 ||
        s.width > 255 || s.height > 255 || g.width <= 0 || g.height <= 0 ||
        g.width > 255 || g.height > 255 || g.offsetX < 0 || g.offsetY < 0 ||
        g.offsetX + s.width > g.width || g.offsetY + s.height > g.height ||
        !std::isfinite(scale) || scale <= 0 || g.width * scale > 32767 ||
        g.height * scale > 32767 ||
        (mode != Mode::Solid && s.indexed && (!s.palette || !s.paletteSize || s.paletteSize > 16))) return false;
    if (!canvas.attached()) return mode != Mode::Tinted;

    int16_t xs[256], ys[256];
    if (mode == Mode::Tinted) {
        const int dw = std::max(1, int(std::lround(g.width * scale)));
        const int dh = std::max(1, int(std::lround(g.height * scale)));
        int dest = 0;
        for (int i = 0; i <= g.width; ++i) {
            while (dest < dw && std::min(g.width - 1, int(dest / scale)) < i) ++dest;
            xs[i] = int16_t(dest);
        }
        dest = 0;
        for (int i = 0; i <= g.height; ++i) {
            while (dest < dh && std::min(g.height - 1, int(dest / scale)) < i) ++dest;
            ys[i] = int16_t(dest);
        }
    }
    uint16_t colors[16]{};
    if (s.indexed && mode != Mode::Solid) for (int i = 0; i < s.paletteSize; ++i) {
        colors[i] = FlashStorage::readWord(s.palette + s.paletteOffset + i);
        if (mode == Mode::Tinted) colors[i] = TintedSpriteRle::blend(colors[i], color, amount);
    }
    uint16_t rowColors[255]{};
    uint8_t opaque[255]{};
    uint32_t cursor = 0, pixel = 0;
    const uint32_t total = uint32_t(s.width * s.height);
    auto paintRow = [&](int imageRow) {
        const int sr = g.offsetY + imageRow;
        for (int logicalCol = 0; logicalCol < s.width; ++logicalCol) {
            const int imageCol = g.imageFlipX ? s.width - 1 - logicalCol : logicalCol;
            if (!opaque[imageCol]) continue;
            int sc = g.offsetX + logicalCol;
            if (flipX) sc = g.width - 1 - sc;
            int left, top, w, h;
            if (mode == Mode::Tinted) {
                left = x + xs[sc]; top = y + ys[sr];
                w = xs[sc + 1] - xs[sc]; h = ys[sr + 1] - ys[sr];
            } else if (scale == 1) {
                left = x + sc; top = y + sr; w = h = 1;
            } else if (scale < 1) {
                int dc, dr;
                if (!downsample(sc, scale, std::max(1, int(g.width * scale)), dc) ||
                    !downsample(sr, scale, std::max(1, int(g.height * scale)), dr)) continue;
                left = x + dc; top = y + dr; w = h = 1;
            } else {
                interval(x, sc, g.width, scale, s.indexed, nearest, left, w);
                interval(y, sr, g.height, scale, s.indexed, nearest, top, h);
            }
            if (w > 0 && h > 0) canvas.fillAssetRect(left, top, w, h,
                mode == Mode::Solid ? color : rowColors[imageCol]);
        }
    };
    while (cursor < s.length && pixel < total) {
        const uint16_t token = FlashStorage::readWord(s.data + s.offset + cursor++);
        const uint32_t run = token & 0x7fffU;
        if (!run) continue;
        if (run > total - pixel) return false;
        const bool skip = token & 0x8000U;
        const uint32_t words = skip ? 0 : s.indexed ? (run + 3) / 4 : run;
        if (words > s.length - cursor) return false;
        for (uint32_t i = 0; i < run; ++i, ++pixel) {
            const int c = int(pixel % s.width);
            opaque[c] = !skip;
            if (!skip && mode != Mode::Solid) {
                const uint16_t word = FlashStorage::readWord(s.data + s.offset + cursor + (s.indexed ? i / 4 : i));
                const unsigned index = (word >> ((i % 4) * 4)) & 15U;
                if (s.indexed && index >= s.paletteSize) {
                    if (mode == Mode::Tinted) return false;
                    opaque[c] = false;
                } else rowColors[c] = s.indexed ? colors[index] :
                    mode == Mode::Tinted ? TintedSpriteRle::blend(word, color, amount) : word;
            }
            if (c + 1 == s.width) paintRow(int(pixel / s.width));
        }
        cursor += words;
    }
    return pixel == total;
}
} // namespace Ui::TransformedSpriteRle
