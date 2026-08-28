#include <cassert>

#include "game/FriendshipPity.h"
#include "game/FriendshipService.h"
#include "game/BondSystem.h"
#include "game/GameState.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"

int main() {
    Game::GameState state;
    Game::MonsterRuntime wild = Game::MonsterFactory::create(1, 5);
    const Species* species = findSpecies(wild.speciesId);
    assert(species != nullptr);

    auto noOffer = Game::FriendshipService::evaluateOffer(
        state, *species, wild, false, false);
    assert(!noOffer.eligible && !noOffer.offered);

    auto forced = Game::FriendshipService::evaluateOffer(
        state, *species, wild, false, true, 0, true);
    assert(forced.eligible && forced.offered);

    int8_t pityIndex = FriendshipPity::indexFor(wild.speciesId);
    assert(pityIndex >= 0);
    Game::FriendshipService::recordFailure(state, wild.speciesId);
    assert(state.friendshipPityFailCounts[pityIndex] == 1);
    Game::FriendshipService::recordSuccess(state, wild.speciesId);
    assert(state.friendshipPityFailCounts[pityIndex] == 0);

    uint8_t contactSlot = 0xFF;
    assert(Game::FriendshipService::recordContact(
        state, wild, 2, 1234, &contactSlot));
    assert(contactSlot == 0 && state.storageCount == 1);
    const Game::MonsterRuntime& contact = state.storage[contactSlot];
    assert(contact.origin == Game::Origin::BEFRIENDED);
    assert(contact.hpCur == contact.hpMax && contact.bond == Game::Bond::NEW_CONTACT_VALUE);
    assert(contact.metArea == 2 && contact.metAt == 1234);

    state.teamCount = 0;
    assert(Game::FriendshipService::inviteContact(state, contactSlot, 8 * 60) ==
           Game::FriendshipService::InviteResult::JOINED);
    assert(state.teamCount == 1 && state.team[0].speciesId == wild.speciesId);

    Game::GameState full;
    full.storageCount = Game::STORAGE_CAP;
    auto fullOffer = Game::FriendshipService::evaluateOffer(
        full, *species, wild, false, true, 0, true);
    assert(!fullOffer.eligible && !fullOffer.offered);
    assert(!Game::FriendshipService::recordContact(
        full, wild, 0, 1, nullptr));
    return 0;
}
