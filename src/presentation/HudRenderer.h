#pragma once

#include <cstdint>

class Canvas565;

namespace HudRenderer {

void drawHungerIcon(Canvas565& canvas, int x, int y, uint8_t hunger,
                    uint8_t pixelScale = 1);

}  // namespace HudRenderer
