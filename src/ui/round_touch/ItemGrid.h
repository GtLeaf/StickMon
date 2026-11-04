// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/compact_touch/ItemTouchUi.h"

namespace Ui::RoundItems {
constexpr int ICON_SIZE = 48;
inline ItemTouchUi::Grid grid(MenuGrid::Rect viewport, int first, int second = -1) {
    ItemTouchUi::Grid result(viewport, first, second);
    result.layout = {viewport, 3, 68, 68, 8, 8, 10, 28};
    // Side cards must scroll clear of the round panel's narrowing bottom edge.
    result.scrollBottomInset = 44;
    return result;
}
}  // namespace Ui::RoundItems
