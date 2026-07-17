#pragma once

#include <cstdint>
#include <memory>
#include "core/MainSceneViewState.h"
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

struct FoodConsumeResult {
    bool consumed = false;
    uint8_t foodIndex = 0;
    uint8_t satietyBefore = 0;
    uint8_t satietyAfter = 0;
    uint8_t moodBefore = 0;
    uint8_t moodAfter = 0;
    bool lastBite = false;
    bool becameFull = false;
};

enum class PetOutcome : uint8_t {
    REWARDED,
    DAILY_LIMIT,
    NEEDS_REST,
};

struct PetResult {
    PetOutcome outcome = PetOutcome::DAILY_LIMIT;
    uint8_t moodGain = 0;
    uint8_t affectionGain = 0;
};

enum class ExploreTravelPhase : uint8_t {
    NONE,
    DEPARTING,
    ACTIVE,
    RETURNING,
    RETURNING_FAINTED,
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
    uint8_t bowlFoodBitesRemaining() const { return state.room.bowlBitesRemaining; }
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
    FoodConsumeResult consumeBowlFood();
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
    bool useAntidote();
    uint8_t itemCount(Game::ItemId item) const;
    bool addItem(Game::ItemId item, uint8_t amount = 1, bool immediate = true);
    bool removeItem(Game::ItemId item, uint8_t amount = 1, bool immediate = true);
    bool spendCoins(uint32_t amount);
    void addCoins(uint32_t amount);
    bool recordCapture(uint16_t speciesId);
    bool recordCapture(const Game::MonsterRuntime& monster);
    bool recordCapture(const Game::MonsterRuntime& monster, uint8_t metArea);
    void grantEffortFrom(const Species& defeatedSpecies);
    void grantEffortToTeamMember(uint8_t teamSlot, const Species& defeatedSpecies);
    PetResult petMonster();
    void finishHatch(uint8_t starterStyle);
    void addExperience(uint32_t amount);
    uint32_t addExperienceToTeamMember(uint8_t teamSlot, uint32_t amount);
    bool hasPendingLevelUp() const { return state.pendingLevelUp; }
    uint8_t pendingLevelUpLevel() const { return state.pendingLevelUpLevel; }
    bool acknowledgePendingLevelUp();
    bool hasPendingMoveLearn() const { return state.pendingMoveLearn; }
    Game::MoveId pendingMoveLearnId() const { return state.pendingMoveId; }
    uint8_t pendingMoveLearnSlot() const { return state.pendingMoveSlot; }
    bool resolvePendingMoveLearn(bool learn);
    bool hasPendingMoveReplacement() const { return moveReplacementEventCount > 0; }
    uint8_t pendingMoveReplacementSlot() const;
    Game::MoveId pendingMoveReplacementOldId() const;
    Game::MoveId pendingMoveReplacementNewId() const;
    bool acknowledgePendingMoveReplacement();
    bool hasPendingEvolution() const { return evolutionEventCount > 0; }
    uint8_t pendingEvolutionSlot() const;
    uint16_t pendingEvolutionFromSpeciesId() const;
    uint16_t pendingEvolutionToSpeciesId() const;
    bool acknowledgePendingEvolution();
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
    bool debugEnemyDrawBoundsVisible() const { return debugShowEnemyDrawBounds; }
    void toggleDebugEnemyDrawBounds() {
        debugShowEnemyDrawBounds = !debugShowEnemyDrawBounds;
    }
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
    void beginDebugBattle();
    bool consumeDebugBattleRequest();
    void endDebugBattle();
    bool consumeDebugMenuReturnRequest();

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
    bool sanitizeMonsterMovesForSpecies(Game::MonsterRuntime& mon,
                                        const Species& species);
    void tickCare(uint32_t nowMs);
    void resetDailyCountersIfNeeded();
    void grantCareExperience(uint8_t baseAmount, bool weakGain = false);
    bool syncSpriteCache(uint8_t loadBudget = 0xFF);
    uint32_t randomIvPacked() const;
    bool queueNextPendingMove(Game::MonsterRuntime& mon, const Species& species,
                              uint8_t teamSlot, uint16_t startIndex);
    void queueMoveLearnIfReady(Game::MonsterRuntime& mon, const Species& species,
                               uint8_t oldLevel, uint8_t teamSlot);
    void queueMoveReplacementEvent(uint8_t teamSlot, Game::MoveId oldMoveId,
                                   Game::MoveId newMoveId);
    bool applyLevelUpEvolutions(Game::MonsterRuntime& mon, uint8_t teamSlot,
                                bool notify);
    bool reconcileLevelUpEvolutions();
    void queueEvolutionEvent(uint8_t teamSlot, uint16_t fromSpeciesId,
                             uint16_t toSpeciesId);
    void clearPendingMoveLearn();

    static constexpr uint32_t INPUT_SAMPLE_MS = 16;
    static constexpr uint32_t FRAME_MS = 66;
    static constexpr uint32_t IDLE_FRAME_MS = 250;

    enum class SceneFadePhase : uint8_t {
        NONE,
        OUT,
        HOLD,
        IN,
    };

    struct EvolutionEvent {
        uint8_t teamSlot = 0;
        uint16_t fromSpeciesId = 0;
        uint16_t toSpeciesId = 0;
    };

    struct MoveReplacementEvent {
        uint8_t teamSlot = 0;
        Game::MoveId oldMoveId = 0;
        Game::MoveId newMoveId = 0;
    };

    static constexpr uint8_t EVOLUTION_EVENT_CAP = Game::TEAM_CAP * 2;
    // Current learnsets top out at 25 entries; leave room for a full catch-up
    // level-up on every teammate before the UI drains the notifications.
    static constexpr uint8_t MOVE_REPLACEMENT_EVENT_CAP = Game::TEAM_CAP * 32;

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
    bool debugShowWalkBoundary = false;
    bool debugTiltControl = false;
    bool debugShowEnemyDrawBounds = false;
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
    bool debugBattleRequested = false;
    bool debugMenuReturnRequested = false;
    EvolutionEvent evolutionEvents[EVOLUTION_EVENT_CAP] = {};
    uint8_t evolutionEventHead = 0;
    uint8_t evolutionEventCount = 0;
    MoveReplacementEvent moveReplacementEvents[MOVE_REPLACEMENT_EVENT_CAP] = {};
    uint8_t moveReplacementEventHead = 0;
    uint8_t moveReplacementEventCount = 0;
    MainSceneViewState mainViewState;

    Game::GameState state;
    SaveManager saveManager;
};
