#pragma once

#include "ui/PageProfile.h"

#include "core/AppSceneFlow.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"

namespace Ui::ExploreHudUi {
constexpr int MENU_ROW_HEIGHT = 36;
inline MenuGrid::Rect menuBounds() {
    const int height = AppSceneFlow::exploreMenuItemCount() * MENU_ROW_HEIGHT + 8;
    return {Ui::Page::WIDTH - 108, (Ui::Page::HEIGHT - height) / 2 - 8, 80, height};
}
inline MenuGrid::Rect menuItem(int index) {
    const auto panel = menuBounds();
    return {panel.x + 4, panel.y + 4 + index * MENU_ROW_HEIGHT,
        panel.width - 8, MENU_ROW_HEIGHT - 4};
}
}  // namespace Ui::ExploreHudUi
