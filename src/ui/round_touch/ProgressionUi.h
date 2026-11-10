// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/TouchScroll.h"
#include "ui/common/ProgressionUiBase.h"

namespace Ui {
// Round move learning follows the AMOLED list: new move first, inline details,
// one explicit action and no second confirmation. Shared rules stay in the base.
class ProgressionUi final : public ProgressionUiBase {
public:
    explicit ProgressionUi(ProgressionEffects& effects) : ProgressionUiBase(effects) {}
    ProgressionLayout layout() const override;
    MoveLearnAction handleMoveLearnInput(MoveLearnState& state, uint8_t button,
                                         const MoveLearnFrame& frame) override;
    MoveLearnAction handleMoveLearnTouch(MoveLearnState& state, const TouchTap::Event& event,
                                         const MoveLearnFrame& frame) override;
    bool handleLevelUpTouch(const TouchTap::Event& event) override;
    bool handleMoveReplacementTouch(const TouchTap::Event& event) override;
    void cancelMoveLearnTouch() override;
    void renderMoveLearn(const MoveLearnState& state, const MoveLearnFrame& frame) override;
    void renderMoveReplacement(Game::MoveId oldId, Game::MoveId newId) override;
    // Native 360px evolution: centered sprite, hold anywhere to cancel, tap to continue.
    void renderEvolution(uint16_t fromSpeciesId, uint16_t toSpeciesId, uint32_t nowMs) override;
    void renderEvolutionCancelled(uint16_t speciesId, uint32_t nowMs) override;
    EvolutionTouchAction handleEvolutionTouch(const TouchTap::Event& event, uint32_t nowMs) override;
    bool evolutionCancelHeld(uint32_t nowMs) override;
    static constexpr uint32_t EVOLUTION_CANCEL_HOLD_MS = 1000;
    static constexpr int EVOLUTION_CENTER_X = 180;
    static constexpr int EVOLUTION_CENTER_Y = 168;
    static constexpr int EVOLUTION_GROUND_Y = 220;
    static constexpr int EVOLUTION_MESSAGE_Y = 240;
    static MenuGrid::Rect evolutionContinueButton() { return {110, 290, 140, 36}; }
    bool evolutionHolding() const { return evolutionHold; }

    static constexpr int CARD_COUNT = 3;
    static constexpr int ACTION_TARGET = 100;
    // Cards are ordered like AMOLED: pending move, then special slots 1 and 2.
    static uint8_t cardSlot(int card);
    static MenuGrid::Rect listViewport();
    static MenuGrid::Rect actionButton();
    float listScroll() const { return scroll; }
    float maxListScroll(const MoveLearnState& state, const MoveLearnFrame& frame) const;
    MenuGrid::Rect cardRect(const MoveLearnState& state, const MoveLearnFrame& frame, int card) const;
    int moveLearnTarget(const MoveLearnState& state, const MoveLearnFrame& frame, int x, int y) const;
private:
    TouchTap learnTap;
    TouchScroll listTouch;
    const Game::MonsterRuntime* touchMonster = nullptr;
    Game::MoveId touchMove = 0;
    uint16_t touchState = 0;
    const Game::MonsterRuntime* shownMonster = nullptr;
    Game::MoveId shownMove = 0;
    float scroll = 0.0f;
    bool revealSelection = false;
    uint32_t evolutionHoldStarted = 0;
    bool evolutionHold = false;
    bool evolutionTapArmed = false;
    bool evolutionAnimating(uint32_t nowMs) const;
    bool evolutionReady(uint32_t nowMs) const;
    void drawEvolutionHint(uint32_t nowMs, bool cancelling);
    void syncFrame(const MoveLearnFrame& frame);
    void select(MoveLearnState& state, uint8_t slot);
    int detailHeight(const MoveLearnState& state, const MoveLearnFrame& frame) const;
    int contentHeight(const MoveLearnState& state, const MoveLearnFrame& frame) const;
    void renderReplacementList(const MoveLearnState& state, const MoveLearnFrame& frame);
    void renderLearnedCard(const MoveLearnFrame& frame);
};
}  // namespace Ui
