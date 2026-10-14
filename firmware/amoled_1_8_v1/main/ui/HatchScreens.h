#pragma once

#include <cstdint>

#include "AmoledGeometry.h"

class Canvas565;

namespace AmoledV1 {

struct HatchViewModel;

void renderHatchScreen(Canvas565& canvas, const HatchViewModel& model,
                       uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT,
                       AmoledUi::Rect* eggHitBounds = nullptr);
bool hatchMenuAt(int x, int y);

}  // namespace AmoledV1
