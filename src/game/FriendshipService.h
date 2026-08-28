#pragma once

#include <cstdint>

#include "game/GameState.h"

struct Species;

namespace Game {
namespace FriendshipService {

struct OfferResult {
    bool eligible = false;
    bool offered = false;
};

enum class InviteResult : uint8_t {
    JOINED,
    REFUSED,
    LOCKED,
    ALREADY_IN_TEAM,
    TEAM_FULL,
    INVALID,
};

// Resolves the post-battle friendship offer using the shared odds and pity
// rules. Callers own the confirmation UI and contact/team transitions.
OfferResult evaluateOffer(const GameState& state,
                          const Species& species,
                          const MonsterRuntime& monster,
                          bool boss,
                          bool allowsFriendship,
                          uint8_t foodBond = 0,
                          bool forceOffer = false);

void recordFailure(GameState& state, uint16_t speciesId);
void recordSuccess(GameState& state, uint16_t speciesId);

bool recordContact(GameState& state, const MonsterRuntime& monster,
                   uint8_t metArea, uint32_t metAt, uint8_t* contactSlot);
InviteResult inviteContact(GameState& state, uint8_t slot,
                           uint32_t gameMinutesTotal);

}  // namespace FriendshipService
}  // namespace Game
