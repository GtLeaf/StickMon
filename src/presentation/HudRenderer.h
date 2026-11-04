#pragma once

#include <cstdint>

class Canvas565;

namespace HudRenderer {

// Scale 1 is 18x8; the default preserves the AMOLED 36x16 icon.
void drawBattery(Canvas565& canvas, int x, int y, int percent,
                 uint8_t pixelScale = 2, bool charging = false);

void drawHungerIcon(Canvas565& canvas, int x, int y, uint8_t hunger,
                    uint8_t pixelScale = 1);

}  // namespace HudRenderer
