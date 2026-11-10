#include "game/ExploreDestinations.h"
#include "game/MarketMapData.h"
#include "ui/common/MarketView.h"
#include "scenes/ExploreScene.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "assets/GameAssets.h"
#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/AudioManager.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/ProgressionUi.h"
#include "core/UiStrings.h"
#include "ui/SceneSurface.h"
#include "presentation/ExploreBattleTiming.h"
#include "presentation/ExploreRouteMotion.h"
#include "game/BattleSystem.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreDaylight.h"
#include "game/ExploreBoss.h"
#include "game/ExploreBossPity.h"
#include "game/ExploreEncounters.h"
#include "game/ExploreEncounterRules.h"
#include "game/ExploreIceSlide.h"
#include "game/ExploreItemProgression.h"
#include "game/ExplorePartyService.h"
#include "game/ExplorePool.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreRunRules.h"
#include "game/ExplorePickupRules.h"
#include "game/FriendshipPity.h"
#include "game/FriendshipService.h"
#include "game/FriendshipSystem.h"
#include "game/GameRandom.h"
#include "hardware/Hal.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"

namespace {
namespace Pickup = ExplorePickupRules;
using EncounterEntry = ExploreEncounters::Entry;
using ExploreRunRules::cooldownAfterCompletedStep;
using ExploreRunRules::cooldownAfterCompletedSteps;
using ExploreRunRules::encounterGateOpen;
using ExploreRunRules::encounterGateAllows;
using ExploreRunRules::repelAllowsEncounter;
using ExploreRunRules::canScheduleGuaranteedEncounter;
using ExploreRunRules::guaranteedEncounterIndex;
using ExploreRunRules::MAP_PICKUP_CHANCE;
using ExploreRunRules::MAP_GENERATION_SAFE_SEED;
using ExploreRunRules::MAP_GENERATION_RETRY_SALTS;

static constexpr uint8_t WILD_LEVEL_MIN = ExploreEncounterRules::WILD_LEVEL_MIN;
static constexpr uint8_t WILD_LEVEL_MAX = Game::LEVEL_MAX;
static constexpr uint8_t WILD_LEVEL_VARIANCE =
    ExploreEncounterRules::WILD_LEVEL_VARIANCE;
static constexpr uint8_t ENCOUNTER_COOLDOWN_STEP_COUNT = ExploreRunRules::COOLDOWN_STEPS;
using ExploreRunRules::MAX_ENCOUNTERS_PER_MAP;
static constexpr uint16_t ICE_SLIDE_STEP_MS = 180;
static constexpr uint32_t AREA_PREVIEW_CYCLE_MS = Ui::ExploreRouteTiming::PREVIEW_CYCLE_MS;
static constexpr uint32_t AREA_PREVIEW_MOVE_MS = Ui::ExploreRouteTiming::PREVIEW_MOVE_MS;
static constexpr uint32_t AREA_PREVIEW_HOLD_MS =
    AREA_PREVIEW_CYCLE_MS - AREA_PREVIEW_MOVE_MS;
static constexpr uint32_t PREVIEW_LOAD_AFTER_CURSOR_MS = 80;
static constexpr uint32_t PREVIEW_BACKGROUND_LOAD_INTERVAL_MS = 80;

using namespace ExploreBattleTiming;
static constexpr uint8_t EXPLORE_MAP_TILES_W = 16;
static constexpr uint8_t EXPLORE_MAP_TILES_H = 12;
static constexpr uint16_t EXPLORE_TILE_SIZE =
    ExploreRouteGeometry::TILE_SIZE;
static constexpr uint16_t EXPLORE_MAP_W = EXPLORE_MAP_TILES_W * EXPLORE_TILE_SIZE;
static constexpr uint16_t EXPLORE_MAP_H = EXPLORE_MAP_TILES_H * EXPLORE_TILE_SIZE;
static constexpr float ROUTE_FOLLOWER_START_OFFSET = EXPLORE_TILE_SIZE;
static constexpr uint16_t ROUTE_FOLLOWER_DELAY_MS = 120;
static constexpr uint8_t ROUTE_FOLLOWER_GAP_STEPS = 2;
// Leave enough room for the widest scaled sprite to clear the viewport.
static constexpr float ROUTE_EXIT_MARGIN = EXPLORE_TILE_SIZE * 3.0f;

struct RouteMap {
    const char* name;
    const char* description;
    GameAssets::Kind battleBackground;
    uint8_t averageLevel;
    uint8_t depthSpread;
    uint8_t minMapCount;
    uint8_t maxMapCount;
    uint16_t encounterChance;
    const Pickup::Table& pickups;
    const EncounterEntry* encounters;
    uint8_t encounterCount;
    uint16_t fieldColor;
    uint16_t accentColor;
};

static constexpr auto& GRASS_PATH_ENCOUNTERS = ExploreEncounters::GRASS_PATH;
static constexpr auto& CREEK_SLOPE_ENCOUNTERS = ExploreEncounters::CREEK_SLOPE;
static constexpr auto& TALL_GRASS_PARK_ENCOUNTERS =
    ExploreEncounters::TALL_GRASS_PARK;
static constexpr auto& FROST_CRYSTAL_CAVE_ENCOUNTERS =
    ExploreEncounters::FROST_CRYSTAL_CAVE;
static constexpr auto& MIST_FOREST_PATH_ENCOUNTERS =
    ExploreEncounters::MIST_FOREST_PATH;
static constexpr auto& ANCIENT_WATERFALL_VALLEY_ENCOUNTERS =
    ExploreEncounters::ANCIENT_WATERFALL_VALLEY;

#if STICKMON_EXTERNAL_CONTENT
#define ENTRY_COUNT(value) uint8_t(value.size())
#else
#define ENTRY_COUNT(entriesValue) \
    static_cast<uint8_t>(sizeof(entriesValue) / sizeof(entriesValue[0]))

#endif
#if STICKMON_EXTERNAL_CONTENT
const RouteMap (&routeMaps())[6] {
    static const RouteMap maps[] = {
#else
static constexpr RouteMap ROUTE_MAPS[] = {
#endif
    {
        Ui::Explore::GRASS_PATH,
        Ui::Explore::AREA_DESCS[0],
        ExploreAreaCatalog::battleBackground(0),
        ExploreAreaCatalog::recommendedLevel(0),
        ExploreAreaCatalog::depthSpread(0),
        ExploreAreaCatalog::minMapCount(0),
        ExploreAreaCatalog::maxMapCount(0),
        ExploreAreaCatalog::encounterChance(0),
        Pickup::tableForArea(0),
        GRASS_PATH_ENCOUNTERS,
        ENTRY_COUNT(GRASS_PATH_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(0),
        0x5EEE,
    },
    {
        Ui::Explore::CREEK_SLOPE,
        Ui::Explore::AREA_DESCS[1],
        ExploreAreaCatalog::battleBackground(1),
        ExploreAreaCatalog::recommendedLevel(1),
        ExploreAreaCatalog::depthSpread(1),
        ExploreAreaCatalog::minMapCount(1),
        ExploreAreaCatalog::maxMapCount(1),
        ExploreAreaCatalog::encounterChance(1),
        Pickup::tableForArea(1),
        CREEK_SLOPE_ENCOUNTERS,
        ENTRY_COUNT(CREEK_SLOPE_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(1),
        0x4D38,
    },
    {
        Ui::Explore::TALL_GRASS_PARK,
        Ui::Explore::AREA_DESCS[2],
        ExploreAreaCatalog::battleBackground(2),
        ExploreAreaCatalog::recommendedLevel(2),
        ExploreAreaCatalog::depthSpread(2),
        ExploreAreaCatalog::minMapCount(2),
        ExploreAreaCatalog::maxMapCount(2),
        ExploreAreaCatalog::encounterChance(2),
        Pickup::tableForArea(2),
        TALL_GRASS_PARK_ENCOUNTERS,
        ENTRY_COUNT(TALL_GRASS_PARK_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(2),
        0x8EA9,
    },
    {
        Ui::Explore::FROST_CRYSTAL_CAVE,
        Ui::Explore::AREA_DESCS[3],
        ExploreAreaCatalog::battleBackground(3),
        ExploreAreaCatalog::recommendedLevel(3),
        ExploreAreaCatalog::depthSpread(3),
        ExploreAreaCatalog::minMapCount(3),
        ExploreAreaCatalog::maxMapCount(3),
        ExploreAreaCatalog::encounterChance(3),
        Pickup::tableForArea(3),
        FROST_CRYSTAL_CAVE_ENCOUNTERS,
        ENTRY_COUNT(FROST_CRYSTAL_CAVE_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(3),
        0x5D7F,
    },
    {
        Ui::Explore::MIST_FOREST_PATH,
        Ui::Explore::AREA_DESCS[4],
        ExploreAreaCatalog::battleBackground(4),
        ExploreAreaCatalog::recommendedLevel(4),
        ExploreAreaCatalog::depthSpread(4),
        ExploreAreaCatalog::minMapCount(4),
        ExploreAreaCatalog::maxMapCount(4),
        ExploreAreaCatalog::encounterChance(4),
        Pickup::tableForArea(4),
        MIST_FOREST_PATH_ENCOUNTERS,
        ENTRY_COUNT(MIST_FOREST_PATH_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(4),
        0x644D,
    },
    {
        Ui::Explore::ANCIENT_WATERFALL_VALLEY,
        Ui::Explore::AREA_DESCS[5],
        ExploreAreaCatalog::battleBackground(5),
        ExploreAreaCatalog::recommendedLevel(5),
        ExploreAreaCatalog::depthSpread(5),
        ExploreAreaCatalog::minMapCount(5),
        ExploreAreaCatalog::maxMapCount(5),
        ExploreAreaCatalog::encounterChance(5),
        Pickup::tableForArea(5),
        ANCIENT_WATERFALL_VALLEY_ENCOUNTERS,
        ENTRY_COUNT(ANCIENT_WATERFALL_VALLEY_ENCOUNTERS),
        ExploreAreaCatalog::fieldColor(5),
        0x54F7,
    },
};
#if STICKMON_EXTERNAL_CONTENT
    return maps;
}
#define ROUTE_MAPS routeMaps()
#endif
#undef ENTRY_COUNT

static constexpr uint8_t ROUTE_MAP_COUNT = sizeof(ROUTE_MAPS) / sizeof(ROUTE_MAPS[0]);
static_assert(ROUTE_MAP_COUNT == Game::EXPLORE_AREA_COUNT,
              "route tables and persistent pool counters must stay aligned");

const EncounterEntry* findEncounterEntry(const RouteMap& map, uint16_t speciesId) {
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        if (map.encounters[i].speciesId == speciesId) return &map.encounters[i];
    }
    return nullptr;
}

constexpr uint8_t mapCountForRoll(const RouteMap& map, uint8_t roll) {
    return ExploreRunRules::mapCountForRoll(map.minMapCount, map.maxMapCount, roll);
}

template <size_t N>
constexpr bool routeLevelsStrictlyIncrease(const RouteMap (&maps)[N], size_t index = 1) {
    return index >= N ||
           (maps[index - 1].averageLevel < maps[index].averageLevel &&
            routeLevelsStrictlyIncrease(maps, index + 1));
}

template <size_t N>
constexpr bool routeTuningValid(const RouteMap (&maps)[N], size_t index = 0) {
    return index >= N ||
           (maps[index].minMapCount >= 3 &&
            maps[index].maxMapCount >= maps[index].minMapCount &&
            maps[index].maxMapCount <= 9 &&
            maps[index].maxMapCount - maps[index].minMapCount <= 2 &&
            maps[index].encounterChance <= 10000 &&
            maps[index].pickups.count > 0 &&
            Pickup::weightTotal(maps[index].pickups, 5000) > 0 &&
            maps[index].pickups.maxCoin >= maps[index].pickups.minCoin &&
            routeTuningValid(maps, index + 1));
}

static_assert(ROUTE_MAP_COUNT == Ui::Explore::AREA_COUNT,
              "explore area strings and route maps must stay aligned");
static_assert(ROUTE_MAP_COUNT == Game::EXPLORE_AREA_COUNT,
              "save reroll counters and route maps must stay aligned");
static_assert(ROUTE_MAP_COUNT == ExploreBoss::AREA_COUNT,
              "explore boss configs and route maps must stay aligned");
static_assert(ROUTE_MAP_COUNT == ExploreSpecial::AREA_COUNT,
              "special encounter areas and route maps must stay aligned");
#if !STICKMON_EXTERNAL_CONTENT
static_assert(routeLevelsStrictlyIncrease(ROUTE_MAPS),
              "explore area average levels must follow menu order");
#endif
#if !STICKMON_EXTERNAL_CONTENT
static_assert(routeTuningValid(ROUTE_MAPS),
              "explore encounter chances and pickup weights must be valid");
#endif
#if !STICKMON_EXTERNAL_CONTENT
static_assert(mapCountForRoll(ROUTE_MAPS[0], 0) == 3 &&
                  mapCountForRoll(ROUTE_MAPS[0], 49) == 3 &&
                  mapCountForRoll(ROUTE_MAPS[0], 50) == 4 &&
                  mapCountForRoll(ROUTE_MAPS[0], 99) == 4,
              "two-value expedition ranges must split evenly");
#endif
#if !STICKMON_EXTERNAL_CONTENT
static_assert(mapCountForRoll(ROUTE_MAPS[2], 0) == 4 &&
                  mapCountForRoll(ROUTE_MAPS[2], 24) == 4 &&
                  mapCountForRoll(ROUTE_MAPS[2], 25) == 5 &&
                  mapCountForRoll(ROUTE_MAPS[2], 74) == 5 &&
                  mapCountForRoll(ROUTE_MAPS[2], 75) == 6 &&
                  mapCountForRoll(ROUTE_MAPS[2], 99) == 6,
              "three-value expedition ranges must use 25/50/25 weights");
#endif
static_assert(ENCOUNTER_COOLDOWN_STEP_COUNT == 5 && MAX_ENCOUNTERS_PER_MAP == 2,
              "explore encounter limits must match the design contract");
static_assert(cooldownAfterCompletedSteps(ENCOUNTER_COOLDOWN_STEP_COUNT, 4) == 1 &&
                  cooldownAfterCompletedSteps(ENCOUNTER_COOLDOWN_STEP_COUNT, 5) == 0 &&
                  encounterGateOpen(0, 1) &&
                  !encounterGateOpen(1, 0) &&
                  !encounterGateOpen(0, MAX_ENCOUNTERS_PER_MAP),
                  "encounter cooldown and per-map cap must gate random battles");
static_assert(encounterGateAllows(5, MAX_ENCOUNTERS_PER_MAP, true) &&
                  !encounterGateAllows(5, MAX_ENCOUNTERS_PER_MAP, false) &&
                  repelAllowsEncounter(true, true) &&
                  !repelAllowsEncounter(false, true),
              "honey must bypass the gate while repel only blocks random battles");
static_assert(canScheduleGuaranteedEncounter(12, 5) &&
                  guaranteedEncounterIndex(12, 5) == 8 &&
                  !canScheduleGuaranteedEncounter(6, 5),
              "guaranteed battles must use an interior point after cooldown");
static_assert(MAP_PICKUP_CHANCE == 6500,
              "long expeditions must keep the per-map pickup chance at 65 percent");
#if !STICKMON_EXTERNAL_CONTENT
static_assert(ROUTE_MAPS[ROUTE_MAP_COUNT - 1].averageLevel == 60,
              "ancient waterfall valley must average level 60");
#endif
#if !STICKMON_EXTERNAL_CONTENT
static_assert(ROUTE_MAPS[0].averageLevel - ROUTE_MAPS[0].depthSpread -
                  WILD_LEVEL_VARIANCE >= WILD_LEVEL_MIN,
              "first area shallow range must stay above level one");
#endif
#if !STICKMON_EXTERNAL_CONTENT
static_assert(ROUTE_MAPS[ROUTE_MAP_COUNT - 1].averageLevel +
                  ROUTE_MAPS[ROUTE_MAP_COUNT - 1].depthSpread +
                  WILD_LEVEL_VARIANCE <= WILD_LEVEL_MAX,
              "deepest area level must stay within the wild level cap");
#endif
static_assert(ExploreEncounterRules::depthLevelOffset(332, 4) == -4 &&
                  ExploreEncounterRules::depthLevelOffset(333, 4) == 0 &&
                  ExploreEncounterRules::depthLevelOffset(666, 4) == 0 &&
                  ExploreEncounterRules::depthLevelOffset(667, 4) == 4,
              "exploration depth thresholds must cover three progress bands");
static_assert(static_cast<uint8_t>(ExploreScene::Area::GRASS_PATH) ==
                  ExploreMapGenerator::GRASS_PATH_AREA,
              "grass path area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::CREEK_SLOPE) ==
                  ExploreMapGenerator::CREEK_BRIDGE_SLOPE_AREA,
              "creek slope area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::TALL_GRASS_PARK) ==
                  ExploreMapGenerator::TALL_GRASS_PARK_AREA,
              "tall grass area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::FROST_CRYSTAL_CAVE) ==
                  ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA,
              "frost cave area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::MIST_FOREST_PATH) ==
                  ExploreMapGenerator::MIST_FOREST_PATH_AREA,
              "mist forest area index must follow menu order");
static_assert(static_cast<uint8_t>(ExploreScene::Area::ANCIENT_WATERFALL_VALLEY) ==
                  ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA,
              "ancient valley area index must follow menu order");
using RouteWorldPoint = ExploreRouteGeometry::WorldPoint;

RouteWorldPoint routePathPointWorld(const ExploreMapGenerator::Path& path,
                                    uint8_t index) {
    return ExploreRouteGeometry::pathPoint(path, index);
}

constexpr uint8_t routeFollowerTargetIndex(uint8_t leaderIndex) {
    return leaderIndex >= ROUTE_FOLLOWER_GAP_STEPS
        ? leaderIndex - ROUTE_FOLLOWER_GAP_STEPS
        : 0;
}

uint8_t routeLeaderSlot(const Game::GameState& state) {
    return Game::ExplorePartyService::leaderSlot(state);
}

bool hasHealthyRouteFollower(const Game::GameState& state) {
    return Game::ExplorePartyService::hasHealthyFollower(state);
}

uint16_t routeStepDurationForSpecies(uint16_t speciesId) {
    PokemonSprites::WalkingAnimation animation{};
    uint8_t frameCount = 1;
    if (PokemonSprites::walkingAnimation(
            speciesId, PokemonSprites::WalkDirection::DOWN, animation) &&
        animation.frameCount > 0) {
        frameCount = animation.frameCount;
    }
    return PokemonMotion::routeStepDurationMs(
        PokemonMotion::behaviorForSpecies(speciesId), frameCount);
}

static_assert(routeFollowerTargetIndex(0) == 0 &&
                  routeFollowerTargetIndex(1) == 0 &&
                  routeFollowerTargetIndex(2) == 0 &&
                  routeFollowerTargetIndex(3) == 1,
              "route follower must remain two path steps behind the leader");

uint8_t routeDirectionForDelta(float dx, float dy, uint8_t fallback) {
    return ExploreRouteMotion::directionForDelta(dx, dy, fallback);
}

uint16_t routeDurationForDistance(float fromX, float fromY, float toX, float toY,
                                  uint16_t stepDurationMs) {
    float stepCount = hypotf(toX - fromX, toY - fromY) / EXPLORE_TILE_SIZE;
    if (stepCount < 1.0f) stepCount = 1.0f;
    uint32_t duration = static_cast<uint32_t>(ceilf(stepCount * stepDurationMs));
    return static_cast<uint16_t>(MathUtil::min<uint32_t>(duration, 60000U));
}

const RouteMap& routeMap(uint8_t index) {
    return ROUTE_MAPS[index < ROUTE_MAP_COUNT ? index : 0];
}

uint8_t rollPickupId(const RouteMap& map) {
    uint16_t stepsToday = GameEngine::ins().gameState().stepsToday;
    uint16_t total = Pickup::weightTotal(map.pickups, stepsToday);
    if (total == 0) return Pickup::NONE;
    uint16_t roll = static_cast<uint16_t>(GameRandom::random(0, total));
    return Pickup::forRoll(map.pickups, stepsToday, roll);
}

PokemonSprites::WalkDirection inwardDirection(ExploreMapGenerator::Edge edge) {
    switch (edge) {
    case ExploreMapGenerator::Edge::TOP: return PokemonSprites::WalkDirection::DOWN;
    case ExploreMapGenerator::Edge::RIGHT: return PokemonSprites::WalkDirection::LEFT;
    case ExploreMapGenerator::Edge::BOTTOM: return PokemonSprites::WalkDirection::UP;
    case ExploreMapGenerator::Edge::LEFT: return PokemonSprites::WalkDirection::RIGHT;
    }
    return PokemonSprites::WalkDirection::DOWN;
}

const EncounterEntry* rollEncounterEntry(const RouteMap& map) {
    if (!map.encounters || map.encounterCount == 0) return nullptr;
    uint32_t total = 0;
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        total += map.encounters[i].weight;
    }
    if (total == 0) return nullptr;

    uint32_t roll = static_cast<uint32_t>(GameRandom::random(static_cast<long>(total)));
    for (uint8_t i = 0; i < map.encounterCount; ++i) {
        if (roll < map.encounters[i].weight) return &map.encounters[i];
        roll -= map.encounters[i].weight;
    }
    return nullptr;
}

// 活跃池内滚点：稀有成员按 weight × RARE_ROLL_BONUS 计（§7.4 / §7.9.3）
const ExplorePool::PoolEntry* rollPoolEntry(const ExplorePool::Pool& pool) {
    uint32_t total = ExplorePool::poolWeightTotal(pool);
    if (total == 0) return nullptr;

    uint32_t roll = static_cast<uint32_t>(GameRandom::random(static_cast<long>(total)));
    return ExplorePool::entryForRoll(pool, roll);
}
}

void ExploreScene::onEnter() {
    cancelTouch();
    auto& engine = GameEngine::ins();
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    phase = Phase::SELECT;
    adventureBondGain = 0;
    adventureBondSettled = false;
    progressionCancelledSpeciesId = 0;
    ProgressionUi::resetMoveLearnState(moveLearnState);
    ui.state.areaCursor = 0;
    ui.state.areaAnimCursor = 0.0f;
    ui.routeVisual.areaBackgroundTraced = false;
    resultMessage = nullptr;
    departureToastUntil = 0;
    defeatAwaitInput = false;
    clearFriendshipFlow();
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    autoWalkActive = false;
    routePausedForTouch = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    walkStepResolutionPending = false;
    battleIsBoss = false;
    battleAllowsFriendship = true;
    battleFoodBond = 0;
    expeditionBossScheduled = false;
    expeditionNormalBossScheduled = false;
    naturalRunCompletionPending = false;
    normalBossPitySettled = false;
    expeditionPitySlotIndex = 0;
    expeditionBossSpeciesId = 0;
    expeditionBossLevel = 0;
    expeditionBossExperiencePercent = 100;
    expeditionSpecialKind = ExploreSpecial::Kind::NONE;
    specialRelocationHandled = false;
    ui.state.specialChallengeYes = true;
    routeBossPending = false;
#if STICKMON_ENABLE_DEBUG_FEATURES
    debugBattleMode = engine.consumeDebugBattleRequest();
    if (debugBattleMode) {
        activeArea = Area::GRASS_PATH;
        ui.state.areaCursor = static_cast<uint8_t>(activeArea);
        mapBlocks[0] = 0;
        mapBlockCount = 1;
        currentMapBlock = 0;
        beginDebugEncounter();
        return;
    }
#endif
    if (engine.exploreTravelPhase() == ExploreTravelPhase::DEPARTING) {
        uint8_t area = engine.pendingExploreArea();
        if (area == ExploreDestinations::MARKET) {
            if (beginMarket()) engine.markExploreActive();
            else engine.finishExploreReturn();
            if (phase == Phase::SELECT) loadAreaPreview();
            prepareSurface();
            return;
        }
        if (area >= static_cast<uint8_t>(Area::COUNT)) area = 0;
        activeArea = static_cast<Area>(area);
        ui.state.areaCursor = ExploreDestinations::visibleIndex(area, engine.gameState());
        engine.markExploreActive();
        resetWalk();
    }
    if (phase == Phase::SELECT) loadAreaPreview();
    prepareSurface();
}

void ExploreScene::prepareSurface() {
    if (!exploreSubViewOpen) Ui::SceneSurface::explorePhase(phase);
}

void ExploreScene::onExit() {
    if (market_) { market_.reset(); Ui::MarketView::release(); }
    cancelTouch();
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
}

Game::MonsterRuntime& ExploreScene::battlePlayerMonster() {
    auto& state = GameEngine::ins().gameState();
    uint8_t slot = battlePlayerSlot < state.teamCount ? battlePlayerSlot : 0;
    return state.team[slot];
}

const Game::MonsterRuntime& ExploreScene::battlePlayerMonster() const {
    const auto& state = GameEngine::ins().gameState();
    uint8_t slot = battlePlayerSlot < state.teamCount ? battlePlayerSlot : 0;
    return state.team[slot];
}

const Species& ExploreScene::battlePlayerSpecies() const {
    return GameEngine::ins().speciesFor(battlePlayerMonster());
}

bool ExploreScene::areaPreviewScrolls() const {
#if STICKMON_UI_ROUND_TOUCH
    return Ui::RoundExplore::PreviewLayout::scrolls(areaPreviewFrames, areaPreviewPool.count);
#else
    return areaPreviewPool.count > 0;
#endif
}

uint32_t ExploreScene::visualFrameIntervalMs(uint32_t nowMs) const {
#if STICKMON_UI_ROUND_TOUCH
    if (phase == Phase::SELECT && !market_ && !exploreSubViewOpen &&
        areaPreviewReady && !areaPreviewPaused && areaPreviewScrolls() &&
        (nowMs - areaPreviewStartedAt) % AREA_PREVIEW_CYCLE_MS >= AREA_PREVIEW_HOLD_MS)
        return 33;
#else
    (void)nowMs;
#endif
    return 66;
}

SceneUpdateResult ExploreScene::update(uint32_t nowMs, float dtSeconds) {
    if (market_) {
        marketUi_.update(*market_,nowMs,!GameEngine::ins().sceneFadeActive());
        if (market_->finished()) {
            if (phase != Phase::ENDING) closeMarket();
            return SceneUpdateResult::animate(33);
        }
        return market_->paused() ? SceneUpdateResult::parked() : SceneUpdateResult::animate(66);
    }
    if (exploreSubViewOpen) {
        ui.updateRouteVisual(nowMs, false);
        const auto result = exploreSubView.update(nowMs, dtSeconds);
        if (finishSubViewInput()) return SceneUpdateResult::frame();
        return result;
    }
    if (phase == Phase::EVOLUTION && ProgressionUi::evolutionCancelHeld(nowMs)) {
        handleExploreProgressionButton({1, BtnAction::PRESSED});
        return SceneUpdateResult::frame();
    }
    if (phase == Phase::WALKING && pendingFrostFall &&
        static_cast<int32_t>(nowMs - frostFallAtMs) >= 0) {
        advanceMapBlock(exitNextMaps[currentRoutePath], true);
        return SceneUpdateResult::frame();
    }
    RenderDemand demand;
    if (departureToastUntil) {
        if (static_cast<int32_t>(nowMs - departureToastUntil) >= 0) {
            departureToastUntil = 0;
            demand.changed(true);
        } else demand.wakeIn(departureToastUntil - nowMs);
    }
    bool areaCursorAnimating = false;
    bool areaCursorMoved = false;
    bool previewCanAnimate = areaPreviewPool.count > 0;
#if STICKMON_UI_ROUND_TOUCH
    if (phase == Phase::SELECT) {
        const bool paused = GameEngine::ins().idleModeActive() || !areaPreviewReady;
        if (paused && !areaPreviewPaused) {
            areaPreviewPausedElapsed = areaPreviewReady
                ? ((nowMs - areaPreviewStartedAt) / AREA_PREVIEW_CYCLE_MS) * AREA_PREVIEW_CYCLE_MS : 0;
            demand.redraw();
        }
        if (paused || areaPreviewPaused)
            areaPreviewStartedAt = nowMs - areaPreviewPausedElapsed;
        areaPreviewPaused = paused;
        previewCanAnimate = areaPreviewScrolls() && !paused;
    }
#endif
    if (phase == Phase::SELECT) {
        uint8_t visibleAreaCount = ExploreDestinations::visibleCount(
            GameEngine::ins().gameState());
        const auto cursor = ui.updateAreaCursor(ExploreDestinations::COUNT, visibleAreaCount,
            nowMs, areaPreviewStartedAt, previewCanAnimate);
        areaCursorMoved = cursor.moved;
        areaCursorAnimating = cursor.animating;
        demand.changed(cursor.redraw);
    }

    // Keep synchronous LittleFS decode work out of the cursor transition.
    // The final cursor frame is rendered first, then preloading resumes.
    if (areaCursorMoved && areaPreviewLoadPending) {
        areaPreviewNextLoadAt = nowMs + PREVIEW_LOAD_AFTER_CURSOR_MS;
    }
    const bool previewLoaded = updateAreaPreviewLoading(nowMs);
    demand.changed(previewLoaded);
    // Readiness can change after the paused-state check above. Deliver a final
    // update even when this was the last queued species, so the clock resumes.
    if (previewLoaded) demand.wakeIn(66);
    demand.changed(updateRouteMovement(nowMs));

    bool expWasActive = expAnimationActive;
    updateExpAnimation(nowMs);
    demand.changed(expWasActive != expAnimationActive);
    demand.changed(updateFoodThrow(nowMs));
    const BattleLogUpdate logUpdate = serviceBattleLog(nowMs);
    if (logUpdate.sceneReplaced) return SceneUpdateResult::frame();
    demand.changed(logUpdate.changed);

    BattleSwitchStage switchStageBefore = battleSwitchStage;
    uint8_t playerSlotBefore = battlePlayerSlot;
    updateBattleSwitch(nowMs);
    demand.changed(switchStageBefore != battleSwitchStage ||
                   playerSlotBefore != battlePlayerSlot);

    Phase phaseBeforeTurn = phase;
    BattleTurnStage turnStageBefore = battleTurnStage;
    uint16_t playerHpBefore = battlePlayerMonster().hpCur;
    uint16_t wildHpBefore = wildHp;
    updateBattleTurn(nowMs);
    demand.changed(phaseBeforeTurn != phase ||
                   turnStageBefore != battleTurnStage ||
                   playerHpBefore != battlePlayerMonster().hpCur ||
                   wildHpBefore != wildHp);

    // A synchronous preload may have extended the hold past this update's
    // starting timestamp. Use the completed clock for preview-only decisions.
#if STICKMON_UI_ROUND_TOUCH
    const uint32_t previewNowMs = Hal::ins().millis();
#else
    const uint32_t previewNowMs = nowMs;
#endif
    bool dynamic = (phase == Phase::WALKING && !exploreMenuOpen && !routePausedForTouch) ||
        phase == Phase::EXITING;
    if (phase == Phase::SELECT) {
        dynamic = areaCursorAnimating;
        if (ui.state.areaCursor < ExploreDestinations::COUNT && previewCanAnimate) {
            uint32_t cycleElapsed =
                (previewNowMs - areaPreviewStartedAt) % AREA_PREVIEW_CYCLE_MS;
            dynamic = dynamic || cycleElapsed > AREA_PREVIEW_HOLD_MS;
        }
    }
    if (phase == Phase::ENCOUNTER || phase == Phase::FRIENDSHIP) {
        const auto& active = battlePlayerMonster();
        dynamic = dynamic || battleLogBusy() || expAnimationActive ||
                  battleSwitchStage != BattleSwitchStage::NONE ||
                  battleTurnStage != BattleTurnStage::IDLE ||
                  wildRuntime.majorStatus != Game::MajorStatus::NONE ||
                  active.majorStatus != Game::MajorStatus::NONE;
    }
    if (phase == Phase::EVOLUTION) {
        dynamic = dynamic || !ProgressionUi::evolutionAnimationComplete(
            GameEngine::ins().pendingEvolutionFromSpeciesId(),
            GameEngine::ins().pendingEvolutionToSpeciesId(),
            nowMs);
    }
    if (phase == Phase::EVOLUTION_CANCELLED) {
        dynamic = dynamic ||
            !ProgressionUi::evolutionCancellationComplete(nowMs);
    }

    demand.animate(dynamic, visualFrameIntervalMs(previewNowMs));
    if (phase == Phase::SELECT && !dynamic &&
        ui.state.areaCursor < ExploreDestinations::COUNT && previewCanAnimate) {
        uint32_t cycleElapsed =
            (previewNowMs - areaPreviewStartedAt) % AREA_PREVIEW_CYCLE_MS;
        demand.wakeIn(MathUtil::max<uint32_t>(
            1, AREA_PREVIEW_HOLD_MS - cycleElapsed + 1));
    }
    if (areaPreviewLoadPending) {
        uint32_t loadDelay =
            static_cast<int32_t>(nowMs - areaPreviewNextLoadAt) >= 0
                ? 1 : areaPreviewNextLoadAt - nowMs;
        demand.wakeIn(loadDelay);
    }
    ui.updateRouteVisual(nowMs, phase == Phase::WALKING && !exploreMenuOpen && !routePausedForTouch &&
        !pendingFrostFall);
    return demand.result();
}

bool ExploreScene::onButton(const ButtonEvent& event) {
    cancelTouch();
    if (market_) {
        if (event.action == BtnAction::LONG_PRESS && event.btn == 1) market_->leave();
        else if (event.action == BtnAction::PRESSED) {
            if (event.btn == 0) market_->walkSelected();
            else if (event.btn == 1) market_->nextTarget();
        }
        return true;
    }
    if (exploreSubViewOpen) {
        exploreSubView.onButton(event);
        finishSubViewInput();
        return true;
    }
    return ui.onButton(event);
}

void ExploreScene::cancelTouch() {
    marketUi_.cancel();
    if(market_)market_->setDirection(-1);
    ProgressionUi::cancelMoveLearnTouch();
    Scene::cancelTouch();
    ui.cancelTouch();
    exploreSubView.cancelTouch();
}

bool ExploreScene::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    if (market_) {
        marketUi_.touchSession(*market_,event,Ui::MarketWalkUi::viewport(),!market_->paused()&&!market_->exiting());
        return true;
    }
    if (!exploreSubViewOpen && phase == Phase::LEARN_MOVE) {
        if (ProgressionUi::handleMoveLearnTouch(moveLearnState, event)) {
            if (!enterPendingProgression(progressionReturnPhase)) finishProgression();
        }
        return true;
    }
    if (!exploreSubViewOpen && phase == Phase::MOVE_REPLACED) {
        if (ProgressionUi::handleMoveReplacementTouch(event))
            return onButton({0, BtnAction::PRESSED});
        return true;
    }
    if (!exploreSubViewOpen &&
        (phase == Phase::EVOLUTION || phase == Phase::EVOLUTION_CANCELLED)) {
        switch (ProgressionUi::handleEvolutionTouch(event, Hal::ins().millis())) {
        case Ui::EvolutionTouchAction::CONTINUE: return handleExploreProgressionButton({0, BtnAction::PRESSED});
        case Ui::EvolutionTouchAction::CANCEL: return handleExploreProgressionButton({1, BtnAction::PRESSED});
        default: return true;
        }
    }
    ProgressionUi::cancelMoveLearnTouch();
    if (!exploreSubViewOpen) return ui.onTouch(event);
    const bool changed = exploreSubView.onTouch(event);
    return finishSubViewInput() || changed;
}

bool ExploreScene::finishSubViewInput() {
    MenuScene::BattleBagResult itemResult =
        exploreSubView.consumeBattleBagResult();
    if (itemResult != MenuScene::BattleBagResult::NONE) {
        exploreSubViewOpen = false;
        cancelTouch();
        switch (itemResult) {
        case MenuScene::BattleBagResult::POTION:
            enqueueBattleLog(Ui::Bag::USED_POTION);
            break;
        case MenuScene::BattleBagResult::SUPER_POTION:
            enqueueBattleLog(Ui::Bag::USED_SUPER_POTION);
            break;
        case MenuScene::BattleBagResult::ANTIDOTE:
            enqueueBattleLog(Ui::Bag::USED_ANTIDOTE);
            break;
        case MenuScene::BattleBagResult::PARALYZE_HEAL:
            enqueueBattleLog(Ui::Bag::USED_PARALYZE_HEAL);
            break;
        case MenuScene::BattleBagResult::AWAKENING:
            enqueueBattleLog(Ui::Bag::USED_AWAKENING);
            break;
        case MenuScene::BattleBagResult::BURN_HEAL:
            enqueueBattleLog(Ui::Bag::USED_BURN_HEAL);
            break;
        case MenuScene::BattleBagResult::ICE_HEAL:
            enqueueBattleLog(Ui::Bag::USED_ICE_HEAL);
            break;
        case MenuScene::BattleBagResult::MAX_POTION:
            enqueueBattleLog(Ui::Bag::USED_MAX_POTION);
            break;
        case MenuScene::BattleBagResult::FULL_RESTORE:
            enqueueBattleLog(Ui::Bag::USED_FULL_RESTORE);
            break;
        case MenuScene::BattleBagResult::FULL_HEAL:
            enqueueBattleLog(Ui::Bag::USED_FULL_HEAL);
            break;
        case MenuScene::BattleBagResult::REVIVE:
            enqueueBattleLog(Ui::Bag::USED_REVIVE);
            break;
        case MenuScene::BattleBagResult::FOOD_THROWN:
            throwFood(exploreSubView.battleBagThrownFoodIndex());
            return true;
        default:
            break;
        }
        wildCounterattack();
        return true;
    }
    if (exploreSubView.exploreViewClosed()) {
        exploreSubViewOpen = false;
        cancelTouch();
        return true;
    }
    return false;
}

bool ExploreScene::handleExploreProgressionButton(const ButtonEvent& event) {
    if (phase == Phase::LEVEL_UP) {
        if (event.btn == 0) {
            GameEngine::ins().acknowledgePendingLevelUp();
            if (GameEngine::ins().hasPendingEvolution()) {
                phase = Phase::EVOLUTION;
            } else if (GameEngine::ins().hasPendingMoveLearn()) {
                ProgressionUi::resetMoveLearnState(moveLearnState);
                phase = Phase::LEARN_MOVE;
            } else {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::EVOLUTION) {
        if (event.btn == 1) {
            uint16_t retainedSpeciesId =
                GameEngine::ins().pendingEvolutionFromSpeciesId();
            uint16_t targetSpeciesId =
                GameEngine::ins().pendingEvolutionToSpeciesId();
            if (GameEngine::ins().cancelPendingEvolution()) {
                ProgressionUi::beginEvolutionCancellation(
                    retainedSpeciesId, targetSpeciesId,
                    Hal::ins().millis());
                progressionCancelledSpeciesId = retainedSpeciesId;
                phase = Phase::EVOLUTION_CANCELLED;
            }
            return true;
        }
        if (event.btn == 0) {
            uint32_t nowMs = Hal::ins().millis();
            if (!ProgressionUi::evolutionAnimationComplete(
                    GameEngine::ins().pendingEvolutionFromSpeciesId(),
                    GameEngine::ins().pendingEvolutionToSpeciesId(),
                    nowMs)) {
                return true;
            }
            GameEngine::ins().acknowledgePendingEvolution();
            ProgressionUi::resetEvolutionAnimation();
            if (GameEngine::ins().hasPendingEvolution()) {
                phase = Phase::EVOLUTION;
            } else if (GameEngine::ins().hasPendingMoveLearn()) {
                ProgressionUi::resetMoveLearnState(moveLearnState);
                phase = Phase::LEARN_MOVE;
            } else {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::EVOLUTION_CANCELLED) {
        if (event.btn == 0) {
            if (!ProgressionUi::evolutionCancellationComplete(
                    Hal::ins().millis())) {
                return true;
            }
            ProgressionUi::resetEvolutionAnimation();
            progressionCancelledSpeciesId = 0;
            if (!enterPendingProgression(progressionReturnPhase)) {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::LEARN_MOVE) {
        if (ProgressionUi::handleMoveLearnInput(
                moveLearnState, event.btn)) {
            if (!enterPendingProgression(progressionReturnPhase)) {
                finishProgression();
            }
        }
        return true;
    }

    if (phase == Phase::MOVE_REPLACED) {
        if (event.btn == 0) {
            GameEngine::ins().acknowledgePendingMoveReplacement();
            if (!enterPendingProgression(progressionReturnPhase)) {
                finishProgression();
            }
        }
        return true;
    }

    return false;
}


#if STICKMON_ENABLE_DEBUG_FEATURES
void ExploreScene::returnToDebugMenu() {
    debugBattleMode = false;
    GameEngine::ins().endDebugBattle();
}
#endif

void ExploreScene::beginRouteExit() {
    if (phase == Phase::EXITING || phase == Phase::ENDING) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (finalRoute() &&
        routeIndex + 1 >= path.pointCount) {
        naturalRunCompletionPending = true;
    }

    routeFromX = routeWorldX;
    routeFromY = routeWorldY;
    routeTargetX = routeWorldX;
    routeTargetY = routeWorldY;
    const bool interiorStair = ExploreRouteGeometry::interiorPortalExit(
        generatedMap, path);
    if (!interiorStair) {
        switch (path.exit.edge) {
        case ExploreMapGenerator::Edge::TOP:
            routeTargetY = -ROUTE_EXIT_MARGIN;
            break;
        case ExploreMapGenerator::Edge::RIGHT:
            routeTargetX = EXPLORE_MAP_W + ROUTE_EXIT_MARGIN;
            break;
        case ExploreMapGenerator::Edge::BOTTOM:
            routeTargetY = EXPLORE_MAP_H + ROUTE_EXIT_MARGIN;
            break;
        case ExploreMapGenerator::Edge::LEFT:
            routeTargetX = -ROUTE_EXIT_MARGIN;
            break;
        }
    }
    routeWalkDirection = routeDirectionForDelta(
        routeTargetX - routeFromX, routeTargetY - routeFromY, routeWalkDirection);

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFollowerTargetX = interiorStair ? routeFollowerFromX : routeTargetX;
    routeFollowerTargetY = interiorStair ? routeFollowerFromY : routeTargetY;
    routeFollowerWalkDirection = routeDirectionForDelta(
        routeFollowerTargetX - routeFollowerFromX,
        routeFollowerTargetY - routeFollowerFromY,
        routeFollowerWalkDirection);

    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    uint16_t leaderStepDuration = routeStepDurationForSpecies(
        engine.speciesFor(state.team[leaderSlot]).id);
    routeLeaderMoveDurationMs = routeDurationForDistance(
        routeFromX, routeFromY, routeTargetX, routeTargetY, leaderStepDuration);
    routeMoveDurationMs = routeLeaderMoveDurationMs;
    routeFollowerMoveDurationMs = routeLeaderMoveDurationMs;
    routeFollowerMoving = false;
    if (!interiorStair && hasHealthyRouteFollower(state)) {
        uint16_t followerStepDuration = routeStepDurationForSpecies(
            engine.speciesFor(state.team[1]).id);
        routeFollowerMoveDurationMs = routeDurationForDistance(
            routeFollowerFromX, routeFollowerFromY,
            routeFollowerTargetX, routeFollowerTargetY,
            followerStepDuration);
        routeFollowerMoving = true;
        routeMoveDurationMs = MathUtil::max<uint16_t>(
            routeLeaderMoveDurationMs,
            ROUTE_FOLLOWER_DELAY_MS + routeFollowerMoveDurationMs);
    } else {
        routeFollowerTargetX = routeFollowerFromX;
        routeFollowerTargetY = routeFollowerFromY;
    }

    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    routePickupAvailable = false;
    routeGuaranteedEncounterPending = false;
    autoWalkActive = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    routeMoveStarted = Hal::ins().millis();
    routeMoving = true;
    phase = Phase::EXITING;
}

void ExploreScene::requestExploreExit(bool fainted, bool showEndPrompt) {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) {
        returnToDebugMenu();
        return;
    }
#endif
    if (phase == Phase::SELECT &&
        GameEngine::ins().exploreTravelPhase() != ExploreTravelPhase::ACTIVE) {
        if (GameEngine::ins().contactVisitExploring()) {
            GameEngine::ins().requestContactVisitFarewell();
            GameEngine::ins().requestScene(SceneID::MAIN);
        } else {
            GameEngine::ins().requestScene(SceneID::MENU);
        }
        return;
    }
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool returnFainted = fainted || mon.fainted || mon.hpCur == 0;
    routeMoving = false;
    autoWalkActive = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    if (returnFainted || !showEndPrompt) {
        exploreMenuOpen = false;
        exploreSubViewOpen = false;
        exploreMenuOpenedAt = 0;
        completeExploreReturn(returnFainted);
        return;
    }
    if (phase == Phase::ENDING) return;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    beginExploreEnding();
}

void ExploreScene::settleExploreCosts(bool returning) {
    auto& engine = GameEngine::ins();
    engine.advanceExploreMinutes(exploreCosts.settleBattle(engine.gameState(), battleCosts));
    if (returning) {
        exploreCosts.finish(engine.gameState());
        if (exploreCosts.active) engine.markDirty(SaveUrgency::SOON);
    }
}

void ExploreScene::beginExploreEnding() {
    phase = Phase::EXITING;
    completeExploreReturn(false);
}

void ExploreScene::settleAdventureBond() {
    if (adventureBondSettled) return;
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) return;
#endif
    adventureBondSettled = true;
    adventureBondGain = GameEngine::ins().grantAdventureBond(steps);
}

void ExploreScene::completeExploreReturn(bool fainted) {
    settleExploreCosts(true);
    settleAdventureBond();
    settleNormalBossPity();
    GameEngine::ins().beginExploreReturn(fainted);
}

void ExploreScene::settleNormalBossPity() {
    if (normalBossPitySettled || !naturalRunCompletionPending) return;
    normalBossPitySettled = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) return;
#endif
    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    uint32_t currentSlot = ExploreSpecial::slotIndexFor(
        engine.gameMinutesTotal());
    bool changed = ExploreBossPity::syncSlot(state, currentSlot);
    if (currentSlot == expeditionPitySlotIndex &&
        !expeditionBossScheduled) {
        ExploreBossPity::increment(state, static_cast<uint8_t>(activeArea));
        changed = true;
    }
    if (changed) engine.markDirty(SaveUrgency::SOON);
}

void ExploreScene::closeExploreMenu() {
    if (!exploreMenuOpen) return;
    if (routeMoving && !routePausedForTouch) {
        routeMoveStarted += Hal::ins().millis() - exploreMenuOpenedAt;
    }
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
}

void ExploreScene::walk() {
    if (routeMoving || pendingFrostFall) return;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (routeIndex + 1 >= path.pointCount) {
        if (!finalRoute()) {
            advanceMapBlock(exitNextMaps[currentRoutePath]);
        } else {
            beginRouteExit();
        }
        return;
    }

    if (!iceSliding) {
        int8_t dx = 0;
        int8_t dy = 0;
        if (ExploreIceSlide::begins(
                generatedMap, path, routeIndex, dx, dy)) {
            iceSliding = true;
            iceSlideDx = dx;
            iceSlideDy = dy;
        }
    }

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFromX = routeWorldX;
    routeFromY = routeWorldY;
    ++routeIndex;
    RouteWorldPoint target = routePathPointWorld(path, routeIndex);
    routeTargetX = target.x;
    routeTargetY = target.y;
    routeFollowerTargetX = routeFollowerFromX;
    routeFollowerTargetY = routeFollowerFromY;
    if (routeIndex >= ROUTE_FOLLOWER_GAP_STEPS) {
        RouteWorldPoint followerTarget = routePathPointWorld(
            path, routeFollowerTargetIndex(routeIndex));
        routeFollowerTargetX = followerTarget.x;
        routeFollowerTargetY = followerTarget.y;
    }
    float dx = routeTargetX - routeFromX;
    float dy = routeTargetY - routeFromY;
    routeWalkDirection = routeDirectionForDelta(dx, dy, routeWalkDirection);
    routeFollowerWalkDirection = routeDirectionForDelta(
        routeFollowerTargetX - routeFollowerFromX,
        routeFollowerTargetY - routeFollowerFromY,
        routeFollowerWalkDirection);
    routeFollowerMoving = fabsf(routeFollowerTargetX - routeFollowerFromX) >= 0.01f ||
                          fabsf(routeFollowerTargetY - routeFollowerFromY) >= 0.01f;
    routeMoveStarted = Hal::ins().millis();
    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    bool hasFollower = hasHealthyRouteFollower(state);
    routeLeaderMoveDurationMs = routeStepDurationForSpecies(
        engine.speciesFor(state.team[leaderSlot]).id);
    routeFollowerMoveDurationMs = routeLeaderMoveDurationMs;
    routeMoveDurationMs = routeLeaderMoveDurationMs;
    if (iceSliding) {
        routeLeaderMoveDurationMs = ICE_SLIDE_STEP_MS;
        routeFollowerMoveDurationMs = ICE_SLIDE_STEP_MS;
        routeMoveDurationMs = ICE_SLIDE_STEP_MS;
        if (!hasFollower) {
            routeFollowerTargetX = routeFollowerFromX;
            routeFollowerTargetY = routeFollowerFromY;
            routeFollowerMoving = false;
        }
    } else if (hasFollower) {
        routeFollowerMoveDurationMs = routeStepDurationForSpecies(
            engine.speciesFor(state.team[1]).id);
        if (routeFollowerMoving) {
            routeMoveDurationMs = MathUtil::max<uint16_t>(
                routeLeaderMoveDurationMs,
                ROUTE_FOLLOWER_DELAY_MS + routeFollowerMoveDurationMs);
        }
    } else {
        routeFollowerTargetX = routeFollowerFromX;
        routeFollowerTargetY = routeFollowerFromY;
        routeFollowerMoving = false;
    }
    routeMoving = true;
    ++steps;
    GameEngine::ins().addWalkSteps(1);
}

void ExploreScene::beginAutoWalk() {
    if (phase == Phase::WALKING && routePausedForTouch) {
        if (routeMoving) routeMoveStarted += Hal::ins().millis() - routePausedAt;
        routePausedForTouch = false;
        autoWalkActive = true;
        if (!routeMoving) walk();
        return;
    }
    if (phase != Phase::WALKING || routeMoving || autoWalkActive) return;
    autoWalkActive = true;
    walk();
}

bool ExploreScene::updateRouteMovement(uint32_t nowMs) {
    if (!routeMoving || exploreMenuOpen || routePausedForTouch) return false;
    uint32_t elapsed = nowMs - routeMoveStarted;
    uint32_t completedAt = routeMoveStarted + routeMoveDurationMs;
    uint16_t leaderDurationMs = MathUtil::max<uint16_t>(1, routeLeaderMoveDurationMs);
    float leaderProgress = MathUtil::min(
        1.0f, elapsed / static_cast<float>(leaderDurationMs));
    const Game::GameState& state = GameEngine::ins().gameState();
    uint8_t leaderSlot = routeLeaderSlot(state);
    const PokemonMotion::Behavior leaderMotion =
        PokemonMotion::behaviorForSpecies(
            GameEngine::ins().speciesFor(state.team[leaderSlot]).id);
    float leaderEased = phase == Phase::EXITING || iceSliding
        ? leaderProgress
        : PokemonMotion::stepPosition(leaderMotion, leaderProgress);
    routeWorldX = routeFromX + (routeTargetX - routeFromX) * leaderEased;
    routeWorldY = routeFromY + (routeTargetY - routeFromY) * leaderEased;

    if (routeFollowerMoving) {
        uint16_t followerDelayMs = iceSliding ? 0 : ROUTE_FOLLOWER_DELAY_MS;
        uint32_t followerElapsed = elapsed > followerDelayMs
            ? elapsed - followerDelayMs
            : 0;
        uint16_t followerDurationMs = MathUtil::max<uint16_t>(1, routeFollowerMoveDurationMs);
        float followerProgress = elapsed > followerDelayMs
            ? MathUtil::min(1.0f, followerElapsed / static_cast<float>(followerDurationMs))
            : 0.0f;
        const PokemonMotion::Behavior followerMotion =
            PokemonMotion::behaviorForSpecies(
                GameEngine::ins().speciesFor(state.team[1]).id);
        float followerEased = phase == Phase::EXITING || iceSliding
            ? followerProgress
            : PokemonMotion::stepPosition(followerMotion, followerProgress);
        routeFollowerWorldX = routeFollowerFromX +
                              (routeFollowerTargetX - routeFollowerFromX) * followerEased;
        routeFollowerWorldY = routeFollowerFromY +
                              (routeFollowerTargetY - routeFollowerFromY) * followerEased;
    }
    if (elapsed < routeMoveDurationMs) return true;

    routeWorldX = routeTargetX;
    routeWorldY = routeTargetY;
    routeFollowerWorldX = routeFollowerTargetX;
    routeFollowerWorldY = routeFollowerTargetY;
    routeMoving = false;
    routeFollowerMoving = false;
    if (phase == Phase::EXITING) {
        beginExploreEnding();
        return true;
    }
    if (!iceSliding && enterPendingProgression(Phase::WALKING)) {
        walkStepResolutionPending = true;
        return true;
    }
    finishCompletedWalkStep();
    if (routeMoving && (autoWalkActive || iceSliding) &&
        phase == Phase::WALKING &&
        static_cast<int32_t>(nowMs - completedAt) >= 0) {
        routeMoveStarted = completedAt;
    }
    return true;
}

void ExploreScene::finishCompletedWalkStep() {
    bool encounterBlockedThisStep = encounterCooldownSteps > 0;
    encounterCooldownSteps = cooldownAfterCompletedStep(encounterCooldownSteps);
    auto& engine = GameEngine::ins();
    engine.advanceExploreMinutes(exploreCosts.step(engine.gameState()));
    bool repelActiveThisStep = engine.repelStepsRemaining() > 0;
    engine.tickRepelStep();
    recoverTeamForCompletedSteps();

    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (exploreMenuOpen) return;
    if (currentMapBlock + 1 < mapBlockCount &&
        generatedMap.areaIndex == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA &&
        path.fallsToNextLevel && routeIndex + 1 == path.pointCount &&
        ExploreIceSlide::isCrackedIce(generatedMap, path, routeIndex)) {
        ExploreIceSlide::breakIce(generatedMap, path, routeIndex);
        iceSliding = false;
        autoWalkActive = false;
        pendingFrostFall = true;
        frostFallAtMs = Hal::ins().millis() + 240;
        return;
    }
    if (iceSliding) {
        if (ExploreIceSlide::continues(
                generatedMap, path, routeIndex,
                iceSlideDx, iceSlideDy)) {
            walk();
            return;
        }
        bool stoppedOnIce = ExploreIceSlide::indexIsSmoothIce(
            generatedMap, path, routeIndex);
        iceSliding = false;
        iceSlideDx = 0;
        iceSlideDy = 0;
        autoWalkActive = false;
        if (stoppedOnIce) return;
    }
    if (routeBossPending && routeIndex == routeBossIndex) {
        promptOrBeginRouteBoss();
        return;
    }
    if (collectRoutePickup()) {
        autoWalkActive = false;
        return;
    }
    if (routeIndex + 1 >= path.pointCount) {
        if (!finalRoute()) {
            advanceMapBlock(exitNextMaps[currentRoutePath]);
            if (phase == Phase::WALKING && autoWalkActive) walk();
        } else {
            beginRouteExit();
        }
        return;
    }
    if (routeBossPending && routeIndex + 1 == routeBossIndex) {
        autoWalkActive = false;
        return;
    }
    bool guaranteeEncounter = routeGuaranteedEncounterPending &&
                              routeIndex >= routeGuaranteedEncounterIndex;
    // 甜甜蜜：下一步必遇敌（保底标记等价物），遇敌闸门关闭时保留到再下一步。
    bool honeyActive = engine.honeyEncounterPending();
    if ((honeyActive || !encounterBlockedThisStep) &&
        rollRandomEncounter(guaranteeEncounter || honeyActive,
                            honeyActive, repelActiveThisStep)) {
        if (honeyActive) engine.clearHoneyEncounter();
        autoWalkActive = false;
        return;
    }
    if (autoWalkActive && phase == Phase::WALKING) walk();
}

void ExploreScene::recoverTeamForCompletedSteps() {
    auto& engine = GameEngine::ins();
    if (Game::ExplorePartyService::recoverAfterCompletedSteps(
            engine.gameState(), steps)) {
        engine.markDirty(SaveUrgency::DEFERRED);
    }
}

void ExploreScene::finishProgression() {
    phase = progressionReturnPhase;
    if (phase != Phase::WALKING || !walkStepResolutionPending) return;
    walkStepResolutionPending = false;
    finishCompletedWalkStep();
}

bool ExploreScene::rollRandomEncounter(bool guaranteed, bool bypassGate,
                                       bool repelActiveThisStep) {
    if (!encounterGateAllows(
            encounterCooldownSteps, mapEncounterCount, bypassGate)) {
        return false;
    }
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    // 黄金喷雾生效中：非保底随机遇敌不触发（保底/BOSS 不受影响）。
    if (!repelAllowsEncounter(guaranteed, repelActiveThisStep)) return false;
    if (!guaranteed && GameRandom::random(0, 10000) >= map.encounterChance) return false;
    ++mapEncounterCount;
    encounterCooldownSteps = ENCOUNTER_COOLDOWN_STEP_COUNT;
    routeGuaranteedEncounterPending = false;
    rollEncounter();
    return true;
}

void ExploreScene::resolvePickup(uint8_t pickupId) {
    if (pickupId == Pickup::COIN) {
        const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
        uint32_t coins = GameRandom::random(map.pickups.minCoin, (uint32_t)map.pickups.maxCoin + 1);
        GameEngine::ins().addCoins(coins);
        snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::PICKUP_COIN_FMT,
                 (unsigned long)coins);
        resultMessage = resultBuf;
        autoWalkActive = false;
        phase = Phase::PICKUP;
        return;
    }

    Game::ItemId itemId = Pickup::itemId(pickupId);
    const char* itemName = nullptr;
    switch (pickupId) {
    case Pickup::POTION:
        itemName = Ui::Explore::PICKUP_POTION;
        break;
    case Pickup::SUPER_POTION:
        itemName = Ui::Explore::PICKUP_SUPER_POTION;
        break;
    case Pickup::ANTIDOTE:
        itemName = Ui::Explore::PICKUP_ANTIDOTE;
        break;
    case Pickup::RARE_CANDY:
        itemName = Ui::Explore::PICKUP_CANDY;
        break;
    case Pickup::MAX_POTION:
        itemName = Ui::MAX_POTION;
        break;
    case Pickup::FULL_RESTORE:
        itemName = Ui::FULL_RESTORE;
        break;
    case Pickup::FULL_HEAL:
        itemName = Ui::FULL_HEAL;
        break;
    case Pickup::REVIVE:
        itemName = Ui::REVIVE;
        break;
    case Pickup::MAX_REPEL:
        itemName = Ui::MAX_REPEL;
        break;
    case Pickup::HONEY:
        itemName = Ui::HONEY;
        break;
    case Pickup::NUGGET:
        itemName = Ui::NUGGET;
        break;
    case Pickup::BIG_PEARL:
        itemName = Ui::BIG_PEARL;
        break;
    case Pickup::STAR_PIECE:
        itemName = Ui::STAR_PIECE;
        break;
    case Pickup::HEART_SCALE:
        itemName = Ui::HEART_SCALE;
        break;
    default:
        return;
    }
    bool stored = GameEngine::ins().addItem(itemId, 1);
    if (itemName) {
        if (stored) {
            snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::PICKUP_FMT,
                     itemName);
        } else if (routePickupFinalReward) {
            const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
            uint32_t coins = GameRandom::random(map.pickups.minCoin, (uint32_t)map.pickups.maxCoin + 1);
            GameEngine::ins().addCoins(coins);
            snprintf(resultBuf, sizeof(resultBuf),
                     Ui::Explore::PICKUP_COIN_FMT,
                     static_cast<unsigned long>(coins));
        } else {
            snprintf(resultBuf, sizeof(resultBuf), "%s", Ui::Shop::BAG_FULL);
        }
    }
    resultMessage = resultBuf;
    autoWalkActive = false;
    phase = Phase::PICKUP;
}

void ExploreScene::beginEncounter(const Species& species, uint8_t level, bool boss) {
    GameEngine::ins().recordEncounteredSpecies(species.id);
    ui.revealRouteSpecies(species.id);
    wild = &species;
    battleIsBoss = boss;
    battleAllowsFriendship = true;
    wildRuntime = GameEngine::ins().createMonster(wild->id, level);
    wildHpMax = wildRuntime.hpMax;
    wildHp = wildHpMax;
    ui.state.battleCursor = 0;
    battleFoodBond = 0;
    clearFriendshipFlow();
    defeatAwaitInput = false;
    battlePlayerSlot = routeLeaderSlot(GameEngine::ins().gameState());
    // Resolve the battle pack and both active species before the first battle
    // frame. Resource decompression must not run inside the render hot path.
    GameAssets::preloadBattleAssets();
    const uint16_t battleSpecies[2] = {
        battlePlayerMonster().speciesId,
        wild->id,
    };
    PokemonSprites::preloadDynamicSpecies(battleSpecies, 2, 2);
    battleCosts.begin(GameEngine::ins().gameState(), battlePlayerSlot, boss);
    pendingBattleSwitchSlot = 0xFF;
    BattleSystem::resetVolatile(playerBattleState);
    BattleSystem::resetVolatile(wildBattleState);
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    forcedBattleEndPending = false;
    battleTurnController.reset();
    fleeAttempts = 0;
    autoWalkActive = false;
    phase = Phase::ENCOUNTER;
    AudioManager::ins().setMusic(
        boss ? MusicTrack::BATTLE_SPECIAL : MusicTrack::BATTLE);
    clearBattleLogs();
    enqueueBattleLog(battleIsBoss ? Ui::Explore::BOSS_APPEARED
                                  : Ui::Explore::WILD_APPEARED);
    BattleSystem::EffectResolution entryEffects;
    BattleSystem::applyEntryAbility(
        battlePlayerSpecies(), playerBattleState, *wild, wildBattleState,
        entryEffects);
    enqueueBattleEffectLogs(entryEffects, false);
    entryEffects = BattleSystem::EffectResolution{};
    BattleSystem::applyEntryAbility(
        *wild, wildBattleState, battlePlayerSpecies(), playerBattleState,
        entryEffects);
    enqueueBattleEffectLogs(entryEffects, true);
}

void ExploreScene::promptOrBeginRouteBoss() {
    if (!routeBossPending) return;
    if (expeditionSpecialKind != ExploreSpecial::Kind::NONE &&
        ExploreSpecial::configFor(expeditionSpecialKind).optional) {
        autoWalkActive = false;
        ui.state.specialChallengeYes = true;
        phase = Phase::SPECIAL_PROMPT;
        return;
    }
    beginRouteBossEncounter();
}

void ExploreScene::beginRouteBossEncounter() {
    if (!routeBossPending || !finalRoute()) return;
    const Species* boss = findSpecies(expeditionBossSpeciesId);
    routeBossPending = false;
    if (!boss) {
        Platform::errorf("[ExploreBoss] missing species=%u area=%u\n",
                      expeditionBossSpeciesId, mapBlocks[currentMapBlock]);
        return;
    }
    encounterCooldownSteps = ENCOUNTER_COOLDOWN_STEP_COUNT;
    Platform::logf("[ExploreBoss] encounter area=%u species=%u level=%u "
                  "exp=%u%% special=%u\n",
                  mapBlocks[currentMapBlock], expeditionBossSpeciesId,
                  expeditionBossLevel, expeditionBossExperiencePercent,
                  static_cast<unsigned>(expeditionSpecialKind));
    beginEncounter(*boss, expeditionBossLevel, true);
    if (expeditionSpecialKind != ExploreSpecial::Kind::NONE) {
        battleAllowsFriendship =
            ExploreSpecial::configFor(expeditionSpecialKind).allowsFriendship;
    }
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void ExploreScene::beginDebugEncounter() {
    const Species* table = speciesTable();
    uint8_t count = speciesCount();
    const Species& opponent = count > 0
        ? table[static_cast<uint8_t>(GameRandom::random(0, count))]
        : starterSpecies();
    uint8_t level = ExploreEncounterRules::levelForRoll(
        WILD_LEVEL_MIN, WILD_LEVEL_MAX, GameEngine::ins().activeMonster().level,
        static_cast<uint8_t>(GameRandom::random(0, 100)));
    Platform::logf("[DebugBattle] opponent=%u level=%u\n", opponent.id, level);
    beginEncounter(opponent, level);
}
#endif

void ExploreScene::rollEncounter() {
    const RouteMap& map = routeMap(mapBlocks[currentMapBlock]);
    // 趟内只在活跃池快照中滚点（§7.8）；快照为空时退回全表兜底
    const EncounterEntry* encounter = nullptr;
    if (activePool.count > 0) {
        const ExplorePool::PoolEntry* picked = rollPoolEntry(activePool);
        encounter = picked ? findEncounterEntry(map, picked->speciesId) : nullptr;
    } else {
        encounter = rollEncounterEntry(map);
    }
    const Species* opponent = encounter ? findSpecies(encounter->speciesId) : nullptr;
    if (!opponent) opponent = &starterSpecies();
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    uint8_t targetLevel = ExploreEncounterRules::targetLevel(
        map.averageLevel, map.depthSpread, currentMapBlock, mapBlockCount,
        routeIndex, path.pointCount);
    uint8_t wildLevel = encounter
        ? ExploreEncounterRules::levelForRoll(
            encounter->minLevel, encounter->maxLevel, targetLevel,
            static_cast<uint8_t>(GameRandom::random(0, 100)))
        : targetLevel;
    if (!encounter) {
        Platform::logf("[Explore] empty encounter table area=%u\n",
                      static_cast<unsigned>(activeArea));
    }
    beginEncounter(*opponent, wildLevel);
}

void ExploreScene::clearBattleLogs() {
    battleLogHead = 0;
    battleLogCount = 0;
    battleLogVisibleCount = 0;
    battleLogUntil = 0;
    battleLogActive = false;
    battleResultPending = false;
    fleeExitPending = false;
    battleExpVisible = false;
    expAnimationPending = false;
    expAnimationActive = false;
    expAnimationFrom = 0;
    expAnimationTo = 0;
    expAnimationStarted = 0;
    battleTurnStage = BattleTurnStage::IDLE;
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleActionIndex = 0;
    battleActionAttackerWild = false;
    battleActionSelfHit = false;
    battlePendingAction.reset();
    battleHpFrom = 0;
    battleHpTo = 0;
    battleActionStarted = 0;
    foodThrowActive = false;
    foodThrowIndex = 0;
    foodThrowStarted = 0;
    pendingBattleSwitchSlot = 0xFF;
    battleSwitchStage = BattleSwitchStage::NONE;
    battleSwitchStarted = 0;
    battleSwitchConsumesTurn = false;
    for (uint8_t i = 0; i < BATTLE_LOG_QUEUE_CAP; ++i) {
        battleLogCues[i] = BattleLogCue::NONE;
    }
    for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
        battleLogVisible[i][0] = '\0';
    }
}

void ExploreScene::enqueueBattleLog(const char* text, BattleLogCue cue) {
    if (!text || !text[0]) return;
    if (battleLogCount >= BATTLE_LOG_QUEUE_CAP) {
        if (battleLogCues[battleLogHead] == BattleLogCue::EXP_GAIN) {
            expAnimationPending = false;
        }
        battleLogCues[battleLogHead] = BattleLogCue::NONE;
        battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
        battleLogCount--;
    }
    uint8_t tail = (battleLogHead + battleLogCount) % BATTLE_LOG_QUEUE_CAP;
    strncpy(battleLogQueue[tail], text, BATTLE_LOG_LEN - 1);
    battleLogQueue[tail][BATTLE_LOG_LEN - 1] = '\0';
    battleLogCues[tail] = cue;
    battleLogCount++;
    serviceBattleLog(Hal::ins().millis());
}

ExploreScene::BattleLogUpdate ExploreScene::serviceBattleLog(uint32_t nowMs) {
    if (battleLogActive && (int32_t)(nowMs - battleLogUntil) < 0) return false;
    if (battleLogCount == 0) {
        bool playbackEnded = battleLogActive;
        bool changed = battleLogActive || battleLogVisibleCount > 0;
        battleLogActive = false;

        // Keep the two-line context visible while the player answers a
        // friendship question. The next queued message will scroll it out.
        if (phase == Phase::FRIENDSHIP) {
            switch (friendshipStep) {
            case FriendshipStep::CONTACT_INTRO:
                friendshipStep = FriendshipStep::CONTACT_CONFIRM;
                ui.state.friendshipConfirmYes = true;
                return true;
            case FriendshipStep::CONTACT_ACQUIRED:
                if (GameEngine::ins().gameState().teamCount < Game::TEAM_CAP) {
                    friendshipStep = FriendshipStep::TEAM_CONFIRM;
                    ui.state.friendshipConfirmYes = true;
                    return true;
                }
                break;
            case FriendshipStep::CONTACT_CONFIRM:
            case FriendshipStep::TEAM_CONFIRM:
                return playbackEnded;
            case FriendshipStep::TEAM_JOINED:
            case FriendshipStep::FINISHING:
                break;
            }
        }

        battleLogVisibleCount = 0;
        for (uint8_t i = 0; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
            battleLogVisible[i][0] = '\0';
        }
        if (fleeExitPending) {
            fleeExitPending = false;
            clearFriendshipFlow();
#if STICKMON_ENABLE_DEBUG_FEATURES
            if (debugBattleMode) {
                returnToDebugMenu();
                return {true, true};
            }
            resumeWalk();
#else
            resumeWalk();
#endif
            return true;
        }
        if (battleResultPending) {
            battleResultPending = false;
            if (friendshipOfferPending) {
                clearFriendshipFlow();
                phase = Phase::FRIENDSHIP;
                char logBuf[BATTLE_LOG_LEN];
                snprintf(logBuf, sizeof(logBuf),
                         Ui::Explore::FRIEND_RECOGNIZES_FMT,
                         wild ? wild->name : "");
                enqueueBattleLog(logBuf);
                enqueueBattleLog(Ui::Explore::FRIEND_CONTACT_QUESTION);
                return true;
            }
            finishBattleVictoryFlow();
            return true;
        }
        if (phase == Phase::FRIENDSHIP) {
            switch (friendshipStep) {
            case FriendshipStep::CONTACT_ACQUIRED:
                clearFriendshipFlow();
                finishBattleVictoryFlow();
                return true;
            case FriendshipStep::TEAM_JOINED:
            case FriendshipStep::FINISHING:
                clearFriendshipFlow();
                finishBattleVictoryFlow();
                return true;
            case FriendshipStep::CONTACT_INTRO:
            case FriendshipStep::CONTACT_CONFIRM:
            case FriendshipStep::TEAM_CONFIRM:
                break;
            }
        }
        return changed;
    }

    uint8_t line = 0;
    if (battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP) {
        line = battleLogVisibleCount++;
    } else {
        for (uint8_t i = 1; i < BATTLE_LOG_VISIBLE_CAP; ++i) {
            strncpy(battleLogVisible[i - 1], battleLogVisible[i], BATTLE_LOG_LEN);
        }
        line = BATTLE_LOG_VISIBLE_CAP - 1;
    }
    uint8_t queueIndex = battleLogHead;
    strncpy(battleLogVisible[line], battleLogQueue[queueIndex], BATTLE_LOG_LEN - 1);
    battleLogVisible[line][BATTLE_LOG_LEN - 1] = '\0';
    BattleLogCue cue = battleLogCues[queueIndex];
    battleLogCues[queueIndex] = BattleLogCue::NONE;
    battleLogHead = (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP;
    battleLogCount--;
    if (cue == BattleLogCue::EXP_GAIN) {
        startExpAnimation(nowMs);
    } else if (cue == BattleLogCue::LEVEL_UP) {
        AudioManager::ins().playSfx(SfxCue::LEVEL_UP);
    } else if (cue == BattleLogCue::MOVE_LEARNT) {
        AudioManager::ins().playSfx(SfxCue::MOVE_LEARNT);
    } else if (cue == BattleLogCue::FAINT) {
        AudioManager::ins().playSfx(SfxCue::FAINT);
    } else if (cue == BattleLogCue::CONTACT) {
        AudioManager::ins().playSfx(SfxCue::CONTACT);
    }
    battleLogActive = true;
    battleLogUntil = nowMs + 1000;
    return true;
}

bool ExploreScene::battleLogBusy() const {
    return battleLogActive || battleLogCount > 0 || battleResultPending ||
           fleeExitPending || expAnimationPending ||
           expAnimationActive || foodThrowActive ||
           battleSwitchStage != BattleSwitchStage::NONE ||
           battleTurnStage != BattleTurnStage::IDLE;
}

bool ExploreScene::battleLogPlaybackBusy() const {
    return battleLogActive || battleLogCount > 0;
}

void ExploreScene::prepareExpAnimation(uint32_t fromExp, uint32_t toExp) {
    expAnimationFrom = fromExp;
    expAnimationTo = toExp;
    expAnimationStarted = 0;
    expAnimationActive = false;
    expAnimationPending = toExp > fromExp;
}

void ExploreScene::startExpAnimation(uint32_t nowMs) {
    battleExpVisible = true;
    AudioManager::ins().playSfx(SfxCue::EXP_GAIN);
    if (!expAnimationPending) return;
    expAnimationPending = false;
    expAnimationActive = true;
    expAnimationStarted = nowMs;
}

void ExploreScene::updateExpAnimation(uint32_t nowMs) {
    if (!expAnimationActive || nowMs < expAnimationStarted) return;
    if (nowMs - expAnimationStarted >= EXP_ANIMATION_MS) {
        expAnimationActive = false;
        AudioManager::ins().playSfx(SfxCue::EXP_FULL);
    }
}

bool ExploreScene::enterPendingProgression(Phase returnPhase) {
    progressionReturnPhase = returnPhase;
    if (GameEngine::ins().hasPendingLevelUp()) {
        phase = Phase::LEVEL_UP;
        return true;
    }
    if (GameEngine::ins().hasPendingEvolution()) {
        phase = Phase::EVOLUTION;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveReplacement()) {
        phase = Phase::MOVE_REPLACED;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveLearn()) {
        ProgressionUi::resetMoveLearnState(moveLearnState);
        phase = Phase::LEARN_MOVE;
        return true;
    }
    return false;
}

void ExploreScene::enqueueBattleProgressionLogs(uint8_t teamSlot) {
    auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return;
    const Species& species = engine.speciesFor(state.team[teamSlot]);
    char logBuf[BATTLE_LOG_LEN];

    if (engine.hasPendingLevelUp()) {
        uint8_t level = engine.pendingLevelUpLevel();
        engine.acknowledgePendingLevelUp();
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::LEVEL_UP_LOG_FMT,
                 species.name, level);
        enqueueBattleLog(logBuf, BattleLogCue::LEVEL_UP);
    }

    while (engine.hasPendingMoveLearn()) {
        uint8_t learnerSlot = engine.pendingMoveLearnSlot();
        if (learnerSlot >= state.teamCount || learnerSlot >= Game::TEAM_CAP) {
            engine.resolvePendingMoveLearn(false);
            continue;
        }
        if (engine.pendingMoveLearnNeedsReplacement()) {
            break;
        }
        const Species& learnerSpecies = engine.speciesFor(state.team[learnerSlot]);
        const MoveInfo* move = findMove(engine.pendingMoveLearnId());
        bool learned = engine.resolvePendingMoveLearn(true);
        if (!learned || !move) continue;
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_LEARNED_LOG_FMT,
                 learnerSpecies.name, move->name);
        enqueueBattleLog(logBuf, BattleLogCue::MOVE_LEARNT);
    }
}

void ExploreScene::enqueueBattleEffectLogs(const BattleSystem::EffectResolution& effects,
                                           bool attackerWild) {
    if (!wild) return;
    const Species& playerSpecies = battlePlayerSpecies();
    auto statusLabel = [](Game::MajorStatus status) -> const char* {
        switch (status) {
        case Game::MajorStatus::POISON: return Ui::Status::STATUS_POISON;
        case Game::MajorStatus::TOXIC: return Ui::Status::STATUS_TOXIC;
        case Game::MajorStatus::PARALYSIS: return Ui::Status::STATUS_PARALYSIS;
        case Game::MajorStatus::SLEEP: return Ui::Status::STATUS_SLEEP;
        case Game::MajorStatus::BURN: return Ui::Status::STATUS_BURN;
        case Game::MajorStatus::FREEZE: return Ui::Status::STATUS_FREEZE;
        default: return Ui::Status::STATUS_OK;
        }
    };

    char logBuf[BATTLE_LOG_LEN];
    for (uint8_t index = 0; index < effects.count; ++index) {
        const BattleSystem::EffectOutcome& outcome = effects.outcomes[index];
        bool targetWild = outcome.target == MoveEffectTarget::ATTACKER
            ? attackerWild
            : !attackerWild;
        const char* targetName = targetWild ? wild->name : playerSpecies.name;
        const char* defenderName = attackerWild ? playerSpecies.name : wild->name;
        const bool defenderOwnsAbility = outcome.ability == AbilityId::STATIC ||
            outcome.ability == AbilityId::EFFECT_SPORE ||
            outcome.ability == AbilityId::SYNCHRONIZE;
        switch (outcome.kind) {
        case BattleSystem::EffectOutcomeKind::STATUS_APPLIED:
        case BattleSystem::EffectOutcomeKind::YAWN_SLEEP:
            if (outcome.ability != AbilityId::NONE) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                         defenderOwnsAbility ? defenderName : targetName,
                         abilityName(outcome.ability));
                enqueueBattleLog(logBuf);
            }
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::STATUS_APPLIED_FMT,
                     targetName, statusLabel(outcome.status));
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_FAILED:
            enqueueBattleLog(Ui::Explore::NO_EFFECT);
            break;
        case BattleSystem::EffectOutcomeKind::CONFUSED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::CONFUSED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::FLINCHED:
            break;
        case BattleSystem::EffectOutcomeKind::BOUND:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::BOUND_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::STAT_CHANGED: {
            uint8_t statIndex = static_cast<uint8_t>(outcome.stat);
            if (statIndex >= static_cast<uint8_t>(BattleStat::COUNT)) break;
            if (outcome.ability != AbilityId::NONE) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                         outcome.ability == AbilityId::INTIMIDATE
                             ? (attackerWild ? wild->name : playerSpecies.name)
                             : targetName,
                         abilityName(outcome.ability));
                enqueueBattleLog(logBuf);
            }
            snprintf(logBuf, sizeof(logBuf), outcome.stageDelta > 0
                         ? Ui::Explore::STAT_ROSE_FMT
                         : Ui::Explore::STAT_FELL_FMT,
                     targetName, Ui::Explore::STAT_NAMES[statIndex]);
            enqueueBattleLog(logBuf);
            break;
        }
        case BattleSystem::EffectOutcomeKind::DRAINED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::DRAINED_FMT, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::RECOIL:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::RECOIL_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::HEALED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::HEALED_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::CURED:
            if (outcome.ability != AbilityId::NONE) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                         targetName, abilityName(outcome.ability));
                enqueueBattleLog(logBuf);
            }
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::CURED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_CLEARED:
        case BattleSystem::EffectOutcomeKind::BIND_ENDED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::BIND_ENDED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::YAWNED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::YAWNED_FMT, targetName);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_DAMAGE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::STATUS_DAMAGE_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_DAMAGE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::BIND_DAMAGE_FMT,
                     targetName, outcome.amount);
            enqueueBattleLog(logBuf);
            break;
        case BattleSystem::EffectOutcomeKind::ABILITY_ACTIVATED:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                     targetName, abilityName(outcome.ability));
            enqueueBattleLog(logBuf);
            if (outcome.amount > 0) {
                snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABSORB_HEAL_FMT,
                         targetName, outcome.amount);
                enqueueBattleLog(logBuf);
            }
            break;
        }
    }
}

void ExploreScene::updateBattleTurn(uint32_t nowMs) {
    if (phase != Phase::ENCOUNTER) return;
    switch (battleTurnStage) {
    case BattleTurnStage::IDLE:
        return;
    case BattleTurnStage::WAIT_ACTION_START:
        if (!battleLogPlaybackBusy()) beginBattleAction();
        return;
    case BattleTurnStage::ANIMATING:
        if (nowMs - battleActionStarted < BATTLE_ACTION_MS) return;
        applyBattleDamage();
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    case BattleTurnStage::WAIT_ACTION_LOGS:
        if (!battleLogPlaybackBusy()) finishBattleAction();
        return;
    case BattleTurnStage::WAIT_END_TURN_LOGS:
        if (!battleLogPlaybackBusy()) finishBattleEndTurn();
        return;
    }
}

void ExploreScene::beginBattleAction() {
    if (!wild || battleActionIndex >= battleTurnPlan.count) {
        battleTurnStage = BattleTurnStage::IDLE;
        return;
    }

    auto& engine = GameEngine::ins();
    auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    const BattleTurnController::Action& plannedAction =
        battleTurnPlan.actions[battleActionIndex];
    battleActionAttackerWild =
        plannedAction.side == BattleTurnController::Side::WILD;
    battleActionSelfHit = false;
    battlePendingAction.reset();
    wildRuntime.hpCur = wildHp;

    Game::MonsterRuntime& attacker = battleActionAttackerWild ? wildRuntime : activeMon;
    const Species& attackerSpecies = battleActionAttackerWild ? *wild : activeSpecies;
    BattleSystem::BattleActorState& attackerState = battleActionAttackerWild
        ? wildBattleState : playerBattleState;
    BattleSystem::BattleActorState& defenderState = battleActionAttackerWild
        ? playerBattleState : wildBattleState;
    Game::MonsterRuntime& defender = battleActionAttackerWild ? activeMon : wildRuntime;
    const Species& defenderSpecies = battleActionAttackerWild ? activeSpecies : *wild;
    uint8_t sideIndex = battleActionAttackerWild ? 1 : 0;
    Game::MoveId moveId = plannedAction.moveId;
    const char* attackerName = attackerSpecies.name;
    char logBuf[BATTLE_LOG_LEN];

    uint8_t opponentIndex = sideIndex == 0 ? 1 : 0;
    const BattleTurnController::Action* opponentAction =
        battleTurnPlan.actionFor(battleActionAttackerWild
            ? BattleTurnController::Side::PLAYER
            : BattleTurnController::Side::WILD);
    Game::MoveId opponentMoveId = opponentAction ? opponentAction->moveId : 0;
    const MoveInfo* opponentMove = findMove(opponentMoveId);
    BattleSystem::DamageContext damageContext;
    damageContext.attackerMovesSecond = battleActionIndex > 0;
    damageContext.defenderDamagedThisTurn = battleTurnDamaged[opponentIndex];
    damageContext.defenderMoveIsDamaging = opponentMove && opponentMove->power > 0 &&
        opponentMove->damageClass != DamageClass::STATUS;
    damageContext.allowForceWildEnd = !battleActionAttackerWild;
    BattleActionService::prepare(
        battlePendingAction, attacker, attackerSpecies, attackerState,
        defender, defenderSpecies, defenderState, plannedAction.specialSlot,
        moveId, damageContext);
    const auto& battleActionCheck = battlePendingAction.check;
    const auto& battleActionResult = battlePendingAction.damage;
    if (!battleActionAttackerWild) engine.markDirty(SaveUrgency::DEFERRED);
    if (battleActionCheck.wokeUp) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WOKE_UP_FMT, attackerName);
        enqueueBattleLog(logBuf);
    }
    if (battleActionCheck.thawed) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::THAWED_FMT, attackerName);
        enqueueBattleLog(logBuf);
    }
    if (battleActionCheck.confusionEnded) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::CONFUSION_ENDED_FMT, attackerName);
        enqueueBattleLog(logBuf);
    }

    if (battlePendingAction.kind == BattleActionService::Kind::BLOCKED) {
        switch (battleActionCheck.blockReason) {
        case BattleSystem::ActionBlockReason::FLINCH:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::FLINCHED_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::SLEEP:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ASLEEP_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::FREEZE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::FROZEN_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::PARALYSIS:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::PARALYZED_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::CONFUSION_SELF_HIT:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::CONFUSION_HIT_FMT, attackerName);
            break;
        case BattleSystem::ActionBlockReason::RECHARGE:
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::RECHARGING_FMT,
                     attackerName);
            break;
        case BattleSystem::ActionBlockReason::MOVE_FAILED:
            snprintf(logBuf, sizeof(logBuf), "%s", Ui::Explore::MOVE_FAILED);
            break;
        default:
            snprintf(logBuf, sizeof(logBuf), "%s", battleActionAttackerWild
                         ? Ui::Explore::WILD_CANNOT_MOVE
                         : Ui::Explore::CANNOT_MOVE);
            break;
        }
        enqueueBattleLog(logBuf);
        if (battleActionCheck.blockReason == BattleSystem::ActionBlockReason::CONFUSION_SELF_HIT &&
            battleActionCheck.selfDamage > 0) {
            battleActionSelfHit = true;
            battleHpFrom = attacker.hpCur;
            battleHpTo = battleActionCheck.selfDamage >= battleHpFrom
                ? 0
                : battleHpFrom - battleActionCheck.selfDamage;
            battleActionStarted = Hal::ins().millis();
            battleTurnStage = BattleTurnStage::ANIMATING;
        } else {
            applyBattleDamage();
            battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        }
        return;
    }

    if (battlePendingAction.kind == BattleActionService::Kind::CHARGING) {
        if (moveId == 76) {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::SOLAR_BEAM_CHARGE_FMT,
                     attackerName);
        } else {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_CHARGE_FMT,
                     attackerName);
        }
        enqueueBattleLog(logBuf);
        if (battlePendingAction.defenseRaised) {
            uint8_t defense = static_cast<uint8_t>(BattleStat::DEFENSE);
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::STAT_ROSE_FMT,
                     attackerName, Ui::Explore::STAT_NAMES[defense]);
            enqueueBattleLog(logBuf);
        }
        applyBattleDamage();
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    const MoveInfo* move = findMove(battleActionResult.moveId);
    if (battleActionAttackerWild) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::WILD_MOVE_USED_FMT,
                 move ? move->name : Ui::Status::MOVE_UNKNOWN);
    } else {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::MOVE_USED_FMT,
                 activeSpecies.name, move ? move->name : Ui::Status::MOVE_UNKNOWN);
    }
    enqueueBattleLog(logBuf);

    if (battleActionResult.failed && move) {
        applyBattleDamage();
        enqueueBattleLog(Ui::Explore::MOVE_FAILED);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }

    if (battleActionResult.missed) {
        applyBattleDamage();
        enqueueBattleLog(Ui::Explore::MOVE_MISSED);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    if (battleActionResult.effectiveness == 0) {
        applyBattleDamage();
        enqueueBattleLog(Ui::Explore::NO_EFFECT);
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    if (battleActionResult.critical) enqueueBattleLog(Ui::Explore::CRITICAL_HIT);
    if (battleActionResult.effectiveness > 100) {
        enqueueBattleLog(Ui::Explore::SUPER_EFFECTIVE);
    } else if (battleActionResult.effectiveness < 100) {
        enqueueBattleLog(Ui::Explore::NOT_VERY_EFFECTIVE);
    }

    battleHpFrom = battleActionAttackerWild ? activeMon.hpCur : wildHp;
    battleHpTo = battleActionResult.damage >= battleHpFrom
        ? 0
        : battleHpFrom - battleActionResult.damage;
    if (battleActionResult.damage == 0) {
        // Expose the committed status action's visual clock without delaying its rules or logs.
        battleActionStarted = Hal::ins().millis();
        applyBattleDamage();
        battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS;
        return;
    }
    snprintf(logBuf, sizeof(logBuf), battleActionAttackerWild
                 ? Ui::Explore::WILD_DAMAGE_FMT
                 : Ui::Explore::DAMAGE_FMT,
             battleActionResult.damage);
    enqueueBattleLog(logBuf);
    if (battleActionResult.hitCount > 1) {
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::HIT_COUNT_FMT,
                 battleActionResult.hitCount);
        enqueueBattleLog(logBuf);
    }
    battleActionStarted = Hal::ins().millis();
    battleTurnStage = BattleTurnStage::ANIMATING;
}

void ExploreScene::applyBattleDamage() {
    auto& engine = GameEngine::ins();
    auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    wildRuntime.hpCur = wildHp;

    Game::MonsterRuntime& attacker = battleActionAttackerWild ? wildRuntime : activeMon;
    const Species& attackerSpecies = battleActionAttackerWild ? *wild : activeSpecies;
    BattleSystem::BattleActorState& attackerState = battleActionAttackerWild
        ? wildBattleState : playerBattleState;
    Game::MonsterRuntime& defender = battleActionAttackerWild ? activeMon : wildRuntime;
    const Species& defenderSpecies = battleActionAttackerWild ? activeSpecies : *wild;
    BattleSystem::BattleActorState& defenderState = battleActionAttackerWild
        ? playerBattleState : wildBattleState;
    const bool defenderCanStillAct = battleTurnPlan.hasActionAfter(
        battleActionIndex,
        battleActionAttackerWild ? BattleTurnController::Side::PLAYER
                                 : BattleTurnController::Side::WILD);
    if (!BattleActionService::commit(
            battlePendingAction, attacker, attackerSpecies, attackerState,
            defender, defenderSpecies, defenderState, defenderCanStillAct,
            !battleActionAttackerWild)) {
        return;
    }
    const auto& result = battlePendingAction.result;
    const auto& damage = battlePendingAction.damage;
    wildHp = wildRuntime.hpCur;

    if (battlePendingAction.kind == BattleActionService::Kind::BLOCKED) {
        if (result.selfDamage > 0) {
            AudioManager::ins().playSfx(SfxCue::DAMAGE_NORMAL);
            battleTurnDamaged[battleActionAttackerWild ? 1 : 0] = true;
            if (!battleActionAttackerWild) engine.markDirty(SaveUrgency::DEFERRED);
        }
        return;
    }
    if (battlePendingAction.kind != BattleActionService::Kind::READY ||
        damage.failed || damage.missed) {
        return;
    }
    if (damage.effectiveness == 0) {
        if (!damage.absorbed && damage.activatedAbility != AbilityId::NONE) {
            char logBuf[BATTLE_LOG_LEN];
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                     defenderSpecies.name, abilityName(damage.activatedAbility));
            enqueueBattleLog(logBuf);
        }
        enqueueBattleEffectLogs(result.absorbEffects, battleActionAttackerWild);
        if (result.defenderHpAfter != result.defenderHpBefore) {
            engine.markDirty(SaveUrgency::DEFERRED);
        }
        return;
    }

    if (result.actualDamage > 0) {
        SfxCue damageCue = damage.effectiveness > 100
            ? SfxCue::DAMAGE_SUPER
            : (damage.effectiveness < 100
                ? SfxCue::DAMAGE_WEAK : SfxCue::DAMAGE_NORMAL);
        AudioManager::ins().playSfx(damageCue);
        battleTurnDamaged[battleActionAttackerWild ? 0 : 1] = true;
    }
    enqueueBattleEffectLogs(result.moveEffects, battleActionAttackerWild);
    enqueueBattleEffectLogs(result.abilityEffects, battleActionAttackerWild);
    if (result.rampageEnded) {
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::RAMPAGE_CONFUSED_FMT,
                 attackerSpecies.name);
        enqueueBattleLog(logBuf);
    }
    if (damage.sturdyActivated) {
        char logBuf[BATTLE_LOG_LEN];
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::ABILITY_ACTIVATED_FMT,
                 defenderSpecies.name, abilityName(AbilityId::STURDY));
        enqueueBattleLog(logBuf);
    }
    if (result.forceWildEnd && wildHp > 0) {
        forcedBattleEndPending = true;
        enqueueBattleLog(Ui::Explore::DRAGON_TAIL_END);
    }
    engine.markDirty(SaveUrgency::DEFERRED);
}

void ExploreScene::finishBattleAction() {
    bool playerFainted = battlePlayerMonster().hpCur == 0;
    bool wildFainted = wildHp == 0;
    if (playerFainted || wildFainted) {
        battleTurnStage = BattleTurnStage::IDLE;
        battleTurnPlan = BattleTurnController::TurnPlan{};
        battleActionIndex = 0;
        if (playerFainted) finishPlayerFaint();
        else finishWildFaint();
        return;
    }
    if (forcedBattleEndPending) {
        forcedBattleEndPending = false;
        clearFriendshipFlow();
        finishRoamingEncounter();
        resumeWalk();
        return;
    }

    ++battleActionIndex;
    if (battleActionIndex < battleTurnPlan.count) {
        battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    } else {
        resolveBattleEndTurn();
    }
}

void ExploreScene::resolveBattleEndTurn() {
    auto& engine = GameEngine::ins();
    BattleSystem::EffectResolution playerEffects = BattleSystem::resolveEndTurn(
        battlePlayerMonster(), battlePlayerSpecies(), playerBattleState);
    for (uint8_t index = 0; index < playerEffects.count; ++index) {
        playerEffects.outcomes[index].target = MoveEffectTarget::ATTACKER;
    }
    enqueueBattleEffectLogs(playerEffects, false);

    wildRuntime.hpCur = wildHp;
    BattleSystem::EffectResolution wildEffects = BattleSystem::resolveEndTurn(
        wildRuntime, *wild, wildBattleState);
    wildHp = wildRuntime.hpCur;
    for (uint8_t index = 0; index < wildEffects.count; ++index) {
        wildEffects.outcomes[index].target = MoveEffectTarget::ATTACKER;
    }
    enqueueBattleEffectLogs(wildEffects, true);
    engine.markDirty(SaveUrgency::DEFERRED);

    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    if (battleLogPlaybackBusy()) {
        battleTurnStage = BattleTurnStage::WAIT_END_TURN_LOGS;
    } else {
        finishBattleEndTurn();
    }
}

void ExploreScene::finishBattleEndTurn() {
    battleTurnStage = BattleTurnStage::IDLE;
    if (wildHp == 0) {
        finishWildFaint();
    } else if (battlePlayerMonster().hpCur == 0) {
        finishPlayerFaint();
    } else if (BattleSystem::isChargingMove(playerBattleState)) {
        beginChargedBattleTurn();
    } else if (playerBattleState.lockedMoveId != 0) {
        beginChargedBattleTurn();
    }
}

void ExploreScene::beginChargedBattleTurn() {
    if (!wild || wildHp == 0 || battlePlayerMonster().hpCur == 0) return;
    auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    battleTurnPlan = battleTurnController.planAiTurn(
        activeMon, activeSpecies, playerBattleState,
        wildRuntime, *wild, wildBattleState);
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::markFirstSpecialVictory() {
    if (!ExploreSpecial::isFirstBoss(expeditionSpecialKind)) return;
    uint8_t bit = ExploreSpecial::defeatedBit(expeditionSpecialKind);
    if (bit == 0) return;

    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    if ((state.specialBossDefeatedMask & bit) != 0) return;
    state.specialBossDefeatedMask =
        static_cast<uint8_t>(state.specialBossDefeatedMask | bit);
    engine.markDirty(SaveUrgency::IMMEDIATE);
}

void ExploreScene::finishRoamingEncounter() {
    if (specialRelocationHandled ||
        !ExploreSpecial::isRoaming(expeditionSpecialKind)) {
        return;
    }
    int8_t index = ExploreSpecial::roamingIndex(expeditionSpecialKind);
    if (index < 0 || index >= ExploreSpecial::ROAMER_COUNT) return;

    auto& engine = GameEngine::ins();
    ++engine.gameState().roamingRerollCounts[index];
    specialRelocationHandled = true;
    engine.markDirty(SaveUrgency::IMMEDIATE);
}

void ExploreScene::finishWildFaint() {
    if (!wild) return;
    auto& engine = GameEngine::ins();
    auto& activeMon = battlePlayerMonster();
    uint8_t activeSlot = battlePlayerSlot;
    uint16_t expGain = BattleSystem::experienceReward(*wild, wildRuntime.level);
    if (battleIsBoss) {
        expGain = BattleSystem::scaledExperienceReward(
            expGain, expeditionBossExperiencePercent);
    }
    uint8_t reserveSlot = 0xFF;
    const Game::GameState& state = engine.gameState();
    for (uint8_t slot = 0; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == activeSlot) continue;
        const Game::MonsterRuntime& teammate = state.team[slot];
        if (!teammate.fainted && teammate.hpCur > 0) {
            reserveSlot = slot;
            break;
        }
    }
    BattleSystem::ExperienceAwards expAwards = BattleSystem::experienceAwards(
        expGain, reserveSlot != 0xFF);
    uint16_t reserveExpGain = expAwards.reserve;
    uint16_t activeExpGain = expAwards.active;
    uint32_t expBefore = activeMon.exp;
    engine.grantEffortToTeamMember(activeSlot, *wild);
    if (reserveSlot != 0xFF) {
        engine.grantEffortToTeamMember(reserveSlot, *wild);
    }
    uint32_t activeExpAwarded =
        engine.addExperienceToTeamMember(activeSlot, activeExpGain);
    prepareExpAnimation(expBefore, battlePlayerMonster().exp);
    battleResultPending = true;
    enqueueBattleLog(Ui::Explore::BATTLE_WIN);
    if (battleIsBoss) enqueueBattleLog(Ui::Explore::BOSS_DEFEATED);
    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::EXP_GAIN_FMT,
             static_cast<unsigned>(activeExpAwarded));
    enqueueBattleLog(logBuf, BattleLogCue::EXP_GAIN);
    enqueueBattleProgressionLogs(activeSlot);

    if (reserveSlot != 0xFF && reserveExpGain > 0) {
        uint32_t awarded = engine.addExperienceToTeamMember(
            reserveSlot, reserveExpGain);
        if (awarded > 0) {
            snprintf(logBuf, sizeof(logBuf), Ui::Explore::SHARED_EXP_GAIN_FMT,
                     engine.speciesFor(state.team[reserveSlot]).name,
                     static_cast<unsigned>(awarded));
            enqueueBattleLog(logBuf);
            enqueueBattleProgressionLogs(reserveSlot);
        }
    }

    uint8_t coinReward = ExploreBoss::victoryCoinReward(battleIsBoss);
    if (coinReward > 0) {
        engine.addCoins(coinReward);
        snprintf(logBuf, sizeof(logBuf), Ui::Explore::COIN_GAIN_FMT,
                 static_cast<unsigned>(coinReward));
        enqueueBattleLog(logBuf);
    }
    markFirstSpecialVictory();
    finishRoamingEncounter();
    Game::FriendshipService::OfferResult friendshipOffer =
        Game::FriendshipService::evaluateOffer(
            state, *wild, wildRuntime, battleIsBoss,
            battleAllowsFriendship, battleFoodBond,
#if STICKMON_ENABLE_DEBUG_FEATURES
            debugBattleMode
#else
            false
#endif
        );
    friendshipOfferPending = friendshipOffer.offered;

    // 栖息地轮换：击败区域头目 → 该区域重抽计数 +1 并立即重建活跃池（§7.2）
    if (battleIsBoss) {
        uint8_t bossArea = mapBlocks[currentMapBlock];
        if (bossArea < Game::EXPLORE_AREA_COUNT) {
            uint8_t& rerollCount =
                engine.gameState().explorePoolRerollCounts[bossArea];
            rerollCount = rerollCount == UINT8_MAX
                ? 1 : static_cast<uint8_t>(rerollCount + 1);
            if (expeditionNormalBossScheduled) {
                ExploreBossPity::resetArea(engine.gameState(), bossArea);
            }
            engine.markDirty(SaveUrgency::DEFERRED);
            snapshotActivePool();
        }
    }

    // 按物种独立累计失败层数；加成已叠加在最终结交概率上（§7.9.4）。
    if (friendshipOffer.eligible && !friendshipOfferPending) {
        Game::FriendshipService::recordFailure(
            engine.gameState(), wild->id);
        engine.markDirty(SaveUrgency::DEFERRED);
    }
}

void ExploreScene::attackWild() {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE) return;
    auto& activeMon = battlePlayerMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }

    const Species& activeSpecies = battlePlayerSpecies();
    battleTurnPlan = battleTurnController.planAiTurn(
        activeMon, activeSpecies, playerBattleState,
        wildRuntime, *wild, wildBattleState);
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::wildCounterattack() {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE) return;
    const auto& activeMon = battlePlayerMonster();
    if (activeMon.hpCur == 0 || activeMon.fainted) {
        finishPlayerFaint();
        return;
    }
    battleTurnPlan = battleTurnController.planWildOnly(
        activeMon, battlePlayerSpecies(), playerBattleState,
        wildRuntime, *wild, wildBattleState);
    battleActionIndex = 0;
    battleTurnStage = BattleTurnStage::WAIT_ACTION_START;
    updateBattleTurn(Hal::ins().millis());
}

void ExploreScene::throwFood(uint8_t foodIndex) {
    if (!wild || wildHp == 0 || battleTurnStage != BattleTurnStage::IDLE ||
        foodThrowActive) {
        return;
    }
    if (foodIndex >= Game::ROOM_FOOD_COUNT) foodIndex = 0;

    foodThrowActive = true;
    foodThrowIndex = foodIndex;
    foodThrowStarted = Hal::ins().millis();
    AudioManager::ins().playSfx(SfxCue::THROW);

    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::FOOD_THROW_FMT,
             wild->name, Ui::Room::FOOD_NAMES[foodIndex]);
    enqueueBattleLog(logBuf);
}

bool ExploreScene::updateFoodThrow(uint32_t nowMs) {
    if (!foodThrowActive) return false;
    if (phase != Phase::ENCOUNTER || !wild || wildHp == 0) {
        foodThrowActive = false;
        return true;
    }
    if (nowMs - foodThrowStarted < FOOD_THROW_DURATION_MS) return false;

    foodThrowActive = false;
    finishFoodThrow();
    return true;
}

void ExploreScene::finishFoodThrow() {
    if (!wild || wildHp == 0) return;

    FoodTuning::ThrowClass throwClass =
        FriendshipSystem::classifyFoodThrow(foodThrowIndex, wildRuntime.nature);
    bool accepted = FriendshipSystem::acceptsFoodThrow(
        battleIsBoss, throwClass, static_cast<uint8_t>(GameRandom::random(0, 100)));
    char logBuf[BATTLE_LOG_LEN];
    if (!accepted) {
        snprintf(logBuf, sizeof(logBuf),
                 throwClass == FoodTuning::ThrowClass::DISLIKED
                     ? Ui::Explore::FOOD_REFUSED_DISLIKED_FMT
                     : Ui::Explore::FOOD_REFUSED_FMT,
                 wild->name);
        enqueueBattleLog(logBuf);
        wildCounterattack();
        return;
    }

    battleFoodBond = FriendshipSystem::addFoodBond(
        battleFoodBond, FriendshipSystem::throwBondGain(throwClass));
    snprintf(logBuf, sizeof(logBuf),
             throwClass == FoodTuning::ThrowClass::LIKED
                 ? Ui::Explore::FOOD_ACCEPTED_LIKED_FMT
                 : Ui::Explore::FOOD_ACCEPTED_FMT,
             wild->name);
    enqueueBattleLog(logBuf);
    resolveBattleEndTurn();
}

void ExploreScene::switchBattleMonster() {
    const Game::GameState& state = GameEngine::ins().gameState();
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& candidate = state.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            beginBattleSwitch(slot, true);
            return;
        }
    }
    enqueueBattleLog(Ui::Explore::NO_SWITCH_TARGET);
}

void ExploreScene::beginBattleSwitch(uint8_t slot, bool consumesTurn) {
    const auto& state = GameEngine::ins().gameState();
    if (battleSwitchStage != BattleSwitchStage::NONE ||
        slot == battlePlayerSlot || slot >= state.teamCount ||
        slot >= Game::TEAM_CAP ||
        state.team[slot].fainted || state.team[slot].hpCur == 0) {
        if (consumesTurn) enqueueBattleLog(Ui::Explore::NO_SWITCH_TARGET);
        return;
    }

    pendingBattleSwitchSlot = slot;
    battleSwitchConsumesTurn = consumesTurn;
    battleSwitchStage = BattleSwitchStage::RETREATING;
    battleSwitchStarted = Hal::ins().millis();
    ui.state.battleCursor = 0;
}

void ExploreScene::updateBattleSwitch(uint32_t nowMs) {
    if (battleSwitchStage == BattleSwitchStage::NONE ||
        nowMs - battleSwitchStarted < BATTLE_SWITCH_PHASE_MS) {
        return;
    }

    if (battleSwitchStage == BattleSwitchStage::RETREATING) {
        uint8_t slot = pendingBattleSwitchSlot;
        const auto& state = GameEngine::ins().gameState();
        if (slot == 0xFF || slot >= state.teamCount ||
            slot >= Game::TEAM_CAP || state.team[slot].fainted ||
            state.team[slot].hpCur == 0) {
            bool consumedTurn = battleSwitchConsumesTurn;
            pendingBattleSwitchSlot = 0xFF;
            battleSwitchConsumesTurn = false;
            battleSwitchStage = BattleSwitchStage::NONE;
            if (consumedTurn) enqueueBattleLog(Ui::Explore::NO_SWITCH_TARGET);
            else defeatAwaitInput = true;
            return;
        }
        battlePlayerSlot = slot;
        battleCosts.enter(state, slot);
        battleTurnController.resetPlayerAi();
        pendingBattleSwitchSlot = 0xFF;
        BattleSystem::resetVolatile(playerBattleState);
        BattleSystem::EffectResolution entryEffects;
        BattleSystem::applyEntryAbility(
            battlePlayerSpecies(), playerBattleState, *wild, wildBattleState,
            entryEffects);
        enqueueBattleEffectLogs(entryEffects, false);
        battleSwitchStage = BattleSwitchStage::ENTERING;
        battleSwitchStarted = nowMs;
        return;
    }

    bool consumedTurn = battleSwitchConsumesTurn;
    battleSwitchConsumesTurn = false;
    battleSwitchStage = BattleSwitchStage::NONE;

    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::SWITCH_IN_FMT,
             battlePlayerSpecies().name);
    enqueueBattleLog(logBuf);
    if (consumedTurn) wildCounterattack();
}

void ExploreScene::finishPlayerFaint() {
    if (defeatAwaitInput) return;
    auto& engine = GameEngine::ins();
    const char* faintedName = battlePlayerSpecies().name;
    uint32_t loss =
        engine.applyFaintPenaltyToTeamMember(battlePlayerSlot);
    char logBuf[BATTLE_LOG_LEN];
    snprintf(logBuf, sizeof(logBuf), Ui::Explore::FAINTED_FMT,
             faintedName);
    enqueueBattleLog(logBuf, BattleLogCue::FAINT);
    snprintf(resultBuf, sizeof(resultBuf), Ui::Explore::FAINTED_EXP_LOSS_FMT, (unsigned long)loss);
    enqueueBattleLog(resultBuf);
    resultMessage = resultBuf;
    clearFriendshipFlow();

    const Game::GameState& state = engine.gameState();
    pendingBattleSwitchSlot = 0xFF;
    for (uint8_t slot = 0; slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& candidate = state.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            pendingBattleSwitchSlot = slot;
            break;
        }
    }

    if (pendingBattleSwitchSlot != 0xFF) {
        beginBattleSwitch(pendingBattleSwitchSlot, false);
        return;
    }

    enqueueBattleLog(Ui::Explore::BATTLE_LOST);
    finishRoamingEncounter();
    defeatAwaitInput = true;
}

void ExploreScene::finishBattleVictoryFlow() {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleMode) {
        phase = Phase::RESULT;
        resultMessage = Ui::Explore::BATTLE_WIN;
        enterPendingProgression(Phase::RESULT);
        return;
    }
#endif
    resumeWalk();
    enterPendingProgression(Phase::WALKING);
}

void ExploreScene::clearFriendshipFlow() {
    friendshipOfferPending = false;
    ui.state.friendshipConfirmYes = true;
    friendshipStep = FriendshipStep::CONTACT_INTRO;
    friendshipContactIndex = 0xFF;
}

void ExploreScene::resolveFriendshipOffer(bool confirmed) {
    auto& engine = GameEngine::ins();
    if (!wild) {
        clearFriendshipFlow();
        finishBattleVictoryFlow();
        return;
    }

    switch (friendshipStep) {
    case FriendshipStep::CONTACT_INTRO:
        return;
    case FriendshipStep::CONTACT_CONFIRM: {
        if (!confirmed) {
            clearFriendshipFlow();
            finishBattleVictoryFlow();
            return;
        }
        uint8_t contactSlot = 0xFF;
#if STICKMON_ENABLE_DEBUG_FEATURES
        uint8_t metArea = debugBattleMode
            ? Game::MET_AREA_UNKNOWN
            : mapBlocks[currentMapBlock];
#else
        uint8_t metArea = mapBlocks[currentMapBlock];
#endif
        if (!engine.recordFriendContact(wildRuntime, metArea, &contactSlot)) {
            friendshipStep = FriendshipStep::FINISHING;
            enqueueBattleLog(Ui::Explore::FRIEND_CONTACTS_FULL);
            return;
        }
        friendshipContactIndex = contactSlot;
        friendshipStep = FriendshipStep::CONTACT_ACQUIRED;
        // 结交成功只清零当前物种，其他物种的累积不受影响。
        {
            Game::GameState& save = engine.gameState();
            int8_t pityIndex = FriendshipPity::indexFor(wild->id);
            if (pityIndex >= 0 &&
                save.friendshipPityFailCounts[pityIndex] != 0) {
                Game::FriendshipService::recordSuccess(save, wild->id);
                engine.markDirty(SaveUrgency::DEFERRED);
            }
        }
        snprintf(resultBuf, sizeof(resultBuf),
                 Ui::Explore::FRIEND_CONTACT_ACQUIRED_FMT, wild->name);
        enqueueBattleLog(resultBuf, BattleLogCue::CONTACT);
        if (engine.gameState().teamCount < Game::TEAM_CAP) {
            enqueueBattleLog(Ui::Explore::FRIEND_TEAM_QUESTION);
        }
        return;
    }
    case FriendshipStep::CONTACT_ACQUIRED:
        return;
    case FriendshipStep::TEAM_CONFIRM:
        if (!confirmed) {
            clearFriendshipFlow();
            finishBattleVictoryFlow();
            return;
        }
        if (friendshipContactIndex != 0xFF &&
            engine.inviteContactToTeam(friendshipContactIndex) ==
                ContactInviteResult::JOINED) {
            friendshipContactIndex = 0xFF;
            initializeRouteFollowerPosition(true);
            friendshipStep = FriendshipStep::TEAM_JOINED;
            snprintf(resultBuf, sizeof(resultBuf),
                     Ui::Explore::FRIEND_TEAM_JOINED_FMT, wild->name);
            enqueueBattleLog(resultBuf);
            return;
        }
        friendshipStep = FriendshipStep::FINISHING;
        enqueueBattleLog(Ui::Storage::TEAM_FULL_TOAST);
        return;
    case FriendshipStep::TEAM_JOINED:
    case FriendshipStep::FINISHING:
        return;
    }
}

void ExploreScene::fleeEncounter() {
    if (!wild) return;
    if (!BattleSystem::canFlee(playerBattleState)) {
        enqueueBattleLog(Ui::Explore::BOUND_CANNOT_FLEE);
        wildCounterattack();
        return;
    }
    const auto& activeMon = battlePlayerMonster();
    const Species& activeSpecies = battlePlayerSpecies();
    uint16_t activeSpeed = BattleSystem::effectiveSpeed(
        activeMon, activeSpecies, playerBattleState);
    uint16_t wildSpeed = BattleSystem::effectiveSpeed(
        wildRuntime, *wild, wildBattleState);
    bool escaped = wildSpeed == 0 || activeSpeed >= wildSpeed;
    if (!escaped) {
        fleeAttempts++;
        uint32_t odds = ((uint32_t)activeSpeed * 128) / wildSpeed + 30UL * fleeAttempts;
        escaped = odds >= 256 || GameRandom::random(0, 256) < odds;
    }

    if (!escaped) {
        enqueueBattleLog(Ui::Explore::FLEE_FAILED);
        wildCounterattack();
        return;
    }

    clearFriendshipFlow();
    finishRoamingEncounter();
    fleeExitPending = true;
    enqueueBattleLog(Ui::Explore::FLEE_SUCCESS);
}

void ExploreScene::snapshotActivePool() {
    // 活跃池由 (时段序号, 区域ID, 重抽计数) 派生种子确定性重建，不进存档（§7.6）
    serviceAreaPoolCache(0);
    uint8_t areaIndex = static_cast<uint8_t>(activeArea);
    if (areaIndex >= ROUTE_MAP_COUNT) {
        activePool.count = 0;
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
        return;
    }
    activePool = buildAreaPool(areaIndex);

    memset(areaPreloadSpeciesIds, 0, sizeof(areaPreloadSpeciesIds));
    areaPreloadSpeciesCount = 0;
    for (uint8_t i = 0; i < activePool.count; ++i) {
        uint16_t speciesId = activePool.entries[i].speciesId;
        bool duplicate = false;
        for (uint8_t j = 0; j < areaPreloadSpeciesCount; ++j) {
            if (areaPreloadSpeciesIds[j] == speciesId) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && speciesId != 0) {
            areaPreloadSpeciesIds[areaPreloadSpeciesCount++] = speciesId;
        }
    }
    PokemonSprites::setDynamicSceneSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount);
    const uint8_t routeCount = ui.routeSpriteSpecies(
        activePool, areaPreloadSpeciesIds, AREA_PRELOAD_CAP);
    if (routeCount) {
        areaPreloadSpeciesCount = routeCount;
        if (expeditionBossSpeciesId && areaPreloadSpeciesCount < AREA_PRELOAD_CAP &&
            std::find(areaPreloadSpeciesIds, areaPreloadSpeciesIds + areaPreloadSpeciesCount,
                expeditionBossSpeciesId) == areaPreloadSpeciesIds + areaPreloadSpeciesCount)
            areaPreloadSpeciesIds[areaPreloadSpeciesCount++] = expeditionBossSpeciesId;
        // Prioritize this route without releasing cached previews for other areas.
        uint16_t pinned[AREA_PRELOAD_CAP] = {};
        uint16_t previews[AREA_PRELOAD_CAP] = {};
        uint8_t pinnedCount = areaPreloadSpeciesCount;
        std::copy_n(areaPreloadSpeciesIds, pinnedCount, pinned);
        const uint8_t previewCount = collectAreaPoolSpecies(previews, AREA_PRELOAD_CAP);
        for (uint8_t i = 0; i < previewCount && pinnedCount < AREA_PRELOAD_CAP; ++i) {
            if (std::find(pinned, pinned + pinnedCount, previews[i]) == pinned + pinnedCount)
                pinned[pinnedCount++] = previews[i];
        }
        PokemonSprites::setPinnedDynamicSpecies(pinned, pinnedCount);
        PokemonSprites::preloadDynamicSpecies(areaPreloadSpeciesIds, areaPreloadSpeciesCount);
    }
    areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount, 0);
    areaPreviewNextLoadAt = Hal::ins().millis() + 1;
}

bool ExploreScene::ownsSpecies(uint16_t speciesId) const {
    const Game::GameState& state = GameEngine::ins().gameState();
    for (uint8_t i = 0; i < state.teamCount && i < Game::TEAM_CAP; ++i) {
        if (state.team[i].speciesId == speciesId) return true;
    }
    for (uint8_t i = 0; i < state.storageCount && i < Game::STORAGE_CAP; ++i) {
        if (state.storage[i].speciesId == speciesId) return true;
    }
    return false;
}

ExploreSpecial::Kind ExploreScene::specialKindForArea(uint8_t area) const {
    const auto& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    uint32_t slotIndex = ExploreSpecial::slotIndexFor(
        engine.gameMinutesTotal());
    return ExploreSpecial::kindForArea(
        area,
        state.specialBossDefeatedMask,
        slotIndex,
        state.roamingRerollCounts,
        ownsSpecies(ExploreSpecial::MEW));
}

void ExploreScene::resetWalk() {
    exploreCosts.begin();
    battleCosts = Game::ExploreCost::Battle{};
    phase = Phase::WALKING;
    generateMapBlocks();
    snapshotActivePool();
    currentMapBlock = 0;
    pendingFrostLadder = false;
    pendingFrostFall = false;
    frostFallAtMs = 0;
    steps = 0;
    mapEncounterCount = 0;
    encounterCooldownSteps = 0;
    if (!resetRouteSegment()) {
        requestExploreExit(false, false);
        return;
    }
    resumeWalk();
}

void ExploreScene::resumeWalk() {
    settleExploreCosts(false);
    phase = Phase::WALKING;
    AudioManager::ins().setMusic(MusicTrack::EXPLORE);
    battleIsBoss = false;
    wild = nullptr;
    wildHp = wildHpMax = 0;
    battleResultPending = false;
    fleeExitPending = false;
    battleExpVisible = false;
    clearFriendshipFlow();
    resultMessage = nullptr;
    exploreMenuOpen = false;
    exploreSubViewOpen = false;
    exploreMenuOpenedAt = 0;
    routeMoving = false;
    autoWalkActive = false;
    routePausedForTouch = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
}

void ExploreScene::initializeRouteFollowerPosition(bool useTrailPosition) {
    routeFollowerWalkDirection = routeWalkDirection;
    ui.routeVisual.routeFollowerVisualWalkDirection = routeFollowerWalkDirection;
    routeFollowerWorldX = routeWorldX;
    routeFollowerWorldY = routeWorldY;

    if (generatedMap.pathCount > 0 &&
        currentRoutePath < generatedMap.pathCount &&
        useTrailPosition &&
        routeIndex >= ROUTE_FOLLOWER_GAP_STEPS) {
        const ExploreMapGenerator::Path& path =
            generatedMap.paths[currentRoutePath];
        RouteWorldPoint trail = routePathPointWorld(
            path, routeFollowerTargetIndex(routeIndex));
        routeFollowerWorldX = trail.x;
        routeFollowerWorldY = trail.y;
        routeFollowerWalkDirection = routeDirectionForDelta(
            routeWorldX - routeFollowerWorldX,
            routeWorldY - routeFollowerWorldY,
            routeWalkDirection);
        ui.routeVisual.routeFollowerVisualWalkDirection = routeFollowerWalkDirection;
    } else if (ExploreRouteGeometry::hasValleyForestCenterline(&generatedMap)) {
        const auto start = ExploreRouteGeometry::valleyForestFollowerStart(
            generatedMap, {routeWorldX, routeWorldY}, ROUTE_FOLLOWER_START_OFFSET);
        routeFollowerWorldX = start.x;
        routeFollowerWorldY = start.y;
    } else {
        PokemonSprites::WalkDirection direction =
            static_cast<PokemonSprites::WalkDirection>(routeWalkDirection);
        if (direction == PokemonSprites::WalkDirection::UP ||
            direction == PokemonSprites::WalkDirection::DOWN) {
            float candidate = routeWorldX + ROUTE_FOLLOWER_START_OFFSET;
            if (candidate > EXPLORE_MAP_W - EXPLORE_TILE_SIZE * 0.5f) {
                candidate = routeWorldX - ROUTE_FOLLOWER_START_OFFSET;
            }
            routeFollowerWorldX = candidate;
        } else {
            float candidate = routeWorldY + ROUTE_FOLLOWER_START_OFFSET;
            if (candidate > EXPLORE_MAP_H - EXPLORE_TILE_SIZE * 0.5f) {
                candidate = routeWorldY - ROUTE_FOLLOWER_START_OFFSET;
            }
            routeFollowerWorldY = candidate;
        }
    }

    routeFollowerFromX = routeFollowerWorldX;
    routeFollowerFromY = routeFollowerWorldY;
    routeFollowerTargetX = routeFollowerWorldX;
    routeFollowerTargetY = routeFollowerWorldY;
    routeFollowerMoving = false;
}

bool ExploreScene::resetRouteSegment() {
    routePausedForTouch = false;
    uint8_t mapIndex = mapBlocks[currentMapBlock];
    uint32_t mapSeed = ExploreMapGenerator::deriveSeed(
        expeditionSeed, currentMapBlock, mapIndex);
    bool generated = mapIndex == ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA &&
        ExploreMapGenerator::generateValleyRegion(expeditionSeed, pendingValley,
            currentMapBlock + 1 >= mapBlockCount, generatedMap);
    for (uint8_t attempt = 0;
         mapIndex != ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA &&
         attempt < sizeof(MAP_GENERATION_RETRY_SALTS) /
                       sizeof(MAP_GENERATION_RETRY_SALTS[0]);
         ++attempt) {
        uint32_t candidateSeed = mapSeed ^ MAP_GENERATION_RETRY_SALTS[attempt];
        if (candidateSeed == 0) candidateSeed = MAP_GENERATION_SAFE_SEED;
        if (!ExploreMapGenerator::generate(
                candidateSeed, pendingEntryEdge, mapIndex, generatedMap,
                {currentMapBlock, mapBlockCount, pendingFrostLadder})) {
            continue;
        }
        generated = true;
        break;
    }
    if (!generated && mapIndex != ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA) {
        generated = ExploreMapGenerator::generate(
            MAP_GENERATION_SAFE_SEED, pendingEntryEdge, mapIndex, generatedMap,
            {currentMapBlock, mapBlockCount, pendingFrostLadder});
    }
    if (!generated) {
        Platform::errorf("[ExploreMap] generation failed block=%u area=%u entry=%u\n",
                      currentMapBlock, mapIndex,
                      static_cast<unsigned>(pendingEntryEdge));
        return false;
    }
    Platform::logf("[ExploreMap] block=%u area=%u seed=%08lx fingerprint=%08lx "
                  "entry=%u coast=%u forest=%u creek=%u cliff=%u waterfall=%u\n",
                  currentMapBlock,
                  mapIndex,
                  static_cast<unsigned long>(generatedMap.seed),
                  static_cast<unsigned long>(ExploreMapGenerator::fingerprint(generatedMap)),
                  static_cast<unsigned>(generatedMap.entry.edge),
                  generatedMap.hasCoast ? 1 : 0,
                  generatedMap.hasForest ? 1 : 0,
                  generatedMap.hasCreek ? 1 : 0,
                  generatedMap.hasCliff ? 1 : 0,
                  generatedMap.hasWaterfall ? 1 : 0);
    mapEncounterCount = 0;
    prepareMapRoutes();
    if (generatedMap.pathCount == 0) return false;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    routeIndex = 0;
    routeMoving = false;
    iceSliding = false;
    iceSlideDx = 0;
    iceSlideDy = 0;
    routeWalkDirection = static_cast<uint8_t>(inwardDirection(generatedMap.entry.edge));
    ui.routeVisual.routeVisualWalkDirection = routeWalkDirection;
    mapTargetSteps = MathUtil::max<uint16_t>(1, path.pointCount - 1);
    RouteWorldPoint start = routePathPointWorld(path, 0);
    routeWorldX = start.x;
    routeWorldY = start.y;
    if (pendingFrostFall) {
        routeIndex = ExploreIceSlide::landingIndex(
            generatedMap, path, generatedMap.seed ^ expeditionSeed);
        RouteWorldPoint landing = routePathPointWorld(path, routeIndex);
        routeWorldX = landing.x;
        routeWorldY = landing.y;
        pendingFrostFall = false;
    }
    routeFromX = routeTargetX = routeWorldX;
    routeFromY = routeTargetY = routeWorldY;
    initializeRouteFollowerPosition(false);
    placeRouteBoss();
    placeRoutePickup();
    uint8_t hiddenMask = 0;
    for (uint8_t i = 0; i < activePool.count; ++i) {
        if (ExplorePool::isRare(activePool.entries[i].rarity) &&
            !GameEngine::ins().hasEncounteredSpecies(activePool.entries[i].speciesId))
            hiddenMask |= 1U << i;
    }
    ui.resetRouteVisual(activePool, hiddenMask);
    return true;
}

void ExploreScene::generateMapBlocks() {
    uint8_t selectedMap = static_cast<uint8_t>(activeArea);
    if (selectedMap >= ROUTE_MAP_COUNT) selectedMap = 0;
    const RouteMap& map = routeMap(selectedMap);
    auto& engine = GameEngine::ins();
    Game::GameState& state = engine.gameState();
    uint32_t currentPitySlot = ExploreSpecial::slotIndexFor(
        engine.gameMinutesTotal());
    if (ExploreBossPity::syncSlot(state, currentPitySlot)) {
        engine.markDirty(SaveUrgency::SOON);
    }
    expeditionPitySlotIndex = currentPitySlot;
    uint8_t pityMisses = state.normalBossMissCount[selectedMap];
    mapBlockCount = ExploreBossPity::requiresGuaranteedEligibleRun(pityMisses)
        ? map.maxMapCount
        : mapCountForRoll(
              map, static_cast<uint8_t>(GameRandom::random(0, 100)));
    bool bossLengthEligible = ExploreRunRules::allowsRegionalBoss(
        mapBlockCount, map.maxMapCount);
    expeditionSpecialKind = bossLengthEligible
        ? specialKindForArea(selectedMap)
        : ExploreSpecial::Kind::NONE;
    specialRelocationHandled = false;
    expeditionNormalBossScheduled = false;
    if (expeditionSpecialKind != ExploreSpecial::Kind::NONE) {
        ExploreSpecial::Config special =
            ExploreSpecial::configFor(expeditionSpecialKind);
        expeditionBossScheduled = true;
        expeditionBossSpeciesId = special.speciesId;
        expeditionBossLevel = ExploreSpecial::encounterLevel(
            expeditionSpecialKind,
            ExploreBoss::configForArea(selectedMap).level);
        expeditionBossExperiencePercent = special.experiencePercent;
    } else if (bossLengthEligible) {
        expeditionBossScheduled =
            GameRandom::random(0, ExploreBoss::SPAWN_ROLL_MAX) <
            ExploreBossPity::chanceForMisses(pityMisses);
        expeditionNormalBossScheduled = expeditionBossScheduled;
        expeditionBossSpeciesId = expeditionBossScheduled
            ? ExploreBoss::speciesForRoll(
                  selectedMap,
                  static_cast<uint32_t>(
                      GameRandom::random(0, ExploreBoss::CANDIDATE_COUNT)))
            : 0;
        const ExploreBoss::Config& normal =
            ExploreBoss::configForArea(selectedMap);
        expeditionBossLevel = normal.level;
        expeditionBossExperiencePercent = normal.experiencePercent;
    } else {
        expeditionBossScheduled = false;
        expeditionBossSpeciesId = 0;
        expeditionBossLevel = 0;
        expeditionBossExperiencePercent = 100;
    }
    for (uint8_t i = 0; i < MAP_BLOCK_CAP; ++i) {
        mapBlocks[i] = i < mapBlockCount ? selectedMap : 0xFF;
    }
    expeditionSeed = static_cast<uint32_t>(GameRandom::random(1, 0x7FFFFFFF));
    pendingEntryEdge = static_cast<ExploreMapGenerator::Edge>((expeditionSeed >> 8) & 0x03);
    pendingValley = {};
    currentRoutePath = 0;
    activeExitMask = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    Platform::logf("[ExploreRun] area=%u maps=%u maxMaps=%u bossEligible=%u "
                  "boss=%u normalBoss=%u pity=%u chance=%u bossSpecies=%u "
                  "bossLevel=%u special=%u seed=%08lx\n",
                  selectedMap, mapBlockCount,
                  map.maxMapCount,
                  bossLengthEligible ? 1 : 0,
                  expeditionBossScheduled ? 1 : 0,
                  expeditionNormalBossScheduled ? 1 : 0,
                  pityMisses,
                  ExploreBossPity::chanceForMisses(pityMisses),
                  expeditionBossSpeciesId,
                  expeditionBossLevel,
                  static_cast<unsigned>(expeditionSpecialKind),
                  static_cast<unsigned long>(expeditionSeed));
}

void ExploreScene::prepareMapRoutes() {
    uint8_t pathCount = MathUtil::min<uint8_t>(MAP_EXIT_CAP, generatedMap.pathCount);
    activeExitMask = 0;
    currentRoutePath = 0;
    for (uint8_t i = 0; i < MAP_EXIT_CAP; ++i) exitNextMaps[i] = 0xFF;
    if (pathCount == 0) return;

    for (uint8_t pathId = 0; pathId < pathCount; ++pathId) {
        activeExitMask = static_cast<uint8_t>(activeExitMask | (1U << pathId));
        if (!ExploreMapGenerator::finalPath(generatedMap, pathId, currentMapBlock, mapBlockCount))
            exitNextMaps[pathId] = generatedMap.valleyJourney ? generatedMap.areaIndex : mapBlocks[currentMapBlock + 1];
    }
    if (expeditionBossScheduled) {
        uint8_t bossPaths[MAP_EXIT_CAP];
        uint8_t bossPathCount = 0;
        for (uint8_t pathId = 0; pathId < pathCount; ++pathId) {
            if (ExploreMapGenerator::finalPath(generatedMap, pathId, currentMapBlock, mapBlockCount) &&
                ExploreBoss::canPlaceOnPath(generatedMap.paths[pathId].pointCount)) {
                bossPaths[bossPathCount++] = pathId;
            }
        }
        if (bossPathCount > 0) {
            currentRoutePath = bossPaths[GameRandom::random(0, bossPathCount)];
            return;
        }
    }
    currentRoutePath = GameRandom::random(0, pathCount);
}

void ExploreScene::placeRouteBoss() {
    routeBossIndex = 0;
    routeBossPending = false;
    if (!expeditionBossScheduled || expeditionBossSpeciesId == 0 ||
        !finalRoute()) {
        return;
    }

    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (!ExploreBoss::canPlaceOnPath(path.pointCount)) {
        Platform::logf("[ExploreBoss] no placement block=%u path=%u points=%u\n",
                      currentMapBlock, currentRoutePath, path.pointCount);
        return;
    }

    uint8_t preferred = ExploreBoss::routeIndex(path.pointCount);
    routeBossIndex = ExploreIceSlide::nearestNonIceIndex(
        generatedMap, path, preferred, 1,
        static_cast<uint8_t>(path.pointCount - 2));
    if (routeBossIndex == ExploreIceSlide::INVALID_INDEX) {
        Platform::logf("[ExploreBoss] no non-ice placement block=%u path=%u points=%u\n",
                      currentMapBlock, currentRoutePath, path.pointCount);
        routeBossIndex = 0;
        return;
    }
    routeBossPending = true;
    Platform::logf("[ExploreBoss] placed area=%u path=%u index=%u species=%u "
                  "level=%u exp=%u%%\n",
                  mapBlocks[currentMapBlock], currentRoutePath, routeBossIndex,
                  expeditionBossSpeciesId, expeditionBossLevel,
                  expeditionBossExperiencePercent);
}

void ExploreScene::placeRoutePickup() {
    routePickupAvailable = false;
    routePickupFinalReward = false;
    routePickupIndex = 0;
    routePickupItem = Pickup::NONE;
    routeGuaranteedEncounterIndex = 0;
    routeGuaranteedEncounterPending = false;
    const ExploreMapGenerator::Path& path = generatedMap.paths[currentRoutePath];
    if (path.pointCount < 2) return;
    if (routeBossPending) {
        Platform::logf("[ExploreEvent] block=%u type=boss index=%u\n",
                      currentMapBlock, routeBossIndex);
        return;
    }

    if (finalRoute() && !routeBossPending) {
        routePickupFinalReward = true;
        uint8_t preferred = path.pointCount >= 3
            ? static_cast<uint8_t>(path.pointCount - 2)
            : static_cast<uint8_t>(path.pointCount - 1);
        uint8_t last = path.pointCount >= 3
            ? static_cast<uint8_t>(path.pointCount - 2)
            : static_cast<uint8_t>(path.pointCount - 1);
        routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path, preferred, 1, last);
        if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) {
            routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
                generatedMap, path, preferred, 1,
                static_cast<uint8_t>(path.pointCount - 1));
        }
        if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        routePickupItem = rollPickupId(
            routeMap(mapBlocks[currentMapBlock]));
        if (routePickupItem == Pickup::NONE) routePickupItem = Pickup::COIN;
        routePickupAvailable = true;
        Platform::logf("[ExploreEvent] block=%u type=final-reward index=%u item=%u\n",
                      currentMapBlock, routePickupIndex, routePickupItem);
        return;
    }

    if (path.pointCount < 3) {
        routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path, static_cast<uint8_t>(path.pointCount - 1),
            1, static_cast<uint8_t>(path.pointCount - 1));
        if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        routePickupItem = rollPickupId(routeMap(mapBlocks[currentMapBlock]));
        routePickupAvailable = routePickupItem != Pickup::NONE;
        return;
    }

    bool choosePickup = GameRandom::random(0, 10000) < MAP_PICKUP_CHANCE;
    if (!choosePickup &&
        canScheduleGuaranteedEncounter(path.pointCount, encounterCooldownSteps)) {
        uint8_t first = static_cast<uint8_t>(encounterCooldownSteps + 1);
        uint8_t last = static_cast<uint8_t>(path.pointCount - 2);
        routeGuaranteedEncounterIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path,
            guaranteedEncounterIndex(path.pointCount, encounterCooldownSteps),
            first, last);
        if (routeGuaranteedEncounterIndex != ExploreIceSlide::INVALID_INDEX) {
            routeGuaranteedEncounterPending = true;
            Platform::logf("[ExploreEvent] block=%u type=battle index=%u cooldown=%u\n",
                          currentMapBlock, routeGuaranteedEncounterIndex,
                          encounterCooldownSteps);
            return;
        }
    }

    uint8_t first = MathUtil::max<uint8_t>(1, path.pointCount / 3);
    uint8_t last = MathUtil::min<uint8_t>(path.pointCount - 2, path.pointCount * 3 / 4);
    if (first > last) first = last = path.pointCount / 2;
    uint8_t preferred = static_cast<uint8_t>(GameRandom::random(first, last + 1));
    routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
        generatedMap, path, preferred, first, last);
    if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) {
        routePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            generatedMap, path, preferred, 1,
            static_cast<uint8_t>(path.pointCount - 2));
    }
    if (routePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
    routePickupItem = rollPickupId(routeMap(mapBlocks[currentMapBlock]));
    routePickupAvailable = routePickupItem != Pickup::NONE;
    Platform::logf("[ExploreEvent] block=%u type=item index=%u cooldown=%u\n",
                  currentMapBlock, routePickupIndex, encounterCooldownSteps);
}

bool ExploreScene::collectRoutePickup() {
    if (!routePickupAvailable || routeIndex != routePickupIndex) return false;
    routePickupAvailable = false;
    resolvePickup(routePickupItem);
    return true;
}

void ExploreScene::advanceMapBlock(uint8_t nextMap, bool fell) {
    if (finalRoute()) {
        requestExploreExit();
        return;
    }
    if (nextMap >= ROUTE_MAP_COUNT || (!generatedMap.valleyJourney && nextMap != mapBlocks[currentMapBlock + 1])) {
        requestExploreExit();
        return;
    }
    pendingEntryEdge = ExploreMapGenerator::opposite(
        generatedMap.paths[currentRoutePath].exit.edge);
    pendingFrostLadder = !fell && currentRoutePath == 1;
    pendingFrostFall = fell;
    if (generatedMap.valleyJourney) {
        const auto& path = generatedMap.paths[currentRoutePath];
        pendingValley = {path.nextValleyNode, path.nextValleyEntrance};
    }
    if (!ExploreMapGenerator::internalValleyTransfer(generatedMap, currentRoutePath)) ++currentMapBlock;
    if (!resetRouteSegment()) {
        requestExploreExit(false, false);
        return;
    }
    phase = Phase::WALKING;
    resultMessage = nullptr;
}

void ExploreScene::render() {
#if STICKMON_UI_ROUND_TOUCH
    if (market_ || exploreSubViewOpen || phase != Phase::SELECT) ui.releaseSelectionCache();
#endif
    if (market_) {
        marketUi_.render(*market_,Ui::MarketWalkUi::viewport(),GameEngine::ins().gameState().gameMinutesTotal);
        return;
    }
    if (exploreSubViewOpen) {
        exploreSubView.render();
        return;
    }
    ui.render();
}



ExplorePool::Pool ExploreScene::buildAreaPool(uint8_t areaIndex) {
    if (areaIndex >= ROUTE_MAP_COUNT) return ExplorePool::Pool{};

    const auto& engine = GameEngine::ins();
    return ExploreEncounters::poolForArea(
        areaIndex, engine.gameMinutesTotal(),
        engine.gameState().explorePoolRerollCounts[areaIndex]);
}

uint8_t ExploreScene::collectAreaPoolSpecies(uint16_t* speciesIds,
                                             uint8_t capacity,
                                             uint8_t priorityArea) {
    if (!speciesIds || capacity == 0) return 0;
    const auto& state = GameEngine::ins().gameState();
    uint8_t unlockedArea = ExploreItemProgression::unlockedArea(state);
    uint8_t count = 0;
    if (priorityArea < ROUTE_MAP_COUNT && priorityArea <= unlockedArea) {
        count = ExplorePool::appendUniqueSpecies(
            buildAreaPool(priorityArea), speciesIds, count, capacity);
    }
    for (uint8_t area = 0;
         area < ROUTE_MAP_COUNT && area <= unlockedArea; ++area) {
        if (area == priorityArea) continue;
        count = ExplorePool::appendUniqueSpecies(
            buildAreaPool(area), speciesIds, count, capacity);
    }
    return count;
}

bool ExploreScene::serviceAreaPoolCache(uint8_t loadBudget) {
    static uint32_t cachedSlotIndex = UINT32_MAX;
    static uint8_t cachedRerollCounts[Game::EXPLORE_AREA_COUNT] = {};
    static bool cacheSignatureValid = false;
    static uint16_t speciesIds[AREA_PRELOAD_CAP] = {};
    static uint8_t speciesCount = 0;
    static uint8_t firstPoolSpeciesCount = 0;
    static bool firstPoolReadyLogged = false;
    static bool allPoolsReadyLogged = false;

    auto& engine = GameEngine::ins();
    uint32_t slotIndex =
        ExplorePool::slotIndexFor(engine.gameMinutesTotal());
    const uint8_t* rerollCounts =
        engine.gameState().explorePoolRerollCounts;
    bool signatureChanged =
        !cacheSignatureValid || slotIndex != cachedSlotIndex;
    if (!signatureChanged) {
        for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
            if (rerollCounts[area] != cachedRerollCounts[area]) {
                signatureChanged = true;
                break;
            }
        }
    }
    if (signatureChanged) {
        memset(speciesIds, 0, sizeof(speciesIds));
        firstPoolSpeciesCount = buildAreaPool(0).count;
        speciesCount = collectAreaPoolSpecies(
            speciesIds, AREA_PRELOAD_CAP, 0);
        cachedSlotIndex = slotIndex;
        for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area) {
            cachedRerollCounts[area] = rerollCounts[area];
        }
        cacheSignatureValid = true;
        firstPoolReadyLogged = false;
        allPoolsReadyLogged = false;
        Platform::logf(
            "[ExplorePreload] pools reset slot=%lu species=%u rerolls=%u,%u,%u,%u,%u,%u\n",
            static_cast<unsigned long>(slotIndex), speciesCount,
            cachedRerollCounts[0], cachedRerollCounts[1],
            cachedRerollCounts[2], cachedRerollCounts[3],
            cachedRerollCounts[4], cachedRerollCounts[5]);
    }

    PokemonSprites::setPinnedDynamicSpecies(
        speciesIds, speciesCount);
    bool ready = PokemonSprites::preloadDynamicSpecies(
        speciesIds, speciesCount, loadBudget);
    bool firstPoolReady = PokemonSprites::preloadDynamicSpecies(
        speciesIds, firstPoolSpeciesCount, 0);
    if (firstPoolReady && !firstPoolReadyLogged) {
        firstPoolReadyLogged = true;
        Platform::logf(
            "[ExplorePreload] first_pool ready slot=%lu reroll=%u species=%u\n",
            static_cast<unsigned long>(slotIndex),
            cachedRerollCounts[0], firstPoolSpeciesCount);
    }
    if (ready && !allPoolsReadyLogged) {
        allPoolsReadyLogged = true;
        const auto& stats = PokemonSprites::cacheStats();
        Platform::logf(
            "[ExplorePreload] all_pools ready slot=%lu species=%u decoded=%lu free=%lu\n",
            static_cast<unsigned long>(slotIndex), speciesCount,
            static_cast<unsigned long>(stats.decodedBytes),
            static_cast<unsigned long>(stats.freePsram));
    } else if (!ready) {
        allPoolsReadyLogged = false;
    }
    return ready;
}

void ExploreScene::loadAreaPreview() {
    serviceAreaPoolCache(0);
    memset(areaPreviewFrames, 0, sizeof(areaPreviewFrames));
    memset(areaPreviewSpeciesIds, 0, sizeof(areaPreviewSpeciesIds));
    memset(areaPreloadSpeciesIds, 0, sizeof(areaPreloadSpeciesIds));
    areaPreloadSpeciesCount = 0;
    areaPreviewPool = ExplorePool::Pool{};
    areaPreviewStartedAt = Hal::ins().millis();
    ui.state.areaPreviewVisualCycle = UINT32_MAX;
    areaPreviewNextLoadAt = areaPreviewStartedAt + 80;
    areaPreviewLoadPending = false;
    areaPreviewReady = false;
    areaPreviewPaused = false;
    areaPreviewPausedElapsed = 0;
    const auto selected = ExploreDestinations::visibleId(ui.state.areaCursor, GameEngine::ins().gameState());
    if (selected == ExploreDestinations::MARKET) {
        const auto& d=MarketMapData::get();
        areaPreviewPool.count=std::min<uint8_t>(d.residentCount, ExplorePool::POOL_CAP);
        for(uint8_t i=0;i<areaPreviewPool.count;++i) {
            areaPreviewSpeciesIds[i]=d.residents[i].species;
            areaPreviewPool.entries[i].speciesId=d.residents[i].species;
        }
        PokemonSprites::setPinnedDynamicSpecies(areaPreviewSpeciesIds,areaPreviewPool.count);
        PokemonSprites::setDynamicSceneSpecies(areaPreviewSpeciesIds, areaPreviewPool.count);
        areaPreloadSpeciesCount = areaPreviewPool.count;
        std::copy_n(areaPreviewSpeciesIds, areaPreloadSpeciesCount, areaPreloadSpeciesIds);
        areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
            areaPreloadSpeciesIds, areaPreloadSpeciesCount, 0);
        refreshAreaPreviewFrames(); return;
    }
    if (selected == ExploreDestinations::INVALID) {
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
        return;
    }
    if (!ExploreItemProgression::isAreaUnlocked(
            selected, GameEngine::ins().gameState())) {
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
        return;
    }

    // 预览展示当前时段种子的活跃池全成员：所见即本趟可遭遇（§7.1/§7.5）
    areaPreviewPool = buildAreaPool(selected);
    for (uint8_t i = 0; i < areaPreviewPool.count; ++i) {
        areaPreviewSpeciesIds[i] = areaPreviewPool.entries[i].speciesId;
    }
    PokemonSprites::setDynamicSceneSpecies(
        areaPreviewSpeciesIds, areaPreviewPool.count);

    // Current-area members lead the queue. All six active pools remain pinned,
    // so changing the cursor never evicts a preview that was already loaded.
    areaPreloadSpeciesCount = collectAreaPoolSpecies(
        areaPreloadSpeciesIds, AREA_PRELOAD_CAP, selected);
    areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount, 0);
    areaPreviewStartedAt = Hal::ins().millis();
    areaPreviewNextLoadAt =
        areaPreviewStartedAt + PREVIEW_LOAD_AFTER_CURSOR_MS;
    refreshAreaPreviewFrames();
}

bool ExploreScene::updateAreaPreviewLoading(uint32_t nowMs) {
    if (!areaPreviewLoadPending ||
        static_cast<int32_t>(nowMs - areaPreviewNextLoadAt) < 0) return false;
#if STICKMON_UI_ROUND_TOUCH
    if (phase == Phase::SELECT && GameEngine::ins().idleModeActive()) {
        areaPreviewNextLoadAt = nowMs + 1000;
        return false;
    }
    const uint32_t elapsed = nowMs - areaPreviewStartedAt;
    const uint32_t cycle = elapsed % AREA_PREVIEW_CYCLE_MS;
    // Leave headroom for one synchronous species decode. Never start one in
    // motion, and extend the hold if an unexpectedly slow decode overruns it.
    const uint32_t guard = std::min<uint32_t>(AREA_PREVIEW_HOLD_MS / 2,
        std::max<uint32_t>(500, PokemonSprites::cacheStats().lastReloadMs + 100));
    if (phase == Phase::SELECT && areaPreviewReady && areaPreviewScrolls() &&
        cycle >= AREA_PREVIEW_HOLD_MS - guard) {
        areaPreviewNextLoadAt = nowMs + AREA_PREVIEW_CYCLE_MS - cycle;
        return false;
    }
#endif
    areaPreviewLoadPending = !PokemonSprites::preloadDynamicSpecies(
        areaPreloadSpeciesIds, areaPreloadSpeciesCount, 1);
    const bool wasReady = areaPreviewReady;
    if (phase == Phase::SELECT && ui.state.areaCursor < ExploreDestinations::COUNT)
        refreshAreaPreviewFrames();
    const uint32_t finished = Hal::ins().millis();
#if STICKMON_UI_ROUND_TOUCH
    if (!wasReady && areaPreviewReady) {
        areaPreviewStartedAt = finished;
        areaPreviewPausedElapsed = 0;
    } else if (phase == Phase::SELECT && areaPreviewReady && areaPreviewScrolls() &&
               cycle < AREA_PREVIEW_HOLD_MS &&
               finished - nowMs >= AREA_PREVIEW_HOLD_MS - cycle) {
        areaPreviewStartedAt = finished - (elapsed / AREA_PREVIEW_CYCLE_MS) * AREA_PREVIEW_CYCLE_MS;
    }
#endif
    areaPreviewNextLoadAt = finished + PREVIEW_BACKGROUND_LOAD_INTERVAL_MS;
#if STICKMON_UI_ROUND_TOUCH
    return !wasReady && areaPreviewReady;
#else
    (void)wasReady;
    return true;
#endif
}

void ExploreScene::refreshAreaPreviewFrames() {
    areaPreviewReady = PokemonSprites::preloadDynamicSpecies(
        areaPreviewSpeciesIds, areaPreviewPool.count, 0);
#if STICKMON_UI_ROUND_TOUCH
    // Commit the whole strip together; icon-to-front changes cannot resize a
    // moving strip. Missing files count as settled and retain a fixed slot.
    if (!areaPreviewReady) return;
#endif
    for (uint8_t i = 0; i < areaPreviewPool.count; ++i) {
        uint16_t speciesId = areaPreviewPool.entries[i].speciesId;
        areaPreviewFrames[i] = PokemonSprites::findCachedSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
        if (!areaPreviewFrames[i]) {
            areaPreviewFrames[i] = PokemonSprites::findCachedSpeciesSprite(
                speciesId, PokemonSprites::SpriteKind::ICON_0);
        }
    }
}


Ui::ExploreInputFrame ExploreScene::exploreInputFrame() const {
    Ui::ExploreInputFrame frame;
    frame.phase = phase;
    frame.areaCount = ExploreDestinations::COUNT;
    frame.visibleAreaCount = ExploreDestinations::visibleCount(
        GameEngine::ins().gameState());
    frame.friendshipConfirmation = friendshipStep == FriendshipStep::CONTACT_CONFIRM ||
        friendshipStep == FriendshipStep::TEAM_CONFIRM;
    frame.iceSliding = iceSliding;
    frame.routeBlocked = pendingFrostFall;
    frame.menuOpen = exploreMenuOpen;
    frame.battleBusy = battleLogBusy();
    frame.defeatAwaitInput = defeatAwaitInput;
#if STICKMON_ENABLE_DEBUG_FEATURES
    frame.debugBattle = debugBattleMode;
#endif
    return frame;
}

Ui::ExploreTutorialFrame ExploreScene::exploreTutorialFrame() const {
    Ui::ExploreTutorialFrame frame;
    const auto& engine = GameEngine::ins();
    if (!engine.gameState().oobeDone || exploreSubViewOpen || exploreMenuOpen) return frame;
    if (phase == Phase::ENCOUNTER && !battleLogBusy() && battleLogVisibleCount == 0 && !defeatAwaitInput &&
        !engine.tutorialComplete(Game::TutorialStep::BATTLE_ACTION)) {
        frame.kind = Ui::ExploreTutorialKind::BATTLE;
        frame.text = Ui::Tutorial::BATTLE_ACTION;
        return frame;
    }
    if (phase != Phase::WALKING) return frame;
    if (!engine.tutorialComplete(Game::TutorialStep::EXPLORE_WALK)) {
        frame.text = Ui::Tutorial::EXPLORE_WALK;
    } else if (!engine.tutorialComplete(Game::TutorialStep::EXPLORE_MENU)) {
        frame.button = 1;
        frame.kind = Ui::ExploreTutorialKind::MENU;
        frame.text = Ui::Tutorial::EXPLORE_MENU;
    }
    return frame;
}

void ExploreScene::renderExploreProgression() {
    auto& engine = GameEngine::ins();
    switch (phase) {
    case Phase::LEVEL_UP:
        ProgressionUi::renderLevelUp(engine.speciesFor(engine.activeMonster()).name,
                                     engine.pendingLevelUpLevel());
        break;
    case Phase::EVOLUTION:
        ProgressionUi::renderEvolution(engine.pendingEvolutionFromSpeciesId(),
            engine.pendingEvolutionToSpeciesId(), Hal::ins().millis());
        break;
    case Phase::EVOLUTION_CANCELLED:
        ProgressionUi::renderEvolutionCancelled(progressionCancelledSpeciesId,
                                                Hal::ins().millis());
        break;
    case Phase::LEARN_MOVE: ProgressionUi::renderMoveLearn(moveLearnState); break;
    case Phase::MOVE_REPLACED: ProgressionUi::renderMoveReplacement(); break;
    default: break;
    }
}

bool ExploreScene::activateExplore(Ui::ExploreAction action, uint8_t value) {
    switch (action) {
    case Ui::ExploreAction::RETURN_HOME: completeExploreReturn(false); return true;
    case Ui::ExploreAction::RESUME_WALK: resumeWalk(); return true;
    case Ui::ExploreAction::MENU_RETURN_PAUSE:
        if (phase != Phase::WALKING || !exploreMenuOpen || iceSliding || pendingFrostFall)
            return false;
        closeExploreMenu();
        if (!routePausedForTouch) routePausedAt = Hal::ins().millis();
        routePausedForTouch = true;
        autoWalkActive = false;
        return true;
    case Ui::ExploreAction::FRIENDSHIP_CONFIRM:
        resolveFriendshipOffer(value != 0);
        return true;
    case Ui::ExploreAction::SPECIAL_CONFIRM:
        if (value) beginRouteBossEncounter();
        else {
            routeBossPending = false;
            phase = Phase::WALKING;
        }
        return true;
    case Ui::ExploreAction::MENU_ITEM: {
        const auto item = AppSceneFlow::exploreMenuEntry(value).item;
        if (item == AppSceneFlow::ExploreMenuItem::TEAM) {
            cancelTouch();
            exploreSubView.openExploreTeamView();
            exploreSubViewOpen = true;
        } else if (item == AppSceneFlow::ExploreMenuItem::BAG) {
            cancelTouch();
            exploreSubView.openExploreBagView();
            exploreSubViewOpen = true;
        } else if (item == AppSceneFlow::ExploreMenuItem::END) {
            requestExploreExit(false, false);
        } else closeExploreMenu();
        return true;
    }
    case Ui::ExploreAction::OPEN_MENU:
        exploreMenuOpen = true;
        exploreMenuOpenedAt = Hal::ins().millis();
        GameEngine::ins().completeTutorial(Game::TutorialStep::EXPLORE_MENU);
        return true;
    case Ui::ExploreAction::EXIT: requestExploreExit(); return true;
    case Ui::ExploreAction::AREA_BACK:
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    case Ui::ExploreAction::DEPART: {
        const auto& state = GameEngine::ins().gameState();
        const auto destination = ExploreDestinations::visibleId(value, state);
        if (destination == ExploreDestinations::MARKET) {
            return GameEngine::ins().beginExploreDeparture(destination);
        }
        if (!ExploreDestinations::unlocked(destination, state)) return true;
        const int8_t hungry = Game::ExploreCost::hungrySlot(state);
        if (hungry >= 0) {
            snprintf(departureToast, sizeof(departureToast), Ui::Explore::HUNGRY_FMT,
                     GameEngine::ins().speciesFor(state.team[hungry]).name);
            departureToastUntil = Hal::ins().millis() + 2000;
            return true;
        }
        // Success replaces this scene synchronously; it needs no local update.
        GameEngine::ins().beginExploreDeparture(destination);
        return true;
    }
    case Ui::ExploreAction::AREA_CHANGED: loadAreaPreview(); return true;
    case Ui::ExploreAction::WALK:
        beginAutoWalk();
        GameEngine::ins().completeTutorial(Game::TutorialStep::EXPLORE_WALK);
        return true;
    case Ui::ExploreAction::BATTLE_DEFEAT:
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugBattleMode) returnToDebugMenu();
        else requestExploreExit(true);
#else
        requestExploreExit(true);
#endif
        return true;
    case Ui::ExploreAction::BATTLE_COMMAND:
        GameEngine::ins().completeTutorial(Game::TutorialStep::BATTLE_ACTION);
        AudioManager::ins().playSfx(SfxCue::UI_CONFIRM);
        if (value == 0) attackWild();
        else if (value == 1) {
            cancelTouch();
            exploreSubView.openBattleBagView(wild ? wild->name : nullptr, battlePlayerSlot);
            exploreSubViewOpen = true;
        } else if (value == 2) switchBattleMonster();
        else fleeEncounter();
        return true;
    case Ui::ExploreAction::BATTLE_CURSOR:
        AudioManager::ins().playSfx(SfxCue::UI_CURSOR);
        return true;
    case Ui::ExploreAction::RESULT_CONTINUE: {
        const auto& path = generatedMap.paths[currentRoutePath];
        if (routeIndex + 1 >= path.pointCount) beginRouteExit();
        else {
            resumeWalk();
            beginAutoWalk();
        }
        return true;
    }
    case Ui::ExploreAction::DEBUG_RETURN:
#if STICKMON_ENABLE_DEBUG_FEATURES
        returnToDebugMenu();
#endif
        return true;
    }
    return false;
}

Ui::ExploreRouteFrame ExploreScene::exploreRouteFrame() const {
    Ui::ExploreRouteFrame frame;
    const auto& engine = GameEngine::ins();
    const auto& state = engine.gameState();
    frame.nowMs = Hal::ins().millis();
    frame.phaseDebug = static_cast<uint8_t>(phase);
    frame.resultMessage = resultMessage;
    frame.toast = departureToastUntil ? departureToast : nullptr;
    frame.wildName = wild ? wild->name : nullptr;
    frame.adventureBondGain = adventureBondGain;
    if (phase == Phase::SELECT) {
        frame.areaCursor = ui.state.areaCursor;
        frame.areaAnimCursor = ui.state.areaAnimCursor;
        frame.visibleAreaCount = ExploreDestinations::visibleCount(state);
        for (uint8_t i = 0; i < Ui::ExploreRouteFrame::AREA_CAP; ++i) {
            const auto destination = ExploreDestinations::visibleId(i, state);
            frame.areaNames[i] = destination == ExploreDestinations::MARKET ? Ui::MarketView::NAME :
                destination < ROUTE_MAP_COUNT ? routeMap(destination).name : Ui::BACK;
            frame.areaUnlocked[i] = ExploreDestinations::unlocked(destination, state);
            frame.areaSpecialActive[i] = destination < ROUTE_MAP_COUNT && frame.areaUnlocked[i] &&
                specialKindForArea(destination) != ExploreSpecial::Kind::NONE;
        }
        frame.marketPreview = ExploreDestinations::visibleId(ui.state.areaCursor,state)==ExploreDestinations::MARKET;
        frame.areaPreviewStartedAt = areaPreviewStartedAt;
#if STICKMON_UI_ROUND_TOUCH
        if (areaPreviewPaused || !areaPreviewReady)
            frame.areaPreviewStartedAt = frame.nowMs - areaPreviewPausedElapsed;
#endif
        frame.areaPreviewCount = areaPreviewPool.count;
        frame.areaPreviewHasRare = ExplorePool::poolHasRare(areaPreviewPool);
        for (uint8_t i = 0; i < frame.areaPreviewCount; ++i) {
            frame.areaPreviewFrames[i] = areaPreviewFrames[i];
            frame.areaPreviewHidden[i] = ExplorePool::isRare(areaPreviewPool.entries[i].rarity) &&
                !engine.hasEncounteredSpecies(areaPreviewPool.entries[i].speciesId);
        }
        return frame;
    }
    if (phase == Phase::RESULT) return frame;
    frame.generatedMap = &generatedMap;
    frame.currentRoutePath = currentRoutePath;
    const auto& map = routeMap(mapBlocks[currentMapBlock]);
    frame.mapName = map.name;
    frame.fieldColor = map.fieldColor;
    frame.teamHudCount = std::min<uint8_t>(state.teamCount, 2);
    for (uint8_t i = 0; i < frame.teamHudCount; ++i) {
        const auto& member = state.team[i];
        frame.teamHud[i] = {member.hpCur, member.hpMax, member.majorStatus, bool(member.fainted)};
    }
    frame.menuAvailable = phase == Phase::WALKING && !iceSliding && !pendingFrostFall;
    frame.daylight = ExploreDaylight::at(state.gameMinutesTotal, generatedMap.areaIndex);
    const uint8_t leaderSlot = routeLeaderSlot(state);
    frame.leader = &engine.speciesFor(state.team[leaderSlot]);
    frame.follower = hasHealthyRouteFollower(state) ? &engine.speciesFor(state.team[1]) : nullptr;
    frame.boss = findSpecies(expeditionBossSpeciesId);
    frame.routeWorldX = routeWorldX;
    frame.routeWorldY = routeWorldY;
    frame.routeFollowerWorldX = routeFollowerWorldX;
    frame.routeFollowerWorldY = routeFollowerWorldY;
    frame.routeWalkDirection = routeWalkDirection;
    frame.routeFollowerWalkDirection = routeFollowerWalkDirection;
    frame.routeMoving = routeMoving && !routePausedForTouch;
    frame.routeFollowerMoving = routeFollowerMoving && !routePausedForTouch;
    frame.iceSliding = iceSliding;
    frame.exiting = phase == Phase::EXITING;
    frame.routeIndex = routeIndex;
    frame.routeFollowerIndex = routeFollowerTargetIndex(routeIndex);
    frame.routeMoveStarted = routeMoveStarted;
    frame.routeLeaderMoveDurationMs = routeLeaderMoveDurationMs;
    frame.routeFollowerMoveDurationMs = routeFollowerMoveDurationMs;
    frame.routeFollowerDelayMs = ROUTE_FOLLOWER_DELAY_MS;
    if (frame.exiting) {
        frame.leaderExitStepDurationMs = routeStepDurationForSpecies(frame.leader->id);
        if (frame.follower)
            frame.followerExitStepDurationMs = routeStepDurationForSpecies(frame.follower->id);
    }
    frame.routeBossPending = routeBossPending;
    frame.routeBossIndex = routeBossIndex;
    frame.routePickupAvailable = routePickupAvailable;
    frame.routePickupIndex = routePickupIndex;
    frame.exploreMenuOpen = exploreMenuOpen;
    frame.exploreMenuCursor = ui.state.exploreMenuCursor;
    frame.exploreMenuOpenedAt = exploreMenuOpenedAt;
    frame.specialChallengeYes = ui.state.specialChallengeYes;
    return frame;
}

Ui::ExploreBattleFrame ExploreScene::exploreBattleFrame() const {
    Ui::ExploreBattleFrame frame;
    frame.nowMs = Hal::ins().millis();
    frame.background = routeMap(mapBlocks[currentMapBlock]).battleBackground;
    frame.wildSpecies = wild;
    frame.playerSpecies = &battlePlayerSpecies();
    frame.wildHp = wildHp;
    frame.wildHpMax = wildHpMax;
    frame.wildLevel = wildRuntime.level;
    frame.wildStatus = wildRuntime.majorStatus;
    const auto& player = battlePlayerMonster();
    frame.playerHp = player.hpCur;
    frame.playerHpMax = player.hpMax;
    frame.playerLevel = player.level;
    frame.playerExp = player.exp;
    frame.playerStatus = player.majorStatus;
    frame.wildBattleState = wildBattleState;
    frame.playerBattleState = playerBattleState;
    frame.battleTurnAnimating = battleTurnStage == BattleTurnStage::ANIMATING;
    frame.battleTurnIdle = battleTurnStage == BattleTurnStage::IDLE;
    frame.battleActionSelfHit = battleActionSelfHit;
    frame.battleActionAttackerWild = battleActionAttackerWild;
    if (battlePendingAction.kind == BattleActionService::Kind::READY &&
        !battlePendingAction.damage.missed && !battlePendingAction.damage.failed &&
        battlePendingAction.damage.effectiveness != 0) {
        frame.battleMoveId = battlePendingAction.damage.moveId;
        const auto& effects = battlePendingAction.result.moveEffects;
        for (uint8_t i = 0; i < effects.count; ++i) {
            const auto& outcome = effects.outcomes[i];
            if (outcome.kind == BattleSystem::EffectOutcomeKind::STAT_CHANGED &&
                outcome.target == MoveEffectTarget::DEFENDER && outcome.stageDelta != 0)
                frame.battleDefenderStatChanged = true;
        }
    }
    frame.battleHpFrom = battleHpFrom;
    frame.battleHpTo = battleHpTo;
    frame.battleActionStarted = battleActionStarted;
    switch (battleSwitchStage) {
    case BattleSwitchStage::NONE:
        frame.battleSwitchStage = Ui::BattleSwitchVisualStage::NONE;
        break;
    case BattleSwitchStage::RETREATING:
        frame.battleSwitchStage = Ui::BattleSwitchVisualStage::RETREATING;
        break;
    case BattleSwitchStage::ENTERING:
        frame.battleSwitchStage = Ui::BattleSwitchVisualStage::ENTERING;
        break;
    }
    frame.battleSwitchStarted = battleSwitchStarted;
    frame.battleResultPending = battleResultPending;
    frame.battleExpVisible = battleExpVisible;
    frame.expAnimationPending = expAnimationPending;
    frame.expAnimationActive = expAnimationActive;
    frame.expAnimationFrom = expAnimationFrom;
    frame.expAnimationTo = expAnimationTo;
    frame.expAnimationStarted = expAnimationStarted;
    frame.foodThrowActive = foodThrowActive;
    frame.foodThrowIndex = foodThrowIndex;
    frame.foodThrowStarted = foodThrowStarted;
    frame.battleLogActive = battleLogActive;
    frame.battleLogVisibleCount = battleLogVisibleCount;
    static_assert(sizeof(frame.battleLogVisible) / sizeof(frame.battleLogVisible[0]) ==
                      BATTLE_LOG_VISIBLE_CAP,
                  "battle frame must expose every visible log");
    for (uint8_t i = 0; i < battleLogVisibleCount; ++i) {
        frame.battleLogVisible[i] = battleLogVisible[i];
    }
    frame.encounterPhase = phase == Phase::ENCOUNTER;
    frame.defeatAwaitInput = defeatAwaitInput;
    frame.battleCursor = ui.state.battleCursor;
    frame.friendshipPromptVisible =
        friendshipStep == FriendshipStep::CONTACT_CONFIRM ||
        friendshipStep == FriendshipStep::TEAM_CONFIRM;
    frame.friendshipConfirmYes = ui.state.friendshipConfirmYes;
#if STICKMON_ENABLE_DEBUG_FEATURES
    frame.drawBounds = GameEngine::ins().debugBattleDrawBoundsVisible();
#endif
    return frame;
}

bool ExploreScene::beginMarket() {
    if (!BuildConfig::DEBUG_FEATURES) return false;
    auto& engine = GameEngine::ins();
    const auto& state = engine.gameState();
    const uint8_t slot = Game::ExplorePartyService::leaderSlot(state);
    const uint16_t leader = state.teamCount ? state.team[slot].speciesId : 25;
    const uint16_t follower = state.teamCount > 1 && slot == 0 && Game::ExplorePartyService::healthy(state.team[1]) ? state.team[1].speciesId : 0;
    auto market = std::unique_ptr<Market::Session>(new (std::nothrow) Market::Session());
    if (!market || !market->reset(MarketMapData::get(), GameRandom::range(1,0x7fffffff), leader, follower, Hal::ins().millis())) return false;
    if (!Ui::MarketView::preload(MarketMapData::get(), leader, follower)) {
        Ui::MarketView::release(); loadAreaPreview(); return false;
    }
    market_ = std::move(market);
    market_->enableManual();marketUi_.reset();
#if STICKMON_UI_ROUND_TOUCH
    marketUi_.enableDialogue();
#endif
    AudioManager::ins().setMusic(MusicTrack::MARKET);
    phase = Phase::WALKING; cancelTouch(); prepareSurface(); return true;
}

void ExploreScene::closeMarket() {
    // Keep the last market frame/resources until the full-screen fade reaches
    // black. onExit releases them before the ordinary room return begins.
    cancelTouch(); marketUi_.reset();
    phase = Phase::ENDING;
    GameEngine::ins().beginExploreReturn(false);
}
