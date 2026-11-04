#include <cassert>
#include <cstring>
#include <iostream>

#include "app/ContactsController.h"
#include "game/BondSystem.h"

namespace {
using AmoledV1::ComputerViewModel;
using AmoledV1::ContactsController;
using Action = ContactsController::Action;
using Gate = ContactsController::InviteGate;

Game::MonsterRuntime monster(uint16_t identity, Game::Origin origin) {
    Game::MonsterRuntime result;
    result.speciesId = identity;
    result.ivPacked = identity;
    result.metAt = 1000 + identity;
    result.metArea = static_cast<uint8_t>(identity);
    result.origin = origin;
    return result;
}

ContactsController::Result actionTap(
    ContactsController& controller, const Game::GameState& game,
    int action, bool insideMenu, bool visiting) {
    if (action < 0) {
        if (!insideMenu) {
            const uint8_t slot = controller.state().contactActionSlot;
            controller.closeOverlay();
            return {Action::REDRAW, Gate::NONE, slot};
        }
        return {};
    }
    controller.pressAction(action);
    return controller.selectAction(game, action, visiting);
}

void checkActionsAndReadOnlyState() {
    Game::GameState game;
    game.teamCount = 0;
    game.storageCount = 3;
    game.storage[0] = monster(10, Game::Origin::BEFRIENDED);
    game.storage[1] = monster(11, Game::Origin::BEFRIENDED);
    game.storage[2] = monster(12, Game::Origin::HATCHED);
    game.teamCount = 1;
    game.team[0] = game.storage[0];
    game.team[0].hpCur = 7;
    const auto before = game;

    ContactsController controller;
    assert(controller.state().contactActionSlot == 0xFF);
    assert(!controller.openAction(game, 3));
    assert(controller.selectAction(game, 0, false).action == Action::NONE);
    assert(controller.monster(game, 0) == &game.team[0]);
    assert(controller.monster(game, 1) == &game.storage[1]);
    assert(controller.monster(game, 3) == nullptr);
    assert(controller.inTeam(game, 0));
    assert(!controller.inTeam(game, 1));
    assert(controller.actionCount(game, 0, false) == 1);
    assert(controller.actionCount(game, 1, false) == 3);
    assert(controller.actionCount(game, 1, true) == 2);
    assert(controller.actionCount(game, 2, false) == 2);
    assert(controller.actionCount(game, 3, false) == 0);

    assert(controller.openAction(game, 1));
    assert(actionTap(controller, game, 0, true, false).action == Action::OPEN_STATUS);
    assert(controller.state().contactActionOpen);
    controller.closeOverlay();
    assert(!controller.state().contactActionOpen);
    assert(controller.state().contactActionSlot == 1);

    assert(controller.openAction(game, 2));
    auto result = actionTap(controller, game, 1, true, false);
    assert(result.action == Action::CONFIRM_INVITE && result.slot == 2);
    assert(controller.state().contactActionOpen);
    assert(controller.state().contactConfirmOpen);
    assert(controller.state().contactConfirmYes);
    assert(!controller.state().contactConfirmDelete);
    assert(controller.state().contactActionPressedItem == 1);
    controller.cancelTouch();
    assert(controller.state().contactActionPressedItem == -1);
    assert(controller.state().contactConfirmOpen);

    assert(controller.confirm(0).action == Action::CONFIRM_INVITE);
    assert(controller.confirm(1).action == Action::REDRAW);
    assert(!controller.state().contactActionOpen);
    assert(!controller.state().contactConfirmOpen);

    assert(controller.openAction(game, 2));
    assert(controller.selectAction(game, 1, false).action == Action::CONFIRM_INVITE);
    assert(controller.confirm(-1).action == Action::REDRAW);
    assert(!controller.state().contactActionOpen);

    assert(controller.openAction(game, 1));
    assert(controller.selectAction(game, 2, false).action == Action::CONFIRM_DELETE);
    assert(controller.state().contactConfirmDelete);
    assert(controller.confirm(0).action == Action::CONFIRM_DELETE);
    controller.finishDelete();
    assert(!controller.state().contactActionOpen);
    assert(!controller.state().contactConfirmOpen);
    assert(controller.state().contactActionSlot == 0xFF);

    assert(controller.openAction(game, 2));
    controller.dismissConfirmation();
    assert(controller.state().contactActionOpen);
    controller.reset();
    assert(!controller.state().contactActionOpen);
    assert(!controller.state().contactConfirmOpen);
    assert(controller.state().contactActionSlot == 0xFF);
    assert(controller.state().contactActionPressedItem == -1);
    assert(!controller.state().contactConfirmYes);
    assert(!controller.state().contactConfirmDelete);
    assert(std::memcmp(&game, &before, sizeof(game)) == 0);
}

void checkMenuBoundsAndDismissal() {
    Game::GameState game;
    game.teamCount = 0;
    game.storageCount = 1;
    game.storage[0] = monster(20, Game::Origin::BEFRIENDED);
    const auto before = game;
    ContactsController controller;

    assert(controller.openAction(game, 0));
    assert(actionTap(controller, game, -1, true, false).action == Action::NONE);
    assert(controller.state().contactActionOpen);
    assert(actionTap(controller, game, -1, false, false).action == Action::REDRAW);
    assert(!controller.state().contactActionOpen);

    assert(controller.openAction(game, 0));
    assert(controller.selectAction(game, 1, false).action == Action::CONFIRM_INVITE);
    controller.dismissConfirmation();
    assert(controller.state().contactActionOpen);
    assert(!controller.state().contactConfirmOpen);
    assert(controller.state().contactConfirmYes);
    assert(controller.confirm(0).action == Action::NONE);
    assert(std::memcmp(&game, &before, sizeof(game)) == 0);
}

void checkInviteGatePriority() {
    Game::GameState game;
    game.teamCount = Game::TEAM_CAP;
    game.storageCount = 1;
    game.storage[0] = monster(30, Game::Origin::BEFRIENDED);
    game.storage[0].petCountToday = Game::Bond::inviteLockMarker(
        Game::Bond::invitationDay(game.gameMinutesTotal));
    ContactsController controller;

    assert(controller.openAction(game, 0));
    auto result = controller.selectAction(game, 1, true);
    assert(result.action == Action::INVITE_GATE && result.gate == Gate::LOCKED);
    assert(!controller.state().contactActionOpen);

    game.storage[0].petCountToday = 0;
    const auto unlocked = game;
    assert(controller.openAction(game, 0));
    result = controller.selectAction(game, 1, true);
    assert(result.action == Action::INVITE_GATE && result.gate == Gate::VISITING);

    assert(controller.openAction(game, 0));
    result = controller.selectAction(game, 1, false);
    assert(result.action == Action::INVITE_GATE && result.gate == Gate::TEAM_FULL);
    assert(std::memcmp(&game, &unlocked, sizeof(game)) == 0);
}

void checkStorageViewModel() {
    Game::GameState game;
    game.teamCount = 0;
    game.storageCount = 2;
    game.storage[0] = monster(40, Game::Origin::BEFRIENDED);
    game.storage[1] = monster(41, Game::Origin::BEFRIENDED);
    game.storage[1].petCountToday = Game::Bond::inviteLockMarker(
        Game::Bond::invitationDay(game.gameMinutesTotal));
    const auto before = game;
    ContactsController controller;
    assert(controller.openAction(game, 1));
    controller.pressAction(1);
    assert(controller.selectAction(game, 1, true).gate == Gate::LOCKED);
    assert(controller.openAction(game, 1));
    controller.pressAction(2);
    assert(controller.selectAction(game, 2, false).action == Action::CONFIRM_DELETE);

    const ComputerViewModel model = controller.storageModel(game, 24.0f, 1, 1);
    assert(model.state == &game);
    assert(model.page == ComputerViewModel::Page::STORAGE);
    assert(model.storageScroll == 24.0f);
    assert(model.pressedItem == 1);
    assert(model.contactActionOpen && model.contactActionSlot == 1);
    assert(model.contactVisitingSlot == 1 && model.contactVisiting);
    assert(model.contactActionPressedItem == 2);
    assert(model.contactConfirmOpen && model.contactConfirmYes);
    assert(model.contactConfirmDelete);
    assert(!model.contactInTeam && model.contactInviteLocked);
    assert(!model.contactCanDelete);
    assert(std::memcmp(&game, &before, sizeof(game)) == 0);
}
}

int main() {
    checkActionsAndReadOnlyState();
    checkMenuBoundsAndDismissal();
    checkInviteGatePriority();
    checkStorageViewModel();
    std::cout << "contacts controller: state, actions, menu dismissal, gates and view model passed\n";
}
