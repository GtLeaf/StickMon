#include "core/features/TeamFeature.h"
#include "game/TeamRoster.h"

namespace Features { namespace Team {

uint8_t memberCount(const Game::GameState& state) {
    return Game::TeamRoster::memberCount(state);
}

uint8_t formalCount(const Game::GameState& state) {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < memberCount(state); ++slot)
        if (state.team[slot].origin != Game::Origin::VISITOR) ++count;
    return count;
}

Capabilities capabilities(const Game::GameState& state, uint8_t slot,
                          bool exploring) {
    const bool member = slot < memberCount(state);
    const bool visitor = member && state.team[slot].origin == Game::Origin::VISITOR;
    return {member, visitor,
        member && slot > 0 && Game::TeamRoster::canMoveToFront(state, slot),
        member && !visitor && !exploring && formalCount(state) > 1};
}

} }  // namespace Features::Team
