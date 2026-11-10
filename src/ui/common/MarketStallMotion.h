#pragma once

#include <cstdint>
#include "presentation/PixelRenderer.h"

// Motion and close control shared by the Kecleon and Gengar stall panels
// (368x448 touch layout). Panels ease in and out symmetrically so the first
// and last frames move only a few pixels even at a low frame rate.
namespace Ui::MarketStallMotion {
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

// Cubic ease-in-out from `from` to `to` after elapsedMs of durationMs.
inline int tween(int from, int to, uint32_t elapsedMs, uint32_t durationMs) {
    if (!durationMs || elapsedMs >= durationMs) return to;
    const int64_t t = int64_t(elapsedMs) * 1000 / durationMs;
    int64_t ease;
    if (t < 500) {
        ease = 4 * t * t * t / 1000000;
    } else {
        const int64_t u = 2000 - 2 * t;
        ease = 1000 - u * u * u / 2000000;
    }
    return from + int((to - from) * ease / 1000);
}

// Smoothstep from `from` to `to`. Peak speed is 1.5x the average (cubic
// ease-in-out peaks at 3x), used for the longer closing travel so its middle
// frames do not jump.
inline int smooth(int from, int to, uint32_t elapsedMs, uint32_t durationMs) {
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
    if (closing) return smooth(0, HUD_HIDDEN, elapsedMs, durationMs);
    return 0;
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
