#pragma once

#include <cstdint>

#include "game/MoveManagementService.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Owns transient team-page state. Game mutations and navigation remain with
// the application and shared services, rather than a back-reference to App.
class TeamController {
public:
    static constexpr uint8_t STATUS_PAGE_COUNT = 5;
    static constexpr uint32_t STATUS_SLIDE_MS = 180;
    static constexpr uint32_t DETAIL_ANIMATION_MS = 240;

    struct State {
        bool teamActionPopupOpen = false;
        uint8_t teamActionPopupSlot = 0;
        int pressedTeamSlot = -1;
        bool teamConfirmOpen = false;
        uint8_t pendingTeamSlot = 0;
        bool teamStatusOpen = false;
        bool teamStatusFromContact = false;
        uint8_t teamStatusContactSlot = 0xFF;
        uint8_t teamStatusSlot = 0;
        uint8_t teamStatusPage = 0;
        bool teamStatusDragging = false;
        int16_t teamStatusSlideX = 0;
        bool teamStatusAnimating = false;
        uint32_t teamStatusAnimStartMs = 0;
        int16_t teamStatusAnimFromX = 0;
        int16_t teamStatusAnimToX = 0;
        uint8_t teamStatusAnimTargetPage = 0;
        bool teamMovesOpen = false;
        uint8_t teamMovesSlot = 0;
        TeamMovesViewModel::Mode teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
        int16_t teamMovesScroll = 0;
        bool teamMovesDragging = false;
        uint8_t teamMovesSelectedItem = 0xFF;
        Game::MoveId teamMovesRecallIds[
            Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT] = {};
        uint8_t teamMovesRecallCount = 0;
        uint8_t teamMovesRecallSelected = 0xFF;
        float teamMovesDetailProgress = 0.0f;
        bool teamMovesDetailAnimating = false;
        bool teamMovesDetailTargetVisible = false;
        uint32_t teamMovesDetailAnimStartMs = 0;
    };

    struct Update {
        bool statusChanged = false;
        bool statusCompleted = false;
        bool movesChanged = false;
        bool changed() const { return statusChanged || movesChanged; }
    };

    enum class MoveAction : uint8_t { NONE, EMPTY, REDRAW, RECALL };
    struct MoveSelection {
        MoveAction action = MoveAction::NONE;
        Game::MoveId moveId = 0;
        uint8_t replacementSlot = 0xFF;
    };

    const State& state() const { return state_; }

    void openTeam();
    void cancelTouch();
    void pressMember(int slot) { state_.pressedTeamSlot = slot; }
    void beginMovesGesture() { state_.teamMovesDragging = false; }
    void dragMoves(int deltaY);
    void endMovesGesture() { state_.teamMovesDragging = false; }
    void endStatusGesture() { state_.teamStatusDragging = false; }
    void beginMovesDrag() { state_.teamMovesDragging = true; }
    void beginStatusDrag() { state_.teamStatusDragging = true; }
    void showActions(uint8_t slot);
    void dismissActions() { state_.teamActionPopupOpen = false; }
    void selectLeaderCandidate(uint8_t slot) { state_.pendingTeamSlot = slot; }
    void requestLeaderConfirmation(uint8_t slot);
    void dismissConfirmation() { state_.teamConfirmOpen = false; }
    void completeLeaderChange();
    void beginRecallSelection() {
        state_.teamMovesMode = TeamMovesViewModel::Mode::RECALL_SELECT;
        state_.teamMovesScroll = 0;
    }
    void closeMoves() { state_.teamMovesOpen = false; }
    void resetStatus();
    void closeStatus();
    void openStatus(uint8_t slot);
    void openContactStatus(uint8_t contactSlot);
    void interruptStatusSlide();
    void dragStatus(int deltaX);
    void releaseStatus(uint32_t nowMs);
    void beginStatusSlide(uint8_t targetPage, uint32_t nowMs);
    bool openMoves(const Game::GameState& gameState, uint8_t slot);
    void refreshRecallable(const Game::GameState& gameState);
    void clampMovesScroll();
    void startDetailAnimation(bool visible, uint32_t nowMs);
    MoveSelection selectMove(int item, bool learned, uint32_t nowMs);
    void completeRecall(const Game::GameState& gameState);
    Update tick(uint32_t nowMs);

    TeamViewModel teamModel(const Game::GameState& gameState,
                            const char* toast = nullptr) const;
    TeamStatusViewModel statusModel(const Game::GameState& gameState,
        const Game::MonsterRuntime* contactMonster = nullptr) const;
    TeamMovesViewModel movesModel(const Game::GameState& gameState,
                                  const char* toast = nullptr) const;

private:
    State state_{};
};

}  // namespace AmoledV1
