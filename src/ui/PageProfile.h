// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/UiProfile.h"

namespace Ui::Page {
// Native pixels in the menu surface, independent of the physical room canvas.
constexpr int WIDTH = STICKMON_UI_ROUND_TOUCH ? 240 : device.width;
constexpr int HEIGHT = STICKMON_UI_ROUND_TOUCH ? 240 : device.height;
constexpr int HEADER_INSET = STICKMON_UI_ROUND_TOUCH ? 8 : device.headerInsetX;
constexpr int HEADER_TOP = STICKMON_UI_ROUND_TOUCH ? 4 : device.headerTop;
}  // namespace Ui::Page
