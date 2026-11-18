// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/AppSceneFlow.h"
#include "ui/common/ExploreUiModel.h"

namespace Ui::RoundExplore {
constexpr MenuGrid::Rect ROUTE_NAME{116, 19, 128, 32};
constexpr MenuGrid::Rect ROUTE_TITLE{116, 19, 84, 32};
constexpr MenuGrid::Rect routeStatusSlot(unsigned member) {
    // Continue past the fixed outer arc endpoint, leaving a 3-4px icon gap.
    return {member ? 302 : 37, 70, 20, 20};
}
constexpr MenuGrid::Rect ROUTE_MENU{202, 13, 44, 44};
constexpr MenuGrid::Rect ROUTE_MENU_BUTTON{208, 19, 32, 32};
constexpr int ROUTE_MENU_ROW_HEIGHT = 40;
constexpr MenuGrid::Rect ROUTE_PANEL{224, 90, 92,
    AppSceneFlow::exploreMenuItemCount(true) * ROUTE_MENU_ROW_HEIGHT + 8};
constexpr MenuGrid::Rect routeMenuItem(int index) {
    return {ROUTE_PANEL.x + 4, ROUTE_PANEL.y + 4 + index * ROUTE_MENU_ROW_HEIGHT,
        ROUTE_PANEL.width - 8, ROUTE_MENU_ROW_HEIGHT};
}
// Route geometry is independent of battle and progression layouts.
constexpr bool nativeRoute(ExplorePhase phase) {
    return phase == ExplorePhase::WALKING || phase == ExplorePhase::EXITING ||
        phase == ExplorePhase::PICKUP || phase == ExplorePhase::SPECIAL_PROMPT ||
        phase == ExplorePhase::ENDING;
}
constexpr ExploreRouteLayout routeLayout(ExplorePhase phase) {
    return {nativeRoute(phase) ? 360 : 240, nativeRoute(phase) ? 360 : 240,
        true, true, 1.0f / 6.0f, nativeRoute(phase), 1.0f};
}
bool routePoint(int x, int y);
bool routeHudHit(int x, int y, uint8_t members);
void drawRouteHud(const ExploreRouteFrame& frame);
void drawRouteMenu(bool open, uint8_t cursor, bool buttonVisible = true);
}  // namespace Ui::RoundExplore
