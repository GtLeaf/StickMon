#pragma once

#include "game/GameState.h"
#include "game/FriendshipService.h"

namespace Features { namespace Contacts {

enum class Action : uint8_t { STATUS, INVITE, DELETE, BACK };
using InviteResult = Game::FriendshipService::InviteResult;
enum class InviteGate : uint8_t { READY, TEAM_FULL, VISITING, LOCKED, IN_TEAM, INVALID };
struct Capabilities {
    bool valid = false;
    bool inTeam = false;
    bool visiting = false;
    bool locked = false;
    bool canDelete = false;
};

Capabilities capabilities(const Game::GameState& state, uint8_t slot, bool visiting, bool locked);
uint8_t actionCount(const Capabilities& capabilities);
Action actionAt(const Capabilities& capabilities, uint8_t index);
InviteGate inviteGate(const Game::GameState& state, uint8_t slot, bool visiting, bool locked);

} }  // namespace Features::Contacts
