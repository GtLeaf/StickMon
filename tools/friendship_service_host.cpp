#include <cassert>

#include "game/FriendshipPity.h"
#include "game/FriendshipService.h"
#include "game/BondSystem.h"
#include "game/GameState.h"
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"

namespace {
unsigned thousandRolls = 0;
uint32_t failEveryRoll(uint32_t, uint32_t maximum) {
    if (maximum == 1000) ++thousandRolls;
    return maximum - 1;
}
uint32_t rescueWithPity(uint32_t, uint32_t maximum) {
    if (maximum == 1000 && ++thousandRolls == 2) return 0;
    return maximum - 1;
}
} // namespace

int main() {
    using ExploreSpecial::Kind;
    using namespace Game::FriendshipService;
    Game::GameState state;
    Game::MonsterRuntime wild = Game::MonsterFactory::create(1, 5);
    const Species* species = findSpecies(wild.speciesId);
    assert(species != nullptr);

    auto noOffer = Game::FriendshipService::evaluateOffer(
        state, *findSpecies(380), wild, true, Kind::FIRST_LATIAS);
    assert(!noOffer.eligible && !noOffer.offered);

    auto forced = Game::FriendshipService::evaluateOffer(
        state, *species, wild, false, Kind::NONE, 0, true);
    assert(forced.eligible && forced.offered);

    GameRandom::setRangeProvider(failEveryRoll);
    assert(!evaluateOffer(state, *species, wild, false, Kind::NONE).pityChanged);
    const auto snorlax = Game::MonsterFactory::create(143, 27);
    assert(!evaluateOffer(state, *findSpecies(143), snorlax, true, Kind::FIRST_SNORLAX).pityChanged);
    auto mew = Game::MonsterFactory::create(151, 70);
    const Species& mewSpecies = *findSpecies(mew.speciesId);
    assert(!evaluateOffer(state, mewSpecies, mew, false, Kind::NONE).pityChanged);
    auto failed = evaluateOffer(state, mewSpecies, mew, true, Kind::MEW_EVENT);
    assert(failed.eligible && !failed.offered && failed.pityChanged);
    assert(FriendshipPity::failuresFor(state.friendshipPity, 151) == 1);
    const auto latios = Game::MonsterFactory::create(381, 70);
    assert(evaluateOffer(state, *findSpecies(381), latios, true, Kind::ROAMING_LATIOS).pityChanged);
    assert(state.friendshipPity.entries[0].speciesId == 381);
    assert(evaluateOffer(state, mewSpecies, mew, true, Kind::MEW_EVENT).pityChanged);
    assert(state.friendshipPity.entries[0].speciesId == 151);
    assert(FriendshipPity::failuresFor(state.friendshipPity, 151) == 2);
    GameRandom::setRangeProvider(rescueWithPity);
    thousandRolls = 0;
    assert(!evaluateOffer(state, mewSpecies, mew, true, Kind::NONE).offered);
    assert(thousandRolls == 1);
    thousandRolls = 0;
    const auto rescued = evaluateOffer(state, mewSpecies, mew, true, Kind::MEW_EVENT);
    assert(rescued.offered && !rescued.pityChanged && thousandRolls == 2);
    assert(FriendshipPity::failuresFor(state.friendshipPity, 151) == 2);
    GameRandom::setRangeProvider(nullptr);

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
    assert(recordContact(state, mew, 4, 2000, nullptr, Kind::MEW_EVENT));
    assert(FriendshipPity::failuresFor(state.friendshipPity, 151) == 0);
    assert(FriendshipPity::failuresFor(state.friendshipPity, 381) == 1);

    Game::GameState full;
    full.storageCount = Game::STORAGE_CAP;
    auto fullOffer = Game::FriendshipService::evaluateOffer(
        full, mewSpecies, mew, true, Kind::MEW_EVENT, 0, true);
    assert(!fullOffer.eligible && !fullOffer.offered);
    assert(FriendshipPity::failuresFor(full.friendshipPity, 151) == 0);
    assert(!Game::FriendshipService::recordContact(
        full, wild, 0, 1, nullptr));
    return 0;
}
