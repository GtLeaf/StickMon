#include <cassert>

#include "game/HomeHud.h"

int main() {
    Game::GameState state;
    uint8_t slots[Game::TEAM_CAP] = {};
    assert(Game::HomeHud::visibleTeamSlots(state, slots) == 1);
    assert(slots[0] == 0);

    state.teamCount = 2;
    state.team[1].speciesId = 4;
    assert(Game::HomeHud::visibleTeamSlots(state, slots) == 2);
    assert(slots[0] == 0);
    assert(slots[1] == 1);

    state.team[1].origin = Game::Origin::VISITOR;
    assert(Game::HomeHud::visibleTeamSlots(state, slots) == 1);
    assert(slots[0] == 0);

    state.team[0].origin = Game::Origin::VISITOR;
    state.team[1].origin = Game::Origin::STARTER;
    assert(Game::HomeHud::visibleTeamSlots(state, slots) == 2);
    assert(slots[0] == 0);
    assert(slots[1] == 1);

    state.team[0].satiety = 37;
    assert(Game::HomeHud::hungerPercent(state.team[0]) == 37);
    state.team[0].hpCur = 10;
    state.team[0].hpMax = 20;
    assert(Game::HomeHud::hpPercent(state.team[0]) == 50);
    state.team[0].fainted = true;
    assert(Game::HomeHud::hpPercent(state.team[0]) == 0);
    return 0;
}
