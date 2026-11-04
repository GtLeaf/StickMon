#include "core/ProgressionUi.h"
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "ui/ProgressionUi.h"

namespace {
class Effects : public Ui::ProgressionEffects {
    void stopProgressionCry() override { CryPlayer::ins().stop(); }
    void playProgressionCry(uint16_t speciesId) override { CryPlayer::ins().replay(speciesId); }
};
Effects effects;
Ui::ProgressionUi ui(effects);

Ui::MoveLearnFrame moveLearnFrame() {
    auto& engine = GameEngine::ins();
    const auto& state = engine.gameState();
    Ui::MoveLearnFrame frame;
    frame.pending = engine.hasPendingMoveLearn();
    frame.needsReplacement = engine.pendingMoveLearnNeedsReplacement();
    frame.moveId = engine.pendingMoveLearnId();
    const auto slot = engine.pendingMoveLearnSlot();
    if (slot < state.teamCount && slot < Game::TEAM_CAP) {
        frame.monster = &state.team[slot];
        frame.species = &engine.speciesFor(*frame.monster);
    }
    return frame;
}
}  // namespace

namespace ProgressionUi {
void renderLevelUp(const char* name, uint8_t level) { ui.renderLevelUp(name, level); }
bool handleLevelUpTouch(const TouchTap::Event& event) { return ui.handleLevelUpTouch(event); }
bool evolutionAnimationComplete(uint16_t from, uint16_t to, uint32_t now) {
    return ui.evolutionAnimationComplete(from, to, now);
}
void resetEvolutionAnimation() { ui.resetEvolutionAnimation(); }
void beginEvolutionCancellation(uint16_t from, uint16_t to, uint32_t now) {
    ui.beginEvolutionCancellation(from, to, now);
}
bool evolutionCancellationComplete(uint32_t now) { return ui.evolutionCancellationComplete(now); }
void renderEvolution(uint16_t from, uint16_t to, uint32_t now) { ui.renderEvolution(from, to, now); }
void renderEvolutionCancelled(uint16_t species, uint32_t now) { ui.renderEvolutionCancelled(species, now); }
void resetMoveLearnState(MoveLearnState& state) { ui.resetMoveLearnState(state); }
namespace {
bool applyMoveLearnAction(MoveLearnState& state, Ui::MoveLearnAction action) {
    auto& engine = GameEngine::ins();
    switch (action) {
    case Ui::MoveLearnAction::COMPLETE: return true;
    case Ui::MoveLearnAction::ACCEPT: engine.resolvePendingMoveLearn(true); return true;
    case Ui::MoveLearnAction::DECLINE: engine.resolvePendingMoveLearn(false); return true;
    case Ui::MoveLearnAction::REPLACE: {
        bool replaced = engine.resolvePendingMoveLearnReplacing(state.replacementSlot);
        return ui.moveLearnReplacementResolved(state, replaced, engine.hasPendingMoveLearn());
    }
    default: return false;
    }
}
}  // namespace
bool handleMoveLearnInput(MoveLearnState& state, uint8_t button) {
    ui.cancelMoveLearnTouch();
    return applyMoveLearnAction(state, ui.handleMoveLearnInput(state, button, moveLearnFrame()));
}
bool handleMoveLearnTouch(MoveLearnState& state, const TouchTap::Event& event) {
    return applyMoveLearnAction(state, ui.handleMoveLearnTouch(state, event, moveLearnFrame()));
}
bool handleMoveReplacementTouch(const TouchTap::Event& event) {
    return ui.handleMoveReplacementTouch(event);
}
void cancelMoveLearnTouch() { ui.cancelMoveLearnTouch(); }
void renderMoveLearn(const MoveLearnState& state) { ui.renderMoveLearn(state, moveLearnFrame()); }
void renderMoveReplacement() {
    auto& engine = GameEngine::ins();
    ui.renderMoveReplacement(engine.pendingMoveReplacementOldId(), engine.pendingMoveReplacementNewId());
}
}  // namespace ProgressionUi
