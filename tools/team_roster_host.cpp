#include <cassert>

#include "game/TeamRoster.h"

int main() {
    Game::GameState state;
    assert(Game::TeamRoster::memberCount(state) == 1);
    assert(Game::TeamRoster::moveToFront(state, 0));
    assert(!Game::TeamRoster::moveToFront(state, 1));

    state.teamCount = 2;
    state.team[0].speciesId = 1;
    state.team[1].speciesId = 4;
    state.team[1].hpCur = 9;
    assert(Game::TeamRoster::canMoveToFront(state, 1));
    assert(Game::TeamRoster::moveToFront(state, 1));
    assert(state.team[0].speciesId == 4);
    assert(state.team[0].hpCur == 9);
    assert(state.team[1].speciesId == 1);
    assert(state.activeSlot == 0);

    state.team[1].origin = Game::Origin::VISITOR;
    assert(!Game::TeamRoster::moveToFront(state, 1));
    assert(Game::TeamRoster::moveToFront(state, 1, true));
    state.team[0].origin = Game::Origin::VISITOR;
    assert(Game::TeamRoster::moveToFront(state, 0));
    return 0;
}
