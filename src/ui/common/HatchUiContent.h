#pragma once

#include "ui/common/HatchUiModel.h"

namespace Ui::HatchUiContent {
struct Point { int x, y; };
Point eggCenter(const HatchLayout& layout);
void draw(const HatchFrame& frame, const HatchLayout& layout, const char* toast);
}  // namespace Ui::HatchUiContent
