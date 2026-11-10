#pragma once
#include "game/EvolutionSequence.h"
#include "assets/PokemonSprites.h"
#include "ui/common/ProgressionUiModel.h"
#include "core/TouchTap.h"
#include "presentation/MenuGrid.h"
namespace Ui {
struct ProgressionLayout {
    int width;
    int height;
    bool touchMoveLearn = false;
    bool bottomLevelUp = false;
    MenuGrid::Rect levelUpPanel() const { return {-4, height - 74, width + 8, 82}; }
    MenuGrid::Rect learnPrompt() const { return {28, (height - 148) / 2, width - 56, 148}; }
    MenuGrid::Rect learnConfirm() const { return {28, (height - 136) / 2, width - 56, 136}; }
    MenuGrid::Rect learnChoice(bool confirmation, bool yes) const {
        const auto popup = confirmation ? learnConfirm() : learnPrompt();
        const int w = (popup.width - 32) / 2;
        return {popup.x + (yes ? 12 : popup.width / 2 + 4),
                popup.y + popup.height - 52, w, 44};
    }
    MenuGrid::Rect learnRow(int slot) const { return {2, 12 + slot * 50, 86, 44}; }
    MenuGrid::Rect replacedPanel() const { return {28, (height - 128) / 2, width - 56, 128}; }
    MenuGrid::Rect replacedContinue() const {
        const auto panel = replacedPanel();
        return {panel.x + 12, panel.y + 84, panel.width - 24, 36};
    }
};
class ProgressionUiBase {
public:
    explicit ProgressionUiBase(ProgressionEffects& effects) : effects(effects) {}
    virtual ~ProgressionUiBase() = default;
    virtual ProgressionLayout layout() const = 0;
    void renderLevelUp(const char* speciesName, uint8_t level);
    bool evolutionAnimationComplete(uint16_t fromSpeciesId,
                                    uint16_t toSpeciesId,
                                    uint32_t nowMs);
    void resetEvolutionAnimation();
    void beginEvolutionCancellation(uint16_t fromSpeciesId,
                                    uint16_t toSpeciesId,
                                    uint32_t nowMs);
    bool evolutionCancellationComplete(uint32_t nowMs);
    virtual void renderEvolution(uint16_t fromSpeciesId,
                                 uint16_t toSpeciesId,
                                 uint32_t nowMs);
    virtual void renderEvolutionCancelled(uint16_t speciesId, uint32_t nowMs);
    virtual EvolutionTouchAction handleEvolutionTouch(const TouchTap::Event&, uint32_t) {
        return EvolutionTouchAction::NONE;
    }
    // True once when a cancel hold reaches its duration; polled by scene updates.
    virtual bool evolutionCancelHeld(uint32_t) { return false; }
    void resetMoveLearnState(MoveLearnState& state);
    virtual MoveLearnAction handleMoveLearnInput(MoveLearnState& state, uint8_t button,
                                                 const MoveLearnFrame& frame);
    bool moveLearnReplacementResolved(MoveLearnState& state, bool replaced, bool pending);
    virtual void renderMoveLearn(const MoveLearnState& state, const MoveLearnFrame& frame);
    virtual void renderMoveReplacement(Game::MoveId oldId, Game::MoveId newId);
    virtual MoveLearnAction handleMoveLearnTouch(MoveLearnState&, const TouchTap::Event&,
                                                const MoveLearnFrame&) { return MoveLearnAction::NONE; }
    virtual bool handleLevelUpTouch(const TouchTap::Event&) { return false; }
    virtual bool handleMoveReplacementTouch(const TouchTap::Event&) { return false; }
    virtual void cancelMoveLearnTouch() {}
protected:
    ProgressionEffects& effects;
    Game::EvolutionSequence evolutionAnimation;
    void ensureEvolutionAnimation(uint16_t fromSpeciesId, uint16_t toSpeciesId, uint32_t nowMs);
    uint32_t evolutionElapsed(uint32_t nowMs);
    void renderMoveLearnPrompt(const MoveLearnState& state, const MoveLearnFrame& frame);
    void renderMoveLearnReplacementChoice(const MoveLearnState& state, const MoveLearnFrame& frame);
    void drawEvolutionBackground();
    void drawCenteredEvolutionSprite(const PokemonSprites::SpriteFrame* frame, bool silhouette, uint16_t color = 0xFFFF);
    void drawEvolutionRays(uint32_t elapsed);
    void drawCancellationRays(uint32_t elapsed);
    void drawEvolutionSparkles(uint32_t elapsed);
};
}  // namespace Ui
