#include "core/features/ContactsFeature.h"

#include "game/ContactRoster.h"

namespace Features { namespace Contacts {

Capabilities capabilities(const Game::GameState& state, uint8_t slot, bool visiting, bool locked) {
    Capabilities result;
    if (slot >= state.storageCount || slot >= Game::STORAGE_CAP) return result;
    result.valid = true;
    result.inTeam = ContactRoster::teamSlotForContact(state, slot) >= 0;
    result.visiting = visiting;
    result.locked = locked;
    result.canDelete = ContactRoster::canDelete(state, slot, visiting);
    return result;
}

uint8_t actionCount(const Capabilities& caps) {
    return caps.valid ? 2 + (caps.inTeam ? 0 : 1) + (caps.canDelete ? 1 : 0) : 0;
}

Action actionAt(const Capabilities& caps, uint8_t index) {
    if (!caps.valid) return Action::BACK;
    uint8_t current = 0;
    if (index == current++) return Action::STATUS;
    if (!caps.inTeam && index == current++) return Action::INVITE;
    if (caps.canDelete && index == current) return Action::DELETE;
    return Action::BACK;
}

InviteGate inviteGate(const Game::GameState& state, uint8_t slot, bool visiting, bool locked) {
    const auto caps = capabilities(state, slot, visiting, locked);
    if (!caps.valid) return InviteGate::INVALID;
    if (state.teamCount >= Game::TEAM_CAP) return InviteGate::TEAM_FULL;
    if (caps.visiting) return InviteGate::VISITING;
    if (caps.locked) return InviteGate::LOCKED;
    if (caps.inTeam) return InviteGate::IN_TEAM;
    return InviteGate::READY;
}

} }  // namespace Features::Contacts
