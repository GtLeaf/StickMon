#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>

#ifndef STICKMON_HAS_CLAW
#define STICKMON_HAS_CLAW 0
#endif

#include "TouchEvent.h"
#include "app/SettingsController.h"
#include "app/HatchController.h"
#include "app/TeamController.h"
#include "app/InventoryController.h"
#include "app/ShowerController.h"
#include "app/ProgressionFlow.h"
#include "app/BattleSession.h"
#include "app/ExploreSession.h"
#include "app/HomeFlow.h"
#include "app/PairFlow.h"
#include "app/VisitorFlow.h"
#include "app/VisitAwayFlow.h"
#include "app/ExplorePresenter.h"
#include "SleepBreathClock.h"
#include "AmoledGeometry.h"
#include "HomeScreen.h"
#include "ui/RenderCaches.h"
#include "ui/SceneFade.h"
#if STICKMON_HAS_CLAW
#include "brain/BrainBridge.h"
#endif
#include "core/AppSceneFlow.h"
#include "core/GameClockService.h"
#include "core/MainSceneViewState.h"
#include "core/RoomResource.h"
#include "core/SaveManager.h"
#include "core/SaveCoordinator.h"
#include "core/VisitSessionService.h"
#include "game/CareTicker.h"
#include "game/BathService.h"
#include "game/BattleSystem.h"
#include "game/BattleTurnController.h"
#include "game/EncounterHistory.h"
#include "game/ExploreGrass.h"
#include "game/ExploreItemEffects.h"
#include "game/ExploreBoss.h"
#include "game/ExploreBossPity.h"
#include "game/ExploreSpecialEncounter.h"
#include "game/GameState.h"
#include "game/ExploreMapGenerator.h"
#include "game/ExplorePool.h"
#include "game/HomeActorController.h"
#include "game/HomeSimulation.h"
#include "game/HomeVisitArrival.h"
#include "game/HomeVisitDeparture.h"
#include "game/MonsterMind.h"
#include "game/MoveManagementService.h"
#include "game/ShopService.h"

class Canvas565;

namespace AmoledV1 {

class AmoledApp {
public:
    void begin(uint32_t nowMs);
    void handleTouch(const TouchEvent& event);
    void cancelTouch();
    void update(uint32_t nowMs);
    void render(Canvas565& canvas) const;

    bool startupStateLoaded() const { return startupStateLoaded_; }
    // Flush through the same validated save path used by the device lifecycle.
    bool flushSave() { return saveState(); }
    bool saveWriteBlocked() const { return saveWritesBlocked; }
    bool needsRender() const { return dirtyRowsValid; }
    uint16_t renderRowBegin() const { return dirtyRowBegin; }
    uint16_t renderRowEnd() const { return dirtyRowEnd; }
    uint16_t renderColumnBegin() const { return dirtyColumnBegin; }
    uint16_t renderColumnEnd() const { return dirtyColumnEnd; }
    void markRendered();
    using SystemUpdateHandler = bool (*)();
    void setSystemUpdateHandler(SystemUpdateHandler handler) { systemUpdateHandler = handler; }
    void setExternalSceneFade(bool enabled) { externalSceneFade = enabled; }
    bool sceneFadeActive() const { return expeditionFade.active(); }
    bool sceneFadeInward() const { return expeditionFade.inward(); }
    uint8_t sceneFadeAlpha() const { return expeditionFade.alpha(); }
    bool displayLockAllowed() const;
    bool lockedHomePowerAllowed() const;
    void setDisplayLocked(bool locked) { displayLocked = locked; }
    void forceRenderRegion(int left, int top, int right, int bottom) {
        dirtyRowsValid = false;
        requestRenderRect(left, top, right, bottom);
    }
    void forceFullRender() { requestFullRender(); }
    void forceRenderRows(uint16_t begin, uint16_t end) {
        begin = std::min<uint16_t>(begin, AmoledUi::HEIGHT);
        end = std::min<uint16_t>(std::max<uint16_t>(end, begin),
                                 AmoledUi::HEIGHT);
        dirtyColumnBegin = 0;
        dirtyColumnEnd = AmoledUi::WIDTH;
        dirtyRowBegin = begin;
        dirtyRowEnd = end;
        dirtyRowsValid = begin < end;
    }
    bool consumeLockRequest();
    bool prepareDisplayLock(uint32_t nowMs);
    void onWake(uint32_t nowMs);
    bool lockFocusPoint(int16_t& x, int16_t& y) const;
    bool lockTapHitsPet(int16_t x, int16_t y) const;
    bool petIsSleeping() const;
    uint8_t petSleepFrame() const { return petFrame; }
    float petSleepExpansion(uint32_t nowMs) const { return sleepBreath.expansion(nowMs); }
#if STICKMON_ENABLE_DEBUG_FEATURES
    void renderDebugTouchOverlay(Canvas565& canvas) const;
    bool debugStartExplore(uint8_t area);
    void debugReportSaveState();
    void debugReportHomeState() const;
    bool debugBattleScenario(uint8_t scenario, uint32_t nowMs);
    bool debugStartShowerDrag(uint8_t tool, uint32_t nowMs);
    bool showerDraggingForDiagnostics() const {
        return sceneFlow.current() == AppSceneFlow::Scene::SHOWER && showerController_.state().showerToolDragging;
    }
    uint8_t showerToolForDiagnostics() const {
        return showerController_.state().showerMode == ShowerMode::BRUSHING ? 1 : 0;
    }
    bool debugTriggerContact(uint8_t kind);
    bool debugPromptContact(uint8_t kind);
    bool debugAcceptContact();
    bool debugStartPairTalk();
    bool debugReturnContactExplore();
    bool exploreRouteMovingForDiagnostics() const {
        return sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE &&
               exploreSession_.state().exploreRouteMoving && !exploreSession_.state().exploreRoutePaused;
    }
    bool encounterFirstFramePendingForDiagnostics() const {
        return encounterFirstFramePending;
    }
    void markEncounterFirstFramePresented(uint32_t drawUs, uint32_t lcdUs);
#endif

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
    void updateBattery(uint32_t nowMs);
    void fillHomePetModel(HomeViewModel& model) const;
    friend struct AmoledAppTestAccess;
    friend struct AmoledPageTestAccess;
    friend struct AmoledBatteryTestAccess;
    friend struct AmoledEcologyTestAccess;
    friend struct AmoledShowerTestAccess;
    friend struct AmoledCareTestAccess;
    friend struct AmoledPowerTestAccess;
    friend struct AmoledRoomPreviewTestAccess;
    enum class MusicContext : uint8_t {
        HOME = 0,
        EXPLORE,
        BATTLE,
        BATTLE_SPECIAL,
    };

    void setMusicContext(MusicContext context);
    void startVisitHost();
    void startVisitSearch();
    void releaseVisitRadioSession();
    mutable RenderCaches renderCaches_;
#if STICKMON_ENABLE_DEBUG_FEATURES
    uint32_t encounterStartedMs = 0;
    uint32_t encounterPreparedMs = 0;
    uint32_t encounterSpritePrepMs = 0;
    uint32_t encounterSfxPrepMs = 0;
    bool encounterFirstFramePending = false;
    uint32_t diagnosticSaveCount = 0;
#endif
    enum class PetMotion : uint8_t {
        IDLE,
        TURNING,
        WANDERING,
        SEEKING_FOOD,
        SEEKING_SLEEP,
        SLEEPING,
        STOPPING,
        EATING,
    };

    using RoomAction = HomeFlow::Action;

    using PairPhase = PairFlow::Phase;

    using VisitorMotion = VisitorFlow::Motion;

    using PendingItemAction = InventoryController::PendingItemAction;

    enum class ExpeditionDeparturePhase : uint8_t {
        NONE = 0,
        WALK_TO_DOOR,
        CROSS_DOOR,
        WALK_COMPANION_TO_DOOR,
        CROSS_COMPANION_DOOR,
        FADE_OUT,
        FADE_IN,
        RETURN_FADE_OUT,
        RETURN_WALK_IN,
        RETURN_CLEAR_MAIN,
        RETURN_COMPANION_WALK_IN,
        RETURN_FADE_IN,
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
    bool beginDebugContactEvent(uint8_t kind, uint32_t nowMs);
    void acceptDebugContact(uint32_t nowMs, bool autoResolve = false);
    void showDebugContactEventPrompt(uint32_t nowMs);
    void showDebugContactStripMessage(const char* text, uint32_t nowMs,
                                      uint32_t durationMs);
    void completeDebugContact(uint32_t nowMs);
    void finalizeDebugContact(uint32_t nowMs);
    void startDebugPairChase(uint32_t nowMs);
    void updateDebugPairChase(uint32_t nowMs, float elapsedSeconds);
    void stopDebugPairChase(uint32_t nowMs, bool reward);
    void openDebugSwitchPopup();
    void openDebugTimePopup();
    void persistDebugMotionFlags();
    void handleDebugTouchTest(const TouchEvent& event);
#endif
    float itemMaxScroll() const;
    void clampItemScroll();
    uint8_t shopDailyItemCount() const;
    uint8_t shopExploreItemCount() const;
    uint8_t currentItemCount() const;
    void openItemScene(AppSceneFlow::Scene target);
    void closeItemScene();
    void performPendingItemAction(uint32_t nowMs);
    void openRoomScene();
    void openRoomFoodScene();
    void openComputerScene();
    const Game::MonsterRuntime* contactMonster(uint8_t slot) const;
    bool contactIsInTeam(uint8_t slot) const;
    bool contactIsVisiting(uint8_t slot) const;
    bool contactInviteLocked(uint8_t slot) const;
    bool contactCanDelete(uint8_t slot) const;
    uint8_t contactActionCount(uint8_t slot) const;
    void openContactAction(uint8_t slot);
    void openContactStatus(uint8_t slot);
    void handleContactActionTap(int x, int y, uint32_t nowMs);
    void handleContactConfirmTap(int x, int y, uint32_t nowMs);
    void closeContactOverlay();
    void openSettingsScene();
    void closeUtilityScene();
    void clampComputerScroll();
    void clampClawLogScroll();
    void clampTeamMovesScroll();
#if STICKMON_HAS_CLAW
    // Copies the shared ClawStatusLog into the render snapshot buffer and
    // reclamps the scroll offset. Called from update/tap paths only.
    void refreshClawLogSnapshot();
#endif
    void applySettingsChange(SettingsController::Change change);
    bool resetGame();
    void updateHatch(uint32_t nowMs);
    void syncHatchImu(uint32_t nowMs);
    void interactWithHatch(bool shake, uint32_t nowMs);
    bool persistHatchProgress(bool force);
    void completeHatch(uint32_t nowMs);
    void handleHatchTouch(const TouchEvent& event);
    void openShowerScene(uint32_t nowMs);
    void closeShowerScene();
    void startShowerSoap(uint8_t soapIndex, uint32_t nowMs);
    void startShowerTool(ShowerMode mode, uint32_t nowMs);
    void updateShowerToolDrag(int x, int y, uint32_t nowMs);
    void rubShowerFoamAt(int x, int y, uint32_t nowMs);
    bool tryMergeShowerFoam(ShowerFoamState& source, uint32_t nowMs);
    bool anyShowerFoam() const;
    void checkShowerAtmosphereThreshold(uint32_t nowMs);
    Game::BathInteraction::Layout showerBathLayout() const;
    void updateShower(uint32_t nowMs);
    uint32_t applyHomeCareExperience(uint8_t slot, uint32_t amount);
    bool startPendingCareProgression(uint32_t nowMs);
    void flushShowerSave(uint32_t nowMs);
    void applyShowerEffects(uint32_t nowMs);
    void grantShowerStage(Game::BathService::Stage stage, uint32_t nowMs);
    void startShowerRinse(uint32_t nowMs);
    void resetShowerSession(uint32_t nowMs);
    void openProgressionScene(AppSceneFlow::Scene returnScene,
                              uint8_t teamSlot, uint8_t oldLevel,
                              uint32_t nowMs, bool fromCare = false);
    void openEvolutionProgression(AppSceneFlow::Scene returnScene,
                                  uint8_t teamSlot, uint16_t fromSpeciesId,
                                  uint16_t toSpeciesId, uint32_t nowMs,
                                  Game::ItemId consumedItem = Game::ItemId::COUNT);
    void advanceProgression(uint32_t nowMs);
    void applyProgressionEffects(uint32_t nowMs);
    void openTeamScene(bool preserveSubSceneReturn = false);
    void finishTeamItemUse(uint32_t nowMs);
    uint16_t teamItemHpForRender(uint32_t nowMs) const;
    void openTeamMoves(uint8_t teamSlot, uint32_t nowMs);
    void switchTeamLeader(uint32_t nowMs);
    void leaveTeamMember(uint8_t teamSlot, uint32_t nowMs);
    bool queueExploreDeparture(uint8_t area, bool autoWalk);
    bool updateExploreDeparture(uint32_t nowMs);
    void cancelExploreDeparture();
    void beginExploreReturn(uint32_t nowMs);
    bool startExploreRoute(uint32_t nowMs);
    bool generateExploreRouteMap(uint32_t nowMs);
    bool beginExploreRouteStep(uint32_t nowMs);
    void beginExploreRouteExit(uint32_t nowMs);
    void updateExploreRoute(uint32_t nowMs);
    void updateExploreEcology(uint32_t nowMs);
    void requestExploreRouteDynamicRender();
    bool beginExploreEncounter(
        uint32_t nowMs, bool boss = false, uint16_t speciesOverride = 0,
        uint8_t levelOverride = 0, uint16_t experiencePercent = 100,
        ExploreSpecial::Kind specialKind = ExploreSpecial::Kind::NONE);
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
    int8_t availableBattleSwitchSlot() const;
    bool resolveBattleFaint(uint32_t nowMs);
    uint8_t battleHpPercentForRender(bool wildSide, uint16_t currentHp,
                                     uint16_t maxHp, uint32_t nowMs) const;
    uint32_t battleExperienceForRender(uint32_t nowMs) const;
    void performBattleBag(uint32_t nowMs);
    void performBattleBagItem(Game::ItemId item, uint32_t nowMs);
    void performBattleFlee(uint32_t nowMs);
    void finishBattleVictory(uint32_t nowMs);
    void resolveBattleFriendship(uint8_t choice, uint32_t nowMs);
    void finishBattleAfterFriendship(uint32_t nowMs);
    void queueBattleProgression(uint8_t teamSlot, uint8_t oldLevel,
                                bool leveledUp);
    bool startNextBattleProgression(uint32_t nowMs);
    void finishBattleDefeat(uint32_t nowMs);
    void closeBattle(uint32_t nowMs);
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
    void completeExploreReturn();
    void placeFaintedHomeActors(uint32_t nowMs);
    void leaveExploreRoute();
    void updateClockAndCare(uint32_t nowMs);
    void updateMoodHearts(uint32_t nowMs);
    void startSleepAnimation(uint32_t nowMs, uint16_t frameMs);
    void updateSleepAnimation(uint32_t nowMs);
    void updatePet(uint32_t nowMs);
    void beginVisitDeparture(uint32_t nowMs);
    bool updateVisitDeparture(uint32_t nowMs);
    bool ambientActionAllowed() const;
    HomeFlow::Layout homeActionLayout(bool includeWindow = false) const;
    void applyHomeFlowEffects(uint32_t nowMs);
    void startFeedFinish(uint32_t nowMs);
    bool updateRoomAction(uint32_t nowMs, float elapsedSeconds);
    void cancelRoomAction(uint32_t nowMs);
    void configureHomeRuntime();
    void syncHomeActors(uint32_t nowMs);
    void initializeCompanionActor(uint32_t nowMs);
    void updateCompanion(uint32_t nowMs);
    Home::ActorObservation homeActorObservation(
        uint8_t teamSlot, uint32_t nowMs) const;
    Home::HouseholdObservation homeHouseholdObservation(
        uint32_t nowMs) const;
    bool homeActorCanEatFromBowl(uint8_t teamSlot,
                                 uint32_t nowMs) const;
    int8_t preferredBowlEater(uint32_t nowMs) const;
    bool claimBowl(uint8_t teamSlot, uint32_t nowMs);
    bool homeActorNearFood(uint8_t actorId) const;
    bool homeActorBlocksFoodApproach(uint8_t actorId) const;
    bool tryYieldHomeBlocker(uint8_t waitingActorId, float goalX, float goalY,
                             uint32_t nowMs);
    bool startMainFoodSeek(uint32_t nowMs);
    bool startCompanionFoodSeek(uint32_t nowMs);
    bool startCompanionFoodExit(uint32_t nowMs);
    void wakeHomeFoodArbitration(uint32_t nowMs);
    void serviceHomeFoodArbitration(uint32_t nowMs);
    bool chooseCompanionTarget(float& x, float& y);
    float companionSleepMinDistance() const;
    bool companionSleepSpotUsableWithDistance(
        float x, float y, float minDistance) const;
    bool companionSleepSpotUsable(float x, float y) const;
    bool companionPointBlocksBedRoute(float x, float y) const;
    bool chooseCompanionSleepSpot(float& x, float& y,
                                  bool avoidBedRoute = false) const;
    bool beginCompanionMove(Home::Task task, float x, float y,
                            uint32_t nowMs, bool allowOutsideStart = false,
                            bool avoidOther = true);
    void stopCompanion(uint32_t nowMs, uint32_t idleDelayMs = 0);
    PairFlow::Environment pairEnvironment() const;
    PairFlow::Layout pairLayout(bool includeSpacing = false) const;
    void applyPairFlowEffects(uint32_t nowMs);
    bool pairInteractionAllowed() const;
    void schedulePairInteraction(uint32_t nowMs, bool immediate = false);
    bool startPairInteraction(uint32_t nowMs, bool forceChase = false,
                              bool forceTalk = false, bool arrivalTalk = false);
    bool updatePairInteraction(uint32_t nowMs, float elapsedSeconds);
    void finishPairInteraction(uint32_t nowMs, bool reward);
    void cancelPairInteraction(uint32_t nowMs);
    void updateVisitorMotion(uint32_t nowMs, float elapsedSeconds);
    void beginVisitorEntry(uint32_t nowMs);
    VisitorFlow::Layout visitorLayout() const;
    void applyVisitorEffects(uint32_t nowMs);
    void applyVisitAwayEffects(uint32_t nowMs);
    void finishVisitorEntry(uint32_t nowMs, bool timedOut);
    void beginVisitorExit(uint32_t nowMs, bool debugVisitor);
    void finishVisitorExit(uint32_t nowMs, bool debugVisitor);
    void requestVisitEnd(uint32_t nowMs);
    void requestHomeActorRows(float previousY, float currentY);
    void persistHomeViewState(uint32_t nowMs);
    bool beginPetMove(PetMotion motion, float x, float y,
                      uint32_t nowMs);
    void finishPetMove(uint32_t nowMs);
    PetMotion petMotion() const;
    bool petIsResting() const;
    void schedulePetDecision(uint32_t nowMs);
    void updatePetFootprint();
    bool petFootprintInsideWalkArea(float x, float y) const;
    bool companionFootprintInsideWalkArea(float x, float y) const;
    bool petPathInsideWalkArea(float fromX, float fromY,
                               float toX, float toY) const;
    bool chooseWanderTarget(float& x, float& y,
                            bool requirePath = true) const;
    bool chooseWakeTarget(float& x, float& y) const;
    bool chooseFoodApproachTarget(float desiredX, float desiredY,
                                  float& x, float& y) const;
    void updateCamera();
    int worldToScreenX(float worldX) const;
    int worldToScreenY(float worldY) const;
    void requestRenderRows(uint16_t begin, uint16_t end);
    void requestRenderRect(int left, int top, int right, int bottom);
    ExploreRouteViewModel exploreRouteModel() const;
    void requestFullRender();
    float gameSpeed() const;
    SystemUpdateHandler systemUpdateHandler = nullptr;
    bool saveState();

    AppSceneFlow::Controller sceneFlow;
    bool dirtyRowsValid = true;
    uint16_t dirtyRowBegin = 0;
    uint16_t dirtyRowEnd = AmoledUi::HEIGHT;
    uint16_t dirtyColumnBegin = 0;
    uint16_t dirtyColumnEnd = AmoledUi::WIDTH;
    bool externalSceneFade = false;
    bool lockRequested = false;
    bool displayLocked = false;
    int8_t batteryPercent = -1;
    bool batterySampled = false;
    uint32_t lastBatterySampleMs = 0;

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
    uint8_t debugBedStyle = 0;
    uint8_t debugRugStyle = 0;
    uint8_t debugFocus = 0;
    uint8_t debugDigits[4] = {};
    bool debugBattleActive = false;
    bool debugBattleScenarioActive = false;
    bool debugBattleRequested = false;
    bool debugTiltControl = false;
    uint32_t nextHomeStateDiagnosticMs = 0;
    bool debugWalkBoundaryVisible = false;
    bool debugTalkPointsVisible = false;
    bool debugBattleDrawBoundsVisible = false;
    bool debugTouchDisplayEnabled = false;
    bool debugTouchPointValid = false;
    int16_t debugTouchX = 0;
    int16_t debugTouchY = 0;
    TouchTest::State debugTouchTest;
    uint8_t debugLightSource = 0;
    char debugToastBuffer[48] = {};
    bool debugContactPending = false;
    bool debugContactEventPending = false;
    bool debugContactEventWaitingForEntry = false;
    uint8_t debugContactPromptFade = 0;
    char debugPromptBuffer[64] = {};
    uint32_t debugContactMessageUntilMs = 0;
    bool debugContactChoiceVisible = false;
    int8_t debugContactSelectedChoice = -1;
    uint32_t debugContactChoiceConfirmUntilMs = 0;
    bool debugContactActive = false;
    uint8_t debugContactKind = 0;
    uint8_t debugContactStorageSlot = 0xFF;
    uint32_t debugContactStartedMs = 0;
    uint32_t visitorDiagNextLogMs = 0;
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
    uint32_t explorePreviewLastRenderRequestMs = 0;
    uint8_t exploreMenuCursor = 0;
    int pressedExploreMenuItem = -1;
    int pressedRoomItem = -1;
    int pressedCommunicationItem = -1;
    MusicContext musicContext = MusicContext::HOME;
    uint8_t selectedExploreArea = 0;
    ComputerViewModel::Page computerPage = ComputerViewModel::Page::MENU;
    // Social can be opened from the computer menu or from the background
    // assistant. Keep the return route explicit because both use the same
    // communication scene.
    bool communicationReturnToComputer = false;
    float computerScroll = 0.0f;
    float computerVelocity = 0.0f;
    uint8_t computerPressedItem = 0xFF;
    bool contactActionOpen = false;
    uint8_t contactActionSlot = 0xFF;
    int contactActionPressedItem = -1;
    bool contactConfirmOpen = false;
    bool contactConfirmYes = false;
    bool contactConfirmDelete = false;
    // CLAW_SETUP log view state. Entries themselves are copied into a
    // file-scope buffer at render time to avoid retaining another log copy
    // in every application instance.
    bool clawLogView = false;
    float clawLogScroll = 0.0f;
    float clawLogVelocity = 0.0f;
    bool clawLogPinned = true;
    uint32_t clawLogGen = 0;
    size_t clawLogCount = 0;
    SettingsController settingsController;
    TeamController teamController_;
    InventoryController inventoryController_;
    bool gameResetPending = false;
    bool saveWritesBlocked = false;
    HatchController hatchController_;
    std::unique_ptr<Game::GameState> hatchResult;
    ProgressionFlow progressionFlow_;

    ShowerController showerController_;
    bool showerSavePending = false;
    uint32_t showerSaveAfterMs = 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
    uint32_t showerDiagnosticStartedMs = 0;
#endif
    char showerToast[64] = {};

    ExploreSession exploreSession_;
    void applyExploreEffects(uint32_t nowMs);
    void resetExploreMapPresentation(uint32_t nowMs);
    void startExploreBattle(const ExploreSession::Encounter& encounter, uint32_t nowMs);
    ExplorePresenter explorePresenter_;
    void applyExplorePresentationDamage();

    using BattleContinuation = BattleSession::BattleContinuation;
    BattleSession battleSession_;
    void applyBattleEffects(uint32_t nowMs);

    SaveManager saveManager;
    GameClockService gameClock;
    Game::GameState gameState;
    Game::EncounterHistory encounterHistory;
    MainSceneViewState mainViewState;
    Game::CareTickAccumulators careAcc = {};
    Home::Actor homeMainActor;
    Home::Actor homeCompanionActor;
    Home::Simulation homeRuntime;
    bool startupStateLoaded_ = false;
    bool storageReady = false;
    bool homeRuntimeReady = false;
    uint32_t lastCareMs = 0;
    SaveCoordinator saveCoordinator_;
    uint32_t lastPersistMs = 0;
    uint32_t lastInteractionMs = 0;
    bool encounterHistoryDirty = false;
    Communication::VisitSessionService visitSession;
    Home::VisitArrivalFlow visitArrival;
    using VisitDeparture = Home::VisitDeparturePhase;
    VisitAwayFlow visitAwayFlow_{homeMainActor, homeRuntime};
    bool visitRecallConfirm = false;
    bool visitRadioExclusive = false;

    float cameraX = 0.0f;
    float cameraY = 0.0f;
    uint32_t lastPetUpdateMs = 0;
    SleepBreathClock sleepBreath;
    uint32_t nextPetFrameMs = 0;
    uint32_t nextFeedBiteMs = 0;
    uint8_t petFrame = 0;
    PokemonSprites::WalkDirection petDirection =
        PokemonSprites::WalkDirection::DOWN;
    bool petStopping = false;
    bool petLongMove = true;
    HomeFlow homeFlow_{gameState, homeMainActor, homeCompanionActor, homeRuntime};
    bool autonomousExpedition = false;
    bool exploreSessionActive = false;
    ExpeditionDeparturePhase expeditionDeparturePhase =
        ExpeditionDeparturePhase::NONE;
    SceneFade expeditionFade;
    bool pendingExpedition = false;
    bool pendingExpeditionAutoWalk = false;
    uint8_t pendingExpeditionArea = 0;
    float expeditionDoorInsideX = 0.0f;
    float expeditionDoorInsideY = 0.0f;
    float expeditionDoorOutsideX = 0.0f;
    float expeditionDoorOutsideY = 0.0f;
    bool expeditionMainHidden = false;
    bool expeditionCompanionDeparting = false;
    bool expeditionCompanionHidden = false;
    float expeditionCompanionDoorInsideX = 0.0f;
    float expeditionCompanionDoorInsideY = 0.0f;
    float expeditionCompanionDoorOutsideX = 0.0f;
    float expeditionCompanionDoorOutsideY = 0.0f;
    uint32_t expeditionDoorPhaseStartedMs = 0;
    uint32_t nextPetDebugLogMs = 0;
    uint32_t lastCompanionUpdateMs = 0;
    uint32_t nextCompanionFrameMs = 0;
    uint8_t companionFrame = 0;
    PokemonSprites::WalkDirection companionDirection =
        PokemonSprites::WalkDirection::DOWN;
    bool companionLongMove = true;
    uint32_t companionFoodBiteMs = 0;
    bool homeFoodArbitrationPending = false;
    uint32_t nextHomeFoodArbitrationMs = 0;
    uint32_t nextHomeFoodDebugLogMs = 0;

    PairFlow pairFlow_{gameState, homeMainActor, homeCompanionActor, homeRuntime, visitArrival};

    VisitorFlow visitorFlow_{gameState, homeMainActor, homeCompanionActor, homeRuntime, visitArrival};

    uint32_t heartsUntil = 0;
    uint8_t moodHeartCount = 0;
    uint8_t moodBurstHeart = 0xFF;
    uint32_t moodBurstStartedMs = 0;
    uint32_t moodBurstUntilMs = 0;
    uint32_t nextMoodBurstFrameMs = 0;
    char toastText[161] = {};
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    AppSceneFlow::Scene toastScene = AppSceneFlow::Scene::HOME;
    char brainMessage[161] = {};
    char clawSetupMessage[96] = {};
};

}  // namespace AmoledV1
