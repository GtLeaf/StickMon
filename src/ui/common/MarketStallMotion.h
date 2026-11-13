#pragma once

#include <cstdint>
#include <cstring>
#include "presentation/PixelRenderer.h"

// Motion and close control shared by all three market stall panels
// (368x448 touch layout). Panels ease in and out symmetrically so the first
// and last frames move only a few pixels even at a low frame rate.
namespace Ui::MarketStallMotion {
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

// Smoothstep peaks at 1.5x the average speed, keeping middle-frame travel
// gentle at the existing market frame rate. Panel, camera and HUD share it.
inline int tween(int from, int to, uint32_t elapsedMs, uint32_t durationMs) {
    if (!durationMs || elapsedMs >= durationMs) return to;
    const int64_t t = int64_t(elapsedMs) * 1000 / durationMs;
    const int64_t ease = (3000 * t * t - 2 * t * t * t) / 1000000;
    return from + int((to - from) * ease / 1000);
}

// Coin badge travel above the screen edge while it is hidden.
constexpr int HUD_HIDDEN = -64;
inline int hudOffset(bool greeting, bool opening, bool closing, uint32_t elapsedMs, uint32_t durationMs) {
    if (greeting) return HUD_HIDDEN;
    if (opening) return tween(HUD_HIDDEN, 0, elapsedMs, durationMs);
    if (closing) return tween(0, HUD_HIDDEN, elapsedMs, durationMs);
    return 0;
}

// Shared compact stall badge: sample native glyphs at 28px. labelWidth is
// measured at that size; other currency HUDs keep their existing metrics.
inline void drawBadge(int x, int y, const char* label, int labelWidth, uint16_t paper, uint16_t ink) {
    auto& c = PixelRenderer::canvas();
    constexpr int height = 28;
    c.fillRoundRect(x - 8, y - 4, labelWidth + 16, 40, 6, paper);
    uint16_t pixels[32 * 32];
    Canvas565 glyph;
    glyph.attach({pixels, 32, 32, false});
    glyph.setNativeText(true);
    for (const auto* p = reinterpret_cast<const uint8_t*>(label); *p;) {
        const int n = *p < 0x80 ? 1 : *p < 0xe0 ? 2 : *p < 0xf0 ? 3 : 4;
        char value[5]{}; std::memcpy(value, p, n);
        glyph.fillRect(0, 0, 32, 32, paper);
        PixelRenderer::text(glyph, 0, 0, value, ink);
        const int width = n == 1 ? height / 2 : height;
        for (int row = 0; row < height; ++row)
            for (int col = 0; col < width; ++col)
                c.drawPixel(x + col, y + row + 2, glyph.readPixel(col * 32 / height, row * 32 / height));
        x += width; p += n;
    }
}

// Top-right cross on the panel header (title row y168..216). The touch
// target is larger than the drawn glyph.
constexpr int CLOSE_CX = 336, CLOSE_CY = 194, CLOSE_ARM = 10;
inline Rect closeHit() { return {304, 168, 64, 52}; }
inline void drawClose(int offsetY, uint16_t color) {
    auto& c = PixelRenderer::canvas();
    const int cx = CLOSE_CX, cy = CLOSE_CY + offsetY;
    for (int d = -1; d <= 1; ++d) {
        c.drawLine(cx - CLOSE_ARM + d, cy - CLOSE_ARM, cx + CLOSE_ARM + d, cy + CLOSE_ARM, color);
        c.drawLine(cx - CLOSE_ARM + d, cy + CLOSE_ARM, cx + CLOSE_ARM + d, cy - CLOSE_ARM, color);
    }
}
} // namespace Ui::MarketStallMotion
