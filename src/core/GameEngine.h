#pragma once

#include <cstdint>
#include <memory>
#include "core/BuildConfig.h"
#include "core/MainSceneViewState.h"
#include "core/GameClockService.h"
#include "core/ResourceService.h"
#include "core/SaveCoordinator.h"
#include "core/SaveManager.h"
#include "core/Scene.h"
#include "game/GameState.h"
#include "game/CareTicker.h"
#include "game/ExploreItemEffects.h"
#include "game/EncounterHistory.h"
#include "game/Species.h"

enum class FoodPlacementResult : uint8_t {
    ADDED,
    NO_STOCK,
    BOWL_FULL,
    DIFFERENT_FOOD,
};

enum class FoodReaction : uint8_t {
    NORMAL,
    LIKED,
    DISLIKED,
};

enum class BathRewardStage : uint8_t {
    SOAP,
    BRUSH,
    RINSE,
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
    FoodReaction reaction = FoodReaction::NORMAL;
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

enum class ContactInviteResult : uint8_t {
    JOINED,
    REFUSED,
    LOCKED,
    TEAM_FULL,
    INVALID,
};

enum class ContactVisitKind : uint8_t {
    NONE,
    PLAY,
    GIFT,
    EXPLORE,
};

enum class DebugContactEventResult : uint8_t {
    STARTED,
    TEAM_NOT_SOLO,
    NO_CONTACT,
    BUSY,
    INVALID,
};

enum class VisitHostResult : uint8_t {
    ACCEPTED = 0,
    STORAGE_FULL = 1,
    NO_MONSTER = 2,
    TEAM_NOT_SOLO = 3,
};

enum class ExploreTravelPhase : uint8_t {
    NONE,
    DEPARTING,
    ACTIVE,
    RETURNING,
    RETURNING_FAINTED,
};

enum class SaveUrgency : uint8_t {
    DEFERRED,
    SOON,
    IMMEDIATE,
};

struct VisitSessionState {
    bool active = false;
    bool asHost = false;
    uint32_t startedMs = 0;
    uint32_t lastPingSentMs = 0;
    uint32_t lastStatusSentMs = 0;
    uint32_t lastPeerMessageMs = 0;
    uint8_t peerMac[6] = {};
};

class GameEngine {
public:
    static GameEngine& ins();

    bool begin();
    void run();
    void requestScene(SceneID id, bool saveBeforeSwitch = true);
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
    uint8_t candyCount() const { return state.bag.candy; }
    uint8_t potionCount() const { return state.bag.potion; }
    uint8_t superPotionCount() const { return state.bag.superPotion; }
    uint8_t antidoteCount() const { return state.bag.antidote; }
    uint8_t paralyzeHealCount() const { return state.bag.paralyzeHeal; }
    uint8_t awakeningCount() const { return state.bag.awakening; }
    uint8_t burnHealCount() const { return state.bag.burnHeal; }
    uint8_t iceHealCount() const { return state.bag.iceHeal; }
    uint8_t maxPotionCount() const { return state.bag.maxPotion; }
    uint8_t fullRestoreCount() const { return state.bag.fullRestore; }
    uint8_t fullHealCount() const { return state.bag.fullHeal; }
    uint8_t fireStoneCount() const { return state.bag.fireStone; }
    uint8_t waterStoneCount() const { return state.bag.waterStone; }
    uint8_t thunderStoneCount() const { return state.bag.thunderStone; }
    uint8_t reviveCount() const { return state.bag.revive; }
    uint8_t maxRepelCount() const { return state.bag.maxRepel; }
    uint8_t honeyCount() const { return state.bag.honey; }
    uint8_t nuggetCount() const { return state.bag.nugget; }
    uint8_t bigPearlCount() const { return state.bag.bigPearl; }
    uint8_t starPieceCount() const { return state.bag.starPiece; }
    uint8_t heartScaleCount() const { return state.bag.heartScale; }
    uint8_t soapCount(uint8_t soapIndex) const {
        return soapIndex < Game::SOAP_VARIANT_COUNT ? state.bag.soap[soapIndex] : 0;
    }
    uint8_t companionCount() const { return state.teamCount + state.storageCount; }
    uint32_t coinCount() const { return state.coins; }
    bool candyPurchaseAvailable();
    bool recordCandyPurchase();
    uint8_t hungerValue() const;
    uint8_t moodValue() const;
    const Game::GameState& gameState() const { return state; }
    Game::GameState& gameState() { return state; }
    bool hasEncounteredSpecies(uint16_t speciesId) const;
    bool recordEncounteredSpecies(uint16_t speciesId);
    const Game::MonsterRuntime& activeMonster() const;
    Game::MonsterRuntime& activeMonster();
    const Species& activeSpecies() const;
    const Species& speciesFor(const Game::MonsterRuntime& monster) const;
    Game::MonsterRuntime createMonster(uint16_t speciesId, uint8_t level) const;
    uint8_t activeSlot() const { return 0; }
    bool switchActiveMonster();
    bool moveTeamMemberToFront(uint8_t slot);
    bool forgetTeamMemberMove(uint8_t teamSlot, uint8_t moveSlot);
    bool moveTeamMemberToContacts(uint8_t slot);
    ContactInviteResult inviteContactToTeam(uint8_t slot);
    bool contactInviteLocked(uint8_t slot) const;
    uint8_t contactInviteChance(uint8_t slot) const;
    bool prepareDailyContactVisit();
    DebugContactEventResult debugTriggerContactVisit(ContactVisitKind kind);
    bool contactKnockPending() const { return contactVisit.pendingKnock; }
    bool localContactVisitActive() const { return contactVisit.active; }
    ContactVisitKind contactVisitKind() const { return contactVisit.kind; }
    uint16_t contactVisitSpeciesId() const;
    bool acceptContactKnock();
    void declineContactKnock();
    void acceptContactExploreInvitation();
    bool contactVisitExploring() const {
        return contactVisit.active && contactVisit.exploring;
    }
    bool exploreBlockedByGuest() const;
    bool contactVisitFarewellPending() const {
        return contactVisit.active && contactVisit.farewellPending;
    }
    bool contactVisitTimedOut(uint32_t nowMs) const;
    void requestContactVisitFarewell();
    bool restoreContactHostToFront();
    void completeContactVisit();
    bool contactIsVisiting(uint8_t storageSlot) const {
        return contactVisit.active &&
               contactVisit.storageSlot == storageSlot;
    }
    bool canDeleteContact(uint8_t slot) const;
    bool deleteContact(uint8_t slot);
    bool addFood(uint8_t amount = 1);
    bool addFoodStock(uint8_t foodIndex, uint8_t amount = 1);
    bool selectFood(uint8_t foodIndex);
    FoodPlacementResult placeSelectedFoodInBowl();
    FoodConsumeResult consumeBowlFood(uint8_t teamSlot = 0);
    bool addCandy(uint8_t amount);
    bool addPotion(uint8_t amount);
    bool addSuperPotion(uint8_t amount);
    bool usePotion(uint8_t teamSlot = 0);
    bool useSuperPotion(uint8_t teamSlot = 0);
    bool addAntidote(uint8_t amount);
    bool useAntidote(uint8_t teamSlot = 0);
    bool addParalyzeHeal(uint8_t amount);
    bool useParalyzeHeal(uint8_t teamSlot = 0);
    bool addAwakening(uint8_t amount);
    bool useAwakening(uint8_t teamSlot = 0);
    bool addBurnHeal(uint8_t amount);
    bool useBurnHeal(uint8_t teamSlot = 0);
    bool addIceHeal(uint8_t amount);
    bool useIceHeal(uint8_t teamSlot = 0);
    bool useMaxPotion(uint8_t teamSlot = 0);
    bool useFullRestore(uint8_t teamSlot = 0);
    bool useFullHeal(uint8_t teamSlot = 0);
    bool useEvolutionStone(uint8_t teamSlot, Game::ItemId stone);
    bool useRevive(uint8_t teamSlot = 0);
    // 黄金喷雾/甜甜蜜：效果为会话状态（不进存档，重启失效），对下一次探索生效。
    bool useMaxRepel();
    bool useHoney();
    uint8_t repelStepsRemaining() const {
        return exploreItemEffects.repelStepsRemaining();
    }
    void tickRepelStep() { exploreItemEffects.completeWalkStep(); }
    bool honeyEncounterPending() const {
        return exploreItemEffects.honeyEncounterPending();
    }
    void clearHoneyEncounter() { exploreItemEffects.consumeHoneyEncounter(); }
    uint8_t itemCount(Game::ItemId item) const;
    bool addItem(Game::ItemId item, uint8_t amount = 1,
                 SaveUrgency urgency = SaveUrgency::SOON);
    bool removeItem(Game::ItemId item, uint8_t amount = 1,
                    SaveUrgency urgency = SaveUrgency::SOON);
    uint8_t collectRecallableMoves(uint8_t teamSlot,
                                   Game::MoveId* outMoves,
                                   uint8_t capacity) const;
    bool recallMove(uint8_t teamSlot, Game::MoveId moveId,
                    uint8_t replacementSlot);
    uint8_t grantBathReward(BathRewardStage stage);
    bool spendCoins(uint32_t amount);
    void addCoins(uint32_t amount);
    bool recordFriendContact(const Game::MonsterRuntime& monster,
                             uint8_t metArea,
                             uint8_t* contactSlot = nullptr);
    void grantEffortFrom(const Species& defeatedSpecies);
    void grantEffortToTeamMember(uint8_t teamSlot, const Species& defeatedSpecies);
    bool rewardPairInteractionMood();
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
    bool pendingMoveLearnNeedsReplacement() const;
    bool resolvePendingMoveLearn(bool learn);
    bool resolvePendingMoveLearnReplacing(uint8_t replacementSlot);
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
    bool cancelPendingEvolution();
    uint32_t applyFaintPenaltyToTeamMember(uint8_t teamSlot);
    uint32_t applyActiveFaintPenalty() {
        return applyFaintPenaltyToTeamMember(0);
    }
    uint8_t grantAdventureBond(uint16_t steps);
    void addWalkSteps(uint16_t steps);
    void debugRecoverTeam();
    bool debugLevelUpActiveMonster();
    bool debugSetActiveSpecies(uint16_t speciesId);
    uint32_t debugAdvanceToTimeOfDay(uint16_t targetMinutesOfDay);
    uint8_t debugLightSourceIndex() const {
        return BuildConfig::DEBUG_FEATURES ? debugLightSource : 0;
    }
    const char* debugLightSourceLabel() const;
    void cycleDebugLightSource();
    bool debugWalkBoundaryVisible() const {
        return BuildConfig::DEBUG_FEATURES && debugShowWalkBoundary;
    }
    void toggleDebugWalkBoundary() {
        if (BuildConfig::DEBUG_FEATURES) debugShowWalkBoundary = !debugShowWalkBoundary;
    }
    bool debugTiltControlEnabled() const {
        return BuildConfig::DEBUG_FEATURES && debugTiltControl;
    }
    void toggleDebugTiltControl() {
        if (BuildConfig::DEBUG_FEATURES) debugTiltControl = !debugTiltControl;
    }
    bool debugBattleDrawBoundsVisible() const {
        return BuildConfig::DEBUG_FEATURES && debugShowBattleDrawBounds;
    }
    void toggleDebugBattleDrawBounds() {
        if (BuildConfig::DEBUG_FEATURES) {
            debugShowBattleDrawBounds = !debugShowBattleDrawBounds;
        }
    }
    void wakeFromIdle();
    bool idleModeActive() const { return idleActive; }
    void invalidateScene();
    void markDirty(SaveUrgency urgency = SaveUrgency::DEFERRED);
    bool saveNow();
    // 主界面长按 B 键进入深度睡眠：睡前强制存档，唤醒为冷启动读档。
    void enterDeepSleep();
    // 深度睡眠定时静默唤醒入口（main.cpp setup 中按唤醒原因调用）：
    // 只跑照护逻辑并写档，随后重新武装定时器再次深睡；不返回。
    void runSilentCareWake();
    bool resetGame();
    const MainSceneViewState& mainSceneViewState() const { return mainViewState; }
    void saveMainSceneViewState(const MainSceneViewState& value) { mainViewState = value; }
    void clearMainSceneViewState() { mainViewState = MainSceneViewState{}; }
    bool loadHatchProgress(Game::HatchProgress& progress);
    bool saveHatchProgress(const Game::HatchProgress& progress);
    void clearHatchProgress();
    bool beginExploreDeparture(uint8_t area);
    void markExploreActive();
    void beginExploreReturn(bool fainted);
    void finishExploreReturn();
    ExploreTravelPhase exploreTravelPhase() const { return exploreTravel; }
    uint8_t pendingExploreArea() const { return exploreArea; }
    void beginDebugBattle();
    bool consumeDebugBattleRequest();
    void endDebugBattle();
    bool consumeDebugMenuReturnRequest();
    bool visitActive() const { return visitSession.active; }
    bool visitAsHost() const { return visitSession.asHost; }
    bool canHostVisit() const { return state.teamCount == 1; }
    VisitHostResult beginVisitAsHost(uint16_t speciesId, uint8_t level,
                                     uint8_t nature, uint8_t satiety,
                                     uint8_t mood, uint8_t affection);
    void beginVisitAsVisitor();
    void endVisit();
    bool takeVisitLinkLost();

private:
    GameEngine() = default;

    void switchScene(SceneID id, bool saveBeforeSwitch = true);
    void processInput(uint32_t nowMs);
    void update(uint32_t nowMs);
    void scheduleSceneUpdate(uint32_t nowMs);
    void logSceneDemand(const SceneUpdateResult& result, uint32_t nowMs);
    void emitRenderStats(uint32_t nowMs);
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
    void markSaveDirty(SaveUrgency urgency);
    void initDefaultState();
    void sanitizeMonsterMoves();
    bool sanitizeMonsterMovesForSpecies(Game::MonsterRuntime& mon,
                                        const Species& species);
    void tickCare(uint32_t nowMs);
    void updateVisit(uint32_t nowMs);
    void resetDailyCountersIfNeeded();
    void grantCareExperience(uint8_t baseAmount, bool weakGain = false,
                             uint8_t teamSlot = 0);
    bool syncSpriteCache(uint8_t loadBudget = 0xFF,
                         bool* cacheChanged = nullptr);
    uint32_t randomIvPacked() const;
    bool queueNextPendingMove(Game::MonsterRuntime& mon, const Species& species,
                              uint8_t teamSlot, uint16_t startIndex);
    void queueMoveLearnIfReady(Game::MonsterRuntime& mon, const Species& species,
                               uint8_t oldLevel, uint8_t teamSlot);
    void queueMoveReplacementEvent(uint8_t teamSlot, Game::MoveId oldMoveId,
                                   Game::MoveId newMoveId);
    bool applyLevelUpEvolutions(Game::MonsterRuntime& mon, uint8_t teamSlot,
                                bool notify, uint8_t oldLevel);
    bool queueEvolutionEvent(uint8_t teamSlot, uint16_t fromSpeciesId,
                             uint16_t toSpeciesId,
                             uint8_t oldLevel,
                             Game::ItemId consumedItem = Game::ItemId::COUNT);
    void removeEvolutionEventsForSlot(uint8_t teamSlot);
    void removeMoveReplacementEventsForSlot(uint8_t teamSlot);
    void clearPendingMoveLearn();
    bool resolvePendingMoveLearnInternal(bool learn,
                                         uint8_t replacementSlot);

    static constexpr uint32_t INPUT_SAMPLE_MS = 16;
    static constexpr uint32_t FRAME_MS = 66;
    static constexpr uint32_t IDLE_FRAME_MS = 250;
    static constexpr uint32_t RENDER_STATS_INTERVAL_MS = 10000;

    enum class SceneFadePhase : uint8_t {
        NONE,
        OUT,
        HOLD,
        IN,
    };

    struct EvolutionEvent {
        uint8_t teamSlot = 0;
        uint8_t oldLevel = 0;
        uint16_t fromSpeciesId = 0;
        uint16_t toSpeciesId = 0;
        Game::ItemId consumedItem = Game::ItemId::COUNT;
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
    uint32_t lastSceneUpdateMs = 0;
    uint32_t nextSceneUpdateMs = 0;
    uint32_t lastCareMs = 0;
    uint32_t lastActivityMs = 0;
    // 跨 tickCare 调用保留的会话型照护累加器（不持久化），见 game/CareTicker.h。
    Game::CareTickAccumulators careAcc = {};
    bool idleActive = false;
    bool debugShowWalkBoundary = false;
    bool debugTiltControl = false;
    bool debugShowBattleDrawBounds = false;
    uint8_t debugLightSource = 0;
    bool startupFirstFrameRendered = false;
    bool startupSpriteCacheReady = false;
    bool mainSceneFirstFrameRendered = false;
    uint32_t nextExplorePoolPreloadMs = 0;
    bool sceneDirty = true;
    bool sceneUpdateScheduled = true;
    bool resourceAlertWasVisible = false;
#if STICKMON_ENABLE_RENDER_STATS
    uint32_t renderStatsStartedMs = 0;
    uint32_t renderStatsCoreUpdates = 0;
    uint32_t renderStatsSceneUpdates = 0;
    uint32_t renderStatsRedrawRequests = 0;
    uint32_t renderStatsFlushes = 0;
    uint32_t renderStatsInputWakes = 0;
    uint32_t renderStatsStateWakes = 0;
    uint32_t renderStatsSceneSwitches = 0;
    uint32_t renderStatsDrawUs = 0;
    uint32_t renderStatsFlushUs = 0;
    uint32_t renderStatsMaxDrawUs = 0;
    uint32_t renderStatsMaxFlushUs = 0;
    uint8_t lastLoggedDemandMode = 0xFF;
    SceneID lastLoggedDemandScene = SceneID::MAIN;
#endif
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
    // 会话级道具效果（黄金喷雾/甜甜蜜），不进存档，重启失效。
    Game::ExploreItemEffects exploreItemEffects;
    EvolutionEvent evolutionEvents[EVOLUTION_EVENT_CAP] = {};
    uint8_t evolutionEventHead = 0;
    uint8_t evolutionEventCount = 0;
    MoveReplacementEvent moveReplacementEvents[MOVE_REPLACEMENT_EVENT_CAP] = {};
    uint8_t moveReplacementEventHead = 0;
    uint8_t moveReplacementEventCount = 0;
    MainSceneViewState mainViewState;
    VisitSessionState visitSession;
    struct ContactVisitSession {
        bool pendingKnock = false;
        bool active = false;
        bool exploring = false;
        bool farewellPending = false;
        uint8_t storageSlot = 0xFF;
        ContactVisitKind kind = ContactVisitKind::NONE;
        uint32_t startedMs = 0;
        uint32_t checkedDay = 0xFFFFFFFFUL;
    } contactVisit;
    bool visitLinkLost = false;

    Game::GameState state;
    Game::EncounterHistory encounterHistory;
    bool encounterHistoryDirty = false;
    SaveManager saveManager;
    SaveCoordinator saveCoordinator;
    GameClockService gameClock;
    ResourceService resourceService;

    bool syncOwnedSpeciesToEncounterHistory();
    uint8_t contactVisitorTeamSlot() const;
};
