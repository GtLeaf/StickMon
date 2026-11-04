#pragma once

#include "ui/common/ExploreBattleUiModel.h"

namespace Ui::ExploreBattleUiContent {

void drawEncounter(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout);
void drawFoodThrow(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout);
void drawFriendshipPrompt(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout);
void drawBattleHud(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout);
void drawCommandBox(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout);
void drawMonsterSprite(const Species& species, int x, int groundY,
                       int maxWidth, int maxHeight, bool back = false,
                       int spriteOffsetX = 0, MenuGrid::Rect* drawnBounds = nullptr);
uint16_t battleHpForRender(const ExploreBattleFrame& frame, bool wildSide,
                          uint16_t currentHp, uint32_t nowMs);
int battleHitShakeX(const ExploreBattleFrame& frame, bool wildSide, uint32_t nowMs);
int battleSwitchOffsetX(const ExploreBattleFrame& frame, uint32_t nowMs);
uint32_t battleExpForRender(const ExploreBattleFrame& frame, uint32_t nowMs);

}  // namespace Ui::ExploreBattleUiContent
