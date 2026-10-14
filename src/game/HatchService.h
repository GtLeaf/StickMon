#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace Game::HatchService {

void initializeNewGame(GameState& state);
uint8_t starterStyle(uint16_t pokeCount, uint16_t wipeCount);
bool finish(GameState& state, uint8_t style);
uint8_t progressPercent(float elapsedSeconds, uint16_t totalSeconds);
uint16_t remainingSeconds(float elapsedSeconds, uint16_t totalSeconds);
int8_t shakeOffset(uint8_t progress, uint32_t nowMs);

}  // namespace Game::HatchService
