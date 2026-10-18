#include "TeamController.h"

#include <algorithm>
#include <cmath>

#include "AmoledGeometry.h"
#include "game/Species.h"
#include "ui/UiMetrics.h"

namespace AmoledV1 {
namespace {
constexpr int STATUS_SNAP_THRESHOLD = 72;
constexpr int MOVES_ROW_STEP = 66;
constexpr int MOVES_HEADER_HEIGHT = UiMetrics::PAGE_HEADER_HEIGHT;
}

void TeamController::resetStatus() {
    state_.teamStatusFromContact = false;
    state_.teamStatusContactSlot = 0xFF;
    state_.teamStatusPage = 0;
    state_.teamStatusSlideX = 0;
    state_.teamStatusAnimating = false;
    state_.teamStatusDragging = false;
}

void TeamController::openTeam() {
    state_ = State{};
}

void TeamController::closeStatus() {
    state_.teamStatusOpen = false;
    resetStatus();
}

void TeamController::openStatus(uint8_t slot) {
    resetStatus();
    state_.teamStatusOpen = true;
    state_.teamStatusSlot = slot;
}

void TeamController::openContactStatus(uint8_t contactSlot) {
    resetStatus();
    state_.teamStatusFromContact = true;
    state_.teamStatusContactSlot = contactSlot;
}

void TeamController::interruptStatusSlide() {
    if (state_.teamStatusAnimating) {
        state_.teamStatusPage = state_.teamStatusAnimTargetPage;
        state_.teamStatusAnimating = false;
        state_.teamStatusSlideX = 0;
    }
    state_.teamStatusDragging = false;
}

void TeamController::dragStatus(int deltaX) {
    if ((state_.teamStatusPage == 0 && deltaX > 0) ||
        (state_.teamStatusPage + 1 >= STATUS_PAGE_COUNT && deltaX < 0)) {
        deltaX /= 3;
    }
    state_.teamStatusSlideX = static_cast<int16_t>(
        std::clamp(deltaX, -AmoledUi::WIDTH, AmoledUi::WIDTH));
}

void TeamController::releaseStatus(uint32_t nowMs) {
    state_.teamStatusDragging = false;
    uint8_t target = state_.teamStatusPage;
    if (state_.teamStatusSlideX <= -STATUS_SNAP_THRESHOLD &&
        target + 1 < STATUS_PAGE_COUNT) ++target;
    else if (state_.teamStatusSlideX >= STATUS_SNAP_THRESHOLD && target > 0) --target;
    beginStatusSlide(target, nowMs);
}

void TeamController::beginStatusSlide(uint8_t targetPage, uint32_t nowMs) {
    targetPage = std::min<uint8_t>(targetPage, STATUS_PAGE_COUNT - 1);
    state_.teamStatusAnimating = true;
    state_.teamStatusAnimStartMs = nowMs;
    state_.teamStatusAnimFromX = state_.teamStatusSlideX;
    state_.teamStatusAnimToX = static_cast<int16_t>(
        (static_cast<int>(state_.teamStatusPage) - targetPage) * AmoledUi::WIDTH);
    state_.teamStatusAnimTargetPage = targetPage;
}

bool TeamController::openMoves(const Game::GameState& gameState, uint8_t slot) {
    if (slot >= gameState.teamCount || slot >= Game::TEAM_CAP) return false;
    state_.teamMovesOpen = true;
    state_.teamMovesSlot = slot;
    state_.teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
    state_.teamMovesScroll = 0;
    state_.teamMovesDragging = false;
    state_.teamMovesSelectedItem = 0xFF;
    state_.teamMovesRecallSelected = 0xFF;
    state_.teamMovesDetailProgress = 0.0f;
    state_.teamMovesDetailAnimating = false;
    state_.teamMovesDetailTargetVisible = false;
    refreshRecallable(gameState);
    return true;
}

void TeamController::refreshRecallable(const Game::GameState& gameState) {
    state_.teamMovesRecallCount = 0;
    if (!state_.teamMovesOpen || state_.teamMovesSlot >= gameState.teamCount ||
        state_.teamMovesSlot >= Game::TEAM_CAP) return;
    const auto& monster = gameState.team[state_.teamMovesSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return;
    state_.teamMovesRecallCount = Game::MoveManagementService::collectRecallable(
        *species, monster, state_.teamMovesRecallIds,
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
}

void TeamController::clampMovesScroll() {
    uint8_t rows = Game::MOVE_SLOT_COUNT;
    if (state_.teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT)
        rows = static_cast<uint8_t>(state_.teamMovesRecallCount + 1);
    else if (state_.teamMovesMode == TeamMovesViewModel::Mode::RECALL_REPLACE) rows = 3;
    int height = static_cast<int>(rows) * MOVES_ROW_STEP;
    if (state_.teamMovesMode == TeamMovesViewModel::Mode::MANAGE &&
        state_.teamMovesSelectedItem < rows) {
        height += static_cast<int>(std::lround(
            UiMetrics::MOVE_DETAIL_HEIGHT * state_.teamMovesDetailProgress));
    }
    const int maximum = std::max(0, height - (AmoledUi::HEIGHT - MOVES_HEADER_HEIGHT));
    state_.teamMovesScroll = std::clamp<int16_t>(
        state_.teamMovesScroll, 0, static_cast<int16_t>(maximum));
}

void TeamController::startDetailAnimation(bool visible, uint32_t nowMs) {
    state_.teamMovesDetailTargetVisible = visible;
    state_.teamMovesDetailAnimStartMs = nowMs;
    state_.teamMovesDetailAnimating = true;
    if ((visible && state_.teamMovesDetailProgress >= 1.0f) ||
        (!visible && state_.teamMovesDetailProgress <= 0.0f)) {
        state_.teamMovesDetailAnimating = false;
        state_.teamMovesDetailProgress = visible ? 1.0f : 0.0f;
        if (!visible) state_.teamMovesSelectedItem = 0xFF;
    }
}

TeamController::MoveSelection TeamController::selectMove(
    int item, bool learned, uint32_t nowMs) {
    if (item < 0) return {};
    if (state_.teamMovesMode == TeamMovesViewModel::Mode::MANAGE) {
        if (item >= Game::MOVE_SLOT_COUNT) return {};
        if (!learned) return {MoveAction::EMPTY, 0, 0xFF};
        if (state_.teamMovesSelectedItem == item &&
            !state_.teamMovesDetailAnimating && state_.teamMovesDetailProgress >= 0.99f) {
            startDetailAnimation(false, nowMs);
        } else if (state_.teamMovesSelectedItem != item) {
            state_.teamMovesSelectedItem = static_cast<uint8_t>(item);
            if (state_.teamMovesDetailProgress > 0.0f) {
                state_.teamMovesDetailProgress = 1.0f;
                state_.teamMovesDetailAnimating = false;
            } else startDetailAnimation(true, nowMs);
        }
        return {MoveAction::REDRAW, 0, 0xFF};
    }
    if (state_.teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT) {
        if (item >= state_.teamMovesRecallCount) {
            state_.teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
            state_.teamMovesRecallSelected = 0xFF;
            state_.teamMovesSelectedItem = 0xFF;
        } else {
            state_.teamMovesRecallSelected = static_cast<uint8_t>(item);
            state_.teamMovesMode = TeamMovesViewModel::Mode::RECALL_REPLACE;
        }
    } else if (item < 2 && state_.teamMovesRecallSelected < state_.teamMovesRecallCount) {
        return {MoveAction::RECALL,
            state_.teamMovesRecallIds[state_.teamMovesRecallSelected],
            static_cast<uint8_t>(item + 1)};
    } else state_.teamMovesMode = TeamMovesViewModel::Mode::RECALL_SELECT;
    state_.teamMovesScroll = 0;
    return {MoveAction::REDRAW, 0, 0xFF};
}

void TeamController::completeRecall(const Game::GameState& gameState) {
    refreshRecallable(gameState);
    state_.teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
    state_.teamMovesRecallSelected = 0xFF;
    state_.teamMovesSelectedItem = 0xFF;
    state_.teamMovesScroll = 0;
}

TeamController::Update TeamController::tick(uint32_t nowMs) {
    Update result;
    if (state_.teamStatusAnimating) {
        result.statusChanged = true;
        const uint32_t elapsed = nowMs - state_.teamStatusAnimStartMs;
        if (elapsed >= STATUS_SLIDE_MS) {
            state_.teamStatusPage = state_.teamStatusAnimTargetPage;
            state_.teamStatusSlideX = 0;
            state_.teamStatusAnimating = false;
            result.statusCompleted = true;
        } else {
            const float t = static_cast<float>(elapsed) / STATUS_SLIDE_MS;
            const float ease = 1.0f - (1.0f - t) * (1.0f - t);
            state_.teamStatusSlideX = static_cast<int16_t>(state_.teamStatusAnimFromX +
                static_cast<int>((state_.teamStatusAnimToX - state_.teamStatusAnimFromX) * ease));
        }
    }
    if (state_.teamMovesOpen && state_.teamMovesDetailAnimating) {
        result.movesChanged = true;
        const float raw = std::min(1.0f,
            static_cast<float>(nowMs - state_.teamMovesDetailAnimStartMs) / DETAIL_ANIMATION_MS);
        const float eased = state_.teamMovesDetailTargetVisible
            ? 1.0f - (1.0f - raw) * (1.0f - raw) : raw * raw;
        state_.teamMovesDetailProgress = state_.teamMovesDetailTargetVisible ? eased : 1.0f - eased;
        if (raw >= 1.0f) {
            state_.teamMovesDetailAnimating = false;
            state_.teamMovesDetailProgress = state_.teamMovesDetailTargetVisible ? 1.0f : 0.0f;
            if (!state_.teamMovesDetailTargetVisible) state_.teamMovesSelectedItem = 0xFF;
            clampMovesScroll();
        }
    }
    return result;
}

TeamViewModel TeamController::teamModel(const Game::GameState& gameState, const char* toast) const {
    TeamViewModel model;
    model.state = &gameState;
    model.pressedSlot = state_.pressedTeamSlot;
    model.confirmOpen = state_.teamConfirmOpen;
    model.actionPopupOpen = state_.teamActionPopupOpen;
    model.actionPopupSlot = state_.teamActionPopupSlot;
    model.pendingSlot = state_.pendingTeamSlot;
    model.toast = toast;
    return model;
}

TeamStatusViewModel TeamController::statusModel(const Game::GameState& gameState,
    const Game::MonsterRuntime* contactMonster) const {
    TeamStatusViewModel model;
    model.state = &gameState;
    model.teamSlot = state_.teamStatusFromContact ? 0 : state_.teamStatusSlot;
    model.monster = state_.teamStatusFromContact ? contactMonster : nullptr;
    model.page = state_.teamStatusPage;
    model.slideOffsetX = state_.teamStatusSlideX;
    return model;
}

TeamMovesViewModel TeamController::movesModel(const Game::GameState& gameState,
    const char* toast) const {
    TeamMovesViewModel model;
    model.state = &gameState;
    model.teamSlot = state_.teamMovesSlot;
    model.mode = state_.teamMovesMode;
    model.selectedItem = state_.teamMovesMode == TeamMovesViewModel::Mode::MANAGE
        ? state_.teamMovesSelectedItem : state_.teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT
        ? state_.teamMovesRecallSelected : 0xFF;
    model.scrollOffsetY = state_.teamMovesScroll;
    model.recallCount = state_.teamMovesRecallCount;
    std::copy_n(state_.teamMovesRecallIds, model.recallCount, model.recallIds);
    model.recallSelected = state_.teamMovesRecallSelected;
    model.detailProgress = state_.teamMovesDetailProgress;
    model.toast = toast;
    return model;
}

}  // namespace AmoledV1
