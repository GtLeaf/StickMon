#include "app/ExploreSession.h"

#include <algorithm>
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreBoss.h"
#include "game/ExploreBossPity.h"
#include "game/ExploreEncounters.h"
#include "game/ExploreEncounterRules.h"
#include "game/ExploreIceSlide.h"
#include "game/ExploreItemProgression.h"
#include "game/ExploreRunRules.h"
#include "game/BondSystem.h"
#include "game/GameRandom.h"
#include "game/ItemInventory.h"
#include "game/MonsterFactory.h"
#include "game/Species.h"

namespace AmoledV1 {
namespace {
constexpr uint8_t EXPLORE_MAP_MIN_COUNT[] = {3, 4, 4, 5, 6, 7};
constexpr uint8_t EXPLORE_MAP_MAX_COUNT[] = {4, 5, 6, 7, 8, 9};
constexpr uint16_t EXPLORE_ENCOUNTER_CHANCE[] = {
    500, 600, 700, 900, 1100, 1300,
};
constexpr uint8_t EXPLORE_ENCOUNTER_COOLDOWN_STEPS = 5;
constexpr uint8_t EXPLORE_MAX_ENCOUNTERS_PER_MAP = 2;
constexpr uint16_t EXPLORE_MAP_PICKUP_CHANCE = 6500;
constexpr uint32_t EXPLORE_MAP_GENERATION_SAFE_SEED = 1;
constexpr uint32_t EXPLORE_MAP_GENERATION_RETRY_SALTS[] = {
    0,
    0x6D2B79F5U,
    0x9E3779B9U,
    0x85EBCA6BU,
};
constexpr uint8_t exploreMapCountForRoll(uint8_t area, uint8_t roll) {
    uint8_t minCount = EXPLORE_MAP_MIN_COUNT[area];
    uint8_t maxCount = EXPLORE_MAP_MAX_COUNT[area];
    return maxCount == minCount
        ? minCount
        : (maxCount == minCount + 1
            ? (roll < 50 ? minCount : maxCount)
            : (roll < 25
                ? minCount
                : (roll < 75 ? minCount + 1 : maxCount)));
}

constexpr uint8_t exploreCooldownAfterStep(uint8_t cooldown) {
    return cooldown > 0 ? cooldown - 1 : 0;
}

constexpr bool exploreEncounterGateOpen(uint8_t cooldown,
                                        uint8_t encounterCount) {
    return cooldown == 0 &&
           encounterCount < EXPLORE_MAX_ENCOUNTERS_PER_MAP;
}

constexpr bool exploreCanScheduleGuaranteedEncounter(uint8_t pointCount,
                                                     uint8_t cooldown) {
    return pointCount >= 3 && cooldown + 1 <= pointCount - 2;
}

constexpr uint8_t exploreGuaranteedEncounterIndex(uint8_t pointCount,
                                                  uint8_t cooldown) {
    return pointCount * 2 / 3 < cooldown + 1
        ? cooldown + 1
        : (pointCount * 2 / 3 > pointCount - 2
            ? pointCount - 2
            : pointCount * 2 / 3);
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


}  // namespace

bool ExploreSession::healthy(const Game::MonsterRuntime& monster) {
    return !monster.fainted && monster.hpCur > 0;
}

uint8_t ExploreSession::leaderSlot(const Game::GameState& state) {
    const bool firstHealthy = state.teamCount > 0 &&
        healthy(state.team[0]);
    const bool secondHealthy = state.teamCount > 1 &&
        healthy(state.team[1]);
    return ExploreRunRules::leaderSlotForHealth(firstHealthy, secondHealthy);
}

bool ExploreSession::hasCompanion(const Game::GameState& state) {
    return leaderSlot(state) == 0 && state.teamCount > 1 &&
           state.team[1].speciesId != 0 &&
           healthy(state.team[1]);
}

bool ExploreSession::begin(Game::GameState& gameState, uint8_t area) {
    if (!ExploreItemProgression::isAreaUnlocked(area, gameState)) return false;
    area_ = area;
    effects_ = {};
    battleActive_ = false;
    returnSettled_ = false;
    state_.exploreRoutePool = ExploreEncounters::poolForArea(area_, gameState.gameMinutesTotal,
        gameState.explorePoolRerollCounts[area_]);
    state_.exploreRouteMapBlock = 0;
    state_.exploreRouteMapBlockCount = exploreMapCountForRoll(
        area_,
        static_cast<uint8_t>(GameRandom::range(0, 100)));
    state_.exploreRouteMapEncounterCount = 0;
    state_.exploreRouteEncounterCooldownSteps = 0;
    state_.exploreRouteGuaranteedEncounterIndex = 0;
    state_.exploreRouteGuaranteedEncounterPending = false;
    state_.exploreRouteExpeditionSeed = static_cast<uint32_t>(
        GameRandom::range(1, 0x7FFFFFFFU));
    state_.exploreRoutePendingEntryEdge = static_cast<ExploreMapGenerator::Edge>(
        (state_.exploreRouteExpeditionSeed >> 8) & 0x03U);
    state_.exploreRoutePendingFrostLadder = false;
    state_.exploreRoutePendingFrostFall = false;
    state_.exploreRouteFrostFallAtMs = 0;

    state_.exploreRouteIndex = 0;
    state_.exploreRouteSteps = 0;
    state_.exploreRouteMoving = false;
    state_.exploreRouteAutoWalk = false;
    state_.exploreRouteExiting = false;
    state_.exploreRoutePlayerWalkActive = false;
    state_.exploreRoutePaused = false;
    state_.exploreRoutePausedAtMs = 0;
    state_.exploreRouteComplete = false;
    state_.exploreRouteExitConfirm = false;
    state_.exploreRouteIceSliding = false;
    state_.exploreRouteIceDx = 0;
    state_.exploreRouteIceDy = 0;
    state_.exploreRoutePickupIndex = 0;
    state_.exploreRoutePickupItem = EXPLORE_PICKUP_NONE;
    state_.exploreRoutePickupAvailable = false;
    state_.exploreRouteBossScheduled = false;
    state_.exploreRouteBossIndex = 0;
    state_.exploreRouteBossPending = false;
    state_.exploreRoutePityEligible = false;
    state_.exploreRouteBossSpeciesId = 0;
    state_.exploreRouteBossLevel = 0;
    state_.exploreRouteBossExperiencePercent = 100;
    state_.exploreRouteSpecialKind = ExploreSpecial::Kind::NONE;
    state_.exploreItemEffects.reset();
    bool bossLengthEligible = ExploreRunRules::allowsRegionalBoss(
        state_.exploreRouteMapBlockCount,
        EXPLORE_MAP_MAX_COUNT[area_]);
    if (bossLengthEligible) {
        uint32_t slotIndex = ExploreSpecial::slotIndexFor(
            gameState.gameMinutesTotal);
        if (ExploreBossPity::syncSlot(gameState, slotIndex)) effects_.save = true;
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
        state_.exploreRouteSpecialKind = ExploreSpecial::kindForArea(
            area_, gameState.specialBossDefeatedMask,
            slotIndex, gameState.roamingRerollCounts, ownsMew);
        if (state_.exploreRouteSpecialKind != ExploreSpecial::Kind::NONE) {
            ExploreSpecial::Config config = ExploreSpecial::configFor(
                state_.exploreRouteSpecialKind);
            state_.exploreRouteBossScheduled = true;
            state_.exploreRouteBossSpeciesId = config.speciesId;
            state_.exploreRouteBossLevel = ExploreSpecial::encounterLevel(
                state_.exploreRouteSpecialKind,
                ExploreBoss::configForArea(area_).level);
            state_.exploreRouteBossExperiencePercent = config.experiencePercent;
        } else {
            uint8_t misses = gameState.normalBossMissCount[area_];
            uint32_t chance = ExploreBossPity::chanceForMisses(misses);
            if (ExploreBossPity::requiresGuaranteedEligibleRun(misses) ||
                GameRandom::range(0, ExploreBoss::SPAWN_ROLL_MAX) < chance) {
                const ExploreBoss::Config& config =
                    ExploreBoss::configForArea(area_);
                state_.exploreRouteBossScheduled = true;
                state_.exploreRouteBossSpeciesId = ExploreBoss::speciesForRoll(
                    area_,
                    GameRandom::range(0, ExploreBoss::CANDIDATE_COUNT));
                state_.exploreRouteBossLevel = config.level;
                state_.exploreRouteBossExperiencePercent = config.experiencePercent;
            } else {
                state_.exploreRoutePityEligible = true;
            }
        }
    }

    return true;
}

bool ExploreSession::generateMap(Game::GameState& gameState, uint8_t area) {
    if (area >= Game::EXPLORE_AREA_COUNT) return false;
    area_ = area;

    uint32_t mapSeed = ExploreMapGenerator::deriveSeed(
        state_.exploreRouteExpeditionSeed, state_.exploreRouteMapBlock,
        area_);
    bool generated = false;
    for (uint32_t salt : EXPLORE_MAP_GENERATION_RETRY_SALTS) {
        uint32_t candidateSeed = mapSeed ^ salt;
        if (candidateSeed == 0) candidateSeed = EXPLORE_MAP_GENERATION_SAFE_SEED;
        if (ExploreMapGenerator::generate(
                candidateSeed, state_.exploreRoutePendingEntryEdge,
                area_, state_.exploreRouteMap,
                {state_.exploreRouteMapBlock, state_.exploreRouteMapBlockCount,
                 state_.exploreRoutePendingFrostLadder})) {
            generated = true;
            break;
        }
    }
    if (!generated) {
        generated = ExploreMapGenerator::generate(
            EXPLORE_MAP_GENERATION_SAFE_SEED,
            state_.exploreRoutePendingEntryEdge, area_,
            state_.exploreRouteMap,
            {state_.exploreRouteMapBlock, state_.exploreRouteMapBlockCount,
             state_.exploreRoutePendingFrostLadder});
    }
    if (!generated || state_.exploreRouteMap.pathCount == 0) {
        effects_.notice = Notice::MAP_FAILED;
        return false;
    }

    state_.exploreRoutePath = 0;
    if (state_.exploreRouteMapBlock + 1 == state_.exploreRouteMapBlockCount &&
        state_.exploreRouteBossScheduled) {
        uint8_t bossPaths[ExploreMapGenerator::MAX_PATH_COUNT] = {};
        uint8_t bossPathCount = 0;
        for (uint8_t path = 0; path < state_.exploreRouteMap.pathCount; ++path) {
            if (ExploreBoss::canPlaceOnPath(
                    state_.exploreRouteMap.paths[path].pointCount)) {
                bossPaths[bossPathCount++] = path;
            }
        }
        if (bossPathCount > 0) {
            state_.exploreRoutePath = bossPaths[
                GameRandom::range(0, bossPathCount)];
        }
    } else if (state_.exploreRouteMap.pathCount > 1) {
        state_.exploreRoutePath = static_cast<uint8_t>(
            GameRandom::range(0, state_.exploreRouteMap.pathCount));
    }

    const ExploreMapGenerator::Path& path =
        state_.exploreRouteMap.paths[state_.exploreRoutePath];
    if (path.pointCount == 0) {
        effects_.notice = Notice::MAP_FAILED;
        return false;
    }
    state_.exploreRouteMapEncounterCount = 0;
    state_.exploreRouteBossPending = false;
    state_.exploreRouteBossIndex = 0;
    if (state_.exploreRouteMapBlock + 1 == state_.exploreRouteMapBlockCount &&
        state_.exploreRouteBossScheduled && ExploreBoss::canPlaceOnPath(
            path.pointCount)) {
        uint8_t preferred = ExploreBoss::routeIndex(path.pointCount);
        state_.exploreRouteBossIndex = ExploreIceSlide::nearestNonIceIndex(
            state_.exploreRouteMap, path, preferred, 1,
            static_cast<uint8_t>(path.pointCount - 2));
        if (state_.exploreRouteBossIndex != ExploreIceSlide::INVALID_INDEX) {
            state_.exploreRouteBossPending = true;
        }
    }


    state_.exploreRouteIndex = 0;
    state_.exploreRouteMoving = false;
    state_.exploreRouteIceSliding = false;
    state_.exploreRouteIceDx = 0;
    state_.exploreRouteIceDy = 0;
    if (state_.exploreRoutePendingFrostFall) {
        state_.exploreRouteIndex = ExploreIceSlide::landingIndex(state_.exploreRouteMap, path,
            state_.exploreRouteMap.seed ^ state_.exploreRouteExpeditionSeed);
        state_.exploreRoutePendingFrostFall = false;
    }
    placePickup(gameState);
    effects_.mapChanged = true;
    effects_.fullRender = true;
    return true;
}

void ExploreSession::placePickup(const Game::GameState& gameState) {
    state_.exploreRoutePickupIndex = 0;
    state_.exploreRoutePickupItem = EXPLORE_PICKUP_NONE;
    state_.exploreRoutePickupAvailable = false;
    state_.exploreRouteGuaranteedEncounterIndex = 0;
    state_.exploreRouteGuaranteedEncounterPending = false;

    if (state_.exploreRouteMap.pathCount == 0) return;
    const ExploreMapGenerator::Path& path =
        state_.exploreRouteMap.paths[state_.exploreRoutePath];
    if (path.pointCount < 2) return;

    if (state_.exploreRouteBossPending) return;

    if (state_.exploreRouteMapBlock + 1 == state_.exploreRouteMapBlockCount) {
        uint8_t preferred = path.pointCount >= 3
            ? static_cast<uint8_t>(path.pointCount - 2)
            : static_cast<uint8_t>(path.pointCount - 1);
        uint8_t last = preferred;
        state_.exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            state_.exploreRouteMap, path, preferred, 1, last);
        if (state_.exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) {
            state_.exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
                state_.exploreRouteMap, path, preferred, 1,
                static_cast<uint8_t>(path.pointCount - 1));
        }
        if (state_.exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        state_.exploreRoutePickupItem = rollExplorePickup(
            area_, gameState.stepsToday);
        if (state_.exploreRoutePickupItem == EXPLORE_PICKUP_NONE) {
            state_.exploreRoutePickupItem = EXPLORE_PICKUP_COIN;
        }
        state_.exploreRoutePickupAvailable = true;
        return;
    }

    // Match Stick: place the pickup in the middle portion of the route and
    // move it to the nearest non-ice point so sliding cannot skip it.
    if (path.pointCount < 3) {
        state_.exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            state_.exploreRouteMap, path,
            static_cast<uint8_t>(path.pointCount - 1),
            1, static_cast<uint8_t>(path.pointCount - 1));
        if (state_.exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        state_.exploreRoutePickupItem = rollExplorePickup(
            area_, gameState.stepsToday);
        state_.exploreRoutePickupAvailable =
            state_.exploreRoutePickupItem != EXPLORE_PICKUP_NONE;
        return;
    }

    bool choosePickup = GameRandom::range(0, 10000) <
                        EXPLORE_MAP_PICKUP_CHANCE;
    if (!choosePickup && exploreCanScheduleGuaranteedEncounter(
            path.pointCount, state_.exploreRouteEncounterCooldownSteps)) {
        uint8_t first = static_cast<uint8_t>(
            state_.exploreRouteEncounterCooldownSteps + 1);
        uint8_t last = static_cast<uint8_t>(path.pointCount - 2);
        state_.exploreRouteGuaranteedEncounterIndex = ExploreIceSlide::nearestNonIceIndex(
            state_.exploreRouteMap, path,
            exploreGuaranteedEncounterIndex(
                path.pointCount, state_.exploreRouteEncounterCooldownSteps),
            first, last);
        if (state_.exploreRouteGuaranteedEncounterIndex !=
            ExploreIceSlide::INVALID_INDEX) {
            state_.exploreRouteGuaranteedEncounterPending = true;
            return;
        }
    }

    uint8_t first = std::max<uint8_t>(1, path.pointCount / 3);
    uint8_t last = std::min<uint8_t>(path.pointCount - 2,
                                     path.pointCount * 3 / 4);
    if (first > last) first = last = path.pointCount / 2;

    uint8_t preferred = static_cast<uint8_t>(
        GameRandom::range(first, static_cast<uint32_t>(last) + 1));
    state_.exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
        state_.exploreRouteMap, path, preferred, first, last);
    if (state_.exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) {
        state_.exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            state_.exploreRouteMap, path, preferred, 1,
            static_cast<uint8_t>(path.pointCount - 1));
    }
    if (state_.exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) return;

    state_.exploreRoutePickupItem = rollExplorePickup(
        area_, gameState.stepsToday);
    state_.exploreRoutePickupAvailable =
        state_.exploreRoutePickupItem != EXPLORE_PICKUP_NONE;
}

bool ExploreSession::beginStep(Game::GameState& gameState) {
    if (state_.exploreRouteMoving || state_.exploreRoutePaused || state_.exploreRouteComplete || battleActive_ ||
        state_.exploreRoutePendingFrostFall || state_.exploreRouteMap.pathCount == 0 ||
        state_.exploreRoutePath >= state_.exploreRouteMap.pathCount) return false;
    const auto& path = state_.exploreRouteMap.paths[state_.exploreRoutePath];
    if (state_.exploreRouteIndex + 1 >= path.pointCount) {
        if (state_.exploreRouteMapBlock + 1 >= state_.exploreRouteMapBlockCount) effects_.startExit = true;
        else finishAtEnd(gameState);
        effects_.fullRender = true;
        return false;
    }
    if (!state_.exploreRouteIceSliding) {
        int8_t dx = 0, dy = 0;
        if (ExploreIceSlide::begins(state_.exploreRouteMap, path, state_.exploreRouteIndex, dx, dy)) {
            state_.exploreRouteIceSliding = true;
            state_.exploreRouteIceDx = dx;
            state_.exploreRouteIceDy = dy;
        }
    }
    ++state_.exploreRouteIndex;
    state_.exploreRouteMoving = true;
    return true;
}


void ExploreSession::completeStep(Game::GameState& gameState, uint32_t nowMs) {
    if (!state_.exploreRouteMoving || state_.exploreRouteExiting || state_.exploreRoutePaused || battleActive_) return;
    state_.exploreRouteMoving = false;
    ++state_.exploreRouteSteps;
    if (recoverTeam(gameState)) effects_.statusChanged = true;
    bool encounterBlockedThisStep =
        state_.exploreRouteEncounterCooldownSteps > 0;
    bool repelActiveThisStep = state_.exploreItemEffects.repelStepsRemaining() > 0;
    state_.exploreRouteEncounterCooldownSteps = exploreCooldownAfterStep(
        state_.exploreRouteEncounterCooldownSteps);
    state_.exploreItemEffects.completeWalkStep();
    gameState.stepsToday = static_cast<uint16_t>(std::min<uint32_t>(
        60000, static_cast<uint32_t>(gameState.stepsToday) + 1));
    const ExploreMapGenerator::Path& path =
        state_.exploreRouteMap.paths[state_.exploreRoutePath];
    if (area_ == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA &&
        state_.exploreRouteMapBlock + 1 < state_.exploreRouteMapBlockCount &&
        ExploreIceSlide::isCrackedIce(state_.exploreRouteMap, path, state_.exploreRouteIndex)) {
        ExploreIceSlide::breakIce(state_.exploreRouteMap, path, state_.exploreRouteIndex);
        effects_.invalidateWorld = true;
        state_.exploreRoutePendingFrostFall = true;
        state_.exploreRouteFrostFallAtMs = nowMs + 240;
        state_.exploreRouteIceSliding = false;
        state_.exploreRouteIceDx = 0;
        state_.exploreRouteIceDy = 0;
        effects_.fullRender = true;
        return;
    }
    bool continueIce = state_.exploreRouteIceSliding && ExploreIceSlide::continues(
        state_.exploreRouteMap, path, state_.exploreRouteIndex,
        state_.exploreRouteIceDx, state_.exploreRouteIceDy);
    if (!continueIce) {
        state_.exploreRouteIceSliding = false;
        state_.exploreRouteIceDx = 0;
        state_.exploreRouteIceDy = 0;
        resolveStepEvent(gameState, encounterBlockedThisStep,
                                repelActiveThisStep);
    }
    if (effects_.startBattle) {
        effects_.fullRender = true;
        return;
    }
    if (state_.exploreRouteIndex + 1 >= path.pointCount) {
        if (state_.exploreRouteMapBlock + 1 >= state_.exploreRouteMapBlockCount) {
            effects_.startExit = true;
            return;
        }
        if (finishAtEnd(gameState)) return;
    } else if (state_.exploreRoutePlayerWalkActive &&
               state_.exploreRouteBossPending &&
               state_.exploreRouteIndex + 1 == state_.exploreRouteBossIndex) {
        // Stick stops one route point before the regional boss. The next tap
        // enters the boss point; Agent mode continues without this pause.
        state_.exploreRoutePlayerWalkActive = false;
    } else if (continueIce || state_.exploreRouteAutoWalk ||
               state_.exploreRoutePlayerWalkActive) {
        effects_.startStep = true;
    }
}

bool ExploreSession::finishAtEnd(Game::GameState& gameState) {
    if (state_.exploreRouteComplete || battleActive_) return false;

    if (state_.exploreRouteBossPending) {
        if (beginEncounter(
                gameState, area_, true, state_.exploreRouteBossSpeciesId,
                state_.exploreRouteBossLevel, state_.exploreRouteBossExperiencePercent,
                state_.exploreRouteSpecialKind)) {
            state_.exploreRouteBossPending = false;
            state_.exploreRouteAutoWalk = false;
            state_.exploreRoutePlayerWalkActive = false;
            return true;
        }
        // Keep the pending boss at the route end when the active team cannot
        // enter battle yet; a later tap can retry after the player recovers.
        state_.exploreRouteAutoWalk = false;
        state_.exploreRoutePlayerWalkActive = false;
        return false;
    }
    if (state_.exploreRouteMapBlock + 1 < state_.exploreRouteMapBlockCount) {
        const ExploreMapGenerator::Path& path =
            state_.exploreRouteMap.paths[state_.exploreRoutePath];
        state_.exploreRoutePendingEntryEdge = ExploreMapGenerator::opposite(
            path.exit.edge);
        state_.exploreRoutePendingFrostLadder =
            !state_.exploreRoutePendingFrostFall && state_.exploreRoutePath == 1;
        ++state_.exploreRouteMapBlock;
        if (!generateMap(gameState, area_)) {
            state_.exploreRouteAutoWalk = false;
            state_.exploreRoutePlayerWalkActive = false;
            return false;
        }
        if (state_.exploreRouteAutoWalk || state_.exploreRoutePlayerWalkActive) {
            effects_.startStep = true;
        }
        return true;
    }
    if (state_.exploreRoutePityEligible) {
        ExploreBossPity::increment(gameState, area_);
        state_.exploreRoutePityEligible = false;
        effects_.save = true;
    }
    state_.exploreRouteComplete = true;
    state_.exploreRouteAutoWalk = false;
    state_.exploreRoutePlayerWalkActive = false;
    effects_.completed = true;
    effects_.fullRender = true;
    return false;
}

void ExploreSession::resolveStepEvent(Game::GameState& gameState,
    bool encounterBlockedThisStep, bool repelActiveThisStep) {
    bool honeyEncounter = state_.exploreItemEffects.honeyEncounterPending();

    if (state_.exploreRouteBossPending &&
        state_.exploreRouteIndex == state_.exploreRouteBossIndex) {
        if (beginEncounter(
                gameState, area_, true, state_.exploreRouteBossSpeciesId,
                state_.exploreRouteBossLevel, state_.exploreRouteBossExperiencePercent,
                state_.exploreRouteSpecialKind)) {
            state_.exploreRouteBossPending = false;
            state_.exploreRouteAutoWalk = false;
            state_.exploreRoutePlayerWalkActive = false;
        }
        return;
    }
    if (state_.exploreRoutePickupAvailable &&
        state_.exploreRouteIndex == state_.exploreRoutePickupIndex) {
        resolvePickup(gameState);
        return;
    }

    const ExploreMapGenerator::Path& path =
        state_.exploreRouteMap.paths[state_.exploreRoutePath];
    if (state_.exploreRouteIndex + 1 >= path.pointCount) return;

    bool guaranteedEncounter = state_.exploreRouteGuaranteedEncounterPending &&
        state_.exploreRouteIndex >= state_.exploreRouteGuaranteedEncounterIndex;
    bool encounterGateBypassed = honeyEncounter;
    bool encounterGateOpen = encounterGateBypassed ||
        exploreEncounterGateOpen(encounterBlockedThisStep,
                                 state_.exploreRouteMapEncounterCount);
    bool repelAllowsEncounter = !repelActiveThisStep ||
                                guaranteedEncounter || honeyEncounter;
    uint16_t encounterChance = EXPLORE_ENCOUNTER_CHANCE[
        std::min<uint8_t>(area_,
                          static_cast<uint8_t>(Game::EXPLORE_AREA_COUNT - 1))];
    bool randomEncounter = false;
    if (encounterGateOpen && repelAllowsEncounter &&
        !guaranteedEncounter && !honeyEncounter) {
        randomEncounter = GameRandom::range(0, 10000) < encounterChance;
    }
    if (encounterGateOpen && repelAllowsEncounter &&
        (guaranteedEncounter || honeyEncounter || randomEncounter) &&
        beginEncounter(gameState, area_)) {
        ++state_.exploreRouteMapEncounterCount;
        state_.exploreRouteEncounterCooldownSteps =
            EXPLORE_ENCOUNTER_COOLDOWN_STEPS;
        state_.exploreRouteGuaranteedEncounterPending = false;
        if (honeyEncounter) state_.exploreItemEffects.consumeHoneyEncounter();
        return;
    }
}

void ExploreSession::resolvePickup(Game::GameState& gameState) {
    if (!state_.exploreRoutePickupAvailable) return;
    // Manual route walking pauses at a pickup so the next tap controls when
    // movement resumes. Autonomous departures must keep their route running;
    // otherwise collecting one item permanently strands the expedition.
    const bool resumeAutoWalk = state_.exploreRouteAutoWalk;
    state_.exploreRoutePickupAvailable = false;
    state_.exploreRouteAutoWalk = false;
    state_.exploreRoutePlayerWalkActive = false;
    state_.exploreRouteAutoWalk = resumeAutoWalk;

    const ExplorePickupTable& table =
        explorePickupTableForArea(area_);
    if (state_.exploreRoutePickupItem == EXPLORE_PICKUP_COIN) {
        uint32_t coins = GameRandom::range(table.minCoin,
                                           static_cast<uint32_t>(table.maxCoin) + 1);
        gameState.coins += coins;
        effects_.notice = Notice::COINS;
        effects_.coins = coins;
        effects_.save = true;
        return;
    }

    Game::ItemId item = Game::ItemId::COUNT;
    switch (state_.exploreRoutePickupItem) {
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

    if (item == Game::ItemId::COUNT ||
        !Game::ItemInventory::add(gameState, item)) {
        effects_.notice = Notice::BAG_FULL;
        effects_.save = true;
        return;
    }

    effects_.notice = Notice::ITEM;
    effects_.item = item;
    effects_.save = true;
}

bool ExploreSession::beginEncounter(Game::GameState& gameState, uint8_t area,
    bool boss, uint16_t speciesOverride, uint8_t levelOverride,
    uint16_t experiencePercent, ExploreSpecial::Kind specialKind) {
    if (battleActive_ || area >= Game::EXPLORE_AREA_COUNT) return false;
    area_ = area;
    const uint8_t leaderSlot = ExploreSession::leaderSlot(gameState);
    if (leaderSlot >= gameState.teamCount ||
        !healthy(gameState.team[leaderSlot])) {
        effects_.notice = Notice::PET_REST;
        return false;
    }
    const Species* species = nullptr;
    uint8_t levelMinimum = 1;
    uint8_t levelMaximum = Game::LEVEL_MAX;
    if (boss && speciesOverride != 0) {
        species = findSpecies(speciesOverride);
    } else {
        ExploreEncounters::Table table =
            ExploreEncounters::tableForArea(area_);
        if (!table.entries || table.count == 0) return false;
        const ExploreEncounters::Entry* picked = nullptr;
        if (state_.exploreRoutePool.count > 0) {
            const uint32_t totalWeight =
                ExplorePool::poolWeightTotal(state_.exploreRoutePool);
            if (totalWeight == 0) return false;
            const ExplorePool::PoolEntry* poolEntry = ExplorePool::entryForRoll(
                state_.exploreRoutePool, GameRandom::range(0, totalWeight));
            if (!poolEntry) return false;
            for (uint8_t index = 0; index < table.count; ++index) {
                if (table.entries[index].speciesId == poolEntry->speciesId) {
                    picked = &table.entries[index];
                    break;
                }
            }
        } else {
            uint32_t totalWeight = 0;
            for (uint8_t index = 0; index < table.count; ++index) {
                totalWeight += table.entries[index].weight;
            }
            if (totalWeight == 0) return false;
            uint32_t roll = GameRandom::range(0, totalWeight);
            for (uint8_t index = 0; index < table.count; ++index) {
                if (roll < table.entries[index].weight) {
                    picked = &table.entries[index];
                    break;
                }
                roll -= table.entries[index].weight;
            }
        }
        if (!picked) return false;
        species = findSpecies(picked->speciesId);
        levelMinimum = picked->minLevel;
        levelMaximum = picked->maxLevel;
    }
    if (!species) return false;
    uint8_t level = levelOverride;
    if (level == 0) {
        const ExploreMapGenerator::Path& path =
            state_.exploreRouteMap.paths[state_.exploreRoutePath];
        const uint8_t targetLevel = ExploreEncounterRules::targetLevel(
            ExploreAreaCatalog::recommendedLevel(area_),
            ExploreAreaCatalog::depthSpread(area_),
            state_.exploreRouteMapBlock, state_.exploreRouteMapBlockCount,
            state_.exploreRouteIndex, path.pointCount);
        level = ExploreEncounterRules::levelForRoll(
            levelMinimum, levelMaximum, targetLevel,
            static_cast<uint8_t>(GameRandom::range(0, 100)));
    }

    Game::MonsterRuntime wild = Game::MonsterFactory::create(species->id, level);
    wild.origin = Game::Origin::UNKNOWN;
    wild.metArea = area_;
    wild.metAt = gameState.gameMinutesTotal * 60UL;
    wild.lastSeenAt = wild.metAt;

    effects_.encounter = {wild, leaderSlot, boss, experiencePercent, specialKind};
    effects_.startBattle = true;
    state_.exploreRoutePlayerWalkActive = false;
    state_.exploreRouteAutoWalk = false;
    battleActive_ = true;
    return true;
}

bool ExploreSession::recoverTeam(Game::GameState& gameState) {
    if (!ExploreRunRules::isRecoveryStep(state_.exploreRouteSteps)) return false;

    bool recovered = false;
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& monster = gameState.team[slot];
        if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0 ||
            monster.hpCur >= monster.hpMax) {
            continue;
        }
        const uint16_t amount =
            ExploreRunRules::recoveryAmount(monster.hpMax);
        monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
            monster.hpMax,
            static_cast<uint32_t>(monster.hpCur) + amount));
        recovered = true;
    }
    return recovered;
}

void ExploreSession::settleReturn(Game::GameState& gameState) {
    if (returnSettled_) return;
    returnSettled_ = true;

    const uint8_t bondGain = Game::Bond::adventureGain(state_.exploreRouteSteps);
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        Game::MonsterRuntime& monster = gameState.team[slot];
        if (monster.majorStatus != Game::MajorStatus::NONE ||
            monster.majorStatusTurns != 0) {
            monster.majorStatus = Game::MajorStatus::NONE;
            monster.majorStatusTurns = 0;
        }
        if (bondGain == 0 || monster.origin == Game::Origin::VISITOR ||
            monster.fainted || monster.hpCur == 0) {
            continue;
        }
        Game::Bond::Value nextBond = Game::Bond::increase(
            monster.bond, bondGain);
        if (nextBond == monster.bond) continue;
        monster.bond = nextBond;
    }
    state_.exploreItemEffects.reset();
    // Even a short, uneventful route can have unsaved steps and care changes.
    effects_.save = true;
}


bool ExploreSession::beginExit() {
    if (state_.exploreRouteExiting || state_.exploreRouteComplete || state_.exploreRoutePaused || battleActive_ ||
        state_.exploreRoutePath >= state_.exploreRouteMap.pathCount) return false;
    state_.exploreRouteMoving = true;
    state_.exploreRouteExiting = true;
    stopWalking();
    return true;
}
void ExploreSession::finishExit(Game::GameState& gameState) {
    if (!state_.exploreRouteExiting || state_.exploreRoutePaused) return;
    state_.exploreRouteMoving = false;
    state_.exploreRouteExiting = false;
    finishAtEnd(gameState);
}
void ExploreSession::updatePendingFall(Game::GameState& gameState, uint32_t nowMs) {
    if (!state_.exploreRoutePaused && state_.exploreRoutePendingFrostFall &&
        static_cast<int32_t>(nowMs - state_.exploreRouteFrostFallAtMs) >= 0) {
        finishAtEnd(gameState);
        effects_.fullRender = true;
    }
}
void ExploreSession::pause(uint32_t nowMs) {
    if (state_.exploreRoutePaused) return;
    state_.exploreRoutePaused = true;
    state_.exploreRoutePausedAtMs = nowMs;
}
uint32_t ExploreSession::resume(uint32_t nowMs) {
    if (!state_.exploreRoutePaused) return 0;
    const uint32_t duration = nowMs - state_.exploreRoutePausedAtMs;
    if (state_.exploreRoutePendingFrostFall) state_.exploreRouteFrostFallAtMs += duration;
    state_.exploreRoutePaused = false;
    state_.exploreRoutePausedAtMs = 0;
    return duration;
}
void ExploreSession::stopRoute() {
    battleActive_ = false;
    state_.exploreRouteMoving = false;
    state_.exploreRouteExiting = false;
    state_.exploreRoutePaused = false;
    state_.exploreRoutePausedAtMs = 0;
    state_.exploreRouteExitConfirm = false;
    state_.exploreRoutePendingFrostFall = false;
    state_.exploreRouteIceSliding = false;
    stopWalking();
}
void ExploreSession::battleClosed() {
    battleActive_ = false;
    state_.exploreRoutePaused = false;
    stopWalking();
}
void ExploreSession::finishReturn(Game::GameState& gameState) {
    state_.exploreRoutePool = {};
    settleReturn(gameState);
    stopRoute();
    battleActive_ = false;
}
bool ExploreSession::useItem(Game::GameState& gameState, Game::ItemId item) {
    if (item == Game::ItemId::MAX_REPEL) {
        if (state_.exploreItemEffects.repelStepsRemaining() != 0 ||
            !Game::ItemInventory::remove(gameState, item)) return false;
        if (state_.exploreItemEffects.activateMaxRepel()) return true;
    } else if (item == Game::ItemId::HONEY) {
        if (state_.exploreItemEffects.honeyEncounterPending() ||
            !Game::ItemInventory::remove(gameState, item)) return false;
        if (state_.exploreItemEffects.activateHoney()) return true;
    } else return false;
    Game::ItemInventory::add(gameState, item);
    return false;
}
ExploreSession::Effects ExploreSession::takeEffects() {
    const auto effects = effects_;
    effects_ = {};
    return effects;
}


}  // namespace AmoledV1
