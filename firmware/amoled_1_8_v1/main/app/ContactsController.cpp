#include "ContactsController.h"

#include "game/BondSystem.h"
#include "game/ContactRoster.h"

namespace AmoledV1 {

void ContactsController::reset() {
    state_ = State{};
}

void ContactsController::closeOverlay() {
    state_.contactActionOpen = false;
    state_.contactConfirmOpen = false;
    state_.contactActionPressedItem = -1;
    state_.contactConfirmYes = false;
    state_.contactConfirmDelete = false;
}

void ContactsController::finishDelete() {
    state_.contactActionOpen = false;
    state_.contactConfirmOpen = false;
    state_.contactActionSlot = 0xFF;
}

bool ContactsController::openAction(const Game::GameState& gameState,
                                    uint8_t slot) {
    if (!monster(gameState, slot)) return false;
    state_.contactActionOpen = true;
    state_.contactActionSlot = slot;
    state_.contactActionPressedItem = -1;
    state_.contactConfirmOpen = false;
    state_.contactConfirmYes = false;
    state_.contactConfirmDelete = false;
    return true;
}

const Game::MonsterRuntime* ContactsController::monster(
    const Game::GameState& gameState, uint8_t slot) const {
    return ContactRoster::liveMonsterForContact(gameState, slot);
}

bool ContactsController::inTeam(const Game::GameState& gameState,
                                uint8_t slot) const {
    return ContactRoster::teamSlotForContact(gameState, slot) >= 0;
}

bool ContactsController::inviteLocked(const Game::GameState& gameState,
                                      uint8_t slot) const {
    const Game::MonsterRuntime* contact = monster(gameState, slot);
    return contact && Game::Bond::inviteLockedToday(
        contact->petCountToday,
        Game::Bond::invitationDay(gameState.gameMinutesTotal));
}

bool ContactsController::canDelete(const Game::GameState& gameState,
                                   uint8_t slot, bool visiting) const {
    return ContactRoster::canDelete(gameState, slot, visiting);
}

uint8_t ContactsController::actionCount(const Game::GameState& gameState,
                                        uint8_t slot, bool visiting) const {
    if (!monster(gameState, slot)) return 0;
    return static_cast<uint8_t>(1 + (inTeam(gameState, slot) ? 0 : 1) +
                               (canDelete(gameState, slot, visiting) ? 1 : 0));
}

ContactsController::Result ContactsController::selectAction(
    const Game::GameState& gameState, int action, bool visiting) {
    const uint8_t slot = state_.contactActionSlot;
    if (!state_.contactActionOpen) return {};
    const uint8_t count = actionCount(gameState, slot, visiting);
    if (action < 0 || action >= count) {
        closeOverlay();
        return {Action::REDRAW, InviteGate::NONE, slot};
    }
    if (action == 0) return {Action::OPEN_STATUS, InviteGate::NONE, slot};

    int cursor = 1;
    if (!inTeam(gameState, slot)) {
        if (action == cursor) {
            if (inviteLocked(gameState, slot)) {
                closeOverlay();
                return {Action::INVITE_GATE, InviteGate::LOCKED, slot};
            }
            if (visiting) {
                closeOverlay();
                return {Action::INVITE_GATE, InviteGate::VISITING, slot};
            }
            if (gameState.teamCount >= Game::TEAM_CAP) {
                closeOverlay();
                return {Action::INVITE_GATE, InviteGate::TEAM_FULL, slot};
            }
            state_.contactConfirmOpen = true;
            state_.contactConfirmYes = true;
            state_.contactConfirmDelete = false;
            return {Action::CONFIRM_INVITE, InviteGate::NONE, slot};
        }
        ++cursor;
    }
    if (canDelete(gameState, slot, visiting) && action == cursor) {
        state_.contactConfirmOpen = true;
        state_.contactConfirmYes = true;
        state_.contactConfirmDelete = true;
        return {Action::CONFIRM_DELETE, InviteGate::NONE, slot};
    }
    closeOverlay();
    return {Action::REDRAW, InviteGate::NONE, slot};
}

ContactsController::Result ContactsController::confirm(int choice) {
    const uint8_t slot = state_.contactActionSlot;
    if (!state_.contactConfirmOpen) return {};
    if (choice != 0) {
        closeOverlay();
        return {Action::REDRAW, InviteGate::NONE, slot};
    }
    return {state_.contactConfirmDelete ? Action::CONFIRM_DELETE
                                       : Action::CONFIRM_INVITE,
            InviteGate::NONE, slot};
}

ComputerViewModel ContactsController::storageModel(
    const Game::GameState& gameState, float storageScroll, int pressedItem,
    uint8_t visitingSlot) const {
    ComputerViewModel model;
    model.state = &gameState;
    model.page = ComputerViewModel::Page::STORAGE;
    model.storageScroll = storageScroll;
    model.pressedItem = pressedItem;
    model.contactActionOpen = state_.contactActionOpen;
    model.contactActionSlot = state_.contactActionSlot;
    model.contactVisitingSlot = visitingSlot;
    model.contactActionPressedItem = state_.contactActionPressedItem;
    model.contactConfirmOpen = state_.contactConfirmOpen;
    model.contactConfirmYes = state_.contactConfirmYes;
    model.contactConfirmDelete = state_.contactConfirmDelete;
    model.contactInTeam = inTeam(gameState, state_.contactActionSlot);
    model.contactVisiting = state_.contactActionSlot == visitingSlot;
    model.contactInviteLocked = inviteLocked(gameState, state_.contactActionSlot);
    model.contactCanDelete = canDelete(
        gameState, state_.contactActionSlot, model.contactVisiting);
    return model;
}

}  // namespace AmoledV1
