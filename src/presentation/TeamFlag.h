#pragma once

#include "presentation/Canvas565.h"

namespace TeamFlag {

inline int width(int scale) { return 9 * scale; }

inline void draw(Canvas565& canvas, int x, int y, int scale = 1) {
    // Cropped silhouette and #d81e06 color from origin_asset/ui/team_flag.svg.
    constexpr uint16_t red = 0xD8E0;
    canvas.fillRoundRect(x, y, scale, 14 * scale, scale / 2, red);
    canvas.fillTriangle(x + scale, y + scale,
                        x + width(scale) - 1, y + 5 * scale + scale / 2,
                        x + scale, y + 10 * scale, red);
}

} // namespace TeamFlag
