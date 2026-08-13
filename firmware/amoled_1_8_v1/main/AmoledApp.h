#pragma once

#include <cstdint>

#include "TouchInput.h"
#include "HomeScreen.h"
#include "core/AppSceneFlow.h"
#include "core/GameClockService.h"
#include "core/MainSceneViewState.h"
#include "core/SaveManager.h"
#include "game/CareTicker.h"
#include "game/BathService.h"
#include "game/GameState.h"
#include "game/ExploreMapGenerator.h"
#include "game/MonsterMind.h"
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
    void markRendered() { dirtyRowsValid = false; }
    bool consumeLockRequest();
    void onWake();

private:
    enum class PetMotion : uint8_t {
        IDLE,
        WANDERING,
        SEEKING_FOOD,
        EATING,
    };

    enum class PendingItemAction : uint8_t {
        NONE = 0,
        BUY,
        SELL,
        USE,
    };

    void handleTap(int x, int y, uint32_t nowMs);
    void setToast(const char* value, uint32_t nowMs,
                  uint32_t durationMs = 1100);
    void clampMenuScroll();
    void clampExploreScroll();
    void clampItemScroll();
    void snapShopItemScroll();
    uint8_t currentItemCount() const;
    Game::ItemId currentItemAt(uint8_t index) const;
    void openItemScene(AppSceneFlow::Scene target);
    void closeItemScene();
    void performPendingItemAction(uint32_t nowMs);
    void openRoomScene();
    void openShowerScene(uint32_t nowMs);
    void closeShowerScene();
    void startShowerSoap(uint8_t soapIndex, uint32_t nowMs);
    void startShowerTool(ShowerMode mode, uint32_t nowMs);
    void updateShowerToolDrag(int x, int y, uint32_t nowMs);
    void grantShowerStage(Game::BathService::Stage stage, uint32_t nowMs);
    void startShowerRinse(uint32_t nowMs);
    void requestShowerExit();
    void resetShowerSession(uint32_t nowMs);
    void openTeamScene();
    void switchTeamLeader(uint32_t nowMs);
    bool startExploreRoute(uint32_t nowMs);
    bool beginExploreRouteStep(uint32_t nowMs);
    void updateExploreRoute(uint32_t nowMs);
    void updateExploreRouteCamera();
    void pauseExploreRoute(uint32_t nowMs);
    void resumeExploreRoute(uint32_t nowMs);
    void leaveExploreRoute();
    void updateClockAndCare(uint32_t nowMs);
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
    float exploreScroll = 0.0f;
    float exploreVelocity = 0.0f;
    int pressedExploreArea = -1;
    int pressedExploreMenuItem = -1;
    float itemScroll = 0.0f;
    float itemVelocity = 0.0f;
    int pressedItemRow = -1;
    int pressedShopCategory = -1;
    int pressedTeamSlot = -1;
    int pressedRoomItem = -1;
    int pressedShowerItem = -1;
    bool teamConfirmOpen = false;
    uint8_t pendingTeamSlot = 0;
    bool shopCategoryView = true;
    Game::ShopService::Category shopCategory =
        Game::ShopService::Category::DAILY;
    bool itemConfirmOpen = false;
    Game::ItemId pendingItem = Game::ItemId::COUNT;
    PendingItemAction pendingItemAction = PendingItemAction::NONE;
    uint8_t selectedExploreArea = 0;

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
    char showerToast[24] = {};

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

    SaveManager saveManager;
    GameClockService gameClock;
    Game::GameState gameState;
    MainSceneViewState mainViewState;
    Game::CareTickAccumulators careAcc = {};
    MonsterMind monsterMind;
    MonsterBehaviorProfile behaviorProfile;
    bool storageReady = false;
    uint32_t lastCareMs = 0;
    uint32_t lastPersistMs = 0;

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
    bool petFacingRight = true;

    uint32_t heartsUntil = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
};

}  // namespace AmoledV1
