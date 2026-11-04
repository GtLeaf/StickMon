#pragma once

#include <cstdint>
#include "game/GameState.h"

namespace Features { namespace Room {

enum class Action : uint8_t { FOOD, SHOWER, BACK, NONE };

uint8_t normalizedFoodIndex(uint8_t index);
uint8_t selectedFoodIndex(const Game::GameState& state);
uint8_t foodStock(const Game::GameState& state, uint8_t index);
uint16_t totalFoodStock(const Game::GameState& state);
uint8_t displayedFoodStock(const Game::GameState& state);
bool canShower(const Game::GameState& state);
Action roomAction(uint8_t index);
bool foodBack(uint8_t index);

} }  // namespace Features::Room
