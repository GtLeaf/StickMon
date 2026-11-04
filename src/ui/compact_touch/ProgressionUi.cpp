#include "ui/compact_touch/ProgressionUi.h"
#include "ui/SceneSurface.h"

#include "hardware/Hal.h"
namespace Ui {
ProgressionLayout ProgressionUi::layout() const { return {Ui::SceneSurface::width(), Ui::SceneSurface::height(), true, STICKMON_UI_ROUND_TOUCH != 0}; }

void ProgressionUi::cancelMoveLearnTouch() {
    learnTap.cancel();
    touchMonster = nullptr;
    touchMove = 0;
    touchState = 0;
}

MoveLearnAction ProgressionUi::handleMoveLearnTouch(MoveLearnState& state,
        const TouchTap::Event& event, const MoveLearnFrame& frame) {
    const uint16_t key = 1 + static_cast<uint16_t>(state.stage) +
        (state.confirmOpen ? 4 : 0) + (state.confirmYes ? 8 : 0) +
        state.replacementSlot * 16 + state.promptCursor * 128 +
        (frame.needsReplacement ? 256 : 0);
    if (event.type == TouchTap::Type::DOWN) {
        touchMonster = frame.monster;
        touchMove = frame.moveId;
        touchState = key;
    } else if (touchMonster != frame.monster || touchMove != frame.moveId || touchState != key) {
        cancelMoveLearnTouch();
        return MoveLearnAction::NONE;
    }
    if (!frame.pending) { cancelMoveLearnTouch(); return MoveLearnAction::NONE; }
    const auto metrics = layout();
    int target = -1;
    const bool prompt = state.stage == MoveLearnStage::PROMPT || !frame.monster || !frame.species;
    if (prompt || state.confirmOpen) {
        for (int choice = 0; choice < 2; ++choice)
            if (metrics.learnChoice(!prompt, choice == 0).contains(event.x, event.y)) target = choice;
    } else {
        // Slot zero is the permanent basic move and cannot be forgotten.
        for (int slot = 1; slot <= Game::MOVE_SLOT_COUNT; ++slot)
            if (metrics.learnRow(slot).contains(event.x, event.y)) target = 10 + slot;
    }
    const int tapped = learnTap.update(event, target);
    if (tapped < 0) return MoveLearnAction::NONE;
    cancelMoveLearnTouch();
    if (prompt) state.promptCursor = static_cast<uint8_t>(tapped);
    else if (state.confirmOpen) state.confirmYes = tapped == 0;
    else {
        state.replacementSlot = static_cast<uint8_t>(tapped - 10);
        state.confirmOpen = true;
        state.confirmYes = false;
        return MoveLearnAction::NONE;
    }
    return handleMoveLearnInput(state, 0, frame);
}

bool ProgressionUi::handleLevelUpTouch(const TouchTap::Event& event) {
    if (!layout().bottomLevelUp) return false;
    if (event.type == TouchTap::Type::DOWN) {
        cancelMoveLearnTouch();
        touchState = 2048;
    } else if (touchState != 2048) {
        cancelMoveLearnTouch();
        return false;
    }
    const int target = layout().levelUpPanel().contains(event.x, event.y) ? 0 : -1;
    const bool tapped = learnTap.update(event, target) == 0;
    if (tapped) cancelMoveLearnTouch();
    return tapped;
}

bool ProgressionUi::handleMoveReplacementTouch(const TouchTap::Event& event) {
    if (event.type == TouchTap::Type::DOWN) {
        cancelMoveLearnTouch();
        touchState = 1024;
    } else if (touchState != 1024) {
        cancelMoveLearnTouch();
        return false;
    }
    const int target = layout().replacedContinue().contains(event.x, event.y) ? 0 : -1;
    const bool tapped = learnTap.update(event, target) == 0;
    if (tapped) cancelMoveLearnTouch();
    return tapped;
}
}  // namespace Ui
