#include "game/ContactRoster.h"

#include <cassert>

namespace {
Game::MonsterRuntime monster(uint32_t iv, uint32_t metAt) {
    Game::MonsterRuntime mon;
    mon.ivPacked = iv;
    mon.nature = 7;
    mon.metAt = metAt;
    mon.metArea = 2;
    mon.origin = Game::Origin::BEFRIENDED;
    return mon;
}
}

int main() {
    Game::GameState state;
    state.storageCount = 2;
    state.storage[0] = monster(1234, 500);
    state.storage[1] = monster(5678, 500);
    state.teamCount = 2;
    state.team[0] = monster(42, 100);
    state.team[1] = state.storage[0];

    assert(ContactRoster::teamSlotForContact(state, 0) == 1);
    assert(ContactRoster::teamSlotForContact(state, 1) == -1);
    assert(ContactRoster::liveMonsterForContact(state, 0) == &state.team[1]);
    assert(ContactRoster::liveMonsterForContact(state, 1) == &state.storage[1]);
    assert(!ContactRoster::canDelete(state, 0));
    assert(ContactRoster::canDelete(state, 1));
    assert(!ContactRoster::canDelete(state, 1, true));

    state.team[1].speciesId = 2;
    state.team[1].level = 18;
    state.team[1].bond = 83;
    ContactRoster::syncTeamContacts(state);
    assert(state.storage[0].speciesId == 2);
    assert(state.storage[0].level == 18);
    assert(state.storage[0].bond == 83);
    assert(state.storage[1].ivPacked == 5678);

    state.teamCount = 1;
    state.storage[0].origin = Game::Origin::HATCHED;
    assert(!ContactRoster::canDelete(state, 0));
    assert(ContactRoster::deleteContact(state, 1));
    assert(state.storageCount == 1);
    assert(state.storage[0].ivPacked == 1234);
    assert(state.storage[1].ivPacked == 0);
    return 0;
}
