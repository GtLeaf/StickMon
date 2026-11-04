#pragma once

#include <cstdint>

#include "AmoledGeometry.h"
#include "ui/models/ScreenModels.h"

class Canvas565;

namespace AmoledV1 {

void renderProgressionScreen(Canvas565& canvas,
                             const ProgressionViewModel& model,
                             uint16_t rowBegin = 0, uint16_t rowEnd = AmoledUi::HEIGHT);
int progressionItemAt(int x, int y, ProgressionViewModel::Mode mode);
int progressionReplaceItemAt(int x, int y, int16_t scrollOffsetY,
                             uint8_t selectedItem, float detailProgress);

}  // namespace AmoledV1
