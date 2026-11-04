// SPDX-License-Identifier: Apache-2.0
#pragma once

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
constexpr MenuGrid::Rect ROUTE_PANEL{236, 88, 92, 184};
constexpr MenuGrid::Rect routeMenuItem(int index) {
    return {240, 92 + index * 44, 84, 44};
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
}  // namespace Ui::RoundExplore
