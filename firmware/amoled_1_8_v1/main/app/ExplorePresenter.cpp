#include "app/ExplorePresenter.h"

#include <algorithm>
#include <cmath>

#include "HomeScreen.h"
#include "ui/ExploreMapRenderer.h"
#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "game/ExploreRouteGeometry.h"

namespace AmoledV1 {
namespace {

constexpr uint16_t EXPLORE_ROUTE_STEP_MS = 360;
constexpr uint16_t EXPLORE_ROUTE_FRAME_MS = 90;
constexpr uint16_t EXPLORE_ROUTE_FOLLOWER_DELAY_MS = 120;
constexpr uint8_t EXPLORE_ROUTE_FOLLOWER_GAP_STEPS = 2;
constexpr float EXPLORE_ROUTE_EXIT_MARGIN =
    ExploreRouteGeometry::TILE_SIZE * 3.0f;
constexpr float EXPLORE_ROUTE_EXIT_SPEED = 42.0f;
constexpr uint16_t EXPLORE_ROUTE_MAP_FRAME_MS = 280;
constexpr uint16_t EXPLORE_ROUTE_BOSS_FRAME_MS = 140;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_CYCLE_MS = 6000;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS = 1600;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS = 2300;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS = 3800;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS = 4500;
constexpr int EXPLORE_ROUTE_VIEW_HEIGHT = UiMetrics::HOME_STATUS_TOP;
constexpr int EXPLORE_ROUTE_WORLD_WIDTH =
    ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE;
constexpr int EXPLORE_ROUTE_WORLD_HEIGHT =
    ExploreMapGenerator::HEIGHT * ExploreRouteGeometry::TILE_SIZE;

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

bool exploreRouteBossPatrolMoving(uint32_t nowMs) {
    const uint32_t phase = nowMs % EXPLORE_ROUTE_BOSS_PATROL_CYCLE_MS;
    return (phase >= EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS &&
            phase < EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS) ||
           (phase >= EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS &&
            phase < EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS);
}


}  // namespace

void ExplorePresenter::reset() {
    state_.exploreEcology = ExploreEcology::Simulation{};
    damage_ = {};
    invalidateDrawn();
}

void ExplorePresenter::updateDaylight(uint32_t gameMinutes, uint8_t area) {
    const auto tone = ExploreDaylight::at(gameMinutes, area);
    if (tone == state_.exploreRouteDaylight) return;
    state_.exploreRouteDaylight = tone;
    requestRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
}

void ExplorePresenter::startRun(uint32_t nowMs) {
    state_.exploreRouteExitStartedMs = state_.exploreRouteExitDurationMs = 0;
    state_.exploreRoutePetFrame = state_.exploreRouteMapFrame = 0;
    state_.nextExploreRouteFrameMs = nowMs;
    state_.nextExploreRouteMapFrameMs = nowMs + EXPLORE_ROUTE_MAP_FRAME_MS;
    state_.nextExploreRouteBossFrameMs = nowMs;
    state_.exploreRouteBossWasMoving = false;
}

void ExplorePresenter::resetMap(const ExploreSession::State& route,
    const Game::GameState& gameState, uint8_t area, uint8_t hiddenPoolMask, uint32_t nowMs) {
    const auto& path = route.exploreRouteMap.paths[route.exploreRoutePath];
    state_.exploreRouteFollowerMoving = false;
    state_.exploreRouteDirection = exploreInwardDirection(route.exploreRouteMap.entry.edge);
    ExploreRouteGeometry::WorldPoint start =
        ExploreRouteGeometry::pathPoint(path, 0);
    state_.exploreRouteWorldX = state_.exploreRouteFromX = state_.exploreRouteTargetX = start.x;
    state_.exploreRouteWorldY = state_.exploreRouteFromY = state_.exploreRouteTargetY = start.y;
    if (route.exploreRouteIndex != 0) {
        const auto landing = ExploreRouteGeometry::pathPoint(path, route.exploreRouteIndex);
        state_.exploreRouteWorldX = state_.exploreRouteFromX = state_.exploreRouteTargetX = landing.x;
        state_.exploreRouteWorldY = state_.exploreRouteFromY = state_.exploreRouteTargetY = landing.y;
    }
    state_.exploreRouteFollowerWorldX = state_.exploreRouteFollowerFromX =
        state_.exploreRouteFollowerTargetX = start.x;
    state_.exploreRouteFollowerWorldY = state_.exploreRouteFollowerFromY =
        state_.exploreRouteFollowerTargetY = start.y;
    state_.exploreRouteFollowerDirection = state_.exploreRouteDirection;
    const float followerOffset =
        ExploreRouteGeometry::TILE_SIZE * 0.75f;
    switch (static_cast<PokemonSprites::WalkDirection>(
                state_.exploreRouteFollowerDirection)) {
    case PokemonSprites::WalkDirection::LEFT:
        state_.exploreRouteFollowerWorldX += followerOffset;
        break;
    case PokemonSprites::WalkDirection::UP:
        state_.exploreRouteFollowerWorldY += followerOffset;
        break;
    case PokemonSprites::WalkDirection::RIGHT:
        state_.exploreRouteFollowerWorldX -= followerOffset;
        break;
    case PokemonSprites::WalkDirection::DOWN:
    default:
        state_.exploreRouteFollowerWorldY -= followerOffset;
        break;
    }
    state_.exploreRouteFollowerWorldX = std::clamp(
        state_.exploreRouteFollowerWorldX, 0.0f,
        static_cast<float>(ExploreMapGenerator::WIDTH *
                           ExploreRouteGeometry::TILE_SIZE));
    state_.exploreRouteFollowerWorldY = std::clamp(
        state_.exploreRouteFollowerWorldY, 0.0f,
        static_cast<float>(ExploreMapGenerator::HEIGHT *
                           ExploreRouteGeometry::TILE_SIZE));
    state_.exploreRouteFollowerFromX = state_.exploreRouteFollowerTargetX =
        state_.exploreRouteFollowerWorldX;
    state_.exploreRouteFollowerFromY = state_.exploreRouteFollowerTargetY =
        state_.exploreRouteFollowerWorldY;
    state_.exploreRouteFollowerFrame = 0;
    state_.nextExploreRouteBossFrameMs = nowMs;
    state_.exploreRouteBossWasMoving = false;
    updateCamera();
    state_.exploreEcology.reset(route.exploreRouteMap, route.exploreRoutePath, route.exploreRoutePool,
                         hiddenPoolMask, nowMs);
    state_.exploreGrass.reset(nowMs);
    state_.exploreRouteDaylight = ExploreDaylight::at(gameState.gameMinutesTotal, area);
    invalidateDrawn();
    damage_ = {};
}

void ExplorePresenter::beginStep(const ExploreSession::State& route, uint32_t nowMs) {
    const auto& path = route.exploreRouteMap.paths[route.exploreRoutePath];
    state_.exploreRouteFromX = state_.exploreRouteWorldX;
    state_.exploreRouteFromY = state_.exploreRouteWorldY;
    state_.exploreRouteFollowerFromX = state_.exploreRouteFollowerWorldX;
    state_.exploreRouteFollowerFromY = state_.exploreRouteFollowerWorldY;
    ExploreRouteGeometry::WorldPoint target =
        ExploreRouteGeometry::pathPoint(path, route.exploreRouteIndex);
    state_.exploreRouteTargetX = target.x;
    state_.exploreRouteTargetY = target.y;
    state_.exploreRouteFollowerTargetX = state_.exploreRouteFollowerFromX;
    state_.exploreRouteFollowerTargetY = state_.exploreRouteFollowerFromY;
    if (route.exploreRouteIndex >= EXPLORE_ROUTE_FOLLOWER_GAP_STEPS) {
        ExploreRouteGeometry::WorldPoint followerTarget =
            ExploreRouteGeometry::pathPoint(
                path, static_cast<uint8_t>(
                          route.exploreRouteIndex - EXPLORE_ROUTE_FOLLOWER_GAP_STEPS));
        state_.exploreRouteFollowerTargetX = followerTarget.x;
        state_.exploreRouteFollowerTargetY = followerTarget.y;
    }
    state_.exploreRouteDirection = exploreDirectionForDelta(
        state_.exploreRouteTargetX - state_.exploreRouteFromX,
        state_.exploreRouteTargetY - state_.exploreRouteFromY,
        state_.exploreRouteDirection);
    state_.exploreRouteFollowerDirection = exploreDirectionForDelta(
        state_.exploreRouteFollowerTargetX - state_.exploreRouteFollowerFromX,
        state_.exploreRouteFollowerTargetY - state_.exploreRouteFollowerFromY,
        state_.exploreRouteFollowerDirection);
    state_.exploreRouteMoveStartedMs = nowMs;
    state_.nextExploreRouteFrameMs = nowMs;
    state_.exploreRouteFollowerMoving =
        std::fabs(state_.exploreRouteFollowerTargetX - state_.exploreRouteFollowerFromX) >= 0.01f ||
        std::fabs(state_.exploreRouteFollowerTargetY - state_.exploreRouteFollowerFromY) >= 0.01f;
}

void ExplorePresenter::beginExit(const ExploreSession::State& route, bool hasFollower, uint32_t nowMs) {
    const ExploreMapGenerator::Path& path =
        route.exploreRouteMap.paths[route.exploreRoutePath];
    state_.exploreRouteFromX = state_.exploreRouteWorldX;
    state_.exploreRouteFromY = state_.exploreRouteWorldY;
    state_.exploreRouteTargetX = state_.exploreRouteWorldX;
    state_.exploreRouteTargetY = state_.exploreRouteWorldY;
    switch (path.exit.edge) {
    case ExploreMapGenerator::Edge::TOP:
        state_.exploreRouteTargetY = -EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::RIGHT:
        state_.exploreRouteTargetX = EXPLORE_ROUTE_WORLD_WIDTH + EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::BOTTOM:
        state_.exploreRouteTargetY = EXPLORE_ROUTE_WORLD_HEIGHT + EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::LEFT:
        state_.exploreRouteTargetX = -EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    }
    state_.exploreRouteFollowerFromX = state_.exploreRouteFollowerWorldX;
    state_.exploreRouteFollowerFromY = state_.exploreRouteFollowerWorldY;
    state_.exploreRouteFollowerTargetX = state_.exploreRouteTargetX;
    state_.exploreRouteFollowerTargetY = state_.exploreRouteTargetY;
    state_.exploreRouteDirection = exploreDirectionForDelta(
        state_.exploreRouteTargetX - state_.exploreRouteFromX,
        state_.exploreRouteTargetY - state_.exploreRouteFromY,
        state_.exploreRouteDirection);
    state_.exploreRouteFollowerDirection = exploreDirectionForDelta(
        state_.exploreRouteFollowerTargetX - state_.exploreRouteFollowerFromX,
        state_.exploreRouteFollowerTargetY - state_.exploreRouteFollowerFromY,
        state_.exploreRouteFollowerDirection);
    const float leaderDistance = std::hypot(
        state_.exploreRouteTargetX - state_.exploreRouteFromX,
        state_.exploreRouteTargetY - state_.exploreRouteFromY);
    const float followerDistance = std::hypot(
        state_.exploreRouteFollowerTargetX - state_.exploreRouteFollowerFromX,
        state_.exploreRouteFollowerTargetY - state_.exploreRouteFollowerFromY);
    const float longestDistance = hasFollower
        ? std::max(leaderDistance, followerDistance) : leaderDistance;
    state_.exploreRouteExitDurationMs = static_cast<uint32_t>(std::max(
        500.0f, longestDistance / EXPLORE_ROUTE_EXIT_SPEED * 1000.0f));
    state_.exploreRouteExitStartedMs = nowMs;
    state_.exploreRouteMoveStartedMs = nowMs;
    state_.nextExploreRouteFrameMs = nowMs;
    state_.exploreRouteFollowerMoving = hasFollower && followerDistance > 0.01f;
}

bool ExplorePresenter::advanceWalkFrames(const ExploreSession::State& route, uint32_t nowMs) {
    if (!route.exploreRouteMoving ||
        static_cast<int32_t>(nowMs - state_.nextExploreRouteFrameMs) < 0) {
        return false;
    }
    state_.exploreRoutePetFrame = static_cast<uint8_t>(state_.exploreRoutePetFrame + 1);
    if (state_.exploreRouteFollowerMoving) {
        state_.exploreRouteFollowerFrame = static_cast<uint8_t>(
            state_.exploreRouteFollowerFrame + 1);
    }
    state_.nextExploreRouteFrameMs = nowMs + EXPLORE_ROUTE_FRAME_MS;
    return true;
}

void ExplorePresenter::updateEcology(const ExploreSession::State& route,
    const Game::GameState& gameState, uint8_t area, bool active, uint32_t nowMs) {
    ExploreEcology::Pose before[ExploreEcology::CAPACITY];
    for (uint8_t i = 0; i < state_.exploreEcology.count(); ++i)
        before[i] = state_.exploreEcology.pose(i);
    state_.exploreEcology.update(nowMs, active, state_.exploreRouteWorldX, state_.exploreRouteWorldY,
        {float(state_.exploreRouteCameraX), float(state_.exploreRouteCameraY),
         AmoledUi::WIDTH / float(AmoledUi::RESOURCE_SCALE),
         EXPLORE_ROUTE_VIEW_HEIGHT / float(AmoledUi::RESOURCE_SCALE)});
    ExploreGrass::Frame grassBefore[ExploreMapGenerator::CELL_COUNT];
    for (uint16_t cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell)
        grassBefore[cell] = state_.exploreGrass.frame(cell);
    ExploreGrass::Contact contacts[ExploreGrass::ACTORS]{};
    const auto grounded = [](uint16_t species) {
        return ExploreEcology::locomotionForSpecies(species) == ExploreEcology::Locomotion::GROUND;
    };
    const uint8_t leaderSlot = ExploreSession::leaderSlot(gameState);
    contacts[0] = {state_.exploreRouteWorldX, exploreRouteActorFootWorldY(state_.exploreRouteWorldY),
        grounded(gameState.team[leaderSlot].speciesId)};
    contacts[1] = {state_.exploreRouteFollowerWorldX, exploreRouteActorFootWorldY(state_.exploreRouteFollowerWorldY),
        ExploreSession::hasCompanion(gameState) && grounded(gameState.team[1].speciesId)};
    for (uint8_t i = 0; i < state_.exploreEcology.count(); ++i) {
        const auto& pose = state_.exploreEcology.pose(i);
        contacts[i + 2] = {pose.x, pose.y,
            !pose.hidden && !pose.airborne && !pose.absent && pose.lift <= 0 && grounded(pose.speciesId)};
    }
    state_.exploreGrass.update(route.exploreRouteMap, nowMs, active, contacts, ExploreGrass::ACTORS);
    if (!active) return;
    constexpr int grassTile = ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    for (uint16_t cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        const auto frame = state_.exploreGrass.frame(cell);
        if (grassBefore[cell] == frame) continue;
        const bool high = frame.kind == ExploreGrass::Kind::HIGH ||
            grassBefore[cell].kind == ExploreGrass::Kind::HIGH;
        const int x = (int(cell % ExploreMapGenerator::WIDTH) * ExploreRouteGeometry::TILE_SIZE -
            state_.exploreRouteCameraX) * AmoledUi::RESOURCE_SCALE;
        const int y = (int(cell / ExploreMapGenerator::WIDTH) * ExploreRouteGeometry::TILE_SIZE -
            state_.exploreRouteCameraY) * AmoledUi::RESOURCE_SCALE;
        requestRect(x, std::max(0, y - (high ? grassTile : 0)), x + grassTile,
            std::min(EXPLORE_ROUTE_VIEW_HEIGHT, y + grassTile));
    }
    auto model = this->model(route, gameState, area, nowMs);
    for (uint8_t i = 0; i < state_.exploreEcology.count(); ++i) {
        if (ExploreEcology::samePose(before[i], state_.exploreEcology.pose(i))) continue;
        const auto current = exploreRouteWildlifeBounds(model, i);
        model.wildlife[i] = before[i];
        const auto previous = exploreRouteWildlifeBounds(model, i);
        model.wildlife[i] = state_.exploreEcology.pose(i);
        if (!previous.width || !current.width) {
            const auto& visible = previous.width ? previous : current;
            if (visible.width) requestRect(visible.x, std::max(0, visible.y),
                visible.x + visible.width,
                std::min(EXPLORE_ROUTE_VIEW_HEIGHT, visible.y + visible.height));
            continue;
        }
        requestRect(std::min(previous.x, current.x),
            std::max(0, std::min(previous.y, current.y)),
            std::max(previous.x + previous.width, current.x + current.width),
            std::min(EXPLORE_ROUTE_VIEW_HEIGHT,
                std::max(previous.y + previous.height, current.y + current.height)));
    }
}

ExplorePresenter::Motion ExplorePresenter::updateMotion(const ExploreSession::State& route,
    const Game::GameState& gameState, uint8_t area, uint32_t nowMs) {
    if (route.exploreRoutePaused || route.exploreRouteComplete ||
        route.exploreRoutePendingFrostFall) return Motion::NONE;
    if (route.exploreRouteExiting) {
        advanceWalkFrames(route, nowMs);
        const uint32_t elapsed = nowMs - state_.exploreRouteExitStartedMs;
        const float progress = std::min(
            1.0f, elapsed / static_cast<float>(
                std::max<uint32_t>(1, state_.exploreRouteExitDurationMs)));
        state_.exploreRouteWorldX = state_.exploreRouteFromX +
            (state_.exploreRouteTargetX - state_.exploreRouteFromX) * progress;
        state_.exploreRouteWorldY = state_.exploreRouteFromY +
            (state_.exploreRouteTargetY - state_.exploreRouteFromY) * progress;
        if (state_.exploreRouteFollowerMoving) {
            state_.exploreRouteFollowerWorldX = state_.exploreRouteFollowerFromX +
                (state_.exploreRouteFollowerTargetX - state_.exploreRouteFollowerFromX) * progress;
            state_.exploreRouteFollowerWorldY = state_.exploreRouteFollowerFromY +
                (state_.exploreRouteFollowerTargetY - state_.exploreRouteFollowerFromY) * progress;
        }
        const int16_t previousCameraX = state_.exploreRouteCameraX;
        const int16_t previousCameraY = state_.exploreRouteCameraY;
        updateCamera();
        if (previousCameraX != state_.exploreRouteCameraX ||
            previousCameraY != state_.exploreRouteCameraY) {
            requestRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
        } else {
            requestDynamicDamage(model(route, gameState, area, nowMs));
        }
        if (progress < 1.0f) return Motion::NONE;
        state_.exploreRouteWorldX = state_.exploreRouteTargetX;
        state_.exploreRouteWorldY = state_.exploreRouteTargetY;
        state_.exploreRouteFollowerWorldX = state_.exploreRouteFollowerTargetX;
        state_.exploreRouteFollowerWorldY = state_.exploreRouteFollowerTargetY;
        state_.exploreRouteFollowerMoving = false;
        return Motion::EXIT_FINISHED;
    }
    if (route.exploreRouteBossPending &&
        static_cast<int32_t>(nowMs - state_.nextExploreRouteBossFrameMs) >= 0) {
        const bool bossMoving = exploreRouteBossPatrolMoving(nowMs);
        if (bossMoving || state_.exploreRouteBossWasMoving) {
            requestBossDamage(route);
        }
        state_.exploreRouteBossWasMoving = bossMoving;
        state_.nextExploreRouteBossFrameMs = nowMs + EXPLORE_ROUTE_BOSS_FRAME_MS;
    }
    if (!route.exploreRouteMoving) return Motion::NONE;

    uint32_t elapsed = nowMs - state_.exploreRouteMoveStartedMs;
    float progress = std::min(
        1.0f, elapsed / static_cast<float>(EXPLORE_ROUTE_STEP_MS));
    state_.exploreRouteWorldX = state_.exploreRouteFromX +
        (state_.exploreRouteTargetX - state_.exploreRouteFromX) * progress;
    state_.exploreRouteWorldY = state_.exploreRouteFromY +
        (state_.exploreRouteTargetY - state_.exploreRouteFromY) * progress;
    if (state_.exploreRouteFollowerMoving) {
        uint32_t followerElapsed = elapsed > EXPLORE_ROUTE_FOLLOWER_DELAY_MS
            ? elapsed - EXPLORE_ROUTE_FOLLOWER_DELAY_MS : 0;
        float followerProgress = elapsed > EXPLORE_ROUTE_FOLLOWER_DELAY_MS
            ? std::min(1.0f, followerElapsed /
                static_cast<float>(EXPLORE_ROUTE_STEP_MS)) : 0.0f;
        state_.exploreRouteFollowerWorldX = state_.exploreRouteFollowerFromX +
            (state_.exploreRouteFollowerTargetX - state_.exploreRouteFollowerFromX) *
                followerProgress;
        state_.exploreRouteFollowerWorldY = state_.exploreRouteFollowerFromY +
            (state_.exploreRouteFollowerTargetY - state_.exploreRouteFollowerFromY) *
                followerProgress;
    }
    int16_t previousCameraX = state_.exploreRouteCameraX;
    int16_t previousCameraY = state_.exploreRouteCameraY;
    updateCamera();
    bool cameraMoved = previousCameraX != state_.exploreRouteCameraX ||
                       previousCameraY != state_.exploreRouteCameraY;
    if (cameraMoved) {
        // The cached map is keyed by viewport. Rebuild the complete map band
        // when the camera scrolls instead of compositing two viewports.
        requestRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
    }

    // Position interpolation runs every UI tick; walking art retains its own cadence.
    advanceWalkFrames(route, nowMs);
    if (!cameraMoved) requestDynamicDamage(model(route, gameState, area, nowMs));

    if (static_cast<int32_t>(nowMs - state_.nextExploreRouteMapFrameMs) >= 0) {
        do {
            state_.exploreRouteMapFrame = static_cast<uint8_t>(
                state_.exploreRouteMapFrame + 1);
            state_.nextExploreRouteMapFrameMs += EXPLORE_ROUTE_MAP_FRAME_MS;
        } while (static_cast<int32_t>(nowMs - state_.nextExploreRouteMapFrameMs) >= 0);
        if (route.exploreRouteMap.hasCreek || route.exploreRouteMap.hasWaterfall ||
            route.exploreRouteMap.hasCoast) {
            requestMapAnimationDamage(route);
        }
    }

    if (progress < 1.0f) return Motion::NONE;
    state_.exploreRouteWorldX = state_.exploreRouteTargetX;
    state_.exploreRouteWorldY = state_.exploreRouteTargetY;
    state_.exploreRouteFollowerWorldX = state_.exploreRouteFollowerTargetX;
    state_.exploreRouteFollowerWorldY = state_.exploreRouteFollowerTargetY;
    state_.exploreRouteFollowerMoving = false;
    return Motion::STEP_FINISHED;
}

ExploreRouteViewModel ExplorePresenter::model(const ExploreSession::State& route,
    const Game::GameState& gameState, uint8_t area, uint32_t nowMs,
    uint8_t fadeAlpha, const char* toast) const {
    ExploreRouteViewModel model;
    model.grass = &state_.exploreGrass;
    model.daylight = state_.exploreRouteDaylight;
    model.wildlifeCount = state_.exploreEcology.count();
    for (uint8_t i = 0; i < model.wildlifeCount; ++i)
        model.wildlife[i] = state_.exploreEcology.pose(i);
    model.map = &route.exploreRouteMap;
    model.state = &gameState;
    const uint8_t leaderSlot = ExploreSession::leaderSlot(gameState);
    model.speciesId = gameState.team[leaderSlot].speciesId;
    model.area = area;
    model.pathIndex = route.exploreRoutePath;
    model.routeIndex = route.exploreRouteIndex;
    if (route.exploreRoutePath < route.exploreRouteMap.pathCount) {
        model.routePointCount =
            route.exploreRouteMap.paths[route.exploreRoutePath].pointCount;
    }
    model.walkDirection = state_.exploreRouteDirection;
    model.petFrame = state_.exploreRoutePetFrame;
    model.mapFrame = state_.exploreRouteMapFrame;
    model.animationNowMs = nowMs;
    model.steps = route.exploreRouteSteps;
    model.worldX = state_.exploreRouteWorldX;
    model.worldY = state_.exploreRouteWorldY;
    model.companionVisible = ExploreSession::hasCompanion(gameState);
    model.companionSpeciesId = model.companionVisible
        ? gameState.team[1].speciesId : 0;
    model.companionWorldX = state_.exploreRouteFollowerWorldX;
    model.companionWorldY = state_.exploreRouteFollowerWorldY;
    model.companionWalkDirection = state_.exploreRouteFollowerDirection;
    model.companionFrame = state_.exploreRouteFollowerFrame;
    model.companionWalking = state_.exploreRouteFollowerMoving;
    model.cameraX = state_.exploreRouteCameraX;
    model.cameraY = state_.exploreRouteCameraY;
    model.walking = route.exploreRouteMoving && !route.exploreRoutePaused;
    model.autoWalk = route.exploreRouteAutoWalk && !route.exploreRoutePaused;
    model.sliding = route.exploreRouteIceSliding;
    model.complete = route.exploreRouteComplete;
    model.bossPending = route.exploreRouteBossPending;
    model.bossIndex = route.exploreRouteBossIndex;
    model.bossSpeciesId = route.exploreRouteBossSpeciesId;
    model.fadeAlpha = fadeAlpha;
    model.exitConfirm = route.exploreRouteExitConfirm;
    model.pickupIndex = route.exploreRoutePickupIndex;
    model.pickupItem = route.exploreRoutePickupItem;
    model.pickupAvailable = route.exploreRoutePickupAvailable;
    model.toast = toast;
    return model;
}

void ExplorePresenter::requestDynamicDamage(const ExploreRouteViewModel& model) {
    if (!drawnValid_ || drawnCameraX_ != state_.exploreRouteCameraX ||
        drawnCameraY_ != state_.exploreRouteCameraY || drawnMapSeed_ != model.map->seed) {
        requestRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
        return;
    }
    const auto current = exploreRouteActorBounds(model);
    const auto previous = drawnBounds_;
    requestRect(std::min(previous.x, current.x),
        std::min(previous.y, current.y),
        std::max(previous.x + previous.width, current.x + current.width),
        std::min(EXPLORE_ROUTE_VIEW_HEIGHT,
            std::max(previous.y + previous.height, current.y + current.height)));
}

void ExplorePresenter::requestBossDamage(const ExploreSession::State& route) {
    if (!route.exploreRouteBossPending ||
        route.exploreRoutePath >= route.exploreRouteMap.pathCount) {
        return;
    }
    const ExploreMapGenerator::Path& path =
        route.exploreRouteMap.paths[route.exploreRoutePath];
    if (route.exploreRouteBossIndex >= path.pointCount) return;

    const ExploreRouteGeometry::WorldPoint point =
        ExploreRouteGeometry::pathPoint(path, route.exploreRouteBossIndex);
    const int centerY = static_cast<int>(std::lround(
        (point.y - state_.exploreRouteCameraY) * AmoledUi::RESOURCE_SCALE)) +
        EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y;
    // Covers the largest 2x walking frames, airborne bobbing, shadow, and the
    // complete 5px patrol path so the cached map clears every previous pose.
    constexpr int BOSS_TOP_MARGIN = 112;
    constexpr int BOSS_BOTTOM_MARGIN = 88;
    if (centerY + BOSS_BOTTOM_MARGIN <= 0 ||
        centerY - BOSS_TOP_MARGIN >= EXPLORE_ROUTE_VIEW_HEIGHT) {
        return;
    }
    const int top = std::clamp(centerY - BOSS_TOP_MARGIN, 0,
                               EXPLORE_ROUTE_VIEW_HEIGHT - 1);
    const int bottom = std::clamp(centerY + BOSS_BOTTOM_MARGIN, top + 1,
                                  EXPLORE_ROUTE_VIEW_HEIGHT);
    requestRows(static_cast<uint16_t>(top),
                      static_cast<uint16_t>(bottom));
}

void ExplorePresenter::requestMapAnimationDamage(const ExploreSession::State& route) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    constexpr int mapTop = 0;
    constexpr int mapBottom = EXPLORE_ROUTE_VIEW_HEIGHT;
    int top = mapBottom;
    int bottom = mapTop;

    for (uint8_t layer = 0;
         layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (uint8_t tileY = 0; tileY < ExploreMapGenerator::HEIGHT; ++tileY) {
            for (uint8_t tileX = 0; tileX < ExploreMapGenerator::WIDTH;
                 ++tileX) {
                uint16_t tileId = route.exploreRouteMap.layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (!GameAssets::isExploreTileAnimated(tileId)) continue;
                int tileTop = mapTop + static_cast<int>(tileY) * tileSize -
                              state_.exploreRouteCameraY * AmoledUi::RESOURCE_SCALE;
                int tileBottom = tileTop + tileSize;
                top = std::min(top, tileTop);
                bottom = std::max(bottom, tileBottom);
            }
        }
    }

    top = std::clamp(top, mapTop, mapBottom);
    bottom = std::clamp(bottom, top, mapBottom);
    if (top < bottom) {
        requestRows(static_cast<uint16_t>(top),
                          static_cast<uint16_t>(bottom));
    }
}

void ExplorePresenter::updateCamera() {
    int maximumX = std::max(0, EXPLORE_ROUTE_WORLD_WIDTH - AmoledUi::WIDTH / AmoledUi::RESOURCE_SCALE);
    int maximumY = std::max(0,
        EXPLORE_ROUTE_WORLD_HEIGHT - EXPLORE_ROUTE_VIEW_HEIGHT / AmoledUi::RESOURCE_SCALE);
    int cameraX = static_cast<int>(std::lround(state_.exploreRouteWorldX)) - 92;
    int cameraY = static_cast<int>(std::lround(state_.exploreRouteWorldY)) -
                  EXPLORE_ROUTE_VIEW_HEIGHT / (2 * AmoledUi::RESOURCE_SCALE);
    const int16_t nextCameraX = static_cast<int16_t>(
        std::clamp(cameraX, 0, maximumX));
    const int16_t nextCameraY = static_cast<int16_t>(
        std::clamp(cameraY, 0, maximumY));
    state_.exploreRouteCameraX = nextCameraX;
    state_.exploreRouteCameraY = nextCameraY;
}

void ExplorePresenter::resume(const ExploreSession::State& route, uint32_t paused, uint32_t nowMs) {
    if (route.exploreRouteMoving) state_.exploreRouteMoveStartedMs += paused;
    if (route.exploreRouteExiting) state_.exploreRouteExitStartedMs += paused;
    state_.nextExploreRouteMapFrameMs += paused;
    state_.nextExploreRouteBossFrameMs = nowMs;
    state_.nextExploreRouteFrameMs = nowMs;
}

void ExplorePresenter::requestRows(int top, int bottom) {
    requestRect(0, top, AmoledUi::WIDTH, bottom);
}

void ExplorePresenter::requestRect(int left, int top, int right, int bottom) {
    left = std::clamp(left, 0, int(AmoledUi::WIDTH));
    right = std::clamp(right, left, int(AmoledUi::WIDTH));
    top = std::clamp(top, 0, EXPLORE_ROUTE_VIEW_HEIGHT);
    bottom = std::clamp(bottom, top, EXPLORE_ROUTE_VIEW_HEIGHT);
    if (left >= right || top >= bottom) return;
    if (damage_.valid) {
        left = std::min(left, int(damage_.bounds.x));
        top = std::min(top, int(damage_.bounds.y));
        right = std::max(right, damage_.bounds.x + damage_.bounds.width);
        bottom = std::max(bottom, damage_.bounds.y + damage_.bounds.height);
    }
    damage_.valid = true;
    damage_.bounds = {left, top, right - left, bottom - top};
}

ExplorePresenter::Damage ExplorePresenter::takeDamage() {
    const auto result = damage_;
    damage_ = {};
    return result;
}

void ExplorePresenter::recordDrawn(const ExploreRouteViewModel& model) const {
    drawnBounds_ = exploreRouteActorBounds(model);
    drawnCameraX_ = model.cameraX;
    drawnCameraY_ = model.cameraY;
    drawnMapSeed_ = model.map ? model.map->seed : 0;
    drawnValid_ = model.map != nullptr;
}

}  // namespace AmoledV1
