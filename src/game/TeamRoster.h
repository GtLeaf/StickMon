#pragma once

#include <cstdint>

#include "game/GameState.h"

namespace Game {
namespace TeamRoster {

inline uint8_t memberCount(const GameState& state) {
    return state.teamCount < TEAM_CAP ? state.teamCount : TEAM_CAP;
}

inline bool canMoveToFront(const GameState& state, uint8_t slot,
                           bool allowVisitor = false) {
    if (slot >= memberCount(state)) return false;
    if (slot == 0) return true;
    return allowVisitor || state.team[slot].origin != Origin::VISITOR;
}

inline bool moveToFront(GameState& state, uint8_t slot,
                        bool allowVisitor = false) {
    if (!canMoveToFront(state, slot, allowVisitor)) return false;
    if (slot == 0) return true;
    MonsterRuntime selected = state.team[slot];
    for (int8_t index = static_cast<int8_t>(slot); index > 0; --index) {
        state.team[index] = state.team[index - 1];
    }
    state.team[0] = selected;
    state.activeSlot = 0;
    return true;
}

}  // namespace TeamRoster
}  // namespace Game
