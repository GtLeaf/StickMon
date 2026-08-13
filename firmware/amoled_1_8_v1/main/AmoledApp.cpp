#include "AmoledApp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "HomeScreen.h"
#include "assets/PokemonSprites.h"
#include "core/MathUtil.h"
#include "core/RoomMovementArea.h"
#include "core/RoomResource.h"
#include "game/ExploreItemProgression.h"
#include "game/BathService.h"
#include "game/ExploreRouteGeometry.h"
#include "game/GameRandom.h"
#include "game/HomeCare.h"
#include "game/HomeHud.h"
#include "game/ItemInventory.h"
#include "game/ShopService.h"
#include "game/Species.h"
#include "game/SpeciesBehavior.h"
#include "game/TeamRoster.h"
#include "platform/api/PlatformServices.h"
#include "platform/api/FlashStorage.h"
#include "presentation/Canvas565.h"

namespace AmoledV1 {
namespace {

constexpr int TAP_SLOP = 6;
constexpr int DRAG_START_SLOP = 3;
constexpr int MENU_HEADER_HEIGHT = 28;
constexpr uint32_t MIND_UPDATE_MS = 400;
constexpr uint32_t MOTION_FRAME_MS = 140;
constexpr uint32_t CARE_TICK_MS = 60000;
constexpr uint32_t PERIODIC_SAVE_MS = 5UL * 60UL * 1000UL;
constexpr uint16_t EXPLORE_ROUTE_STEP_MS = 360;
constexpr uint16_t EXPLORE_ROUTE_FRAME_MS = 90;
constexpr int EXPLORE_ROUTE_VIEW_HEIGHT = 224 - HOME_HEADER_HEIGHT;
constexpr int EXPLORE_ROUTE_WORLD_WIDTH =
    ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE;
constexpr int EXPLORE_ROUTE_WORLD_HEIGHT =
    ExploreMapGenerator::HEIGHT * ExploreRouteGeometry::TILE_SIZE;
constexpr float FALLBACK_ROOM_MIN_X = 56.0f;
constexpr float FALLBACK_ROOM_MAX_X = 122.0f;
constexpr float FALLBACK_ROOM_MIN_Y = 126.0f;
constexpr float FALLBACK_ROOM_MAX_Y = 151.0f;
constexpr float FALLBACK_FOOD_APPROACH_X = 121.0f;
constexpr float FALLBACK_FOOD_APPROACH_Y = 149.0f;
constexpr float FOOD_FEED_OFFSET_X = 12.0f;
constexpr float FOOD_FEED_OFFSET_Y = 4.0f;
constexpr float CAMERA_SAFE_LEFT = 62.0f;
constexpr float CAMERA_SAFE_RIGHT = 122.0f;
constexpr float CAMERA_SAFE_TOP = 58.0f;
constexpr float CAMERA_SAFE_BOTTOM = 126.0f;
constexpr int SHOWER_BODY_LEFT = 42;
constexpr int SHOWER_BODY_RIGHT = 142;
constexpr int SHOWER_BODY_TOP = 48;
constexpr int SHOWER_BODY_BOTTOM = 158;
constexpr int SHOWER_TOOL_MIN_X = 12;
constexpr int SHOWER_TOOL_MAX_X = 172;
constexpr int SHOWER_TOOL_MIN_Y = 36;
constexpr int SHOWER_TOOL_MAX_Y = 210;
constexpr float SHOWER_PROGRESS_DISTANCE = 20.0f;
constexpr uint8_t SHOWER_PROGRESS_MAX = 8;
constexpr RoomResource::Point FALLBACK_WALK_POLYGON[] = {
    {static_cast<int16_t>(FALLBACK_ROOM_MIN_X),
     static_cast<int16_t>(FALLBACK_ROOM_MIN_Y)},
    {static_cast<int16_t>(FALLBACK_ROOM_MAX_X),
     static_cast<int16_t>(FALLBACK_ROOM_MIN_Y)},
    {static_cast<int16_t>(FALLBACK_ROOM_MAX_X),
     static_cast<int16_t>(FALLBACK_ROOM_MAX_Y)},
    {static_cast<int16_t>(FALLBACK_ROOM_MIN_X),
     static_cast<int16_t>(FALLBACK_ROOM_MAX_Y)},
};

uint8_t exploreDirectionForDelta(float dx, float dy, uint8_t fallback) {
    if (std::fabs(dx) < 0.01f && std::fabs(dy) < 0.01f) return fallback;
    if (std::fabs(dx) >= std::fabs(dy)) {
        return static_cast<uint8_t>(
            dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                       : PokemonSprites::WalkDirection::LEFT);
    }
    return static_cast<uint8_t>(
        dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                   : PokemonSprites::WalkDirection::UP);
}

uint8_t exploreInwardDirection(ExploreMapGenerator::Edge edge) {
    switch (edge) {
    case ExploreMapGenerator::Edge::TOP:
        return static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN);
    case ExploreMapGenerator::Edge::RIGHT:
        return static_cast<uint8_t>(PokemonSprites::WalkDirection::LEFT);
    case ExploreMapGenerator::Edge::BOTTOM:
        return static_cast<uint8_t>(PokemonSprites::WalkDirection::UP);
    case ExploreMapGenerator::Edge::LEFT:
        return static_cast<uint8_t>(PokemonSprites::WalkDirection::RIGHT);
    }
    return static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN);
}

}  // namespace

void AmoledApp::begin(uint32_t nowMs) {
    storageReady = saveManager.begin();
    bool normalized = false;
    bool loaded = storageReady &&
                  saveManager.load(gameState, mainViewState, &normalized);
    if (!loaded) {
        gameState = Game::GameState{};
        gameState.oobeDone = true;
        mainViewState = MainSceneViewState{};
        if (storageReady) saveState();
    } else if (normalized) {
        saveState();
    }
    Platform::logf("[AmoledApp] save=%s species=%u level=%u food=%u\n",
                   loaded ? "loaded" : "created",
                   gameState.team[0].speciesId,
                   gameState.team[0].level,
                   gameState.room.food[gameState.room.selectedFood]);
    gameClock.start(nowMs, gameState.gameMinutesTotal);
    lastCareMs = nowMs;
    lastPersistMs = nowMs;
    lastPetUpdateMs = nowMs;
    nextMindUpdateMs = nowMs;
    GameRandom::seed(nowMs ^
                     (static_cast<uint32_t>(gameState.team[0].speciesId) << 16));
    monsterMind.reset(nowMs);
    if (const Species* species = findSpecies(gameState.team[0].speciesId)) {
        behaviorProfile = behaviorProfileFor(*species, gameState.team[0]);
    }
    RoomResource::ins().begin();
    updatePetFootprint();
    if (!petFootprintInsideWalkArea(petX, petY)) {
        float x = petX;
        float y = petY;
        if (chooseWanderTarget(x, y, false)) {
            petX = petTargetX = x;
            petY = petTargetY = y;
        }
    }
    updateCamera();
    schedulePetDecision(nowMs);
    requestFullRender();
}

void AmoledApp::handleTouch(const TouchEvent& event) {
    switch (event.type) {
    case TouchEventType::DOWN:
        pointerDown = true;
        dragging = false;
        touchStartX = event.x;
        touchStartY = event.y;
        touchLastY = event.y;
        menuVelocity = 0.0f;
        if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU &&
            event.y >= MENU_HEADER_HEIGHT) {
            pressedMenuItem = mainMenuItemAt(event.x, event.y, menuScroll);
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedExploreArea = exploreAreaAt(
                event.x, event.y, exploreScroll,
                ExploreItemProgression::visibleAreaCount(gameState));
            exploreVelocity = 0.0f;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedExploreMenuItem = exploreRouteMenuItemAt(event.x, event.y);
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamConfirmOpen && event.y >= MENU_HEADER_HEIGHT) {
            pressedTeamSlot = teamMemberAt(
                event.x, event.y,
                Game::TeamRoster::memberCount(gameState));
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::ROOM &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedRoomItem = roomMenuItemAt(event.x, event.y);
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
            if ((showerMode == ShowerMode::SOAPING ||
                 showerMode == ShowerMode::BRUSHING) &&
                showerToolAt(event.x, event.y, showerToolX, showerToolY)) {
                showerToolDragging = true;
                dragging = true;
                showerLastStrokeX = event.x;
                showerLastStrokeY = event.y;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            } else {
                pressedShowerItem = showerMode == ShowerMode::SOAP_SELECT
                    ? showerSoapItemAt(event.x, event.y)
                    : showerMode == ShowerMode::EXIT_CONFIRM
                        ? showerExitChoiceAt(event.x, event.y)
                        : showerMode == ShowerMode::MENU
                            ? showerMenuItemAt(event.x, event.y) : -1;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                   shopCategoryView && event.y >= MENU_HEADER_HEIGHT) {
            pressedShopCategory = shopCategoryItemAt(event.x, event.y);
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                    (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                     !shopCategoryView)) &&
                   !itemConfirmOpen && event.y >= MENU_HEADER_HEIGHT) {
            itemVelocity = 0.0f;
            if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
                pressedItemRow = shopActionAt(event.x, event.y)
                    ? shopSelectedItem(itemScroll, currentItemCount())
                    : shopItemAt(event.x, event.y, itemScroll,
                                 currentItemCount());
            } else {
                pressedItemRow = itemListItemAt(
                    event.x, event.y, itemScroll, currentItemCount());
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        }
        break;

    case TouchEventType::MOVE:
        if (!pointerDown) break;
        if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
            if (showerToolDragging) {
                updateShowerToolDrag(event.x, event.y, event.timestampMs);
            } else if (std::max(std::abs(event.x - touchStartX),
                                std::abs(event.y - touchStartY)) > TAP_SLOP) {
                pressedShowerItem = -1;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU &&
            touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                pressedMenuItem = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                menuScroll -= static_cast<float>(deltaY);
                menuVelocity = static_cast<float>(-deltaY);
                clampMenuScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                pressedExploreArea = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                exploreScroll -= static_cast<float>(deltaY);
                exploreVelocity = static_cast<float>(-deltaY);
                clampExploreScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedExploreMenuItem >= 0) {
            pressedExploreMenuItem = -1;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamConfirmOpen &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedTeamSlot >= 0) {
            pressedTeamSlot = -1;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                   shopCategoryView &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedShopCategory >= 0) {
            pressedShopCategory = -1;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                    (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                     !shopCategoryView)) &&
                   !itemConfirmOpen &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                pressedItemRow = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                itemScroll -= static_cast<float>(deltaY);
                itemVelocity = static_cast<float>(-deltaY);
                clampItemScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        }
        touchLastY = event.y;
        break;

    case TouchEventType::UP: {
        if (!pointerDown) break;
        int distance = std::max(std::abs(event.x - touchStartX),
                                std::abs(event.y - touchStartY));
        if (!dragging && distance <= TAP_SLOP) {
            handleTap(event.x, event.y, event.timestampMs);
        }
        pointerDown = false;
        dragging = false;
        pressedMenuItem = -1;
        pressedExploreArea = -1;
        pressedExploreMenuItem = -1;
        pressedItemRow = -1;
        pressedShopCategory = -1;
        pressedTeamSlot = -1;
        pressedRoomItem = -1;
        pressedShowerItem = -1;
        showerToolDragging = false;
        requestRenderRows(
                          sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE
                              ? HOME_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                            sceneFlow.current() == AppSceneFlow::Scene::SHOP ||
                            sceneFlow.current() == AppSceneFlow::Scene::TEAM ||
                            sceneFlow.current() == AppSceneFlow::Scene::ROOM ||
                            sceneFlow.current() == AppSceneFlow::Scene::SHOWER
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::HOME
                              ? (touchStartY >= HOME_STATUS_TOP
                                     ? HOME_STATUS_TOP : HOME_ROOM_TOP)
                                                 : HOME_ROOM_TOP,
                          224);
        break;
    }
    }
}

void AmoledApp::handleTap(int x, int y, uint32_t nowMs) {
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        RoomResource& room = RoomResource::ins();
        int bowlX = room.available()
            ? worldToScreenX(room.foodX())
            : 145;
        int bowlY = room.available()
            ? worldToScreenY(room.foodY())
            : 143;
        switch (homeHitTargetAt(x, y, worldToScreenX(petX),
                               worldToScreenY(petY), bowlX, bowlY)) {
        case HomeHitTarget::MENU:
            sceneFlow.openMenu();
            menuScroll = 0.0f;
            menuVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            break;
        case HomeHitTarget::LOCK:
            saveState();
            lockRequested = true;
            break;
        case HomeHitTarget::PET:
            switch (Game::HomeCare::petMonster(
                gameState, 0, gameState.gameMinutesTotal * 60UL).outcome) {
            case PetOutcome::REWARDED:
                heartsUntil = nowMs + 1000;
                setToast("PET HAPPY", nowMs);
                saveState();
                break;
            case PetOutcome::DAILY_LIMIT:
                setToast("PET REST", nowMs);
                saveState();
                break;
            case PetOutcome::NEEDS_REST:
                setToast("NEEDS REST", nowMs);
                break;
            }
            break;
        case HomeHitTarget::BOWL: {
            FoodPlacementResult result =
                Game::HomeCare::placeSelectedFoodInBowl(gameState);
            switch (result) {
            case FoodPlacementResult::ADDED:
                setToast("FOOD ADDED", nowMs);
                nextMindUpdateMs = nowMs;
                nextPetDecisionMs = nowMs;
                saveState();
                break;
            case FoodPlacementResult::NO_STOCK:
                setToast("NO FOOD", nowMs);
                break;
            case FoodPlacementResult::BOWL_FULL:
                setToast("BOWL FULL", nowMs);
                break;
            case FoodPlacementResult::DIFFERENT_FOOD:
                setToast("CHANGE FOOD", nowMs);
                break;
            }
            break;
        }
        case HomeHitTarget::NONE:
            break;
        }
        requestRenderRows(HOME_ROOM_TOP, 224);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        if (exploreRouteExitConfirm) {
            int choice = exploreRouteExitChoiceAt(x, y);
            if (choice == 0) {
                exploreRouteExitConfirm = false;
                resumeExploreRoute(nowMs);
                requestRenderRows(HOME_HEADER_HEIGHT, 224);
            } else if (choice == 1) {
                leaveExploreRoute();
            }
            return;
        }
        if (exploreRouteBackAt(x, y)) {
            exploreRouteExitConfirm = true;
            pauseExploreRoute(nowMs);
            requestRenderRows(HOME_HEADER_HEIGHT, 224);
            return;
        }
        if (exploreRouteMenuAt(x, y)) {
            pauseExploreRoute(nowMs);
            sceneFlow.openExploreMenu();
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (exploreRouteMapAt(x, y)) {
            if (exploreRouteComplete) {
                leaveExploreRoute();
            } else {
                exploreRouteAutoWalk = !exploreRouteAutoWalk;
                if (exploreRouteAutoWalk && !exploreRouteMoving) {
                    beginExploreRouteStep(nowMs);
                }
                requestRenderRows(200, 224);
            }
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        if (exploreRouteMenuBackAt(x, y)) {
            sceneFlow.closeExploreMenu();
            resumeExploreRoute(nowMs);
            toast = nullptr;
            requestFullRender();
            return;
        }
        int itemIndex = exploreRouteMenuItemAt(x, y);
        if (itemIndex < 0) return;
        AppSceneFlow::ExploreMenuEntry entry =
            AppSceneFlow::exploreMenuEntry(static_cast<uint8_t>(itemIndex));
        switch (entry.item) {
        case AppSceneFlow::ExploreMenuItem::TEAM:
            openTeamScene();
            break;
        case AppSceneFlow::ExploreMenuItem::BAG:
            openItemScene(AppSceneFlow::Scene::BAG);
            break;
        case AppSceneFlow::ExploreMenuItem::END:
            leaveExploreRoute();
            break;
        case AppSceneFlow::ExploreMenuItem::BACK:
            sceneFlow.closeExploreMenu();
            resumeExploreRoute(nowMs);
            toast = nullptr;
            requestFullRender();
            break;
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM) {
        if (teamConfirmOpen) {
            int choice = teamConfirmChoiceAt(x, y);
            if (choice == 0) {
                switchTeamLeader(nowMs);
            } else if (choice == 1) {
                teamConfirmOpen = false;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
            return;
        }
        if (teamBackAt(x, y)) {
            closeItemScene();
            return;
        }
        int slot = teamMemberAt(
            x, y, Game::TeamRoster::memberCount(gameState));
        if (slot < 0) return;
        if (slot == 0) {
            setToast("CURRENT LEADER", nowMs);
        } else if (!Game::TeamRoster::canMoveToFront(
                       gameState, static_cast<uint8_t>(slot))) {
            setToast("VISITOR LOCKED", nowMs);
        } else {
            pendingTeamSlot = static_cast<uint8_t>(slot);
            teamConfirmOpen = true;
            toast = nullptr;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
        if (itemListBackAt(x, y)) {
            closeItemScene();
            return;
        }
        int item = roomMenuItemAt(x, y);
        if (item == 0) {
            setToast("MIGRATION NEXT", nowMs);
        } else if (item == 1) {
            openShowerScene(nowMs);
        } else if (item == 2) {
            closeItemScene();
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
        if (showerMode == ShowerMode::EXIT_CONFIRM) {
            int choice = showerExitChoiceAt(x, y);
            if (choice == 0) {
                showerExitConfirmYes = true;
                closeShowerScene();
            } else if (choice == 1) {
                showerExitConfirmYes = false;
                showerMode = ShowerMode::MENU;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
            return;
        }
        if (showerMode == ShowerMode::SOAP_SELECT) {
            if (showerBackAt(x, y)) {
                showerMode = ShowerMode::MENU;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
                return;
            }
            int soap = showerSoapItemAt(x, y);
            if (soap >= 0) {
                startShowerSoap(static_cast<uint8_t>(soap), nowMs);
            }
            return;
        }
        if (showerBackAt(x, y)) {
            requestShowerExit();
            return;
        }
        if (showerMode != ShowerMode::MENU) return;

        int item = showerMenuItemAt(x, y);
        if (item == 0) {
            if (showerSoapConsumed) {
                setToast("ALREADY SOAPED", nowMs);
            } else if (Game::BathService::nextOwnedSoap(gameState, -1) < 0) {
                setToast("NO SOAP", nowMs);
            } else {
                showerMode = ShowerMode::SOAP_SELECT;
                toast = nullptr;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (item == 1) {
            if (!showerSoapRewarded) {
                setToast("SOAP FIRST", nowMs);
            } else {
                startShowerTool(ShowerMode::BRUSHING, nowMs);
            }
        } else if (item == 2) {
            if (!showerSoapRewarded) {
                setToast("SOAP FIRST", nowMs);
            } else if (!showerBrushRewarded) {
                setToast("BRUSH FIRST", nowMs);
            } else {
                startShowerRinse(nowMs);
            }
        } else if (item == 3) {
            requestShowerExit();
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        if (itemConfirmOpen) {
            int choice = itemConfirmChoiceAt(x, y);
            if (choice == 0) {
                performPendingItemAction(nowMs);
            } else if (choice == 1) {
                itemConfirmOpen = false;
                pendingItem = Game::ItemId::COUNT;
                pendingItemAction = PendingItemAction::NONE;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
            return;
        }
        if (itemListBackAt(x, y)) {
            closeItemScene();
            return;
        }
        int index = itemListItemAt(x, y, itemScroll, currentItemCount());
        if (index < 0) return;
        Game::ItemId item = currentItemAt(static_cast<uint8_t>(index));
        if (!Game::ItemInventory::usableFromHomeBag(item)) {
            setToast("MIGRATION NEXT", nowMs);
            return;
        }
        pendingItem = item;
        pendingItemAction = PendingItemAction::USE;
        itemConfirmOpen = true;
        toast = nullptr;
        requestRenderRows(MENU_HEADER_HEIGHT, 224);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        if (shopCategoryView) {
            if (itemListBackAt(x, y)) {
                closeItemScene();
                return;
            }
            int category = shopCategoryItemAt(x, y);
            if (category < 0) return;
            if (category == 3) {
                closeItemScene();
                return;
            }
            shopCategory = category == 0
                ? Game::ShopService::Category::DAILY
                : category == 1 ? Game::ShopService::Category::EXPLORE
                                : Game::ShopService::Category::SELL;
            shopCategoryView = false;
            itemScroll = 0.0f;
            itemVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (itemConfirmOpen) {
            int choice = itemConfirmChoiceAt(x, y);
            if (choice == 0) {
                performPendingItemAction(nowMs);
            } else if (choice == 1) {
                itemConfirmOpen = false;
                pendingItem = Game::ItemId::COUNT;
                pendingItemAction = PendingItemAction::NONE;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
            return;
        }
        if (itemListBackAt(x, y)) {
            shopCategoryView = true;
            itemScroll = 0.0f;
            itemVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            return;
        }
        int index = shopItemAt(x, y, itemScroll, currentItemCount());
        if (index >= 0) {
            itemScroll = shopItemScrollForIndex(static_cast<uint8_t>(index));
            itemVelocity = 0.0f;
            toast = nullptr;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
            return;
        }
        if (!shopActionAt(x, y)) return;
        index = shopSelectedItem(itemScroll, currentItemCount());
        if (index < 0) return;
        pendingItem = currentItemAt(static_cast<uint8_t>(index));
        pendingItemAction = shopCategory == Game::ShopService::Category::SELL
            ? PendingItemAction::SELL : PendingItemAction::BUY;
        itemConfirmOpen = pendingItem != Game::ItemId::COUNT;
        toast = nullptr;
        requestRenderRows(MENU_HEADER_HEIGHT, 224);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        if (exploreBackAt(x, y)) {
            sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
            exploreVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (exploreMenuAt(x, y)) {
            sceneFlow.openMenu();
            menuScroll = 0.0f;
            menuVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            return;
        }
        int area = exploreAreaAt(
            x, y, exploreScroll,
            ExploreItemProgression::visibleAreaCount(gameState));
        if (area >= 0) {
            if (ExploreItemProgression::isAreaUnlocked(area, gameState)) {
                if (selectedExploreArea == static_cast<uint8_t>(area)) {
                    startExploreRoute(nowMs);
                } else {
                    selectedExploreArea = static_cast<uint8_t>(area);
                    toast = nullptr;
                }
            } else {
                setToast("AREA LOCKED", nowMs);
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        }
        return;
    }

    if (mainMenuBackAt(x, y)) {
        AppSceneFlow::Scene destination = sceneFlow.closeMenu();
        if (destination == AppSceneFlow::Scene::EXPLORE_ROUTE) {
            resumeExploreRoute(nowMs);
        }
        menuVelocity = 0.0f;
        toast = nullptr;
        requestFullRender();
        return;
    }

    int itemIndex = mainMenuItemAt(x, y, menuScroll);
    if (itemIndex >= 0) {
        AppSceneFlow::MainMenuEntry entry = AppSceneFlow::mainMenuEntry(
            static_cast<uint8_t>(itemIndex), false);
        if (entry.target == AppSceneFlow::Scene::EXPLORE_AREAS) {
            if (sceneFlow.menuReturn() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
                sceneFlow.enterExploreRoute();
                resumeExploreRoute(nowMs);
            } else {
                sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);
                exploreScroll = 0.0f;
                exploreVelocity = 0.0f;
                selectedExploreArea = std::min<uint8_t>(
                    selectedExploreArea,
                    ExploreItemProgression::unlockedArea(gameState));
            }
            toast = nullptr;
            requestFullRender();
        } else if (entry.target == AppSceneFlow::Scene::TEAM) {
            openTeamScene();
        } else if (entry.target == AppSceneFlow::Scene::ROOM) {
            openRoomScene();
        } else if (entry.target == AppSceneFlow::Scene::BAG ||
                   entry.target == AppSceneFlow::Scene::SHOP) {
            openItemScene(entry.target);
        } else if (entry.target == AppSceneFlow::Scene::HOME) {
            exploreRouteMoving = false;
            exploreRouteAutoWalk = false;
            exploreRoutePaused = false;
            exploreRouteExitConfirm = false;
            sceneFlow.goHome();
            toast = nullptr;
            requestFullRender();
        } else {
            setToast("MIGRATION NEXT", nowMs);
        }
    }
}

void AmoledApp::update(uint32_t nowMs) {
    updateClockAndCare(nowMs);
    updatePet(nowMs);
    updateExploreRoute(nowMs);

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
        if (showerMode == ShowerMode::RINSING &&
            nowMs - showerLastFrameMs >= 80) {
            showerLastFrameMs = nowMs;
            uint32_t elapsed = nowMs - showerModeStartedMs;
            showerRinseProgress = static_cast<uint8_t>(
                std::min<uint32_t>(100, elapsed * 100 / 1800));
            requestRenderRows(MENU_HEADER_HEIGHT, 176);
            if (elapsed >= 1800) {
                grantShowerStage(Game::BathService::Stage::RINSE, nowMs);
                showerCompletionHearts = static_cast<uint8_t>(
                    (showerSoapRewarded ? 1 : 0) +
                    (showerBrushRewarded ? 1 : 0) +
                    (showerRinseRewarded ? 1 : 0));
                showerMode = ShowerMode::COMPLETE;
                showerModeStartedMs = nowMs;
                requestFullRender();
            }
        } else if (showerMode == ShowerMode::COMPLETE &&
                   nowMs - showerModeStartedMs >= 1500) {
            resetShowerSession(nowMs);
            requestFullRender();
        }
    }

    if (toast && static_cast<int32_t>(nowMs - toastUntil) >= 0) {
        toast = nullptr;
        requestRenderRows(
            sceneFlow.current() == AppSceneFlow::Scene::HOME
                ? HOME_ROOM_TOP : MENU_HEADER_HEIGHT,
            sceneFlow.current() == AppSceneFlow::Scene::HOME
                ? HOME_STATUS_TOP : 224);
    }
    if (heartsUntil && static_cast<int32_t>(nowMs - heartsUntil) >= 0) {
        heartsUntil = 0;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU && !pointerDown &&
        std::fabs(menuVelocity) > 0.12f) {
        float previous = menuScroll;
        menuScroll += menuVelocity;
        menuVelocity *= 0.86f;
        clampMenuScroll();
        if (std::fabs(menuScroll - previous) > 0.01f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else {
            menuVelocity = 0.0f;
        }
    } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
               !pointerDown &&
               std::fabs(exploreVelocity) > 0.12f) {
        float previous = exploreScroll;
        exploreScroll += exploreVelocity;
        exploreVelocity *= 0.86f;
        clampExploreScroll();
        if (std::fabs(exploreScroll - previous) > 0.01f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else {
            exploreVelocity = 0.0f;
        }
    } else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                 !shopCategoryView)) &&
               !itemConfirmOpen && !pointerDown &&
               std::fabs(itemVelocity) > 0.12f) {
        float previous = itemScroll;
        itemScroll += itemVelocity;
        itemVelocity *= 0.86f;
        clampItemScroll();
        if (std::fabs(itemScroll - previous) > 0.01f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else {
            itemVelocity = 0.0f;
        }
        if (std::fabs(itemVelocity) <= 0.12f) {
            itemVelocity = 0.0f;
            if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
                snapShopItemScroll();
            }
        }
    }
}

bool AmoledApp::startExploreRoute(uint32_t nowMs) {
    if (!ExploreItemProgression::isAreaUnlocked(
            selectedExploreArea, gameState)) {
        return false;
    }

    uint32_t baseSeed = gameState.gameMinutesTotal * 2654435761UL;
    baseSeed ^= static_cast<uint32_t>(gameState.team[0].speciesId) * 97UL;
    baseSeed ^= static_cast<uint32_t>(selectedExploreArea + 1) * 2246822519UL;
    static constexpr uint32_t RETRY_SALTS[] = {
        0x00000000UL, 0x9E3779B9UL, 0xA341316CUL, 0xC8013EA4UL,
    };
    bool generated = false;
    for (uint32_t salt : RETRY_SALTS) {
        uint32_t seed = baseSeed ^ salt;
        if (seed == 0) seed = 1;
        if (ExploreMapGenerator::generate(
                seed, ExploreMapGenerator::Edge::TOP,
                selectedExploreArea, exploreRouteMap)) {
            generated = true;
            break;
        }
    }
    if (!generated || exploreRouteMap.pathCount == 0 ||
        exploreRouteMap.paths[0].pointCount == 0) {
        setToast("MAP FAILED", nowMs);
        return false;
    }

    exploreRoutePath = 0;
    exploreRouteIndex = 0;
    exploreRouteSteps = 0;
    exploreRouteMoving = false;
    exploreRouteAutoWalk = false;
    exploreRoutePaused = false;
    exploreRouteComplete = false;
    exploreRouteExitConfirm = false;
    exploreRouteDirection = exploreInwardDirection(exploreRouteMap.entry.edge);
    exploreRoutePetFrame = 0;
    ExploreRouteGeometry::WorldPoint start =
        ExploreRouteGeometry::pathPoint(exploreRouteMap.paths[0], 0);
    exploreRouteWorldX = exploreRouteFromX = exploreRouteTargetX = start.x;
    exploreRouteWorldY = exploreRouteFromY = exploreRouteTargetY = start.y;
    nextExploreRouteFrameMs = nowMs;
    updateExploreRouteCamera();
    sceneFlow.enterExploreRoute();
    toast = nullptr;
    Platform::logf(
        "[AmoledExplore] area=%u seed=%08lx fingerprint=%08lx points=%u\n",
        static_cast<unsigned>(selectedExploreArea),
        static_cast<unsigned long>(exploreRouteMap.seed),
        static_cast<unsigned long>(
            ExploreMapGenerator::fingerprint(exploreRouteMap)),
        static_cast<unsigned>(exploreRouteMap.paths[0].pointCount));
    requestFullRender();
    return true;
}

bool AmoledApp::beginExploreRouteStep(uint32_t nowMs) {
    if (exploreRouteMoving || exploreRoutePaused || exploreRouteComplete ||
        exploreRouteMap.pathCount == 0 ||
        exploreRoutePath >= exploreRouteMap.pathCount) {
        return false;
    }
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (exploreRouteIndex + 1 >= path.pointCount) {
        exploreRouteComplete = true;
        exploreRouteAutoWalk = false;
        requestRenderRows(HOME_HEADER_HEIGHT, 224);
        return false;
    }

    exploreRouteFromX = exploreRouteWorldX;
    exploreRouteFromY = exploreRouteWorldY;
    ++exploreRouteIndex;
    ExploreRouteGeometry::WorldPoint target =
        ExploreRouteGeometry::pathPoint(path, exploreRouteIndex);
    exploreRouteTargetX = target.x;
    exploreRouteTargetY = target.y;
    exploreRouteDirection = exploreDirectionForDelta(
        exploreRouteTargetX - exploreRouteFromX,
        exploreRouteTargetY - exploreRouteFromY,
        exploreRouteDirection);
    exploreRouteMoveStartedMs = nowMs;
    nextExploreRouteFrameMs = nowMs;
    exploreRouteMoving = true;
    return true;
}

void AmoledApp::updateExploreRoute(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE ||
        exploreRoutePaused ||
        exploreRouteComplete) {
        return;
    }
    if (!exploreRouteMoving) {
        if (exploreRouteAutoWalk) beginExploreRouteStep(nowMs);
        return;
    }

    uint32_t elapsed = nowMs - exploreRouteMoveStartedMs;
    float progress = std::min(
        1.0f, elapsed / static_cast<float>(EXPLORE_ROUTE_STEP_MS));
    exploreRouteWorldX = exploreRouteFromX +
        (exploreRouteTargetX - exploreRouteFromX) * progress;
    exploreRouteWorldY = exploreRouteFromY +
        (exploreRouteTargetY - exploreRouteFromY) * progress;
    updateExploreRouteCamera();

    if (static_cast<int32_t>(nowMs - nextExploreRouteFrameMs) >= 0) {
        exploreRoutePetFrame = static_cast<uint8_t>(
            exploreRoutePetFrame + 1);
        nextExploreRouteFrameMs = nowMs + EXPLORE_ROUTE_FRAME_MS;
        requestRenderRows(HOME_HEADER_HEIGHT, 224);
    }

    if (progress < 1.0f) return;
    exploreRouteWorldX = exploreRouteTargetX;
    exploreRouteWorldY = exploreRouteTargetY;
    exploreRouteMoving = false;
    ++exploreRouteSteps;
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (exploreRouteIndex + 1 >= path.pointCount) {
        exploreRouteComplete = true;
        exploreRouteAutoWalk = false;
    } else if (exploreRouteAutoWalk) {
        beginExploreRouteStep(nowMs);
    }
    requestRenderRows(HOME_HEADER_HEIGHT, 224);
}

void AmoledApp::updateExploreRouteCamera() {
    int maximumX = std::max(0, EXPLORE_ROUTE_WORLD_WIDTH - 184);
    int maximumY = std::max(0,
        EXPLORE_ROUTE_WORLD_HEIGHT - EXPLORE_ROUTE_VIEW_HEIGHT);
    int cameraX = static_cast<int>(std::lround(exploreRouteWorldX)) - 92;
    int cameraY = static_cast<int>(std::lround(exploreRouteWorldY)) -
                  EXPLORE_ROUTE_VIEW_HEIGHT / 2;
    exploreRouteCameraX = static_cast<int16_t>(
        std::clamp(cameraX, 0, maximumX));
    exploreRouteCameraY = static_cast<int16_t>(
        std::clamp(cameraY, 0, maximumY));
}

void AmoledApp::pauseExploreRoute(uint32_t nowMs) {
    if (exploreRoutePaused) return;
    exploreRoutePaused = true;
    exploreRoutePausedAtMs = nowMs;
}

void AmoledApp::resumeExploreRoute(uint32_t nowMs) {
    if (!exploreRoutePaused) return;
    if (exploreRouteMoving) {
        exploreRouteMoveStartedMs += nowMs - exploreRoutePausedAtMs;
    }
    exploreRoutePaused = false;
    exploreRoutePausedAtMs = 0;
    nextExploreRouteFrameMs = nowMs;
}

void AmoledApp::leaveExploreRoute() {
    exploreRouteMoving = false;
    exploreRouteAutoWalk = false;
    exploreRoutePaused = false;
    exploreRouteExitConfirm = false;
    sceneFlow.leaveExploreRoute();
    requestFullRender();
}

void AmoledApp::updateClockAndCare(uint32_t nowMs) {
    uint16_t previousMinuteOfDay = static_cast<uint16_t>(
        gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
    bool previousNight = previousMinuteOfDay < 6U * 60U ||
                         previousMinuteOfDay >= 18U * 60U;
    bool clockChanged =
        gameClock.sync(nowMs, gameSpeed(), gameState.gameMinutesTotal);
    if (clockChanged) {
        Game::resetDailyCareCounters(gameState);
        requestRenderRows(0, HOME_HEADER_HEIGHT);
        uint16_t minuteOfDay = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        bool night = minuteOfDay < 6U * 60U || minuteOfDay >= 18U * 60U;
        if (night != previousNight) {
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
    }

    uint32_t elapsedMs = nowMs - lastCareMs;
    if (elapsedMs >= CARE_TICK_MS) {
        uint32_t elapsedMinutes = elapsedMs / CARE_TICK_MS;
        lastCareMs += elapsedMinutes * CARE_TICK_MS;
        Game::applyCareMinutes(gameState, careAcc, elapsedMinutes,
                               gameSpeed(), true);
        requestRenderRows(HOME_STATUS_TOP, 224);
    }

    if ((clockChanged || elapsedMs >= CARE_TICK_MS) &&
        nowMs - lastPersistMs >= PERIODIC_SAVE_MS) {
        saveState();
        lastPersistMs = nowMs;
    }
}

void AmoledApp::updatePet(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME) {
        lastPetUpdateMs = nowMs;
        return;
    }
    if (gameState.teamCount == 0) return;
    Game::MonsterRuntime& monster = gameState.team[0];
    if (monster.fainted || monster.hpCur == 0) {
        petMotion = PetMotion::IDLE;
        return;
    }

    if (static_cast<int32_t>(nowMs - nextMindUpdateMs) >= 0) {
        const Game::SpeciesCareProfile care =
            Game::speciesCareProfileFor(monster.speciesId);
        bool sleepTime = care.usesBed && Game::isSleepCareTime(
            gameState.gameMinutesTotal, monster.nature);
        monsterMind.update(monster, sleepTime,
                           care.needsFood && gameState.room.bowlCount > 0,
                           nowMs);
        nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
    }

    float elapsedSeconds = static_cast<float>(nowMs - lastPetUpdateMs) / 1000.0f;
    lastPetUpdateMs = nowMs;
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);

    if (petMotion == PetMotion::EATING) {
        bool canContinue = gameState.room.bowlCount > 0 &&
                           monster.satiety < MONSTER_FEED_TARGET_SATIETY;
        if (canContinue &&
            static_cast<int32_t>(nowMs - nextFeedBiteMs) >= 0) {
            FoodConsumeResult result =
                Game::HomeCare::consumeBowlFood(gameState, 0);
            nextFeedBiteMs = nowMs + GameRandom::range(1000, 1601);
            if (result.consumed) {
                Platform::logf(
                    "[AmoledApp] auto-feed satiety=%u->%u bites=%u\n",
                    static_cast<unsigned>(result.satietyBefore),
                    static_cast<unsigned>(result.satietyAfter),
                    static_cast<unsigned>(gameState.room.bowlBitesRemaining));
                setToast("YUM", nowMs, 800);
                if (result.reaction == FoodReaction::LIKED ||
                    result.foodIndex == Game::ROOM_TASTY_FOOD_INDEX) {
                    heartsUntil = nowMs + 900;
                }
                saveState();
            } else {
                feedingUntilMs = nowMs;
            }
        }
        if (!canContinue ||
            static_cast<int32_t>(nowMs - feedingUntilMs) >= 0) {
            petMotion = PetMotion::IDLE;
            monsterMind.onAte(nowMs);
            schedulePetDecision(nowMs);
            requestRenderRows(HOME_ROOM_TOP, 224);
        }
        return;
    }

    if (petMotion == PetMotion::WANDERING ||
        petMotion == PetMotion::SEEKING_FOOD) {
        float dx = petTargetX - petX;
        float dy = petTargetY - petY;
        float distance = std::sqrt(dx * dx + dy * dy);
        float speed = (petMotion == PetMotion::SEEKING_FOOD ? 19.0f : 10.5f) *
                      behaviorProfile.moveSpeedScale;
        if (monster.mood < 40 || monster.satiety < 20) speed *= 0.72f;
        float step = speed * elapsedSeconds;
        if (distance <= 0.8f || step >= distance) {
            petX = petTargetX;
            petY = petTargetY;
            finishPetMove(nowMs);
        } else if (distance > 0.0f) {
            float nextX = petX + dx / distance * step;
            float nextY = petY + dy / distance * step;
            if (!petFootprintInsideWalkArea(nextX, nextY)) {
                Platform::logf(
                    "[AmoledApp] movement boundary stop pos=%.1f,%.1f target=%.1f,%.1f\n",
                    petX, petY, petTargetX, petTargetY);
                petMotion = PetMotion::IDLE;
                petTargetX = petX;
                petTargetY = petY;
                monsterMind.onActivity(nowMs);
                schedulePetDecision(nowMs);
            } else {
                petX = nextX;
                petY = nextY;
            }
        }
        updateCamera();
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
        return;
    }

    if (static_cast<int32_t>(nowMs - nextPetDecisionMs) < 0) return;
    if (monsterMind.topDesire() == MonsterDesire::EAT &&
        gameState.room.bowlCount > 0 &&
        monster.satiety < MONSTER_FEED_TARGET_SATIETY) {
        RoomResource& room = RoomResource::ins();
        float foodX = room.available()
            ? room.foodX() + FOOD_FEED_OFFSET_X
            : FALLBACK_FOOD_APPROACH_X;
        float foodY = room.available()
            ? room.foodY() + FOOD_FEED_OFFSET_Y
            : FALLBACK_FOOD_APPROACH_Y;
        float approachX = foodX;
        float approachY = foodY;
        if (chooseFoodApproachTarget(
                foodX, foodY, approachX, approachY) &&
            beginPetMove(
                PetMotion::SEEKING_FOOD, approachX, approachY, nowMs)) {
            return;
        }
    }

    if (monsterMind.topDesire() == MonsterDesire::WANDER &&
        behaviorProfile.movementMode == MonsterMovementMode::NORMAL) {
        float x = petX;
        float y = petY;
        if (chooseWanderTarget(x, y) &&
            beginPetMove(PetMotion::WANDERING, x, y, nowMs)) {
            return;
        }
    }
    monsterMind.onActivity(nowMs);
    schedulePetDecision(nowMs);
}

bool AmoledApp::beginPetMove(PetMotion motion, float x, float y,
                             uint32_t nowMs) {
    if (!petFootprintInsideWalkArea(x, y) ||
        !petPathInsideWalkArea(petX, petY, x, y)) {
        return false;
    }
    petMotion = motion;
    petTargetX = x;
    petTargetY = y;
    petFacingRight = petTargetX >= petX;
    petFrame = 0;
    nextPetFrameMs = nowMs;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

void AmoledApp::finishPetMove(uint32_t nowMs) {
    if (petMotion == PetMotion::SEEKING_FOOD) {
        petMotion = PetMotion::EATING;
        nextFeedBiteMs = nowMs + GameRandom::range(700, 1301);
        feedingUntilMs = nowMs + GameRandom::range(3200, 5601);
        petFrame = 0;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }
    petMotion = PetMotion::IDLE;
    monsterMind.onActivity(nowMs);
    schedulePetDecision(nowMs);
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::schedulePetDecision(uint32_t nowMs) {
    uint32_t minimum = behaviorProfile.idleMinMs;
    uint32_t maximum = behaviorProfile.idleMaxMs;
    if (maximum < minimum) maximum = minimum;
    nextPetDecisionMs = nowMs + GameRandom::range(minimum, maximum + 1);
}

void AmoledApp::updatePetFootprint() {
    PokemonSprites::WalkingAnimation animation{};
    if (!PokemonSprites::walkingAnimation(
            gameState.team[0].speciesId,
            PokemonSprites::WalkDirection::DOWN, animation) ||
        animation.frameCount == 0) {
        return;
    }
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            gameState.team[0].speciesId, animation.base);
    if (!frame) return;
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    petFootprintRadiusX = static_cast<float>(
        MathUtil::clamp(static_cast<int>(width * 0.24f), 7, 16));
    petFootprintRadiusY = static_cast<float>(
        MathUtil::clamp(static_cast<int>(height * 0.10f), 4, 8));
}

bool AmoledApp::petFootprintInsideWalkArea(float x, float y) const {
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    uint8_t count = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    RoomMovementArea::Footprint footprint = {
        petFootprintRadiusX, petFootprintRadiusY};
    return RoomMovementArea::containsFootprint(
        polygon, count, x, y, footprint);
}

bool AmoledApp::petPathInsideWalkArea(float fromX, float fromY,
                                      float toX, float toY) const {
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    uint8_t count = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    RoomMovementArea::Footprint footprint = {
        petFootprintRadiusX, petFootprintRadiusY};
    return RoomMovementArea::segmentInsideFootprint(
        polygon, count, fromX, fromY, toX, toY, footprint);
}

bool AmoledApp::chooseWanderTarget(float& x, float& y,
                                   bool requirePath) const {
    RoomResource& room = RoomResource::ins();
    int minimumX = room.available() ? room.walkMinX()
                                    : static_cast<int>(FALLBACK_ROOM_MIN_X);
    int maximumX = room.available() ? room.walkMaxX()
                                    : static_cast<int>(FALLBACK_ROOM_MAX_X);
    int minimumY = room.available() ? room.walkMinY()
                                    : static_cast<int>(FALLBACK_ROOM_MIN_Y);
    int maximumY = room.available() ? room.walkMaxY()
                                    : static_cast<int>(FALLBACK_ROOM_MAX_Y);
    for (uint8_t attempt = 0; attempt < 40; ++attempt) {
        int candidateX;
        int candidateY;
        if (attempt < 24) {
            int radiusX = std::max<int>(1, behaviorProfile.wanderRadiusX);
            int radiusY = std::max<int>(1, behaviorProfile.wanderRadiusY);
            int rawX = static_cast<int>(std::lround(petX)) +
                       static_cast<int>(GameRandom::random(
                           -radiusX, radiusX + 1));
            int rawY = static_cast<int>(std::lround(petY)) +
                       static_cast<int>(GameRandom::random(
                           -radiusY, radiusY + 1));
            candidateX = std::clamp(rawX, minimumX, maximumX);
            candidateY = std::clamp(rawY, minimumY, maximumY);
        } else {
            candidateX = GameRandom::random(minimumX, maximumX + 1);
            candidateY = GameRandom::random(minimumY, maximumY + 1);
        }
        if (!petFootprintInsideWalkArea(candidateX, candidateY)) continue;
        if (requirePath &&
            !petPathInsideWalkArea(petX, petY, candidateX, candidateY)) {
            continue;
        }
        if (std::fabs(candidateX - petX) < 8.0f &&
            std::fabs(candidateY - petY) < 4.0f) {
            continue;
        }
        x = static_cast<float>(candidateX);
        y = static_cast<float>(candidateY);
        return true;
    }
    return false;
}

bool AmoledApp::chooseFoodApproachTarget(float desiredX, float desiredY,
                                         float& x, float& y) const {
    auto tryCandidate = [&](float candidateX, float candidateY) {
        if (!petFootprintInsideWalkArea(candidateX, candidateY) ||
            !petPathInsideWalkArea(
                petX, petY, candidateX, candidateY)) {
            return false;
        }
        x = candidateX;
        y = candidateY;
        return true;
    };

    if (tryCandidate(desiredX, desiredY)) return true;
    for (int radius = 2; radius <= 36; radius += 2) {
        int yRadius = std::min(radius, 24);
        for (int dy = -yRadius; dy <= yRadius; dy += 2) {
            if (tryCandidate(desiredX + radius, desiredY + dy) ||
                tryCandidate(desiredX - radius, desiredY + dy)) {
                return true;
            }
        }
        for (int dx = -radius + 2; dx <= radius - 2; dx += 2) {
            if (tryCandidate(desiredX + dx, desiredY + yRadius) ||
                tryCandidate(desiredX + dx, desiredY - yRadius)) {
                return true;
            }
        }
    }
    return false;
}

void AmoledApp::updateCamera() {
    RoomResource& room = RoomResource::ins();
    if (!room.available()) {
        cameraX = 0.0f;
        cameraY = 0.0f;
        return;
    }

    float screenX = petX - cameraX;
    if (screenX < CAMERA_SAFE_LEFT) cameraX = petX - CAMERA_SAFE_LEFT;
    else if (screenX > CAMERA_SAFE_RIGHT) cameraX = petX - CAMERA_SAFE_RIGHT;

    float screenY = petY - cameraY;
    if (screenY < CAMERA_SAFE_TOP) cameraY = petY - CAMERA_SAFE_TOP;
    else if (screenY > CAMERA_SAFE_BOTTOM) cameraY = petY - CAMERA_SAFE_BOTTOM;

    float maximumCameraX = std::max<float>(0.0f,
        static_cast<float>(room.width() - HOME_ROOM_WIDTH));
    float minimumCameraY = static_cast<float>(room.roomY());
    float maximumCameraY = std::max<float>(minimumCameraY,
        static_cast<float>(room.roomY() + room.height() - HOME_ROOM_HEIGHT));
    cameraX = std::clamp(cameraX, 0.0f, maximumCameraX);
    cameraY = std::clamp(cameraY, minimumCameraY, maximumCameraY);
}

int AmoledApp::worldToScreenX(float worldX) const {
    if (!RoomResource::ins().available()) {
        return static_cast<int>(std::lround(worldX));
    }
    return static_cast<int>(std::lround(worldX - cameraX));
}

int AmoledApp::worldToScreenY(float worldY) const {
    if (!RoomResource::ins().available()) {
        return static_cast<int>(std::lround(worldY));
    }
    return HOME_ROOM_TOP +
           static_cast<int>(std::lround(worldY - cameraY));
}

void AmoledApp::requestRenderRows(uint16_t begin, uint16_t end) {
    begin = std::min<uint16_t>(begin, 224);
    end = std::min<uint16_t>(std::max<uint16_t>(end, begin), 224);
    if (begin == end) return;
    if (!dirtyRowsValid) {
        dirtyRowBegin = begin;
        dirtyRowEnd = end;
        dirtyRowsValid = true;
        return;
    }
    dirtyRowBegin = std::min(dirtyRowBegin, begin);
    dirtyRowEnd = std::max(dirtyRowEnd, end);
}

void AmoledApp::requestFullRender() {
    requestRenderRows(0, 224);
}

float AmoledApp::gameSpeed() const {
    static constexpr float SPEEDS[] = {1.0f, 2.0f, 4.0f, 8.0f};
    uint8_t index = gameState.settings.speedIndex;
    return SPEEDS[index < 4 ? index : 0];
}

void AmoledApp::render(Canvas565& canvas) const {
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        const Game::MonsterRuntime& monster = gameState.team[0];
        HomeViewModel model;
        model.speciesId = monster.speciesId;
        uint8_t visibleSlots[Game::TEAM_CAP] = {};
        model.monsterCount = Game::HomeHud::visibleTeamSlots(
            gameState, visibleSlots);
        for (uint8_t index = 0; index < model.monsterCount; ++index) {
            const Game::MonsterRuntime& hudMonster =
                gameState.team[visibleSlots[index]];
            model.monsters[index].hp =
                Game::HomeHud::hpPercent(hudMonster);
            model.monsters[index].hunger =
                Game::HomeHud::hungerPercent(hudMonster);
        }
        model.gameMinutesOfDay = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        model.cameraX = static_cast<int16_t>(std::lround(cameraX));
        model.cameraY = static_cast<int16_t>(std::lround(cameraY));
        model.petCenterX = static_cast<int16_t>(worldToScreenX(petX));
        model.petGroundY = static_cast<int16_t>(worldToScreenY(petY));
        RoomResource& room = RoomResource::ins();
        if (room.available()) {
            model.bowlCenterX = static_cast<int16_t>(
                worldToScreenX(room.foodX()));
            model.bowlCenterY = static_cast<int16_t>(
                worldToScreenY(room.foodY()));
        }
        model.petWalking = petMotion == PetMotion::WANDERING ||
                           petMotion == PetMotion::SEEKING_FOOD;
        model.petFacingRight = petFacingRight;
        model.petFrame = petFrame;
        model.night = model.gameMinutesOfDay < 6U * 60U ||
                      model.gameMinutesOfDay >= 18U * 60U;
        model.showHearts = heartsUntil != 0;
        model.bowlFilled = gameState.room.bowlCount > 0;
        model.toast = toast;
        renderHomeScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        ExploreViewModel model;
        model.scroll = exploreScroll;
        model.visibleAreaCount =
            ExploreItemProgression::visibleAreaCount(gameState);
        model.unlockedArea = ExploreItemProgression::unlockedArea(gameState);
        model.selectedArea = selectedExploreArea;
        model.currentLevel = gameState.team[0].level;
        model.pressedArea = pressedExploreArea;
        model.toast = toast;
        renderExploreScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        ExploreRouteViewModel model;
        model.map = &exploreRouteMap;
        model.speciesId = gameState.team[0].speciesId;
        model.area = selectedExploreArea;
        model.pathIndex = exploreRoutePath;
        model.routeIndex = exploreRouteIndex;
        if (exploreRoutePath < exploreRouteMap.pathCount) {
            model.routePointCount =
                exploreRouteMap.paths[exploreRoutePath].pointCount;
        }
        model.walkDirection = exploreRouteDirection;
        model.petFrame = exploreRoutePetFrame;
        model.steps = exploreRouteSteps;
        model.worldX = exploreRouteWorldX;
        model.worldY = exploreRouteWorldY;
        model.cameraX = exploreRouteCameraX;
        model.cameraY = exploreRouteCameraY;
        model.walking = exploreRouteMoving && !exploreRoutePaused;
        model.autoWalk = exploreRouteAutoWalk && !exploreRoutePaused;
        model.complete = exploreRouteComplete;
        model.exitConfirm = exploreRouteExitConfirm;
        renderExploreRouteScreen(
            canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        ExploreMenuViewModel model;
        model.pressedItem = pressedExploreMenuItem;
        model.toast = toast;
        renderExploreMenuScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM) {
        TeamViewModel model;
        model.state = &gameState;
        model.pressedSlot = pressedTeamSlot;
        model.confirmOpen = teamConfirmOpen;
        model.pendingSlot = pendingTeamSlot;
        model.toast = toast;
        renderTeamScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
        RoomMenuViewModel model;
        model.state = &gameState;
        model.pressedItem = pressedRoomItem;
        model.toast = toast;
        renderRoomMenuScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
        ShowerViewModel model;
        model.state = &gameState;
        model.mode = showerMode;
        model.speciesId = gameState.team[0].speciesId;
        model.soapIndex = showerSoapIndex;
        model.soapProgress = showerSoapProgress;
        model.brushProgress = showerBrushProgress;
        model.rinseProgress = showerRinseProgress;
        model.completionHearts = showerCompletionHearts;
        model.toolX = showerToolX;
        model.toolY = showerToolY;
        model.pressedItem = pressedShowerItem;
        model.toolDragging = showerToolDragging;
        model.exitConfirmYes = showerExitConfirmYes;
        model.toast = toast;
        renderShowerScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
        shopCategoryView) {
        ShopCategoryViewModel model;
        model.state = &gameState;
        model.coins = gameState.coins;
        model.category = shopCategory;
        model.pressedItem = pressedShopCategory;
        model.toast = toast;
        renderShopCategoryScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BAG ||
        sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        ItemListViewModel model;
        model.state = &gameState;
        model.mode = sceneFlow.current() == AppSceneFlow::Scene::BAG
            ? ItemListMode::BAG
            : shopCategory == Game::ShopService::Category::SELL
                ? ItemListMode::SELL : ItemListMode::BUY;
        model.category = shopCategory;
        model.scroll = itemScroll;
        model.itemCount = currentItemCount();
        model.coins = gameState.coins;
        model.pressedItem = pressedItemRow;
        model.confirmOpen = itemConfirmOpen;
        model.pendingItem = pendingItem;
        model.toast = toast;
        if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
            renderShopItemScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        } else {
            renderItemListScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        }
        return;
    }

    MenuViewModel model;
    model.scroll = menuScroll;
    model.pressedItem = pressedMenuItem;
    model.toast = toast;
    renderMainMenu(canvas, model, dirtyRowBegin, dirtyRowEnd);
}

bool AmoledApp::consumeLockRequest() {
    bool requested = lockRequested;
    lockRequested = false;
    return requested;
}

void AmoledApp::onWake() {
    requestFullRender();
}

void AmoledApp::setToast(const char* value, uint32_t nowMs,
                         uint32_t durationMs) {
    toast = value;
    toastUntil = nowMs + durationMs;
    requestRenderRows(
        sceneFlow.current() == AppSceneFlow::Scene::HOME
            ? HOME_ROOM_TOP : MENU_HEADER_HEIGHT,
        sceneFlow.current() == AppSceneFlow::Scene::HOME
            ? HOME_STATUS_TOP : 224);
}

void AmoledApp::clampMenuScroll() {
    float maximum = mainMenuMaxScroll();
    if (menuScroll <= 0.0f) {
        menuScroll = 0.0f;
        if (menuVelocity < 0.0f) menuVelocity = 0.0f;
    } else if (menuScroll >= maximum) {
        menuScroll = maximum;
        if (menuVelocity > 0.0f) menuVelocity = 0.0f;
    }
}

void AmoledApp::clampExploreScroll() {
    float maximum = exploreMaxScroll(
        ExploreItemProgression::visibleAreaCount(gameState));
    if (exploreScroll <= 0.0f) {
        exploreScroll = 0.0f;
        if (exploreVelocity < 0.0f) exploreVelocity = 0.0f;
    } else if (exploreScroll >= maximum) {
        exploreScroll = maximum;
        if (exploreVelocity > 0.0f) exploreVelocity = 0.0f;
    }
}

void AmoledApp::clampItemScroll() {
    float maximum = sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                            !shopCategoryView
        ? shopItemMaxScroll(currentItemCount())
        : itemListMaxScroll(currentItemCount());
    if (itemScroll <= 0.0f) {
        itemScroll = 0.0f;
        if (itemVelocity < 0.0f) itemVelocity = 0.0f;
    } else if (itemScroll >= maximum) {
        itemScroll = maximum;
        if (itemVelocity > 0.0f) itemVelocity = 0.0f;
    }
}

void AmoledApp::snapShopItemScroll() {
    int selected = shopSelectedItem(itemScroll, currentItemCount());
    float target = selected < 0
        ? 0.0f : shopItemScrollForIndex(static_cast<uint8_t>(selected));
    if (std::fabs(itemScroll - target) <= 0.01f) return;
    itemScroll = target;
    requestRenderRows(MENU_HEADER_HEIGHT, 224);
}

uint8_t AmoledApp::currentItemCount() const {
    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        return Game::ItemInventory::homeBagItemCount(gameState);
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::SHOP ||
        shopCategoryView) {
        return 0;
    }
    return shopCategory == Game::ShopService::Category::SELL
        ? Game::ShopService::sellItemCount(gameState)
        : Game::ShopService::buyItemCount(shopCategory, gameState);
}

Game::ItemId AmoledApp::currentItemAt(uint8_t index) const {
    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        return Game::ItemInventory::homeBagItemAt(gameState, index);
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::SHOP ||
        shopCategoryView) {
        return Game::ItemId::COUNT;
    }
    return shopCategory == Game::ShopService::Category::SELL
        ? Game::ShopService::sellItemAt(gameState, index)
        : Game::ShopService::buyItemAt(shopCategory, gameState, index);
}

void AmoledApp::openItemScene(AppSceneFlow::Scene target) {
    sceneFlow.openSubScene(target);
    itemScroll = 0.0f;
    itemVelocity = 0.0f;
    pressedItemRow = -1;
    pressedShopCategory = -1;
    shopCategoryView = true;
    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    teamConfirmOpen = false;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::closeItemScene() {
    itemVelocity = 0.0f;
    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    teamConfirmOpen = false;
    pressedTeamSlot = -1;
    toast = nullptr;
    sceneFlow.closeSubScene();
    requestFullRender();
}

void AmoledApp::openRoomScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::ROOM);
    pressedRoomItem = -1;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::openShowerScene(uint32_t nowMs) {
    sceneFlow.enter(AppSceneFlow::Scene::SHOWER);
    resetShowerSession(nowMs);
    requestFullRender();
}

void AmoledApp::closeShowerScene() {
    showerToolDragging = false;
    pressedShowerItem = -1;
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::ROOM);
    requestFullRender();
}

void AmoledApp::startShowerSoap(uint8_t soapIndex, uint32_t nowMs) {
    if (soapIndex >= Game::SOAP_VARIANT_COUNT ||
        !Game::BathService::consumeSoap(gameState, soapIndex)) {
        setToast("NO SOAP", nowMs);
        return;
    }

    showerSoapIndex = soapIndex;
    showerSoapConsumed = true;
    saveState();
    startShowerTool(ShowerMode::SOAPING, nowMs);
}

void AmoledApp::startShowerTool(ShowerMode mode, uint32_t nowMs) {
    if (mode != ShowerMode::SOAPING && mode != ShowerMode::BRUSHING) return;

    showerMode = mode;
    showerModeStartedMs = nowMs;
    showerToolDragging = false;
    showerStrokeCarry = 0.0f;
    showerToolX = mode == ShowerMode::SOAPING ? 24 : 69;
    showerToolY = 196;
    showerLastStrokeX = showerToolX;
    showerLastStrokeY = showerToolY;
    pressedShowerItem = -1;
    toast = nullptr;
    requestRenderRows(MENU_HEADER_HEIGHT, 224);
}

void AmoledApp::updateShowerToolDrag(int x, int y, uint32_t nowMs) {
    if (!showerToolDragging ||
        (showerMode != ShowerMode::SOAPING &&
         showerMode != ShowerMode::BRUSHING)) {
        return;
    }

    x = std::clamp(x, SHOWER_TOOL_MIN_X, SHOWER_TOOL_MAX_X);
    y = std::clamp(y, SHOWER_TOOL_MIN_Y, SHOWER_TOOL_MAX_Y);
    bool previousInside = showerLastStrokeX >= SHOWER_BODY_LEFT &&
                          showerLastStrokeX <= SHOWER_BODY_RIGHT &&
                          showerLastStrokeY >= SHOWER_BODY_TOP &&
                          showerLastStrokeY <= SHOWER_BODY_BOTTOM;
    bool currentInside = x >= SHOWER_BODY_LEFT && x <= SHOWER_BODY_RIGHT &&
                         y >= SHOWER_BODY_TOP && y <= SHOWER_BODY_BOTTOM;
    if (previousInside && currentInside) {
        float dx = static_cast<float>(x - showerLastStrokeX);
        float dy = static_cast<float>(y - showerLastStrokeY);
        showerStrokeCarry += std::sqrt(dx * dx + dy * dy);
    }

    showerToolX = static_cast<int16_t>(x);
    showerToolY = static_cast<int16_t>(y);
    showerLastStrokeX = showerToolX;
    showerLastStrokeY = showerToolY;

    uint8_t& progress = showerMode == ShowerMode::SOAPING
        ? showerSoapProgress : showerBrushProgress;
    while (showerStrokeCarry >= SHOWER_PROGRESS_DISTANCE &&
           progress < SHOWER_PROGRESS_MAX) {
        showerStrokeCarry -= SHOWER_PROGRESS_DISTANCE;
        ++progress;
    }

    requestRenderRows(MENU_HEADER_HEIGHT, 224);
    if (progress < SHOWER_PROGRESS_MAX) return;

    ShowerMode completedMode = showerMode;
    showerToolDragging = false;
    grantShowerStage(completedMode == ShowerMode::SOAPING
                         ? Game::BathService::Stage::SOAP
                         : Game::BathService::Stage::BRUSH,
                     nowMs);
    showerMode = ShowerMode::MENU;
    requestRenderRows(MENU_HEADER_HEIGHT, 224);
}

void AmoledApp::grantShowerStage(Game::BathService::Stage stage,
                                 uint32_t nowMs) {
    bool* rewarded = nullptr;
    switch (stage) {
    case Game::BathService::Stage::SOAP:
        rewarded = &showerSoapRewarded;
        break;
    case Game::BathService::Stage::BRUSH:
        rewarded = &showerBrushRewarded;
        break;
    case Game::BathService::Stage::RINSE:
        rewarded = &showerRinseRewarded;
        break;
    }
    if (!rewarded || *rewarded) return;

    *rewarded = true;
    Game::BathService::RewardResult reward =
        Game::BathService::applyStageReward(gameState, stage);
    Game::MonsterRuntime& monster = gameState.team[0];
    uint8_t oldLevel = monster.level;
    uint16_t oldHpMax = monster.hpMax;
    if (reward.experience > 0) {
        if (const Species* species = findSpecies(monster.speciesId)) {
            uint32_t maxExp = minimumExpForLevel(
                species->growthRate, Game::LEVEL_MAX);
            monster.exp = static_cast<uint32_t>(std::min<uint64_t>(
                static_cast<uint64_t>(maxExp),
                static_cast<uint64_t>(monster.exp) + reward.experience));
            monster.level = std::max<uint8_t>(
                oldLevel, levelForExp(species->growthRate, monster.exp));
            monster.hpMax = maxHpFor(*species, monster);
            if (monster.hpMax > oldHpMax) {
                monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
                    monster.hpMax,
                    static_cast<uint32_t>(monster.hpCur) +
                        monster.hpMax - oldHpMax));
            }
            if (monster.level > oldLevel) {
                gameState.pendingLevelUp = true;
                gameState.pendingLevelUpLevel = monster.level;
            }
        }
    }

    if (reward.experience > 0 && reward.moodGain > 0) {
        std::snprintf(showerToast, sizeof(showerToast), "+%u EXP  +%u MOOD",
                      reward.experience, reward.moodGain);
    } else if (reward.experience > 0) {
        std::snprintf(showerToast, sizeof(showerToast), "+%u EXP",
                      reward.experience);
    } else if (reward.moodGain > 0) {
        std::snprintf(showerToast, sizeof(showerToast), "+%u MOOD",
                      reward.moodGain);
    } else {
        std::snprintf(showerToast, sizeof(showerToast), "CARE LIMIT");
    }
    setToast(showerToast, nowMs, 1300);
    saveState();
}

void AmoledApp::startShowerRinse(uint32_t nowMs) {
    showerMode = ShowerMode::RINSING;
    showerModeStartedMs = nowMs;
    showerLastFrameMs = nowMs;
    showerRinseProgress = 0;
    showerToolDragging = false;
    pressedShowerItem = -1;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::requestShowerExit() {
    showerToolDragging = false;
    pressedShowerItem = -1;
    if (showerSoapConsumed && !showerRinseRewarded) {
        showerMode = ShowerMode::EXIT_CONFIRM;
        showerExitConfirmYes = false;
        toast = nullptr;
        requestRenderRows(MENU_HEADER_HEIGHT, 224);
        return;
    }
    closeShowerScene();
}

void AmoledApp::resetShowerSession(uint32_t nowMs) {
    showerMode = ShowerMode::MENU;
    showerSoapIndex = 0;
    showerSoapProgress = 0;
    showerBrushProgress = 0;
    showerRinseProgress = 0;
    showerCompletionHearts = 0;
    showerToolX = 24;
    showerToolY = 196;
    showerLastStrokeX = showerToolX;
    showerLastStrokeY = showerToolY;
    showerStrokeCarry = 0.0f;
    showerToolDragging = false;
    showerSoapConsumed = false;
    showerSoapRewarded = false;
    showerBrushRewarded = false;
    showerRinseRewarded = false;
    showerExitConfirmYes = false;
    showerModeStartedMs = nowMs;
    showerLastFrameMs = nowMs;
    pressedShowerItem = -1;
    showerToast[0] = '\0';
    toast = nullptr;
}

void AmoledApp::openTeamScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::TEAM);
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    uint8_t count = Game::TeamRoster::memberCount(gameState);
    for (uint8_t slot = 0; slot < count; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, count);
    pressedTeamSlot = -1;
    pendingTeamSlot = 0;
    teamConfirmOpen = false;
    itemConfirmOpen = false;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::switchTeamLeader(uint32_t nowMs) {
    bool changed = pendingTeamSlot > 0 &&
        Game::TeamRoster::moveToFront(gameState, pendingTeamSlot);
    teamConfirmOpen = false;
    pendingTeamSlot = 0;
    if (!changed) {
        setToast("CANNOT SWITCH", nowMs);
        return;
    }

    uint16_t speciesIds[Game::TEAM_CAP] = {};
    uint8_t count = Game::TeamRoster::memberCount(gameState);
    for (uint8_t slot = 0; slot < count; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, count);
    if (const Species* species = findSpecies(gameState.team[0].speciesId)) {
        behaviorProfile = behaviorProfileFor(*species, gameState.team[0]);
    }
    petMotion = PetMotion::IDLE;
    petTargetX = petX;
    petTargetY = petY;
    monsterMind.reset(nowMs);
    nextMindUpdateMs = nowMs;
    schedulePetDecision(nowMs);
    updatePetFootprint();
    updateCamera();
    saveState();
    setToast("LEADER CHANGED", nowMs);
}

void AmoledApp::performPendingItemAction(uint32_t nowMs) {
    const char* resultText = "NO EFFECT";
    bool changed = false;
    if (pendingItemAction == PendingItemAction::BUY) {
        switch (Game::ShopService::buy(gameState, pendingItem)) {
        case Game::ShopService::BuyResult::BOUGHT:
            resultText = "ITEM BOUGHT";
            changed = true;
            break;
        case Game::ShopService::BuyResult::LOCKED:
            resultText = "ITEM LOCKED";
            break;
        case Game::ShopService::BuyResult::NOT_ENOUGH_COINS:
            resultText = "NOT ENOUGH COINS";
            break;
        case Game::ShopService::BuyResult::BAG_FULL:
            resultText = "BAG FULL";
            break;
        case Game::ShopService::BuyResult::DAILY_LIMIT:
            resultText = "DAILY LIMIT";
            break;
        case Game::ShopService::BuyResult::INVALID_ITEM:
            resultText = "INVALID ITEM";
            break;
        }
    } else if (pendingItemAction == PendingItemAction::SELL) {
        switch (Game::ShopService::sell(gameState, pendingItem)) {
        case Game::ShopService::SellResult::SOLD:
            resultText = "ITEM SOLD";
            changed = true;
            break;
        case Game::ShopService::SellResult::NO_STOCK:
            resultText = "NO STOCK";
            break;
        case Game::ShopService::SellResult::INVALID_ITEM:
            resultText = "INVALID ITEM";
            break;
        }
    } else if (pendingItemAction == PendingItemAction::USE) {
        uint8_t target = Game::ItemInventory::preferredTarget(
            gameState, pendingItem);
        switch (Game::ItemInventory::useOnTeam(
                    gameState, pendingItem, target)) {
        case Game::ItemInventory::UseResult::USED:
            resultText = "ITEM USED";
            changed = true;
            break;
        case Game::ItemInventory::UseResult::NO_STOCK:
            resultText = "NO STOCK";
            break;
        case Game::ItemInventory::UseResult::INVALID_TARGET:
            resultText = "NO TARGET";
            break;
        case Game::ItemInventory::UseResult::FAINTED:
            resultText = "MON FAINTED";
            break;
        case Game::ItemInventory::UseResult::HP_FULL:
            resultText = "HP FULL";
            break;
        case Game::ItemInventory::UseResult::STATUS_NORMAL:
            resultText = "STATUS NORMAL";
            break;
        case Game::ItemInventory::UseResult::NO_FAINTED_TARGET:
            resultText = "NO FAINTED MON";
            break;
        case Game::ItemInventory::UseResult::NOT_USABLE:
            resultText = "MIGRATION NEXT";
            break;
        }
    }

    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    if (changed) saveState();
    clampItemScroll();
    setToast(resultText, nowMs);
}

bool AmoledApp::saveState() {
    if (!storageReady) return false;
    bool saved = saveManager.saveSnapshot(gameState, mainViewState);
    if (!saved) Platform::logLine("[AmoledApp] save failed");
    return saved;
}

}  // namespace AmoledV1
