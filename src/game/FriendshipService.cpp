#include "game/FriendshipService.h"

#include "game/FriendshipPity.h"
#include "game/FriendshipSystem.h"
#include "game/GameRandom.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/Species.h"

namespace Game {
namespace FriendshipService {

OfferResult evaluateOffer(const GameState& state,
                          const Species& species,
                          const MonsterRuntime& monster,
                          bool boss,
                          bool allowsFriendship,
                          uint8_t foodBond,
                          bool forceOffer) {
    OfferResult result;
    result.eligible = allowsFriendship &&
                      state.storageCount < STORAGE_CAP;
    if (!result.eligible) return result;
    if (forceOffer) {
        result.offered = true;
        return result;
    }

    int8_t pityIndex = FriendshipPity::indexFor(species.id);
    uint8_t failCount = pityIndex >= 0
        ? state.friendshipPityFailCounts[pityIndex] : 0;
    FriendshipPity::Tier tier = pityIndex >= 0
        ? FriendshipPity::tierAt(static_cast<uint8_t>(pityIndex))
        : FriendshipPity::Tier::NONE;
    uint16_t baseChance = FriendshipSystem::offerChancePermille(
        species, monster, boss, foodBond);
    uint16_t finalChance = FriendshipPity::chanceWithBonus(
        baseChance, tier, failCount);

    uint16_t shakeRolls[FriendshipSystem::SHAKE_CHECK_COUNT];
    for (uint8_t index = 0;
         index < FriendshipSystem::SHAKE_CHECK_COUNT; ++index) {
        shakeRolls[index] = static_cast<uint16_t>(
            GameRandom::random(0, 65536));
    }
    bool basePassed = FriendshipSystem::passesOfferChecks(
        species, monster, boss,
        static_cast<uint16_t>(GameRandom::random(0, 1000)),
        shakeRolls, foodBond);
    uint16_t conditionalBonus = FriendshipPity::conditionalBonusPermille(
        baseChance, finalChance);
    bool pityPassed = !basePassed && conditionalBonus > 0 &&
        static_cast<uint16_t>(GameRandom::random(0, 1000)) < conditionalBonus;
    result.offered = basePassed || pityPassed;
    return result;
}

void recordFailure(GameState& state, uint16_t speciesId) {
    if (state.storageCount >= STORAGE_CAP) return;
    int8_t index = FriendshipPity::indexFor(speciesId);
    if (index < 0) return;
    uint8_t& failCount = state.friendshipPityFailCounts[index];
    if (failCount < FriendshipPity::MAX_FAIL_COUNT) ++failCount;
}

void recordSuccess(GameState& state, uint16_t speciesId) {
    int8_t index = FriendshipPity::indexFor(speciesId);
    if (index >= 0) state.friendshipPityFailCounts[index] = 0;
}

bool recordContact(GameState& state, const MonsterRuntime& monster,
                   uint8_t metArea, uint32_t metAt, uint8_t* contactSlot) {
    const Species* species = findSpecies(monster.speciesId);
    if (!species || state.storageCount >= STORAGE_CAP) return false;

    MonsterRuntime contact = monster;
    contact.hpMax = maxHpFor(*species, contact);
    contact.hpCur = contact.hpMax;
    resetMovesForLevel(contact, *species);
    contact.origin = Origin::BEFRIENDED;
    contact.bond = Bond::NEW_CONTACT_VALUE;
    contact.metArea = metArea;
    contact.metAt = metAt;
    contact.lastSeenAt = metAt;
    contact.lastExploredAt = metAt;
    contact.lastWindowGazeAt = metAt;

    uint8_t slot = state.storageCount++;
    state.storage[slot] = contact;
    if (contactSlot) *contactSlot = slot;
    return true;
}

InviteResult inviteContact(GameState& state, uint8_t slot,
                           uint32_t gameMinutesTotal) {
    if (slot >= state.storageCount || slot >= STORAGE_CAP) {
        return InviteResult::INVALID;
    }
    if (ContactRoster::teamSlotForContact(state, slot) >= 0) {
        return InviteResult::ALREADY_IN_TEAM;
    }
    if (state.teamCount >= TEAM_CAP) return InviteResult::TEAM_FULL;

    uint32_t day = Bond::invitationDay(gameMinutesTotal);
    MonsterRuntime& contact = state.storage[slot];
    if (Bond::inviteLockedToday(contact.petCountToday, day)) {
        return InviteResult::LOCKED;
    }
    bool firstInvitation = contact.bond == Bond::NEW_CONTACT_VALUE &&
                           contact.lastExploredAt <= contact.metAt;
    uint8_t chance = Bond::inviteChance(contact.bond, firstInvitation);
    if (GameRandom::random(100) >= chance) {
        contact.petCountToday = Bond::inviteLockMarker(day);
        return InviteResult::REFUSED;
    }

    contact.petCountToday = 0;
    state.team[state.teamCount++] = contact;
    return InviteResult::JOINED;
}

}  // namespace FriendshipService
}  // namespace Game
