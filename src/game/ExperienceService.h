#pragma once

#include <cstdint>

#include "game/GameState.h"

struct Species;

namespace Game {
namespace ExperienceService {

struct Result {
    uint32_t awarded = 0;
    uint8_t oldLevel = 0;
    uint8_t newLevel = 0;
    uint16_t oldHpMax = 0;
    uint16_t newHpMax = 0;
    bool leveledUp = false;
};

// Applies capped experience, level recalculation, and level-based HP growth.
// Evolution, move-learning prompts, persistence, and UI remain caller-owned.
Result add(MonsterRuntime& monster, const Species& species, uint32_t amount);

}  // namespace ExperienceService
}  // namespace Game
