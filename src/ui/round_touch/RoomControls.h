// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "presentation/MenuGrid.h"

namespace Ui { namespace RoundRoom {
constexpr MenuGrid::Rect HOME_MENU_RECT{161, 295, 38, 38};
void drawMenuButton();
inline bool lastSaveOk = true;
} }  // namespace Ui::RoundRoom
