#pragma once

#include <algorithm>
#include <cstdint>

#ifndef STICKMON_HAS_CLAW
#define STICKMON_HAS_CLAW 0
#endif

#include "TouchInput.h"
#include "HomeScreen.h"
#if STICKMON_HAS_CLAW
#include "brain/BrainBridge.h"
#endif
#include "core/AppSceneFlow.h"
#include "core/GameClockService.h"
#include "core/MainSceneViewState.h"
#include "core/SaveManager.h"
#include "core/VisitSessionService.h"
#include "game/CareTicker.h"
#include "game/BathService.h"
#include "game/BattleSystem.h"
#include "game/BattleTurnController.h"
#include "game/EncounterHistory.h"
#include "game/ExploreItemEffects.h"
#include "game/ExploreBoss.h"
#include "game/ExploreBossPity.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"
#include "game/ExploreMapGenerator.h"
#include "game/ExplorePool.h"
#include "game/MonsterMind.h"
#include "game/MoveManagementService.h"
#include "game/ShopService.h"

class Canvas565;

namespace AmoledV1 {

class AmoledApp {
public:
    void begin(uint32_t nowMs);
    void handleTouch(const TouchEvent& event);
    void update(uint32_t nowMs);
    void render(Canvas565& canvas) const;

    bool needsRender() const { return dirtyRowsValid; }
    uint16_t renderRowBegin() const { return dirtyRowBegin; }
    uint16_t renderRowEnd() const { return dirtyRowEnd; }
    void markRendered();
    void forceFullRender() { requestFullRender(); }
    void forceRenderRows(uint16_t begin, uint16_t end) {
        begin = std::min<uint16_t>(begin, 224);
        end = std::min<uint16_t>(std::max<uint16_t>(end, begin), 224);
        dirtyRowBegin = begin;
        dirtyRowEnd = end;
        dirtyRowsValid = begin < end;
    }
    bool consumeLockRequest();
    void onWake(uint32_t nowMs);
    bool lockFocusPoint(int16_t& x, int16_t& y) const;

#if STICKMON_HAS_CLAW
    bool brainSnapshot(Stickmon::BrainBridge::Snapshot& out) const;
    bool brainStartExpedition(uint8_t area);
    bool brainReturnHome();
    bool brainInviteFriend();
    bool brainEat();
    bool brainBuyFood(uint8_t foodIndex);
    bool brainSay(const char* text);
#endif

private:
    enum class PetMotion : uint8_t {
        IDLE,
        TURNING,
        WANDERING,
        SEEKING_FOOD,
        STOPPING,
        EATING,
    };

    enum class PendingItemAction : uint8_t {
        NONE = 0,
        BUY,
        SELL,
        USE,
    };

    enum class ExpeditionDeparturePhase : uint8_t {
        NONE = 0,
        WALK_TO_DOOR,
        CROSS_DOOR,
    };

    void handleTap(int x, int y, uint32_t nowMs);
    void setToast(const char* value, uint32_t nowMs,
                  uint32_t durationMs = 1100);
    void clampMenuScroll();
#if STICKMON_ENABLE_DEBUG_FEATURES
    void clampDebugScroll();
    void handleDebugTap(int x, int y, uint32_t nowMs);
    void handleDebugPopupTap(int x, int y, uint32_t nowMs);
    void executeDebugAction(uint32_t nowMs);
    void acceptDebugContact(uint32_t nowMs);
    void completeDebugContact(uint32_t nowMs);
    void startDebugPairChase(uint32_t nowMs);
    void updateDebugPairChase(uint32_t nowMs, float elapsedSeconds);
    void stopDebugPairChase(uint32_t nowMs, bool reward);
    void openDebugSwitchPopup();
    void openDebugTimePopup();
#endif
    void clampItemScroll();
    uint8_t shopDailyItemCount() const;
    uint8_t shopExploreItemCount() const;
    uint8_t currentItemCount() const;
    Game::ItemId currentItemAt(uint8_t index) const;
    void openItemScene(AppSceneFlow::Scene target);
    void closeItemScene();
    void performPendingItemAction(uint32_t nowMs);
    void openRoomScene();
    void openRoomFoodScene();
    void openComputerScene();
    void openSettingsScene();
    void closeUtilityScene();
    void clampComputerScroll();
    void clampClawLogScroll();
#if STICKMON_HAS_CLAW
    // Copies the shared ClawStatusLog into the render snapshot buffer and
    // reclamps the scroll offset. Called from update/tap paths only.
    void refreshClawLogSnapshot();
#endif
    void setSettingsSliderValue(uint8_t item, int x, uint32_t nowMs);
    void placeExploreRoutePickup();
    void resolveExploreRoutePickup(uint32_t nowMs);
    void openShowerScene(uint32_t nowMs);
    void closeShowerScene();
    void startShowerSoap(uint8_t soapIndex, uint32_t nowMs);
    void startShowerTool(ShowerMode mode, uint32_t nowMs);
    void updateShowerToolDrag(int x, int y, uint32_t nowMs);
    void grantShowerStage(Game::BathService::Stage stage, uint32_t nowMs);
    void startShowerRinse(uint32_t nowMs);
    void requestShowerExit();
    void resetShowerSession(uint32_t nowMs);
    void openProgressionScene(AppSceneFlow::Scene returnScene,
                              uint8_t teamSlot, uint8_t oldLevel,
                              uint32_t nowMs);
    void openEvolutionProgression(AppSceneFlow::Scene returnScene,
                                  uint8_t teamSlot, uint16_t fromSpeciesId,
                                  uint16_t toSpeciesId, uint32_t nowMs);
    void advanceProgression(uint32_t nowMs);
    bool findNextProgressionMove(uint8_t teamSlot, uint8_t oldLevel,
                                 uint16_t& cursor, Game::MoveId& moveId) const;
    void completeProgression(uint32_t nowMs);
    void openTeamScene();
    void openTeamMoves(uint8_t teamSlot, uint32_t nowMs);
    void refreshTeamMoveRecallable();
    void switchTeamLeader(uint32_t nowMs);
    bool queueExploreDeparture(uint8_t area, bool autoWalk);
    bool updateExploreDeparture(uint32_t nowMs);
    void cancelExploreDeparture();
    bool startExploreRoute(uint32_t nowMs);
    bool beginExploreRouteStep(uint32_t nowMs);
    void updateExploreRoute(uint32_t nowMs);
    bool finishExploreRouteAtEnd(uint32_t nowMs);
    void resolveExploreStepEvent(uint32_t nowMs);
    bool beginExploreEncounter(
        uint32_t nowMs, bool boss = false, uint16_t speciesOverride = 0,
        uint8_t levelOverride = 0, uint16_t experiencePercent = 100,
        ExploreSpecial::Kind specialKind = ExploreSpecial::Kind::NONE);
    void grantExplorePickup(uint32_t nowMs);
    void performBattleAttack(uint32_t nowMs);
    void performBattlePlayerAction(
        const BattleTurnController::Action& action, uint32_t nowMs);
    void performBattleSwitch(uint8_t teamSlot, bool consumesTurn,
                             uint32_t nowMs);
    void performBattleWildTurn(uint32_t nowMs);
    void performBattleWildAction(
        const BattleTurnController::Action& action, uint32_t nowMs);
    void performBattlePlannedAction(uint32_t nowMs);
    void advanceBattleTurn(uint32_t nowMs);
    bool resolveBattleFaint(uint32_t nowMs);
    void performBattleBag(uint32_t nowMs);
    void performBattleBagItem(Game::ItemId item, uint32_t nowMs);
    void performBattleFlee(uint32_t nowMs);
    void pushBattleLog(uint32_t nowMs);
    void clearBattleLog();
    void finishBattleVictory(uint32_t nowMs);
    void resolveBattleFriendship(uint8_t choice, uint32_t nowMs);
    void finishBattleAfterFriendship(uint32_t nowMs);
    void finishBattleDefeat(uint32_t nowMs);
    void closeBattle(uint32_t nowMs);
    void updateExploreRouteCamera();
    void loadExplorePreview(uint32_t nowMs);
    void updateExplorePreviewLoading(uint32_t nowMs);
    void refreshExplorePreviewFrames();
    void clearExplorePreview();
    void selectExploreArea(uint8_t area, uint32_t nowMs);
    bool hasEncounteredSpecies(uint16_t speciesId) const;
    bool recordEncounteredSpecies(uint16_t speciesId);
    bool syncOwnedSpeciesToEncounterHistory();
    void pauseExploreRoute(uint32_t nowMs);
    void resumeExploreRoute(uint32_t nowMs);
    void settleExploreReturn();
    void leaveExploreRoute();
    void updateClockAndCare(uint32_t nowMs);
    void updateMoodHearts(uint32_t nowMs);
    void updatePet(uint32_t nowMs);
    bool beginPetMove(PetMotion motion, float x, float y,
                      uint32_t nowMs);
    void finishPetMove(uint32_t nowMs);
    void schedulePetDecision(uint32_t nowMs);
    void updatePetFootprint();
    bool petFootprintInsideWalkArea(float x, float y) const;
    bool petPathInsideWalkArea(float fromX, float fromY,
                               float toX, float toY) const;
    bool chooseWanderTarget(float& x, float& y,
                            bool requirePath = true) const;
    bool chooseFoodApproachTarget(float desiredX, float desiredY,
                                  float& x, float& y) const;
    void updateCamera();
    int worldToScreenX(float worldX) const;
    int worldToScreenY(float worldY) const;
    void requestRenderRows(uint16_t begin, uint16_t end);
    void requestFullRender();
    float gameSpeed() const;
    bool saveState();

    AppSceneFlow::Controller sceneFlow;
    bool dirtyRowsValid = true;
    uint16_t dirtyRowBegin = 0;
    uint16_t dirtyRowEnd = 224;
    bool lockRequested = false;

    bool pointerDown = false;
    bool dragging = false;
    int16_t touchStartX = 0;
    int16_t touchStartY = 0;
    int16_t touchLastY = 0;

    float menuScroll = 0.0f;
    float menuVelocity = 0.0f;
    int pressedMenuItem = -1;
#if STICKMON_ENABLE_DEBUG_FEATURES
    DebugViewModel::Category debugCategory = DebugViewModel::Category::ROOT;
    uint8_t debugCursor = 0;
    float debugScroll = 0.0f;
    float debugVelocity = 0.0f;
    int debugPressedItem = -1;
    DebugViewModel::Popup debugPopup = DebugViewModel::Popup::NONE;
    uint8_t debugFocus = 0;
    uint8_t debugDigits[4] = {};
    bool debugBattleActive = false;
    bool debugBattleRequested = false;
    bool debugTiltControl = false;
    bool debugWalkBoundaryVisible = false;
    bool debugBattleDrawBoundsVisible = false;
    uint8_t debugLightSource = 0;
    char debugToastBuffer[48] = {};
    bool debugContactPending = false;
    bool debugContactActive = false;
    uint8_t debugContactKind = 0;
    uint8_t debugContactStorageSlot = 0xFF;
    uint32_t debugContactStartedMs = 0;
    bool debugPairChaseActive = false;
    float debugPairX = 92.0f;
    float debugPairY = 151.0f;
    uint8_t debugPairFrame = 0;
    PokemonSprites::WalkDirection debugPairDirection =
        PokemonSprites::WalkDirection::DOWN;
    uint32_t debugPairChaseUntilMs = 0;
    uint32_t debugPairNextFrameMs = 0;
#endif
    int pressedExploreArea = -1;
    int exploreDragStartArea = -1;
    float exploreAreaAnimCursor = 0.0f;
    uint32_t explorePreviewStartedAt = 0;
    uint32_t explorePreviewNextLoadAt = 0;
    ExplorePool::Pool explorePreviewPool{};
    uint16_t explorePreviewSpeciesIds[ExplorePool::POOL_CAP] = {};
    static constexpr uint8_t EXPLORE_PRELOAD_CAP =
        ExplorePool::POOL_CAP * Game::EXPLORE_AREA_COUNT;
    uint16_t explorePreloadSpeciesIds[EXPLORE_PRELOAD_CAP] = {};
    uint8_t explorePreloadSpeciesCount = 0;
    const PokemonSprites::SpriteFrame*
        explorePreviewFrames[ExplorePool::POOL_CAP] = {};
    bool explorePreviewHidden[ExplorePool::POOL_CAP] = {};
    bool explorePreviewLoadPending = false;
    uint32_t explorePreviewVisualCycle = UINT32_MAX;
    uint8_t exploreMenuCursor = 0;
    int pressedExploreMenuItem = -1;
    float itemScroll = 0.0f;
    float itemVelocity = 0.0f;
    int pressedItemRow = -1;
    int pressedShopCategory = -1;
    int pressedShopDetailAction = -1;
    int pressedTeamSlot = -1;
    bool teamMovesOpen = false;
    uint8_t teamMovesSlot = 0;
    TeamMovesViewModel::Mode teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
    Game::MoveId teamMovesRecallIds[
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT] = {};
    uint8_t teamMovesRecallCount = 0;
    uint8_t teamMovesRecallSelected = 0xFF;
    uint8_t teamMovesForgetSlot = 0;
    bool teamMovesForgetConfirm = false;
    int pressedRoomItem = -1;
    int pressedCommunicationItem = -1;
    int pressedShowerItem = -1;
    bool teamConfirmOpen = false;
    uint8_t pendingTeamSlot = 0;
    Game::ShopService::Category shopCategory =
        Game::ShopService::Category::DAILY;
    float shopDetailProgress = 0.0f;
    int shopDetailItemIndex = -1;
    bool itemConfirmOpen = false;
    Game::ItemId pendingItem = Game::ItemId::COUNT;
    PendingItemAction pendingItemAction = PendingItemAction::NONE;
    uint8_t selectedExploreArea = 0;
    ComputerViewModel::Page computerPage = ComputerViewModel::Page::MENU;
    float computerScroll = 0.0f;
    float computerVelocity = 0.0f;
    uint8_t computerPressedItem = 0xFF;
    // CLAW_SETUP log view state. Entries themselves are copied into a
    // file-scope buffer at render time (this object lives on the app_main
    // stack, so a 64-entry array must not be a member).
    bool clawLogView = false;
    float clawLogScroll = 0.0f;
    float clawLogVelocity = 0.0f;
    bool clawLogPinned = true;
    uint32_t clawLogGen = 0;
    size_t clawLogCount = 0;
    uint8_t settingsPressedItem = 0xFF;
    bool settingsSliderDragging = false;
    bool settingsSliderChanged = false;
    AppSceneFlow::Scene progressionReturnScene = AppSceneFlow::Scene::HOME;
    ProgressionViewModel::Mode progressionMode =
        ProgressionViewModel::Mode::LEVEL_UP;
    uint8_t progressionTeamSlot = 0;
    uint8_t progressionOldLevel = 1;
    uint8_t progressionLevel = 1;
    uint16_t progressionFromSpeciesId = 0;
    uint16_t progressionToSpeciesId = 0;
    Game::MoveId progressionMoveId = 0;
    Game::MoveId progressionOldMove2 = 0;
    Game::MoveId progressionOldMove3 = 0;
    uint16_t progressionMoveCursor = 0;
    uint8_t progressionPressedItem = 0xFF;

    ShowerMode showerMode = ShowerMode::MENU;
    uint8_t showerSoapIndex = 0;
    uint8_t showerSoapProgress = 0;
    uint8_t showerBrushProgress = 0;
    uint8_t showerRinseProgress = 0;
    uint8_t showerCompletionHearts = 0;
    int16_t showerToolX = 24;
    int16_t showerToolY = 196;
    int16_t showerLastStrokeX = 24;
    int16_t showerLastStrokeY = 196;
    float showerStrokeCarry = 0.0f;
    bool showerToolDragging = false;
    bool showerSoapConsumed = false;
    bool showerSoapRewarded = false;
    bool showerBrushRewarded = false;
    bool showerRinseRewarded = false;
    bool showerExitConfirmYes = false;
    uint32_t showerModeStartedMs = 0;
    uint32_t showerLastFrameMs = 0;
    char showerToast[64] = {};

    ExploreMapGenerator::Map exploreRouteMap;
    uint8_t exploreRoutePath = 0;
    uint8_t exploreRouteIndex = 0;
    uint8_t exploreRouteDirection = 0;
    uint8_t exploreRoutePetFrame = 0;
    uint16_t exploreRouteSteps = 0;
    float exploreRouteWorldX = 0.0f;
    float exploreRouteWorldY = 0.0f;
    float exploreRouteFromX = 0.0f;
    float exploreRouteFromY = 0.0f;
    float exploreRouteTargetX = 0.0f;
    float exploreRouteTargetY = 0.0f;
    int16_t exploreRouteCameraX = 0;
    int16_t exploreRouteCameraY = 0;
    uint32_t exploreRouteMoveStartedMs = 0;
    uint32_t exploreRoutePausedAtMs = 0;
    uint32_t nextExploreRouteFrameMs = 0;
    bool exploreRouteMoving = false;
    bool exploreRouteAutoWalk = false;
    bool exploreRoutePaused = false;
    bool exploreRouteComplete = false;
    bool exploreRouteExitConfirm = false;
    ExploreRouteViewModel::Prompt exploreRoutePrompt =
        ExploreRouteViewModel::Prompt::NONE;
    bool exploreRouteIceSliding = false;
    int8_t exploreRouteIceDx = 0;
    int8_t exploreRouteIceDy = 0;
    uint8_t exploreRoutePickupIndex = 0;
    uint8_t exploreRoutePickupItem = 0;
    bool exploreRoutePickupAvailable = false;
    bool exploreRouteBossPending = false;
    bool exploreRoutePityEligible = false;
    uint16_t exploreRouteBossSpeciesId = 0;
    uint8_t exploreRouteBossLevel = 0;
    uint16_t exploreRouteBossExperiencePercent = 100;
    ExploreSpecial::Kind exploreRouteSpecialKind = ExploreSpecial::Kind::NONE;
    Game::ExploreItemEffects exploreItemEffects;

    Game::MonsterRuntime battleWild;
    uint8_t battlePlayerSlot = 0;
    BattleSystem::BattleActorState battlePlayerState;
    BattleSystem::BattleActorState battleWildState;
    BattleTurnController battleTurnController;
    BattleTurnController::TurnPlan battleTurnPlan;
    uint8_t battleTurnActionIndex = 0;
    bool battleTurnDamaged[2] = {};
    BattleViewModel::Phase battlePhase = BattleViewModel::Phase::ACTION;
    uint8_t battlePressedItem = 0xFF;
    Game::ItemId battleBagItems[4] = {};
    uint8_t battleBagCount = 0;
    uint16_t battleRewardExp = 0;
    uint32_t battleRewardCoins = 0;
    bool battleIsBoss = false;
    uint16_t battleExperiencePercent = 100;
    ExploreSpecial::Kind battleSpecialKind = ExploreSpecial::Kind::NONE;
    BattleViewModel::FriendshipPrompt battleFriendshipPrompt =
        BattleViewModel::FriendshipPrompt::OFFER;
    uint8_t battleFriendshipContactSlot = 0xFF;
    uint8_t battleVictoryOldLevel = 1;
    bool battleVictoryLeveledUp = false;
    bool battleAnimationActive = false;
    bool battleAnimationAttackerWild = false;
    bool battleAnimationHit = false;
    uint16_t battleAnimationDamage = 0;
    uint32_t battleAnimationStartedMs = 0;
    uint32_t battleAnimationDurationMs = 240;
    uint8_t battleAnimationFrame = 0;
    bool battleAudioPending = false;
    bool battleAudioReady = false;
    uint8_t battlePendingSfx = 0xFF;
    uint16_t battlePendingCrySpecies = 0;
    char battleMessage[64] = {};
    char battleLogLines[2][64] = {};
    uint8_t battleLogCount = 0;
    uint32_t battleLogUntil = 0;

    SaveManager saveManager;
    GameClockService gameClock;
    Game::GameState gameState;
    Game::EncounterHistory encounterHistory;
    MainSceneViewState mainViewState;
    Game::CareTickAccumulators careAcc = {};
    MonsterMind monsterMind;
    MonsterBehaviorProfile behaviorProfile;
    bool storageReady = false;
    uint32_t lastCareMs = 0;
    uint32_t lastPersistMs = 0;
    uint32_t lastInteractionMs = 0;
    bool encounterHistoryDirty = false;
    Communication::VisitSessionService visitSession;

    PetMotion petMotion = PetMotion::IDLE;
    float petX = 92.0f;
    float petY = 151.0f;
    float petTargetX = 92.0f;
    float petTargetY = 151.0f;
    float petFootprintRadiusX = 9.0f;
    float petFootprintRadiusY = 4.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    uint32_t lastPetUpdateMs = 0;
    uint32_t nextMindUpdateMs = 0;
    uint32_t nextPetDecisionMs = 0;
    uint32_t nextPetFrameMs = 0;
    uint32_t nextFeedBiteMs = 0;
    uint32_t feedingUntilMs = 0;
    uint8_t petFrame = 0;
    PokemonSprites::WalkDirection petDirection =
        PokemonSprites::WalkDirection::DOWN;
    PetMotion petStopMotion = PetMotion::IDLE;
    bool petResting = false;
    bool petStoppingToEat = false;
    bool petLongMove = true;
    bool autonomousExpedition = false;
    ExpeditionDeparturePhase expeditionDeparturePhase =
        ExpeditionDeparturePhase::NONE;
    bool pendingExpedition = false;
    bool pendingExpeditionAutoWalk = false;
    uint8_t pendingExpeditionArea = 0;
    uint32_t expeditionDepartureStartedMs = 0;
    float expeditionDoorInsideX = 0.0f;
    float expeditionDoorInsideY = 0.0f;
    float expeditionDoorOutsideX = 0.0f;
    float expeditionDoorOutsideY = 0.0f;
    uint32_t petTurnUntilMs = 0;
    uint32_t nextPetDebugLogMs = 0;

    uint32_t heartsUntil = 0;
    uint8_t moodHeartCount = 0;
    uint8_t moodBurstHeart = 0xFF;
    uint32_t moodBurstStartedMs = 0;
    uint32_t moodBurstUntilMs = 0;
    uint32_t nextMoodBurstFrameMs = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    char brainMessage[161] = {};
    char clawSetupMessage[96] = {};
};

}  // namespace AmoledV1
