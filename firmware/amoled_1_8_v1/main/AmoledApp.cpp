#include "AmoledApp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "HomeScreen.h"
#include "assets/PokemonSprites.h"
#include "core/MathUtil.h"
#include "core/AudioManager.h"
#include "core/CryPlayer.h"
#include "core/FontResource.h"
#include "core/RoomMovementArea.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "game/ExploreItemProgression.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreEncounters.h"
#include "game/BathService.h"
#include "game/BattleSystem.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreIceSlide.h"
#include "game/ExperienceService.h"
#include "game/FriendshipService.h"
#include "game/GameRandom.h"
#include "game/HomeCare.h"
#include "game/HomeHud.h"
#include "game/ItemInventory.h"
#include "game/HomeChase.h"
#include "game/MonsterFactory.h"
#include "game/ShopService.h"
#include "game/Species.h"
#include "game/SpeciesBehavior.h"
#include "game/TeamRoster.h"
#if STICKMON_HAS_CLAW
#include "brain/StickmonClawRuntime.h"
#endif
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
constexpr uint32_t TURN_PAUSE_MS = 160;
constexpr uint32_t PET_WANDER_RETRY_MIN_MS = 700;
constexpr uint32_t PET_WANDER_RETRY_MAX_MS = 1400;
constexpr uint32_t PET_DEBUG_LOG_INTERVAL_MS = 10000;
constexpr uint32_t CARE_TICK_MS = 60000;
constexpr uint32_t PERIODIC_SAVE_MS = 5UL * 60UL * 1000UL;
constexpr uint16_t EXPLORE_ROUTE_STEP_MS = 360;
constexpr uint16_t EXPLORE_ROUTE_FRAME_MS = 90;
constexpr int EXPLORE_ROUTE_VIEW_HEIGHT = 224 - HOME_HEADER_HEIGHT;
constexpr int EXPLORE_ROUTE_WORLD_WIDTH =
    ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE;
constexpr int EXPLORE_ROUTE_WORLD_HEIGHT =
    ExploreMapGenerator::HEIGHT * ExploreRouteGeometry::TILE_SIZE;
constexpr uint32_t EXPLORE_PREVIEW_CYCLE_MS = 2800;
constexpr uint32_t EXPLORE_PREVIEW_MOVE_MS = 500;
constexpr uint32_t EXPLORE_PREVIEW_HOLD_MS =
    EXPLORE_PREVIEW_CYCLE_MS - EXPLORE_PREVIEW_MOVE_MS;
constexpr float EXPLORE_AREA_CURSOR_LERP = 0.5f;
constexpr uint32_t EXPLORE_PREVIEW_LOAD_DELAY_MS = 80;
constexpr uint32_t EXPLORE_PREVIEW_BACKGROUND_LOAD_MS = 80;
constexpr uint16_t BATTLE_ANIMATION_RENDER_END = 192;
constexpr float FALLBACK_ROOM_MIN_X = 56.0f;
constexpr float FALLBACK_ROOM_MAX_X = 122.0f;
constexpr float FALLBACK_ROOM_MIN_Y = 126.0f;
constexpr float FALLBACK_ROOM_MAX_Y = 151.0f;
constexpr float FALLBACK_FOOD_APPROACH_X = 121.0f;
constexpr float FALLBACK_FOOD_APPROACH_Y = 149.0f;
constexpr uint32_t MOOD_BURST_DURATION_MS = 420;
constexpr uint32_t MOOD_BURST_FRAME_MS = 50;
#if STICKMON_ENABLE_DEBUG_FEATURES
constexpr uint32_t DEBUG_PAIR_CHASE_DURATION_MS = 8000;
constexpr float DEBUG_PAIR_FOLLOW_DISTANCE = 22.0f;
constexpr float DEBUG_PAIR_FOLLOW_SPEED = 16.0f;
constexpr float DEBUG_TILT_DEADZONE = 0.08f;
constexpr float DEBUG_TILT_MAX = 0.62f;
constexpr float DEBUG_TILT_SPEED = 58.0f;
#endif

#if STICKMON_HAS_CLAW
// Render-time snapshot of the shared ClawStatusLog. AmoledApp lives on the
// app_main stack, so the 64-entry buffer stays here in .bss instead.
Stickmon::ClawStatusLog::Entry s_clawLogEntries[Stickmon::ClawStatusLog::CAPACITY];
#endif

uint8_t moodHeartCountFor(uint8_t mood) {
    return std::min<uint8_t>(5, static_cast<uint8_t>(mood / 20));
}

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

enum ExplorePickupId : uint8_t {
    EXPLORE_PICKUP_NONE = 0,
    EXPLORE_PICKUP_COIN,
    EXPLORE_PICKUP_POTION,
    EXPLORE_PICKUP_SUPER_POTION,
    EXPLORE_PICKUP_ANTIDOTE,
    EXPLORE_PICKUP_RARE_CANDY,
    EXPLORE_PICKUP_MAX_POTION,
    EXPLORE_PICKUP_FULL_RESTORE,
    EXPLORE_PICKUP_FULL_HEAL,
    EXPLORE_PICKUP_REVIVE,
    EXPLORE_PICKUP_MAX_REPEL,
    EXPLORE_PICKUP_HONEY,
    EXPLORE_PICKUP_NUGGET,
    EXPLORE_PICKUP_BIG_PEARL,
    EXPLORE_PICKUP_STAR_PIECE,
    EXPLORE_PICKUP_HEART_SCALE,
};

struct ExplorePickupEntry {
    uint8_t id;
    uint16_t weight;
};

static constexpr ExplorePickupEntry GRASS_PATH_PICKUPS[] = {
    {EXPLORE_PICKUP_COIN, 40}, {EXPLORE_PICKUP_POTION, 25},
    {EXPLORE_PICKUP_ANTIDOTE, 10}, {EXPLORE_PICKUP_HONEY, 10},
    {EXPLORE_PICKUP_NUGGET, 3}, {EXPLORE_PICKUP_RARE_CANDY, 2},
};
static constexpr ExplorePickupEntry CREEK_SLOPE_PICKUPS[] = {
    {EXPLORE_PICKUP_COIN, 40}, {EXPLORE_PICKUP_POTION, 20},
    {EXPLORE_PICKUP_ANTIDOTE, 10}, {EXPLORE_PICKUP_MAX_REPEL, 8},
    {EXPLORE_PICKUP_HONEY, 8}, {EXPLORE_PICKUP_NUGGET, 5},
    {EXPLORE_PICKUP_RARE_CANDY, 2},
};
static constexpr ExplorePickupEntry TALL_GRASS_PARK_PICKUPS[] = {
    {EXPLORE_PICKUP_COIN, 38}, {EXPLORE_PICKUP_SUPER_POTION, 20},
    {EXPLORE_PICKUP_ANTIDOTE, 8}, {EXPLORE_PICKUP_REVIVE, 5},
    {EXPLORE_PICKUP_MAX_REPEL, 8}, {EXPLORE_PICKUP_NUGGET, 6},
    {EXPLORE_PICKUP_BIG_PEARL, 3}, {EXPLORE_PICKUP_RARE_CANDY, 2},
};
static constexpr ExplorePickupEntry FROST_CRYSTAL_CAVE_PICKUPS[] = {
    {EXPLORE_PICKUP_COIN, 36}, {EXPLORE_PICKUP_SUPER_POTION, 18},
    {EXPLORE_PICKUP_FULL_HEAL, 8}, {EXPLORE_PICKUP_REVIVE, 6},
    {EXPLORE_PICKUP_MAX_REPEL, 6}, {EXPLORE_PICKUP_NUGGET, 4},
    {EXPLORE_PICKUP_BIG_PEARL, 6}, {EXPLORE_PICKUP_HEART_SCALE, 2},
    {EXPLORE_PICKUP_RARE_CANDY, 2},
};
static constexpr ExplorePickupEntry MIST_FOREST_PATH_PICKUPS[] = {
    {EXPLORE_PICKUP_COIN, 34}, {EXPLORE_PICKUP_MAX_POTION, 15},
    {EXPLORE_PICKUP_FULL_HEAL, 8}, {EXPLORE_PICKUP_REVIVE, 7},
    {EXPLORE_PICKUP_BIG_PEARL, 7}, {EXPLORE_PICKUP_STAR_PIECE, 4},
    {EXPLORE_PICKUP_HEART_SCALE, 2}, {EXPLORE_PICKUP_RARE_CANDY, 2},
};
static constexpr ExplorePickupEntry ANCIENT_WATERFALL_VALLEY_PICKUPS[] = {
    {EXPLORE_PICKUP_COIN, 32}, {EXPLORE_PICKUP_MAX_POTION, 15},
    {EXPLORE_PICKUP_FULL_RESTORE, 6}, {EXPLORE_PICKUP_REVIVE, 8},
    {EXPLORE_PICKUP_BIG_PEARL, 5}, {EXPLORE_PICKUP_STAR_PIECE, 7},
    {EXPLORE_PICKUP_HEART_SCALE, 2}, {EXPLORE_PICKUP_RARE_CANDY, 2},
};

struct ExplorePickupTable {
    const ExplorePickupEntry* entries;
    uint8_t count;
    uint8_t minCoin;
    uint8_t maxCoin;
};

#define EXPLORE_PICKUP_COUNT(value) \
    static_cast<uint8_t>(sizeof(value) / sizeof(value[0]))

static constexpr ExplorePickupTable EXPLORE_PICKUP_TABLES[] = {
    {GRASS_PATH_PICKUPS, EXPLORE_PICKUP_COUNT(GRASS_PATH_PICKUPS), 10, 30},
    {CREEK_SLOPE_PICKUPS, EXPLORE_PICKUP_COUNT(CREEK_SLOPE_PICKUPS), 15, 40},
    {TALL_GRASS_PARK_PICKUPS, EXPLORE_PICKUP_COUNT(TALL_GRASS_PARK_PICKUPS), 20, 60},
    {FROST_CRYSTAL_CAVE_PICKUPS, EXPLORE_PICKUP_COUNT(FROST_CRYSTAL_CAVE_PICKUPS), 30, 80},
    {MIST_FOREST_PATH_PICKUPS, EXPLORE_PICKUP_COUNT(MIST_FOREST_PATH_PICKUPS), 40, 110},
    {ANCIENT_WATERFALL_VALLEY_PICKUPS,
     EXPLORE_PICKUP_COUNT(ANCIENT_WATERFALL_VALLEY_PICKUPS), 50, 150},
};

#undef EXPLORE_PICKUP_COUNT

const ExplorePickupTable& explorePickupTableForArea(uint8_t area) {
    return EXPLORE_PICKUP_TABLES[
        std::min<uint8_t>(area, Game::EXPLORE_AREA_COUNT - 1)];
}

bool explorePickupAvailable(uint8_t pickupId, uint16_t stepsToday) {
    return pickupId != EXPLORE_PICKUP_RARE_CANDY || stepsToday >= 5000;
}

uint8_t rollExplorePickup(uint8_t area, uint16_t stepsToday) {
    const ExplorePickupTable& table = explorePickupTableForArea(area);
    uint16_t total = 0;
    for (uint8_t index = 0; index < table.count; ++index) {
        if (explorePickupAvailable(table.entries[index].id, stepsToday)) {
            total += table.entries[index].weight;
        }
    }
    if (total == 0) return EXPLORE_PICKUP_NONE;

    uint16_t roll = static_cast<uint16_t>(GameRandom::range(0, total));
    for (uint8_t index = 0; index < table.count; ++index) {
        if (!explorePickupAvailable(table.entries[index].id, stepsToday)) {
            continue;
        }
        if (roll < table.entries[index].weight) return table.entries[index].id;
        roll -= table.entries[index].weight;
    }
    return EXPLORE_PICKUP_NONE;
}

PokemonSprites::WalkDirection petDirectionForDelta(float dx, float dy) {
    if (std::fabs(dx) >= std::fabs(dy)) {
        return dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                          : PokemonSprites::WalkDirection::LEFT;
    }
    return dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                     : PokemonSprites::WalkDirection::UP;
}

MusicTrack musicForAmoledScene(AppSceneFlow::Scene scene, bool specialBattle) {
    switch (scene) {
    case AppSceneFlow::Scene::EXPLORE_AREAS:
    case AppSceneFlow::Scene::EXPLORE_ROUTE:
    case AppSceneFlow::Scene::EXPLORE_MENU:
        return MusicTrack::EXPLORE;
    case AppSceneFlow::Scene::BATTLE:
        return specialBattle ? MusicTrack::BATTLE_SPECIAL
                             : MusicTrack::BATTLE;
    default:
        return MusicTrack::HOME;
    }
}

struct AmoledEncounterTable {
    const ExploreEncounters::Entry* entries = nullptr;
    uint8_t count = 0;
};

template <size_t N>
AmoledEncounterTable encounterTable(const ExploreEncounters::Entry (&entries)[N]) {
    return {entries, static_cast<uint8_t>(N)};
}

AmoledEncounterTable encounterTableForArea(uint8_t area) {
    switch (area) {
    case 0: return encounterTable(ExploreEncounters::GRASS_PATH);
    case 1: return encounterTable(ExploreEncounters::CREEK_SLOPE);
    case 2: return encounterTable(ExploreEncounters::TALL_GRASS_PARK);
    case 3: return encounterTable(ExploreEncounters::FROST_CRYSTAL_CAVE);
    case 4: return encounterTable(ExploreEncounters::MIST_FOREST_PATH);
    case 5: return encounterTable(ExploreEncounters::ANCIENT_WATERFALL_VALLEY);
    default: return {};
    }
}

GameAssets::Kind battleBackgroundForArea(uint8_t area) {
    return ExploreAreaCatalog::battleBackground(area);
}

ExplorePool::Pool buildExplorePreviewPool(const Game::GameState& state,
                                          uint8_t area) {
    ExplorePool::Pool pool{};
    AmoledEncounterTable table = encounterTableForArea(area);
    if (!table.entries || table.count == 0) return pool;

    ExplorePool::SourceEntry source[ExplorePool::MAX_SOURCE_ENTRIES] = {};
    uint8_t sourceCount = std::min<uint8_t>(
        table.count, ExplorePool::MAX_SOURCE_ENTRIES);
    for (uint8_t index = 0; index < sourceCount; ++index) {
        const ExploreEncounters::Entry& entry = table.entries[index];
        source[index] = ExplorePool::SourceEntry{
            entry.speciesId, entry.weight, entry.rarity};
    }
    return ExplorePool::buildPool(
        source, sourceCount,
        ExplorePool::mixSeed(
            ExplorePool::slotIndexFor(state.gameMinutesTotal), area,
            area < Game::EXPLORE_AREA_COUNT
                ? state.explorePoolRerollCounts[area] : 0));
}

uint8_t collectExplorePreviewSpecies(const Game::GameState& state,
                                     uint16_t* speciesIds, uint8_t capacity,
                                     uint8_t priorityArea) {
    if (!speciesIds || capacity == 0) return 0;
    uint8_t unlockedArea = ExploreItemProgression::unlockedArea(state);
    uint8_t count = 0;
    if (priorityArea < Game::EXPLORE_AREA_COUNT &&
        priorityArea <= unlockedArea) {
        count = ExplorePool::appendUniqueSpecies(
            buildExplorePreviewPool(state, priorityArea), speciesIds,
            count, capacity);
    }
    for (uint8_t area = 0;
         area < Game::EXPLORE_AREA_COUNT && area <= unlockedArea; ++area) {
        if (area == priorityArea) continue;
        count = ExplorePool::appendUniqueSpecies(
            buildExplorePreviewPool(state, area), speciesIds, count,
            capacity);
    }
    return count;
}

}  // namespace

void AmoledApp::begin(uint32_t nowMs) {
    visitSession.attach(&gameState);
    storageReady = saveManager.begin();
    bool normalized = false;
    bool loaded = storageReady &&
                  saveManager.load(gameState, mainViewState, &normalized);
    if (!loaded) {
        gameState = Game::GameState{};
        gameState.oobeDone = true;
        mainViewState = MainSceneViewState{};
    }

    bool normalizedEncounterHistory = false;
    bool loadedEncounterHistory = storageReady &&
        saveManager.loadEncounterHistory(
            encounterHistory, &normalizedEncounterHistory);
    if (!loadedEncounterHistory) encounterHistory.clear();
    encounterHistoryDirty = normalizedEncounterHistory ||
                            syncOwnedSpeciesToEncounterHistory();

    if (storageReady && (!loaded || normalized || encounterHistoryDirty)) {
        saveState();
    }
    Platform::display().setBrightness(gameState.settings.brightness);
    Platform::audio().setVolume(gameState.settings.volume);
    AudioManager::ins().setMusic(MusicTrack::HOME);
    Platform::logf("[AmoledApp] save=%s species=%u level=%u food=%u\n",
                   loaded ? "loaded" : "created",
                   gameState.team[0].speciesId,
                   gameState.team[0].level,
                   gameState.room.food[gameState.room.selectedFood]);
    gameClock.start(nowMs, gameState.gameMinutesTotal);
    lastCareMs = nowMs;
    lastPersistMs = nowMs;
    lastInteractionMs = nowMs;
    lastPetUpdateMs = nowMs;
    nextMindUpdateMs = nowMs;
    petMotion = PetMotion::IDLE;
    petDirection = PokemonSprites::WalkDirection::DOWN;
    petStopMotion = PetMotion::IDLE;
    petStoppingToEat = false;
    petFrame = 0;
    nextPetFrameMs = nowMs + 520;
    GameRandom::seed(nowMs ^
                     (static_cast<uint32_t>(gameState.team[0].speciesId) << 16));
    monsterMind.reset(nowMs);
    if (const Species* species = findSpecies(gameState.team[0].speciesId)) {
        behaviorProfile = behaviorProfileFor(*species, gameState.team[0]);
    }
    RoomResource::ins().begin();
    Platform::logf("[AmoledApp] begin: room resource ready\n");
    FontResource::ins().begin();
    Platform::logf("[AmoledApp] begin: font resource ready\n");
    petResting = gameState.teamCount > 0 &&
                 (gameState.team[0].fainted || gameState.team[0].hpCur == 0);
    moodHeartCount = gameState.teamCount > 0
        ? moodHeartCountFor(gameState.team[0].mood) : 0;
    moodBurstHeart = 0xFF;
    moodBurstStartedMs = 0;
    moodBurstUntilMs = 0;
    nextMoodBurstFrameMs = 0;
    Platform::logf("[AmoledApp] begin: pet resting=%u\n",
                   petResting ? 1U : 0U);
    if (petResting) {
        RoomResource& room = RoomResource::ins();
        petX = petTargetX = room.available() ? static_cast<float>(room.bedX())
                                              : 76.0f;
        petY = petTargetY = room.available() ? static_cast<float>(room.bedY())
                                              : 99.0f;
        nextPetFrameMs = nowMs + 700;
    }
    Platform::logf("[AmoledApp] begin: resting position ready\n");
    updatePetFootprint();
    Platform::logf("[AmoledApp] begin: footprint ready rx=%.2f ry=%.2f\n",
                   static_cast<double>(petFootprintRadiusX),
                   static_cast<double>(petFootprintRadiusY));
    if (!petFootprintInsideWalkArea(petX, petY)) {
        Platform::logf("[AmoledApp] begin: pet outside walk area\n");
        float x = petX;
        float y = petY;
        if (chooseWanderTarget(x, y, false)) {
            petX = petTargetX = x;
            petY = petTargetY = y;
        }
    }
    Platform::logf("[AmoledApp] begin: walk target ready x=%.2f y=%.2f\n",
                   static_cast<double>(petX), static_cast<double>(petY));
    updateCamera();
    Platform::logf("[AmoledApp] begin: camera ready\n");
    schedulePetDecision(nowMs);
    Platform::logf("[AmoledApp] begin: decision scheduled\n");
    requestFullRender();
    Platform::logf("[AmoledApp] begin: full render requested\n");
}

#if STICKMON_HAS_CLAW
bool AmoledApp::brainSnapshot(Stickmon::BrainBridge::Snapshot& out) const {
    out = Stickmon::BrainBridge::Snapshot{};
    out.initialized = true;
    out.oobeDone = gameState.oobeDone;
    out.visitActive = visitSession.active();
    out.teamCount = gameState.teamCount;
    out.exploreArea = selectedExploreArea;
    out.battery = static_cast<int8_t>(Platform::power().batteryLevel());
    out.coins = gameState.coins;
    out.unlockedArea = ExploreItemProgression::unlockedArea(gameState);
    for (uint8_t index = 0; index < Game::ROOM_FOOD_COUNT; ++index) {
        out.foodCounts[index] = gameState.room.food[index];
    }
    for (uint8_t index = 0; index < static_cast<uint8_t>(Game::ItemId::COUNT); ++index) {
        out.inventoryCounts[index] = Game::ItemInventory::count(
            gameState, static_cast<Game::ItemId>(index));
    }
    out.bowlFood = gameState.room.bowlFood;
    out.bowlBitesRemaining = gameState.room.bowlBitesRemaining;
    // Autonomous actions are only allowed while the normal home state machine
    // is visible and no interaction/communication flow owns the app.
    // `autonomyActive()` is deliberately not part of action_locked. It
    // describes ownership of the current Agent turn, not a gameplay lock;
    // including it would make the first stickmon_get_context call report a
    // locked state and the autonomous prompt would never take an action.
    out.actionLocked = pointerDown ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        visitSession.busy() || pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE;
    if (pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
        out.explorePhase = 1;  // departing
    } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        out.explorePhase = 2;  // active
    }
    if (gameState.teamCount > 0) {
        const Game::MonsterRuntime& monster = gameState.team[0];
        out.speciesId = monster.speciesId;
        const Species* species = findSpecies(monster.speciesId);
        if (species && species->name) {
            std::snprintf(out.speciesName, sizeof(out.speciesName), "%s",
                          species->name);
        }
        out.nature = monster.nature;
        std::snprintf(out.natureName, sizeof(out.natureName), "%s",
                      natureName(monster.nature));
        out.gender = monster.gender;
        out.level = monster.level;
        out.hp = monster.hpCur;
        out.hpMax = monster.hpMax;
        out.satiety = monster.satiety;
        out.mood = monster.mood;
    }
    return true;
}

#endif  // STICKMON_HAS_CLAW

bool AmoledApp::queueExploreDeparture(uint8_t area, bool autoWalk) {
    if (area >= Game::EXPLORE_AREA_COUNT || gameState.teamCount == 0) {
        return false;
    }
    if (!ExploreItemProgression::isAreaUnlocked(area, gameState)) {
        return false;
    }
    pendingExpeditionArea = area;
    pendingExpeditionAutoWalk = autoWalk;
    pendingExpedition = true;
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    lastInteractionMs = Platform::clock().millis();
    // The departure animation always starts from the room, regardless of the
    // page that was visible when the request arrived.
    sceneFlow.goHome();
    requestFullRender();
    return true;
}

void AmoledApp::cancelExploreDeparture() {
    if (expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_DOOR) {
        petX = expeditionDoorInsideX;
        petY = expeditionDoorInsideY;
    }
    pendingExpedition = false;
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    expeditionDepartureStartedMs = 0;
    petMotion = PetMotion::IDLE;
    petTargetX = petX;
    petTargetY = petY;
    petFrame = 0;
    nextPetFrameMs = Platform::clock().millis() + 520;
    updateCamera();
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

bool AmoledApp::updateExploreDeparture(uint32_t nowMs) {
    if (!pendingExpedition &&
        expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
        return false;
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME) {
        sceneFlow.goHome();
        requestFullRender();
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
        RoomResource& room = RoomResource::ins();
        if (!room.available() || room.doorwayPolygonCount() < 3) {
            // A malformed/missing room pack must not strand the Agent. Keep
            // the route functional and log why the visual transition was
            // skipped.
            pendingExpedition = false;
            bool started = startExploreRoute(nowMs);
            exploreRouteAutoWalk = started && pendingExpeditionAutoWalk;
            autonomousExpedition = started && pendingExpeditionAutoWalk;
            return true;
        }
        expeditionDoorInsideX = static_cast<float>(room.doorwayInsideX());
        expeditionDoorInsideY = static_cast<float>(room.doorwayInsideY());
        expeditionDoorOutsideX = static_cast<float>(room.doorwayOutsideX());
        expeditionDoorOutsideY = static_cast<float>(room.doorwayOutsideY());
        petResting = false;
        petStoppingToEat = false;
        feedingUntilMs = 0;
        nextFeedBiteMs = 0;
        petTargetX = expeditionDoorInsideX;
        petTargetY = expeditionDoorInsideY;
        petDirection = petDirectionForDelta(petTargetX - petX,
                                            petTargetY - petY);
        petLongMove = true;
        petMotion = PetMotion::WANDERING;
        petFrame = 0;
        expeditionDepartureStartedMs = nowMs;
        expeditionDeparturePhase = ExpeditionDeparturePhase::WALK_TO_DOOR;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        Platform::logf("[AmoledExplore] departure started area=%u from=%.1f,%.1f door=%.1f,%.1f\n",
                       static_cast<unsigned>(pendingExpeditionArea), petX, petY,
                       expeditionDoorInsideX, expeditionDoorInsideY);
    }

    const float speed = expeditionDeparturePhase ==
        ExpeditionDeparturePhase::CROSS_DOOR ? 42.0f : 30.0f;
    uint32_t elapsedMs = nowMs - lastPetUpdateMs;
    if (lastPetUpdateMs == 0 || nowMs < lastPetUpdateMs) elapsedMs = 0;
    lastPetUpdateMs = nowMs;
    float step = speed * (std::min<uint32_t>(elapsedMs, 120) / 1000.0f);
    float dx = petTargetX - petX;
    float dy = petTargetY - petY;
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.8f || step >= distance) {
        petX = petTargetX;
        petY = petTargetY;
        if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_TO_DOOR) {
            petTargetX = expeditionDoorOutsideX;
            petTargetY = expeditionDoorOutsideY;
            petDirection = petDirectionForDelta(petTargetX - petX,
                                                petTargetY - petY);
            expeditionDeparturePhase = ExpeditionDeparturePhase::CROSS_DOOR;
            expeditionDepartureStartedMs = nowMs;
            petFrame = 0;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return true;
        }

        uint8_t area = pendingExpeditionArea;
        bool autoWalk = pendingExpeditionAutoWalk;
        pendingExpedition = false;
        expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
        petMotion = PetMotion::IDLE;
        petTargetX = petX;
        petTargetY = petY;
        bool started = false;
        if (area < Game::EXPLORE_AREA_COUNT) {
            if (area != selectedExploreArea) selectExploreArea(area, nowMs);
            started = startExploreRoute(nowMs);
        }
        exploreRouteAutoWalk = started && autoWalk;
        autonomousExpedition = started && autoWalk;
        Platform::logf("[AmoledExplore] departure complete area=%u route=%u auto=%u\n",
                       static_cast<unsigned>(area), started ? 1U : 0U,
                       autoWalk ? 1U : 0U);
        requestFullRender();
        return true;
    }
    if (distance > 0.0f && step > 0.0f) {
        petX += dx / distance * step;
        petY += dy / distance * step;
    }
    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    }
    updateCamera();
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

#if STICKMON_HAS_CLAW

bool AmoledApp::brainStartExpedition(uint8_t area) {
    if (gameState.teamCount == 0) {
        Stickmon::ClawRuntime::instance().logf(
            Stickmon::ClawStatusLog::Level::WARN,
            "探险拒绝：队伍为空 area=%u", static_cast<unsigned>(area));
        return false;
    }
    uint8_t unlockedArea = ExploreItemProgression::unlockedArea(gameState);
    if (!ExploreItemProgression::isAreaUnlocked(area, gameState)) {
        Stickmon::ClawRuntime::instance().logf(
            Stickmon::ClawStatusLog::Level::WARN,
            "探险拒绝：区域未解锁 requested=%u unlocked=%u",
            static_cast<unsigned>(area), static_cast<unsigned>(unlockedArea));
        return false;
    }
    AppSceneFlow::Scene scene = sceneFlow.current();
    if (scene == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        scene == AppSceneFlow::Scene::EXPLORE_MENU ||
        scene == AppSceneFlow::Scene::BATTLE ||
        scene == AppSceneFlow::Scene::COMMUNICATION) {
        Stickmon::ClawRuntime::instance().logf(
            Stickmon::ClawStatusLog::Level::WARN,
            "探险拒绝：当前场景不可用 scene=%u area=%u",
            static_cast<unsigned>(scene), static_cast<unsigned>(area));
        return false;
    }
    // All Agent-triggered departures use the same visible home-room entry
    // point. This also handles a request that arrives while a utility page is
    // open: the page is closed first, then the pet walks to the doorway.
    return queueExploreDeparture(area, true);
}

bool AmoledApp::brainReturnHome() {
    AppSceneFlow::Scene scene = sceneFlow.current();
    bool exploring = scene == AppSceneFlow::Scene::EXPLORE_ROUTE ||
                     scene == AppSceneFlow::Scene::EXPLORE_MENU ||
                     scene == AppSceneFlow::Scene::BATTLE;
    if (!exploring) return false;
    if (scene == AppSceneFlow::Scene::BATTLE) {
        closeBattle(Platform::clock().millis());
    }
    autonomousExpedition = false;
    leaveExploreRoute();
    return true;
}

bool AmoledApp::brainInviteFriend() {
    if (gameState.teamCount == 0 ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
        sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION ||
        visitSession.busy()) {
        return false;
    }
    visitSession.attach(&gameState);
    visitSession.startHost();
    if (visitSession.state() !=
        Communication::VisitSessionService::State::HOSTING) {
        return false;
    }
    sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
    requestFullRender();
    return true;
}

bool AmoledApp::brainEat() {
    if (gameState.teamCount == 0 ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
        sceneFlow.current() == AppSceneFlow::Scene::SHOWER ||
        sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION ||
        visitSession.busy()) {
        return false;
    }
    Game::MonsterRuntime& monster = gameState.team[0];
    if (monster.fainted || monster.hpCur == 0 || monster.satiety >= 100) {
        return false;
    }
    if (gameState.room.bowlCount == 0 &&
        Game::HomeCare::placeSelectedFoodInBowl(gameState) !=
            FoodPlacementResult::ADDED) {
        return false;
    }

    bool consumed = false;
    for (uint8_t bite = 0;
         bite < Game::ROOM_NORMAL_FOOD_BITES &&
         gameState.room.bowlCount > 0 && monster.satiety < 100;
         ++bite) {
        consumed = Game::HomeCare::consumeBowlFood(gameState, 0).consumed ||
                   consumed;
    }
    if (!consumed) return false;
    monsterMind.onAte(Platform::clock().millis());
    saveState();
    requestFullRender();
    return true;
}

bool AmoledApp::brainBuyFood(uint8_t foodIndex) {
    if (foodIndex >= Game::ROOM_FOOD_COUNT ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
        sceneFlow.current() == AppSceneFlow::Scene::SHOWER ||
        sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION ||
        visitSession.busy()) {
        return false;
    }
    Game::ItemId item = Game::itemIdForFoodIndex(foodIndex);
    if (item == Game::ItemId::COUNT ||
        Game::ShopService::buy(gameState, item) !=
            Game::ShopService::BuyResult::BOUGHT) {
        return false;
    }
    saveState();
    requestFullRender();
    return true;
}

bool AmoledApp::brainSay(const char* text) {
    if (!text || !text[0]) return false;
    std::snprintf(brainMessage, sizeof(brainMessage), "%s", text);
    setToast(brainMessage, Platform::clock().millis(), 5000);
    requestFullRender();
    return true;
}
#endif

void AmoledApp::handleTouch(const TouchEvent& event) {
    switch (event.type) {
    case TouchEventType::DOWN:
        if (pendingExpedition ||
            expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
            cancelExploreDeparture();
        }
#if STICKMON_HAS_CLAW
        // A physical touch immediately returns control to the player. This
        // also prevents an Agent-started route from resuming after a menu
        // visit or a touch on the route screen.
        autonomousExpedition = false;
        exploreRouteAutoWalk = false;
#endif
        pointerDown = true;
        lastInteractionMs = event.timestampMs;
        dragging = false;
        touchStartX = event.x;
        touchStartY = event.y;
        touchLastY = event.y;
        menuVelocity = 0.0f;
        if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU) {
            pressedMenuItem = mainMenuItemAt(event.x, event.y, menuScroll);
            requestRenderRows(MAIN_MENU_CONTENT_TOP, 224);
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
            debugVelocity = 0.0f;
            if (debugPopup == DebugViewModel::Popup::NONE) {
                debugPressedItem = debugItemAt(
                    event.x, event.y, debugCategory, debugScroll);
            }
            requestRenderRows(0, 224);
#endif
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedExploreArea = exploreAreaAt(
                event.x, event.y, selectedExploreArea,
                ExploreItemProgression::visibleAreaCount(gameState));
            exploreDragStartArea = selectedExploreArea;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
            pressedExploreMenuItem = exploreRouteMenuItemAt(event.x, event.y);
            requestRenderRows(0, 224);
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
        } else if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedRoomItem = roomFoodItemAt(event.x, event.y);
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() ==
                       AppSceneFlow::Scene::COMMUNICATION &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedCommunicationItem = communicationItemAt(
                event.x, event.y, visitSession.viewModel());
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
                   event.y >= MENU_HEADER_HEIGHT) {
            battlePressedItem = battleLogCount > 0
                ? 0xFF
                : static_cast<uint8_t>(std::max(
                      -1, battleItemAt(event.x, event.y, battlePhase)));
            requestRenderRows(176, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   event.y >= MENU_HEADER_HEIGHT) {
            if (computerPage == ComputerViewModel::Page::STORAGE) {
                computerVelocity = 0.0f;
            }
#if STICKMON_HAS_CLAW
            if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
                clawLogVelocity = 0.0f;
            }
#endif
            bool aiClawEnabled = false;
#if STICKMON_HAS_CLAW
            aiClawEnabled = Stickmon::ClawRuntime::instance().enabled();
#endif
            computerPressedItem = static_cast<uint8_t>(std::max(
                -1, computerItemAt(event.x, event.y, computerPage,
                                   computerScroll, gameState.storageCount,
                                   aiClawEnabled)));
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS &&
                   event.y >= MENU_HEADER_HEIGHT) {
            int item = settingsItemAt(event.x, event.y);
            settingsPressedItem = item < 0 ? 0xFF
                                           : static_cast<uint8_t>(item);
            settingsSliderDragging =
                (item == 0 || item == 1) &&
                event.x >= SETTINGS_SLIDER_LEFT - TAP_SLOP &&
                event.x <= SETTINGS_SLIDER_RIGHT + TAP_SLOP;
            settingsSliderChanged = false;
            if (settingsSliderDragging) {
                setSettingsSliderValue(static_cast<uint8_t>(item), event.x,
                                       event.timestampMs);
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
            progressionPressedItem = static_cast<uint8_t>(std::max(
                -1, progressionItemAt(event.x, event.y, progressionMode)));
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
                   event.y >= MENU_HEADER_HEIGHT) {
            itemVelocity = 0.0f;
            if (itemConfirmOpen) {
                pressedShopDetailAction =
                    shopDetailProgress >= 1.0f
                        ? itemConfirmChoiceAt(event.x, event.y) : -1;
            } else {
                ShopViewModel::Mode mode =
                    shopCategory == Game::ShopService::Category::SELL
                        ? ShopViewModel::Mode::SELL
                        : ShopViewModel::Mode::BUY;
                pressedShopCategory = shopMenuItemAt(event.x, event.y);
                pressedItemRow = shopGridItemAt(
                    event.x, event.y, itemScroll, mode,
                    shopDailyItemCount(), shopExploreItemCount(),
                    currentItemCount());
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BAG &&
                   !itemConfirmOpen && event.y >= MENU_HEADER_HEIGHT) {
            itemVelocity = 0.0f;
            pressedItemRow = itemListItemAt(
                event.x, event.y, itemScroll, currentItemCount());
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
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS &&
                   settingsSliderDragging) {
            setSettingsSliderValue(settingsPressedItem, event.x,
                                   event.timestampMs);
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                pressedMenuItem = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                menuScroll -= static_cast<float>(deltaY);
                menuVelocity = static_cast<float>(-deltaY);
                clampMenuScroll();
                requestRenderRows(MAIN_MENU_CONTENT_TOP, 224);
            }
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
                   debugPopup == DebugViewModel::Popup::NONE) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                debugPressedItem = -1;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                debugScroll -= static_cast<float>(deltaY);
                debugVelocity = static_cast<float>(-deltaY);
                clampDebugScroll();
                requestRenderRows(0, 224);
            }
#endif
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                pressedExploreArea = -1;
            }
            if (dragging) {
                int count = std::min<int>(
                    ExploreItemProgression::visibleAreaCount(gameState),
                    Game::EXPLORE_AREA_COUNT);
                int startArea = exploreDragStartArea >= 0
                    ? exploreDragStartArea : selectedExploreArea;
                int candidate = startArea - static_cast<int>(std::lround(
                    (event.y - touchStartY) /
                    static_cast<float>(EXPLORE_SELECTOR_AREA_SPACING)));
                candidate = std::clamp(candidate, 0, std::max(0, count - 1));
                if (candidate != selectedExploreArea) {
                    selectExploreArea(static_cast<uint8_t>(candidate),
                                      event.timestampMs);
                }
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedExploreMenuItem >= 0) {
            pressedExploreMenuItem = -1;
            requestRenderRows(0, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamConfirmOpen &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedTeamSlot >= 0) {
            pressedTeamSlot = -1;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STORAGE &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                computerPressedItem = 0xFF;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                computerScroll -= static_cast<float>(deltaY);
                computerVelocity = static_cast<float>(-deltaY);
                clampComputerScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        }
#if STICKMON_HAS_CLAW
        else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                 computerPage == ComputerViewModel::Page::CLAW_SETUP &&
                 clawLogView && touchStartY >= CLAW_LOG_TOP) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                clawLogScroll -= static_cast<float>(deltaY);
                clawLogVelocity = static_cast<float>(-deltaY);
                clawLogPinned = false;
                clampClawLogScroll();
                requestRenderRows(CLAW_LOG_TOP, 224);
            }
        }
#endif
        else if ((sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD ||
                    sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION ||
                    sceneFlow.current() == AppSceneFlow::Scene::COMPUTER ||
                    sceneFlow.current() == AppSceneFlow::Scene::SETTINGS ||
                    sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP) {
            if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD) {
                pressedRoomItem = -1;
            } else if (sceneFlow.current() ==
                       AppSceneFlow::Scene::COMMUNICATION) {
                pressedCommunicationItem = -1;
            } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
                computerPressedItem = 0xFF;
            } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS) {
                settingsPressedItem = 0xFF;
            } else {
                progressionPressedItem = 0xFF;
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP) {
            battlePressedItem = 0xFF;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                    sceneFlow.current() == AppSceneFlow::Scene::SHOP) &&
                   !itemConfirmOpen &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                           touchStartX >= SHOP_LEFT_PANEL_WIDTH;
                pressedItemRow = -1;
                pressedShopCategory = -1;
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
        bool settingsSliderWasDragging = settingsSliderDragging;
        if (settingsSliderWasDragging) {
            setSettingsSliderValue(settingsPressedItem, event.x,
                                   event.timestampMs);
            if (settingsSliderChanged) {
                saveState();
                if (settingsPressedItem == 0) {
                    setToast(Ui::Settings::BRIGHTNESS_CHANGED,
                             event.timestampMs);
                } else if (settingsPressedItem == 1) {
                    setToast(Ui::Settings::VOLUME_CHANGED,
                             event.timestampMs);
                }
            }
        } else if (!dragging && distance <= TAP_SLOP) {
            handleTap(event.x, event.y, event.timestampMs);
        }
        pointerDown = false;
        dragging = false;
        pressedMenuItem = -1;
#if STICKMON_ENABLE_DEBUG_FEATURES
        debugPressedItem = -1;
#endif
        pressedExploreArea = -1;
        exploreDragStartArea = -1;
        pressedExploreMenuItem = -1;
        pressedItemRow = -1;
        pressedShopCategory = -1;
        pressedShopDetailAction = -1;
        pressedTeamSlot = -1;
        pressedRoomItem = -1;
        pressedCommunicationItem = -1;
        pressedShowerItem = -1;
        computerPressedItem = 0xFF;
        settingsPressedItem = 0xFF;
        settingsSliderDragging = false;
        settingsSliderChanged = false;
        progressionPressedItem = 0xFF;
        battlePressedItem = 0xFF;
        showerToolDragging = false;
        requestRenderRows(
                          sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                              ? MAIN_MENU_CONTENT_TOP
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS
                              ? MENU_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE
                              ? HOME_HEADER_HEIGHT
                          : sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU
                              ? 0
                          : sceneFlow.current() == AppSceneFlow::Scene::BAG ||
                            sceneFlow.current() == AppSceneFlow::Scene::SHOP ||
                            sceneFlow.current() == AppSceneFlow::Scene::TEAM ||
                            sceneFlow.current() == AppSceneFlow::Scene::ROOM ||
                            sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD ||
                            sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION ||
                            sceneFlow.current() == AppSceneFlow::Scene::COMPUTER ||
                            sceneFlow.current() == AppSceneFlow::Scene::SETTINGS ||
                            sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION ||
                            sceneFlow.current() == AppSceneFlow::Scene::BATTLE ||
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
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugContactPending) {
            int choice = debugContactChoiceAt(x, y);
            if (choice == 0) {
                acceptDebugContact(nowMs);
            } else if (choice == 1) {
                debugContactPending = false;
                debugContactStorageSlot = 0xFF;
                debugContactKind = 0;
                setToast(Ui::ContactVisit::BYE_VISIT, nowMs);
                requestFullRender();
            }
            if (choice >= 0) return;
        }
#endif
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
                setToast(Ui::Menu::PET_TOAST, nowMs);
                saveState();
                break;
            case PetOutcome::DAILY_LIMIT:
                setToast(Ui::Menu::PET_LIMIT, nowMs);
                saveState();
                break;
            case PetOutcome::NEEDS_REST:
                setToast(Ui::Menu::PET_REST, nowMs);
                break;
            }
            break;
        case HomeHitTarget::BOWL: {
            FoodPlacementResult result =
                Game::HomeCare::placeSelectedFoodInBowl(gameState);
            switch (result) {
            case FoodPlacementResult::ADDED:
                setToast(Ui::Menu::FOOD_ADDED, nowMs);
                nextMindUpdateMs = nowMs;
                nextPetDecisionMs = nowMs;
                saveState();
                break;
            case FoodPlacementResult::NO_STOCK:
                setToast(Ui::Menu::NO_FOOD, nowMs);
                break;
            case FoodPlacementResult::BOWL_FULL:
                setToast(Ui::Menu::FOOD_FULL, nowMs);
                break;
            case FoodPlacementResult::DIFFERENT_FOOD:
                setToast(Ui::Menu::FOOD_MIXED, nowMs);
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

    if (sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        CommunicationViewModel model = visitSession.viewModel();
        if (communicationBackAt(x, y)) {
            visitSession.stop();
            sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
            requestFullRender();
            return;
        }
        int item = communicationItemAt(x, y, model);
        using CommState = Communication::VisitSessionService::State;
        if (model.state == CommState::IDLE) {
            if (item == 0) visitSession.startHost();
            else if (item == 1) visitSession.startSearch();
        } else if ((model.state == CommState::SEARCHING ||
                    model.state == CommState::JOINING) && item >= 0) {
            visitSession.selectRoom(static_cast<uint8_t>(item));
        } else if (model.state == CommState::WAITING_HOST_DECISION) {
            if (item == 0) visitSession.acceptIncoming(true);
            else if (item == 1) visitSession.acceptIncoming(false);
        } else if (model.state == CommState::ACTIVE && item == 0) {
            visitSession.endVisit();
        } else if ((model.state == CommState::FAILED ||
                    model.state == CommState::ENDED) && item == 0) {
            visitSession.stop();
        }
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        if (exploreRouteIceSliding) return;
        if (exploreRoutePrompt != ExploreRouteViewModel::Prompt::NONE) {
            int choice = exploreRoutePromptChoiceAt(x, y);
            if (choice == 0) {
                exploreRoutePrompt = ExploreRouteViewModel::Prompt::NONE;
                // Match Stick's resume flow: continue walking until the next
                // route interaction stops the player-controlled run.
                exploreRouteAutoWalk = true;
                setToast(Ui::Amoled::OPEN, nowMs);
                beginExploreRouteStep(nowMs);
                requestRenderRows(HOME_HEADER_HEIGHT, 224);
            } else if (choice == 1) {
                exploreRoutePrompt = ExploreRouteViewModel::Prompt::NONE;
                leaveExploreRoute();
            }
            return;
        }
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
            exploreMenuCursor = 0;
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (exploreRouteMapAt(x, y)) {
            if (exploreRouteComplete) {
                leaveExploreRoute();
            } else {
                // Match Stick's beginAutoWalk(): one player tap walks through
                // route points until the next interaction. It is a command,
                // not a toggle; menu/back remain the explicit pause controls.
                exploreRouteAutoWalk = true;
                if (!exploreRouteMoving) {
                    beginExploreRouteStep(nowMs);
                }
                requestRenderRows(200, 224);
            }
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        if (battleAnimationActive || battleLogCount > 0) return;
        if (battleBackAt(x, y)) {
            if (battlePhase == BattleViewModel::Phase::BAG_SELECT ||
                battlePhase == BattleViewModel::Phase::SWITCH_SELECT) {
                battlePhase = BattleViewModel::Phase::ACTION;
                battlePressedItem = 0xFF;
                requestFullRender();
            }
            return;
        }
        int item = battleItemAt(x, y, battlePhase);
        if (item < 0) return;
        if (battlePhase == BattleViewModel::Phase::FRIENDSHIP) {
            resolveBattleFriendship(static_cast<uint8_t>(item), nowMs);
        } else if (battlePhase == BattleViewModel::Phase::VICTORY) {
            finishBattleVictory(nowMs);
        } else if (battlePhase == BattleViewModel::Phase::DEFEAT) {
            finishBattleDefeat(nowMs);
        } else if (battlePhase == BattleViewModel::Phase::BAG_SELECT) {
            if (item < 0 || item >= 4 ||
                item >= battleBagCount) return;
            performBattleBagItem(battleBagItems[item], nowMs);
        } else if (battlePhase == BattleViewModel::Phase::SWITCH_SELECT) {
            battlePressedItem = static_cast<uint8_t>(item);
            performBattleSwitch(static_cast<uint8_t>(item), true, nowMs);
        } else if (item == 0) {
            battlePressedItem = 0xFF;
            performBattleAttack(nowMs);
        } else if (item == 1) {
            performBattleBag(nowMs);
        } else if (item == 2) {
            battlePhase = BattleViewModel::Phase::SWITCH_SELECT;
            battlePressedItem = 0xFF;
            requestFullRender();
        } else if (item == 3) {
            performBattleFlee(nowMs);
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
        exploreMenuCursor = static_cast<uint8_t>(itemIndex);
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
        if (teamMovesOpen) {
            if (teamMovesBackAt(x, y)) {
                teamMovesOpen = false;
                teamMovesForgetConfirm = false;
                requestFullRender();
                return;
            }
            if (teamMovesForgetConfirm) {
                int choice = teamConfirmChoiceAt(x, y);
                if (choice == 0) {
                    if (Game::MoveManagementService::forgetSpecialMove(
                            gameState, teamMovesSlot, teamMovesForgetSlot)) {
                        saveState();
                        refreshTeamMoveRecallable();
                        setToast(Ui::Team::MOVE_FORGOT, nowMs);
                    } else {
                        setToast(Ui::Amoled::CANNOT_FORGET, nowMs);
                    }
                    teamMovesForgetConfirm = false;
                    requestRenderRows(MENU_HEADER_HEIGHT, 224);
                } else if (choice == 1) {
                    teamMovesForgetConfirm = false;
                    requestRenderRows(MENU_HEADER_HEIGHT, 224);
                }
                return;
            }
            int item = teamMovesItemAt(
                x, y, teamMovesMode, teamMovesRecallCount);
            if (item < 0) return;
            if (teamMovesMode == TeamMovesViewModel::Mode::MANAGE) {
                if (item < Game::MOVE_SLOT_COUNT) {
                    teamMovesForgetSlot = static_cast<uint8_t>(item);
                    if (item == 0) {
                        setToast(Ui::Team::MOVE_BASIC_LOCKED, nowMs);
                    } else if (const Species* teamSpecies = findSpecies(
                                   gameState.team[teamMovesSlot].speciesId);
                               teamSpecies &&
                               Game::MoveManagementService::learnedMove(
                                   *teamSpecies, gameState.team[teamMovesSlot],
                                   static_cast<uint8_t>(item))) {
                        teamMovesForgetConfirm = true;
                        requestRenderRows(MENU_HEADER_HEIGHT, 224);
                    } else {
                        setToast(Ui::EMPTY, nowMs);
                    }
                } else if (item == 3) {
                    refreshTeamMoveRecallable();
                    if (Game::ItemInventory::count(
                            gameState, Game::ItemId::HEART_SCALE) == 0) {
                        setToast(Ui::Amoled::NEED_HEART_SCALE, nowMs);
                    } else if (teamMovesRecallCount == 0) {
                        setToast(Ui::Team::MOVE_RECALL_EMPTY, nowMs);
                    } else {
                        teamMovesMode = TeamMovesViewModel::Mode::RECALL_SELECT;
                        teamMovesRecallSelected = 0xFF;
                        requestRenderRows(MENU_HEADER_HEIGHT, 224);
                    }
                } else {
                    teamMovesOpen = false;
                    requestFullRender();
                }
            } else if (teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT) {
                if (item >= teamMovesRecallCount) {
                    teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
                } else {
                    teamMovesRecallSelected = static_cast<uint8_t>(item);
                    teamMovesMode = TeamMovesViewModel::Mode::RECALL_REPLACE;
                    requestRenderRows(MENU_HEADER_HEIGHT, 224);
                }
            } else if (item < 2 && teamMovesRecallSelected < teamMovesRecallCount) {
                if (Game::MoveManagementService::recallMove(
                        gameState, teamMovesSlot,
                        teamMovesRecallIds[teamMovesRecallSelected],
                        static_cast<uint8_t>(item + 1))) {
                    saveState();
                    refreshTeamMoveRecallable();
                    teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
                    setToast(Ui::Amoled::MOVE_RECALLED, nowMs);
                } else {
                    setToast(Ui::Team::MOVE_RECALL_FAILED, nowMs);
                }
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            } else {
                teamMovesMode = TeamMovesViewModel::Mode::RECALL_SELECT;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
            return;
        }
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
        if (teamMovesButtonAt(x, y, static_cast<uint8_t>(slot))) {
            openTeamMoves(static_cast<uint8_t>(slot), nowMs);
            return;
        }
        if (slot == 0) {
            setToast(Ui::Amoled::CURRENT_LEADER, nowMs);
        } else if (!Game::TeamRoster::canMoveToFront(
                       gameState, static_cast<uint8_t>(slot))) {
            setToast(Ui::Amoled::VISITOR_LOCKED, nowMs);
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
            openRoomFoodScene();
        } else if (item == 1) {
            openShowerScene(nowMs);
        } else if (item == 2) {
            closeItemScene();
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD) {
        if (roomFoodBackAt(x, y)) {
            closeUtilityScene();
            return;
        }
        int item = roomFoodItemAt(x, y);
        if (item < 0 || item >= Game::ROOM_FOOD_COUNT) return;
        gameState.room.selectedFood = static_cast<uint8_t>(item);
        saveState();
        setToast(Ui::Room::FOOD_SELECTED, nowMs);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
        if (progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE) {
            int choice = progressionItemAt(x, y, progressionMode);
            if (choice == 1 || choice == 2) {
                Game::MonsterRuntime& monster =
                    gameState.team[progressionTeamSlot];
                Game::MoveId& slot = choice == 1
                    ? monster.move2Id : monster.move3Id;
                slot = progressionMoveId;
                progressionPressedItem = 0xFF;
                saveState();
                advanceProgression(nowMs);
            } else if (choice == 0) {
                completeProgression(nowMs);
            }
            return;
        }
        if (progressionItemAt(x, y, progressionMode) == 0) {
            advanceProgression(nowMs);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
        if (computerBackAt(x, y)) {
            if (computerPage != ComputerViewModel::Page::MENU) {
#if STICKMON_HAS_CLAW
                if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
                    Stickmon::ClawRuntime::instance().stopSetupPortal();
                }
#endif
                computerPage = computerPage == ComputerViewModel::Page::CLAW_SETUP
                    ? ComputerViewModel::Page::AI_HOSTING
                    : ComputerViewModel::Page::MENU;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                computerPressedItem = 0xFF;
                requestFullRender();
            } else {
                closeUtilityScene();
            }
            return;
        }
#if STICKMON_HAS_CLAW
        if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
            const int tab = clawTabAt(x, y);
            if (tab >= 0) {
                const bool wantLog = tab == 1;
                if (wantLog != clawLogView) {
                    clawLogView = wantLog;
                    clawLogScroll = 0.0f;
                    clawLogVelocity = 0.0f;
                    clawLogPinned = true;
                    requestFullRender();
                }
                return;
            }
        }
#endif
        bool aiClawEnabled = false;
#if STICKMON_HAS_CLAW
        aiClawEnabled = Stickmon::ClawRuntime::instance().enabled();
#endif
        int item = computerItemAt(x, y, computerPage, computerScroll,
                                  gameState.storageCount, aiClawEnabled);
        if (item < 0) return;
        if (computerPage == ComputerViewModel::Page::MENU) {
            if (item == 0) {
                computerPage = ComputerViewModel::Page::STATUS;
                computerPressedItem = 0xFF;
                toast = nullptr;
                requestFullRender();
            } else if (item == 1) {
                computerPage = ComputerViewModel::Page::STORAGE;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                computerPressedItem = 0xFF;
                toast = nullptr;
                requestFullRender();
            }
#if STICKMON_HAS_CLAW
            else if (item == 2) {
                computerPage = ComputerViewModel::Page::AI_HOSTING;
                computerPressedItem = 0xFF;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                toast = nullptr;
                requestFullRender();
            }
#else
            else if (item == 2) {
                closeUtilityScene();
            }
#endif
            else {
                closeUtilityScene();
            }
        } else if (computerPage == ComputerViewModel::Page::AI_HOSTING) {
#if STICKMON_HAS_CLAW
            Stickmon::ClawRuntime& claw = Stickmon::ClawRuntime::instance();
            if (item == 0) {
                claw.setWifiEnabled(!claw.wifiEnabled());
                computerPressedItem = 0xFF;
                requestFullRender();
            } else if (item == 1) {
                claw.setEnabled(!claw.enabled());
                computerPressedItem = 0xFF;
                requestFullRender();
            } else if (item == 2) {
                if (!claw.wifiEnabled()) {
                    setToast(Ui::Amoled::CLAW_WIFI_REQUIRED, nowMs);
                    return;
                }
                char ssid[33] = {};
                char password[65] = {};
                char ip[16] = {};
                if (claw.startSetupPortal() &&
                    claw.setupPortalInfo(ssid, sizeof(ssid), password,
                                         sizeof(password), ip, sizeof(ip))) {
                    computerPage = ComputerViewModel::Page::CLAW_SETUP;
                    computerPressedItem = 0xFF;
                    clawLogView = false;
                    clawLogScroll = 0.0f;
                    clawLogVelocity = 0.0f;
                    clawLogPinned = true;
                    clawLogGen = claw.logGeneration();
                    refreshClawLogSnapshot();
                    toast = nullptr;
                    requestFullRender();
                } else {
                    setToast(Ui::Amoled::CLAW_PORTAL_FAILED, nowMs);
                }
            } else {
                computerPage = ComputerViewModel::Page::MENU;
                computerPressedItem = 0xFF;
                requestFullRender();
            }
#else
            (void)item;
#endif
        } else if (computerPage == ComputerViewModel::Page::STATUS) {
            setToast(Ui::Amoled::READY, nowMs);
        } else {
            setToast(Ui::Amoled::STORAGE_READ_ONLY, nowMs);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS) {
        if (settingsBackAt(x, y)) {
            closeUtilityScene();
            return;
        }
        int item = settingsItemAt(x, y);
        if (item < 0) return;
        switch (item) {
        case 0:
        case 1:
            setSettingsSliderValue(static_cast<uint8_t>(item), x, nowMs);
            setToast(item == 0 ? Ui::Settings::BRIGHTNESS_CHANGED
                               : Ui::Settings::VOLUME_CHANGED,
                     nowMs);
            break;
        case 2:
            gameState.settings.speedIndex = static_cast<uint8_t>(
                (gameState.settings.speedIndex + 1) % 4);
            setToast(Ui::Common::SPEED_CHANGED, nowMs);
            break;
        case 3:
            gameState.settings.idleTimeoutIndex = static_cast<uint8_t>(
                (gameState.settings.idleTimeoutIndex + 1) % 5);
            setToast(Ui::Settings::POWER_SAVE, nowMs);
            break;
        case 4:
            gameState.settings.voiceCallEnabled =
                !gameState.settings.voiceCallEnabled;
            setToast(Ui::Settings::VOICE_CALL, nowMs);
            break;
        case 5:
            closeUtilityScene();
            return;
        default:
            return;
        }
        saveState();
        requestRenderRows(MENU_HEADER_HEIGHT, 224);
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
                setToast(Ui::Shower::ALREADY_SOAPED, nowMs);
            } else if (Game::BathService::nextOwnedSoap(gameState, -1) < 0) {
                setToast(Ui::Shower::NO_SOAP, nowMs);
            } else {
                showerMode = ShowerMode::SOAP_SELECT;
                toast = nullptr;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else if (item == 1) {
            if (!showerSoapRewarded) {
                setToast(Ui::Amoled::SOAP_FIRST, nowMs);
            } else {
                startShowerTool(ShowerMode::BRUSHING, nowMs);
            }
        } else if (item == 2) {
            if (!showerSoapRewarded) {
                setToast(Ui::Amoled::SOAP_FIRST, nowMs);
            } else if (!showerBrushRewarded) {
                setToast(Ui::Amoled::BRUSH_FIRST, nowMs);
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
        if (item == Game::ItemId::HEART_SCALE) {
            openTeamScene();
            openTeamMoves(0, nowMs);
            return;
        }
        if (!Game::ItemInventory::usableFromHomeBag(item)) {
            setToast(Ui::Amoled::SELL_IN_SHOP, nowMs);
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
        if (itemConfirmOpen) {
            if (shopDetailProgress < 1.0f) return;
            int choice = itemConfirmChoiceAt(x, y);
            if (choice == 0) {
                performPendingItemAction(nowMs);
            } else if (choice == 1) {
                itemConfirmOpen = false;
                pendingItem = Game::ItemId::COUNT;
                pendingItemAction = PendingItemAction::NONE;
                shopDetailProgress = 0.0f;
                shopDetailItemIndex = -1;
                pressedShopDetailAction = -1;
                toast = nullptr;
                requestFullRender();
            }
            return;
        }
        if (itemListBackAt(x, y)) {
            closeItemScene();
            return;
        }
        int menuItem = shopMenuItemAt(x, y);
        if (menuItem >= 0) {
            if (menuItem == 2) {
                closeItemScene();
                return;
            }
            shopCategory = menuItem == 0
                ? Game::ShopService::Category::DAILY
                : Game::ShopService::Category::SELL;
            itemScroll = 0.0f;
            itemVelocity = 0.0f;
            toast = nullptr;
            requestFullRender();
            return;
        }
        ShopViewModel::Mode mode =
            shopCategory == Game::ShopService::Category::SELL
                ? ShopViewModel::Mode::SELL : ShopViewModel::Mode::BUY;
        int index = shopGridItemAt(
            x, y, itemScroll, mode, shopDailyItemCount(),
            shopExploreItemCount(), currentItemCount());
        if (index < 0) return;
        pendingItem = currentItemAt(static_cast<uint8_t>(index));
        pendingItemAction = shopCategory == Game::ShopService::Category::SELL
            ? PendingItemAction::SELL : PendingItemAction::BUY;
        itemConfirmOpen = pendingItem != Game::ItemId::COUNT;
        shopDetailItemIndex = index;
        shopDetailProgress = 1.0f;
        toast = nullptr;
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        if (exploreBackAt(x, y)) {
            clearExplorePreview();
            sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
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
            x, y, selectedExploreArea,
            ExploreItemProgression::visibleAreaCount(gameState));
        if (area < 0 && x >= EXPLORE_SELECTOR_LEFT_WIDTH &&
            y >= MENU_HEADER_HEIGHT && y < 224) {
            area = selectedExploreArea;
        }
        if (area >= 0) {
            if (ExploreItemProgression::isAreaUnlocked(area, gameState)) {
                if (selectedExploreArea == static_cast<uint8_t>(area)) {
                    queueExploreDeparture(static_cast<uint8_t>(area), false);
                } else {
                    selectExploreArea(static_cast<uint8_t>(area), nowMs);
                    toast = nullptr;
                }
            } else {
                setToast(Ui::Explore::AREA_LOCKED, nowMs);
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        }
        return;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
        handleDebugTap(x, y, nowMs);
        return;
    }
#endif

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
            static_cast<uint8_t>(itemIndex),
            STICKMON_ENABLE_DEBUG_FEATURES != 0);
        if (entry.target == AppSceneFlow::Scene::EXPLORE_AREAS) {
            if (sceneFlow.menuReturn() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
                sceneFlow.enterExploreRoute();
                resumeExploreRoute(nowMs);
            } else {
                sceneFlow.enter(AppSceneFlow::Scene::EXPLORE_AREAS);
                selectedExploreArea = std::min<uint8_t>(
                    selectedExploreArea,
                    ExploreItemProgression::unlockedArea(gameState));
                exploreAreaAnimCursor = selectedExploreArea;
                loadExplorePreview(nowMs);
            }
            toast = nullptr;
            requestFullRender();
        } else if (entry.target == AppSceneFlow::Scene::TEAM) {
            openTeamScene();
        } else if (entry.target == AppSceneFlow::Scene::ROOM) {
            openRoomScene();
        } else if (entry.target == AppSceneFlow::Scene::COMPUTER) {
            openComputerScene();
        } else if (entry.target == AppSceneFlow::Scene::SETTINGS) {
            openSettingsScene();
        } else if (entry.target == AppSceneFlow::Scene::BAG ||
                   entry.target == AppSceneFlow::Scene::SHOP) {
            openItemScene(entry.target);
        } else if (entry.target == AppSceneFlow::Scene::COMMUNICATION) {
            sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
            visitSession.attach(&gameState);
            requestFullRender();
        } else if (entry.target == AppSceneFlow::Scene::HOME) {
            exploreRouteMoving = false;
            exploreRouteAutoWalk = false;
            exploreRoutePaused = false;
            exploreRouteExitConfirm = false;
            sceneFlow.goHome();
            toast = nullptr;
            requestFullRender();
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (entry.target == AppSceneFlow::Scene::DEBUG) {
            debugCategory = DebugViewModel::Category::ROOT;
            debugCursor = 0;
            debugScroll = 0.0f;
            debugVelocity = 0.0f;
            debugPopup = DebugViewModel::Popup::NONE;
            debugPressedItem = -1;
            toast = nullptr;
            sceneFlow.enter(AppSceneFlow::Scene::DEBUG);
            requestFullRender();
#endif
        } else {
            setToast(Ui::Amoled::MIGRATION_NEXT, nowMs);
        }
    }
}

void AmoledApp::update(uint32_t nowMs) {
#if STICKMON_HAS_CLAW
    if (Stickmon::ClawRuntime::instance().playerActive(nowMs)) {
        // WeChat messages are player input too. Stop any route/battle
        // automation before processing another frame of the app state.
        autonomousExpedition = false;
        exploreRouteAutoWalk = false;
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
        computerPage == ComputerViewModel::Page::CLAW_SETUP) {
        Stickmon::ClawRuntime& claw = Stickmon::ClawRuntime::instance();
        if (!clawLogView && claw.setupPhoneJoined()) {
            // A phone just joined the setup hotspot: switch to the log view
            // so Wi-Fi / WeChat login progress is visible without tapping.
            clawLogView = true;
            clawLogPinned = true;
            clawLogVelocity = 0.0f;
            requestFullRender();
        }
        const uint32_t generation = claw.logGeneration();
        if (generation != clawLogGen) {
            clawLogGen = generation;
            refreshClawLogSnapshot();
            if (clawLogView) {
                // Pinned follows the tail; new lines arrive at the bottom.
                requestRenderRows(34, 224);
            }
        }
    }
#endif
    AudioManager::ins().setMusic(
        musicForAmoledScene(sceneFlow.current(), battleIsBoss));
    AudioManager::ins().update();
    CryPlayer::ins().update();
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugContactActive && debugContactKind != 3 &&
        nowMs - debugContactStartedMs >= 30000UL) {
        completeDebugContact(nowMs);
    }
    if (debugBattleRequested) {
        debugBattleRequested = false;
        debugBattleActive = true;
        if (!beginExploreEncounter(nowMs)) debugBattleActive = false;
    }
#endif
    if (battleAudioPending && battleAudioReady) {
        uint8_t pendingSfx = battlePendingSfx;
        uint16_t pendingCrySpecies = battlePendingCrySpecies;
        battleAudioPending = false;
        battleAudioReady = false;
        battlePendingSfx = 0xFF;
        battlePendingCrySpecies = 0;
        uint32_t audioStartedMs = Platform::clock().millis();
        if (pendingSfx != 0xFF) {
            AudioManager::ins().playSfx(static_cast<SfxCue>(pendingSfx));
        }
        if (pendingCrySpecies != 0) {
            CryPlayer::ins().replay(pendingCrySpecies);
        }
        Platform::logf("[BattleAudio] queued species=%u elapsed=%lu\n",
                       pendingCrySpecies,
                       static_cast<unsigned long>(
                           Platform::clock().millis() - audioStartedMs));
    }
    CommunicationViewModel communicationBefore = visitSession.viewModel();
    visitSession.update(nowMs);
    CommunicationViewModel communicationAfter = visitSession.viewModel();
    if (communicationBefore.state != communicationAfter.state ||
        communicationBefore.roomCount != communicationAfter.roomCount ||
        communicationBefore.remote.speciesId != communicationAfter.remote.speciesId ||
        communicationBefore.remote.mood != communicationAfter.remote.mood ||
        communicationBefore.remote.satiety != communicationAfter.remote.satiety ||
        communicationBefore.remainSec != communicationAfter.remainSec) {
        requestFullRender();
    }
    updateClockAndCare(nowMs);
    updateMoodHearts(nowMs);
    if (!updateExploreDeparture(nowMs)) {
        updatePet(nowMs);
    }
    updateExploreRoute(nowMs);

    bool exploreAreaAnimating = false;
    bool explorePreviewMoving = false;
    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        uint8_t visibleAreaCount = std::min<uint8_t>(
            ExploreItemProgression::visibleAreaCount(gameState),
            Game::EXPLORE_AREA_COUNT);
        float target = selectedExploreArea < visibleAreaCount
            ? static_cast<float>(selectedExploreArea)
            : static_cast<float>(std::max<int>(0, visibleAreaCount - 1));
        float difference = target - exploreAreaAnimCursor;
        if (std::fabs(difference) < 0.05f) {
            if (exploreAreaAnimCursor != target) {
                exploreAreaAnimCursor = target;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
        } else {
            exploreAreaAnimCursor += difference * EXPLORE_AREA_CURSOR_LERP;
            if (std::fabs(target - exploreAreaAnimCursor) < 0.05f) {
                exploreAreaAnimCursor = target;
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        }
        exploreAreaAnimating = std::fabs(target - exploreAreaAnimCursor) >= 0.05f;

        bool previewLoadWasDue = explorePreviewLoadPending &&
            static_cast<int32_t>(nowMs - explorePreviewNextLoadAt) >= 0;
        updateExplorePreviewLoading(nowMs);
        if (previewLoadWasDue) {
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        }
        if (explorePreviewPool.count > 0) {
            uint32_t elapsed = nowMs - explorePreviewStartedAt;
            uint32_t visualCycle = elapsed / EXPLORE_PREVIEW_CYCLE_MS;
            if (visualCycle != explorePreviewVisualCycle) {
                explorePreviewVisualCycle = visualCycle;
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
            }
            explorePreviewMoving =
                elapsed % EXPLORE_PREVIEW_CYCLE_MS >
                EXPLORE_PREVIEW_HOLD_MS;
        }
    }

    if (battleAnimationActive &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        // Audio setup can block after the loop timestamp was sampled. Use a
        // fresh monotonic value so the first frame cannot underflow and skip.
        uint32_t animationNowMs = Platform::clock().millis();
        uint32_t elapsed = animationNowMs - battleAnimationStartedMs;
        uint8_t frame = elapsed < battleAnimationDurationMs
            ? static_cast<uint8_t>(std::min<uint32_t>(
                  6, elapsed / 80U + 1U))
            : 0;
        if (frame != battleAnimationFrame) {
            battleAnimationFrame = frame;
            requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
        }
        if (elapsed >= battleAnimationDurationMs) {
            battleAnimationActive = false;
            battleAnimationFrame = 0;
            battleAnimationDamage = 0;
            requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
            Platform::logf("[BattleAnim] complete side=%s elapsed=%lu\n",
                           battleAnimationAttackerWild ? "wild" : "player",
                           static_cast<unsigned long>(elapsed));
            advanceBattleTurn(animationNowMs);
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battleLogCount > 0 &&
        static_cast<int32_t>(nowMs - battleLogUntil) >= 0) {
        clearBattleLog();
        requestRenderRows(176, 224);
    }

#if STICKMON_HAS_CLAW
    if (autonomousExpedition &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        !battleAnimationActive && battleLogCount == 0) {
        // Reuse the game's deterministic battle AI for autonomous routes.
        // Existing animation and log gates keep each action interruptible.
        switch (battlePhase) {
        case BattleViewModel::Phase::ACTION:
            performBattleAttack(nowMs);
            break;
        case BattleViewModel::Phase::VICTORY:
            finishBattleVictory(nowMs);
            break;
        case BattleViewModel::Phase::FRIENDSHIP:
            // Do not silently add a wild monster to the player's team. The
            // player can still take over and make that choice on screen.
            resolveBattleFriendship(1, nowMs);
            break;
        case BattleViewModel::Phase::DEFEAT:
            finishBattleDefeat(nowMs);
            break;
        case BattleViewModel::Phase::SWITCH_SELECT:
            for (uint8_t slot = 0; slot < gameState.teamCount; ++slot) {
                if (slot != battlePlayerSlot &&
                    !gameState.team[slot].fainted &&
                    gameState.team[slot].hpCur > 0) {
                    performBattleSwitch(slot, false, nowMs);
                    break;
                }
            }
            break;
        case BattleViewModel::Phase::BAG_SELECT:
            battlePhase = BattleViewModel::Phase::ACTION;
            break;
        }
    }
#endif

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
                ? HOME_ROOM_TOP
                : sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                    ? MAIN_MENU_CONTENT_TOP : MENU_HEADER_HEIGHT,
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
            requestRenderRows(MAIN_MENU_CONTENT_TOP, 224);
        } else {
            menuVelocity = 0.0f;
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
    } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
               debugPopup == DebugViewModel::Popup::NONE && !pointerDown &&
               std::fabs(debugVelocity) > 0.12f) {
        float previous = debugScroll;
        debugScroll += debugVelocity;
        debugVelocity *= 0.86f;
        clampDebugScroll();
        if (std::fabs(debugScroll - previous) > 0.01f) {
            requestRenderRows(0, 224);
        } else {
            debugVelocity = 0.0f;
        }
#endif
    } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
               computerPage == ComputerViewModel::Page::STORAGE &&
               !pointerDown && std::fabs(computerVelocity) > 0.12f) {
        float previous = computerScroll;
        computerScroll += computerVelocity;
        computerVelocity *= 0.86f;
        clampComputerScroll();
        if (std::fabs(computerScroll - previous) > 0.01f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
        } else {
            computerVelocity = 0.0f;
        }
    }
#if STICKMON_HAS_CLAW
    else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
             computerPage == ComputerViewModel::Page::CLAW_SETUP &&
             clawLogView && !pointerDown &&
             std::fabs(clawLogVelocity) > 0.12f) {
        float previous = clawLogScroll;
        clawLogScroll += clawLogVelocity;
        clawLogVelocity *= 0.86f;
        clampClawLogScroll();
        if (std::fabs(clawLogScroll - previous) > 0.01f) {
            requestRenderRows(CLAW_LOG_TOP, 224);
        } else {
            clawLogVelocity = 0.0f;
        }
    }
#endif
    else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
              sceneFlow.current() == AppSceneFlow::Scene::SHOP) &&
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
        }
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
        (exploreAreaAnimating || explorePreviewMoving)) {
        requestRenderRows(MENU_HEADER_HEIGHT, 224);
    }
    uint32_t idleTimeoutMs = 0;
    switch (gameState.settings.idleTimeoutIndex) {
    case 0: idleTimeoutMs = 30UL * 1000UL; break;
    case 1: idleTimeoutMs = 2UL * 60UL * 1000UL; break;
    case 2: idleTimeoutMs = 5UL * 60UL * 1000UL; break;
    case 3: idleTimeoutMs = 10UL * 60UL * 1000UL; break;
    default: break;
    }
    bool sleepSafeScene = sceneFlow.current() != AppSceneFlow::Scene::BATTLE &&
                          sceneFlow.current() != AppSceneFlow::Scene::SHOWER &&
                          sceneFlow.current() != AppSceneFlow::Scene::PROGRESSION &&
                          !visitSession.busy();
    if (!lockRequested && !pointerDown && idleTimeoutMs > 0 &&
        sleepSafeScene && nowMs - lastInteractionMs >= idleTimeoutMs) {
        saveState();
        lockRequested = true;
        lastInteractionMs = nowMs;
    }
}

void AmoledApp::loadExplorePreview(uint32_t nowMs) {
    std::memset(explorePreviewSpeciesIds, 0,
                sizeof(explorePreviewSpeciesIds));
    std::memset(explorePreloadSpeciesIds, 0,
                sizeof(explorePreloadSpeciesIds));
    std::memset(explorePreviewFrames, 0, sizeof(explorePreviewFrames));
    std::memset(explorePreviewHidden, 0, sizeof(explorePreviewHidden));
    explorePreviewPool = ExplorePool::Pool{};
    explorePreloadSpeciesCount = 0;
    explorePreviewLoadPending = false;
    explorePreviewVisualCycle = UINT32_MAX;
    explorePreviewStartedAt = nowMs;
    explorePreviewNextLoadAt = nowMs + EXPLORE_PREVIEW_LOAD_DELAY_MS;

    uint8_t visibleAreaCount = std::min<uint8_t>(
        ExploreItemProgression::visibleAreaCount(gameState),
        Game::EXPLORE_AREA_COUNT);
    if (visibleAreaCount == 0) {
        clearExplorePreview();
        return;
    }
    if (selectedExploreArea >= visibleAreaCount) {
        selectedExploreArea = static_cast<uint8_t>(visibleAreaCount - 1);
    }

    if (ExploreItemProgression::isAreaUnlocked(
            selectedExploreArea, gameState)) {
        explorePreviewPool = buildExplorePreviewPool(
            gameState, selectedExploreArea);
        for (uint8_t index = 0; index < explorePreviewPool.count; ++index) {
            explorePreviewSpeciesIds[index] =
                explorePreviewPool.entries[index].speciesId;
        }
        PokemonSprites::setDynamicSceneSpecies(
            explorePreviewSpeciesIds, explorePreviewPool.count);
    } else {
        PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    }

    // Keep the visible unlocked pools resident so changing the left rail does
    // not trigger a synchronous decode on the first frame after a tap.
    explorePreloadSpeciesCount = collectExplorePreviewSpecies(
        gameState, explorePreloadSpeciesIds, EXPLORE_PRELOAD_CAP,
        selectedExploreArea);
    PokemonSprites::setPinnedDynamicSpecies(
        explorePreloadSpeciesIds, explorePreloadSpeciesCount);
    bool ready = PokemonSprites::preloadDynamicSpecies(
        explorePreloadSpeciesIds, explorePreloadSpeciesCount, 0);
    explorePreviewLoadPending = !ready;
    refreshExplorePreviewFrames();
}

void AmoledApp::updateExplorePreviewLoading(uint32_t nowMs) {
    if (!explorePreviewLoadPending ||
        static_cast<int32_t>(nowMs - explorePreviewNextLoadAt) < 0) {
        return;
    }
    bool ready = PokemonSprites::preloadDynamicSpecies(
        explorePreloadSpeciesIds, explorePreloadSpeciesCount, 1);
    refreshExplorePreviewFrames();
    explorePreviewLoadPending = !ready;
    explorePreviewNextLoadAt = nowMs +
        (ready ? EXPLORE_PREVIEW_BACKGROUND_LOAD_MS : 1);
}

void AmoledApp::refreshExplorePreviewFrames() {
    for (uint8_t index = 0; index < explorePreviewPool.count; ++index) {
        uint16_t speciesId = explorePreviewPool.entries[index].speciesId;
        const PokemonSprites::SpriteFrame* frame =
            PokemonSprites::findCachedSpeciesSprite(
                speciesId, PokemonSprites::SpriteKind::FRONT);
        if (!frame) {
            frame = PokemonSprites::findCachedSpeciesSprite(
                speciesId, PokemonSprites::SpriteKind::ICON_0);
        }
        explorePreviewFrames[index] = frame;
        explorePreviewHidden[index] =
            ExplorePool::isRare(explorePreviewPool.entries[index].rarity) &&
            !hasEncounteredSpecies(speciesId);
    }
}

void AmoledApp::clearExplorePreview() {
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    PokemonSprites::setPinnedDynamicSpecies(nullptr, 0);
    explorePreviewPool = ExplorePool::Pool{};
    std::memset(explorePreviewSpeciesIds, 0,
                sizeof(explorePreviewSpeciesIds));
    std::memset(explorePreloadSpeciesIds, 0,
                sizeof(explorePreloadSpeciesIds));
    std::memset(explorePreviewFrames, 0, sizeof(explorePreviewFrames));
    std::memset(explorePreviewHidden, 0, sizeof(explorePreviewHidden));
    explorePreloadSpeciesCount = 0;
    explorePreviewLoadPending = false;
}

void AmoledApp::selectExploreArea(uint8_t area, uint32_t nowMs) {
    uint8_t visibleAreaCount = std::min<uint8_t>(
        ExploreItemProgression::visibleAreaCount(gameState),
        Game::EXPLORE_AREA_COUNT);
    if (area >= visibleAreaCount || area == selectedExploreArea) return;
    selectedExploreArea = area;
    loadExplorePreview(nowMs);
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
        setToast(Ui::Amoled::MAP_FAILED, nowMs);
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
    exploreRoutePrompt = ExploreRouteViewModel::Prompt::NONE;
    exploreRouteIceSliding = false;
    exploreRouteIceDx = 0;
    exploreRouteIceDy = 0;
    exploreRoutePickupIndex = 0;
    exploreRoutePickupItem = EXPLORE_PICKUP_NONE;
    exploreRoutePickupAvailable = false;
    exploreRouteBossPending = false;
    exploreRoutePityEligible = false;
    exploreRouteBossSpeciesId = 0;
    exploreRouteBossLevel = 0;
    exploreRouteBossExperiencePercent = 100;
    exploreRouteSpecialKind = ExploreSpecial::Kind::NONE;
    exploreItemEffects.reset();
    const ExploreMapGenerator::Path& initialPath =
        exploreRouteMap.paths[exploreRoutePath];
    if (ExploreBoss::canPlaceOnPath(initialPath.pointCount)) {
        uint32_t slotIndex = ExploreSpecial::slotIndexFor(
            gameState.gameMinutesTotal);
        if (ExploreBossPity::syncSlot(gameState, slotIndex)) saveState();
        bool ownsMew = false;
        for (uint8_t slot = 0; slot < gameState.teamCount &&
             slot < Game::TEAM_CAP; ++slot) {
            ownsMew = ownsMew || gameState.team[slot].speciesId ==
                       ExploreSpecial::MEW;
        }
        for (uint8_t slot = 0; slot < gameState.storageCount &&
             slot < Game::STORAGE_CAP; ++slot) {
            ownsMew = ownsMew || gameState.storage[slot].speciesId ==
                       ExploreSpecial::MEW;
        }
        exploreRouteSpecialKind = ExploreSpecial::kindForArea(
            selectedExploreArea, gameState.specialBossDefeatedMask,
            slotIndex, gameState.roamingRerollCounts, ownsMew);
        if (exploreRouteSpecialKind != ExploreSpecial::Kind::NONE) {
            ExploreSpecial::Config config = ExploreSpecial::configFor(
                exploreRouteSpecialKind);
            exploreRouteBossPending = true;
            exploreRouteBossSpeciesId = config.speciesId;
            exploreRouteBossLevel = ExploreSpecial::encounterLevel(
                exploreRouteSpecialKind,
                ExploreBoss::configForArea(selectedExploreArea).level);
            exploreRouteBossExperiencePercent = config.experiencePercent;
        } else {
            uint8_t misses = gameState.normalBossMissCount[selectedExploreArea];
            bool guaranteed = ExploreBossPity::requiresGuaranteedEligibleRun(misses);
            uint32_t chance = ExploreBossPity::chanceForMisses(misses);
            if (guaranteed || GameRandom::range(0, ExploreBoss::SPAWN_ROLL_MAX) < chance) {
                const ExploreBoss::Config& config =
                    ExploreBoss::configForArea(selectedExploreArea);
                exploreRouteBossPending = true;
                exploreRouteBossSpeciesId = ExploreBoss::speciesForRoll(
                    selectedExploreArea,
                    GameRandom::range(0, ExploreBoss::CANDIDATE_COUNT));
                exploreRouteBossLevel = config.level;
                exploreRouteBossExperiencePercent = config.experiencePercent;
            } else {
                exploreRoutePityEligible = true;
            }
        }
    }
    exploreRouteDirection = exploreInwardDirection(exploreRouteMap.entry.edge);
    exploreRoutePetFrame = 0;
    ExploreRouteGeometry::WorldPoint start =
        ExploreRouteGeometry::pathPoint(exploreRouteMap.paths[0], 0);
    exploreRouteWorldX = exploreRouteFromX = exploreRouteTargetX = start.x;
    exploreRouteWorldY = exploreRouteFromY = exploreRouteTargetY = start.y;
    nextExploreRouteFrameMs = nowMs;
    updateExploreRouteCamera();
    placeExploreRoutePickup();
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

void AmoledApp::placeExploreRoutePickup() {
    exploreRoutePickupIndex = 0;
    exploreRoutePickupItem = EXPLORE_PICKUP_NONE;
    exploreRoutePickupAvailable = false;

    if (exploreRouteMap.pathCount == 0 || exploreRouteBossPending) return;
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (path.pointCount < 2) return;

    // Match Stick: place the pickup in the middle portion of the route and
    // move it to the nearest non-ice point so sliding cannot skip it.
    uint8_t first = path.pointCount < 3
        ? 1 : std::max<uint8_t>(1, path.pointCount / 3);
    uint8_t last = path.pointCount < 3
        ? static_cast<uint8_t>(path.pointCount - 1)
        : std::min<uint8_t>(path.pointCount - 2,
                            static_cast<uint8_t>(path.pointCount * 3 / 4));
    if (first > last) first = last = static_cast<uint8_t>(path.pointCount / 2);

    // Stick's 35% non-pickup roll is reserved for a guaranteed encounter.
    // AMOLED has no separately scheduled guaranteed encounter, so keep the
    // same event slot as a pickup rather than silently dropping the reward.
    uint8_t preferred = static_cast<uint8_t>(GameRandom::range(first, last + 1));
    exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
        exploreRouteMap, path, preferred, first, last);
    if (exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) {
        exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            exploreRouteMap, path, preferred, 1,
            static_cast<uint8_t>(path.pointCount - 1));
    }
    if (exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) return;

    exploreRoutePickupItem = rollExplorePickup(
        selectedExploreArea, gameState.stepsToday);
    exploreRoutePickupAvailable =
        exploreRoutePickupItem != EXPLORE_PICKUP_NONE;
    Platform::logf("[AmoledExplore] pickup index=%u item=%u\n",
                   static_cast<unsigned>(exploreRoutePickupIndex),
                   static_cast<unsigned>(exploreRoutePickupItem));
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
        finishExploreRouteAtEnd(nowMs);
        requestRenderRows(HOME_HEADER_HEIGHT, 224);
        return false;
    }

    if (!exploreRouteIceSliding) {
        int8_t dx = 0;
        int8_t dy = 0;
        if (ExploreIceSlide::begins(
                exploreRouteMap, path, exploreRouteIndex, dx, dy)) {
            exploreRouteIceSliding = true;
            exploreRouteIceDx = dx;
            exploreRouteIceDy = dy;
        }
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
    if (autonomousExpedition &&
        exploreRoutePrompt != ExploreRouteViewModel::Prompt::NONE) {
        // Route prompts are player choices in the normal UI. Autonomous life
        // takes the low-risk "continue" branch and keeps the route moving.
        exploreRoutePrompt = ExploreRouteViewModel::Prompt::NONE;
        exploreRouteAutoWalk = true;
        beginExploreRouteStep(nowMs);
        requestRenderRows(HOME_HEADER_HEIGHT, 224);
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
    bool continueIce = exploreRouteIceSliding && ExploreIceSlide::continues(
        exploreRouteMap, path, exploreRouteIndex,
        exploreRouteIceDx, exploreRouteIceDy);
    if (!continueIce) {
        exploreRouteIceSliding = false;
        exploreRouteIceDx = 0;
        exploreRouteIceDy = 0;
        resolveExploreStepEvent(nowMs);
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE) {
        requestFullRender();
        return;
    }
    if (exploreRouteIndex + 1 >= path.pointCount) {
        if (finishExploreRouteAtEnd(nowMs)) return;
    } else if (continueIce || exploreRouteAutoWalk) {
        beginExploreRouteStep(nowMs);
    }
    requestRenderRows(HOME_HEADER_HEIGHT, 224);
}

bool AmoledApp::finishExploreRouteAtEnd(uint32_t nowMs) {
    if (exploreRouteBossPending) {
        if (beginExploreEncounter(
                nowMs, true, exploreRouteBossSpeciesId,
                exploreRouteBossLevel, exploreRouteBossExperiencePercent,
                exploreRouteSpecialKind)) {
            exploreRouteBossPending = false;
            exploreRouteAutoWalk = false;
            return true;
        }
        // Keep the pending boss at the route end when the active team cannot
        // enter battle yet; a later tap can retry after the player recovers.
        exploreRouteAutoWalk = false;
        return false;
    }
    if (exploreRoutePityEligible) {
        ExploreBossPity::increment(gameState, selectedExploreArea);
        exploreRoutePityEligible = false;
        saveState();
    }
    exploreRouteComplete = true;
    exploreRouteAutoWalk = false;
    if (autonomousExpedition) {
        // A completed autonomous expedition has no player-facing route screen
        // to acknowledge. Return home so the normal care loop can continue.
        leaveExploreRoute();
        return true;
    }
    return false;
}

void AmoledApp::resolveExploreStepEvent(uint32_t nowMs) {
    gameState.stepsToday = static_cast<uint16_t>(std::min<uint32_t>(
        60000, static_cast<uint32_t>(gameState.stepsToday) + 1));

    bool honeyEncounter = exploreItemEffects.honeyEncounterPending();
    bool repelActive = exploreItemEffects.repelStepsRemaining() > 0;

    // The completed step consumes one repel charge before resolving the
    // interaction at this route point, matching Stick's ordering.
    exploreItemEffects.completeWalkStep();
    if (exploreRoutePickupAvailable &&
        exploreRouteIndex == exploreRoutePickupIndex) {
        resolveExploreRoutePickup(nowMs);
        return;
    }

    if (exploreRouteSteps > 0 &&
        exploreRouteSteps % 9 == 0 &&
        selectedExploreArea > 0) {
        exploreRoutePrompt = selectedExploreArea >= 3
            ? ExploreRouteViewModel::Prompt::PUZZLE
            : ExploreRouteViewModel::Prompt::BLOCKED;
        exploreRouteAutoWalk = false;
        saveState();
        setToast(exploreRoutePrompt == ExploreRouteViewModel::Prompt::PUZZLE
                     ? Ui::Amoled::SOLVE : Ui::Amoled::PATH_BLOCKED,
                 nowMs, 1300);
        requestFullRender();
        return;
    }

    uint32_t roll = GameRandom::range(0, 10000);
    if ((honeyEncounter || (!repelActive && roll < 1400)) &&
        beginExploreEncounter(nowMs)) {
        if (honeyEncounter) exploreItemEffects.consumeHoneyEncounter();
        return;
    }
    saveState();
}

void AmoledApp::resolveExploreRoutePickup(uint32_t nowMs) {
    if (!exploreRoutePickupAvailable) return;
    exploreRoutePickupAvailable = false;
    exploreRouteAutoWalk = false;

    const ExplorePickupTable& table =
        explorePickupTableForArea(selectedExploreArea);
    if (exploreRoutePickupItem == EXPLORE_PICKUP_COIN) {
        uint32_t coins = GameRandom::range(table.minCoin,
                                           static_cast<uint32_t>(table.maxCoin) + 1);
        gameState.coins += coins;
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::PICKUP_COIN_FMT,
                      static_cast<unsigned long>(coins));
        setToast(battleMessage, nowMs, 1300);
        saveState();
        return;
    }

    Game::ItemId item = Game::ItemId::COUNT;
    switch (exploreRoutePickupItem) {
    case EXPLORE_PICKUP_POTION: item = Game::ItemId::POTION; break;
    case EXPLORE_PICKUP_SUPER_POTION: item = Game::ItemId::SUPER_POTION; break;
    case EXPLORE_PICKUP_ANTIDOTE: item = Game::ItemId::ANTIDOTE; break;
    case EXPLORE_PICKUP_RARE_CANDY: item = Game::ItemId::CANDY; break;
    case EXPLORE_PICKUP_MAX_POTION: item = Game::ItemId::MAX_POTION; break;
    case EXPLORE_PICKUP_FULL_RESTORE: item = Game::ItemId::FULL_RESTORE; break;
    case EXPLORE_PICKUP_FULL_HEAL: item = Game::ItemId::FULL_HEAL; break;
    case EXPLORE_PICKUP_REVIVE: item = Game::ItemId::REVIVE; break;
    case EXPLORE_PICKUP_MAX_REPEL: item = Game::ItemId::MAX_REPEL; break;
    case EXPLORE_PICKUP_HONEY: item = Game::ItemId::HONEY; break;
    case EXPLORE_PICKUP_NUGGET: item = Game::ItemId::NUGGET; break;
    case EXPLORE_PICKUP_BIG_PEARL: item = Game::ItemId::BIG_PEARL; break;
    case EXPLORE_PICKUP_STAR_PIECE: item = Game::ItemId::STAR_PIECE; break;
    case EXPLORE_PICKUP_HEART_SCALE: item = Game::ItemId::HEART_SCALE; break;
    default: break;
    }

    const char* name = item == Game::ItemId::COUNT
        ? nullptr : Game::ShopService::shortName(item);
    if (item == Game::ItemId::COUNT ||
        !Game::ItemInventory::add(gameState, item)) {
        setToast(Ui::Amoled::BAG_FULL, nowMs);
        saveState();
        return;
    }

    std::snprintf(battleMessage, sizeof(battleMessage), Ui::Explore::PICKUP_FMT,
                  name ? name : Ui::Amoled::ITEM);
    setToast(battleMessage, nowMs, 1300);
    saveState();
}

bool AmoledApp::beginExploreEncounter(
    uint32_t nowMs, bool boss, uint16_t speciesOverride,
    uint8_t levelOverride, uint16_t experiencePercent,
    ExploreSpecial::Kind specialKind) {
    if (gameState.teamCount == 0 || gameState.team[0].fainted ||
        gameState.team[0].hpCur == 0) {
        setToast(Ui::Menu::PET_REST, nowMs);
        return false;
    }
    const Species* species = nullptr;
    uint8_t levelMinimum = 1;
    uint8_t levelMaximum = Game::LEVEL_MAX;
    if (boss && speciesOverride != 0) {
        species = findSpecies(speciesOverride);
    } else {
        AmoledEncounterTable table = encounterTableForArea(selectedExploreArea);
        if (!table.entries || table.count == 0) return false;
        uint32_t totalWeight = 0;
        for (uint8_t index = 0; index < table.count; ++index) {
            totalWeight += table.entries[index].weight;
        }
        if (totalWeight == 0) return false;
        uint32_t roll = GameRandom::range(0, totalWeight);
        const ExploreEncounters::Entry* picked = &table.entries[0];
        for (uint8_t index = 0; index < table.count; ++index) {
            if (roll < table.entries[index].weight) {
                picked = &table.entries[index];
                break;
            }
            roll -= table.entries[index].weight;
        }
        species = findSpecies(picked->speciesId);
        levelMinimum = picked->minLevel;
        levelMaximum = picked->maxLevel;
    }
    if (!species) return false;
    recordEncounteredSpecies(species->id);
    uint8_t level = levelOverride;
    if (level == 0) {
        int16_t targetLevel = gameState.team[0].level;
        targetLevel += static_cast<int16_t>(GameRandom::range(0, 3)) - 1;
        level = static_cast<uint8_t>(std::clamp<int16_t>(
            targetLevel, levelMinimum, levelMaximum));
    }

    battleWild = Game::MonsterFactory::create(species->id, level);
    battleWild.origin = Game::Origin::UNKNOWN;
    battleWild.metArea = selectedExploreArea;
    battleWild.metAt = gameState.gameMinutesTotal * 60UL;
    battleWild.lastSeenAt = battleWild.metAt;
    battleIsBoss = boss;
    battleExperiencePercent = experiencePercent;
    battleSpecialKind = specialKind;

    uint16_t dynamicSpecies[] = {
        gameState.team[0].speciesId, battleWild.speciesId,
    };
    PokemonSprites::setDynamicSceneSpecies(dynamicSpecies, 2);
    PokemonSprites::preloadDynamicSpecies(dynamicSpecies, 2);
    BattleSystem::resetVolatile(battlePlayerState);
    BattleSystem::resetVolatile(battleWildState);
    battleTurnController.reset();
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    BattleSystem::EffectResolution effects;
    battlePlayerSlot = 0;
    const Species* playerSpecies = findSpecies(
        gameState.team[battlePlayerSlot].speciesId);
    if (playerSpecies) {
        BattleSystem::applyEntryAbility(
            *playerSpecies, battlePlayerState, *species, battleWildState,
            effects);
        effects = BattleSystem::EffectResolution{};
        BattleSystem::applyEntryAbility(
            *species, battleWildState, *playerSpecies, battlePlayerState,
            effects);
    }
    battlePhase = BattleViewModel::Phase::ACTION;
    battlePressedItem = 0xFF;
    battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::OFFER;
    battleFriendshipContactSlot = 0xFF;
    battleVictoryOldLevel = 1;
    battleVictoryLeveledUp = false;
    battleAnimationActive = false;
    battleAnimationAttackerWild = false;
    battleAnimationHit = false;
    battleAnimationDamage = 0;
    battleAnimationFrame = 0;
    battleAudioPending = false;
    battleAudioReady = false;
    battlePendingSfx = 0xFF;
    battlePendingCrySpecies = 0;
    battleRewardExp = 0;
    battleRewardCoins = 0;
    clearBattleLog();
    std::snprintf(battleMessage, sizeof(battleMessage), Ui::Amoled::WILD_FMT,
                  species->name);
    pushBattleLog(nowMs);
    toast = nullptr;
    exploreRouteAutoWalk = false;
    sceneFlow.enter(AppSceneFlow::Scene::BATTLE);
    requestFullRender();
    return true;
}

void AmoledApp::pushBattleLog(uint32_t nowMs) {
    if (!battleMessage[0]) return;
    uint8_t line = battleLogCount;
    if (battleLogCount < 2) {
        ++battleLogCount;
    } else {
        std::memcpy(battleLogLines[0], battleLogLines[1],
                    sizeof(battleLogLines[0]));
        line = 1;
    }
    std::snprintf(battleLogLines[line], sizeof(battleLogLines[line]), "%s",
                  battleMessage);
    battleLogUntil = nowMs + 1000;
    requestRenderRows(176, 224);
}

void AmoledApp::clearBattleLog() {
    battleLogCount = 0;
    battleLogUntil = 0;
    battleMessage[0] = '\0';
    battleLogLines[0][0] = '\0';
    battleLogLines[1][0] = '\0';
}

void AmoledApp::performBattleAttack(uint32_t nowMs) {
    if (battlePhase != BattleViewModel::Phase::ACTION) return;
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    battleTurnPlan = battleTurnController.planAiTurn(
        player, *playerSpecies, battlePlayerState,
        battleWild, *wildSpecies, battleWildState);
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    performBattlePlannedAction(nowMs);
}

void AmoledApp::performBattlePlayerAction(
    const BattleTurnController::Action& action, uint32_t nowMs) {
    if (action.side != BattleTurnController::Side::PLAYER ||
        battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) {
        return;
    }
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    bool releasingCharge = BattleSystem::isChargingMove(battlePlayerState);
    uint8_t specialSlot = action.specialSlot;
    Game::MoveId moveId = action.moveId;
    const BattleTurnController::Action* wildAction =
        battleTurnPlan.actionFor(BattleTurnController::Side::WILD);
    const MoveInfo* wildMove = findMove(wildAction ? wildAction->moveId : 0);
    BattleSystem::ActionCheckResult check = BattleSystem::checkAction(
        player, *playerSpecies, battlePlayerState, moveId,
        wildMove && wildMove->power > 0 &&
            wildMove->damageClass != DamageClass::STATUS);
    if (!check.canAct()) {
        if (releasingCharge) {
            BattleSystem::clearChargingMove(battlePlayerState);
        }
        if (check.selfDamage > 0) {
            player.hpCur = static_cast<uint16_t>(
                player.hpCur > check.selfDamage ? player.hpCur - check.selfDamage : 0);
            battleTurnDamaged[0] = true;
        }
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Amoled::ACTION_BLOCKED);
        pushBattleLog(nowMs);
    } else {
        const MoveInfo* selectedMove = findMove(moveId);
        if (selectedMove && BattleSystem::moveRequiresCharge(moveId) &&
            !releasingCharge) {
            BattleSystem::beginChargingMove(
                battlePlayerState, moveId, specialSlot);
            if (selectedMove->flags & MOVE_FLAG_CHARGE_DEFENSE) {
                uint8_t defense = static_cast<uint8_t>(BattleStat::DEFENSE);
                battlePlayerState.statStages[defense] = std::min<int8_t>(
                    6, battlePlayerState.statStages[defense] + 1);
            }
            battlePhase = BattleViewModel::Phase::ACTION;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::CHARGING);
            pushBattleLog(nowMs);
            advanceBattleTurn(nowMs);
            return;
        }
        if (releasingCharge) BattleSystem::clearChargingMove(battlePlayerState);
        BattleSystem::DamageContext damageContext;
        damageContext.attackerMovesSecond = battleTurnActionIndex > 0;
        damageContext.defenderDamagedThisTurn = battleTurnDamaged[1];
        damageContext.defenderMoveIsDamaging = wildMove && wildMove->power > 0 &&
            wildMove->damageClass != DamageClass::STATUS;
        damageContext.allowForceWildEnd = true;
        BattleSystem::DamageResult damage = BattleSystem::calcBasicDamage(
            player, *playerSpecies, battleWild, *wildSpecies,
            specialSlot, battlePlayerState,
            battleWildState, damageContext);
        uint16_t dealt = std::min<uint16_t>(damage.damage, battleWild.hpCur);
        battleWild.hpCur = static_cast<uint16_t>(battleWild.hpCur - dealt);
        if (dealt > 0) battleTurnDamaged[1] = true;
        const MoveInfo* move = findMove(damage.moveId);
        if (move) {
            BattleSystem::recordMoveResult(
                battlePlayerState, player, *playerSpecies, *move,
                !damage.missed && !damage.failed,
                specialSlot);
            BattleSystem::applyMoveEffects(
                *move, player, *playerSpecies, battlePlayerState,
                battleWild, *wildSpecies, battleWildState, dealt,
                battleTurnPlan.hasActionAfter(
                    battleTurnActionIndex, BattleTurnController::Side::WILD));
        }
        if (damage.missed) {
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::ATTACK_MISSED);
        } else {
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Amoled::HIT_FMT, dealt);
        }
        pushBattleLog(nowMs);
        battleAnimationActive = true;
        battleAnimationAttackerWild = false;
        battleAnimationHit = !damage.missed && !damage.failed && dealt > 0;
        battleAnimationDamage = dealt;
        battleAnimationDurationMs = 480;
        battleAnimationFrame = 1;
        battleAnimationStartedMs = Platform::clock().millis();
        battleAudioPending = true;
        battleAudioReady = false;
        battlePendingSfx = static_cast<uint8_t>(
            battleAnimationHit
                ? (damage.effectiveness > 100 ? SfxCue::DAMAGE_SUPER
                   : damage.effectiveness < 100 ? SfxCue::DAMAGE_WEAK
                                                 : SfxCue::DAMAGE_NORMAL)
                : SfxCue::UI_CANCEL);
        battlePendingCrySpecies = player.speciesId;
        Platform::logf("[BattleAnim] start side=player hit=%u damage=%u\n",
                       battleAnimationHit ? 1 : 0, dealt);
        requestFullRender();
        return;
    }
    advanceBattleTurn(nowMs);
}

bool AmoledApp::resolveBattleFaint(uint32_t nowMs) {
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) return false;
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!wildSpecies) return false;
    battlePhase = BattleViewModel::Phase::ACTION;
    if (player.hpCur == 0) {
        player.fainted = true;
        player.lastSeenAt = Game::gameSecondsForMinutes(
            gameState.gameMinutesTotal);
        bool hasSwitch = false;
        for (uint8_t slot = 0; slot < gameState.teamCount &&
             slot < Game::TEAM_CAP; ++slot) {
            const Game::MonsterRuntime& candidate = gameState.team[slot];
            if (slot != battlePlayerSlot && !candidate.fainted &&
                candidate.hpCur > 0) {
                hasSwitch = true;
                break;
            }
        }
        battlePhase = hasSwitch ? BattleViewModel::Phase::SWITCH_SELECT
                                : BattleViewModel::Phase::DEFEAT;
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      hasSwitch ? Ui::Amoled::CHOOSE_NEXT
                                : Ui::Amoled::PET_FAINTED);
        pushBattleLog(nowMs);
        saveState();
        battleTurnPlan = BattleTurnController::TurnPlan{};
        battleTurnActionIndex = 0;
        requestFullRender();
        return true;
    }
    if (battleWild.hpCur == 0) {
        battleWild.fainted = true;
        battleRewardExp = BattleSystem::scaledExperienceReward(
            BattleSystem::experienceReward(*wildSpecies, battleWild.level),
            battleExperiencePercent);
        battleRewardCoins = battleIsBoss
            ? ExploreBoss::victoryCoinReward(true)
            : 8 + static_cast<uint32_t>(battleWild.level) * 2;
        battlePhase = BattleViewModel::Phase::VICTORY;
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Amoled::VICTORY_FMT,
                      battleRewardExp);
        pushBattleLog(nowMs);
        battleTurnPlan = BattleTurnController::TurnPlan{};
        battleTurnActionIndex = 0;
        requestFullRender();
        return true;
    }
    return false;
}

void AmoledApp::performBattleSwitch(uint8_t teamSlot, bool consumesTurn,
                                    uint32_t nowMs) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
        teamSlot == battlePlayerSlot) {
        setToast(Ui::Amoled::CHOOSE_OTHER, nowMs);
        return;
    }
    Game::MonsterRuntime& candidate = gameState.team[teamSlot];
    if (candidate.fainted || candidate.hpCur == 0) {
        setToast(Ui::Amoled::CANNOT_SWITCH, nowMs);
        return;
    }
    const Species* candidateSpecies = findSpecies(candidate.speciesId);
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!candidateSpecies || !wildSpecies) return;
    battlePlayerSlot = teamSlot;
    BattleSystem::resetVolatile(battlePlayerState);
    battleTurnController.resetPlayerAi();
    BattleSystem::EffectResolution effects;
    BattleSystem::applyEntryAbility(
        *candidateSpecies, battlePlayerState, *wildSpecies,
        battleWildState, effects);
    uint16_t dynamicSpecies[] = {candidate.speciesId, battleWild.speciesId};
    PokemonSprites::setDynamicSceneSpecies(dynamicSpecies, 2);
    PokemonSprites::preloadDynamicSpecies(dynamicSpecies, 2);
    battlePhase = BattleViewModel::Phase::ACTION;
    battlePressedItem = 0xFF;
    std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                  Ui::Amoled::SWITCHED_IN);
    pushBattleLog(nowMs);
    saveState();
    if (consumesTurn) performBattleWildTurn(nowMs);
    else requestFullRender();
}

void AmoledApp::performBattleWildTurn(uint32_t nowMs) {
    if (battlePhase != BattleViewModel::Phase::ACTION) return;
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    battleTurnPlan = battleTurnController.planWildOnly(
        player, *playerSpecies, battlePlayerState,
        battleWild, *wildSpecies, battleWildState);
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    performBattlePlannedAction(nowMs);
}

void AmoledApp::performBattleWildAction(
    const BattleTurnController::Action& action, uint32_t nowMs) {
    if (action.side != BattleTurnController::Side::WILD ||
        battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) {
        return;
    }
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    bool releasingCharge = BattleSystem::isChargingMove(battleWildState);
    uint8_t specialSlot = action.specialSlot;
    Game::MoveId moveId = action.moveId;
    const BattleTurnController::Action* playerAction =
        battleTurnPlan.actionFor(BattleTurnController::Side::PLAYER);
    const MoveInfo* playerMove = findMove(
        playerAction ? playerAction->moveId : 0);
    BattleSystem::ActionCheckResult check = BattleSystem::checkAction(
        battleWild, *wildSpecies, battleWildState, moveId,
        playerMove && playerMove->power > 0 &&
            playerMove->damageClass != DamageClass::STATUS);
    if (check.canAct()) {
        const MoveInfo* selectedMove = findMove(moveId);
        if (selectedMove && BattleSystem::moveRequiresCharge(moveId) &&
            !releasingCharge) {
            BattleSystem::beginChargingMove(
                battleWildState, moveId, specialSlot);
            if (selectedMove->flags & MOVE_FLAG_CHARGE_DEFENSE) {
                uint8_t defense = static_cast<uint8_t>(BattleStat::DEFENSE);
                battleWildState.statStages[defense] = std::min<int8_t>(
                    6, battleWildState.statStages[defense] + 1);
            }
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::WILD_CHARGE);
            pushBattleLog(nowMs);
            advanceBattleTurn(nowMs);
            return;
        }
        if (releasingCharge) BattleSystem::clearChargingMove(battleWildState);
        BattleSystem::DamageContext damageContext;
        damageContext.attackerMovesSecond = battleTurnActionIndex > 0;
        damageContext.defenderDamagedThisTurn = battleTurnDamaged[0];
        damageContext.defenderMoveIsDamaging = playerMove && playerMove->power > 0 &&
            playerMove->damageClass != DamageClass::STATUS;
        damageContext.allowForceWildEnd = false;
        BattleSystem::DamageResult damage = BattleSystem::calcBasicDamage(
            battleWild, *wildSpecies, player, *playerSpecies, specialSlot,
            battleWildState, battlePlayerState, damageContext);
        uint16_t dealt = std::min<uint16_t>(damage.damage, player.hpCur);
        player.hpCur = static_cast<uint16_t>(player.hpCur - dealt);
        if (dealt > 0) battleTurnDamaged[0] = true;
        const MoveInfo* move = findMove(damage.moveId);
        if (move) {
            BattleSystem::recordMoveResult(
                battleWildState, battleWild, *wildSpecies, *move,
                !damage.missed && !damage.failed, specialSlot);
            BattleSystem::applyMoveEffects(
                *move, battleWild, *wildSpecies, battleWildState,
                player, *playerSpecies, battlePlayerState, dealt,
                battleTurnPlan.hasActionAfter(
                    battleTurnActionIndex, BattleTurnController::Side::PLAYER));
        }
        if (damage.missed) {
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::WILD_MISSED);
        } else {
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Amoled::WILD_HIT_FMT, dealt);
        }
        pushBattleLog(nowMs);
        battleAnimationActive = true;
        battleAnimationAttackerWild = true;
        battleAnimationHit = !damage.missed && !damage.failed && dealt > 0;
        battleAnimationDamage = dealt;
        battleAnimationDurationMs = 480;
        battleAnimationFrame = 1;
        battleAnimationStartedMs = Platform::clock().millis();
        battleAudioPending = true;
        battleAudioReady = false;
        battlePendingSfx = static_cast<uint8_t>(
            battleAnimationHit
                ? (damage.effectiveness > 100 ? SfxCue::DAMAGE_SUPER
                   : damage.effectiveness < 100 ? SfxCue::DAMAGE_WEAK
                                                 : SfxCue::DAMAGE_NORMAL)
                : SfxCue::UI_CANCEL);
        battlePendingCrySpecies = battleWild.speciesId;
        Platform::logf("[BattleAnim] start side=wild hit=%u damage=%u\n",
                       battleAnimationHit ? 1 : 0, dealt);
        requestFullRender();
        return;
    } else {
        if (releasingCharge) {
            BattleSystem::clearChargingMove(battleWildState);
        }
        if (check.selfDamage > 0) {
            battleWild.hpCur = static_cast<uint16_t>(
                battleWild.hpCur > check.selfDamage
                    ? battleWild.hpCur - check.selfDamage : 0);
            battleTurnDamaged[1] = true;
        }
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Amoled::WILD_BLOCKED);
        pushBattleLog(nowMs);
    }
    advanceBattleTurn(nowMs);
}

void AmoledApp::performBattlePlannedAction(uint32_t nowMs) {
    if (battleTurnActionIndex >= battleTurnPlan.count) {
        advanceBattleTurn(nowMs);
        return;
    }
    const BattleTurnController::Action& action =
        battleTurnPlan.actions[battleTurnActionIndex];
    if (action.side == BattleTurnController::Side::WILD) {
        performBattleWildAction(action, nowMs);
    } else {
        performBattlePlayerAction(action, nowMs);
    }
}

void AmoledApp::advanceBattleTurn(uint32_t nowMs) {
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* playerSpecies = findSpecies(player.speciesId);
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;

    if (resolveBattleFaint(nowMs)) return;

    ++battleTurnActionIndex;
    if (battleTurnActionIndex < battleTurnPlan.count) {
        performBattlePlannedAction(nowMs);
        return;
    }

    BattleSystem::resolveEndTurn(player, *playerSpecies, battlePlayerState);
    BattleSystem::resolveEndTurn(battleWild, *wildSpecies, battleWildState);
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    if (resolveBattleFaint(nowMs)) return;

    if (BattleSystem::isChargingMove(battlePlayerState) ||
        battlePlayerState.lockedMoveId != 0) {
        battleTurnPlan = battleTurnController.planAiTurn(
            player, *playerSpecies, battlePlayerState,
            battleWild, *wildSpecies, battleWildState);
        performBattlePlannedAction(nowMs);
        return;
    }
    requestFullRender();
}

void AmoledApp::performBattleBag(uint32_t nowMs) {
    static constexpr Game::ItemId ITEMS[] = {
        Game::ItemId::POTION, Game::ItemId::SUPER_POTION,
        Game::ItemId::MAX_POTION, Game::ItemId::FULL_RESTORE,
        Game::ItemId::FULL_HEAL, Game::ItemId::REVIVE,
        Game::ItemId::ANTIDOTE, Game::ItemId::PARALYZE_HEAL,
    };
    battleBagCount = 0;
    for (Game::ItemId item : ITEMS) {
        if (Game::ItemInventory::count(gameState, item) == 0) continue;
        if (battleBagCount >= 4) break;
        battleBagItems[battleBagCount++] = item;
    }
    if (battleBagCount == 0) {
        setToast(Ui::Amoled::NO_MEDICINE, nowMs);
        return;
    }
    battlePhase = BattleViewModel::Phase::BAG_SELECT;
    battlePressedItem = 0xFF;
    requestFullRender();
}

void AmoledApp::performBattleBagItem(Game::ItemId item, uint32_t nowMs) {
    if (battlePhase != BattleViewModel::Phase::BAG_SELECT) return;
    Game::ItemInventory::UseResult result = Game::ItemInventory::useOnTeam(
        gameState, item, battlePlayerSlot);
    if (result != Game::ItemInventory::UseResult::USED) {
        setToast(Ui::Amoled::CANNOT_USE, nowMs);
        return;
    }
    saveState();
    std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                  Ui::Amoled::ITEM_USED);
    pushBattleLog(nowMs);
    battlePhase = BattleViewModel::Phase::ACTION;
    performBattleWildTurn(nowMs);
}

void AmoledApp::performBattleFlee(uint32_t nowMs) {
    if (BattleSystem::canFlee(battlePlayerState) &&
        GameRandom::range(0, 100) < 60) {
        closeBattle(nowMs);
        setToast(Ui::Amoled::GOT_AWAY, nowMs);
        return;
    }
    std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                  Ui::Amoled::CANNOT_ESCAPE);
    pushBattleLog(nowMs);
    performBattleWildTurn(nowMs);
}

void AmoledApp::finishBattleVictory(uint32_t nowMs) {
    if (battlePhase != BattleViewModel::Phase::VICTORY) return;
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) {
        closeBattle(nowMs);
        return;
    }
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* species = findSpecies(player.speciesId);
    if (!species) {
        closeBattle(nowMs);
        return;
    }
    Game::ExperienceService::Result playerExperience =
        Game::ExperienceService::add(player, *species, battleRewardExp);
    battleVictoryOldLevel = playerExperience.oldLevel;
    battleVictoryLeveledUp = playerExperience.leveledUp;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        Game::MonsterRuntime& reserve = gameState.team[slot];
        if (reserve.fainted || reserve.hpCur == 0) continue;
        const Species* reserveSpecies = findSpecies(reserve.speciesId);
        if (!reserveSpecies) continue;
        uint16_t reserveReward = BattleSystem::scaledExperienceReward(
            battleRewardExp, BattleSystem::RESERVE_EXP_PERCENT);
        Game::ExperienceService::add(reserve, *reserveSpecies, reserveReward);
    }
    gameState.coins += battleRewardCoins;
    if (battleIsBoss) {
        uint8_t area = selectedExploreArea;
        if (area < Game::EXPLORE_AREA_COUNT) {
            if (gameState.explorePoolRerollCounts[area] < UINT8_MAX) {
                ++gameState.explorePoolRerollCounts[area];
            }
            if (battleSpecialKind == ExploreSpecial::Kind::NONE) {
                ExploreBossPity::resetArea(gameState, area);
            }
            uint8_t defeated = ExploreSpecial::defeatedBit(battleSpecialKind);
            if (defeated != 0) {
                gameState.specialBossDefeatedMask = static_cast<uint8_t>(
                    gameState.specialBossDefeatedMask | defeated);
            }
        }
    }
    bool allowsFriendship = battleSpecialKind == ExploreSpecial::Kind::NONE ||
        ExploreSpecial::configFor(battleSpecialKind).allowsFriendship;
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!wildSpecies) {
        saveState();
        finishBattleAfterFriendship(nowMs);
        return;
    }
    Game::FriendshipService::OfferResult offer =
        Game::FriendshipService::evaluateOffer(
            gameState, *wildSpecies, battleWild,
            battleIsBoss, allowsFriendship, 0);
    if (offer.offered) {
        battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::OFFER;
        battleFriendshipContactSlot = 0xFF;
        battlePhase = BattleViewModel::Phase::FRIENDSHIP;
        battlePressedItem = 0xFF;
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Amoled::BECOME_FRIEND);
        pushBattleLog(nowMs);
        saveState();
        requestFullRender();
        return;
    }
    if (offer.eligible) {
        Game::FriendshipService::recordFailure(
            gameState, battleWild.speciesId);
    }
    saveState();
    finishBattleAfterFriendship(nowMs);
}

void AmoledApp::resolveBattleFriendship(uint8_t choice, uint32_t nowMs) {
    if (battlePhase != BattleViewModel::Phase::FRIENDSHIP) return;
    if (battleFriendshipPrompt == BattleViewModel::FriendshipPrompt::OFFER) {
        if (choice != 0) {
            finishBattleAfterFriendship(nowMs);
            return;
        }
        uint8_t contactSlot = 0xFF;
        if (gameState.storageCount >= Game::STORAGE_CAP) {
            battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::FULL;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::BOX_FULL);
            pushBattleLog(nowMs);
            battlePressedItem = 0xFF;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
            return;
        }
        if (!Game::FriendshipService::recordContact(
                gameState, battleWild, selectedExploreArea,
                gameState.gameMinutesTotal * 60UL, &contactSlot)) {
            battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::FULL;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::BOX_FULL);
            pushBattleLog(nowMs);
            battlePressedItem = 0xFF;
            requestRenderRows(MENU_HEADER_HEIGHT, 224);
            return;
        }
        Game::FriendshipService::recordSuccess(
            gameState, battleWild.speciesId);
        battleFriendshipContactSlot = contactSlot;
        saveState();
        if (gameState.teamCount < Game::TEAM_CAP) {
            battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::TEAM;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::ADD_TO_TEAM);
        } else {
            battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::ACQUIRED;
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Amoled::FRIEND_ADDED);
        }
        pushBattleLog(nowMs);
        battlePressedItem = 0xFF;
        requestRenderRows(MENU_HEADER_HEIGHT, 224);
        return;
    }
    if (battleFriendshipPrompt == BattleViewModel::FriendshipPrompt::TEAM) {
        if (choice == 0 && battleFriendshipContactSlot != 0xFF) {
            Game::FriendshipService::InviteResult result =
                Game::FriendshipService::inviteContact(
                    gameState, battleFriendshipContactSlot,
                    gameState.gameMinutesTotal);
            if (result == Game::FriendshipService::InviteResult::JOINED) {
                uint16_t speciesIds[Game::TEAM_CAP] = {};
                uint8_t count = Game::TeamRoster::memberCount(gameState);
                for (uint8_t slot = 0; slot < count; ++slot) {
                    speciesIds[slot] = gameState.team[slot].speciesId;
                }
                PokemonSprites::syncTeamCache(speciesIds, count);
                battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::ACQUIRED;
                std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                              Ui::Amoled::TEAM_MEMBER_ADDED);
                pushBattleLog(nowMs);
                saveState();
                requestRenderRows(MENU_HEADER_HEIGHT, 224);
                return;
            }
        }
        finishBattleAfterFriendship(nowMs);
        return;
    }
    finishBattleAfterFriendship(nowMs);
}

void AmoledApp::finishBattleAfterFriendship(uint32_t nowMs) {
    saveState();
    if (battleVictoryLeveledUp) {
        autonomousExpedition = false;
        openProgressionScene(AppSceneFlow::Scene::EXPLORE_ROUTE,
                             battlePlayerSlot, battleVictoryOldLevel, nowMs);
        return;
    }
    closeBattle(nowMs);
    if (autonomousExpedition) exploreRouteAutoWalk = true;
    setToast(Ui::Amoled::BATTLE_COMPLETE, nowMs);
}

void AmoledApp::finishBattleDefeat(uint32_t nowMs) {
    if (battlePhase != BattleViewModel::Phase::DEFEAT) return;
    if (battlePlayerSlot < gameState.teamCount &&
        battlePlayerSlot < Game::TEAM_CAP) {
        Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
        player.hpCur = 0;
        player.fainted = true;
        player.lastSeenAt = Game::gameSecondsForMinutes(
            gameState.gameMinutesTotal);
    }
    saveState();
    autonomousExpedition = false;
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleActive) {
        debugBattleActive = false;
        sceneFlow.enter(AppSceneFlow::Scene::DEBUG);
    } else {
        sceneFlow.goHome();
    }
#else
    sceneFlow.goHome();
#endif
    battlePhase = BattleViewModel::Phase::ACTION;
    setToast(Ui::Amoled::REST_AT_HOME, nowMs, 1500);
    requestFullRender();
}

void AmoledApp::closeBattle(uint32_t nowMs) {
    (void)nowMs;
    bool returnToDebug = false;
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugBattleActive) {
        returnToDebug = true;
        debugBattleActive = false;
    }
#endif
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    battlePressedItem = 0xFF;
    battlePlayerSlot = 0;
    battleIsBoss = false;
    battleExperiencePercent = 100;
    battleSpecialKind = ExploreSpecial::Kind::NONE;
    battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::OFFER;
    battleFriendshipContactSlot = 0xFF;
    battleVictoryOldLevel = 1;
    battleVictoryLeveledUp = false;
    battleAnimationActive = false;
    battleAnimationAttackerWild = false;
    battleAnimationHit = false;
    battleAnimationDamage = 0;
    battleAnimationFrame = 0;
    battleAudioPending = false;
    battleAudioReady = false;
    battlePendingSfx = 0xFF;
    battlePendingCrySpecies = 0;
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    clearBattleLog();
    battlePhase = BattleViewModel::Phase::ACTION;
    exploreRouteAutoWalk = false;
    exploreRoutePaused = false;
    sceneFlow.enter(returnToDebug ? AppSceneFlow::Scene::DEBUG
                                  : AppSceneFlow::Scene::EXPLORE_ROUTE);
    requestFullRender();
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

void AmoledApp::settleExploreReturn() {
    const uint8_t bondGain = Game::Bond::adventureGain(exploreRouteSteps);
    bool stateChanged = false;
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& monster = gameState.team[slot];
        if (monster.majorStatus != Game::MajorStatus::NONE ||
            monster.majorStatusTurns != 0) {
            monster.majorStatus = Game::MajorStatus::NONE;
            monster.majorStatusTurns = 0;
            stateChanged = true;
        }
        if (bondGain == 0 || monster.origin == Game::Origin::VISITOR ||
            monster.fainted || monster.hpCur == 0) {
            continue;
        }
        Game::Bond::Value nextBond = Game::Bond::increase(
            monster.bond, bondGain);
        if (nextBond == monster.bond) continue;
        monster.bond = nextBond;
        stateChanged = true;
    }
    exploreItemEffects.reset();
    if (stateChanged) saveState();
}

void AmoledApp::leaveExploreRoute() {
    autonomousExpedition = false;
    settleExploreReturn();
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugContactActive && debugContactKind == 3) {
        completeDebugContact(Platform::clock().millis());
    }
#endif
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

void AmoledApp::updateMoodHearts(uint32_t nowMs) {
    const uint8_t nextHeartCount = gameState.teamCount > 0
        ? moodHeartCountFor(gameState.team[0].mood) : 0;
    if (nextHeartCount < moodHeartCount &&
        sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        // Animate the first heart that disappeared. A single burst also
        // handles a larger drop caused by a delayed care tick.
        moodBurstHeart = nextHeartCount;
        moodBurstStartedMs = nowMs;
        moodBurstUntilMs = nowMs + MOOD_BURST_DURATION_MS;
        nextMoodBurstFrameMs = nowMs;
        requestRenderRows(0, HOME_HEADER_HEIGHT);
    }
    moodHeartCount = nextHeartCount;

    if (moodBurstUntilMs == 0) return;
    if (static_cast<int32_t>(nowMs - moodBurstUntilMs) >= 0) {
        moodBurstUntilMs = 0;
        moodBurstHeart = 0xFF;
        requestRenderRows(0, HOME_HEADER_HEIGHT);
        return;
    }
    if (static_cast<int32_t>(nowMs - nextMoodBurstFrameMs) >= 0) {
        nextMoodBurstFrameMs = nowMs + MOOD_BURST_FRAME_MS;
        requestRenderRows(0, HOME_HEADER_HEIGHT);
    }
}

void AmoledApp::updatePet(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME) {
        lastPetUpdateMs = nowMs;
        return;
    }
    if (gameState.teamCount == 0) return;
#if STICKMON_HAS_CLAW
    if (Stickmon::ClawRuntime::instance().autonomyActive()) {
        // Keep care/clock updates running, but do not let MonsterMind start a
        // competing walk or feed while the Agent owns the idle turn.
        lastPetUpdateMs = nowMs;
        return;
    }
#endif
    Game::MonsterRuntime& monster = gameState.team[0];
    if (monster.fainted || monster.hpCur == 0) {
        RoomResource& room = RoomResource::ins();
        const float restX = room.available() ? static_cast<float>(room.bedX())
                                              : 76.0f;
        const float restY = room.available() ? static_cast<float>(room.bedY())
                                              : 99.0f;
        if (!petResting || std::fabs(petX - restX) > 0.01f ||
            std::fabs(petY - restY) > 0.01f) {
            petResting = true;
            petX = petTargetX = restX;
            petY = petTargetY = restY;
            petMotion = PetMotion::IDLE;
            petStopMotion = PetMotion::IDLE;
            petStoppingToEat = false;
            petDirection = PokemonSprites::WalkDirection::DOWN;
            petFrame = 0;
            nextPetFrameMs = nowMs + 700;
            updateCamera();
            Platform::logf("[AmoledPet] faint rest started bed=%.1f,%.1f\n",
                           restX, restY);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        } else if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            PokemonSprites::PetAnimationProfile profile{};
            uint8_t frameCount = 2;
            if (PokemonSprites::petAnimationProfile(monster.speciesId,
                                                     profile)) {
                frameCount = profile.sleepingFrames > 0
                    ? profile.sleepingFrames : 1;
            }
            do {
                petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
                nextPetFrameMs += 700;
            } while (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
        petMotion = PetMotion::IDLE;
        return;
    }

    if (petResting) {
        petResting = false;
        petMotion = PetMotion::IDLE;
        petFrame = 0;
        nextPetFrameMs = nowMs + 520;
        petTargetX = petX;
        petTargetY = petY;
        monsterMind.reset(nowMs);
        schedulePetDecision(nowMs);
        Platform::logLine("[AmoledPet] faint rest complete; waking at bed");
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
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

#if STICKMON_ENABLE_DEBUG_FEATURES
    updateDebugPairChase(nowMs, elapsedSeconds);
    if (debugTiltControl) {
        float ax = 0.0f;
        float ay = 0.0f;
        float az = 0.0f;
        if (Platform::imu().readAcceleration(ax, ay, az)) {
            auto applyDeadzone = [](float value) {
                if (std::fabs(value) < DEBUG_TILT_DEADZONE) return 0.0f;
                return std::clamp(value, -DEBUG_TILT_MAX, DEBUG_TILT_MAX) /
                       DEBUG_TILT_MAX;
            };
            float inputX = applyDeadzone(-ax);
            float inputY = applyDeadzone(ay);
            float length = std::sqrt(inputX * inputX + inputY * inputY);
            if (length > 1.0f) {
                inputX /= length;
                inputY /= length;
            }
            float nextX = petX + inputX * DEBUG_TILT_SPEED * elapsedSeconds;
            float nextY = petY + inputY * DEBUG_TILT_SPEED * 0.75f *
                                  elapsedSeconds;
            if (petFootprintInsideWalkArea(nextX, nextY)) {
                petX = nextX;
                petY = nextY;
                if (std::fabs(inputX) > 0.01f ||
                    std::fabs(inputY) > 0.01f) {
                    petDirection = petDirectionForDelta(inputX, inputY);
                    petMotion = PetMotion::WANDERING;
                    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                        petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
                        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
                    }
                } else {
                    petMotion = PetMotion::IDLE;
                    petFrame = 0;
                }
                petTargetX = petX;
                petTargetY = petY;
                updateCamera();
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            }
        }
        return;
    }
#endif

    if (petMotion == PetMotion::TURNING) {
        if (static_cast<int32_t>(nowMs - petTurnUntilMs) < 0) return;
        petMotion = petStopMotion;
        petFrame = 0;
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

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
                setToast(Ui::Amoled::YUM, nowMs, 800);
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
            petFrame = 0;
            PokemonSprites::PetAnimationProfile profile{};
            if (PokemonSprites::petAnimationProfile(monster.speciesId,
                                                     profile)) {
                nextPetFrameMs = nowMs + profile.idleFrameMs;
            } else {
                nextPetFrameMs = nowMs + 520;
            }
            monsterMind.onAte(nowMs);
            schedulePetDecision(nowMs);
            requestRenderRows(HOME_ROOM_TOP, 224);
        }
        return;
    }

    if (petMotion == PetMotion::STOPPING) {
        PokemonSprites::PetAnimationProfile profile{};
        uint8_t stopFrames = 1;
        if (PokemonSprites::petAnimationProfile(monster.speciesId, profile) &&
            profile.motionMode == PokemonSprites::PetMotionMode::PINGPONG &&
            profile.walkingFrames >= 2) {
            stopFrames = 2;
        }
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) < 0) return;
        if (petFrame + 1 < stopFrames) {
            ++petFrame;
            nextPetFrameMs += MOTION_FRAME_MS;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return;
        }

        petFrame = 0;
        if (petStoppingToEat) {
            petStoppingToEat = false;
            petMotion = PetMotion::EATING;
            nextFeedBiteMs = nowMs + GameRandom::range(700, 1301);
            feedingUntilMs = nowMs + GameRandom::range(3200, 5601);
        } else {
            petMotion = PetMotion::IDLE;
            monsterMind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            nextPetFrameMs = nowMs + profile.idleFrameMs;
        }
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
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
                petFrame = 0;
                nextPetFrameMs = nowMs + 520;
                monsterMind.onActivity(nowMs);
                schedulePetDecision(nowMs);
            } else {
                petX = nextX;
                petY = nextY;
            }
        }
        updateCamera();
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            PokemonSprites::PetAnimationProfile profile{};
            uint8_t frameCount = 3;
            if (PokemonSprites::petAnimationProfile(monster.speciesId, profile)) {
                frameCount = profile.walkingFrames > 0 ? profile.walkingFrames : 1;
                if (profile.motionMode ==
                        PokemonSprites::PetMotionMode::START_HOLD_END &&
                    frameCount >= 3) {
                    frameCount = 2;
                } else if (profile.motionMode ==
                               PokemonSprites::PetMotionMode::PINGPONG &&
                           frameCount == 3 && !petLongMove) {
                    frameCount = 2;
                }
                if (profile.motionMode ==
                        PokemonSprites::PetMotionMode::START_HOLD_END &&
                    profile.walkingFrames >= 3) {
                    petFrame = std::min<uint8_t>(1, petFrame + 1);
                } else {
                    petFrame = static_cast<uint8_t>(
                        (petFrame + 1) % frameCount);
                }
            } else {
                petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
            }
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
        return;
    }

    if (petMotion == PetMotion::IDLE) {
        PokemonSprites::PetAnimationProfile profile{};
        if (PokemonSprites::petAnimationProfile(monster.speciesId, profile) &&
            profile.idleFrames > 1 &&
            static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            do {
                petFrame = static_cast<uint8_t>(
                    (petFrame + 1) % profile.idleFrames);
                nextPetFrameMs += profile.idleFrameMs;
            } while (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
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

    MonsterDesire desire = monsterMind.topDesire();
    if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
        int32_t decisionInMs = static_cast<int32_t>(
            nextPetDecisionMs - nowMs);
        Platform::logf(
            "[AmoledPet] decision desire=%u motion=%u pos=%.1f,%.1f "
            "next=%ld walk=%u\n",
            static_cast<unsigned>(desire),
            static_cast<unsigned>(petMotion), petX, petY,
            static_cast<long>(decisionInMs),
            petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
        nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
    }

    // Stick uses part of its STARE decisions for a small idle turn. Most
    // species have only one idle frame, so this direction change is the
    // visible waiting animation on those species.
    if (desire == MonsterDesire::STARE &&
        behaviorProfile.movementMode == MonsterMovementMode::NORMAL &&
        GameRandom::random(100) < 32) {
        int direction = static_cast<int>(petDirection) +
            (GameRandom::random(2) == 0 ? -1 : 1);
        if (direction < 0) direction += 4;
        if (direction >= 4) direction -= 4;
        petDirection = static_cast<PokemonSprites::WalkDirection>(direction);
        petStopMotion = PetMotion::IDLE;
        petMotion = PetMotion::TURNING;
        petTurnUntilMs = nowMs + behaviorProfile.turnPauseMs;
        petFrame = 0;
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        monsterMind.onActivity(nowMs);
        schedulePetDecision(nowMs);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

    if (desire == MonsterDesire::WANDER &&
        behaviorProfile.movementMode == MonsterMovementMode::NORMAL) {
        float x = petX;
        float y = petY;
        if (chooseWanderTarget(x, y) &&
            beginPetMove(PetMotion::WANDERING, x, y, nowMs)) {
            return;
        }
        // A rejected target is a routing failure, not activity. Retry soon
        // while preserving boredom so the pet cannot silently stall for a
        // full idle interval after every failed sample.
        nextPetDecisionMs = nowMs + GameRandom::range(
            PET_WANDER_RETRY_MIN_MS, PET_WANDER_RETRY_MAX_MS + 1);
        if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
            Platform::logf(
                "[AmoledPet] wander target rejected desire=%u pos=%.1f,%.1f "
                "walk=%u\n",
                static_cast<unsigned>(monsterMind.topDesire()), petX, petY,
                petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
            nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
        }
        return;
    }
    // A decision that elects to stare is not activity. Keep the boredom
    // timer running so WANDER can eventually outrank STARE, as on Stick.
    schedulePetDecision(nowMs);
}

bool AmoledApp::beginPetMove(PetMotion motion, float x, float y,
                             uint32_t nowMs) {
    if (!petFootprintInsideWalkArea(x, y) ||
        !petPathInsideWalkArea(petX, petY, x, y)) {
        return false;
    }
    petTargetX = x;
    petTargetY = y;
    PokemonSprites::WalkDirection previousDirection = petDirection;
    petDirection = petDirectionForDelta(petTargetX - petX,
                                        petTargetY - petY);
    float dx = petTargetX - petX;
    float dy = petTargetY - petY;
    petLongMove = std::sqrt(dx * dx + dy * dy) > 14.0f;
    if (petDirection != previousDirection) {
        petStopMotion = motion;
        petMotion = PetMotion::TURNING;
        petTurnUntilMs = nowMs + TURN_PAUSE_MS;
    } else {
        petMotion = motion;
    }
    petFrame = 0;
    nextPetFrameMs = nowMs;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

void AmoledApp::finishPetMove(uint32_t nowMs) {
    petStoppingToEat = petMotion == PetMotion::SEEKING_FOOD;
    petStopMotion = PetMotion::IDLE;
    petMotion = PetMotion::STOPPING;
    petFrame = 0;
    nextPetFrameMs = nowMs + MOTION_FRAME_MS;
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
    Home::GroundFootprintProfile footprint =
        Home::groundFootprintForSpecies(
            gameState.team[0].speciesId,
            static_cast<uint8_t>(width), static_cast<uint8_t>(height));
    petFootprintRadiusX = footprint.radiusX;
    petFootprintRadiusY = footprint.radiusY;
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

void AmoledApp::markRendered() {
    dirtyRowsValid = false;
    if (battleAnimationActive && battleAudioPending && !battleAudioReady &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        battleAudioReady = true;
    }
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
        model.petFrame = petFrame;
        model.petDirection = petDirection;
        model.petLongMove = petLongMove;
        model.petAction = petMotion == PetMotion::STOPPING
            ? HomeViewModel::PetVisualAction::STOPPING
            : (petMotion == PetMotion::WANDERING ||
               petMotion == PetMotion::SEEKING_FOOD)
                ? HomeViewModel::PetVisualAction::WALKING
                : HomeViewModel::PetVisualAction::IDLE;
        model.petResting = petResting;
        model.night = model.gameMinutesOfDay < 6U * 60U ||
                      model.gameMinutesOfDay >= 18U * 60U;
        model.moodHearts = moodHeartCount;
        if (moodBurstUntilMs != 0) {
            uint32_t nowMs = Platform::clock().millis();
            model.moodBurstHeart = moodBurstHeart;
            model.moodBurstAgeMs = static_cast<uint16_t>(std::min<uint32_t>(
                0xFFFFU, nowMs - moodBurstStartedMs));
        }
        model.showHearts = heartsUntil != 0;
        model.bowlFilled = gameState.room.bowlCount > 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
        model.debugContactPrompt = debugContactPending;
        model.debugContactActive = debugContactActive;
        model.debugContactKind = debugContactKind;
        model.debugContactSpeciesId = debugContactStorageSlot < gameState.storageCount
            ? gameState.storage[debugContactStorageSlot].speciesId : 0;
        model.debugPairChaseActive = debugPairChaseActive &&
            gameState.teamCount > 1;
        model.debugPairSpeciesId = gameState.teamCount > 1
            ? gameState.team[1].speciesId : 0;
        model.debugPairCenterX = static_cast<int16_t>(
            worldToScreenX(debugPairX));
        model.debugPairGroundY = static_cast<int16_t>(
            worldToScreenY(debugPairY));
        model.debugPairFrame = debugPairFrame;
        model.debugPairDirection = debugPairDirection;
        model.debugLightSource = debugLightSource;
        model.debugBoundaryVisible = debugWalkBoundaryVisible;
#endif
        model.toast = toast;
        renderHomeScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
        DebugViewModel model;
        model.category = debugCategory;
        model.cursor = debugCursor;
        model.scroll = debugScroll;
        model.pressedItem = debugPressedItem;
        model.popup = debugPopup;
        model.focus = debugFocus;
        for (uint8_t index = 0; index < 4; ++index) {
            model.digits[index] = debugDigits[index];
        }
        model.state = &gameState;
        model.toast = toast;
        char debugTime[24] = {};
        uint16_t debugMinutes = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        std::snprintf(debugTime, sizeof(debugTime), Ui::Debug::CURRENT_TIME_FMT,
                      debugMinutes / 60, debugMinutes % 60);
        model.currentTime = debugTime;
        model.lightSource = Ui::Debug::LIGHT_SOURCE_ITEMS[debugLightSource];
        model.tiltEnabled = debugTiltControl;
        model.boundaryVisible = debugWalkBoundaryVisible;
        model.battleBoundsVisible = debugBattleDrawBoundsVisible;
        renderDebugScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }
#endif

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        ExploreViewModel model;
        model.visibleAreaCount =
            ExploreItemProgression::visibleAreaCount(gameState);
        model.unlockedArea = ExploreItemProgression::unlockedArea(gameState);
        model.selectedArea = selectedExploreArea;
        model.currentLevel = gameState.team[0].level;
        model.areaAnimCursor = exploreAreaAnimCursor;
        model.previewStartedAt = explorePreviewStartedAt;
        model.previewPool = explorePreviewPool;
        for (uint8_t index = 0; index < ExplorePool::POOL_CAP; ++index) {
            model.previewFrames[index] = explorePreviewFrames[index];
            model.previewHidden[index] = explorePreviewHidden[index];
        }
        model.pressedArea = pressedExploreArea;
        model.toast = toast;
        renderExploreScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
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
        model.sliding = exploreRouteIceSliding;
        model.complete = exploreRouteComplete;
        model.exitConfirm = exploreRouteExitConfirm;
        model.pickupIndex = exploreRoutePickupIndex;
        model.pickupItem = exploreRoutePickupItem;
        model.pickupAvailable = exploreRoutePickupAvailable;
        model.prompt = exploreRoutePrompt;
        renderExploreRouteScreen(
            canvas, model, dirtyRowBegin, dirtyRowEnd);
        if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
            ExploreMenuViewModel menuModel;
            menuModel.cursor = exploreMenuCursor;
            menuModel.pressedItem = pressedExploreMenuItem;
            menuModel.toast = toast;
            renderExploreMenuScreen(
                canvas, menuModel, dirtyRowBegin, dirtyRowEnd);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        BattleViewModel model;
        model.state = &gameState;
        model.battleBackground = battleBackgroundForArea(selectedExploreArea);
        uint8_t activeSlot = battlePlayerSlot < gameState.teamCount
            ? battlePlayerSlot : 0;
        const Game::MonsterRuntime& active = gameState.team[activeSlot];
        model.playerSpeciesId = active.speciesId;
        model.wildSpeciesId = battleWild.speciesId;
        model.playerLevel = active.level;
        model.wildLevel = battleWild.level;
        model.playerHp = active.hpMax == 0 ? 0
            : static_cast<uint8_t>(std::min<uint32_t>(
                100, static_cast<uint32_t>(active.hpCur) * 100 /
                    active.hpMax));
        model.wildHp = battleWild.hpMax == 0 ? 0
            : static_cast<uint8_t>(std::min<uint32_t>(
                100, static_cast<uint32_t>(battleWild.hpCur) * 100 /
                    battleWild.hpMax));
        model.phase = battlePhase;
        model.friendshipPrompt = battleFriendshipPrompt;
        model.pressedItem = battlePressedItem;
        model.activeSlot = activeSlot;
        model.teamCount = std::min<uint8_t>(gameState.teamCount, Game::TEAM_CAP);
        static constexpr Game::ItemId BATTLE_ITEMS[] = {
            Game::ItemId::POTION, Game::ItemId::SUPER_POTION,
            Game::ItemId::MAX_POTION, Game::ItemId::FULL_RESTORE,
            Game::ItemId::FULL_HEAL, Game::ItemId::REVIVE,
            Game::ItemId::ANTIDOTE, Game::ItemId::PARALYZE_HEAL,
            Game::ItemId::AWAKENING, Game::ItemId::BURN_HEAL,
            Game::ItemId::ICE_HEAL,
        };
        model.battleBagCount = 0;
        for (Game::ItemId item : BATTLE_ITEMS) {
            if (Game::ItemInventory::count(gameState, item) == 0) continue;
            if (model.battleBagCount >= 4) break;
            model.battleBagItems[model.battleBagCount++] = item;
        }
        model.animationActive = battleAnimationActive;
        model.animationAttackerWild = battleAnimationAttackerWild;
        model.animationHit = battleAnimationHit;
        model.animationDamage = battleAnimationDamage;
        model.animationFrame = battleAnimationFrame;
#if STICKMON_ENABLE_DEBUG_FEATURES
        model.debugDrawBounds = debugBattleDrawBoundsVisible;
#endif
        model.playerStatus = active.majorStatus;
        model.wildStatus = battleWild.majorStatus;
        model.playerBattleState = battlePlayerState;
        model.wildBattleState = battleWildState;
        model.logCount = battleLogCount;
        for (uint8_t line = 0; line < model.logCount && line < 2; ++line) {
            model.logLines[line] = battleLogLines[line];
        }
        renderBattleScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        renderCommunicationScreen(canvas, visitSession.viewModel(),
                                  dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM) {
        if (teamMovesOpen) {
            TeamMovesViewModel model;
            model.state = &gameState;
            model.teamSlot = teamMovesSlot;
            model.mode = teamMovesMode;
            model.selectedItem = teamMovesMode == TeamMovesViewModel::Mode::MANAGE
                ? (teamMovesForgetConfirm ? teamMovesForgetSlot : 0xFF)
                : teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT
                    ? teamMovesRecallSelected : 0xFF;
            model.recallCount = teamMovesRecallCount;
            for (uint8_t index = 0; index < teamMovesRecallCount; ++index) {
                model.recallIds[index] = teamMovesRecallIds[index];
            }
            model.recallSelected = teamMovesRecallSelected;
            model.forgetSlot = teamMovesForgetSlot;
            model.forgetConfirmOpen = teamMovesForgetConfirm;
            model.toast = toast;
            renderTeamMovesScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
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

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD) {
        RoomFoodViewModel model;
        model.state = &gameState;
        model.selectedFood = gameState.room.selectedFood;
        model.pressedItem = pressedRoomItem;
        model.toast = toast;
        renderRoomFoodScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
        ComputerViewModel model;
        model.state = &gameState;
        model.page = computerPage;
        model.storageScroll = computerScroll;
        model.pressedItem = computerPressedItem == 0xFF
            ? -1 : computerPressedItem;
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime& clawRuntime =
            Stickmon::ClawRuntime::instance();
        model.clawEnabled = clawRuntime.enabled();
        model.wifiEnabled = clawRuntime.wifiEnabled();
        char clawSsid[33] = {};
        char clawPassword[65] = {};
        char clawIp[16] = {};
        if (computerPage == ComputerViewModel::Page::CLAW_SETUP) {
            clawRuntime.setupPortalInfo(
                clawSsid, sizeof(clawSsid), clawPassword,
                sizeof(clawPassword), clawIp, sizeof(clawIp));
            model.clawSsid = clawSsid;
            model.clawPassword = clawPassword;
            model.clawIp = clawIp;
            model.clawLogView = clawLogView;
            Stickmon::ClawStatus status;
            clawRuntime.statusSnapshot(status);
            model.clawStaConnected = status.staConnected;
            std::memcpy(model.clawStaIp, status.staIp,
                        sizeof(model.clawStaIp));
            model.clawPhoneJoined = status.phoneJoined;
            model.clawStarted = status.clawStarted;
            std::memcpy(model.clawWechatPhase, status.wechatPhase,
                        sizeof(model.clawWechatPhase));
            model.clawWechatPersisted = status.wechatPersisted;
            // Entries were copied by refreshClawLogSnapshot() (update path);
            // rendering stays read-only.
            model.clawLog = s_clawLogEntries;
            model.clawLogCount = clawLogCount;
            model.clawLogScroll = clawLogScroll;
            model.clawLogPinned = clawLogPinned;
        }
#endif
        model.toast = toast;
        renderComputerScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS) {
        SettingsViewModel model;
        model.state = &gameState;
        model.brightness = gameState.settings.brightness;
        model.volume = gameState.settings.volume;
        model.pressedItem = settingsPressedItem;
        model.toast = toast;
        renderSettingsScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
        ProgressionViewModel model;
        model.state = &gameState;
        model.mode = progressionMode;
        model.teamSlot = progressionTeamSlot;
        model.level = progressionLevel;
        model.fromSpeciesId = progressionFromSpeciesId;
        model.toSpeciesId = progressionToSpeciesId;
        model.moveId = progressionMoveId;
        model.oldMove2 = progressionOldMove2;
        model.oldMove3 = progressionOldMove3;
        model.pressedItem = progressionPressedItem;
        model.toast = toast;
        renderProgressionScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
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

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        ShopViewModel model;
        model.state = &gameState;
        model.mode = shopCategory == Game::ShopService::Category::SELL
            ? ShopViewModel::Mode::SELL : ShopViewModel::Mode::BUY;
        model.scroll = itemScroll;
        model.dailyItemCount = shopDailyItemCount();
        model.exploreItemCount = shopExploreItemCount();
        model.itemCount = currentItemCount();
        model.coins = gameState.coins;
        model.pressedMenuItem = pressedShopCategory;
        model.pressedItem = pressedItemRow;
        model.pressedDetailAction = pressedShopDetailAction;
        model.detailItem = pendingItem;
        model.detailItemIndex = shopDetailItemIndex;
        model.detailProgress = shopDetailProgress;
        model.toast = toast;
        renderShopScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        ItemListViewModel model;
        model.state = &gameState;
        model.mode = ItemListMode::BAG;
        model.category = shopCategory;
        model.scroll = itemScroll;
        model.itemCount = currentItemCount();
        model.coins = gameState.coins;
        model.pressedItem = pressedItemRow;
        model.confirmOpen = itemConfirmOpen;
        model.pendingItem = pendingItem;
        model.toast = toast;
        renderItemListScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
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

void AmoledApp::onWake(uint32_t nowMs) {
    lockRequested = false;
    pointerDown = false;
    lastInteractionMs = nowMs;
    requestFullRender();
}

bool AmoledApp::lockFocusPoint(int16_t& x, int16_t& y) const {
    x = 92;
    y = 112;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME &&
        gameState.teamCount > 0) {
        x = static_cast<int16_t>(worldToScreenX(petX));
        y = static_cast<int16_t>(worldToScreenY(petY) - 32);
        return true;
    }
    return false;
}

void AmoledApp::setSettingsSliderValue(uint8_t item, int x,
                                       uint32_t nowMs) {
    (void)nowMs;
    if (item > 1) return;

    const int clampedX = std::clamp(
        x, SETTINGS_SLIDER_LEFT, SETTINGS_SLIDER_RIGHT);
    const int trackWidth = SETTINGS_SLIDER_RIGHT - SETTINGS_SLIDER_LEFT;
    const int minimum = item == 0 ? 32 : 0;
    const int maximum = item == 0 ? 255 : 100;
    const int value = minimum +
        (clampedX - SETTINGS_SLIDER_LEFT) * (maximum - minimum) /
            std::max(1, trackWidth);

    if (item == 0) {
        uint8_t brightness = static_cast<uint8_t>(value);
        if (gameState.settings.brightness == brightness) return;
        gameState.settings.brightness = brightness;
        Platform::display().setBrightness(brightness);
    } else {
        uint8_t volume = static_cast<uint8_t>(value);
        if (gameState.settings.volume == volume) return;
        gameState.settings.volume = volume;
        Platform::audio().setVolume(volume);
    }
    settingsSliderChanged = true;
}

void AmoledApp::setToast(const char* value, uint32_t nowMs,
                         uint32_t durationMs) {
    toast = value;
    toastUntil = nowMs + durationMs;
    requestRenderRows(
        sceneFlow.current() == AppSceneFlow::Scene::HOME
            ? HOME_ROOM_TOP
            : sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU
                ? MAIN_MENU_CONTENT_TOP : MENU_HEADER_HEIGHT,
        sceneFlow.current() == AppSceneFlow::Scene::HOME
            ? HOME_STATUS_TOP : 224);
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void AmoledApp::clampDebugScroll() {
    float maximum = debugMaxScroll(debugCategory);
    if (debugScroll <= 0.0f) {
        debugScroll = 0.0f;
        if (debugVelocity < 0.0f) debugVelocity = 0.0f;
    } else if (debugScroll >= maximum) {
        debugScroll = maximum;
        if (debugVelocity > 0.0f) debugVelocity = 0.0f;
    }
}

void AmoledApp::acceptDebugContact(uint32_t nowMs) {
    if (!debugContactPending ||
        debugContactStorageSlot >= gameState.storageCount ||
        gameState.teamCount != 1) {
        debugContactPending = false;
        debugContactStorageSlot = 0xFF;
        debugContactKind = 0;
        return;
    }

    const uint8_t storageSlot = debugContactStorageSlot;
    Game::MonsterRuntime guest = gameState.storage[storageSlot];
    guest.origin = Game::Origin::VISITOR;
    guest.petCountToday = 0;
    gameState.team[1] = guest;
    gameState.teamCount = 2;
    gameState.activeSlot = 0;
    debugContactPending = false;
    debugContactActive = true;
    debugContactStartedMs = nowMs;

    if (debugContactKind == 1) {
        Game::MonsterRuntime& original = gameState.storage[storageSlot];
        original.bond = Game::Bond::increase(original.bond, 2);
        gameState.team[1].bond = original.bond;
    } else if (debugContactKind == 2) {
        uint8_t& food = gameState.room.food[Game::ROOM_NORMAL_FOOD_INDEX];
        if (food < Game::ITEM_STACK_CAP) ++food;
    }

    uint16_t speciesIds[Game::TEAM_CAP] = {};
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    saveState();

    const Species* species = findSpecies(gameState.team[1].speciesId);
    const char* name = species && species->name ? species->name : "";
    const char* format = debugContactKind == 1
        ? Ui::ContactVisit::PLAY_FMT
        : debugContactKind == 2
            ? Ui::ContactVisit::GIFT_FMT : Ui::ContactVisit::EXPLORE_FMT;
    std::snprintf(debugToastBuffer, sizeof(debugToastBuffer), format, name);
    if (debugContactKind == 3) {
        uint8_t unlocked = ExploreItemProgression::unlockedArea(gameState);
        selectedExploreArea = std::min<uint8_t>(selectedExploreArea, unlocked);
        if (!queueExploreDeparture(selectedExploreArea, false)) {
            completeDebugContact(nowMs);
            setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            return;
        }
    }
    setToast(debugToastBuffer, nowMs, 1400);
    requestFullRender();
}

void AmoledApp::completeDebugContact(uint32_t nowMs) {
    if (!debugContactActive) return;

    uint8_t visitorSlot = 0xFF;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (gameState.team[slot].origin == Game::Origin::VISITOR) {
            visitorSlot = slot;
            break;
        }
    }
    if (debugContactStorageSlot < gameState.storageCount &&
        visitorSlot < gameState.teamCount) {
        Game::Origin originalOrigin =
            gameState.storage[debugContactStorageSlot].origin;
        uint8_t inviteMarker =
            gameState.storage[debugContactStorageSlot].petCountToday;
        gameState.storage[debugContactStorageSlot] = gameState.team[visitorSlot];
        gameState.storage[debugContactStorageSlot].origin = originalOrigin;
        gameState.storage[debugContactStorageSlot].petCountToday = inviteMarker;
        gameState.storage[debugContactStorageSlot].lastSeenAt =
            static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    }
    if (visitorSlot < gameState.teamCount) {
        for (uint8_t slot = visitorSlot; slot + 1 < gameState.teamCount &&
             slot + 1 < Game::TEAM_CAP; ++slot) {
            gameState.team[slot] = gameState.team[slot + 1];
        }
        --gameState.teamCount;
        gameState.team[gameState.teamCount] = Game::MonsterRuntime{};
        gameState.activeSlot = 0;
    }

    uint8_t kind = debugContactKind;
    debugContactActive = false;
    debugContactStorageSlot = 0xFF;
    debugContactKind = 0;
    debugContactStartedMs = 0;
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    saveState();
    setToast(kind == 3 ? Ui::ContactVisit::HAPPY_RETURN
                       : Ui::ContactVisit::HAPPY_VISIT,
             nowMs, 1400);
    requestFullRender();
}

void AmoledApp::startDebugPairChase(uint32_t nowMs) {
    if (gameState.teamCount < 2 || gameState.team[0].fainted ||
        gameState.team[1].fainted || gameState.team[0].hpCur == 0 ||
        gameState.team[1].hpCur == 0) {
        setToast(Ui::Debug::PAIR_NEEDS_TWO, nowMs, 1400);
        return;
    }
    debugPairX = petX - DEBUG_PAIR_FOLLOW_DISTANCE;
    debugPairY = petY;
    debugPairDirection = PokemonSprites::WalkDirection::RIGHT;
    debugPairFrame = 0;
    debugPairNextFrameMs = nowMs;
    debugPairChaseUntilMs = nowMs + DEBUG_PAIR_CHASE_DURATION_MS;
    debugPairChaseActive = true;
    float targetX = petX;
    float targetY = petY;
    if (chooseWanderTarget(targetX, targetY)) {
        beginPetMove(PetMotion::WANDERING, targetX, targetY, nowMs);
    }
    sceneFlow.goHome();
    setToast(Ui::Debug::PAIR_INTERACTION, nowMs, 1000);
    requestFullRender();
}

void AmoledApp::updateDebugPairChase(uint32_t nowMs,
                                     float elapsedSeconds) {
    if (!debugPairChaseActive) return;
    if (gameState.teamCount < 2 ||
        static_cast<int32_t>(nowMs - debugPairChaseUntilMs) >= 0) {
        stopDebugPairChase(nowMs, gameState.teamCount >= 2);
        return;
    }

    float dx = petX - debugPairX;
    float dy = petY - debugPairY;
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance > DEBUG_PAIR_FOLLOW_DISTANCE && distance > 0.0f) {
        float step = std::min(
            distance - DEBUG_PAIR_FOLLOW_DISTANCE,
            DEBUG_PAIR_FOLLOW_SPEED * elapsedSeconds);
        debugPairX += dx / distance * step;
        debugPairY += dy / distance * step;
        debugPairDirection = petDirectionForDelta(dx, dy);
        if (static_cast<int32_t>(nowMs - debugPairNextFrameMs) >= 0) {
            debugPairFrame = static_cast<uint8_t>((debugPairFrame + 1) % 3);
            debugPairNextFrameMs = nowMs + MOTION_FRAME_MS;
        }
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }

    if (petMotion == PetMotion::IDLE) {
        float targetX = petX;
        float targetY = petY;
        if (chooseWanderTarget(targetX, targetY)) {
            beginPetMove(PetMotion::WANDERING, targetX, targetY, nowMs);
        }
    }
}

void AmoledApp::stopDebugPairChase(uint32_t nowMs, bool reward) {
    if (!debugPairChaseActive) return;
    debugPairChaseActive = false;
    debugPairChaseUntilMs = 0;
    if (reward && gameState.pairMoodRewardsToday < 3) {
        for (uint8_t slot = 0; slot < gameState.teamCount &&
             slot < Game::TEAM_CAP; ++slot) {
            Game::MonsterRuntime& monster = gameState.team[slot];
            if (!monster.fainted && monster.hpCur > 0 && monster.mood < 100) {
                ++monster.mood;
            }
        }
        ++gameState.pairMoodRewardsToday;
        saveState();
    }
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::openDebugSwitchPopup() {
    uint16_t speciesId = gameState.teamCount > 0
        ? gameState.team[0].speciesId : 1;
    speciesId = std::min<uint16_t>(speciesId, 999);
    debugDigits[0] = static_cast<uint8_t>((speciesId / 100) % 10);
    debugDigits[1] = static_cast<uint8_t>((speciesId / 10) % 10);
    debugDigits[2] = static_cast<uint8_t>(speciesId % 10);
    debugFocus = 0;
    debugPopup = DebugViewModel::Popup::SWITCH_MONSTER;
    toast = nullptr;
    requestRenderRows(0, 224);
}

void AmoledApp::openDebugTimePopup() {
    uint16_t minutes = static_cast<uint16_t>(
        gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
    uint8_t hour = static_cast<uint8_t>(minutes / 60);
    uint8_t minute = static_cast<uint8_t>(minutes % 60);
    debugDigits[0] = hour / 10;
    debugDigits[1] = hour % 10;
    debugDigits[2] = minute / 10;
    debugDigits[3] = minute % 10;
    debugFocus = 0;
    debugPopup = DebugViewModel::Popup::SET_TIME;
    toast = nullptr;
    requestRenderRows(0, 224);
}

void AmoledApp::handleDebugPopupTap(int x, int y, uint32_t nowMs) {
    uint8_t digitCount = debugPopup == DebugViewModel::Popup::SET_TIME ? 4 : 3;
    int digit = debugPopupDigitAt(x, y, digitCount);
    if (digit >= 0) {
        debugFocus = static_cast<uint8_t>(digit);
        debugDigits[debugFocus] = static_cast<uint8_t>(
            (debugDigits[debugFocus] + 1) % 10);
        requestRenderRows(0, 224);
        return;
    }
    int choice = debugPopupChoiceAt(x, y);
    if (choice < 0) return;
    if (choice == 1) {
        debugPopup = DebugViewModel::Popup::NONE;
        requestRenderRows(0, 224);
        return;
    }

    if (debugPopup == DebugViewModel::Popup::SWITCH_MONSTER) {
        uint16_t speciesId = static_cast<uint16_t>(
            debugDigits[0] * 100 + debugDigits[1] * 10 + debugDigits[2]);
        if (const Species* species = findSpecies(speciesId)) {
            uint8_t level = gameState.teamCount > 0 ? gameState.team[0].level : 5;
            Game::MonsterRuntime monster = Game::MonsterFactory::create(
                species->id, level);
            if (gameState.teamCount == 0) gameState.teamCount = 1;
            gameState.team[0] = monster;
            gameState.activeSlot = 0;
            debugPopup = DebugViewModel::Popup::NONE;
            petFrame = 0;
            nextPetDecisionMs = nowMs;
            PokemonSprites::syncTeamCache(&speciesId, 1);
            saveState();
            setToast(Ui::Debug::SWITCHED, nowMs);
        } else {
            setToast(Ui::Debug::INVALID_ID, nowMs);
        }
    } else {
        uint8_t hour = static_cast<uint8_t>(debugDigits[0] * 10 + debugDigits[1]);
        uint8_t minute = static_cast<uint8_t>(debugDigits[2] * 10 + debugDigits[3]);
        if (hour > 23) hour = 23;
        if (minute > 59) minute = 59;
        uint16_t current = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        uint16_t target = static_cast<uint16_t>(hour * 60 + minute);
        uint32_t delta = target >= current
            ? target - current
            : Game::GAME_MINUTES_PER_DAY - current + target;
        gameState.gameMinutesTotal += delta;
        gameClock.set(nowMs, gameState.gameMinutesTotal);
        saveState();
        debugPopup = DebugViewModel::Popup::NONE;
        std::snprintf(debugToastBuffer, sizeof(debugToastBuffer), "%s +%lum",
                      Ui::Debug::TIME_SET,
                      static_cast<unsigned long>(delta));
        setToast(debugToastBuffer, nowMs);
    }
    requestRenderRows(0, 224);
}

void AmoledApp::executeDebugAction(uint32_t nowMs) {
    if (debugCategory == DebugViewModel::Category::ROOT) {
        switch (debugCursor) {
        case 0: debugCategory = DebugViewModel::Category::MONSTER; break;
        case 1: debugCategory = DebugViewModel::Category::RESOURCE; break;
        case 2: debugCategory = DebugViewModel::Category::ENV; break;
        case 3: debugCategory = DebugViewModel::Category::MOTION; break;
        case 4: debugCategory = DebugViewModel::Category::BATTLE; break;
        case 5: debugCategory = DebugViewModel::Category::CONTACT_EVENT; break;
        default:
            sceneFlow.closeMenu();
            requestFullRender();
            return;
        }
        debugCursor = 0;
        debugScroll = 0.0f;
        requestFullRender();
        return;
    }

    const uint8_t backIndex = debugCategory == DebugViewModel::Category::MONSTER ? 3
        : debugCategory == DebugViewModel::Category::RESOURCE ? 1
        : debugCategory == DebugViewModel::Category::ENV ? 2
        : debugCategory == DebugViewModel::Category::MOTION ? 3
        : debugCategory == DebugViewModel::Category::BATTLE ? 2 : 3;
    if (debugCursor == backIndex) {
        debugCategory = DebugViewModel::Category::ROOT;
        debugCursor = 0;
        debugScroll = 0.0f;
        debugVelocity = 0.0f;
        requestFullRender();
        return;
    }

    switch (debugCategory) {
    case DebugViewModel::Category::MONSTER:
        if (debugCursor == 0) {
            if (gameState.teamCount == 0) setToast(Ui::Menu::HATCH_FIRST, nowMs);
            else {
                for (uint8_t slot = 0; slot < gameState.teamCount &&
                     slot < Game::TEAM_CAP; ++slot) {
                    Game::MonsterRuntime& monster = gameState.team[slot];
                    monster.fainted = false;
                    monster.majorStatus = Game::MajorStatus::NONE;
                    monster.majorStatusTurns = 0;
                    monster.hpCur = monster.hpMax;
                    monster.satiety = 100;
                    monster.mood = 100;
                }
                saveState();
                setToast(Ui::Debug::RECOVERED, nowMs);
            }
        } else if (debugCursor == 1) {
            if (gameState.teamCount == 0) {
                setToast(Ui::Menu::HATCH_FIRST, nowMs);
            } else if (gameState.team[0].level < Game::LEVEL_MAX) {
                const Species* species = findSpecies(gameState.team[0].speciesId);
                const uint8_t oldLevel = gameState.team[0].level;
                bool leveledUp = false;
                if (species) {
                    uint32_t targetExp = minimumExpForLevel(
                        species->growthRate,
                        static_cast<uint8_t>(oldLevel + 1));
                    uint32_t required = targetExp > gameState.team[0].exp
                        ? targetExp - gameState.team[0].exp : 1;
                    Game::ExperienceService::Result experience =
                        Game::ExperienceService::add(
                            gameState.team[0], *species, required);
                    leveledUp = experience.leveledUp;
                    if (leveledUp) {
                        gameState.pendingLevelUp = true;
                        gameState.pendingLevelUpLevel =
                            gameState.team[0].level;
                    }
                }
                saveState();
                std::snprintf(debugToastBuffer, sizeof(debugToastBuffer),
                              Ui::Debug::LEVEL_UP_FMT,
                              gameState.team[0].level);
                setToast(debugToastBuffer, nowMs);
                if (leveledUp) {
                    openProgressionScene(AppSceneFlow::Scene::DEBUG, 0,
                                         oldLevel, nowMs);
                    return;
                }
            } else {
                setToast(Ui::Debug::LEVEL_MAX, nowMs);
            }
        } else if (debugCursor == 2) {
            openDebugSwitchPopup();
        }
        break;
    case DebugViewModel::Category::RESOURCE:
        gameState.coins += 1000;
        saveState();
        setToast(Ui::Debug::COINS_ADDED, nowMs);
        break;
    case DebugViewModel::Category::ENV:
        if (debugCursor == 0) openDebugTimePopup();
        else debugLightSource = static_cast<uint8_t>((debugLightSource + 1) % 6);
        break;
    case DebugViewModel::Category::MOTION:
        if (debugCursor == 0) {
            if (debugTiltControl) {
                debugTiltControl = false;
            } else {
                float ax = 0.0f;
                float ay = 0.0f;
                float az = 0.0f;
                if (Platform::imu().readAcceleration(ax, ay, az)) {
                    debugTiltControl = true;
                    stopDebugPairChase(nowMs, false);
                    sceneFlow.goHome();
                    requestFullRender();
                    return;
                }
                setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            }
        }
        else if (debugCursor == 1) debugWalkBoundaryVisible = !debugWalkBoundaryVisible;
        else if (debugCursor == 2) {
            if (!gameState.oobeDone || gameState.teamCount < 2) {
                setToast(Ui::Debug::PAIR_NEEDS_TWO, nowMs, 1400);
            } else {
                startDebugPairChase(nowMs);
                return;
            }
        }
        break;
    case DebugViewModel::Category::BATTLE:
        if (debugCursor == 0) {
            if (!gameState.oobeDone || gameState.teamCount == 0) {
                setToast(Ui::Menu::HATCH_FIRST, nowMs);
            } else if (gameState.team[0].fainted || gameState.team[0].hpCur == 0) {
                setToast(Ui::Menu::FAINTED_TOAST, nowMs);
            } else {
                debugBattleRequested = true;
            }
        } else if (debugCursor == 1) {
            debugBattleDrawBoundsVisible = !debugBattleDrawBoundsVisible;
        }
        break;
    case DebugViewModel::Category::CONTACT_EVENT: {
        if (gameState.teamCount != 1) {
            setToast(Ui::Social::HOST_TEAM_REQUIRED, nowMs, 1400);
            break;
        }
        if (debugContactPending || debugContactActive) {
            setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            break;
        }
        uint8_t selected = 0xFF;
        for (uint8_t slot = 0; slot < gameState.storageCount &&
             slot < Game::STORAGE_CAP; ++slot) {
            const Game::MonsterRuntime& contact = gameState.storage[slot];
            bool representedByTeam = false;
            for (uint8_t teamSlot = 0; teamSlot < gameState.teamCount &&
                 teamSlot < Game::TEAM_CAP; ++teamSlot) {
                if (ContactRoster::sameMonster(
                        contact, gameState.team[teamSlot])) {
                    representedByTeam = true;
                    break;
                }
            }
            if (representedByTeam || contact.fainted || contact.hpCur == 0 ||
                contact.origin == Game::Origin::VISITOR) {
                continue;
            }
            if (selected == 0xFF ||
                contact.bond > gameState.storage[selected].bond) {
                selected = slot;
            }
        }
        if (selected == 0xFF) {
            setToast(Ui::Debug::NO_CONTACT, nowMs, 1400);
            break;
        }
        debugContactStorageSlot = selected;
        debugContactKind = static_cast<uint8_t>(debugCursor + 1);
        debugContactPending = true;
        gameState.storage[selected].lastSeenAt =
            static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
        saveState();
        sceneFlow.enter(AppSceneFlow::Scene::HOME);
        setToast(Ui::ContactVisit::KNOCK, nowMs, 1400);
        requestFullRender();
        return;
    }
    case DebugViewModel::Category::ROOT:
    default: break;
    }
    requestRenderRows(0, 224);
}

void AmoledApp::handleDebugTap(int x, int y, uint32_t nowMs) {
    if (debugPopup != DebugViewModel::Popup::NONE) {
        handleDebugPopupTap(x, y, nowMs);
        return;
    }
    if (debugBackAt(x, y)) {
        if (debugCategory == DebugViewModel::Category::ROOT) {
            // DEBUG is opened from the main menu with enter(), so closeMenu()
            // cannot pop it. Return explicitly to the menu scene.
            sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);
        } else {
            debugCategory = DebugViewModel::Category::ROOT;
            debugCursor = 0;
            debugScroll = 0.0f;
        }
        requestFullRender();
        return;
    }
    int item = debugItemAt(x, y, debugCategory, debugScroll);
    if (item < 0) return;
    debugCursor = static_cast<uint8_t>(item);
    executeDebugAction(nowMs);
}
#endif

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

void AmoledApp::clampItemScroll() {
    float maximum = itemListMaxScroll(currentItemCount());
    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        ShopViewModel::Mode mode =
            shopCategory == Game::ShopService::Category::SELL
                ? ShopViewModel::Mode::SELL : ShopViewModel::Mode::BUY;
        maximum = shopGridMaxScroll(
            mode, shopDailyItemCount(), shopExploreItemCount(),
            currentItemCount());
    }
    if (itemScroll <= 0.0f) {
        itemScroll = 0.0f;
        if (itemVelocity < 0.0f) itemVelocity = 0.0f;
    } else if (itemScroll >= maximum) {
        itemScroll = maximum;
        if (itemVelocity > 0.0f) itemVelocity = 0.0f;
    }
}

uint8_t AmoledApp::shopDailyItemCount() const {
    return Game::ShopService::buyItemCount(
        Game::ShopService::Category::DAILY, gameState);
}

uint8_t AmoledApp::shopExploreItemCount() const {
    return Game::ShopService::buyItemCount(
        Game::ShopService::Category::EXPLORE, gameState);
}

uint8_t AmoledApp::currentItemCount() const {
    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        return Game::ItemInventory::homeBagItemCount(gameState);
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::SHOP) {
        return 0;
    }
    return shopCategory == Game::ShopService::Category::SELL
        ? Game::ShopService::sellItemCount(gameState)
        : static_cast<uint8_t>(shopDailyItemCount() +
                               shopExploreItemCount());
}

Game::ItemId AmoledApp::currentItemAt(uint8_t index) const {
    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        return Game::ItemInventory::homeBagItemAt(gameState, index);
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::SHOP) {
        return Game::ItemId::COUNT;
    }
    if (shopCategory == Game::ShopService::Category::SELL) {
        return Game::ShopService::sellItemAt(gameState, index);
    }
    uint8_t dailyCount = shopDailyItemCount();
    return index < dailyCount
        ? Game::ShopService::buyItemAt(
              Game::ShopService::Category::DAILY, gameState, index)
        : Game::ShopService::buyItemAt(
              Game::ShopService::Category::EXPLORE, gameState,
              static_cast<uint8_t>(index - dailyCount));
}

void AmoledApp::openItemScene(AppSceneFlow::Scene target) {
    sceneFlow.openSubScene(target);
    itemScroll = 0.0f;
    itemVelocity = 0.0f;
    pressedItemRow = -1;
    pressedShopCategory = -1;
    pressedShopDetailAction = -1;
    shopCategory = Game::ShopService::Category::DAILY;
    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    shopDetailProgress = 0.0f;
    shopDetailItemIndex = -1;
    teamConfirmOpen = false;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::closeItemScene() {
    itemVelocity = 0.0f;
    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    pressedShopDetailAction = -1;
    shopDetailProgress = 0.0f;
    shopDetailItemIndex = -1;
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

void AmoledApp::openRoomFoodScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::ROOM_FOOD);
    pressedRoomItem = -1;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::openComputerScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::COMPUTER);
    computerPage = ComputerViewModel::Page::MENU;
    computerScroll = 0.0f;
    computerVelocity = 0.0f;
    computerPressedItem = 0xFF;
    clawLogView = false;
    clawLogScroll = 0.0f;
    clawLogVelocity = 0.0f;
    clawLogPinned = true;
    clawLogCount = 0;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::openSettingsScene() {
    sceneFlow.openSubScene(AppSceneFlow::Scene::SETTINGS);
    settingsPressedItem = 0xFF;
    Platform::display().setBrightness(gameState.settings.brightness);
    Platform::audio().setVolume(gameState.settings.volume);
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::closeUtilityScene() {
    computerScroll = 0.0f;
    computerVelocity = 0.0f;
    computerPressedItem = 0xFF;
    clawLogView = false;
    clawLogScroll = 0.0f;
    clawLogVelocity = 0.0f;
    clawLogPinned = true;
    clawLogCount = 0;
    settingsPressedItem = 0xFF;
    pressedRoomItem = -1;
    toast = nullptr;
    sceneFlow.closeSubScene();
    requestFullRender();
}

void AmoledApp::clampComputerScroll() {
    int contentHeight = static_cast<int>(gameState.storageCount) * 43;
    float maximum = static_cast<float>(std::max(
        0, contentHeight - (224 - MENU_HEADER_HEIGHT)));
    if (computerScroll <= 0.0f) {
        computerScroll = 0.0f;
        if (computerVelocity < 0.0f) computerVelocity = 0.0f;
    } else if (computerScroll >= maximum) {
        computerScroll = maximum;
        if (computerVelocity > 0.0f) computerVelocity = 0.0f;
    }
}

void AmoledApp::clampClawLogScroll() {
    const float maximum = std::max(
        0.0f, static_cast<float>(clawLogCount) * CLAW_LOG_ROW_HEIGHT -
                  CLAW_LOG_VIEWPORT);
    if (clawLogPinned) {
        // Pinned follows the tail: always show the newest entries.
        clawLogScroll = maximum;
        clawLogVelocity = 0.0f;
        return;
    }
    if (clawLogScroll <= 0.0f) {
        clawLogScroll = 0.0f;
        if (clawLogVelocity < 0.0f) clawLogVelocity = 0.0f;
    } else if (clawLogScroll >= maximum) {
        // Scrolling back to the bottom re-engages tail-follow.
        clawLogScroll = maximum;
        clawLogPinned = true;
        if (clawLogVelocity > 0.0f) clawLogVelocity = 0.0f;
    }
}

#if STICKMON_HAS_CLAW
void AmoledApp::refreshClawLogSnapshot() {
    clawLogCount = Stickmon::ClawRuntime::instance().copyLog(
        s_clawLogEntries, Stickmon::ClawStatusLog::CAPACITY);
    clampClawLogScroll();
}
#endif

void AmoledApp::openProgressionScene(AppSceneFlow::Scene returnScene,
                                     uint8_t teamSlot, uint8_t oldLevel,
                                     uint32_t nowMs) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& monster = gameState.team[teamSlot];
    if (monster.level <= oldLevel) return;

    progressionReturnScene = returnScene;
    progressionTeamSlot = teamSlot;
    progressionOldLevel = oldLevel;
    progressionLevel = monster.level;
    progressionFromSpeciesId = monster.speciesId;
    progressionToSpeciesId = 0;
    progressionMoveId = 0;
    progressionOldMove2 = 0;
    progressionOldMove3 = 0;
    progressionMoveCursor = 0;
    progressionPressedItem = 0xFF;
    progressionMode = ProgressionViewModel::Mode::LEVEL_UP;
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
    requestFullRender();
    (void)nowMs;
}

void AmoledApp::openEvolutionProgression(AppSceneFlow::Scene returnScene,
                                          uint8_t teamSlot,
                                          uint16_t fromSpeciesId,
                                          uint16_t toSpeciesId,
                                          uint32_t nowMs) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
        fromSpeciesId == 0 || toSpeciesId == 0 ||
        fromSpeciesId == toSpeciesId) return;
    progressionReturnScene = returnScene;
    progressionTeamSlot = teamSlot;
    progressionOldLevel = gameState.team[teamSlot].level;
    progressionLevel = gameState.team[teamSlot].level;
    progressionFromSpeciesId = fromSpeciesId;
    progressionToSpeciesId = toSpeciesId;
    progressionMoveId = 0;
    progressionOldMove2 = 0;
    progressionOldMove3 = 0;
    progressionMoveCursor = 0;
    progressionPressedItem = 0xFF;
    progressionMode = ProgressionViewModel::Mode::EVOLUTION;
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
    requestFullRender();
    (void)nowMs;
}

bool AmoledApp::findNextProgressionMove(uint8_t teamSlot,
                                        uint8_t oldLevel,
                                        uint16_t& cursor,
                                        Game::MoveId& moveId) const {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    const Game::MonsterRuntime& monster = gameState.team[teamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;

    uint16_t count = learnsetEntryCountForSpecies(*species);
    while (cursor < count) {
        uint16_t index = cursor++;
        const LearnsetEntry* entry = learnsetEntryForSpecies(*species, index);
        if (!entry) continue;
        if (entry->level > monster.level) break;
        if (entry->level <= oldLevel ||
            !canLearnAsSpecialMove(*species, entry->moveId) ||
            entry->moveId == monster.move1Id ||
            entry->moveId == monster.move2Id ||
            entry->moveId == monster.move3Id) {
            continue;
        }
        moveId = entry->moveId;
        return true;
    }
    return false;
}

void AmoledApp::advanceProgression(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::PROGRESSION ||
        progressionTeamSlot >= gameState.teamCount ||
        progressionTeamSlot >= Game::TEAM_CAP) {
        return;
    }

    Game::MonsterRuntime& monster = gameState.team[progressionTeamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) {
        completeProgression(nowMs);
        return;
    }

    if (progressionMode == ProgressionViewModel::Mode::LEVEL_UP) {
        gameState.pendingLevelUp = false;
        gameState.pendingLevelUpLevel = 0;
        const Species* target = levelUpEvolutionTarget(*species, monster);
        if (target) {
            progressionFromSpeciesId = monster.speciesId;
            progressionToSpeciesId = target->id;
            progressionMode = ProgressionViewModel::Mode::EVOLUTION;
            progressionPressedItem = 0xFF;
            saveState();
            requestFullRender();
            return;
        }
        progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
        progressionMoveCursor = 0;
    } else if (progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
        const Species* target = findSpecies(progressionToSpeciesId);
        if (target && target->id != monster.speciesId) {
            uint16_t oldHpMax = monster.hpMax;
            monster.speciesId = target->id;
            monster.hpMax = maxHpFor(*target, monster);
            if (monster.hpMax > oldHpMax) {
                monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
                    monster.hpMax,
                    static_cast<uint32_t>(monster.hpCur) +
                        monster.hpMax - oldHpMax));
            } else {
                monster.hpCur = std::min(monster.hpCur, monster.hpMax);
            }
            if (!canRetainSpecialMove(*target, monster.move2Id,
                                       monster.level)) {
                monster.move2Id = 0;
                monster.moveProficiency[1] = 0;
            }
            if (!canRetainSpecialMove(*target, monster.move3Id,
                                       monster.level)) {
                monster.move3Id = 0;
                monster.moveProficiency[2] = 0;
            }
            progressionFromSpeciesId = target->id;
            progressionToSpeciesId = 0;
            progressionMoveCursor = 0;
            uint16_t speciesIds[Game::TEAM_CAP] = {};
            uint8_t count = Game::TeamRoster::memberCount(gameState);
            for (uint8_t slot = 0; slot < count; ++slot) {
                speciesIds[slot] = gameState.team[slot].speciesId;
            }
            PokemonSprites::syncTeamCache(speciesIds, count);
            if (progressionTeamSlot == 0) {
                if (const Species* leader = findSpecies(monster.speciesId)) {
                    behaviorProfile = behaviorProfileFor(*leader, monster);
                }
            }
        }
        progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
    } else if (progressionMode == ProgressionViewModel::Mode::MOVE_LEARN) {
        // The button is also the skip action when a move is not wanted.
    }

    Game::MoveId nextMove = 0;
    if (!findNextProgressionMove(progressionTeamSlot, progressionOldLevel,
                                 progressionMoveCursor, nextMove)) {
        completeProgression(nowMs);
        return;
    }
    progressionMoveId = nextMove;
    if (monster.move2Id == 0) {
        monster.move2Id = nextMove;
        monster.moveProficiency[1] = 0;
        saveState();
        advanceProgression(nowMs);
        return;
    }
    if (monster.move3Id == 0) {
        monster.move3Id = nextMove;
        monster.moveProficiency[2] = 0;
        saveState();
        advanceProgression(nowMs);
        return;
    }
    progressionOldMove2 = monster.move2Id;
    progressionOldMove3 = monster.move3Id;
    progressionMode = ProgressionViewModel::Mode::MOVE_REPLACE;
    progressionPressedItem = 0xFF;
    saveState();
    requestFullRender();
}

void AmoledApp::completeProgression(uint32_t nowMs) {
    gameState.pendingLevelUp = false;
    gameState.pendingLevelUpLevel = 0;
    gameState.pendingMoveLearn = false;
    gameState.pendingMoveSlot = 0;
    gameState.pendingMoveId = 0;
    gameState.pendingMoveCursor = 0;
    progressionPressedItem = 0xFF;
    saveState();
    sceneFlow.enter(progressionReturnScene);
    setToast(Ui::Amoled::GROWTH_COMPLETE, nowMs, 1200);
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
        setToast(Ui::Shower::NO_SOAP, nowMs);
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
    const uint8_t oldLevel = monster.level;
    Game::ExperienceService::Result experience;
    if (reward.experience > 0) {
        if (const Species* species = findSpecies(monster.speciesId)) {
            experience = Game::ExperienceService::add(
                monster, *species, reward.experience);
            if (experience.leveledUp) {
                gameState.pendingLevelUp = true;
                gameState.pendingLevelUpLevel = monster.level;
            }
        }
    }

    if (reward.experience > 0 && reward.moodGain > 0) {
        std::snprintf(showerToast, sizeof(showerToast),
                      Ui::Amoled::SHOWER_EXP_MOOD_FMT,
                      reward.experience, reward.moodGain);
    } else if (reward.experience > 0) {
        std::snprintf(showerToast, sizeof(showerToast),
                      Ui::Amoled::SHOWER_EXP_FMT,
                      reward.experience);
    } else if (reward.moodGain > 0) {
        std::snprintf(showerToast, sizeof(showerToast),
                      Ui::Amoled::SHOWER_MOOD_FMT,
                      reward.moodGain);
    } else {
        std::snprintf(showerToast, sizeof(showerToast), "%s",
                      Ui::Amoled::CARE_LIMIT);
    }
    setToast(showerToast, nowMs, 1300);
    saveState();
    if (experience.leveledUp) {
        openProgressionScene(AppSceneFlow::Scene::SHOWER, 0, oldLevel, nowMs);
    }
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
    teamMovesOpen = false;
    teamMovesForgetConfirm = false;
    itemConfirmOpen = false;
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::refreshTeamMoveRecallable() {
    teamMovesRecallCount = 0;
    if (!teamMovesOpen || teamMovesSlot >= gameState.teamCount ||
        teamMovesSlot >= Game::TEAM_CAP) return;
    const Game::MonsterRuntime& monster = gameState.team[teamMovesSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return;
    teamMovesRecallCount = Game::MoveManagementService::collectRecallable(
        *species, monster, teamMovesRecallIds,
        Game::MoveManagementService::MAX_RECALLABLE_MOVE_COUNT);
}

void AmoledApp::openTeamMoves(uint8_t teamSlot, uint32_t nowMs) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) {
        setToast(Ui::Amoled::NO_TEAM_MEMBER, nowMs);
        return;
    }
    teamMovesOpen = true;
    teamMovesSlot = teamSlot;
    teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
    teamMovesRecallSelected = 0xFF;
    teamMovesForgetSlot = 0;
    teamMovesForgetConfirm = false;
    refreshTeamMoveRecallable();
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::switchTeamLeader(uint32_t nowMs) {
    bool changed = pendingTeamSlot > 0 &&
        Game::TeamRoster::moveToFront(gameState, pendingTeamSlot);
    teamConfirmOpen = false;
    pendingTeamSlot = 0;
    if (!changed) {
        setToast(Ui::Amoled::CANNOT_SWITCH, nowMs);
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
    petResting = false;
    petTargetX = petX;
    petTargetY = petY;
    monsterMind.reset(nowMs);
    nextMindUpdateMs = nowMs;
    schedulePetDecision(nowMs);
    updatePetFootprint();
    updateCamera();
    saveState();
    setToast(Ui::Amoled::LEADER_CHANGED, nowMs);
}

void AmoledApp::performPendingItemAction(uint32_t nowMs) {
    const char* resultText = Ui::Amoled::NO_EFFECT;
    bool changed = false;
    if (pendingItemAction == PendingItemAction::BUY) {
        switch (Game::ShopService::buy(gameState, pendingItem)) {
        case Game::ShopService::BuyResult::BOUGHT:
            resultText = Ui::Amoled::ITEM_BOUGHT;
            changed = true;
            break;
        case Game::ShopService::BuyResult::LOCKED:
            resultText = Ui::Amoled::ITEM_LOCKED;
            break;
        case Game::ShopService::BuyResult::NOT_ENOUGH_COINS:
            resultText = Ui::Amoled::NOT_ENOUGH_COINS;
            break;
        case Game::ShopService::BuyResult::BAG_FULL:
            resultText = Ui::Amoled::BAG_FULL;
            break;
        case Game::ShopService::BuyResult::DAILY_LIMIT:
            resultText = Ui::Amoled::DAILY_LIMIT;
            break;
        case Game::ShopService::BuyResult::INVALID_ITEM:
            resultText = Ui::Amoled::INVALID_ITEM;
            break;
        }
    } else if (pendingItemAction == PendingItemAction::SELL) {
        switch (Game::ShopService::sell(gameState, pendingItem)) {
        case Game::ShopService::SellResult::SOLD:
            resultText = Ui::Amoled::ITEM_SOLD;
            changed = true;
            break;
        case Game::ShopService::SellResult::NO_STOCK:
            resultText = Ui::Amoled::NO_STOCK;
            break;
        case Game::ShopService::SellResult::INVALID_ITEM:
            resultText = Ui::Amoled::INVALID_ITEM;
            break;
        }
    } else if (pendingItemAction == PendingItemAction::USE) {
        bool explorationItem = pendingItem == Game::ItemId::MAX_REPEL ||
                               pendingItem == Game::ItemId::HONEY;
        if (explorationItem &&
            sceneFlow.subSceneReturn() == AppSceneFlow::Scene::EXPLORE_MENU) {
            bool activated = false;
            if (pendingItem == Game::ItemId::MAX_REPEL) {
                activated = exploreItemEffects.repelStepsRemaining() == 0 &&
                           Game::ItemInventory::remove(gameState, pendingItem);
                if (activated && !exploreItemEffects.activateMaxRepel()) {
                    Game::ItemInventory::add(gameState, pendingItem);
                    activated = false;
                }
            } else if (!exploreItemEffects.honeyEncounterPending() &&
                       Game::ItemInventory::remove(gameState, pendingItem)) {
                activated = exploreItemEffects.activateHoney();
                if (!activated) Game::ItemInventory::add(gameState, pendingItem);
            }
            if (activated) {
                resultText = pendingItem == Game::ItemId::MAX_REPEL
                    ? Ui::Bag::MAX_REPEL_ACTIVE : Ui::Bag::HONEY_ACTIVE;
                changed = true;
            } else {
                resultText = Ui::Amoled::ITEM_NOT_READY;
            }
        } else if (explorationItem) {
            resultText = Ui::Amoled::EXPLORE_ONLY;
        } else {
            uint8_t target = Game::ItemInventory::preferredTarget(
                gameState, pendingItem);
            uint8_t oldLevel = target < gameState.teamCount
                ? gameState.team[target].level : 0;
            uint16_t oldSpeciesId = target < gameState.teamCount
                ? gameState.team[target].speciesId : 0;
            switch (Game::ItemInventory::useOnTeam(
                        gameState, pendingItem, target)) {
        case Game::ItemInventory::UseResult::USED:
            resultText = Ui::Amoled::ITEM_USED;
            changed = true;
            break;
        case Game::ItemInventory::UseResult::NO_STOCK:
            resultText = Ui::Amoled::NO_STOCK;
            break;
        case Game::ItemInventory::UseResult::INVALID_TARGET:
            resultText = Ui::Amoled::NO_TARGET;
            break;
        case Game::ItemInventory::UseResult::FAINTED:
            resultText = Ui::Amoled::MON_FAINTED;
            break;
        case Game::ItemInventory::UseResult::HP_FULL:
            resultText = Ui::Amoled::HP_FULL;
            break;
        case Game::ItemInventory::UseResult::STATUS_NORMAL:
            resultText = Ui::Amoled::STATUS_NORMAL;
            break;
        case Game::ItemInventory::UseResult::NO_FAINTED_TARGET:
            resultText = Ui::Amoled::NO_FAINTED_MON;
            break;
        case Game::ItemInventory::UseResult::NOT_USABLE:
            resultText = Ui::Amoled::NOT_READY;
            break;
            }
            if (changed && target == 0 &&
                (oldLevel != gameState.team[0].level ||
                 oldSpeciesId != gameState.team[0].speciesId)) {
                uint16_t speciesIds[Game::TEAM_CAP] = {};
                uint8_t count = Game::TeamRoster::memberCount(gameState);
                for (uint8_t slot = 0; slot < count; ++slot) {
                    speciesIds[slot] = gameState.team[slot].speciesId;
                }
                PokemonSprites::syncTeamCache(speciesIds, count);
                if (oldLevel != gameState.team[0].level) {
                    saveState();
                    openProgressionScene(
                        sceneFlow.subSceneReturn() == AppSceneFlow::Scene::EXPLORE_MENU
                            ? AppSceneFlow::Scene::EXPLORE_MENU
                            : AppSceneFlow::Scene::HOME,
                        0, oldLevel, nowMs);
                    return;
                }
                if (oldSpeciesId != gameState.team[0].speciesId) {
                    saveState();
                    openEvolutionProgression(
                        sceneFlow.subSceneReturn() == AppSceneFlow::Scene::EXPLORE_MENU
                            ? AppSceneFlow::Scene::EXPLORE_MENU
                            : AppSceneFlow::Scene::HOME,
                        0, oldSpeciesId, gameState.team[0].speciesId, nowMs);
                    return;
                }
            }
        }
    }

    bool shopTransaction = sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
        (pendingItemAction == PendingItemAction::BUY ||
         pendingItemAction == PendingItemAction::SELL);
    if (shopTransaction) {
        if (changed) {
            saveState();
            toast = nullptr;
            requestFullRender();
        } else {
            setToast(resultText, nowMs);
        }
        return;
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
#if STICKMON_ENABLE_DEBUG_FEATURES
    Game::GameState persistentState = gameState;
    for (uint8_t slot = 0; slot < persistentState.teamCount;) {
        if (persistentState.team[slot].origin != Game::Origin::VISITOR) {
            ++slot;
            continue;
        }
        for (uint8_t next = slot + 1; next < persistentState.teamCount;
             ++next) {
            persistentState.team[next - 1] = persistentState.team[next];
        }
        --persistentState.teamCount;
        persistentState.team[persistentState.teamCount] =
            Game::MonsterRuntime{};
    }
    persistentState.activeSlot = 0;
    bool snapshotSaved = saveManager.saveSnapshot(
        persistentState, mainViewState);
#else
    bool snapshotSaved = saveManager.saveSnapshot(gameState, mainViewState);
#endif
    bool historySaved = !encounterHistoryDirty ||
        saveManager.saveEncounterHistory(encounterHistory);
    if (historySaved) encounterHistoryDirty = false;
    if (!snapshotSaved || !historySaved) {
        Platform::logLine("[AmoledApp] save failed");
    }
    return snapshotSaved && historySaved;
}

bool AmoledApp::hasEncounteredSpecies(uint16_t speciesId) const {
    if (encounterHistory.contains(speciesId)) return true;
    for (uint8_t i = 0;
         i < gameState.teamCount && i < Game::TEAM_CAP; ++i) {
        if (gameState.team[i].speciesId == speciesId) return true;
    }
    for (uint8_t i = 0;
         i < gameState.storageCount && i < Game::STORAGE_CAP; ++i) {
        if (gameState.storage[i].speciesId == speciesId) return true;
    }
    return false;
}

bool AmoledApp::recordEncounteredSpecies(uint16_t speciesId) {
    if (hasEncounteredSpecies(speciesId)) return false;
    if (!encounterHistory.add(speciesId)) return false;
    encounterHistoryDirty = true;
    Platform::logf("[AmoledApp] encountered species=%u total=%u\n",
                   speciesId, encounterHistory.count);
    return true;
}

bool AmoledApp::syncOwnedSpeciesToEncounterHistory() {
    bool changed = false;
    for (uint8_t i = 0;
         i < gameState.teamCount && i < Game::TEAM_CAP; ++i) {
        changed |= encounterHistory.add(gameState.team[i].speciesId);
    }
    for (uint8_t i = 0;
         i < gameState.storageCount && i < Game::STORAGE_CAP; ++i) {
        changed |= encounterHistory.add(gameState.storage[i].speciesId);
    }
    return changed;
}

}  // namespace AmoledV1
