// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/common/ExploreBattleUiModel.h"
#include "ui/common/ExploreUiModel.h"

namespace Ui::RoundExplore {
constexpr bool nativeBattle(ExplorePhase phase) {
    return phase == ExplorePhase::ENCOUNTER || phase == ExplorePhase::FRIENDSHIP;
}
constexpr ExploreBattleLayout battleLayout() {
    ExploreBattleLayout layout(360, 360, 0, true, 0, true);
    layout.floatingControls = true;
    layout.wildX = 246; layout.wildGroundY = 182;
    layout.wildMaxW = 112; layout.wildMaxH = 104;
    layout.playerX = 98; layout.playerGroundY = 246;
    layout.playerMaxW = 126; layout.playerMaxH = 88;
    layout.wildHud = {112, 31, 248, 232, 49, 112, 55, 104, {104, 26, 152, 46}};
    layout.playerHud = {180, 213, 310, 294, 231, 180, 237, 108, {172, 209, 146, 43}};
    layout.playerNameY = layout.playerHud.nameY;
    layout.expLabelX = 180; layout.expBarX = 212; layout.expBarY = 237;
    layout.expBarW = 98; layout.expLabelY = 236;
    layout.commands[0] = {60, 264, 56, 44};
    layout.commands[1] = {120, 282, 56, 44};
    layout.commands[2] = {180, 282, 56, 44};
    layout.commands[3] = {240, 264, 56, 44};
    layout.message = {66, 270, 228, 46};
    layout.friendshipRowHeight = 44;
    layout.foodStartX = 98; layout.foodStartY = 225;
    layout.foodEndX = 246; layout.foodEndY = 145;
    return layout;
}
}  // namespace Ui::RoundExplore
