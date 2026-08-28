#include <cassert>

#include "game/ItemInventory.h"
#include "game/MonsterFactory.h"
#include "game/MoveManagementService.h"
#include "game/Species.h"

int main() {
    Game::GameState state;
    state.teamCount = 1;
    state.team[0] = Game::MonsterFactory::create(1, 30);
    const Species* species = findSpecies(state.team[0].speciesId);
    assert(species != nullptr);
    assert(Game::MoveManagementService::learnedMoveCount(
               *species, state.team[0]) >= 1);

    state.bag.heartScale = 1;
    Game::MoveId recallable[
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t count = Game::MoveManagementService::collectRecallable(
        *species, state.team[0], recallable,
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
    assert(count > 0);
    assert(Game::MoveManagementService::recallMove(
        state, 0, recallable[0], 1));
    assert(state.bag.heartScale == 0);
    assert(state.team[0].move2Id == recallable[0]);
    assert(Game::MoveManagementService::forgetSpecialMove(state, 0, 1));
    assert(state.team[0].move2Id == 0);
    assert(!Game::MoveManagementService::forgetSpecialMove(state, 0, 0));
    return 0;
}
