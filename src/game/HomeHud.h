#pragma once

#include <algorithm>
#include <cstdint>

#include "game/CareTicker.h"
#include "game/GameState.h"
#include "game/SpeciesBehavior.h"

namespace Game {
namespace HomeHud {

inline uint8_t visibleTeamSlots(const GameState& state,
                                uint8_t slots[TEAM_CAP],
                                bool includeVisitingGuests = false) {
    uint8_t count = 0;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < TEAM_CAP; ++slot) {
        if (slot == 0 || state.team[slot].origin != Origin::VISITOR ||
            includeVisitingGuests) {
            slots[count++] = slot;
        }
    }
    if (count == 0 && state.teamCount > 0) slots[count++] = 0;
    return count;
}

inline uint8_t hungerPercent(const MonsterRuntime& monster) {
    return speciesCareProfileFor(monster.speciesId).needsFood
        ? monster.satiety : 100;
}

inline uint8_t hpPercent(const MonsterRuntime& monster) {
    if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0) return 0;
    uint32_t percent = static_cast<uint32_t>(monster.hpCur) * 100U /
                       monster.hpMax;
    return static_cast<uint8_t>(percent > 100U ? 100U : percent);
}

inline uint8_t faintRestPercent(const MonsterRuntime& monster,
                                uint32_t gameMinutesTotal) {
    if (!monster.fainted || monster.lastSeenAt == 0) return 0;
    const uint32_t nowGameSec = gameSecondsForMinutes(gameMinutesTotal);
    if (nowGameSec <= monster.lastSeenAt) return 0;
    const uint32_t elapsed = std::min<uint32_t>(
        nowGameSec - monster.lastSeenAt, FAINT_REST_SECONDS);
    const uint32_t percent = elapsed * 100U / FAINT_REST_SECONDS;
    return static_cast<uint8_t>(percent < 100U ? percent : 99U);
}

}  // namespace HomeHud
}  // namespace Game
