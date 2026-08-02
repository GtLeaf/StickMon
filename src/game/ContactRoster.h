#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace ContactRoster {

inline bool sameMonster(const Game::MonsterRuntime& lhs,
                        const Game::MonsterRuntime& rhs) {
    return lhs.ivPacked == rhs.ivPacked &&
           lhs.nature == rhs.nature &&
           lhs.metAt == rhs.metAt &&
           lhs.metArea == rhs.metArea &&
           lhs.origin == rhs.origin;
}

inline int8_t teamSlotForContact(const Game::GameState& state,
                                 uint8_t contactSlot) {
    if (contactSlot >= state.storageCount ||
        contactSlot >= Game::STORAGE_CAP) {
        return -1;
    }
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (sameMonster(state.storage[contactSlot], state.team[slot])) {
            return static_cast<int8_t>(slot);
        }
    }
    return -1;
}

inline void syncTeamContacts(Game::GameState& state) {
    for (uint8_t contact = 0;
         contact < state.storageCount && contact < Game::STORAGE_CAP;
         ++contact) {
        int8_t teamSlot = teamSlotForContact(state, contact);
        if (teamSlot >= 0) {
            state.storage[contact] = state.team[teamSlot];
        }
    }
}

} // namespace ContactRoster
