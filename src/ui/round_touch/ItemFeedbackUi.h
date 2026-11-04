// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "game/GameState.h"

namespace Ui::ItemFeedback {
void renderLevelUp(const Game::MonsterRuntime& monster, const char* name, uint8_t level);
void renderEvolutionNavigation(const char* message, bool complete);
int evolutionTarget(int16_t x, int16_t y, bool cancellable);
}
