#pragma once

#include <algorithm>
#include <cstdint>

#include "game/BondSystem.h"
#include "game/ExploreRunRules.h"
#include "game/TeamRoster.h"

namespace Game::ExplorePartyService {

inline bool healthy(const MonsterRuntime& monster) {
    return !monster.fainted && monster.hpCur > 0;
}

inline uint8_t leaderSlot(const GameState& state) {
    const bool firstHealthy = state.teamCount > 0 && healthy(state.team[0]);
    const bool secondHealthy = state.teamCount > 1 && healthy(state.team[1]);
    return ExploreRunRules::leaderSlotForHealth(firstHealthy, secondHealthy);
}

inline bool hasHealthyFollower(const GameState& state) {
    const bool firstHealthy = state.teamCount > 0 && healthy(state.team[0]);
    const bool secondHealthy = state.teamCount > 1 && healthy(state.team[1]);
    return ExploreRunRules::showsHealthyFollower(firstHealthy, secondHealthy);
}

inline bool recoverAfterCompletedSteps(GameState& state, uint16_t completedSteps) {
    if (!ExploreRunRules::isRecoveryStep(completedSteps)) return false;

    bool changed = false;
    for (uint8_t slot = 0; slot < TeamRoster::memberCount(state); ++slot) {
        MonsterRuntime& monster = state.team[slot];
        if (!healthy(monster) || monster.hpMax == 0 ||
            monster.hpCur >= monster.hpMax) {
            continue;
        }
        const uint32_t recoveredHp = static_cast<uint32_t>(monster.hpCur) +
            ExploreRunRules::recoveryAmount(monster.hpMax);
        monster.hpCur = static_cast<uint16_t>(
            std::min<uint32_t>(monster.hpMax, recoveredHp));
        changed = true;
    }
    return changed;
}

// The application decides whether an active visiting companion is eligible.
inline uint8_t grantAdventureBond(GameState& state, uint16_t completedSteps,
                                  bool includeVisitors = false) {
    const uint8_t gain = Bond::adventureGain(completedSteps);
    if (gain == 0) return 0;

    bool changed = false;
    for (uint8_t slot = 0; slot < TeamRoster::memberCount(state); ++slot) {
        MonsterRuntime& monster = state.team[slot];
        if ((monster.origin == Origin::VISITOR && !includeVisitors) ||
            !healthy(monster)) {
            continue;
        }
        const Bond::Value next = Bond::increase(monster.bond, gain);
        if (next == monster.bond) continue;
        monster.bond = next;
        changed = true;
    }
    return changed ? gain : 0;
}

inline bool clearBattleStatuses(GameState& state) {
    bool changed = false;
    for (uint8_t slot = 0; slot < TeamRoster::memberCount(state); ++slot) {
        MonsterRuntime& monster = state.team[slot];
        if (monster.majorStatus == MajorStatus::NONE &&
            monster.majorStatusTurns == 0) {
            continue;
        }
        monster.majorStatus = MajorStatus::NONE;
        monster.majorStatusTurns = 0;
        changed = true;
    }
    return changed;
}

}  // namespace Game::ExplorePartyService
