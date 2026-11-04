#pragma once

#include <cstdint>
#include <cmath>
#include "assets/PokemonSprites.h"
#include "game/ExploreDaylight.h"
#include "game/ExploreDestinations.h"
#include "game/ExploreMapGenerator.h"
#include "game/ExplorePool.h"
#include "game/Species.h"
#include "presentation/MenuGrid.h"

namespace Ui {
class ExploreRouteOverlay;

namespace ExploreRouteTiming {
constexpr uint32_t PREVIEW_CYCLE_MS = 2800;
constexpr uint32_t PREVIEW_MOVE_MS = 500;
constexpr uint32_t PREVIEW_HOLD_MS = PREVIEW_CYCLE_MS - PREVIEW_MOVE_MS;
}

struct ExploreRouteLayout {
    int width = 240;
    int height = 135;
    bool centeredPrompts = false;
    bool feetOnRouteCenter = false;
    // Fraction of road height below the route center; zero preserves the anchor.
    float routeGroundBias = 0.0f;
    // Decorative edge tiles fill viewports taller than the authored map.
    bool extendMapEdges = false;
    float actorScale = 0.8f;
    static constexpr int AREA_LIST_WIDTH = 70;
    static constexpr int AREA_ROW_SPACING = 34;
    static constexpr float AREA_VISIBLE_DISTANCE = 2.25f;

    int areaRowCenter(int index, float cursor) const {
        return height / 2 + static_cast<int>(roundf((index - cursor) * AREA_ROW_SPACING));
    }
    MenuGrid::Rect specialPrompt() const { return prompt(22, 24, 196, 88); }
    MenuGrid::Rect pickupPrompt() const { return prompt(36, 40, 168, 54); }
    MenuGrid::Rect endPrompt() const { return prompt(34, 35, 172, 66); }
    MenuGrid::Rect resultPrompt() const {
        return {(width - 156) / 2, (height - 58) / 2, 156, 58};
    }
    MenuGrid::Rect sideMenuItem(int index) const {
        return {width - 60, index * 30, 60, 30};
    }
private:
    MenuGrid::Rect prompt(int x, int y, int w, int h) const {
        return {centeredPrompts ? (width - w) / 2 : x,
                centeredPrompts ? (height - h) / 2 : y, w, h};
    }
};

struct ExploreRouteVisualState {
    bool areaBackgroundTraced = false;
    uint8_t routeVisualWalkDirection = 0;
    uint8_t routeFollowerVisualWalkDirection = 0;
};

// A view of the current scene. Map and sprite resources remain host-owned.
struct ExploreRouteFrame {
    static constexpr uint8_t AREA_CAP = ExploreDestinations::COUNT;
    static constexpr uint8_t PREVIEW_CAP = ExplorePool::POOL_CAP;
    uint32_t nowMs = 0;
    uint8_t phaseDebug = 0;
    uint8_t areaCursor = 0;
    float areaAnimCursor = 0;
    uint8_t visibleAreaCount = 0;
    const char* areaNames[AREA_CAP] = {};
    bool areaUnlocked[AREA_CAP] = {};
    bool areaSpecialActive[AREA_CAP] = {};
    uint32_t areaPreviewStartedAt = 0;
    uint8_t areaPreviewCount = 0;
    bool areaPreviewHasRare = false;
    bool marketPreview = false;
    const PokemonSprites::SpriteFrame* areaPreviewFrames[PREVIEW_CAP] = {};
    bool areaPreviewHidden[PREVIEW_CAP] = {};

    const ExploreMapGenerator::Map* generatedMap = nullptr;
    const ExploreRouteOverlay* overlay = nullptr;
    uint8_t currentRoutePath = 0;
    const char* mapName = nullptr;
    uint16_t fieldColor = 0;
    struct TeamHudRow {
        uint16_t hpCur = 0, hpMax = 0;
        Game::MajorStatus status = Game::MajorStatus::NONE;
        bool fainted = false;
    };
    TeamHudRow teamHud[2];
    uint8_t teamHudCount = 0;
    bool menuAvailable = false;
    ExploreDaylight::Tone daylight;
    const Species* leader = nullptr;
    const Species* follower = nullptr;
    const Species* boss = nullptr;
    float routeWorldX = 0, routeWorldY = 0;
    float routeFollowerWorldX = 0, routeFollowerWorldY = 0;
    uint8_t routeWalkDirection = 0, routeFollowerWalkDirection = 0;
    bool routeMoving = false, routeFollowerMoving = false;
    bool iceSliding = false, exiting = false;
    uint8_t routeIndex = 0, routeFollowerIndex = 0;
    uint32_t routeMoveStarted = 0;
    uint16_t routeLeaderMoveDurationMs = 0, routeFollowerMoveDurationMs = 0;
    uint16_t routeFollowerDelayMs = 120;
    uint16_t leaderExitStepDurationMs = 1, followerExitStepDurationMs = 1;
    bool routeBossPending = false;
    uint8_t routeBossIndex = 0;
    bool routePickupAvailable = false;
    uint8_t routePickupIndex = 0;
    bool exploreMenuOpen = false;
    uint8_t exploreMenuCursor = 0;
    uint32_t exploreMenuOpenedAt = 0;
    bool specialChallengeYes = true;
    const char* resultMessage = nullptr;
    const char* toast = nullptr;
    const char* wildName = nullptr;
    uint8_t adventureBondGain = 0;
};

}  // namespace Ui
