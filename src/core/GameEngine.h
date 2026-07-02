#pragma once

#include <cstdint>
#include <memory>
#include "core/SaveManager.h"
#include "core/Scene.h"
#include "game/GameState.h"
#include "game/Species.h"

class GameEngine {
public:
    static GameEngine& ins();

    bool begin();
    void run();
    void requestScene(SceneID id);
    SceneID previousScene() const { return prevId; }
    SceneID homeScene() const { return state.oobeDone ? SceneID::MAIN : SceneID::HATCH; }

    float gameSpeed() const;
    uint32_t gameMinutesTotal() const;
    uint16_t gameMinutesOfDay() const;
    void cycleGameSpeed();
    uint8_t idleTimeoutIndex() const;
    const char* idleTimeoutLabel() const;
    void cycleIdleTimeout();
    uint8_t foodCount() const { return state.bag.normalFood; }
    uint8_t ballCount() const { return state.bag.pokeBall; }
    uint8_t greatBallCount() const { return state.bag.greatBall; }
    uint8_t candyCount() const { return state.bag.candy; }
    uint8_t potionCount() const { return state.bag.potion; }
    uint8_t superPotionCount() const { return state.bag.superPotion; }
    uint8_t antidoteCount() const { return state.bag.antidote; }
    uint8_t capturedCount() const { return state.teamCount + state.storageCount; }
    uint32_t coinCount() const { return state.coins; }
    uint8_t hungerValue() const;
    uint8_t moodValue() const;
    const Game::GameState& gameState() const { return state; }
    Game::GameState& gameState() { return state; }
    const Game::MonsterRuntime& activeMonster() const;
    Game::MonsterRuntime& activeMonster();
    const Species& activeSpecies() const;
    const Species& speciesFor(const Game::MonsterRuntime& monster) const;
    Game::MonsterRuntime createMonster(uint16_t speciesId, uint8_t level) const;
    uint8_t activeSlot() const { return 0; }
    bool switchActiveMonster();
    bool moveTeamMemberToFront(uint8_t slot);
    void addFood(uint8_t amount = 1);
    bool consumeFood();
    void addBalls(uint8_t amount);
    bool consumeBall();
    bool addGreatBalls(uint8_t amount);
    bool consumeGreatBall();
    void addCandy(uint8_t amount);
    bool addPotion(uint8_t amount);
    bool addSuperPotion(uint8_t amount);
    bool usePotion();
    bool useSuperPotion();
    bool addAntidote(uint8_t amount);
    uint8_t itemCount(Game::ItemId item) const;
    bool removeItem(Game::ItemId item, uint8_t amount = 1, bool immediate = true);
    bool spendCoins(uint32_t amount);
    void addCoins(uint32_t amount);
    bool recordCapture(uint16_t speciesId);
    bool recordCapture(const Game::MonsterRuntime& monster);
    void grantEffortFrom(const Species& defeatedSpecies);
    void petMonster();
    void finishHatch(uint8_t starterStyle);
    void addExperience(uint32_t amount);
    uint32_t applyActiveFaintPenalty();
    void addWalkSteps(uint16_t steps);
    void debugRecoverActiveMonster();
    bool debugSetActiveSpecies(uint16_t speciesId);
    void markDirty(bool immediate = false);
    bool saveNow();
    bool loadHatchProgress(Game::HatchProgress& progress);
    bool saveHatchProgress(const Game::HatchProgress& progress);
    void clearHatchProgress();

private:
    GameEngine() = default;

    void switchScene(SceneID id);
    void processInput(uint32_t nowMs);
    void update(uint32_t nowMs);
    void render(uint32_t nowMs);
    void resetIdle(uint32_t nowMs);
    void updateIdle(uint32_t nowMs);
    uint32_t idleTimeoutMs() const;
    uint32_t gameMinutesTotalAt(uint32_t nowMs) const;
    void syncGameClock(uint32_t nowMs);
    void resetGameClockAnchor(uint32_t nowMs);
    void persistGameClock(uint32_t nowMs, bool force = false);
    void initDefaultState();
    void tickCare(uint32_t nowMs);
    uint32_t randomIvPacked() const;

    static constexpr uint32_t INPUT_SAMPLE_MS = 16;
    static constexpr uint32_t FRAME_MS = 66;
    static constexpr uint32_t IDLE_FRAME_MS = 250;

    std::unique_ptr<Scene> currentScene;
    SceneID currentId = SceneID::MAIN;
    SceneID prevId = SceneID::MAIN;
    uint32_t lastInputMs = 0;
    uint32_t lastFrameMs = 0;
    uint32_t lastUpdateMs = 0;
    uint32_t lastCareMs = 0;
    uint32_t lastSaveMs = 0;
    uint32_t lastActivityMs = 0;
    uint32_t clockAnchorMs = 0;
    uint32_t clockAnchorMinutes = 0;
    uint32_t lastClockSaveMs = 0;
    uint32_t lastSavedClockMinutes = 0;
    uint16_t hpRecoveryMinuteAcc = 0;
    bool idleActive = false;
    bool saveDirty = false;

    Game::GameState state;
    SaveManager saveManager;
};
