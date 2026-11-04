#pragma once

#include <cstdint>

#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

class ContactsController {
public:
    enum class Action : uint8_t {
        NONE,
        OPEN_STATUS,
        INVITE_GATE,
        CONFIRM_INVITE,
        CONFIRM_DELETE,
        REDRAW,
    };
    enum class InviteGate : uint8_t { NONE, LOCKED, VISITING, TEAM_FULL };

    struct State {
        bool contactActionOpen = false;
        uint8_t contactActionSlot = 0xFF;
        int contactActionPressedItem = -1;
        bool contactConfirmOpen = false;
        bool contactConfirmYes = false;
        bool contactConfirmDelete = false;
    };

    struct Result {
        Action action = Action::NONE;
        InviteGate gate = InviteGate::NONE;
        uint8_t slot = 0xFF;
    };

    const State& state() const { return state_; }

    void reset();
    void closeOverlay();
    void finishDelete();
    void dismissConfirmation() { state_.contactConfirmOpen = false; }
    void cancelTouch() { state_.contactActionPressedItem = -1; }
    void pressAction(int item) { state_.contactActionPressedItem = item; }

    bool openAction(const Game::GameState& gameState, uint8_t slot);
    const Game::MonsterRuntime* monster(const Game::GameState& gameState,
                                        uint8_t slot) const;
    bool inTeam(const Game::GameState& gameState, uint8_t slot) const;
    bool inviteLocked(const Game::GameState& gameState, uint8_t slot) const;
    bool canDelete(const Game::GameState& gameState, uint8_t slot,
                   bool visiting) const;
    uint8_t actionCount(const Game::GameState& gameState, uint8_t slot,
                        bool visiting) const;

    Result selectAction(const Game::GameState& gameState, int action,
                        bool visiting);
    Result confirm(int choice);

    ComputerViewModel storageModel(const Game::GameState& gameState,
                                   float storageScroll, int pressedItem,
                                   uint8_t visitingSlot) const;

private:
    State state_{};
};

}  // namespace AmoledV1
