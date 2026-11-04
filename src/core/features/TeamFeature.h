#pragma once

#include <cstdint>
#include "game/GameState.h"

namespace Features { namespace Team {

enum class Action : uint8_t { STATUS, FIRST, MOVES, LEAVE, BACK };

struct Capabilities {
    bool member;
    bool visitor;
    bool first;
    bool leave;
};

uint8_t memberCount(const Game::GameState& state);
uint8_t formalCount(const Game::GameState& state);
Capabilities capabilities(const Game::GameState& state, uint8_t slot,
                          bool exploring);

} }  // namespace Features::Team
