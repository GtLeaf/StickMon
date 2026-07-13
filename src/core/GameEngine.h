#pragma once

#include <cstdint>
#include <memory>
#include "core/SaveManager.h"
#include "core/Scene.h"
#include "game/GameState.h"
#include "game/Species.h"

enum class FoodPlacementResult : uint8_t {
    ADDED,
    NO_STOCK,
    BOWL_FULL,
    DIFFERENT_FOOD,
};

enum class ExploreTravelPhase : uint8_t {
    NONE,
    DEPARTING,
    ACTIVE,
    RETURNING,
    RETURNING_FAINTED,
};

struct MainSceneViewState {
    bool valid = false;
    uint16_t speciesId = 0;
    float monsterX = 98.0f;
    float monsterY = 91.0f;
    float targetX = 98.0f;
    float targetY = 91.0f;
    uint8_t aiMode = 0;
    uint8_t pmdAction = 0;
    uint8_t pmdDirection = 0;
    uint8_t pmdFrame = 0;
    bool facingRight = true;
    bool faintRestActive = false;
    uint32_t nextDecisionRemainingMs = 0;
    uint32_t postFeedAwakeRemainingMs = 0;
};

class GameEngine {
public:
    static GameEngine& ins();

    bool begin();
    void run();
    void requestScene(SceneID id);
    bool fadeToScene(SceneID id, uint16_t durationMs = 300);
    bool sceneFadeActive() const;
    bool sceneFadeInActive() const;
    SceneID previousScene() const { return prevId; }
    SceneID homeScene() const { return state.oobeDone ? SceneID::MAIN : SceneID::HATCH; }

    float gameSpeed() const;
    uint32_t gameMinutesTotal() const;
    uint16_t gameMinutesOfDay() const;
    bool isMonsterSleepTime() const;
    void cycleGameSpeed();
    uint8_t idleTimeoutIndex() const;
    const char* idleTimeoutLabel() const;
    void cycleIdleTimeout();
    uint8_t foodCount() const;
    uint8_t foodCount(uint8_t foodIndex) const;
    uint8_t selectedFoodIndex() const;
    uint8_t selectedFoodCount() const;
    uint8_t bowlFoodIndex() const;
    uint8_t bowlFoodCount() const;
    bool bowlHasFood() const { return state.room.bowlCount > 0; }
    uint8_t ballCount() const { return state.bag.pokeBall; }
    uint8_t greatBallCount() const { return state.bag.greatBall; }
    uint8_t heavyBallCount() const { return state.bag.heavyBall; }
    uint8_t timerBallCount() const { return state.bag.timerBall; }
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
    bool depositTeamMemberToStorage(uint8_t slot);
    bool withdrawStorageMemberToTeam(uint8_t slot);
    bool releaseStorageMember(uint8_t slot);
    bool addFood(uint8_t amount = 1);
    bool addFoodStock(uint8_t foodIndex, uint8_t amount = 1);
    bool selectFood(uint8_t foodIndex);
    FoodPlacementResult placeSelectedFoodInBowl();
    bool consumeBowlFood();
    bool addBalls(uint8_t amount);
    bool consumeBall();
    bool addGreatBalls(uint8_t amount);
    bool consumeGreatBall();
    bool addCandy(uint8_t amount);
    bool addPotion(uint8_t amount);
    bool addSuperPotion(uint8_t amount);
    bool usePotion();
    bool useSuperPotion();
    bool addAntidote(uint8_t amount);
    uint8_t itemCount(Game::ItemId item) const;
    bool addItem(Game::ItemId item, uint8_t amount = 1, bool immediate = true);
    bool removeItem(Game::ItemId item, uint8_t amount = 1, bool immediate = true);
    bool spendCoins(uint32_t amount);
    void addCoins(uint32_t amount);
    bool recordCapture(uint16_t speciesId);
    bool recordCapture(const Game::MonsterRuntime& monster);
    bool recordCapture(const Game::MonsterRuntime& monster, uint8_t metArea);
    void grantEffortFrom(const Species& defeatedSpecies);
    void petMonster();
    void finishHatch(uint8_t starterStyle);
    void addExperience(uint32_t amount);
    bool consumePendingLevelUp(uint8_t& level);
    bool hasPendingMoveLearn() const { return pendingMoveLearn; }
    uint8_t pendingMoveLearnId() const { return pendingMoveId; }
    uint8_t pendingMoveLearnSlot() const { return pendingMoveSlot; }
    bool resolvePendingMoveLearn(bool learn);
    uint32_t applyActiveFaintPenalty();
    void addWalkSteps(uint16_t steps);
    void debugRecoverActiveMonster();
    bool debugSetActiveSpecies(uint16_t speciesId);
    uint32_t debugAdvanceToTimeOfDay(uint16_t targetMinutesOfDay);
    uint8_t debugLightSourceIndex() const { return debugLightSource; }
    const char* debugLightSourceLabel() const;
    void cycleDebugLightSource();
    bool debugWalkBoundaryVisible() const { return debugShowWalkBoundary; }
    void toggleDebugWalkBoundary() { debugShowWalkBoundary = !debugShowWalkBoundary; }
    bool debugTiltControlEnabled() const { return debugTiltControl; }
    void toggleDebugTiltControl() { debugTiltControl = !debugTiltControl; }
    void wakeFromIdle();
    void markDirty(bool immediate = false);
    bool saveNow();
    bool resetGame();
    const MainSceneViewState& mainSceneViewState() const { return mainViewState; }
    void saveMainSceneViewState(const MainSceneViewState& value) { mainViewState = value; }
    void clearMainSceneViewState() { mainViewState = MainSceneViewState{}; }
    bool loadHatchProgress(Game::HatchProgress& progress);
    bool saveHatchProgress(const Game::HatchProgress& progress);
    void clearHatchProgress();
    void beginExploreDeparture(uint8_t area);
    void markExploreActive();
    void beginExploreReturn(bool fainted);
    void finishExploreReturn();
    ExploreTravelPhase exploreTravelPhase() const { return exploreTravel; }
    uint8_t pendingExploreArea() const { return exploreArea; }

private:
    GameEngine() = default;

    void switchScene(SceneID id);
    void processInput(uint32_t nowMs);
    void update(uint32_t nowMs);
    void render(uint32_t nowMs);
    void updateSceneFade(uint32_t nowMs);
    void renderSceneFade(uint32_t nowMs);
    bool resourceAlertVisible() const;
    void renderResourceAlert();
    void resetIdle(uint32_t nowMs);
    void updateIdle(uint32_t nowMs);
    uint32_t idleTimeoutMs() const;
    uint32_t gameMinutesTotalAt(uint32_t nowMs) const;
    void syncGameClock(uint32_t nowMs);
    void resetGameClockAnchor(uint32_t nowMs);
    void persistGameClock(uint32_t nowMs, bool force = false);
    void initDefaultState();
    void sanitizeMonsterMoves();
    void tickCare(uint32_t nowMs);
    void resetDailyCountersIfNeeded();
    void grantCareExperience(uint8_t baseAmount, bool weakGain = false);
    bool syncSpriteCache(uint8_t loadBudget = 0xFF);
    uint32_t randomIvPacked() const;
    void queueMoveLearnIfReady(Game::MonsterRuntime& mon, const Species& species, uint8_t oldLevel);

    static constexpr uint32_t INPUT_SAMPLE_MS = 16;
    static constexpr uint32_t FRAME_MS = 66;
    static constexpr uint32_t IDLE_FRAME_MS = 250;

    enum class SceneFadePhase : uint8_t {
        NONE,
        OUT,
        HOLD,
        IN,
    };

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
    uint16_t satietyDecayMinuteAcc = 0;
    bool satietyDecayWasSleeping = false;
    bool idleActive = false;
    bool saveDirty = false;
    bool pendingMoveLearn = false;
    uint8_t pendingMoveSlot = 0;
    uint8_t pendingMoveId = 0;
    bool pendingLevelUp = false;
    uint8_t pendingLevelUpLevel = 0;
    bool debugShowWalkBoundary = false;
    bool debugTiltControl = false;
    uint8_t debugLightSource = 0;
    uint8_t brightnessTraceFrames = 0;
    uint32_t brightnessTraceStartedMs = 0;
    bool startupFirstFrameRendered = false;
    bool startupSpriteCacheReady = false;
    uint32_t bootStartedMs = 0;
    SceneFadePhase sceneFade = SceneFadePhase::NONE;
    SceneID sceneFadeTarget = SceneID::MAIN;
    uint32_t sceneFadeStartedMs = 0;
    uint32_t sceneFadeLastStepMs = 0;
    uint16_t sceneFadeProgressMs = 0;
    uint16_t sceneFadeDurationMs = 300;
    ExploreTravelPhase exploreTravel = ExploreTravelPhase::NONE;
    uint8_t exploreArea = 0;
    MainSceneViewState mainViewState;

    Game::GameState state;
    SaveManager saveManager;
};
