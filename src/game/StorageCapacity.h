#pragma once
#include <algorithm>
#include "game/GameState.h"
namespace Game::StorageCapacity {
inline constexpr uint8_t INITIAL = 20, STEP = 20, MAX_UPGRADES = 9;
inline constexpr uint8_t capacity(const GameState& s) {
    return uint8_t(INITIAL + std::min(s.storageUpgrades, MAX_UPGRADES) * STEP);
}
inline constexpr bool canExpand(const GameState& s) { return s.storageUpgrades < MAX_UPGRADES; }
inline constexpr uint8_t nextCapacity(const GameState& s) { return canExpand(s) ? uint8_t(capacity(s)+STEP) : STORAGE_CAP; }
inline constexpr uint16_t price(const GameState& s) { return canExpand(s) ? uint16_t(1000 + 1500*s.storageUpgrades) : 0; }
static_assert(INITIAL + MAX_UPGRADES * STEP == STORAGE_CAP);
}
