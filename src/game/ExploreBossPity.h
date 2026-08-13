#pragma once

#include <cstdint>
#include <cstring>

#include "game/ExploreBoss.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"

namespace ExploreBossPity {

static constexpr uint8_t MAX_MISSES = 3;
static constexpr uint16_t CHANCE_STEP = 2000;

constexpr uint8_t clampedMisses(uint8_t misses) {
    return misses > MAX_MISSES ? MAX_MISSES : misses;
}

constexpr uint16_t chanceForMisses(uint8_t misses) {
    return static_cast<uint16_t>(
        ExploreBoss::SPAWN_CHANCE + clampedMisses(misses) * CHANCE_STEP);
}

constexpr bool requiresGuaranteedEligibleRun(uint8_t misses) {
    return misses >= MAX_MISSES;
}

inline bool syncSlot(Game::GameState& state, uint32_t currentSlot) {
    if (state.normalBossPitySlotIndex == currentSlot) return false;
    state.normalBossPitySlotIndex = currentSlot;
    memset(state.normalBossMissCount, 0, sizeof(state.normalBossMissCount));
    return true;
}

inline bool syncSlot(Game::GameState& state) {
    return syncSlot(
        state, ExploreSpecial::slotIndexFor(state.gameMinutesTotal));
}

inline void increment(Game::GameState& state, uint8_t area) {
    if (area >= Game::EXPLORE_AREA_COUNT) return;
    uint8_t& misses = state.normalBossMissCount[area];
    if (misses < MAX_MISSES) ++misses;
}

inline void resetArea(Game::GameState& state, uint8_t area) {
    if (area < Game::EXPLORE_AREA_COUNT) {
        state.normalBossMissCount[area] = 0;
    }
}

static_assert(chanceForMisses(0) == 4000 &&
                  chanceForMisses(1) == 6000 &&
                  chanceForMisses(2) == 8000 &&
                  chanceForMisses(3) == ExploreBoss::SPAWN_ROLL_MAX &&
                  chanceForMisses(255) == ExploreBoss::SPAWN_ROLL_MAX,
              "regional boss pity must follow the 40/60/80/100 curve");

}  // namespace ExploreBossPity
