#pragma once

#include <cstdint>

#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"

struct Species;

namespace Game {
namespace FriendshipService {

struct OfferResult {
    bool eligible = false;
    bool offered = false;
    bool pityChanged = false;
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
OfferResult evaluateOffer(GameState& state,
                          const Species& species,
                          const MonsterRuntime& monster,
                          bool boss,
                          ExploreSpecial::Kind special,
                          uint8_t foodBond = 0,
                          bool forceOffer = false);

bool recordContact(GameState& state, const MonsterRuntime& monster,
                   uint8_t metArea, uint32_t metAt, uint8_t* contactSlot,
                   ExploreSpecial::Kind special = ExploreSpecial::Kind::NONE);
InviteResult inviteContact(GameState& state, uint8_t slot,
                           uint32_t gameMinutesTotal);

}  // namespace FriendshipService
}  // namespace Game
