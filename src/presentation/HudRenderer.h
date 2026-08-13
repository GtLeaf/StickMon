#pragma once

#include <cstdint>

class Canvas565;

namespace HudRenderer {

void drawHungerIcon(Canvas565& canvas, int x, int y, uint8_t hunger);

}  // namespace HudRenderer
