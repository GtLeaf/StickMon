#pragma once

#include "game/EvolutionSequence.h"
#include "game/ItemInventory.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

// Owns the growth session and its gestures. Hardware, storage, rendering and
// navigation are applied by the caller after consuming one-shot effects.
class ProgressionFlow {
public:
    struct State {
        AppSceneFlow::Scene progressionReturnScene = AppSceneFlow::Scene::HOME;
        ProgressionViewModel::Mode progressionMode =
            ProgressionViewModel::Mode::LEVEL_UP;
        uint8_t progressionTeamSlot = 0;
        uint8_t progressionOldLevel = 1;
        uint8_t progressionLevel = 1;
        uint32_t progressionLevelUpStartedMs = 0;
        bool progressionLevelUpTouchAllowed = false;
        uint16_t progressionFromSpeciesId = 0;
        uint16_t progressionToSpeciesId = 0;
        Game::EvolutionSequence progressionEvolution;
        Game::ItemId progressionEvolutionItem = Game::ItemId::COUNT;
        uint32_t nextProgressionAnimationFrameMs = 0;
        uint32_t progressionEvolutionTouchStartedMs = 0;
        uint32_t progressionEvolutionTouchResumeUntilMs = 0;
        bool progressionEvolutionTouchHeld = false;
        bool progressionEvolutionTouchConsumed = false;
        Game::MoveId progressionMoveId = 0;
        Game::MoveId progressionOldMove2 = 0;
        Game::MoveId progressionOldMove3 = 0;
        uint16_t progressionMoveCursor = 0;
        uint8_t progressionPressedItem = 0xFF;
        uint8_t progressionSelectedItem = 0xFF;
        int16_t progressionScroll = 0;
        float progressionDetailProgress = 0.0f;
        bool progressionDetailAnimating = false;
        bool progressionDetailTargetVisible = false;
        uint32_t progressionDetailAnimStartMs = 0;

        bool progressionFromCare = false;
    };
    struct Effects {
        bool save = false;
        bool completed = false;
        bool syncTeam = false;
        bool noStock = false;
        bool stopCry = false;
        uint16_t crySpecies = 0;
        uint16_t preloadFrom = 0, preloadTo = 0;
        int rowBegin = 448, rowEnd = 0;
    };
    struct Release { bool consumed = false; bool resume = false; };

    const State& state() const { return state_; }
    bool openLevel(Game::GameState& gameState, AppSceneFlow::Scene returnScene,
                   uint8_t slot, uint8_t oldLevel, uint32_t nowMs, bool fromCare = false);
    bool openEvolution(const Game::GameState& gameState, AppSceneFlow::Scene returnScene,
                       uint8_t slot, uint16_t from, uint16_t to, uint32_t nowMs,
                       Game::ItemId item = Game::ItemId::COUNT);
    void advance(Game::GameState& gameState, uint32_t nowMs);
    void tap(Game::GameState& gameState, int x, int y, uint32_t nowMs);
    void update(uint32_t nowMs);
    bool cancelEvolution(uint32_t nowMs);
    void beginTouch(int x, int y, uint32_t nowMs);
    Release prepareRelease(uint32_t nowMs);
    void finishTouch(uint32_t nowMs, bool resume);
    void cancelTouch();
    void clearPress() { state_.progressionPressedItem = 0xFF; }
    void dragReplacement(int deltaY);
    ProgressionViewModel model(const Game::GameState& gameState, const char* toast, uint32_t nowMs) const;
    Effects takeEffects();

private:
    void beginEvolution(uint16_t from, uint16_t to, uint32_t nowMs,
                        Game::ItemId item = Game::ItemId::COUNT);
    bool findNextMove(const Game::GameState& gameState, uint8_t slot, uint8_t oldLevel,
                      uint16_t& cursor, Game::MoveId& move) const;
    void complete(Game::GameState& gameState);
    void damageRows(int top, int bottom);
    bool active_ = false;
    State state_{};
    Effects effects_{};
};

}  // namespace AmoledV1
