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
#include "game/ExploreEncounterRules.h"
#include "game/BathService.h"
#include "game/BattleSystem.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
#include "game/EffortService.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreIceSlide.h"
#include "game/ExploreRunRules.h"
#include "game/ExperienceService.h"
#include "game/FriendshipService.h"
#include "game/FriendshipSystem.h"
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
#include "presentation/PixelRenderer.h"

namespace AmoledV1 {
namespace {

constexpr int TAP_SLOP = 12;
constexpr int DRAG_START_SLOP = 6;
constexpr int MENU_HEADER_HEIGHT = 56;
constexpr uint8_t TEAM_STATUS_PAGE_COUNT = 5;
constexpr int TEAM_STATUS_SNAP_THRESHOLD = 72;
constexpr uint32_t TEAM_STATUS_SLIDE_MS = 180;
constexpr uint32_t TEAM_MOVES_DETAIL_ANIM_MS = 240;
constexpr uint32_t MIND_UPDATE_MS = 400;
constexpr uint32_t MOTION_FRAME_MS = 140;
constexpr uint32_t TURN_PAUSE_MS = 160;
constexpr uint32_t PET_WANDER_RETRY_MIN_MS = 700;
constexpr uint32_t PET_WANDER_RETRY_MAX_MS = 1400;
constexpr uint32_t PET_DEBUG_LOG_INTERVAL_MS = 10000;
constexpr uint32_t PROGRESSION_ANIMATION_FRAME_MS = 50;
constexpr uint32_t LEVEL_UP_ANIMATION_MS = 800;
constexpr uint32_t EVOLUTION_CANCEL_HOLD_MS = 600;
constexpr uint32_t EVOLUTION_TOUCH_GAP_MS = 50;
constexpr uint32_t COMPANION_NIGHT_FOOD_RETRY_MS = 60000UL;
constexpr uint16_t HOME_SLEEP_FRAME_MS = 800;
constexpr uint32_t ATTENTION_INITIAL_MIN_MS = 25000UL;
constexpr uint32_t ATTENTION_INITIAL_MAX_MS = 45000UL;
constexpr uint32_t ATTENTION_MIN_MS = 90000UL;
constexpr uint32_t ATTENTION_MAX_MS = 180000UL;

bool isEvolutionStone(Game::ItemId item) {
    return item == Game::ItemId::FIRE_STONE ||
           item == Game::ItemId::WATER_STONE ||
           item == Game::ItemId::THUNDER_STONE;
}
constexpr uint32_t SPECIAL_ACTION_MIN_MS = 20000UL;
constexpr uint32_t SPECIAL_ACTION_MAX_MS = 40000UL;
constexpr uint32_t ATTENTION_WAIT_MS = 6000UL;
constexpr uint32_t WINDOW_GAZE_MIN_MS = 6000UL;
constexpr uint32_t WINDOW_GAZE_MAX_MS = 10000UL;
constexpr uint32_t WINDOW_GAZE_DELAY_SECONDS =
    2UL * 24UL * 60UL * 60UL;
constexpr uint32_t PAIR_INTERACTION_MIN_INTERVAL_MS = 90000UL;
constexpr uint32_t PAIR_INTERACTION_MAX_INTERVAL_MS = 180000UL;
constexpr uint32_t PAIR_INTERACTION_RETRY_MS = 5000UL;
constexpr uint32_t PAIR_TALK_TIMEOUT_MS = 9000UL;
constexpr uint32_t PAIR_CHASE_TIMEOUT_MS = 24000UL;
constexpr uint16_t PAIR_INVITE_MS = 750;
constexpr uint16_t PAIR_CELEBRATE_MS = 1200;
constexpr uint16_t PAIR_TALK_HOP_MS = 560;
constexpr uint16_t PAIR_TALK_GAP_MS = 180;
constexpr uint16_t PAIR_TALK_END_PAUSE_MS = 240;
constexpr uint16_t PAIR_TALK_TOTAL_MS =
    PAIR_TALK_HOP_MS * 2 + PAIR_TALK_GAP_MS + PAIR_TALK_END_PAUSE_MS;
constexpr uint8_t PAIR_CHASE_LEGS = 4;
constexpr float PAIR_APPROACH_DISTANCE = 20.0f;
constexpr int PAIR_TALK_MIN_GAP_PX = 20;
// The rug is the room's visual stage for conversations. Keep these points
// explicit so the pair does not drift into furniture or crowd the doorway.
constexpr float PAIR_TALK_COMPANION_X = 97.0f;
constexpr float PAIR_TALK_MAIN_X = 149.0f;
constexpr float PAIR_TALK_STAGE_Y = 118.0f;
// Actor x/y is rendered as the sprite rectangle's bottom-center anchor.
// Keep the host close to the doorway while leaving its full footprint inside
// the room walk polygon.
constexpr float VISITOR_WELCOME_OFFSET_X = 10.0f;
constexpr float VISITOR_WELCOME_OFFSET_Y = -5.0f;
constexpr float PAIR_APPROACH_SPEED = 12.5f;
constexpr float PAIR_APPROACH_GOAL_TOLERANCE = 1.0f;
constexpr float PAIR_CHASE_LEADER_SPEED = 18.0f;
constexpr float PAIR_CHASE_FOLLOWER_SPEED = 17.0f;
constexpr float VISITOR_DOOR_SPEED = 18.0f;
constexpr uint32_t VISITOR_MOTION_TIMEOUT_MS = 8000UL;
constexpr uint32_t VISITOR_DOOR_OPEN_PAUSE_MS = 420UL;
constexpr float VISITOR_DOOR_CLEAR_DISTANCE = 28.0f;

bool exploreRouteMonsterHealthy(const Game::MonsterRuntime& monster) {
    return !monster.fainted && monster.hpCur > 0;
}

uint8_t exploreRouteLeaderSlot(const Game::GameState& state) {
    const bool firstHealthy = state.teamCount > 0 &&
        exploreRouteMonsterHealthy(state.team[0]);
    const bool secondHealthy = state.teamCount > 1 &&
        exploreRouteMonsterHealthy(state.team[1]);
    return ExploreRunRules::leaderSlotForHealth(firstHealthy, secondHealthy);
}

bool exploreRouteHasCompanion(const Game::GameState& state) {
    return exploreRouteLeaderSlot(state) == 0 && state.teamCount > 1 &&
           state.team[1].speciesId != 0 &&
           exploreRouteMonsterHealthy(state.team[1]);
}

int pairIdleWidthPx(uint16_t speciesId) {
    PokemonSprites::PetAnimationProfile profile{};
    if (!PokemonSprites::petAnimationProfile(speciesId, profile)) return 80;
    int widest = 0;
    for (uint16_t direction = 0; direction < 4; ++direction) {
        for (uint8_t index = 0; index < profile.idleFrames; ++index) {
            const auto kind = static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(profile.idleBase) +
                direction * profile.idleFrames + index);
            const auto* frame = PokemonSprites::findSpeciesSprite(speciesId, kind);
            if (frame) widest = std::max<int>(
                widest, PokemonSprites::frameVisibleWidth(frame) *
                            AmoledUi::RESOURCE_SCALE);
        }
    }
    return widest > 0 ? widest : 80;
}

float pairRouteDistance(const Home::Actor& actor) {
    float routeDistance = 0.0f;
    float fromX = actor.x;
    float fromY = actor.y;
    for (uint8_t index = actor.route.index;
         index < actor.route.count; ++index) {
        const float toX = actor.route.x[index];
        const float toY = actor.route.y[index];
        routeDistance += std::hypot(toX - fromX, toY - fromY);
        fromX = toX;
        fromY = toY;
    }
    if (actor.route.index >= actor.route.count) {
        routeDistance = std::hypot(
            actor.targetX - actor.x, actor.targetY - actor.y);
    }
    return routeDistance;
}

uint32_t pairApproachBudgetMs(const Home::Actor& actor) {
    const float routeDistance = pairRouteDistance(actor);
    return static_cast<uint32_t>(std::clamp(
        routeDistance / PAIR_APPROACH_SPEED * 1000.0f + 1800.0f,
        4000.0f, 20000.0f));
}
constexpr uint32_t CARE_TICK_MS = 60000;
constexpr uint32_t PERIODIC_SAVE_MS = 5UL * 60UL * 1000UL;
constexpr uint16_t EXPLORE_ROUTE_STEP_MS = 360;
constexpr uint16_t EXPLORE_ROUTE_FRAME_MS = 90;
constexpr uint16_t EXPLORE_ROUTE_FOLLOWER_DELAY_MS = 120;
constexpr uint8_t EXPLORE_ROUTE_FOLLOWER_GAP_STEPS = 2;
constexpr uint32_t EXPLORE_DOOR_PHASE_TIMEOUT_MS = 8000;
constexpr float EXPLORE_DOOR_CROSS_SPEED = 42.0f;
constexpr float EXPLORE_DOOR_CLEAR_SPEED = 30.0f;
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
constexpr int EXPLORE_ROUTE_VIEW_HEIGHT = HOME_STATUS_TOP;
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
// During the carousel only the three preview sprites change. Keep their
// invalidation band separate from the full page used by area changes.
constexpr uint16_t EXPLORE_PREVIEW_RENDER_TOP = EXPLORE_PREVIEW_TOP;
constexpr uint16_t EXPLORE_PREVIEW_RENDER_BOTTOM = EXPLORE_PREVIEW_BOTTOM;
constexpr uint16_t BATTLE_ANIMATION_RENDER_END = 384;
constexpr uint16_t BATTLE_LOG_DEFAULT_MS = 700;
constexpr uint16_t BATTLE_ATTACK_LOG_MS = 700;
constexpr uint16_t BATTLE_RESULT_LOG_MS = 650;
constexpr uint32_t BATTLE_HP_ANIMATION_MS = 420;
constexpr uint32_t BATTLE_HP_DAMAGE_DELAY_MS = 280;
constexpr uint32_t BATTLE_EXP_ANIMATION_MS = 900;
constexpr uint32_t BATTLE_GAUGE_FRAME_MS = 40;
constexpr uint32_t BATTLE_SWITCH_PHASE_MS = 360;
constexpr uint32_t BATTLE_SWITCH_FRAME_MS = 40;
constexpr int BATTLE_SWITCH_TRAVEL_X = 180;
constexpr float FALLBACK_ROOM_MIN_X = 56.0f;
constexpr float FALLBACK_ROOM_MAX_X = 122.0f;
constexpr float FALLBACK_ROOM_MIN_Y = 126.0f;
constexpr float FALLBACK_ROOM_MAX_Y = 151.0f;
constexpr float FALLBACK_FOOD_APPROACH_X = 121.0f;
constexpr float FALLBACK_FOOD_APPROACH_Y = 149.0f;
constexpr uint32_t MOOD_BURST_DURATION_MS = 420;
constexpr uint32_t MOOD_BURST_FRAME_MS = 50;
constexpr uint32_t FEED_FINISH_MS = 650;
#if STICKMON_ENABLE_DEBUG_FEATURES
constexpr float DEBUG_TILT_DEADZONE = 0.08f;
constexpr float DEBUG_TILT_MAX = 0.62f;
constexpr float DEBUG_TILT_SPEED = 58.0f;
#endif

#if STICKMON_HAS_CLAW
// Render-time snapshot of the shared ClawStatusLog. Keep this copy in .bss so
// rendering the Claw log does not add another large automatic buffer.
Stickmon::ClawStatusLog::Entry s_clawLogEntries[Stickmon::ClawStatusLog::CAPACITY];
#endif

// Keep the FIFO out of AmoledApp so neither app_main nor callers that create
// a temporary app object need to carry the 1.5 KiB queue in their stack frame.
constexpr uint8_t BATTLE_LOG_QUEUE_CAP = 24;
constexpr uint8_t BATTLE_LOG_LEN = 64;
char s_battleLogQueue[BATTLE_LOG_QUEUE_CAP][BATTLE_LOG_LEN] = {};
uint16_t s_battleLogDurations[BATTLE_LOG_QUEUE_CAP] = {};

uint8_t moodHeartCountFor(uint8_t mood) {
    return std::min<uint8_t>(5, static_cast<uint8_t>(mood / 20));
}

constexpr float FOOD_FEED_OFFSET_X = 12.0f;
constexpr float FOOD_FEED_OFFSET_Y = 4.0f;
constexpr float CAMERA_SAFE_LEFT = 62.0f;
constexpr float CAMERA_SAFE_RIGHT = 122.0f;
constexpr float CAMERA_SAFE_TOP = 58.0f;
constexpr float CAMERA_SAFE_BOTTOM = 126.0f;
constexpr int SHOWER_BODY_LEFT = 84;
constexpr int SHOWER_BODY_RIGHT = 284;
constexpr int SHOWER_BODY_TOP = 96;
constexpr int SHOWER_BODY_BOTTOM = 316;
constexpr int SHOWER_PET_X = 184;
constexpr int SHOWER_PET_Y = 212;
constexpr int SHOWER_TOOL_MIN_X = 24;
constexpr int SHOWER_TOOL_MAX_X = 344;
constexpr int SHOWER_TOOL_MIN_Y = 72;
constexpr int SHOWER_TOOL_MAX_Y = 420;
constexpr float SHOWER_FOAM_DISTANCE = 24.0f;
constexpr float SHOWER_BRUSH_DISTANCE = 8.0f;
constexpr uint32_t SHOWER_BRUSH_INTERVAL_MS = 110;
constexpr uint8_t SHOWER_FOAM_CAP = 8;

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

constexpr bool windowGazeDue(uint32_t lastExploredAt,
                             uint32_t lastWindowGazeAt,
                             uint32_t nowGameSeconds) {
    return lastWindowGazeAt <= lastExploredAt &&
           nowGameSeconds >= lastExploredAt &&
           nowGameSeconds - lastExploredAt >=
               WINDOW_GAZE_DELAY_SECONDS;
}

static_assert(
    !windowGazeDue(100, 100,
                   100 + WINDOW_GAZE_DELAY_SECONDS - 1) &&
        windowGazeDue(100, 100,
                      100 + WINDOW_GAZE_DELAY_SECONDS) &&
        !windowGazeDue(100, 101,
                       100 + WINDOW_GAZE_DELAY_SECONDS),
    "window gaze runs once after two days without exploration");

int16_t gHomeNavParent[RoomNavigator::MAX_NODES] = {};
uint16_t gHomeNavQueue[RoomNavigator::MAX_NODES] = {};

float speciesGroundOffset(uint16_t speciesId) {
    PokemonSprites::PetAnimationProfile profile{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    if (PokemonSprites::petAnimationProfile(speciesId, profile)) {
        frame = PokemonSprites::findSpeciesSprite(speciesId, profile.idleBase);
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    }
    return frame
        ? static_cast<float>(PokemonSprites::frameGroundOffsetY(frame))
        : 18.0f;
}

PokemonSprites::WalkDirection mainDirectionFromView(uint8_t direction) {
    if (direction == 3 || direction == 4 || direction == 5) {
        return PokemonSprites::WalkDirection::UP;
    }
    if (direction == 1 || direction == 2) {
        return PokemonSprites::WalkDirection::LEFT;
    }
    if (direction == 6 || direction == 7) {
        return PokemonSprites::WalkDirection::RIGHT;
    }
    return PokemonSprites::WalkDirection::DOWN;
}

uint8_t mainDirectionForView(PokemonSprites::WalkDirection direction) {
    switch (direction) {
    case PokemonSprites::WalkDirection::LEFT: return 2;
    case PokemonSprites::WalkDirection::UP: return 4;
    case PokemonSprites::WalkDirection::RIGHT: return 6;
    case PokemonSprites::WalkDirection::DOWN:
    default: return 0;
    }
}

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

bool battleEffectsSucceeded(const BattleSystem::EffectResolution& effects) {
    for (uint8_t index = 0; index < effects.count; ++index) {
        if (effects.outcomes[index].kind !=
            BattleSystem::EffectOutcomeKind::STATUS_FAILED) {
            return true;
        }
    }
    return false;
}

void formatBattleOutcome(char* output, size_t outputSize,
                         const BattleSystem::DamageResult& damage,
                         uint16_t dealt,
                         const BattleSystem::EffectResolution& effects,
                         const MoveInfo* move, bool attackerWild) {
    if (damage.failed) {
        std::snprintf(output, outputSize, "%s", Ui::Explore::MOVE_FAILED);
    } else if (damage.missed) {
        std::snprintf(output, outputSize, "%s", Ui::Explore::MOVE_MISSED);
    } else if (damage.effectiveness == 0) {
        std::snprintf(output, outputSize, "%s", Ui::Explore::NO_EFFECT);
    } else if (dealt > 0) {
        std::snprintf(output, outputSize,
                      attackerWild ? Ui::Explore::WILD_DAMAGE_FMT
                                   : Ui::Explore::DAMAGE_FMT,
                      dealt);
    } else if (move && battleEffectsSucceeded(effects)) {
        // The move-used line is queued separately before this result. A
        // successful status move has no additional damage/result text here.
        output[0] = '\0';
    } else {
        std::snprintf(output, outputSize, "%s", Ui::Explore::NO_EFFECT);
    }
}

void formatBattleMoveUsed(char* output, size_t outputSize,
                          const Species& attackerSpecies,
                          const MoveInfo* move, bool attackerWild) {
    const char* moveName = move ? move->name : Ui::Status::MOVE_UNKNOWN;
    if (attackerWild) {
        std::snprintf(output, outputSize, Ui::Explore::WILD_MOVE_USED_FMT,
                      moveName);
    } else {
        std::snprintf(output, outputSize, Ui::Explore::MOVE_USED_FMT,
                      attackerSpecies.name, moveName);
    }
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

PokemonSprites::WalkDirection rotatePetDirection(
    PokemonSprites::WalkDirection direction, int8_t step) {
    int value = static_cast<int>(direction) + step;
    while (value < 0) value += 4;
    while (value >= 4) value -= 4;
    return static_cast<PokemonSprites::WalkDirection>(value);
}

GameAssets::Kind battleBackgroundForArea(uint8_t area) {
    return ExploreAreaCatalog::battleBackground(area);
}

ExplorePool::Pool buildExplorePreviewPool(const Game::GameState& state,
                                          uint8_t area) {
    return ExploreEncounters::poolForArea(
        area, state.gameMinutesTotal,
        area < Game::EXPLORE_AREA_COUNT
            ? state.explorePoolRerollCounts[area] : 0);
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

bool repairZeroIndividualValues(Game::MonsterRuntime& monster) {
    if (monster.ivPacked != 0) return false;

    const Species* species = findSpecies(monster.speciesId);
    if (!species) return false;
    const uint16_t oldMax = monster.hpMax;
    const uint16_t oldCurrent = monster.hpCur;
    Game::MonsterFactory::rollIndividualValues(monster);
    monster.hpMax = maxHpFor(*species, monster);
    if (monster.fainted || oldCurrent == 0) {
        monster.hpCur = 0;
    } else if (monster.hpMax > oldMax) {
        monster.hpCur = static_cast<uint16_t>(std::min<uint32_t>(
            monster.hpMax,
            static_cast<uint32_t>(oldCurrent) + monster.hpMax - oldMax));
    } else {
        monster.hpCur = std::min<uint16_t>(oldCurrent, monster.hpMax);
    }
    return true;
}

uint8_t repairOwnedZeroIndividualValues(Game::GameState& state) {
    uint8_t repaired = 0;
    for (uint8_t slot = 0;
         slot < state.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (repairZeroIndividualValues(state.team[slot])) ++repaired;
    }
    for (uint8_t slot = 0;
         slot < state.storageCount && slot < Game::STORAGE_CAP; ++slot) {
        if (repairZeroIndividualValues(state.storage[slot])) ++repaired;
    }
    return repaired;
}

}  // namespace

void AmoledApp::setMusicContext(MusicContext context) {
    if (musicContext == context) return;
    musicContext = context;

    MusicTrack track = MusicTrack::HOME;
    switch (context) {
    case MusicContext::EXPLORE:
        track = MusicTrack::EXPLORE;
        break;
    case MusicContext::BATTLE:
        track = MusicTrack::BATTLE;
        break;
    case MusicContext::BATTLE_SPECIAL:
        track = MusicTrack::BATTLE_SPECIAL;
        break;
    case MusicContext::HOME:
    default:
        break;
    }
    AudioManager::ins().setMusic(track);
}

void AmoledApp::startVisitHost() {
    if (gameState.teamCount == 1 && !visitRadioExclusive) {
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime::instance().enterPeerSession();
#endif
        visitRadioExclusive = true;
    }
    visitSession.startHost();
    if (visitSession.state() ==
        Communication::VisitSessionService::State::FAILED) {
        releaseVisitRadioSession();
    }
}

void AmoledApp::startVisitSearch() {
    if (gameState.teamCount > 0 && !visitRadioExclusive) {
#if STICKMON_HAS_CLAW
        Stickmon::ClawRuntime::instance().enterPeerSession();
#endif
        visitRadioExclusive = true;
    }
    visitSession.startSearch();
    if (visitSession.state() ==
        Communication::VisitSessionService::State::FAILED) {
        releaseVisitRadioSession();
    }
}

void AmoledApp::releaseVisitRadioSession() {
    if (!visitRadioExclusive) return;
    EspNowLink::ins().end();
#if STICKMON_HAS_CLAW
    Stickmon::ClawRuntime::instance().leavePeerSession();
#endif
    visitRadioExclusive = false;
}

void AmoledApp::begin(uint32_t nowMs) {
    renderCaches_.release();
    expeditionFade.reset();
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    pendingExpedition = false;
    expeditionMainHidden = false;
    expeditionCompanionHidden = false;
    GameRandom::seed(Platform::power().hardwareRandom() ^ nowMs);
    visitSession.attach(&gameState);
    visitRadioExclusive = false;
    storageReady = saveManager.begin();
    bool normalized = false;
    bool loaded = storageReady &&
                  saveManager.load(gameState, mainViewState, &normalized);
    if (!loaded) {
        gameState = Game::GameState{};
        gameState.oobeDone = true;
        gameState.team[0] = Game::MonsterFactory::create(
            gameState.team[0].speciesId, gameState.team[0].level);
        mainViewState = MainSceneViewState{};
    } else {
        const uint8_t repaired = repairOwnedZeroIndividualValues(gameState);
        if (repaired > 0) {
            normalized = true;
            Platform::logf(
                "[AmoledApp] repaired zero individual values: %u monster(s)\n",
                repaired);
        }
    }

    bool normalizedEncounterHistory = false;
    bool loadedEncounterHistory = storageReady &&
        saveManager.loadEncounterHistory(
            encounterHistory, &normalizedEncounterHistory);
    if (!loadedEncounterHistory) encounterHistory.clear();
    encounterHistoryDirty = normalizedEncounterHistory ||
                            syncOwnedSpeciesToEncounterHistory();

#if STICKMON_ENABLE_DEBUG_FEATURES
    debugTiltControl =
        (gameState.debugMotionFlags & Game::DEBUG_MOTION_TILT) != 0;
    debugWalkBoundaryVisible =
        (gameState.debugMotionFlags & Game::DEBUG_MOTION_WALK_BOUNDARY) != 0;
    debugTalkPointsVisible =
        (gameState.debugMotionFlags & Game::DEBUG_MOTION_TALK_POINTS) != 0;
#endif

    if (storageReady && (!loaded || normalized || encounterHistoryDirty)) {
        saveState();
    }
    uint16_t teamSpecies[Game::TEAM_CAP] = {};
    uint8_t teamCount = std::min<uint8_t>(gameState.teamCount,
                                          Game::TEAM_CAP);
    for (uint8_t slot = 0; slot < teamCount; ++slot) {
        teamSpecies[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(teamSpecies, teamCount);
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
    petScheduledSleeping = false;
    roomAction = RoomAction::NONE;
    roomActionPhase = 0;
    roomActionStartedMs = 0;
    roomActionUntilMs = 0;
    petFrame = 0;
    nextPetFrameMs = nowMs + 520;
    monsterMind.reset(nowMs);
    schedulePairInteraction(nowMs);
    if (const Species* species = findSpecies(gameState.team[0].speciesId)) {
        behaviorProfile = behaviorProfileFor(*species, gameState.team[0]);
    }
    scheduleAttention(nowMs, true);
    scheduleSpecialAction(nowMs);
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
    if (!petResting && mainViewState.valid &&
        mainViewState.speciesId == gameState.team[0].speciesId) {
        const Game::MonsterRuntime& savedMonster = gameState.team[0];
        const Game::SpeciesCareProfile savedCare =
            Game::speciesCareProfileFor(savedMonster.speciesId);
        const bool savedSleepTime = savedCare.usesBed &&
            Game::isSleepCareTime(gameState.gameMinutesTotal,
                                  savedMonster.nature);
        const float groundOffset = speciesGroundOffset(
            gameState.team[0].speciesId);
        petX = mainViewState.monsterX;
        petY = mainViewState.monsterY + groundOffset;
        petTargetX = mainViewState.targetX;
        petTargetY = mainViewState.targetY + groundOffset;
        petDirection = mainDirectionFromView(mainViewState.pmdDirection);
        petFrame = mainViewState.pmdFrame;
        if (mainViewState.aiMode == 1) {
            petMotion = PetMotion::WANDERING;
        } else if (mainViewState.aiMode == 3) {
            if (savedCare.needsFood && gameState.room.bowlCount > 0 &&
                savedMonster.satiety < MONSTER_FEED_TARGET_SATIETY) {
                petMotion = PetMotion::SEEKING_FOOD;
            }
        } else if (mainViewState.aiMode == 4) {
            if (savedSleepTime) petMotion = PetMotion::SEEKING_SLEEP;
        } else if (mainViewState.aiMode == 8 && savedSleepTime) {
            petMotion = PetMotion::SLEEPING;
            petScheduledSleeping = true;
            petResting = true;
        }
        if (petMotion == PetMotion::IDLE) {
            petTargetX = petX;
            petTargetY = petY;
            petFrame = 0;
        }
        nextPetDecisionMs = nowMs +
            mainViewState.nextDecisionRemainingMs;
    }
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
    syncHomeActors(nowMs);
    if (petMotion == PetMotion::WANDERING) {
        homeRuntime.transition(0, Home::Task::WANDER, nowMs, 0, true);
    } else if (petMotion == PetMotion::SEEKING_FOOD) {
        homeRuntime.transition(0, Home::Task::SEEK_FOOD, nowMs, 0, true);
        homeRuntime.acquire(Home::Resource::BOWL, 0,
                            Home::Task::SEEK_FOOD, nowMs);
    } else if (petMotion == PetMotion::SEEKING_SLEEP ||
               petMotion == PetMotion::SLEEPING) {
        homeRuntime.transition(
            0, petMotion == PetMotion::SLEEPING
                   ? Home::Task::SLEEPING : Home::Task::SEEK_SLEEP,
            nowMs, 0, true);
        homeRuntime.acquire(Home::Resource::BED, 0,
                            petMotion == PetMotion::SLEEPING
                                ? Home::Task::SLEEPING
                                : Home::Task::SEEK_SLEEP,
                            nowMs);
    }
    if ((petMotion == PetMotion::WANDERING ||
         petMotion == PetMotion::SEEKING_FOOD ||
         petMotion == PetMotion::SEEKING_SLEEP) &&
        !homeRuntime.planRoute(
            0, petTargetX, petTargetY, false,
            homeCompanionActor.active)) {
        petMotion = PetMotion::IDLE;
        petTargetX = petX;
        petTargetY = petY;
        homeRuntime.stop(0, nowMs, 700);
    }
    updateCamera();
    Platform::logf("[AmoledApp] begin: camera ready\n");
    if (nextPetDecisionMs == 0) schedulePetDecision(nowMs);
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
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE ||
        roomAction != RoomAction::NONE || homeRuntime.pairActive();
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
    if (expeditionFade.active()) return false;
    expeditionFade.reset();
    pendingExpeditionArea = area;
    pendingExpeditionAutoWalk = autoWalk;
    pendingExpedition = true;
    exploreSessionActive = true;
    expeditionMainHidden = false;
    expeditionCompanionDeparting = false;
    expeditionCompanionHidden = false;
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
    const uint32_t nowMs = Platform::clock().millis();
    lockRequested = false;
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    // Attach/initialize the companion actor now. The departure phase machine
    // short-circuits updatePet (and therefore syncHomeActors), so without
    // this a freshly joined visitor is never active when the departure
    // evaluates expeditionCompanionDeparting and never walks to the door.
    syncHomeActors(nowMs);
    if (visitorMotion == VisitorMotion::HOST_APPROACH ||
        visitorMotion == VisitorMotion::ENTERING) {
        // A mid-entry visitor joins the departure from wherever they are.
        visitorMotion = VisitorMotion::NONE;
        visitorCrossingDoor = false;
        visitorMotionUntilMs = 0;
        homeRuntime.release(Home::Resource::DOOR, 0);
        homeRuntime.release(Home::Resource::DOOR, 1);
    }
    lastInteractionMs = nowMs;
    // The departure animation always starts from the room, regardless of the
    // page that was visible when the request arrived.
    sceneFlow.goHome();
    requestFullRender();
    return true;
}

#if STICKMON_ENABLE_DEBUG_FEATURES
bool AmoledApp::debugStartExplore(uint8_t area) {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        pendingExpedition || expeditionDeparturePhase != ExpeditionDeparturePhase::NONE ||
        gameState.teamCount == 0 || gameState.team[0].fainted ||
        gameState.team[0].hpCur == 0) return false;
    return queueExploreDeparture(area, true);
}

bool AmoledApp::debugTriggerContact(uint8_t kind) {
    if (kind < 1 || kind > 3 ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    const uint32_t nowMs = Platform::clock().millis();
    if (!beginDebugContactEvent(kind, nowMs)) return false;
    onWake(nowMs);
    acceptDebugContact(nowMs, true);
    return debugContactActive;
}

bool AmoledApp::debugPromptContact(uint8_t kind) {
    if (kind < 1 || kind > 3 ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    const uint32_t nowMs = Platform::clock().millis();
    if (!beginDebugContactEvent(kind, nowMs)) return false;
    onWake(nowMs);
    return true;
}

bool AmoledApp::debugAcceptContact() {
    if (!debugContactPending ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME) return false;
    acceptDebugContact(Platform::clock().millis(), true);
    return debugContactActive;
}

bool AmoledApp::debugStartPairTalk() {
    if (!debugContactActive ||
        sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        visitorMotion != VisitorMotion::ACTIVE) return false;
    const uint32_t nowMs = Platform::clock().millis();
    return startPairInteraction(nowMs, false, true);
}

bool AmoledApp::debugReturnContactExplore() {
    if (!debugContactActive || debugContactKind != 3 ||
        sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) return false;
    Platform::logf("[FriendDiag] return requested kind=3 team=%u\n",
                   static_cast<unsigned>(gameState.teamCount));
    leaveExploreRoute();
    return expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_FADE_OUT;
}
#endif

void AmoledApp::cancelExploreDeparture() {
    expeditionFade.reset();
    if (expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_COMPANION_DOOR) {
        petX = expeditionDoorInsideX;
        petY = expeditionDoorInsideY;
    }
    expeditionMainHidden = false;
    expeditionCompanionHidden = false;
    if (expeditionCompanionDeparting) {
        homeRuntime.stop(1, Platform::clock().millis());
    }
    expeditionCompanionDeparting = false;
    pendingExpedition = false;
    exploreSessionActive = false;
    expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
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
    expeditionFade.update(nowMs);
    const bool departingFromRoom =
        expeditionDeparturePhase == ExpeditionDeparturePhase::NONE ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_COMPANION_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::FADE_OUT;
    if (departingFromRoom && sceneFlow.current() != AppSceneFlow::Scene::HOME) {
        sceneFlow.goHome();
        requestFullRender();
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_FADE_OUT) {
        if (!expeditionFade.complete()) {
            requestFullRender();
            return true;
        }
        completeExploreReturn();
        expeditionMainHidden = false;
        expeditionCompanionHidden = false;
        RoomResource& room = RoomResource::ins();
        const float outsideX = room.available()
            ? static_cast<float>(room.doorwayOutsideX()) : petX;
        const float outsideY = room.available()
            ? static_cast<float>(room.doorwayOutsideY()) : petY;
        expeditionDoorInsideX = room.available()
            ? static_cast<float>(room.doorwayInsideX()) : petX;
        expeditionDoorInsideY = room.available()
            ? static_cast<float>(room.doorwayInsideY()) : petY;
        const RoomResource::Point* walkPolygon = room.available()
            ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
        const uint8_t walkPolygonCount = room.available()
            ? room.walkPolygonCount()
            : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                                   sizeof(FALLBACK_WALK_POLYGON[0]));
        auto chooseInsidePose = [&](const Home::Actor& actor,
                                    float& x, float& y) {
            auto valid = [&](float candidateX, float candidateY) {
                return RoomMovementArea::containsFootprint(
                    walkPolygon, walkPolygonCount,
                    candidateX, candidateY,
                    actor.geometry.footprint);
            };
            if (valid(x, y)) return true;
            const float anchorX = x;
            const float anchorY = y;
            for (int radius = 2; radius <= 48; radius += 2) {
                for (int offset = -radius; offset <= radius; offset += 2) {
                    const float candidates[][2] = {
                        {anchorX + radius, anchorY + offset},
                        {anchorX - radius, anchorY + offset},
                        {anchorX + offset, anchorY + radius},
                        {anchorX + offset, anchorY - radius},
                    };
                    for (const auto& candidate : candidates) {
                        if (!valid(candidate[0], candidate[1])) continue;
                        x = candidate[0];
                        y = candidate[1];
                        return true;
                    }
                }
            }
            return false;
        };
        chooseInsidePose(homeMainActor,
                         expeditionDoorInsideX,
                         expeditionDoorInsideY);
        const bool mainFainted = gameState.team[0].fainted ||
                                 gameState.team[0].hpCur == 0;
        if (!mainFainted) {
            petResting = false;
            petX = outsideX;
            petY = outsideY;
            petTargetX = expeditionDoorInsideX;
            petTargetY = expeditionDoorInsideY;
            petDirection = petDirectionForDelta(
                petTargetX - petX, petTargetY - petY);
            homeMainActor.x = petX;
            homeMainActor.y = petY;
            homeMainActor.targetX = petTargetX;
            homeMainActor.targetY = petTargetY;
            homeMainActor.route.clear();
            homeMainActor.velocityX = 0.0f;
            homeMainActor.velocityY = 0.0f;
            homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
        }
        const bool hasCompanion = gameState.teamCount > 1 &&
            homeCompanionActor.active && gameState.team[1].speciesId != 0 &&
            !gameState.team[1].fainted && gameState.team[1].hpCur > 0;
        if (hasCompanion) {
            expeditionCompanionDoorInsideX = room.available()
                ? static_cast<float>(room.doorwayInsideX())
                : expeditionDoorInsideX;
            expeditionCompanionDoorInsideY = room.available()
                ? static_cast<float>(room.doorwayInsideY())
                : expeditionDoorInsideY;
            chooseInsidePose(homeCompanionActor,
                             expeditionCompanionDoorInsideX,
                             expeditionCompanionDoorInsideY);
            homeCompanionActor.x = outsideX;
            homeCompanionActor.y = outsideY;
            homeCompanionActor.targetX = expeditionCompanionDoorInsideX;
            homeCompanionActor.targetY = expeditionCompanionDoorInsideY;
            homeCompanionActor.route.clear();
            homeCompanionActor.velocityX = 0.0f;
            homeCompanionActor.velocityY = 0.0f;
            homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
            companionDirection = petDirectionForDelta(
                expeditionCompanionDoorInsideX - outsideX,
                expeditionCompanionDoorInsideY - outsideY);
            companionFrame = 0;
            nextCompanionFrameMs = nowMs;
        }
        expeditionCompanionDeparting = hasCompanion;
        placeFaintedHomeActors(nowMs);
        // Stick serializes the doorway: keep the companion hidden outside
        // until the leader has entered and cleared the door.
        expeditionCompanionHidden = hasCompanion;
        if (!mainFainted) {
            petMotion = PetMotion::WANDERING;
            petFrame = 0;
        }
        expeditionDeparturePhase = ExpeditionDeparturePhase::RETURN_FADE_IN;
        expeditionDoorPhaseStartedMs = nowMs;
        expeditionFade.beginIn();
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_WALK_IN) {
        const uint32_t elapsedMs = std::min<uint32_t>(nowMs - lastPetUpdateMs, 120);
        lastPetUpdateMs = nowMs;
        const float step = EXPLORE_DOOR_CROSS_SPEED * (elapsedMs / 1000.0f);

        auto moveTowardsDoor = [step](float& x, float& y,
                                       float targetX, float targetY) {
            const float dx = targetX - x;
            const float dy = targetY - y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance <= 0.8f || step >= distance) {
                x = targetX;
                y = targetY;
                return true;
            }
            if (step > 0.0f) {
                x += dx / distance * step;
                y += dy / distance * step;
            }
            return false;
        };

        bool mainArrived = moveTowardsDoor(
            petX, petY, petTargetX, petTargetY);
        if (!mainArrived &&
            nowMs - expeditionDoorPhaseStartedMs >=
                EXPLORE_DOOR_PHASE_TIMEOUT_MS) {
            petX = petTargetX;
            petY = petTargetY;
            mainArrived = true;
            Platform::logLine(
                "[AmoledExplore] return leader cross timeout fallback");
        }
        homeMainActor.x = petX;
        homeMainActor.y = petY;
        homeMainActor.targetX = petTargetX;
        homeMainActor.targetY = petTargetY;
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (mainArrived && !expeditionCompanionDeparting) {
            petMotion = PetMotion::IDLE;
            homeRuntime.stop(0, nowMs, 700);
            expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
            exploreSessionActive = false;
            lastInteractionMs = nowMs;
            monsterMind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            Platform::logf(
                "[AmoledExplore] return entered main=%.1f,%.1f companion=0\n",
                petX, petY);
        } else if (mainArrived) {
            float inwardX = expeditionDoorInsideX - expeditionDoorOutsideX;
            float inwardY = expeditionDoorInsideY - expeditionDoorOutsideY;
            const float inwardLength = std::sqrt(
                inwardX * inwardX + inwardY * inwardY);
            if (inwardLength > 0.001f) {
                inwardX /= inwardLength;
                inwardY /= inwardLength;
            } else {
                inwardX = 0.0f;
                inwardY = -1.0f;
            }
            const float tangentX = -inwardY;
            const float tangentY = inwardX;
            const float separation = std::max(
                26.0f,
                homeMainActor.geometry.footprint.radiusX +
                    homeCompanionActor.geometry.footprint.radiusX + 8.0f);
            float clearX = petX;
            float clearY = petY;
            bool clearTargetFound = false;
            static constexpr float INWARD_OFFSETS[] = {14.0f, 20.0f, 28.0f};
            static constexpr float SIDE_SCALES[] = {1.0f, -1.0f, 1.25f, -1.25f};
            for (float inwardOffset : INWARD_OFFSETS) {
                for (float sideScale : SIDE_SCALES) {
                    const float candidateX = expeditionDoorInsideX +
                        inwardX * inwardOffset +
                        tangentX * separation * sideScale;
                    const float candidateY = expeditionDoorInsideY +
                        inwardY * inwardOffset +
                        tangentY * separation * sideScale;
                    if (!petFootprintInsideWalkArea(candidateX, candidateY)) {
                        continue;
                    }
                    clearX = candidateX;
                    clearY = candidateY;
                    clearTargetFound = true;
                    break;
                }
                if (clearTargetFound) break;
            }
            if (!clearTargetFound) {
                for (uint8_t attempt = 0; attempt < 8; ++attempt) {
                    float candidateX = petX;
                    float candidateY = petY;
                    if (!chooseWanderTarget(candidateX, candidateY, false)) {
                        continue;
                    }
                    const float dx = candidateX - expeditionDoorInsideX;
                    const float dy = candidateY - expeditionDoorInsideY;
                    if (dx * dx + dy * dy < separation * separation) continue;
                    clearX = candidateX;
                    clearY = candidateY;
                    clearTargetFound = true;
                    break;
                }
            }
            const bool clearRouteReady = clearTargetFound &&
                homeRuntime.planRoute(0, clearX, clearY, true, false) &&
                homeRuntime.transitionPreparedRoute(
                    0, Home::Task::DOOR_ACTION, nowMs, 0, true);
            petTargetX = clearX;
            petTargetY = clearY;
            expeditionDoorPhaseStartedMs = nowMs;
            if (clearRouteReady) {
                expeditionDeparturePhase =
                    ExpeditionDeparturePhase::RETURN_CLEAR_MAIN;
            } else {
                if (clearTargetFound) {
                    petX = clearX;
                    petY = clearY;
                    homeMainActor.x = clearX;
                    homeMainActor.y = clearY;
                }
                homeRuntime.stop(0, nowMs, 700);
                petMotion = PetMotion::IDLE;
                expeditionCompanionHidden = false;
                expeditionDeparturePhase =
                    ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN;
                Platform::logLine(
                    "[AmoledExplore] return leader clearance fallback");
            }
        }
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_CLEAR_MAIN) {
        const uint32_t elapsedMs = std::min<uint32_t>(
            nowMs - lastPetUpdateMs, 120);
        lastPetUpdateMs = nowMs;
        const float previousX = homeMainActor.x;
        const float previousY = homeMainActor.y;
        Home::RouteStep routeStep = homeRuntime.advanceRoute(
            0, nowMs, EXPLORE_DOOR_CLEAR_SPEED, elapsedMs / 1000.0f,
            1.0f, false);
        if (routeStep == Home::RouteStep::BLOCKED ||
            routeStep == Home::RouteStep::NO_ROUTE ||
            nowMs - expeditionDoorPhaseStartedMs >=
                EXPLORE_DOOR_PHASE_TIMEOUT_MS) {
            homeMainActor.x = petTargetX;
            homeMainActor.y = petTargetY;
            homeMainActor.route.clear();
            routeStep = Home::RouteStep::ARRIVED;
            Platform::logLine(
                "[AmoledExplore] return leader clearance timeout fallback");
        }
        petX = homeMainActor.x;
        petY = homeMainActor.y;
        if (std::fabs(petX - previousX) > 0.01f ||
            std::fabs(petY - previousY) > 0.01f) {
            petDirection = petDirectionForDelta(
                petX - previousX, petY - previousY);
        }
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (routeStep == Home::RouteStep::ARRIVED) {
            homeRuntime.stop(0, nowMs, 700);
            petMotion = PetMotion::IDLE;
            expeditionCompanionHidden = false;
            expeditionDeparturePhase =
                ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN;
            expeditionDoorPhaseStartedMs = nowMs;
        }
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase ==
        ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN) {
        const uint32_t elapsedMs = std::min<uint32_t>(
            nowMs - lastPetUpdateMs, 120);
        lastPetUpdateMs = nowMs;
        const float step = EXPLORE_DOOR_CROSS_SPEED *
            (elapsedMs / 1000.0f);
        const float dx = homeCompanionActor.targetX - homeCompanionActor.x;
        const float dy = homeCompanionActor.targetY - homeCompanionActor.y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        bool arrived = distance <= 0.8f || step >= distance;
        if (!arrived &&
            nowMs - expeditionDoorPhaseStartedMs >=
                EXPLORE_DOOR_PHASE_TIMEOUT_MS) {
            arrived = true;
            Platform::logLine(
                "[AmoledExplore] return companion cross timeout fallback");
        }
        if (arrived) {
            homeCompanionActor.x = homeCompanionActor.targetX;
            homeCompanionActor.y = homeCompanionActor.targetY;
        } else if (distance > 0.0f && step > 0.0f) {
            homeCompanionActor.x += dx / distance * step;
            homeCompanionActor.y += dy / distance * step;
            companionDirection = petDirectionForDelta(dx, dy);
        }
        homeCompanionActor.velocityX = 0.0f;
        homeCompanionActor.velocityY = 0.0f;
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            companionFrame = static_cast<uint8_t>((companionFrame + 1) % 3);
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
        }
        if (arrived) {
            stopCompanion(nowMs, 700);
            expeditionCompanionDeparting = false;
            expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
            exploreSessionActive = false;
            lastInteractionMs = nowMs;
            monsterMind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            schedulePairInteraction(nowMs, true);
            Platform::logf(
                "[AmoledExplore] return entered main=%.1f,%.1f companion=1\n",
                petX, petY);
        }
        updateCamera();
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_FADE_IN) {
        if (expeditionFade.complete()) {
            expeditionFade.reset();
            const bool mainFainted = gameState.team[0].fainted ||
                                     gameState.team[0].hpCur == 0;
            if (mainFainted) {
                expeditionCompanionHidden = false;
                expeditionDeparturePhase = expeditionCompanionDeparting
                    ? ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN
                    : ExpeditionDeparturePhase::NONE;
                if (!expeditionCompanionDeparting) {
                    exploreSessionActive = false;
                    lastInteractionMs = nowMs;
                }
            } else {
                expeditionDeparturePhase = ExpeditionDeparturePhase::RETURN_WALK_IN;
            }
            lastPetUpdateMs = nowMs;
            nextPetFrameMs = nowMs;
            expeditionDoorPhaseStartedMs = nowMs;
        }
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::FADE_OUT) {
        if (!expeditionFade.complete()) {
            requestFullRender();
            return true;
        }

        uint8_t area = pendingExpeditionArea;
        bool autoWalk = pendingExpeditionAutoWalk;
        bool started = false;
        if (area < Game::EXPLORE_AREA_COUNT) {
            if (area != selectedExploreArea) selectExploreArea(area, nowMs);
            started = startExploreRoute(nowMs);
        }
        pendingExpedition = false;
        exploreRouteAutoWalk = started && autoWalk;
        autonomousExpedition = started && autoWalk;
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugContactActive && debugContactKind == 3) {
            Platform::logf("[FriendDiag] route kind=3 started=%u team=%u auto=%u\n",
                           started ? 1U : 0U,
                           static_cast<unsigned>(gameState.teamCount),
                           autoWalk ? 1U : 0U);
        }
#endif
        // Start at black after loading, including the failure path back to home.
        expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_IN;
        expeditionFade.beginIn();
        if (!started) {
            expeditionMainHidden = false;
            expeditionCompanionHidden = false;
            petX = petTargetX = expeditionDoorInsideX;
            petY = petTargetY = expeditionDoorInsideY;
            updateCamera();
        }
        Platform::logf(
            "[AmoledExplore] departure complete area=%u route=%u auto=%u\n",
            static_cast<unsigned>(area), started ? 1U : 0U,
            autoWalk ? 1U : 0U);
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::FADE_IN) {
        if (expeditionFade.complete()) {
            expeditionFade.reset();
            expeditionDeparturePhase = ExpeditionDeparturePhase::NONE;
            lastInteractionMs = nowMs;
            if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
                exploreSessionActive = false;
            }
        }
        requestFullRender();
        return true;
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
        RoomResource& room = RoomResource::ins();
        if (!room.available() || room.doorwayPolygonCount() < 3) {
            // A malformed room pack cannot provide a doorway path, but the
            // scene transition must still remain coherent.
            expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT;
            expeditionFade.beginOut();
            requestFullRender();
            return true;
        }
        expeditionDoorInsideX = static_cast<float>(room.doorwayInsideX());
        expeditionDoorInsideY = static_cast<float>(room.doorwayInsideY());
        expeditionDoorOutsideX = static_cast<float>(room.doorwayOutsideX());
        expeditionDoorOutsideY = static_cast<float>(room.doorwayOutsideY());
        expeditionCompanionDeparting = gameState.teamCount > 1 &&
            homeCompanionActor.active && gameState.team[1].speciesId != 0 &&
            !gameState.team[1].fainted && gameState.team[1].hpCur > 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf(
            "[HomeDiag] departure companion departing=%d team=%u active=%d "
            "species=%u fainted=%d hp=%u visitorMotion=%d\n",
            expeditionCompanionDeparting ? 1 : 0,
            static_cast<unsigned>(gameState.teamCount),
            homeCompanionActor.active ? 1 : 0,
            gameState.teamCount > 1
                ? static_cast<unsigned>(gameState.team[1].speciesId) : 0U,
            gameState.teamCount > 1 && gameState.team[1].fainted ? 1 : 0,
            gameState.teamCount > 1
                ? static_cast<unsigned>(gameState.team[1].hpCur) : 0U,
            static_cast<int>(visitorMotion));
#endif
        if (expeditionCompanionDeparting) {
            expeditionCompanionDoorInsideX = expeditionDoorInsideX;
            expeditionCompanionDoorInsideY = expeditionDoorInsideY;
            expeditionCompanionDoorOutsideX = expeditionDoorOutsideX;
            expeditionCompanionDoorOutsideY = expeditionDoorOutsideY;
            homeRuntime.transition(
                1, Home::Task::DOOR_ACTION, nowMs, 0, true);
        }
        petResting = false;
        petStoppingToEat = false;
        feedingUntilMs = 0;
        nextFeedBiteMs = 0;
        petTargetX = expeditionDoorInsideX;
        petTargetY = expeditionDoorInsideY;
        homeMainActor.x = petX;
        homeMainActor.y = petY;
        homeMainActor.targetX = petTargetX;
        homeMainActor.targetY = petTargetY;
        homeMainActor.route.clear();
        homeMainActor.velocityX = 0.0f;
        homeMainActor.velocityY = 0.0f;
        homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
        petDirection = petDirectionForDelta(petTargetX - petX,
                                            petTargetY - petY);
        petLongMove = true;
        petMotion = PetMotion::WANDERING;
        petFrame = 0;
        expeditionDeparturePhase = ExpeditionDeparturePhase::WALK_TO_DOOR;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        Platform::logf("[AmoledExplore] departure started area=%u from=%.1f,%.1f door=%.1f,%.1f\n",
                       static_cast<unsigned>(pendingExpeditionArea), petX, petY,
                       expeditionDoorInsideX, expeditionDoorInsideY);
    }

    if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::CROSS_COMPANION_DOOR) {
        const float speed = expeditionDeparturePhase ==
            ExpeditionDeparturePhase::CROSS_COMPANION_DOOR ? 42.0f : 30.0f;
        uint32_t elapsedMs = nowMs - lastPetUpdateMs;
        if (lastPetUpdateMs == 0 || nowMs < lastPetUpdateMs) elapsedMs = 0;
        lastPetUpdateMs = nowMs;
        float step = speed * (std::min<uint32_t>(elapsedMs, 120) / 1000.0f);
        float dx = homeCompanionActor.targetX - homeCompanionActor.x;
        float dy = homeCompanionActor.targetY - homeCompanionActor.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        bool reached = distance <= 0.8f || step >= distance;
        if (reached) {
            homeCompanionActor.x = homeCompanionActor.targetX;
            homeCompanionActor.y = homeCompanionActor.targetY;
            homeCompanionActor.velocityX = homeCompanionActor.velocityY = 0.0f;
            if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR) {
                homeCompanionActor.targetX = expeditionCompanionDoorOutsideX;
                homeCompanionActor.targetY = expeditionCompanionDoorOutsideY;
                companionDirection = petDirectionForDelta(
                    homeCompanionActor.targetX - homeCompanionActor.x,
                    homeCompanionActor.targetY - homeCompanionActor.y);
                expeditionDeparturePhase = ExpeditionDeparturePhase::CROSS_COMPANION_DOOR;
                companionFrame = 0;
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
                return true;
            }
            expeditionCompanionHidden = true;
            homeCompanionActor.route.clear();
            expeditionCompanionDeparting = false;
            expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT;
            expeditionFade.beginOut();
            requestFullRender();
            return true;
        }
        if (step > 0.0f) {
            homeCompanionActor.x += dx / distance * step;
            homeCompanionActor.y += dy / distance * step;
            homeCompanionActor.velocityX = dx / distance * speed;
            homeCompanionActor.velocityY = dy / distance * speed;
            companionDirection = petDirectionForDelta(dx, dy);
        }
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            companionFrame = static_cast<uint8_t>((companionFrame + 1) % 3);
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
        }
        updateCamera();
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
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
        homeMainActor.x = petX;
        homeMainActor.y = petY;
        homeMainActor.targetX = petTargetX;
        homeMainActor.targetY = petTargetY;
        if (expeditionDeparturePhase == ExpeditionDeparturePhase::WALK_TO_DOOR) {
            petTargetX = expeditionDoorOutsideX;
            petTargetY = expeditionDoorOutsideY;
            petDirection = petDirectionForDelta(petTargetX - petX,
                                                petTargetY - petY);
            expeditionDeparturePhase = ExpeditionDeparturePhase::CROSS_DOOR;
            petFrame = 0;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return true;
        }

        expeditionMainHidden = true;
        petMotion = PetMotion::IDLE;
        petTargetX = petX;
        petTargetY = petY;
        if (expeditionCompanionDeparting) {
            homeCompanionActor.targetX = expeditionCompanionDoorInsideX;
            homeCompanionActor.targetY = expeditionCompanionDoorInsideY;
            homeRuntime.transition(
                1, Home::Task::DOOR_ACTION, nowMs, 0, true);
            homeCompanionActor.route.clear();
            companionDirection = petDirectionForDelta(
                homeCompanionActor.targetX - homeCompanionActor.x,
                homeCompanionActor.targetY - homeCompanionActor.y);
            companionFrame = 0;
            nextCompanionFrameMs = nowMs;
            expeditionDeparturePhase = ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR;
#if STICKMON_ENABLE_DEBUG_FEATURES
            Platform::logf("[HomeDiag] companion walk-to-door from=%.1f,%.1f to=%.1f,%.1f\n",
                           homeCompanionActor.x, homeCompanionActor.y,
                           homeCompanionActor.targetX,
                           homeCompanionActor.targetY);
#endif
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        } else {
            expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT;
            expeditionFade.beginOut();
            requestFullRender();
        }
        return true;
    }
    if (distance > 0.0f && step > 0.0f) {
        petX += dx / distance * step;
        petY += dy / distance * step;
    }
    // Departure bypasses the normal home update loop, so keep the actor used
    // by RenderSnapshot in lockstep with the presentation coordinates.
    homeMainActor.x = petX;
    homeMainActor.y = petY;
    homeMainActor.targetX = petTargetX;
    homeMainActor.targetY = petTargetY;
    homeMainActor.velocityX = distance > 0.0f
        ? dx / distance * speed : 0.0f;
    homeMainActor.velocityY = distance > 0.0f
        ? dy / distance * speed : 0.0f;
    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        petFrame = static_cast<uint8_t>((petFrame + 1) % 3);
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    }
    updateCamera();
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

void AmoledApp::beginExploreReturn(uint32_t nowMs) {
    if (expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) return;
    lockRequested = false;
    lastInteractionMs = nowMs;
    // Close route menus before fading; never expose the area selector on return.
    sceneFlow.enterExploreRoute();
    expeditionDeparturePhase = ExpeditionDeparturePhase::RETURN_FADE_OUT;
    expeditionFade.beginOut();
    autonomousExpedition = false;
    exploreRouteMoving = false;
    exploreRouteExitConfirm = false;
    toast = nullptr;
    exploreRouteAutoWalk = false;
    exploreRoutePlayerWalkActive = false;
    exploreRoutePaused = false;
    requestFullRender();
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
    communicationReturnToComputer = false;
    visitSession.attach(&gameState);
    startVisitHost();
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

void AmoledApp::handleTouch(const TouchEvent& physicalEvent) {
    // TouchInput has already normalized the controller sample at the
    // hardware boundary. The application consumes one stable page-space
    // event and performs no second coordinate conversion.
    const TouchEvent& event = physicalEvent;
    if (event.type == TouchEventType::DOWN) {
        // A real interaction supersedes an idle request that the display task
        // has not consumed yet. Explicit lock taps set a fresh request on UP.
        lockRequested = false;
        lastInteractionMs = event.timestampMs;
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
        debugCategory == DebugViewModel::Category::TOUCH_TEST) {
        lastInteractionMs = event.timestampMs;
        if (event.type != TouchEventType::MOVE) {
            Platform::logf("[TouchRaw] event=%s raw=%d,%d mapped=%d,%d clipped=%d\n",
                event.type == TouchEventType::DOWN ? "DOWN" : "UP",
                event.rawX, event.rawY, event.x, event.y,
                (event.rawX != event.x || event.rawY != event.y) ? 1 : 0);
        }
        handleDebugTouchTest(event);
        return;
    }
    if (event.type == TouchEventType::DOWN && debugTouchDisplayEnabled) {
        debugTouchX = std::clamp<int16_t>(event.x, 0, AmoledUi::WIDTH - 1);
        debugTouchY = std::clamp<int16_t>(event.y, 0, AmoledUi::HEIGHT - 1);
        debugTouchPointValid = true;
        Platform::logf("[TouchDisplay] x=%d y=%d\n", debugTouchX,
                       debugTouchY);
        requestFullRender();
    }
#endif
    // Temporary shop diagnostics also run in release builds. Skip MOVE to
    // avoid serial traffic affecting touch responsiveness.
    const bool shopTouch = sceneFlow.current() == AppSceneFlow::Scene::SHOP;
    if (shopTouch && event.type != TouchEventType::MOVE) {
        Platform::logf(
            "[ShopTouch] t=%lu event=%s raw=%d,%d ui=%d,%d hit=%d "
            "detail=%d item=%d pointer=%d dragging=%d fade=%d\n",
            static_cast<unsigned long>(event.timestampMs),
            event.type == TouchEventType::DOWN ? "DOWN" : "UP",
            physicalEvent.x, physicalEvent.y, event.x, event.y,
            itemConfirmChoiceAt(event.x, event.y), itemConfirmOpen,
            static_cast<int>(pendingItem), pointerDown, dragging,
            static_cast<int>(expeditionDeparturePhase));
    }
    if (expeditionFade.active() ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_WALK_IN ||
        expeditionDeparturePhase == ExpeditionDeparturePhase::RETURN_CLEAR_MAIN ||
        expeditionDeparturePhase ==
            ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN) {
        if (shopTouch && event.type != TouchEventType::MOVE) {
            Platform::logLine("[ShopTouch] rejected=departure_fade");
        }
        return;
    }
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
        // A touch cancels the current one-shot player walk. A route tap below
        // may start a new walk-to-interaction command.
        exploreRoutePlayerWalkActive = false;
        pointerDown = true;
        lastInteractionMs = event.timestampMs;
        dragging = false;
        touchStartX = event.x;
        touchStartY = event.y;
        touchLastY = event.y;
        menuVelocity = 0.0f;
        if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU) {
            pressedMenuItem = mainMenuItemAt(event.x, event.y, menuScroll);
            requestRenderRows(MAIN_MENU_CONTENT_TOP, 448);
#if STICKMON_ENABLE_DEBUG_FEATURES
        } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG) {
            debugVelocity = 0.0f;
            if (debugPopup == DebugViewModel::Popup::NONE) {
                debugPressedItem = debugItemAt(
                    event.x, event.y, debugCategory, debugScroll);
            }
            requestRenderRows(0, 448);
#endif
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
            pressedExploreArea = exploreAreaAt(
                event.x, event.y, selectedExploreArea,
                ExploreItemProgression::visibleAreaCount(gameState));
            exploreDragStartArea = selectedExploreArea;
            requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
            pressedExploreMenuItem = exploreRouteMenuItemAt(event.x, event.y);
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamStatusOpen) {
            if (teamStatusAnimating) {
                teamStatusPage = teamStatusAnimTargetPage;
                teamStatusAnimating = false;
                teamStatusSlideX = 0;
            }
            teamStatusDragging = false;
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamMovesOpen &&
                   event.y >= TEAM_MOVES_HEADER_HEIGHT && event.y < 448) {
            teamMovesDragging = false;
            requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamConfirmOpen && !teamActionPopupOpen && event.y >= MENU_HEADER_HEIGHT) {
            pressedTeamSlot = teamMemberAt(
                event.x, event.y,
                Game::TeamRoster::memberCount(gameState));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
            pressedRoomItem = event.y >= MENU_HEADER_HEIGHT
                ? roomMenuItemAt(event.x, event.y) : -1;
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::ROOM_FOOD &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedRoomItem = roomFoodItemAt(event.x, event.y);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() ==
                       AppSceneFlow::Scene::COMMUNICATION &&
                   event.y >= MENU_HEADER_HEIGHT) {
            pressedCommunicationItem = communicationItemAt(
                event.x, event.y, visitSession.viewModel());
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
                   event.y >= MENU_HEADER_HEIGHT) {
            battlePressedItem = battleLogPlaybackBusy()
                ? 0xFF
                : static_cast<uint8_t>(std::max(
                      -1, battleItemAt(event.x, event.y, battlePhase)));
            requestRenderRows(352, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   event.y >= MENU_HEADER_HEIGHT) {
            if (computerPage == ComputerViewModel::Page::STORAGE &&
                (contactActionOpen || contactConfirmOpen)) {
                computerPressedItem = 0xFF;
                contactActionPressedItem = contactActionOpen
                    ? computerContactActionItemAt(
                          event.x, event.y, contactActionCount(contactActionSlot),
                          contactActionSlot, computerScroll)
                    : -1;
                requestRenderRows(MENU_HEADER_HEIGHT, AmoledUi::HEIGHT);
                break;
            }
            if (computerPage == ComputerViewModel::Page::STATUS) {
                teamStatusDragging = false;
                requestRenderRows(0, 448);
                break;
            }
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION) {
            if (progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
                const bool cancellable = progressionEvolution.initialized() &&
                    !progressionEvolution.cancelling() &&
                    !progressionEvolution.animationComplete(event.timestampMs);
                const bool cancellationAnimating =
                    progressionEvolution.cancelling() &&
                    !progressionEvolution.cancellationComplete(
                        event.timestampMs);
                const bool resumeGesture =
                    progressionEvolutionTouchStartedMs != 0 &&
                    progressionEvolutionTouchResumeUntilMs != 0 &&
                    static_cast<int32_t>(event.timestampMs -
                                         progressionEvolutionTouchResumeUntilMs) <= 0;
                progressionEvolutionTouchHeld = cancellable;
                progressionEvolutionTouchConsumed =
                    resumeGesture || cancellable || cancellationAnimating;
                progressionEvolutionTouchStartedMs = cancellable
                    ? (resumeGesture ? progressionEvolutionTouchStartedMs
                                     : event.timestampMs)
                    : 0;
                progressionEvolutionTouchResumeUntilMs = 0;
                if (cancellable) requestRenderRows(330, 348);
            } else {
                if (progressionMode == ProgressionViewModel::Mode::LEVEL_UP) {
                    progressionLevelUpTouchAllowed =
                        event.timestampMs - progressionLevelUpStartedMs >=
                            LEVEL_UP_ANIMATION_MS;
                }
                const int hit = progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE
                    ? progressionReplaceItemAt(event.x, event.y,
                          progressionScroll, progressionSelectedItem,
                          progressionDetailProgress)
                    : progressionItemAt(event.x, event.y, progressionMode);
                progressionPressedItem = hit < 0 ? 0xFF
                                                 : static_cast<uint8_t>(hit);
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOWER) {
            if ((showerMode == ShowerMode::SOAPING ||
                 showerMode == ShowerMode::BRUSHING) &&
                showerToolAt(event.x, event.y, showerToolX, showerToolY)) {
                showerToolDragging = true;
                dragging = true;
                showerLastStrokeX = event.x;
                showerLastStrokeY = event.y;
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            } else {
                pressedShowerItem = showerMode == ShowerMode::SOAP_SELECT
                    ? showerSoapItemAt(event.x, event.y)
                    : showerMode == ShowerMode::EXIT_CONFIRM
                        ? showerExitChoiceAt(event.x, event.y)
                        : (showerMode == ShowerMode::MENU ||
                           showerMode == ShowerMode::SOAPING ||
                           showerMode == ShowerMode::BRUSHING)
                            ? showerMenuItemAt(event.x, event.y) : -1;
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                   event.y >= MENU_HEADER_HEIGHT) {
            itemVelocity = 0.0f;
            if (itemConfirmOpen) {
                pressedShopDetailAction =
                    itemConfirmChoiceAt(event.x, event.y);
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BAG &&
                   !itemConfirmOpen && event.y >= MENU_HEADER_HEIGHT) {
            itemVelocity = 0.0f;
            const uint8_t dailyCount =
                Game::ItemInventory::homeBagDailyItemCount(gameState);
            pressedItemRow = itemListItemAt(
                event.x, event.y, itemScroll,
                dailyCount,
                Game::ItemInventory::homeBagExploreItemCount(gameState),
                currentItemCount(), false);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SETTINGS &&
                   settingsSliderDragging) {
            setSettingsSliderValue(settingsPressedItem, event.x,
                                   event.timestampMs);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
                requestRenderRows(MAIN_MENU_CONTENT_TOP, 448);
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
                requestRenderRows(0, 448);
            }
#endif
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS &&
                   touchStartY < EXPLORE_SELECTOR_TOP_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                // Scene selection is a horizontal carousel. A vertical move
                // still cancels the press, but does not select an area.
                if (std::abs(event.x - touchStartX) <= DRAG_START_SLOP) {
                    pressedExploreArea = -1;
                    dragging = true;
                    requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
                    break;
                }
            }
            if (std::abs(event.x - touchStartX) > DRAG_START_SLOP) {
                dragging = true;
                pressedExploreArea = -1;
            }
            if (dragging) {
                int count = std::min<int>(
                    ExploreItemProgression::visibleAreaCount(gameState),
                    Game::EXPLORE_AREA_COUNT);
                int startArea = exploreDragStartArea >= 0
                    ? exploreDragStartArea : selectedExploreArea;
                int candidate = startArea + static_cast<int>(std::lround(
                    (event.x - touchStartX) /
                    static_cast<float>(EXPLORE_SELECTOR_AREA_SPACING)));
                candidate = std::clamp(candidate, 0, std::max(0, count - 1));
                if (candidate != selectedExploreArea) {
                    selectExploreArea(static_cast<uint8_t>(candidate),
                                      event.timestampMs);
                }
                requestRenderRows(0, EXPLORE_PREVIEW_RENDER_BOTTOM);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamMovesOpen &&
                   touchStartY >= TEAM_MOVES_HEADER_HEIGHT &&
                   touchStartY < 448) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                teamMovesDragging = true;
                dragging = true;
            }
            if (teamMovesDragging) {
                teamMovesScroll -= static_cast<int16_t>(event.y - touchLastY);
                clampTeamMovesScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
                   progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE &&
                   touchStartY >= TEAM_MOVES_HEADER_HEIGHT) {
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                progressionPressedItem = 0xFF;
            }
            if (dragging) {
                progressionScroll -= static_cast<int16_t>(event.y - touchLastY);
                const int maxScroll = progressionSelectedItem < 3
                    ? std::max(0, static_cast<int>(std::lround(
                          (AmoledUi::HEIGHT / 2) * progressionDetailProgress)) - 174)
                    : 0;
                progressionScroll = std::clamp<int16_t>(progressionScroll, 0,
                    static_cast<int16_t>(maxScroll));
                requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedExploreMenuItem >= 0) {
            pressedExploreMenuItem = -1;
            requestRenderRows(0, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamStatusOpen) {
            const int deltaX = event.x - touchStartX;
            const int deltaY = event.y - touchStartY;
            if (!teamStatusDragging &&
                std::abs(deltaX) > DRAG_START_SLOP &&
                std::abs(deltaX) > std::abs(deltaY)) {
                teamStatusDragging = true;
                dragging = true;
            }
            if (teamStatusDragging) {
                int slide = deltaX;
                if ((teamStatusPage == 0 && slide > 0) ||
                    (teamStatusPage + 1 >= TEAM_STATUS_PAGE_COUNT &&
                     slide < 0)) {
                    // Rubber-band past the first/last page.
                    slide /= 3;
                }
                slide = std::clamp(slide, -AmoledUi::WIDTH, AmoledUi::WIDTH);
                if (slide != teamStatusSlideX) {
                    teamStatusSlideX = static_cast<int16_t>(slide);
                    requestRenderRows(MENU_HEADER_HEIGHT, 448);
                }
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !teamConfirmOpen && !teamActionPopupOpen &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP &&
                   pressedTeamSlot >= 0) {
            pressedTeamSlot = -1;
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STATUS) {
            const int deltaX = event.x - touchStartX;
            const int deltaY = event.y - touchStartY;
            if (!teamStatusDragging &&
                std::abs(deltaX) > DRAG_START_SLOP &&
                std::abs(deltaX) > std::abs(deltaY)) {
                teamStatusDragging = true;
                dragging = true;
            }
            if (teamStatusDragging) {
                int slide = deltaX;
                if ((teamStatusPage == 0 && slide > 0) ||
                    (teamStatusPage + 1 >= TEAM_STATUS_PAGE_COUNT && slide < 0)) {
                    slide /= 3;
                }
                teamStatusSlideX = static_cast<int16_t>(std::clamp(
                    slide, -AmoledUi::WIDTH, AmoledUi::WIDTH));
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STORAGE &&
                   touchStartY >= MENU_HEADER_HEIGHT) {
            if (contactActionOpen || contactConfirmOpen) {
                computerPressedItem = 0xFF;
                break;
            }
            if (std::abs(event.y - touchStartY) > DRAG_START_SLOP) {
                dragging = true;
                computerPressedItem = 0xFF;
            }
            if (dragging) {
                int deltaY = event.y - touchLastY;
                computerScroll -= static_cast<float>(deltaY);
                computerVelocity = static_cast<float>(-deltaY);
                clampComputerScroll();
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
                requestRenderRows(CLAW_LOG_TOP, 448);
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
                   std::max(std::abs(event.x - touchStartX),
                            std::abs(event.y - touchStartY)) > TAP_SLOP) {
            battlePressedItem = 0xFF;
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
        }
        touchLastY = event.y;
        break;

    case TouchEventType::UP: {
        if (!pointerDown) {
            if (shopTouch) Platform::logLine("[ShopTouch] rejected=no_down");
            break;
        }
        int distance = std::max(std::abs(event.x - touchStartX),
                                std::abs(event.y - touchStartY));
        const bool evolutionTouchWasConsumed =
            sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
            progressionMode == ProgressionViewModel::Mode::EVOLUTION &&
            progressionEvolutionTouchConsumed;
        const bool evolutionCompletedAtRelease =
            evolutionTouchWasConsumed &&
            progressionEvolution.animationComplete(event.timestampMs);
        const bool evolutionHoldReached =
            evolutionTouchWasConsumed && progressionEvolutionTouchHeld &&
            !evolutionCompletedAtRelease &&
            !progressionEvolution.cancelling() &&
            event.timestampMs - progressionEvolutionTouchStartedMs >=
                EVOLUTION_CANCEL_HOLD_MS;
        if (evolutionHoldReached &&
            progressionEvolution.beginCancellation(event.timestampMs)) {
            CryPlayer::ins().stop();
            nextProgressionAnimationFrameMs = event.timestampMs;
            requestFullRender();
        }
        const bool evolutionTouchCanResume =
            evolutionTouchWasConsumed && progressionEvolutionTouchHeld &&
            !evolutionCompletedAtRelease && !evolutionHoldReached &&
            !progressionEvolution.cancelling();
        progressionEvolutionTouchHeld = false;
        bool settingsSliderWasDragging = settingsSliderDragging;
        if (settingsSliderWasDragging) {
            if (shopTouch) {
                Platform::logLine("[ShopTouch] intercepted=settings_slider");
            }
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
        } else if (evolutionTouchWasConsumed) {
            // A touch that began while evolution was running cannot cross the
            // completion boundary and acknowledge the completed evolution.
        } else if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
                   progressionMode == ProgressionViewModel::Mode::LEVEL_UP &&
                   !progressionLevelUpTouchAllowed) {
            // A gesture begun during the entrance animation stays consumed.
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   !selectingItemTarget &&
                   pressedTeamSlot >= 0 && !dragging && distance <= TAP_SLOP) {
            teamActionPopupOpen = true;
            teamActionPopupSlot = static_cast<uint8_t>(pressedTeamSlot);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::SHOP &&
                   itemConfirmOpen) {
            // Detail buttons are not scrollable. A release inside the same
            // button remains a tap even if the finger drifted beyond TAP_SLOP.
            const int startChoice = itemConfirmChoiceAt(touchStartX, touchStartY);
            const int endChoice = itemConfirmChoiceAt(event.x, event.y);
            const char* decision = dragging ? "rejected=dragging"
                : startChoice < 0 ? "rejected=start_outside"
                : endChoice < 0 ? "rejected=end_outside"
                : startChoice != endChoice ? "rejected=different_button"
                : "accepted";
            Platform::logf(
                "[ShopTouch] %s start=%d,%d start_hit=%d end_hit=%d "
                "distance=%d\n",
                decision, touchStartX, touchStartY, startChoice, endChoice,
                distance);
            if (!dragging && startChoice >= 0 &&
                startChoice == itemConfirmChoiceAt(event.x, event.y)) {
                handleTap(event.x, event.y, event.timestampMs);
            }
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamStatusDragging) {
            teamStatusDragging = false;
            int target = teamStatusPage;
            if (teamStatusSlideX <= -TEAM_STATUS_SNAP_THRESHOLD &&
                teamStatusPage + 1 < TEAM_STATUS_PAGE_COUNT) {
                target = teamStatusPage + 1;
            } else if (teamStatusSlideX >= TEAM_STATUS_SNAP_THRESHOLD &&
                       teamStatusPage > 0) {
                target = teamStatusPage - 1;
            }
            beginTeamStatusSlide(static_cast<uint8_t>(target),
                                 event.timestampMs);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
                   computerPage == ComputerViewModel::Page::STATUS &&
                   teamStatusDragging) {
            teamStatusDragging = false;
            int target = teamStatusPage;
            if (teamStatusSlideX <= -TEAM_STATUS_SNAP_THRESHOLD &&
                teamStatusPage + 1 < TEAM_STATUS_PAGE_COUNT) {
                target = teamStatusPage + 1;
            } else if (teamStatusSlideX >= TEAM_STATUS_SNAP_THRESHOLD &&
                       teamStatusPage > 0) {
                target = teamStatusPage - 1;
            }
            beginTeamStatusSlide(static_cast<uint8_t>(target),
                                 event.timestampMs);
        } else if (sceneFlow.current() == AppSceneFlow::Scene::TEAM &&
                   teamMovesDragging) {
            teamMovesDragging = false;
        } else if (!dragging && distance <= TAP_SLOP) {
            if (shopTouch) Platform::logLine("[ShopTouch] list_tap=accepted");
            handleTap(event.x, event.y, event.timestampMs);
        } else if (shopTouch) {
            Platform::logf("[ShopTouch] list_tap=rejected dragging=%d distance=%d\n",
                           dragging, distance);
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
        contactActionPressedItem = -1;
        settingsPressedItem = 0xFF;
        settingsSliderDragging = false;
        settingsSliderChanged = false;
        progressionPressedItem = 0xFF;
        progressionLevelUpTouchAllowed = false;
        progressionEvolutionTouchConsumed = false;
        if (evolutionTouchCanResume) {
            progressionEvolutionTouchResumeUntilMs =
                event.timestampMs + EVOLUTION_TOUCH_GAP_MS;
        } else {
            progressionEvolutionTouchStartedMs = 0;
            progressionEvolutionTouchResumeUntilMs = 0;
        }
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
                          448);
        break;
    }
    }
}

void AmoledApp::handleTap(int x, int y, uint32_t nowMs) {
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        if (visitDeparture == VisitDeparture::WALK_TO_DOOR ||
            visitDeparture == VisitDeparture::CROSS_DOOR) return;
        if (visitRecallConfirm) {
            const int choice = recallConfirmChoiceAt(x, y);
            if (choice == 0 && visitSession.active() &&
                !visitSession.viewModel().localIsHost) {
                visitRecallConfirm = false;
                requestVisitEnd(nowMs);
            } else if (choice == 1) {
                visitRecallConfirm = false;
            }
            requestFullRender();
            return;
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (debugContactPending) {
            int choice = debugContactChoiceAt(x, y);
            if (choice == 0) {
                debugContactSelectedChoice = 0;
                debugContactChoiceConfirmUntilMs = nowMs + 100;
                acceptDebugContact(nowMs, false);
            } else if (choice == 1) {
                debugContactSelectedChoice = 1;
                debugContactChoiceConfirmUntilMs = nowMs + 100;
                debugContactPending = false;
                debugContactEventPending = false;
                debugContactEventWaitingForEntry = false;
                debugContactStorageSlot = 0xFF;
                debugContactKind = 0;
                requestFullRender();
            }
            // Modal while the prompt is up: swallow any tap outside the bubbles
            // since the HUD is faded out and its hit areas must stay inactive.
            return;
        }
        if (debugContactEventPending) {
            int choice = debugContactChoiceAt(x, y);
            if (choice < 0) return;
            debugContactSelectedChoice = static_cast<int8_t>(choice);
            debugContactChoiceConfirmUntilMs = nowMs + 100;
            debugContactEventPending = false;
            if (choice == 0 && debugContactKind == 3) {
                uint8_t unlocked = ExploreItemProgression::unlockedArea(gameState);
                selectedExploreArea = std::min<uint8_t>(selectedExploreArea, unlocked);
                if (!queueExploreDeparture(selectedExploreArea, false)) {
                    completeDebugContact(nowMs);
                    setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
                }
            } else if (choice == 1) {
                completeDebugContact(nowMs);
            }
            requestFullRender();
            return;
        }
        // The strip message fades the HUD out as well; keep its hidden hit
        // areas inert while the message is up.
        if (debugContactMessageUntilMs != 0 &&
            static_cast<int32_t>(nowMs - debugContactMessageUntilMs) < 0) {
            return;
        }
#endif
        RoomResource& room = RoomResource::ins();
        int bowlX = room.available()
            ? worldToScreenX(room.foodX())
            : 290;
        int bowlY = room.available()
            ? worldToScreenY(room.foodY())
            : 286;
        switch (homeHitTargetAt(x, y, worldToScreenX(petX),
                               worldToScreenY(petY), bowlX, bowlY)) {
        case HomeHitTarget::MENU:
            if (visitDeparture == VisitDeparture::AWAY) {
                if (visitSession.active() &&
                    !visitSession.viewModel().localIsHost) {
                    visitRecallConfirm = true;
                    requestFullRender();
                }
                break;
            }
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
            Platform::logf(
                "[HomeFood] event=place result=%u bowl=%u bites=%u "
                "satiety=%u/%u\n",
                static_cast<unsigned>(result),
                static_cast<unsigned>(gameState.room.bowlCount),
                static_cast<unsigned>(gameState.room.bowlBitesRemaining),
                gameState.teamCount > 0
                    ? static_cast<unsigned>(gameState.team[0].satiety) : 0U,
                gameState.teamCount > 1
                    ? static_cast<unsigned>(gameState.team[1].satiety) : 0U);
            switch (result) {
            case FoodPlacementResult::ADDED:
                setToast(Ui::Menu::FOOD_ADDED, nowMs);
                wakeHomeFoodArbitration(nowMs);
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
        requestRenderRows(HOME_ROOM_TOP, 448);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        CommunicationViewModel model = visitSession.viewModel();
        if (communicationBackAt(x, y)) {
            using CommState = Communication::VisitSessionService::State;
            if (model.state == CommState::ACTIVE ||
                model.state == CommState::ENDING) {
                requestVisitEnd(nowMs);
                requestFullRender();
                return;
            }
            visitSession.stop();
            releaseVisitRadioSession();
            if (communicationReturnToComputer) {
                communicationReturnToComputer = false;
                sceneFlow.enter(AppSceneFlow::Scene::COMPUTER);
                computerPage = ComputerViewModel::Page::MENU;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
                computerPressedItem = 0xFF;
            } else {
                sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
            }
            requestFullRender();
            return;
        }
        int item = communicationItemAt(x, y, model);
        using CommState = Communication::VisitSessionService::State;
        if (model.state == CommState::IDLE) {
            if (item == 0) startVisitHost();
            else if (item == 1) startVisitSearch();
        } else if (model.state == CommState::ROOM_LIST && item >= 0) {
            visitSession.selectRoom(static_cast<uint8_t>(item));
        } else if (model.state == CommState::WAITING_HOST_DECISION) {
            if (item == 0) visitSession.acceptIncoming(true);
            else if (item == 1) visitSession.acceptIncoming(false);
        } else if (model.state == CommState::ACTIVE && item == 0) {
            requestVisitEnd(nowMs);
        } else if ((model.state == CommState::FAILED ||
                    model.state == CommState::ENDED) && item == 0) {
            visitSession.stop();
            releaseVisitRadioSession();
        }
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        if (exploreRouteIceSliding) return;
        if (exploreRouteComplete) {
            leaveExploreRoute();
            return;
        }
        if (exploreRouteExitConfirm) {
            int choice = exploreRouteExitChoiceAt(x, y);
            if (choice == 0) {
                exploreRouteExitConfirm = false;
                resumeExploreRoute(nowMs);
                requestRenderRows(HOME_HEADER_HEIGHT, 448);
            } else if (choice == 1) {
                leaveExploreRoute();
            }
            return;
        }
        if (exploreRouteBackAt(x, y)) {
            exploreRouteExitConfirm = true;
            pauseExploreRoute(nowMs);
            requestRenderRows(HOME_HEADER_HEIGHT, 448);
            return;
        }
        if (exploreRouteBagAt(x, y)) {
            pauseExploreRoute(nowMs);
            openItemScene(AppSceneFlow::Scene::BAG);
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
            // Match Stick's beginAutoWalk(): one player tap walks through
            // route points until the next interaction. It is not Agent
            // mode and must not leave automatic exploration enabled.
            exploreRoutePlayerWalkActive = true;
            exploreRouteAutoWalk = false;
            if (!exploreRouteMoving) {
                beginExploreRouteStep(nowMs);
            }
            requestRenderRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        if (battleAnimationActive || battleHpAnimationActive ||
            battleSwitchStage != BattleSwitchStage::NONE ||
            battleExperienceAnimationActive ||
            battleLogPlaybackBusy() ||
            battleContinuation != BattleContinuation::NONE) return;
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
            return;
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
            const int8_t switchSlot = availableBattleSwitchSlot();
            if (switchSlot >= 0) {
                performBattleSwitch(static_cast<uint8_t>(switchSlot), true,
                                    nowMs);
            } else {
                setToast(Ui::Amoled::CANNOT_SWITCH, nowMs);
            }
        } else if (item == 3) {
            performBattleFlee(nowMs);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        int itemIndex = exploreRouteMenuItemAt(x, y);
        if (exploreRouteMenuBackAt(x, y) || itemIndex < 0) {
            sceneFlow.closeExploreMenu();
            resumeExploreRoute(nowMs);
            toast = nullptr;
            requestFullRender();
            return;
        }
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
        if (teamStatusOpen) {
            if (teamStatusBackAt(x, y)) {
                teamStatusOpen = false;
                teamStatusPage = 0;
                teamStatusSlideX = 0;
                teamStatusAnimating = false;
                teamStatusDragging = false;
                requestFullRender();
            }
            return;
        }
        if (teamActionPopupOpen) {
            const int choice = teamActionPopupItemAt(x, y, teamActionPopupSlot);
            if (choice >= 0) {
                teamActionPopupOpen = false;
                const bool leader = teamActionPopupSlot == 0;
                if (!leader && choice == 0) {
                    pendingTeamSlot = teamActionPopupSlot;
                    switchTeamLeader(nowMs);
                }
                else if (leader ? choice == 0 : choice == 1) {
                    teamStatusOpen = true;
                    teamStatusSlot = teamActionPopupSlot;
                    teamStatusPage = 0;
                    requestFullRender();
                }
                else if ((leader ? choice == 1 : choice == 2)) openTeamMoves(teamActionPopupSlot, nowMs);
                else if (!leader && choice == 3) leaveTeamMember(teamActionPopupSlot, nowMs);
                else requestRenderRows(MENU_HEADER_HEIGHT, 448);
            } else {
                teamActionPopupOpen = false;
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
            return;
        }
        if (teamMovesOpen) {
            if (teamMovesBackAt(x, y)) {
                teamMovesOpen = false;
                requestFullRender();
                return;
            }
            int item = teamMovesItemAt(
                x, y, teamMovesMode, teamMovesRecallCount,
                teamMovesScroll, teamMovesSelectedItem,
                teamMovesDetailProgress);
            if (item < 0) return;
            if (teamMovesMode == TeamMovesViewModel::Mode::MANAGE) {
                if (item < Game::MOVE_SLOT_COUNT) {
                    const Species* teamSpecies = findSpecies(
                        gameState.team[teamMovesSlot].speciesId);
                    const bool learned = teamSpecies &&
                        Game::MoveManagementService::learnedMove(
                            *teamSpecies, gameState.team[teamMovesSlot],
                            static_cast<uint8_t>(item));
                    if (!learned) {
                        setToast(Ui::EMPTY, nowMs);
                    } else if (teamMovesSelectedItem == item &&
                               !teamMovesDetailAnimating &&
                               teamMovesDetailProgress >= 0.99f) {
                        startTeamMovesDetailAnimation(false, nowMs);
                    } else if (teamMovesSelectedItem != item) {
                        teamMovesSelectedItem = static_cast<uint8_t>(item);
                        if (teamMovesDetailProgress > 0.0f) {
                            teamMovesDetailProgress = 1.0f;
                            teamMovesDetailAnimating = false;
                        } else {
                            startTeamMovesDetailAnimation(true, nowMs);
                        }
                        requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
                    }
                    return;
                }
            } else if (teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT) {
                if (item >= teamMovesRecallCount) {
                    teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
                    teamMovesRecallSelected = 0xFF;
                    teamMovesSelectedItem = 0xFF;
                    teamMovesScroll = 0;
                } else {
                    teamMovesRecallSelected = static_cast<uint8_t>(item);
                    teamMovesMode = TeamMovesViewModel::Mode::RECALL_REPLACE;
                    teamMovesScroll = 0;
                    requestRenderRows(MENU_HEADER_HEIGHT, 448);
                }
            } else if (item < 2 && teamMovesRecallSelected < teamMovesRecallCount) {
                if (Game::MoveManagementService::recallMove(
                        gameState, teamMovesSlot,
                        teamMovesRecallIds[teamMovesRecallSelected],
                        static_cast<uint8_t>(item + 1))) {
                    saveState();
                    refreshTeamMoveRecallable();
                    teamMovesMode = TeamMovesViewModel::Mode::MANAGE;
                    teamMovesRecallSelected = 0xFF;
                    teamMovesSelectedItem = 0xFF;
                    teamMovesScroll = 0;
                    setToast(Ui::Amoled::MOVE_RECALLED, nowMs);
                } else {
                    setToast(Ui::Team::MOVE_RECALL_FAILED, nowMs);
                }
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            } else {
                teamMovesMode = TeamMovesViewModel::Mode::RECALL_SELECT;
                teamMovesScroll = 0;
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
            return;
        }
        if (teamConfirmOpen) {
            int choice = teamConfirmChoiceAt(x, y);
            if (choice == 0) {
                switchTeamLeader(nowMs);
            } else if (choice == 1) {
                teamConfirmOpen = false;
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
            return;
        }
        if (teamBackAt(x, y)) {
            if (selectingItemTarget) {
                selectingItemTarget = false;
                pendingItem = Game::ItemId::COUNT;
                pendingItemAction = PendingItemAction::NONE;
                itemConfirmOpen = false;
                teamActionPopupOpen = false;
                sceneFlow.enter(AppSceneFlow::Scene::BAG);
                requestFullRender();
            } else closeItemScene();
            return;
        }
        int slot = teamMemberAt(
            x, y, Game::TeamRoster::memberCount(gameState));
        if (slot < 0) return;
        if (selectingItemTarget) {
            uint8_t target = static_cast<uint8_t>(slot);
            if (Game::ItemInventory::count(gameState, pendingItem) == 0) {
                setToast(Ui::Amoled::NO_STOCK, nowMs);
            } else {
                const Species* source = findSpecies(
                    gameState.team[target].speciesId);
                const Species* evolutionTarget =
                    source && isEvolutionStone(pendingItem)
                        ? stoneEvolutionTarget(*source, pendingItem) : nullptr;
                if (isEvolutionStone(pendingItem)) {
                    if (!evolutionTarget) {
                        setToast(Ui::Amoled::NO_EFFECT, nowMs);
                        requestFullRender();
                        return;
                    }
                    const Game::ItemId stone = pendingItem;
                    const uint16_t fromSpeciesId =
                        gameState.team[target].speciesId;
                    selectingItemTarget = false;
                    pendingItem = Game::ItemId::COUNT;
                    pendingItemAction = PendingItemAction::NONE;
                    itemConfirmOpen = false;
                    openEvolutionProgression(
                        itemTargetReturnScene, target, fromSpeciesId,
                        evolutionTarget->id, nowMs, stone);
                    return;
                }
                const uint8_t oldLevel = gameState.team[target].level;
                Game::ItemInventory::UseResult used =
                    Game::ItemInventory::useOnTeam(gameState, pendingItem, target);
                if (used == Game::ItemInventory::UseResult::USED) {
                    saveState();
                    selectingItemTarget = false;
                    pendingItem = Game::ItemId::COUNT;
                    pendingItemAction = PendingItemAction::NONE;
                    if (gameState.team[target].level != oldLevel) {
                        openProgressionScene(
                            itemTargetReturnScene, target, oldLevel, nowMs);
                        return;
                    }
                    setToast(Ui::Amoled::ITEM_USED, nowMs);
                    sceneFlow.enter(AppSceneFlow::Scene::BAG);
                } else setToast(Ui::Amoled::NO_EFFECT, nowMs);
            }
            requestFullRender();
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::ROOM) {
        if (roomMenuBackAt(x, y)) {
            closeItemScene();
            return;
        }
        int item = roomMenuItemAt(x, y);
        if (item == 0) {
            openRoomFoodScene();
        } else if (item == 1) {
            openShowerScene(nowMs);
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
            if (x >= 300 && x < 356 && y >= 10 && y < 66) {
                if (progressionSelectedItem >= 3) return;
                if (progressionSelectedItem == 0) {
                    progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
                } else {
                    Game::MonsterRuntime& monster =
                        gameState.team[progressionTeamSlot];
                    Game::MoveId& slot = progressionSelectedItem == 1
                        ? monster.move2Id : monster.move3Id;
                    slot = progressionMoveId;
                    if (progressionSelectedItem == 1) {
                        monster.moveProficiency[1] = 0;
                    } else {
                        monster.moveProficiency[2] = 0;
                    }
                    saveState();
                }
                progressionPressedItem = 0xFF;
                advanceProgression(nowMs);
                return;
            }
            const int choice = progressionReplaceItemAt(
                x, y, progressionScroll, progressionSelectedItem,
                progressionDetailProgress);
            if (choice >= 0) {
                if (progressionSelectedItem == choice &&
                    !progressionDetailAnimating &&
                    progressionDetailProgress >= 0.99f) {
                    progressionDetailTargetVisible = false;
                    progressionDetailAnimStartMs = nowMs;
                    progressionDetailAnimating = true;
                } else if (progressionSelectedItem != choice) {
                    progressionSelectedItem = static_cast<uint8_t>(choice);
                    if (progressionDetailProgress > 0.0f) {
                        progressionDetailProgress = 1.0f;
                        progressionDetailAnimating = false;
                        progressionDetailTargetVisible = true;
                    } else {
                        progressionDetailTargetVisible = true;
                        progressionDetailAnimStartMs = nowMs;
                        progressionDetailAnimating = true;
                    }
                }
                requestRenderRows(0, 448);
            }
            return;
        }
        if (progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
            if (progressionEvolution.cancelling()) {
                if (!progressionEvolution.cancellationComplete(nowMs)) return;
                progressionEvolution.reset();
                progressionEvolutionItem = Game::ItemId::COUNT;
                progressionToSpeciesId = 0;
                progressionMoveCursor = 0;
                progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
                advanceProgression(nowMs);
            } else if (progressionEvolution.animationComplete(nowMs)) {
                advanceProgression(nowMs);
            } else if (progressionEvolution.beginCancellation(nowMs)) {
                CryPlayer::ins().stop();
                progressionPressedItem = 0xFF;
                nextProgressionAnimationFrameMs = nowMs;
                requestFullRender();
            }
            return;
        }
        if (progressionMode == ProgressionViewModel::Mode::LEVEL_UP &&
            nowMs - progressionLevelUpStartedMs < LEVEL_UP_ANIMATION_MS) return;
        if (progressionItemAt(x, y, progressionMode) != 0) return;
        advanceProgression(nowMs);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER) {
        if (computerPage == ComputerViewModel::Page::STORAGE &&
            contactConfirmOpen) {
            handleContactConfirmTap(x, y, nowMs);
            return;
        }
        if (computerPage == ComputerViewModel::Page::STORAGE &&
            contactActionOpen) {
            handleContactActionTap(x, y, nowMs);
            return;
        }
        if (computerPage == ComputerViewModel::Page::STATUS &&
            computerBackAt(x, y, computerPage)) {
            computerPage = ComputerViewModel::Page::STORAGE;
            teamStatusFromContact = false;
            teamStatusContactSlot = 0xFF;
            teamStatusPage = 0;
            teamStatusSlideX = 0;
            teamStatusAnimating = false;
            teamStatusDragging = false;
            computerPressedItem = 0xFF;
            requestFullRender();
            return;
        }
        if (computerBackAt(x, y, computerPage)) {
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
                visitSession.attach(&gameState);
                communicationReturnToComputer = true;
                sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
                pressedCommunicationItem = -1;
                toast = nullptr;
                requestFullRender();
            } else if (item == 1) {
                computerPage = ComputerViewModel::Page::STORAGE;
                computerPressedItem = 0xFF;
                computerScroll = 0.0f;
                computerVelocity = 0.0f;
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
#endif
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
            return;
        } else {
            openContactAction(static_cast<uint8_t>(item));
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
        default:
            return;
        }
        saveState();
        requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
            }
            return;
        }
        if (showerMode == ShowerMode::SOAP_SELECT) {
            if (showerBackAt(x, y)) {
                showerMode = ShowerMode::MENU;
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
        if (showerMode == ShowerMode::SOAPING ||
            showerMode == ShowerMode::BRUSHING) {
            const int item = showerMenuItemAt(x, y);
            if (item == 1 && showerMode == ShowerMode::SOAPING) {
                if (showerSoapRewarded) startShowerTool(ShowerMode::BRUSHING, nowMs);
                else setToast(Ui::Amoled::SOAP_FIRST, nowMs);
            } else if (item == 2) {
                if (!showerSoapRewarded) setToast(Ui::Amoled::SOAP_FIRST, nowMs);
                else if (!showerBrushRewarded) setToast(Ui::Amoled::BRUSH_FIRST, nowMs);
                else startShowerRinse(nowMs);
            }
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
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
        }
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BAG) {
        if (itemConfirmOpen) {
            int choice = itemConfirmChoiceAt(x, y);
            if (choice == 0) {
                if (battleBagMode) {
                    performBattleBagItem(pendingItem, nowMs);
                } else if (Game::ItemInventory::usableFromHomeBag(pendingItem) &&
                    pendingItem != Game::ItemId::MAX_REPEL &&
                    pendingItem != Game::ItemId::HONEY) {
                    itemTargetReturnScene = sceneFlow.subSceneReturn();
                    itemConfirmOpen = false;
                    selectingItemTarget = true;
                    openTeamScene(true);
                } else performPendingItemAction(nowMs);
                } else if (choice == 1) {
                    itemConfirmOpen = false;
                    pendingItem = Game::ItemId::COUNT;
                    pendingItemAction = PendingItemAction::NONE;
                    requestFullRender();
                }
                return;
        }
        if (itemListBackAt(x, y, itemConfirmOpen)) {
            closeItemScene();
            return;
        }
        int index = itemListItemAt(
            x, y, itemScroll,
            Game::ItemInventory::homeBagDailyItemCount(gameState),
            Game::ItemInventory::homeBagExploreItemCount(gameState),
            currentItemCount(), false);
        if (index < 0) return;
        Game::ItemId item = currentItemAt(static_cast<uint8_t>(index));
        if (battleBagMode && Game::foodIndexForItemId(item) < 0 &&
            !Game::ItemInventory::usableInBattle(item)) {
            setToast(Ui::Amoled::CANNOT_USE, nowMs);
            return;
        }
        if (!battleBagMode && item == Game::ItemId::HEART_SCALE) {
            openTeamScene();
            openTeamMoves(0, nowMs);
            return;
        }
        pendingItem = item;
        pendingItemAction = PendingItemAction::USE;
        itemConfirmOpen = true;
        toast = nullptr;
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::SHOP) {
        if (itemListBackAt(x, y, itemConfirmOpen)) {
            closeItemScene();
            return;
        }
        if (itemConfirmOpen) {
            int choice = itemConfirmChoiceAt(x, y);
            Platform::logf("[ShopAction] t=%lu tap=%s item=%d\n",
                           static_cast<unsigned long>(nowMs),
                           choice == 0 ? "transaction" : choice == 1 ? "back" : "outside",
                           static_cast<int>(pendingItem));
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
                Platform::logLine("[ShopAction] detail_closed render_requested");
            }
            return;
        }
        int menuItem = shopMenuItemAt(x, y);
        if (menuItem >= 0) {
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
        Platform::logf("[ShopAction] detail_open=%d item=%d action=%s\n",
                       itemConfirmOpen, static_cast<int>(pendingItem),
                       pendingItemAction == PendingItemAction::SELL ? "sell" : "buy");
        Platform::logLine(
            "[ShopTouch] detail_touch: buy x=[36,176) y=[354,426); "
            "back x=[192,332) y=[354,426)");
        toast = nullptr;
        requestFullRender();
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        if (exploreSelectionBackAt(x, y)) {
            clearExplorePreview();
            sceneFlow.openMenu(AppSceneFlow::Scene::HOME);
            toast = nullptr;
            requestFullRender();
            return;
        }
        if (exploreStartAt(x, y)) {
            if (!ExploreItemProgression::isAreaUnlocked(
                    selectedExploreArea, gameState)) {
                setToast(Ui::Explore::AREA_LOCKED, nowMs);
            } else {
                queueExploreDeparture(selectedExploreArea, false);
            }
            requestFullRender();
            return;
        }
        int area = exploreAreaAt(
            x, y, selectedExploreArea,
            ExploreItemProgression::visibleAreaCount(gameState));
        if (area >= 0) {
            // Visible areas can be selected before they are unlocked. The
            // locked state is communicated by the disabled departure button.
            if (selectedExploreArea != static_cast<uint8_t>(area)) {
                selectExploreArea(static_cast<uint8_t>(area), nowMs);
                toast = nullptr;
            }
            requestRenderRows(0, EXPLORE_SELECTOR_BUTTON_TOP);
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
            communicationReturnToComputer = false;
            sceneFlow.enter(AppSceneFlow::Scene::COMMUNICATION);
            visitSession.attach(&gameState);
            requestFullRender();
        } else if (entry.target == AppSceneFlow::Scene::HOME) {
            exploreRouteMoving = false;
            exploreRouteAutoWalk = false;
            exploreRoutePlayerWalkActive = false;
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
                requestRenderRows(68, 448);
            }
        }
    }
#endif
    AudioManager::ins().update();
    CryPlayer::ins().update();
    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
        progressionMode == ProgressionViewModel::Mode::EVOLUTION &&
        progressionEvolution.initialized()) {
        if (progressionEvolutionTouchHeld) {
            if (progressionEvolution.animationComplete(nowMs)) {
                // Completion wins when it reaches its deadline before the
                // hold. The gesture remains consumed until the finger lifts.
                progressionEvolutionTouchHeld = false;
                requestRenderRows(330, 348);
            } else if (nowMs - progressionEvolutionTouchStartedMs >=
                       EVOLUTION_CANCEL_HOLD_MS) {
                progressionEvolutionTouchHeld = false;
                if (progressionEvolution.beginCancellation(nowMs)) {
                    CryPlayer::ins().stop();
                    nextProgressionAnimationFrameMs = nowMs;
                    requestFullRender();
                }
            }
        }
        if (!progressionEvolution.cancelling() &&
            !progressionEvolution.cryPlayed() &&
            progressionEvolution.elapsed(nowMs) >=
                Game::EvolutionSequence::REVEAL_END_MS) {
            progressionEvolution.markCryPlayed();
            CryPlayer::ins().replay(progressionToSpeciesId);
        }
        const bool animating = progressionEvolution.cancelling()
            ? !progressionEvolution.cancellationComplete(nowMs)
            : !progressionEvolution.animationComplete(nowMs);
        if (animating &&
            static_cast<int32_t>(nowMs - nextProgressionAnimationFrameMs) >= 0) {
            nextProgressionAnimationFrameMs =
                nowMs + PROGRESSION_ANIMATION_FRAME_MS;
            requestRenderRows(0, 348);
        } else if (!animating && nextProgressionAnimationFrameMs != 0) {
            nextProgressionAnimationFrameMs = 0;
            requestFullRender();
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
        progressionMode == ProgressionViewModel::Mode::LEVEL_UP &&
        nextProgressionAnimationFrameMs != 0 &&
        static_cast<int32_t>(nowMs - nextProgressionAnimationFrameMs) >= 0) {
        if (nowMs - progressionLevelUpStartedMs < LEVEL_UP_ANIMATION_MS) {
            nextProgressionAnimationFrameMs =
                nowMs + PROGRESSION_ANIMATION_FRAME_MS;
        } else {
            nextProgressionAnimationFrameMs = 0;
        }
        requestFullRender();
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugContactActive && debugContactKind != 3 &&
        !debugContactEventPending && !debugContactEventWaitingForEntry &&
        nowMs - debugContactStartedMs >= 30000UL) {
        completeDebugContact(nowMs);
    }
    if (debugContactSelectedChoice >= 0 &&
        static_cast<int32_t>(nowMs - debugContactChoiceConfirmUntilMs) >= 0) {
        debugContactSelectedChoice = -1;
        debugContactChoiceConfirmUntilMs = 0;
        debugContactChoiceVisible = false;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }
    // Ease the visitor prompt in/out: the bottom HUD fades away while the
    // message and the yes/no bubbles appear, and comes back after a choice.
    const bool stripMessageActive = debugContactMessageUntilMs != 0 &&
        static_cast<int32_t>(nowMs - debugContactMessageUntilMs) < 0;
    const uint8_t promptFadeTarget =
        (debugContactPending || debugContactEventPending ||
         stripMessageActive) ? 255 : 0;
    if (debugContactPromptFade != promptFadeTarget &&
        sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        // Keep the bottom HUD/message swap brisk. Choice bubbles are hidden
        // independently as soon as the tap is accepted.
        constexpr uint8_t PROMPT_FADE_STEP = 40;
        debugContactPromptFade = debugContactPromptFade < promptFadeTarget
            ? static_cast<uint8_t>(std::min<int>(
                  255, debugContactPromptFade + PROMPT_FADE_STEP))
            : static_cast<uint8_t>(std::max<int>(
                  0, debugContactPromptFade - PROMPT_FADE_STEP));
        if ((debugContactPending || debugContactEventPending) &&
            debugContactPromptFade == 255) {
            Platform::logf("[FriendDiag] prompt ready kind=%u\n",
                           static_cast<unsigned>(debugContactKind));
        }
        requestRenderRows(HOME_ROOM_TOP, 448);
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
    Game::MonsterRuntime detachedVisitor{};
    const bool hadAttachedVisitor = gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR;
    if (hadAttachedVisitor) detachedVisitor = gameState.team[1];
    visitSession.update(nowMs);
    CommunicationViewModel communicationAfter = visitSession.viewModel();
    if (hadAttachedVisitor && gameState.teamCount < 2 &&
        homeCompanionActor.hidden) {
        cancelPairInteraction(nowMs);
        homeRuntime.releaseAll(0);
        homeRuntime.stop(0, nowMs);
        visitorMotion = VisitorMotion::NONE;
        syncHomeActors(nowMs);
        sceneFlow.goHome();
        requestFullRender();
    } else if (hadAttachedVisitor && gameState.teamCount < 2 &&
               visitorMotion != VisitorMotion::EXITING) {
        gameState.team[1] = detachedVisitor;
        gameState.teamCount = 2;
        gameState.activeSlot = 0;
        syncHomeActors(nowMs);
        beginVisitorExit(nowMs, false);
        sceneFlow.goHome();
        requestFullRender();
    }
    using VisitState = Communication::VisitSessionService::State;
    if (visitRadioExclusive &&
        (communicationAfter.state == VisitState::IDLE ||
         communicationAfter.state == VisitState::FAILED ||
         communicationAfter.state == VisitState::ENDED)) {
        releaseVisitRadioSession();
    }
    if (communicationBefore.state !=
            Communication::VisitSessionService::State::ACTIVE &&
        communicationAfter.state ==
            Communication::VisitSessionService::State::ACTIVE &&
        sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        sceneFlow.goHome();
        if (!communicationAfter.localIsHost) beginVisitDeparture(nowMs);
    }
    if (communicationBefore.state != communicationAfter.state ||
        communicationBefore.roomCount != communicationAfter.roomCount ||
        communicationBefore.remote.speciesId != communicationAfter.remote.speciesId ||
        communicationBefore.remote.mood != communicationAfter.remote.mood ||
        communicationBefore.remote.satiety != communicationAfter.remote.satiety ||
        communicationBefore.remote.hpCur != communicationAfter.remote.hpCur ||
        communicationBefore.remote.hpMax != communicationAfter.remote.hpMax ||
        communicationBefore.remainSec != communicationAfter.remainSec) {
        requestFullRender();
    }
    updateClockAndCare(nowMs);
    updateMoodHearts(nowMs);
    if (!updateExploreDeparture(nowMs)) {
        updatePet(nowMs);
        updateExploreRoute(nowMs);
    }
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
                requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
            }
        } else {
            exploreAreaAnimCursor += difference * EXPLORE_AREA_CURSOR_LERP;
            if (std::fabs(target - exploreAreaAnimCursor) < 0.05f) {
                exploreAreaAnimCursor = target;
            }
            requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
        }
        exploreAreaAnimating = std::fabs(target - exploreAreaAnimCursor) >= 0.05f;

        bool previewLoadWasDue = explorePreviewLoadPending &&
            static_cast<int32_t>(nowMs - explorePreviewNextLoadAt) >= 0;
        updateExplorePreviewLoading(nowMs);
        if (previewLoadWasDue) {
            requestRenderRows(EXPLORE_PREVIEW_RENDER_TOP,
                              EXPLORE_PREVIEW_RENDER_BOTTOM);
        }
        if (explorePreviewPool.count > 0) {
            uint32_t elapsed = nowMs - explorePreviewStartedAt;
            uint32_t visualCycle = elapsed / EXPLORE_PREVIEW_CYCLE_MS;
            if (visualCycle != explorePreviewVisualCycle) {
                explorePreviewVisualCycle = visualCycle;
                requestRenderRows(EXPLORE_PREVIEW_RENDER_TOP,
                                  EXPLORE_PREVIEW_RENDER_BOTTOM);
            }
            explorePreviewMoving =
                elapsed % EXPLORE_PREVIEW_CYCLE_MS >
                EXPLORE_PREVIEW_HOLD_MS;
        }
    }

    if (teamStatusAnimating) {
        const uint32_t elapsed = nowMs - teamStatusAnimStartMs;
        if (elapsed >= TEAM_STATUS_SLIDE_MS) {
            teamStatusPage = teamStatusAnimTargetPage;
            teamStatusSlideX = 0;
            teamStatusAnimating = false;
            requestFullRender();
        } else {
            const float t = static_cast<float>(elapsed) /
                            static_cast<float>(TEAM_STATUS_SLIDE_MS);
            const float ease = 1.0f - (1.0f - t) * (1.0f - t);
            teamStatusSlideX = static_cast<int16_t>(
                teamStatusAnimFromX +
                static_cast<int>((teamStatusAnimToX - teamStatusAnimFromX) *
                                 ease));
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        }
    }

    if (teamMovesOpen && teamMovesDetailAnimating) {
        const uint32_t elapsed = nowMs - teamMovesDetailAnimStartMs;
        const float raw = std::min(
            1.0f, static_cast<float>(elapsed) /
                       static_cast<float>(TEAM_MOVES_DETAIL_ANIM_MS));
        const float eased = teamMovesDetailTargetVisible
            ? 1.0f - (1.0f - raw) * (1.0f - raw)
            : raw * raw;
        teamMovesDetailProgress = teamMovesDetailTargetVisible
            ? eased : 1.0f - eased;
        requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
        if (raw >= 1.0f) {
            teamMovesDetailAnimating = false;
            teamMovesDetailProgress = teamMovesDetailTargetVisible ? 1.0f : 0.0f;
            if (!teamMovesDetailTargetVisible) teamMovesSelectedItem = 0xFF;
            clampTeamMovesScroll();
            requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::PROGRESSION &&
        progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE &&
        progressionDetailAnimating) {
        const float raw = std::min(1.0f,
            static_cast<float>(nowMs - progressionDetailAnimStartMs) /
            static_cast<float>(TEAM_MOVES_DETAIL_ANIM_MS));
        const float eased = progressionDetailTargetVisible
            ? 1.0f - (1.0f - raw) * (1.0f - raw) : raw * raw;
        progressionDetailProgress = progressionDetailTargetVisible
            ? eased : 1.0f - eased;
        if (raw >= 1.0f) {
            progressionDetailAnimating = false;
            progressionDetailProgress = progressionDetailTargetVisible ? 1.0f : 0.0f;
            if (!progressionDetailTargetVisible) progressionSelectedItem = 0xFF;
        }
        const int maxScroll = std::max(0,
            static_cast<int>(std::lround(
                (AmoledUi::HEIGHT / 2) * progressionDetailProgress)) - 174);
        progressionScroll = std::clamp<int16_t>(progressionScroll, 0,
            static_cast<int16_t>(maxScroll));
        if (progressionSelectedItem == 2 && progressionDetailTargetVisible) {
            progressionScroll = static_cast<int16_t>(maxScroll);
        }
        requestRenderRows(0, 448);
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battleHpAnimationActive) {
        const uint32_t gaugeNowMs = Platform::clock().millis();
        if (static_cast<int32_t>(gaugeNowMs -
                                 nextBattleHpAnimationFrameMs) >= 0) {
            nextBattleHpAnimationFrameMs =
                gaugeNowMs + BATTLE_GAUGE_FRAME_MS;
            requestRenderRows(battleHpAnimationWild ? 48 : 266,
                              battleHpAnimationWild ? 88 : 306);
        }
        if (static_cast<int32_t>(
                gaugeNowMs - battleHpAnimationStartedMs) >= 0 &&
            gaugeNowMs - battleHpAnimationStartedMs >=
                BATTLE_HP_ANIMATION_MS) {
            battleHpAnimationActive = false;
            requestRenderRows(battleHpAnimationWild ? 48 : 266,
                              battleHpAnimationWild ? 88 : 306);
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battleExperienceAnimationActive) {
        const uint32_t gaugeNowMs = Platform::clock().millis();
        if (static_cast<int32_t>(
                gaugeNowMs - nextBattleExperienceAnimationFrameMs) >= 0) {
            nextBattleExperienceAnimationFrameMs =
                gaugeNowMs + BATTLE_GAUGE_FRAME_MS;
            requestRenderRows(232, 308);
        }
        if (gaugeNowMs - battleExperienceAnimationStartedMs >=
            BATTLE_EXP_ANIMATION_MS) {
            battleExperienceAnimationActive = false;
            AudioManager::ins().playSfx(SfxCue::EXP_FULL);
            requestRenderRows(232, 308);
        }
    }

    if (battleAnimationActive &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        // Audio setup can block after the loop timestamp was sampled. Use a
        // fresh monotonic value so the first frame cannot underflow and skip.
        uint32_t animationNowMs = Platform::clock().millis();
        uint32_t elapsed = animationNowMs - battleAnimationStartedMs;
        if (!battleImpactAudioPlayed &&
            elapsed >= BATTLE_HP_DAMAGE_DELAY_MS) {
            battleImpactAudioPlayed = true;
            if (battleImpactSfx != 0xFF) {
                AudioManager::ins().playSfx(
                    static_cast<SfxCue>(battleImpactSfx));
            }
        }
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
            battleAttackLogHeld = false;
            battleImpactSfx = 0xFF;
            requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
            Platform::logf("[BattleAnim] complete side=%s elapsed=%lu\n",
                           battleAnimationAttackerWild ? "wild" : "player",
                           static_cast<unsigned long>(elapsed));
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        serviceBattleLog(nowMs)) {
        requestRenderRows(352, 448);
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battleSwitchStage != BattleSwitchStage::NONE) {
        updateBattleSwitch(Platform::clock().millis());
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battleContinuation != BattleContinuation::NONE &&
        battleSwitchStage == BattleSwitchStage::NONE &&
        !battleAnimationActive && !battleHpAnimationActive &&
        !battleLogPlaybackBusy()) {
        const BattleContinuation continuation = battleContinuation;
        battleContinuation = BattleContinuation::NONE;
        if (continuation == BattleContinuation::WILD_TURN) {
            performBattleWildTurn(nowMs);
        } else {
            advanceBattleTurn(nowMs);
        }
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battlePhase == BattleViewModel::Phase::VICTORY &&
        !battleExperienceAnimationActive && !battleLogPlaybackBusy() &&
        !battleVictoryFinalizePending &&
        battleContinuation == BattleContinuation::NONE) {
        finishBattleVictory(nowMs);
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        battleVictoryFinalizePending && !battleLogPlaybackBusy()) {
        battleVictoryFinalizePending = false;
        finishBattleAfterFriendship(nowMs);
    }

#if STICKMON_HAS_CLAW
    if (autonomousExpedition &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE &&
        !battleAnimationActive && !battleHpAnimationActive &&
        battleSwitchStage == BattleSwitchStage::NONE &&
        !battleExperienceAnimationActive && !battleLogPlaybackBusy() &&
        battleContinuation == BattleContinuation::NONE) {
        // Reuse the game's deterministic battle AI for autonomous routes.
        // Existing animation and log gates keep each action interruptible.
        switch (battlePhase) {
        case BattleViewModel::Phase::ACTION:
            performBattleAttack(nowMs);
            break;
        case BattleViewModel::Phase::VICTORY:
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
        updateShowerFoamMotion(0.08f);
        const float atmosphereStep = showerAtmosphereTarget ? 24.0f : -34.0f;
        showerAtmosphereAlpha = std::clamp(
            showerAtmosphereAlpha + atmosphereStep, 0.0f, 255.0f);
        if (showerMode == ShowerMode::RINSING &&
            nowMs - showerLastFrameMs >= 80) {
            showerLastFrameMs = nowMs;
            uint32_t elapsed = nowMs - showerModeStartedMs;
            showerRinseProgress = static_cast<uint8_t>(
                std::min<uint32_t>(100, elapsed * 100 / 1800));
            if (nowMs - showerLastRinseFoamMs >= 350) {
                showerLastRinseFoamMs = nowMs;
                rinseShowerFoamOneStage();
            }
            requestRenderRows(MENU_HEADER_HEIGHT, 352);
            if (elapsed >= 1800 && !anyShowerFoam() &&
                showerAtmosphereAlpha <= 1.0f) {
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
                ? HOME_STATUS_TOP : 448);
    }
    if (heartsUntil && static_cast<int32_t>(nowMs - heartsUntil) >= 0) {
        heartsUntil = 0;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }
    if (sceneFlow.current() == AppSceneFlow::Scene::MAIN_MENU && !pointerDown &&
        std::fabs(menuVelocity) > 0.24f) {
        float previous = menuScroll;
        menuScroll += menuVelocity;
        menuVelocity *= 0.86f;
        clampMenuScroll();
        if (std::fabs(menuScroll - previous) > 0.02f) {
            requestRenderRows(MAIN_MENU_CONTENT_TOP, 448);
        } else {
            menuVelocity = 0.0f;
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
    } else if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
               debugPopup == DebugViewModel::Popup::NONE && !pointerDown &&
               std::fabs(debugVelocity) > 0.24f) {
        float previous = debugScroll;
        debugScroll += debugVelocity;
        debugVelocity *= 0.86f;
        clampDebugScroll();
        if (std::fabs(debugScroll - previous) > 0.02f) {
            requestRenderRows(0, 448);
        } else {
            debugVelocity = 0.0f;
        }
#endif
    } else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
               computerPage == ComputerViewModel::Page::STORAGE &&
               !pointerDown && std::fabs(computerVelocity) > 0.24f) {
        float previous = computerScroll;
        computerScroll += computerVelocity;
        computerVelocity *= 0.86f;
        clampComputerScroll();
        if (std::fabs(computerScroll - previous) > 0.02f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else {
            computerVelocity = 0.0f;
        }
    }
#if STICKMON_HAS_CLAW
    else if (sceneFlow.current() == AppSceneFlow::Scene::COMPUTER &&
             computerPage == ComputerViewModel::Page::CLAW_SETUP &&
             clawLogView && !pointerDown &&
             std::fabs(clawLogVelocity) > 0.24f) {
        float previous = clawLogScroll;
        clawLogScroll += clawLogVelocity;
        clawLogVelocity *= 0.86f;
        clampClawLogScroll();
        if (std::fabs(clawLogScroll - previous) > 0.02f) {
            requestRenderRows(CLAW_LOG_TOP, 448);
        } else {
            clawLogVelocity = 0.0f;
        }
    }
#endif
    else if ((sceneFlow.current() == AppSceneFlow::Scene::BAG ||
              sceneFlow.current() == AppSceneFlow::Scene::SHOP) &&
               !itemConfirmOpen && !pointerDown &&
               std::fabs(itemVelocity) > 0.24f) {
        float previous = itemScroll;
        itemScroll += itemVelocity;
        itemVelocity *= 0.86f;
        clampItemScroll();
        if (std::fabs(itemScroll - previous) > 0.02f) {
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
        } else {
            itemVelocity = 0.0f;
        }
        if (std::fabs(itemVelocity) <= 0.24f) {
            itemVelocity = 0.0f;
        }
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS) {
        if (exploreAreaAnimating) {
            requestRenderRows(0, EXPLORE_SELECTOR_TOP_HEIGHT);
        } else if (explorePreviewMoving &&
                   nowMs - explorePreviewLastRenderRequestMs >= 33) {
            explorePreviewLastRenderRequestMs = nowMs;
            requestRenderRows(EXPLORE_PREVIEW_RENDER_TOP,
                              EXPLORE_PREVIEW_RENDER_BOTTOM);
        }
    }
    uint32_t idleTimeoutMs = 0;
    switch (gameState.settings.idleTimeoutIndex) {
    case 0: idleTimeoutMs = 30UL * 1000UL; break;
    case 1: idleTimeoutMs = 2UL * 60UL * 1000UL; break;
    case 2: idleTimeoutMs = 5UL * 60UL * 1000UL; break;
    case 3: idleTimeoutMs = 10UL * 60UL * 1000UL; break;
    default: break;
    }
    bool sleepSafeScene = displayLockAllowed() &&
                          sceneFlow.current() != AppSceneFlow::Scene::BATTLE &&
                          sceneFlow.current() != AppSceneFlow::Scene::SHOWER &&
                          sceneFlow.current() != AppSceneFlow::Scene::PROGRESSION &&
                          !visitSession.busy();
#if STICKMON_ENABLE_DEBUG_FEATURES
    sleepSafeScene = sleepSafeScene && !debugContactPending &&
                     !debugContactEventPending &&
                     !debugContactEventWaitingForEntry &&
                     !debugContactActive &&
                     (debugContactMessageUntilMs == 0 ||
                      static_cast<int32_t>(nowMs - debugContactMessageUntilMs) >= 0);
#endif
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
    exploreSessionActive = true;
    exploreRecoveryPendingSave = false;
    exploreRoutePool = buildExplorePreviewPool(gameState, selectedExploreArea);

    // Keep every possible encounter for this area resident before walking
    // starts. This moves sprite decompression to route entry instead of a
    // random encounter frame.
    ExploreEncounters::Table encounterTable =
        ExploreEncounters::tableForArea(selectedExploreArea);
    uint16_t routeSpecies[ExplorePool::MAX_SOURCE_ENTRIES + 1] = {};
    uint8_t routeSpeciesCount = 0;
    for (uint8_t index = 0; index < encounterTable.count; ++index) {
        uint16_t speciesId = encounterTable.entries[index].speciesId;
        if (speciesId == 0) continue;
        bool duplicate = false;
        for (uint8_t previous = 0; previous < routeSpeciesCount; ++previous) {
            if (routeSpecies[previous] == speciesId) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate && routeSpeciesCount <
            ExplorePool::MAX_SOURCE_ENTRIES) {
            routeSpecies[routeSpeciesCount++] = speciesId;
        }
    }
    exploreRouteMapBlock = 0;
    exploreRouteMapBlockCount = exploreMapCountForRoll(
        selectedExploreArea,
        static_cast<uint8_t>(GameRandom::range(0, 100)));
    exploreRouteMapEncounterCount = 0;
    exploreRouteEncounterCooldownSteps = 0;
    exploreRouteGuaranteedEncounterIndex = 0;
    exploreRouteGuaranteedEncounterPending = false;
    exploreRouteExpeditionSeed = static_cast<uint32_t>(
        GameRandom::range(1, 0x7FFFFFFFU));
    exploreRoutePendingEntryEdge = static_cast<ExploreMapGenerator::Edge>(
        (exploreRouteExpeditionSeed >> 8) & 0x03U);
    exploreRoutePendingFrostLadder = false;
    exploreRoutePendingFrostFall = false;
    exploreRouteFrostFallAtMs = 0;

    exploreRouteIndex = 0;
    exploreRouteSteps = 0;
    exploreRouteMoving = false;
    exploreRouteAutoWalk = false;
    exploreRouteExiting = false;
    exploreRouteExitStartedMs = 0;
    exploreRouteExitDurationMs = 0;
    exploreRoutePlayerWalkActive = false;
    exploreRoutePaused = false;
    exploreRouteComplete = false;
    exploreRouteExitConfirm = false;
    exploreRouteIceSliding = false;
    exploreRouteIceDx = 0;
    exploreRouteIceDy = 0;
    exploreRoutePickupIndex = 0;
    exploreRoutePickupItem = EXPLORE_PICKUP_NONE;
    exploreRoutePickupAvailable = false;
    exploreRouteBossScheduled = false;
    exploreRouteBossIndex = 0;
    exploreRouteBossPending = false;
    exploreRoutePityEligible = false;
    exploreRouteBossSpeciesId = 0;
    exploreRouteBossLevel = 0;
    exploreRouteBossExperiencePercent = 100;
    exploreRouteSpecialKind = ExploreSpecial::Kind::NONE;
    exploreItemEffects.reset();
    bool bossLengthEligible = ExploreRunRules::allowsRegionalBoss(
        exploreRouteMapBlockCount,
        EXPLORE_MAP_MAX_COUNT[selectedExploreArea]);
    if (bossLengthEligible) {
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
            exploreRouteBossScheduled = true;
            exploreRouteBossSpeciesId = config.speciesId;
            exploreRouteBossLevel = ExploreSpecial::encounterLevel(
                exploreRouteSpecialKind,
                ExploreBoss::configForArea(selectedExploreArea).level);
            exploreRouteBossExperiencePercent = config.experiencePercent;
        } else {
            uint8_t misses = gameState.normalBossMissCount[selectedExploreArea];
            uint32_t chance = ExploreBossPity::chanceForMisses(misses);
            if (ExploreBossPity::requiresGuaranteedEligibleRun(misses) ||
                GameRandom::range(0, ExploreBoss::SPAWN_ROLL_MAX) < chance) {
                const ExploreBoss::Config& config =
                    ExploreBoss::configForArea(selectedExploreArea);
                exploreRouteBossScheduled = true;
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

    if (exploreRouteBossScheduled && exploreRouteBossSpeciesId != 0) {
        bool bossAlreadyIncluded = false;
        for (uint8_t index = 0; index < routeSpeciesCount; ++index) {
            if (routeSpecies[index] == exploreRouteBossSpeciesId) {
                bossAlreadyIncluded = true;
                break;
            }
        }
        if (!bossAlreadyIncluded &&
            routeSpeciesCount < ExplorePool::MAX_SOURCE_ENTRIES + 1) {
            routeSpecies[routeSpeciesCount++] = exploreRouteBossSpeciesId;
        }
    }
    PokemonSprites::setPinnedDynamicSpecies(routeSpecies, routeSpeciesCount);
    PokemonSprites::preloadDynamicSpecies(routeSpecies, routeSpeciesCount);
    // Route entry runs under the departure black frame. Warm battle sounds
    // here so a random encounter cannot decode them before its first frame.
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_NORMAL);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_SUPER);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_WEAK);
    AudioManager::ins().preloadSfx(SfxCue::UI_CANCEL);

    if (!generateExploreRouteMap(nowMs)) return false;
    exploreRoutePetFrame = 0;
    exploreRouteMapFrame = 0;
    nextExploreRouteFrameMs = nowMs;
    nextExploreRouteMapFrameMs = nowMs + EXPLORE_ROUTE_MAP_FRAME_MS;
    nextExploreRouteBossFrameMs = nowMs;
    exploreRouteBossWasMoving = false;
    const uint32_t exploredAt =
        static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (gameState.team[slot].origin != Game::Origin::VISITOR) {
            gameState.team[slot].lastExploredAt = exploredAt;
        }
    }
    saveState();
    sceneFlow.enterExploreRoute();
    setMusicContext(MusicContext::EXPLORE);
    toast = nullptr;
    requestFullRender();
    return true;
}

bool AmoledApp::generateExploreRouteMap(uint32_t nowMs) {
    uint32_t mapSeed = ExploreMapGenerator::deriveSeed(
        exploreRouteExpeditionSeed, exploreRouteMapBlock,
        selectedExploreArea);
    bool generated = false;
    for (uint32_t salt : EXPLORE_MAP_GENERATION_RETRY_SALTS) {
        uint32_t candidateSeed = mapSeed ^ salt;
        if (candidateSeed == 0) candidateSeed = EXPLORE_MAP_GENERATION_SAFE_SEED;
        if (ExploreMapGenerator::generate(
                candidateSeed, exploreRoutePendingEntryEdge,
                selectedExploreArea, exploreRouteMap,
                {exploreRouteMapBlock, exploreRouteMapBlockCount,
                 exploreRoutePendingFrostLadder})) {
            generated = true;
            break;
        }
    }
    if (!generated) {
        generated = ExploreMapGenerator::generate(
            EXPLORE_MAP_GENERATION_SAFE_SEED,
            exploreRoutePendingEntryEdge, selectedExploreArea,
            exploreRouteMap,
            {exploreRouteMapBlock, exploreRouteMapBlockCount,
             exploreRoutePendingFrostLadder});
    }
    if (!generated || exploreRouteMap.pathCount == 0) {
        setToast(Ui::Amoled::MAP_FAILED, nowMs);
        return false;
    }

    exploreRoutePath = 0;
    if (exploreRouteMapBlock + 1 == exploreRouteMapBlockCount &&
        exploreRouteBossScheduled) {
        uint8_t bossPaths[ExploreMapGenerator::PATH_COUNT] = {};
        uint8_t bossPathCount = 0;
        for (uint8_t path = 0; path < exploreRouteMap.pathCount; ++path) {
            if (ExploreBoss::canPlaceOnPath(
                    exploreRouteMap.paths[path].pointCount)) {
                bossPaths[bossPathCount++] = path;
            }
        }
        if (bossPathCount > 0) {
            exploreRoutePath = bossPaths[
                GameRandom::range(0, bossPathCount)];
        }
    } else if (exploreRouteMap.pathCount > 1) {
        exploreRoutePath = static_cast<uint8_t>(
            GameRandom::range(0, exploreRouteMap.pathCount));
    }

    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (path.pointCount == 0) {
        setToast(Ui::Amoled::MAP_FAILED, nowMs);
        return false;
    }
    exploreRouteMapEncounterCount = 0;
    exploreRouteBossPending = false;
    exploreRouteBossIndex = 0;
    if (exploreRouteMapBlock + 1 == exploreRouteMapBlockCount &&
        exploreRouteBossScheduled && ExploreBoss::canPlaceOnPath(
            path.pointCount)) {
        uint8_t preferred = ExploreBoss::routeIndex(path.pointCount);
        exploreRouteBossIndex = ExploreIceSlide::nearestNonIceIndex(
            exploreRouteMap, path, preferred, 1,
            static_cast<uint8_t>(path.pointCount - 2));
        if (exploreRouteBossIndex != ExploreIceSlide::INVALID_INDEX) {
            exploreRouteBossPending = true;
        }
    }

    exploreRouteIndex = 0;
    exploreRouteMoving = false;
    exploreRouteFollowerMoving = false;
    exploreRouteIceSliding = false;
    exploreRouteIceDx = 0;
    exploreRouteIceDy = 0;
    exploreRouteDirection = exploreInwardDirection(exploreRouteMap.entry.edge);
    ExploreRouteGeometry::WorldPoint start =
        ExploreRouteGeometry::pathPoint(path, 0);
    exploreRouteWorldX = exploreRouteFromX = exploreRouteTargetX = start.x;
    exploreRouteWorldY = exploreRouteFromY = exploreRouteTargetY = start.y;
    if (exploreRoutePendingFrostFall) {
        exploreRouteIndex = ExploreIceSlide::landingIndex(
            exploreRouteMap, path,
            exploreRouteMap.seed ^ exploreRouteExpeditionSeed);
        ExploreRouteGeometry::WorldPoint landing =
            ExploreRouteGeometry::pathPoint(path, exploreRouteIndex);
        exploreRouteWorldX = exploreRouteFromX = exploreRouteTargetX = landing.x;
        exploreRouteWorldY = exploreRouteFromY = exploreRouteTargetY = landing.y;
        exploreRoutePendingFrostFall = false;
    }
    exploreRouteFollowerWorldX = exploreRouteFollowerFromX =
        exploreRouteFollowerTargetX = start.x;
    exploreRouteFollowerWorldY = exploreRouteFollowerFromY =
        exploreRouteFollowerTargetY = start.y;
    exploreRouteFollowerDirection = exploreRouteDirection;
    const float followerOffset =
        ExploreRouteGeometry::TILE_SIZE * 0.75f;
    switch (static_cast<PokemonSprites::WalkDirection>(
                exploreRouteFollowerDirection)) {
    case PokemonSprites::WalkDirection::LEFT:
        exploreRouteFollowerWorldX += followerOffset;
        break;
    case PokemonSprites::WalkDirection::UP:
        exploreRouteFollowerWorldY += followerOffset;
        break;
    case PokemonSprites::WalkDirection::RIGHT:
        exploreRouteFollowerWorldX -= followerOffset;
        break;
    case PokemonSprites::WalkDirection::DOWN:
    default:
        exploreRouteFollowerWorldY -= followerOffset;
        break;
    }
    exploreRouteFollowerWorldX = std::clamp(
        exploreRouteFollowerWorldX, 0.0f,
        static_cast<float>(ExploreMapGenerator::WIDTH *
                           ExploreRouteGeometry::TILE_SIZE));
    exploreRouteFollowerWorldY = std::clamp(
        exploreRouteFollowerWorldY, 0.0f,
        static_cast<float>(ExploreMapGenerator::HEIGHT *
                           ExploreRouteGeometry::TILE_SIZE));
    exploreRouteFollowerFromX = exploreRouteFollowerTargetX =
        exploreRouteFollowerWorldX;
    exploreRouteFollowerFromY = exploreRouteFollowerTargetY =
        exploreRouteFollowerWorldY;
    exploreRouteFollowerFrame = 0;
    nextExploreRouteBossFrameMs = nowMs;
    exploreRouteBossWasMoving = false;
    updateExploreRouteCamera();
    placeExploreRoutePickup();
    requestFullRender();
    return true;
}

void AmoledApp::placeExploreRoutePickup() {
    exploreRoutePickupIndex = 0;
    exploreRoutePickupItem = EXPLORE_PICKUP_NONE;
    exploreRoutePickupAvailable = false;
    exploreRouteGuaranteedEncounterIndex = 0;
    exploreRouteGuaranteedEncounterPending = false;

    if (exploreRouteMap.pathCount == 0) return;
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (path.pointCount < 2) return;

    if (exploreRouteBossPending) return;

    if (exploreRouteMapBlock + 1 == exploreRouteMapBlockCount) {
        uint8_t preferred = path.pointCount >= 3
            ? static_cast<uint8_t>(path.pointCount - 2)
            : static_cast<uint8_t>(path.pointCount - 1);
        uint8_t last = preferred;
        exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            exploreRouteMap, path, preferred, 1, last);
        if (exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) {
            exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
                exploreRouteMap, path, preferred, 1,
                static_cast<uint8_t>(path.pointCount - 1));
        }
        if (exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        exploreRoutePickupItem = rollExplorePickup(
            selectedExploreArea, gameState.stepsToday);
        if (exploreRoutePickupItem == EXPLORE_PICKUP_NONE) {
            exploreRoutePickupItem = EXPLORE_PICKUP_COIN;
        }
        exploreRoutePickupAvailable = true;
        return;
    }

    // Match Stick: place the pickup in the middle portion of the route and
    // move it to the nearest non-ice point so sliding cannot skip it.
    if (path.pointCount < 3) {
        exploreRoutePickupIndex = ExploreIceSlide::nearestNonIceIndex(
            exploreRouteMap, path,
            static_cast<uint8_t>(path.pointCount - 1),
            1, static_cast<uint8_t>(path.pointCount - 1));
        if (exploreRoutePickupIndex == ExploreIceSlide::INVALID_INDEX) return;
        exploreRoutePickupItem = rollExplorePickup(
            selectedExploreArea, gameState.stepsToday);
        exploreRoutePickupAvailable =
            exploreRoutePickupItem != EXPLORE_PICKUP_NONE;
        return;
    }

    bool choosePickup = GameRandom::range(0, 10000) <
                        EXPLORE_MAP_PICKUP_CHANCE;
    if (!choosePickup && exploreCanScheduleGuaranteedEncounter(
            path.pointCount, exploreRouteEncounterCooldownSteps)) {
        uint8_t first = static_cast<uint8_t>(
            exploreRouteEncounterCooldownSteps + 1);
        uint8_t last = static_cast<uint8_t>(path.pointCount - 2);
        exploreRouteGuaranteedEncounterIndex = ExploreIceSlide::nearestNonIceIndex(
            exploreRouteMap, path,
            exploreGuaranteedEncounterIndex(
                path.pointCount, exploreRouteEncounterCooldownSteps),
            first, last);
        if (exploreRouteGuaranteedEncounterIndex !=
            ExploreIceSlide::INVALID_INDEX) {
            exploreRouteGuaranteedEncounterPending = true;
            return;
        }
    }

    uint8_t first = std::max<uint8_t>(1, path.pointCount / 3);
    uint8_t last = std::min<uint8_t>(path.pointCount - 2,
                                     path.pointCount * 3 / 4);
    if (first > last) first = last = path.pointCount / 2;

    uint8_t preferred = static_cast<uint8_t>(
        GameRandom::range(first, static_cast<uint32_t>(last) + 1));
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
}

bool AmoledApp::beginExploreRouteStep(uint32_t nowMs) {
    if (exploreRouteMoving || exploreRoutePaused || exploreRouteComplete ||
        exploreRoutePendingFrostFall ||
        exploreRouteMap.pathCount == 0 ||
        exploreRoutePath >= exploreRouteMap.pathCount) {
        return false;
    }
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (exploreRouteIndex + 1 >= path.pointCount) {
        if (exploreRouteMapBlock + 1 >= exploreRouteMapBlockCount) {
            beginExploreRouteExit(nowMs);
        } else {
            finishExploreRouteAtEnd(nowMs);
        }
        requestRenderRows(HOME_HEADER_HEIGHT, 448);
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
    exploreRouteFollowerFromX = exploreRouteFollowerWorldX;
    exploreRouteFollowerFromY = exploreRouteFollowerWorldY;
    ++exploreRouteIndex;
    ExploreRouteGeometry::WorldPoint target =
        ExploreRouteGeometry::pathPoint(path, exploreRouteIndex);
    exploreRouteTargetX = target.x;
    exploreRouteTargetY = target.y;
    exploreRouteFollowerTargetX = exploreRouteFollowerFromX;
    exploreRouteFollowerTargetY = exploreRouteFollowerFromY;
    if (exploreRouteIndex >= EXPLORE_ROUTE_FOLLOWER_GAP_STEPS) {
        ExploreRouteGeometry::WorldPoint followerTarget =
            ExploreRouteGeometry::pathPoint(
                path, static_cast<uint8_t>(
                          exploreRouteIndex - EXPLORE_ROUTE_FOLLOWER_GAP_STEPS));
        exploreRouteFollowerTargetX = followerTarget.x;
        exploreRouteFollowerTargetY = followerTarget.y;
    }
    exploreRouteDirection = exploreDirectionForDelta(
        exploreRouteTargetX - exploreRouteFromX,
        exploreRouteTargetY - exploreRouteFromY,
        exploreRouteDirection);
    exploreRouteFollowerDirection = exploreDirectionForDelta(
        exploreRouteFollowerTargetX - exploreRouteFollowerFromX,
        exploreRouteFollowerTargetY - exploreRouteFollowerFromY,
        exploreRouteFollowerDirection);
    exploreRouteMoveStartedMs = nowMs;
    nextExploreRouteFrameMs = nowMs;
    exploreRouteMoving = true;
    exploreRouteFollowerMoving =
        std::fabs(exploreRouteFollowerTargetX - exploreRouteFollowerFromX) >= 0.01f ||
        std::fabs(exploreRouteFollowerTargetY - exploreRouteFollowerFromY) >= 0.01f;
#if STICKMON_ENABLE_DEBUG_FEATURES
    const ExploreMapGenerator::Point& fromPoint =
        path.points[exploreRouteIndex - 1];
    const ExploreMapGenerator::Point& targetPoint =
        path.points[exploreRouteIndex];
    Platform::logf(
        "[AmoledExploreDiag] step block=%u path=%u index=%u/%u "
        "tileFrom=(%u,%u) tileTarget=(%u,%u) "
        "from=(%.2f,%.2f) target=(%.2f,%.2f) delta=(%.2f,%.2f) "
        "dir=%u followerFrom=(%.2f,%.2f) followerTarget=(%.2f,%.2f) "
        "followerDir=%u\n",
        static_cast<unsigned>(exploreRouteMapBlock),
        static_cast<unsigned>(exploreRoutePath),
        static_cast<unsigned>(exploreRouteIndex),
        static_cast<unsigned>(path.pointCount),
        static_cast<unsigned>(fromPoint.x), static_cast<unsigned>(fromPoint.y),
        static_cast<unsigned>(targetPoint.x),
        static_cast<unsigned>(targetPoint.y),
        static_cast<double>(exploreRouteFromX),
        static_cast<double>(exploreRouteFromY),
        static_cast<double>(exploreRouteTargetX),
        static_cast<double>(exploreRouteTargetY),
        static_cast<double>(exploreRouteTargetX - exploreRouteFromX),
        static_cast<double>(exploreRouteTargetY - exploreRouteFromY),
        static_cast<unsigned>(exploreRouteDirection),
        static_cast<double>(exploreRouteFollowerFromX),
        static_cast<double>(exploreRouteFollowerFromY),
        static_cast<double>(exploreRouteFollowerTargetX),
        static_cast<double>(exploreRouteFollowerTargetY),
        static_cast<unsigned>(exploreRouteFollowerDirection));
#endif
    return true;
}

void AmoledApp::beginExploreRouteExit(uint32_t nowMs) {
    if (exploreRouteExiting || exploreRouteComplete ||
        exploreRoutePath >= exploreRouteMap.pathCount) {
        return;
    }
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    exploreRouteFromX = exploreRouteWorldX;
    exploreRouteFromY = exploreRouteWorldY;
    exploreRouteTargetX = exploreRouteWorldX;
    exploreRouteTargetY = exploreRouteWorldY;
    switch (path.exit.edge) {
    case ExploreMapGenerator::Edge::TOP:
        exploreRouteTargetY = -EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::RIGHT:
        exploreRouteTargetX = EXPLORE_ROUTE_WORLD_WIDTH + EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::BOTTOM:
        exploreRouteTargetY = EXPLORE_ROUTE_WORLD_HEIGHT + EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    case ExploreMapGenerator::Edge::LEFT:
        exploreRouteTargetX = -EXPLORE_ROUTE_EXIT_MARGIN;
        break;
    }
    exploreRouteFollowerFromX = exploreRouteFollowerWorldX;
    exploreRouteFollowerFromY = exploreRouteFollowerWorldY;
    exploreRouteFollowerTargetX = exploreRouteTargetX;
    exploreRouteFollowerTargetY = exploreRouteTargetY;
    exploreRouteDirection = exploreDirectionForDelta(
        exploreRouteTargetX - exploreRouteFromX,
        exploreRouteTargetY - exploreRouteFromY,
        exploreRouteDirection);
    exploreRouteFollowerDirection = exploreDirectionForDelta(
        exploreRouteFollowerTargetX - exploreRouteFollowerFromX,
        exploreRouteFollowerTargetY - exploreRouteFollowerFromY,
        exploreRouteFollowerDirection);
    const float leaderDistance = std::hypot(
        exploreRouteTargetX - exploreRouteFromX,
        exploreRouteTargetY - exploreRouteFromY);
    const float followerDistance = std::hypot(
        exploreRouteFollowerTargetX - exploreRouteFollowerFromX,
        exploreRouteFollowerTargetY - exploreRouteFollowerFromY);
    const bool hasFollower = exploreRouteHasCompanion(gameState);
    const float longestDistance = hasFollower
        ? std::max(leaderDistance, followerDistance) : leaderDistance;
    exploreRouteExitDurationMs = static_cast<uint32_t>(std::max(
        500.0f, longestDistance / EXPLORE_ROUTE_EXIT_SPEED * 1000.0f));
    exploreRouteExitStartedMs = nowMs;
    exploreRouteMoveStartedMs = nowMs;
    nextExploreRouteFrameMs = nowMs;
    exploreRouteMoving = true;
    exploreRouteFollowerMoving = hasFollower && followerDistance > 0.01f;
    exploreRouteExiting = true;
    exploreRouteAutoWalk = false;
    exploreRoutePlayerWalkActive = false;
    requestFullRender();
}

bool AmoledApp::advanceExploreRouteWalkFrames(uint32_t nowMs) {
    if (!exploreRouteMoving ||
        static_cast<int32_t>(nowMs - nextExploreRouteFrameMs) < 0) {
        return false;
    }
    exploreRoutePetFrame = static_cast<uint8_t>(exploreRoutePetFrame + 1);
    if (exploreRouteFollowerMoving) {
        exploreRouteFollowerFrame = static_cast<uint8_t>(
            exploreRouteFollowerFrame + 1);
    }
    nextExploreRouteFrameMs = nowMs + EXPLORE_ROUTE_FRAME_MS;
    return true;
}

void AmoledApp::updateExploreRoute(uint32_t nowMs) {
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE ||
        exploreRoutePaused ||
        exploreRouteComplete) {
        return;
    }
    if (exploreRoutePendingFrostFall) {
        if (static_cast<int32_t>(nowMs - exploreRouteFrostFallAtMs) >= 0) {
            finishExploreRouteAtEnd(nowMs);
            requestFullRender();
        }
        return;
    }
    if (exploreRouteExiting) {
        advanceExploreRouteWalkFrames(nowMs);
        const uint32_t elapsed = nowMs - exploreRouteExitStartedMs;
        const float progress = std::min(
            1.0f, elapsed / static_cast<float>(
                std::max<uint32_t>(1, exploreRouteExitDurationMs)));
        exploreRouteWorldX = exploreRouteFromX +
            (exploreRouteTargetX - exploreRouteFromX) * progress;
        exploreRouteWorldY = exploreRouteFromY +
            (exploreRouteTargetY - exploreRouteFromY) * progress;
        if (exploreRouteFollowerMoving) {
            exploreRouteFollowerWorldX = exploreRouteFollowerFromX +
                (exploreRouteFollowerTargetX - exploreRouteFollowerFromX) * progress;
            exploreRouteFollowerWorldY = exploreRouteFollowerFromY +
                (exploreRouteFollowerTargetY - exploreRouteFollowerFromY) * progress;
        }
        const int16_t previousCameraX = exploreRouteCameraX;
        const int16_t previousCameraY = exploreRouteCameraY;
        updateExploreRouteCamera();
        if (previousCameraX != exploreRouteCameraX ||
            previousCameraY != exploreRouteCameraY) {
            requestRenderRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
        } else {
            requestExploreRouteDynamicRender();
        }
        if (progress < 1.0f) return;
        exploreRouteWorldX = exploreRouteTargetX;
        exploreRouteWorldY = exploreRouteTargetY;
        exploreRouteFollowerWorldX = exploreRouteFollowerTargetX;
        exploreRouteFollowerWorldY = exploreRouteFollowerTargetY;
        exploreRouteMoving = false;
        exploreRouteFollowerMoving = false;
        exploreRouteExiting = false;
        finishExploreRouteAtEnd(nowMs);
        requestFullRender();
        return;
    }
    if (exploreRouteBossPending &&
        static_cast<int32_t>(nowMs - nextExploreRouteBossFrameMs) >= 0) {
        const bool bossMoving = exploreRouteBossPatrolMoving(nowMs);
        if (bossMoving || exploreRouteBossWasMoving) {
            requestExploreRouteBossRender();
        }
        exploreRouteBossWasMoving = bossMoving;
        nextExploreRouteBossFrameMs = nowMs + EXPLORE_ROUTE_BOSS_FRAME_MS;
    }
    if (!exploreRouteMoving) {
        if (exploreRouteAutoWalk || exploreRoutePlayerWalkActive) {
            beginExploreRouteStep(nowMs);
        }
        return;
    }

    uint32_t elapsed = nowMs - exploreRouteMoveStartedMs;
    float progress = std::min(
        1.0f, elapsed / static_cast<float>(EXPLORE_ROUTE_STEP_MS));
    exploreRouteWorldX = exploreRouteFromX +
        (exploreRouteTargetX - exploreRouteFromX) * progress;
    exploreRouteWorldY = exploreRouteFromY +
        (exploreRouteTargetY - exploreRouteFromY) * progress;
    if (exploreRouteFollowerMoving) {
        uint32_t followerElapsed = elapsed > EXPLORE_ROUTE_FOLLOWER_DELAY_MS
            ? elapsed - EXPLORE_ROUTE_FOLLOWER_DELAY_MS : 0;
        float followerProgress = elapsed > EXPLORE_ROUTE_FOLLOWER_DELAY_MS
            ? std::min(1.0f, followerElapsed /
                static_cast<float>(EXPLORE_ROUTE_STEP_MS)) : 0.0f;
        exploreRouteFollowerWorldX = exploreRouteFollowerFromX +
            (exploreRouteFollowerTargetX - exploreRouteFollowerFromX) *
                followerProgress;
        exploreRouteFollowerWorldY = exploreRouteFollowerFromY +
            (exploreRouteFollowerTargetY - exploreRouteFollowerFromY) *
                followerProgress;
    }
    int16_t previousCameraX = exploreRouteCameraX;
    int16_t previousCameraY = exploreRouteCameraY;
    updateExploreRouteCamera();
    bool cameraMoved = previousCameraX != exploreRouteCameraX ||
                       previousCameraY != exploreRouteCameraY;
    if (cameraMoved) {
        // The cached map is keyed by viewport. Rebuild the complete map band
        // when the camera scrolls instead of compositing two viewports.
        requestRenderRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
    }

    if (advanceExploreRouteWalkFrames(nowMs)) {
        if (cameraMoved) {
            requestRenderRows(0, EXPLORE_ROUTE_VIEW_HEIGHT);
        } else {
            requestExploreRouteDynamicRender();
        }
    }

    if (static_cast<int32_t>(nowMs - nextExploreRouteMapFrameMs) >= 0) {
        do {
            exploreRouteMapFrame = static_cast<uint8_t>(
                exploreRouteMapFrame + 1);
            nextExploreRouteMapFrameMs += EXPLORE_ROUTE_MAP_FRAME_MS;
        } while (static_cast<int32_t>(nowMs - nextExploreRouteMapFrameMs) >= 0);
        if (exploreRouteMap.hasCreek || exploreRouteMap.hasWaterfall ||
            exploreRouteMap.hasCoast) {
            requestExploreRouteMapAnimationRender();
        }
    }

    if (progress < 1.0f) return;
    exploreRouteWorldX = exploreRouteTargetX;
    exploreRouteWorldY = exploreRouteTargetY;
    exploreRouteFollowerWorldX = exploreRouteFollowerTargetX;
    exploreRouteFollowerWorldY = exploreRouteFollowerTargetY;
    exploreRouteMoving = false;
    exploreRouteFollowerMoving = false;
    ++exploreRouteSteps;
    recoverExploreTeamForCompletedSteps();
    bool encounterBlockedThisStep =
        exploreRouteEncounterCooldownSteps > 0;
    bool repelActiveThisStep = exploreItemEffects.repelStepsRemaining() > 0;
    exploreRouteEncounterCooldownSteps = exploreCooldownAfterStep(
        exploreRouteEncounterCooldownSteps);
    exploreItemEffects.completeWalkStep();
    gameState.stepsToday = static_cast<uint16_t>(std::min<uint32_t>(
        60000, static_cast<uint32_t>(gameState.stepsToday) + 1));
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (selectedExploreArea == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA &&
        exploreRouteMapBlock + 1 < exploreRouteMapBlockCount &&
        ExploreIceSlide::isCrackedIce(exploreRouteMap, path, exploreRouteIndex)) {
        ExploreIceSlide::breakIce(exploreRouteMap, path, exploreRouteIndex);
        renderCaches_.exploreWorld.invalidate();
        exploreRoutePendingFrostFall = true;
        exploreRouteFrostFallAtMs = nowMs + 240;
        exploreRouteIceSliding = false;
        exploreRouteIceDx = 0;
        exploreRouteIceDy = 0;
        requestFullRender();
        return;
    }
    bool continueIce = exploreRouteIceSliding && ExploreIceSlide::continues(
        exploreRouteMap, path, exploreRouteIndex,
        exploreRouteIceDx, exploreRouteIceDy);
    if (!continueIce) {
        exploreRouteIceSliding = false;
        exploreRouteIceDx = 0;
        exploreRouteIceDy = 0;
        resolveExploreStepEvent(nowMs, encounterBlockedThisStep,
                                repelActiveThisStep);
    }
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE) {
        requestFullRender();
        return;
    }
    if (exploreRouteIndex + 1 >= path.pointCount) {
        if (exploreRouteMapBlock + 1 >= exploreRouteMapBlockCount) {
            beginExploreRouteExit(nowMs);
            return;
        }
        if (finishExploreRouteAtEnd(nowMs)) return;
    } else if (exploreRoutePlayerWalkActive &&
               exploreRouteBossPending &&
               exploreRouteIndex + 1 == exploreRouteBossIndex) {
        // Stick stops one route point before the regional boss. The next tap
        // enters the boss point; Agent mode continues without this pause.
        exploreRoutePlayerWalkActive = false;
    } else if (continueIce || exploreRouteAutoWalk ||
               exploreRoutePlayerWalkActive) {
        beginExploreRouteStep(nowMs);
    }
    requestExploreRouteDynamicRender();
}

bool AmoledApp::recoverExploreTeamForCompletedSteps() {
    if (!ExploreRunRules::isRecoveryStep(exploreRouteSteps)) return false;

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
    if (recovered) exploreRecoveryPendingSave = true;
    return recovered;
}

void AmoledApp::requestExploreRouteDynamicRender() {
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE) return;

    // The route renderer restores the cached full-screen map underneath this
    // band before drawing the previous/current pet position.
    constexpr int PET_TOP_MARGIN = 120;
    // Route points are now sprite centers, so the dynamic band must cover the
    // same-sized lower half as the upper half to avoid stale trailing pixels.
    constexpr int PET_BOTTOM_MARGIN = 120;
    int previousY = static_cast<int>(std::lround((exploreRouteFromY -
        exploreRouteCameraY) * AmoledUi::RESOURCE_SCALE));
    int currentY = static_cast<int>(std::lround((exploreRouteWorldY -
        exploreRouteCameraY) * AmoledUi::RESOURCE_SCALE));
    int top = std::min(previousY, currentY) - PET_TOP_MARGIN;
    int bottom = std::max(previousY, currentY) + PET_BOTTOM_MARGIN;
    if (gameState.teamCount > 1 && exploreRouteFollowerMoving) {
        int followerPreviousY = static_cast<int>(std::lround(
            (exploreRouteFollowerFromY - exploreRouteCameraY) *
            AmoledUi::RESOURCE_SCALE));
        int followerCurrentY = static_cast<int>(std::lround(
            (exploreRouteFollowerWorldY - exploreRouteCameraY) *
            AmoledUi::RESOURCE_SCALE));
        top = std::min(top, std::min(followerPreviousY, followerCurrentY) -
                              PET_TOP_MARGIN);
        bottom = std::max(bottom, std::max(followerPreviousY, followerCurrentY) +
                                PET_BOTTOM_MARGIN);
    }
    top = std::clamp(top, 0, EXPLORE_ROUTE_VIEW_HEIGHT - 1);
    bottom = std::clamp(bottom, top + 1, EXPLORE_ROUTE_VIEW_HEIGHT);
    requestRenderRows(static_cast<uint16_t>(top),
                      static_cast<uint16_t>(bottom));
}

void AmoledApp::requestExploreRouteBossRender() {
    if (sceneFlow.current() != AppSceneFlow::Scene::EXPLORE_ROUTE ||
        !exploreRouteBossPending ||
        exploreRoutePath >= exploreRouteMap.pathCount) {
        return;
    }
    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (exploreRouteBossIndex >= path.pointCount) return;

    const ExploreRouteGeometry::WorldPoint point =
        ExploreRouteGeometry::pathPoint(path, exploreRouteBossIndex);
    const int centerY = static_cast<int>(std::lround(
        (point.y - exploreRouteCameraY) * AmoledUi::RESOURCE_SCALE));
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
    requestRenderRows(static_cast<uint16_t>(top),
                      static_cast<uint16_t>(bottom));
}

void AmoledApp::requestExploreRouteMapAnimationRender() {
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
                uint16_t tileId = exploreRouteMap.layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (!GameAssets::isExploreTileAnimated(tileId)) continue;
                int tileTop = mapTop + static_cast<int>(tileY) * tileSize -
                              exploreRouteCameraY * AmoledUi::RESOURCE_SCALE;
                int tileBottom = tileTop + tileSize;
                top = std::min(top, tileTop);
                bottom = std::max(bottom, tileBottom);
            }
        }
    }

    top = std::clamp(top, mapTop, mapBottom);
    bottom = std::clamp(bottom, top, mapBottom);
    if (top < bottom) {
        requestRenderRows(static_cast<uint16_t>(top),
                          static_cast<uint16_t>(bottom));
    }
}

bool AmoledApp::finishExploreRouteAtEnd(uint32_t nowMs) {
    if (exploreRouteBossPending) {
        if (beginExploreEncounter(
                nowMs, true, exploreRouteBossSpeciesId,
                exploreRouteBossLevel, exploreRouteBossExperiencePercent,
                exploreRouteSpecialKind)) {
            exploreRouteBossPending = false;
            exploreRouteAutoWalk = false;
            exploreRoutePlayerWalkActive = false;
            return true;
        }
        // Keep the pending boss at the route end when the active team cannot
        // enter battle yet; a later tap can retry after the player recovers.
        exploreRouteAutoWalk = false;
        exploreRoutePlayerWalkActive = false;
        return false;
    }
    if (exploreRouteMapBlock + 1 < exploreRouteMapBlockCount) {
        const ExploreMapGenerator::Path& path =
            exploreRouteMap.paths[exploreRoutePath];
    exploreRoutePendingEntryEdge = ExploreMapGenerator::opposite(
            path.exit.edge);
        exploreRoutePendingFrostLadder =
            !exploreRoutePendingFrostFall && exploreRoutePath == 1;
        ++exploreRouteMapBlock;
        if (!generateExploreRouteMap(nowMs)) {
            exploreRouteAutoWalk = false;
            exploreRoutePlayerWalkActive = false;
            return false;
        }
        if (exploreRouteAutoWalk || exploreRoutePlayerWalkActive) {
            beginExploreRouteStep(nowMs);
        }
        return true;
    }
    if (exploreRoutePityEligible) {
        ExploreBossPity::increment(gameState, selectedExploreArea);
        exploreRoutePityEligible = false;
        saveState();
    }
    exploreRouteComplete = true;
    exploreRouteAutoWalk = false;
    exploreRoutePlayerWalkActive = false;
    if (autonomousExpedition) {
        // A completed autonomous expedition has no player-facing route screen
        // to acknowledge. Return home so the normal care loop can continue.
        leaveExploreRoute();
        return true;
    }
    requestFullRender();
    return false;
}

void AmoledApp::resolveExploreStepEvent(
    uint32_t nowMs, bool encounterBlockedThisStep,
    bool repelActiveThisStep) {
    bool honeyEncounter = exploreItemEffects.honeyEncounterPending();

    if (exploreRouteBossPending &&
        exploreRouteIndex == exploreRouteBossIndex) {
        if (beginExploreEncounter(
                nowMs, true, exploreRouteBossSpeciesId,
                exploreRouteBossLevel, exploreRouteBossExperiencePercent,
                exploreRouteSpecialKind)) {
            exploreRouteBossPending = false;
            exploreRouteAutoWalk = false;
            exploreRoutePlayerWalkActive = false;
        }
        return;
    }
    if (exploreRoutePickupAvailable &&
        exploreRouteIndex == exploreRoutePickupIndex) {
        resolveExploreRoutePickup(nowMs);
        return;
    }

    const ExploreMapGenerator::Path& path =
        exploreRouteMap.paths[exploreRoutePath];
    if (exploreRouteIndex + 1 >= path.pointCount) return;


    bool guaranteedEncounter = exploreRouteGuaranteedEncounterPending &&
        exploreRouteIndex >= exploreRouteGuaranteedEncounterIndex;
    bool encounterGateBypassed = honeyEncounter;
    bool encounterGateOpen = encounterGateBypassed ||
        exploreEncounterGateOpen(encounterBlockedThisStep,
                                 exploreRouteMapEncounterCount);
    bool repelAllowsEncounter = !repelActiveThisStep ||
                                guaranteedEncounter || honeyEncounter;
    uint16_t encounterChance = EXPLORE_ENCOUNTER_CHANCE[
        std::min<uint8_t>(selectedExploreArea,
                          static_cast<uint8_t>(Game::EXPLORE_AREA_COUNT - 1))];
    bool randomEncounter = false;
    if (encounterGateOpen && repelAllowsEncounter &&
        !guaranteedEncounter && !honeyEncounter) {
        randomEncounter = GameRandom::range(0, 10000) < encounterChance;
    }
    if (encounterGateOpen && repelAllowsEncounter &&
        (guaranteedEncounter || honeyEncounter || randomEncounter) &&
        beginExploreEncounter(nowMs)) {
        ++exploreRouteMapEncounterCount;
        exploreRouteEncounterCooldownSteps =
            EXPLORE_ENCOUNTER_COOLDOWN_STEPS;
        exploreRouteGuaranteedEncounterPending = false;
        if (honeyEncounter) exploreItemEffects.consumeHoneyEncounter();
        return;
    }
    saveState();
}

void AmoledApp::resolveExploreRoutePickup(uint32_t nowMs) {
    if (!exploreRoutePickupAvailable) return;
    // Manual route walking pauses at a pickup so the next tap controls when
    // movement resumes. Autonomous departures must keep their route running;
    // otherwise collecting one item permanently strands the expedition.
    const bool resumeAutoWalk = exploreRouteAutoWalk;
    exploreRoutePickupAvailable = false;
    exploreRouteAutoWalk = false;
    exploreRoutePlayerWalkActive = false;
    exploreRouteAutoWalk = resumeAutoWalk;

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
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t encounterStartMs = Platform::clock().millis();
#endif
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
        ExploreEncounters::Table table =
            ExploreEncounters::tableForArea(selectedExploreArea);
        if (!table.entries || table.count == 0) return false;
        const ExploreEncounters::Entry* picked = nullptr;
        if (exploreRoutePool.count > 0) {
            const uint32_t totalWeight =
                ExplorePool::poolWeightTotal(exploreRoutePool);
            if (totalWeight == 0) return false;
            const ExplorePool::PoolEntry* poolEntry = ExplorePool::entryForRoll(
                exploreRoutePool, GameRandom::range(0, totalWeight));
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
    recordEncounteredSpecies(species->id);
    uint8_t level = levelOverride;
    if (level == 0) {
        const ExploreMapGenerator::Path& path =
            exploreRouteMap.paths[exploreRoutePath];
        const uint8_t targetLevel = ExploreEncounterRules::targetLevel(
            ExploreAreaCatalog::recommendedLevel(selectedExploreArea),
            ExploreAreaCatalog::depthSpread(selectedExploreArea),
            exploreRouteMapBlock, exploreRouteMapBlockCount,
            exploreRouteIndex, path.pointCount);
        level = ExploreEncounterRules::levelForRoll(
            levelMinimum, levelMaximum, targetLevel,
            static_cast<uint8_t>(GameRandom::range(0, 100)));
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
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t spritePrepStartedMs = Platform::clock().millis();
#endif
    PokemonSprites::setDynamicSceneSpecies(dynamicSpecies, 2);
    PokemonSprites::preloadDynamicSpecies(dynamicSpecies, 2);
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t spritePrepMs =
        Platform::clock().millis() - spritePrepStartedMs;
#endif
    BattleSystem::resetVolatile(battlePlayerState);
    BattleSystem::resetVolatile(battleWildState);
    battleTurnController.reset();
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    battleContinuation = BattleContinuation::NONE;
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
    resetBattleProgressionQueue();
    battleAnimationActive = false;
    battleSwitchStage = BattleSwitchStage::NONE;
    pendingBattleSwitchSlot = 0xFF;
    battleSwitchConsumesTurn = false;
    battleAnimationAttackerWild = false;
    battleAnimationHit = false;
    battleAnimationDamage = 0;
    battleAnimationFrame = 0;
    battleAttackLogHeld = false;
    battleImpactAudioPlayed = false;
    battleImpactSfx = 0xFF;
    battleHpAnimationActive = false;
    battleExperienceVisible = false;
    battleExperienceAnimationActive = false;
    battleExperienceAnimationFrom = 0;
    battleExperienceAnimationTo = 0;
    battleBagMode = false;
    battleFoodBond = 0;
    battleAudioPending = false;
    battleAudioReady = false;
    battlePendingSfx = 0xFF;
    battlePendingCrySpecies = 0;
    battleRewardExp = 0;
    battleRewardCoins = 0;
    battleVictoryFinalizePending = false;
    clearBattleLog();
    // Also cover debug battles, which can start without entering a route.
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t sfxPrepStartedMs = Platform::clock().millis();
#endif
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_NORMAL);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_SUPER);
    AudioManager::ins().preloadSfx(SfxCue::DAMAGE_WEAK);
    AudioManager::ins().preloadSfx(SfxCue::UI_CANCEL);
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t sfxPrepMs = Platform::clock().millis() - sfxPrepStartedMs;
#endif
    std::snprintf(battleMessage, sizeof(battleMessage), Ui::Amoled::WILD_FMT,
                  species->name);
    pushBattleLog(nowMs);
    toast = nullptr;
    exploreRoutePlayerWalkActive = false;
    exploreRouteAutoWalk = false;
    sceneFlow.enter(AppSceneFlow::Scene::BATTLE);
    setMusicContext(boss ? MusicContext::BATTLE_SPECIAL
                         : MusicContext::BATTLE);
    requestFullRender();
#if STICKMON_ENABLE_DEBUG_FEATURES
    encounterStartedMs = encounterStartMs;
    encounterPreparedMs = Platform::clock().millis() - encounterStartMs;
    encounterSpritePrepMs = spritePrepMs;
    encounterSfxPrepMs = sfxPrepMs;
    encounterFirstFramePending = true;
#endif
    return true;
}

void AmoledApp::pushBattleLog(uint32_t nowMs, bool invalidate,
                              uint16_t durationMs) {
    if (!battleMessage[0]) return;
    if (battleLogQueueCount >= BATTLE_LOG_QUEUE_CAP) {
        s_battleLogDurations[battleLogHead] = 0;
        battleLogHead = static_cast<uint8_t>(
            (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP);
        --battleLogQueueCount;
    }
    const uint8_t tail = static_cast<uint8_t>(
        (battleLogHead + battleLogQueueCount) % BATTLE_LOG_QUEUE_CAP);
    std::snprintf(s_battleLogQueue[tail], sizeof(s_battleLogQueue[tail]), "%s",
                  battleMessage);
    s_battleLogDurations[tail] = durationMs == 0
        ? BATTLE_LOG_DEFAULT_MS : durationMs;
    ++battleLogQueueCount;
    if (invalidate && serviceBattleLog(nowMs)) {
        requestRenderRows(352, 448);
    }
}

bool AmoledApp::serviceBattleLog(uint32_t nowMs) {
    if (battleAttackLogHeld && battleAnimationActive) return false;
    if (battleLogActive &&
        static_cast<int32_t>(nowMs - battleLogUntil) < 0) {
        return false;
    }
    if (battleLogQueueCount == 0) {
        const bool changed = battleLogActive || battleLogVisibleCount > 0;
        battleLogActive = false;
        battleLogVisibleCount = 0;
        battleLogUntil = 0;
        for (auto& line : battleLogLines) line[0] = '\0';
        return changed;
    }

    uint8_t line = 0;
    if (battleLogVisibleCount < BATTLE_LOG_VISIBLE_CAP) {
        line = battleLogVisibleCount++;
    } else {
        for (uint8_t index = 1; index < BATTLE_LOG_VISIBLE_CAP; ++index) {
            std::memcpy(battleLogLines[index - 1], battleLogLines[index],
                        BATTLE_LOG_LEN);
        }
        line = BATTLE_LOG_VISIBLE_CAP - 1;
    }
    std::memcpy(battleLogLines[line], s_battleLogQueue[battleLogHead],
                BATTLE_LOG_LEN);
    battleLogLines[line][BATTLE_LOG_LEN - 1] = '\0';
    s_battleLogQueue[battleLogHead][0] = '\0';
    const uint16_t durationMs = s_battleLogDurations[battleLogHead] == 0
        ? BATTLE_LOG_DEFAULT_MS : s_battleLogDurations[battleLogHead];
    s_battleLogDurations[battleLogHead] = 0;
    battleLogHead = static_cast<uint8_t>(
        (battleLogHead + 1) % BATTLE_LOG_QUEUE_CAP);
    --battleLogQueueCount;
    battleLogActive = true;
    battleLogUntil = nowMs + durationMs;
    return true;
}

bool AmoledApp::battleLogPlaybackBusy() const {
    return battleLogActive || battleLogQueueCount > 0;
}

void AmoledApp::clearBattleLog() {
    battleLogHead = 0;
    battleLogQueueCount = 0;
    battleLogVisibleCount = 0;
    battleLogUntil = 0;
    battleLogActive = false;
    battleMessage[0] = '\0';
    for (auto& line : s_battleLogQueue) line[0] = '\0';
    for (auto& duration : s_battleLogDurations) duration = 0;
    for (auto& line : battleLogLines) line[0] = '\0';
}

void AmoledApp::enqueueBattleEffectLogs(
    const BattleSystem::EffectResolution& effects, bool attackerWild,
    uint32_t nowMs) {
    const Species* playerSpecies = battlePlayerSlot < gameState.teamCount
        ? findSpecies(gameState.team[battlePlayerSlot].speciesId) : nullptr;
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!playerSpecies || !wildSpecies) return;
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
    for (uint8_t index = 0; index < effects.count; ++index) {
        const auto& outcome = effects.outcomes[index];
        const bool targetWild = outcome.target == MoveEffectTarget::ATTACKER
            ? attackerWild : !attackerWild;
        const char* targetName = targetWild
            ? wildSpecies->name : playerSpecies->name;
        const char* attackerName = attackerWild
            ? wildSpecies->name : playerSpecies->name;
        if (outcome.ability != AbilityId::NONE) {
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::ABILITY_ACTIVATED_FMT,
                          outcome.ability == AbilityId::INTIMIDATE
                              ? attackerName : targetName,
                          abilityName(outcome.ability));
            pushBattleLog(nowMs, false);
        }
        switch (outcome.kind) {
        case BattleSystem::EffectOutcomeKind::STATUS_APPLIED:
        case BattleSystem::EffectOutcomeKind::YAWN_SLEEP:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::STATUS_APPLIED_FMT, targetName,
                          statusLabel(outcome.status));
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_FAILED:
            std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                          Ui::Explore::NO_EFFECT);
            break;
        case BattleSystem::EffectOutcomeKind::CONFUSED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::CONFUSED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::BOUND:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::BOUND_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::STAT_CHANGED: {
            const uint8_t stat = static_cast<uint8_t>(outcome.stat);
            if (stat >= static_cast<uint8_t>(BattleStat::COUNT)) continue;
            std::snprintf(battleMessage, sizeof(battleMessage),
                          outcome.stageDelta > 0 ? Ui::Explore::STAT_ROSE_FMT
                                                 : Ui::Explore::STAT_FELL_FMT,
                          targetName, Ui::Explore::STAT_NAMES[stat]);
            break;
        }
        case BattleSystem::EffectOutcomeKind::DRAINED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::DRAINED_FMT, outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::RECOIL:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::RECOIL_FMT, targetName, outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::HEALED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::HEALED_FMT, targetName, outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::CURED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::CURED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_CLEARED:
        case BattleSystem::EffectOutcomeKind::BIND_ENDED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::BIND_ENDED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::YAWNED:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::YAWNED_FMT, targetName);
            break;
        case BattleSystem::EffectOutcomeKind::STATUS_DAMAGE:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::STATUS_DAMAGE_FMT, targetName,
                          outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::BIND_DAMAGE:
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::BIND_DAMAGE_FMT, targetName,
                          outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::ABILITY_ACTIVATED:
            if (outcome.amount == 0) continue;
            std::snprintf(battleMessage, sizeof(battleMessage),
                          Ui::Explore::ABSORB_HEAL_FMT, targetName,
                          outcome.amount);
            break;
        case BattleSystem::EffectOutcomeKind::FLINCHED:
            continue;
        }
        pushBattleLog(nowMs, false);
    }
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
        battleContinuation = BattleContinuation::ADVANCE_TURN;
        return;
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
            battleContinuation = BattleContinuation::ADVANCE_TURN;
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
        const uint16_t wildHpBefore = battleWild.hpCur;
        uint16_t dealt = std::min<uint16_t>(damage.damage, battleWild.hpCur);
        battleWild.hpCur = static_cast<uint16_t>(battleWild.hpCur - dealt);
        if (dealt > 0) battleTurnDamaged[1] = true;
        const MoveInfo* move = findMove(damage.moveId);
        formatBattleMoveUsed(battleMessage, sizeof(battleMessage),
                             *playerSpecies, move, false);
        battleAttackLogHeld = true;
        pushBattleLog(nowMs, true, BATTLE_ATTACK_LOG_MS);
        BattleSystem::EffectResolution moveEffects{};
        if (move) {
            BattleSystem::recordMoveResult(
                battlePlayerState, player, *playerSpecies, *move,
                !damage.missed && !damage.failed,
                specialSlot);
            if (!damage.missed && !damage.failed &&
                damage.effectiveness > 0) {
                moveEffects = BattleSystem::applyMoveEffects(
                    *move, player, *playerSpecies, battlePlayerState,
                    battleWild, *wildSpecies, battleWildState, dealt,
                    battleTurnPlan.hasActionAfter(
                        battleTurnActionIndex, BattleTurnController::Side::WILD));
            }
        }
        formatBattleOutcome(battleMessage, sizeof(battleMessage), damage,
                            dealt, moveEffects, move, false);
        // Keep the move line visible through the attack; the result line is
        // presented after the animation releases the log queue.
        pushBattleLog(nowMs, false, BATTLE_RESULT_LOG_MS);
        enqueueBattleEffectLogs(moveEffects, false, nowMs);
        battleAnimationActive = true;
        battleAnimationAttackerWild = false;
        battleAnimationHit = !damage.missed && !damage.failed && dealt > 0;
        battleAnimationDamage = dealt;
        battleAnimationDurationMs =
            BATTLE_HP_DAMAGE_DELAY_MS + BATTLE_HP_ANIMATION_MS +
            BATTLE_GAUGE_FRAME_MS;
        battleAnimationFrame = 1;
        battleAnimationStartedMs = Platform::clock().millis();
        battleContinuation = BattleContinuation::ADVANCE_TURN;
        if (battleAnimationHit && battleWild.hpCur != wildHpBefore) {
            startBattleHpAnimation(
                true, wildHpBefore, battleWild.hpCur, battleWild.hpMax,
                battleAnimationStartedMs + BATTLE_HP_DAMAGE_DELAY_MS);
        }
        battleAudioPending = true;
        battleAudioReady = false;
        battlePendingSfx = 0xFF;
        battleImpactAudioPlayed = false;
        battleImpactSfx = static_cast<uint8_t>(
            battleAnimationHit
                ? (damage.effectiveness > 100 ? SfxCue::DAMAGE_SUPER
                   : damage.effectiveness < 100 ? SfxCue::DAMAGE_WEAK
                                                 : SfxCue::DAMAGE_NORMAL)
                : SfxCue::UI_CANCEL);
        battlePendingCrySpecies = player.speciesId;
        Platform::logf("[BattleAnim] start side=player hit=%u damage=%u\n",
                       battleAnimationHit ? 1 : 0, dealt);
        requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
        return;
    }
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
        const int8_t switchSlot = availableBattleSwitchSlot();
        const bool hasSwitch = switchSlot >= 0;
        battlePhase = hasSwitch ? BattleViewModel::Phase::ACTION
                                : BattleViewModel::Phase::DEFEAT;
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Amoled::PET_FAINTED);
        pushBattleLog(nowMs);
        saveState();
        battleTurnPlan = BattleTurnController::TurnPlan{};
        battleTurnActionIndex = 0;
        if (hasSwitch) {
            performBattleSwitch(static_cast<uint8_t>(switchSlot), false,
                                nowMs);
            return true;
        }
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
        startBattleExperienceAnimation(nowMs);
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
    pendingBattleSwitchSlot = teamSlot;
    battleSwitchConsumesTurn = consumesTurn;
    battleSwitchStage = BattleSwitchStage::RETREATING;
    battleSwitchStartedMs = Platform::clock().millis();
    nextBattleSwitchFrameMs = battleSwitchStartedMs;
    battlePhase = BattleViewModel::Phase::ACTION;
    battlePressedItem = 0xFF;
    requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
}

void AmoledApp::updateBattleSwitch(uint32_t nowMs) {
    if (battleSwitchStage == BattleSwitchStage::NONE) return;
    if (static_cast<int32_t>(nowMs - nextBattleSwitchFrameMs) >= 0) {
        nextBattleSwitchFrameMs = nowMs + BATTLE_SWITCH_FRAME_MS;
        requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
    }
    if (nowMs - battleSwitchStartedMs < BATTLE_SWITCH_PHASE_MS) return;
    if (battleSwitchStage == BattleSwitchStage::RETREATING) {
        if (pendingBattleSwitchSlot >= gameState.teamCount ||
            pendingBattleSwitchSlot >= Game::TEAM_CAP ||
            gameState.team[pendingBattleSwitchSlot].fainted ||
            gameState.team[pendingBattleSwitchSlot].hpCur == 0) {
            battleSwitchStage = BattleSwitchStage::NONE;
            pendingBattleSwitchSlot = 0xFF;
            battleSwitchConsumesTurn = false;
            requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
            return;
        }
        battlePlayerSlot = pendingBattleSwitchSlot;
        pendingBattleSwitchSlot = 0xFF;
        const Species* candidateSpecies = findSpecies(
            gameState.team[battlePlayerSlot].speciesId);
        const Species* wildSpecies = findSpecies(battleWild.speciesId);
        if (!candidateSpecies || !wildSpecies) {
            battleSwitchStage = BattleSwitchStage::NONE;
            battleSwitchConsumesTurn = false;
            return;
        }
        BattleSystem::resetVolatile(battlePlayerState);
        battleTurnController.resetPlayerAi();
        BattleSystem::EffectResolution effects;
        BattleSystem::applyEntryAbility(
            *candidateSpecies, battlePlayerState, *wildSpecies,
            battleWildState, effects);
        uint16_t dynamicSpecies[] = {
            gameState.team[battlePlayerSlot].speciesId, battleWild.speciesId};
        PokemonSprites::setDynamicSceneSpecies(dynamicSpecies, 2);
        PokemonSprites::preloadDynamicSpecies(dynamicSpecies, 2);
        enqueueBattleEffectLogs(effects, false, nowMs);
        battleSwitchStage = BattleSwitchStage::ENTERING;
        battleSwitchStartedMs = Platform::clock().millis();
        nextBattleSwitchFrameMs = battleSwitchStartedMs;
        requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
        return;
    }
    battleSwitchStage = BattleSwitchStage::NONE;
    battleSwitchStartedMs = 0;
    const Species* candidateSpecies = findSpecies(
        gameState.team[battlePlayerSlot].speciesId);
    if (candidateSpecies) {
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::SWITCH_IN_FMT, candidateSpecies->name);
        pushBattleLog(nowMs);
    }
    saveState();
    if (battleSwitchConsumesTurn) {
        battleContinuation = BattleContinuation::WILD_TURN;
    }
    battleSwitchConsumesTurn = false;
    requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
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
            battleContinuation = BattleContinuation::ADVANCE_TURN;
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
        const uint16_t playerHpBefore = player.hpCur;
        uint16_t dealt = std::min<uint16_t>(damage.damage, player.hpCur);
        player.hpCur = static_cast<uint16_t>(player.hpCur - dealt);
        if (dealt > 0) battleTurnDamaged[0] = true;
        const MoveInfo* move = findMove(damage.moveId);
        formatBattleMoveUsed(battleMessage, sizeof(battleMessage),
                             *wildSpecies, move, true);
        battleAttackLogHeld = true;
        pushBattleLog(nowMs, true, BATTLE_ATTACK_LOG_MS);
        BattleSystem::EffectResolution moveEffects{};
        if (move) {
            BattleSystem::recordMoveResult(
                battleWildState, battleWild, *wildSpecies, *move,
                !damage.missed && !damage.failed, specialSlot);
            if (!damage.missed && !damage.failed &&
                damage.effectiveness > 0) {
                moveEffects = BattleSystem::applyMoveEffects(
                    *move, battleWild, *wildSpecies, battleWildState,
                    player, *playerSpecies, battlePlayerState, dealt,
                    battleTurnPlan.hasActionAfter(
                        battleTurnActionIndex, BattleTurnController::Side::PLAYER));
            }
        }
        formatBattleOutcome(battleMessage, sizeof(battleMessage), damage,
                            dealt, moveEffects, move, true);
        // Defer the footer update while this attack animation is running.
        pushBattleLog(nowMs, false, BATTLE_RESULT_LOG_MS);
        enqueueBattleEffectLogs(moveEffects, true, nowMs);
        battleAnimationActive = true;
        battleAnimationAttackerWild = true;
        battleAnimationHit = !damage.missed && !damage.failed && dealt > 0;
        battleAnimationDamage = dealt;
        battleAnimationDurationMs =
            BATTLE_HP_DAMAGE_DELAY_MS + BATTLE_HP_ANIMATION_MS +
            BATTLE_GAUGE_FRAME_MS;
        battleAnimationFrame = 1;
        battleAnimationStartedMs = Platform::clock().millis();
        battleContinuation = BattleContinuation::ADVANCE_TURN;
        if (battleAnimationHit && player.hpCur != playerHpBefore) {
            startBattleHpAnimation(
                false, playerHpBefore, player.hpCur, player.hpMax,
                battleAnimationStartedMs + BATTLE_HP_DAMAGE_DELAY_MS);
        }
        battleAudioPending = true;
        battleAudioReady = false;
        battlePendingSfx = 0xFF;
        battleImpactAudioPlayed = false;
        battleImpactSfx = static_cast<uint8_t>(
            battleAnimationHit
                ? (damage.effectiveness > 100 ? SfxCue::DAMAGE_SUPER
                   : damage.effectiveness < 100 ? SfxCue::DAMAGE_WEAK
                                                 : SfxCue::DAMAGE_NORMAL)
                : SfxCue::UI_CANCEL);
        battlePendingCrySpecies = battleWild.speciesId;
        Platform::logf("[BattleAnim] start side=wild hit=%u damage=%u\n",
                       battleAnimationHit ? 1 : 0, dealt);
        requestRenderRows(0, BATTLE_ANIMATION_RENDER_END);
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
        battleContinuation = BattleContinuation::ADVANCE_TURN;
    }
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

int8_t AmoledApp::availableBattleSwitchSlot() const {
    for (uint8_t slot = 0;
         slot < gameState.teamCount && slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& candidate = gameState.team[slot];
        if (!candidate.fainted && candidate.hpCur > 0) {
            return static_cast<int8_t>(slot);
        }
    }
    return -1;
}

void AmoledApp::startBattleHpAnimation(bool wildSide, uint16_t fromHp,
                                       uint16_t toHp, uint16_t maxHp,
                                       uint32_t startedMs) {
    battleHpAnimationWild = wildSide;
    battleHpAnimationFrom = fromHp;
    battleHpAnimationTo = toHp;
    battleHpAnimationMax = std::max<uint16_t>(1, maxHp);
    battleHpAnimationStartedMs = startedMs;
    nextBattleHpAnimationFrameMs = startedMs;
    battleHpAnimationActive = fromHp != toHp;
}

uint8_t AmoledApp::battleHpPercentForRender(
    bool wildSide, uint16_t currentHp, uint16_t maxHp,
    uint32_t nowMs) const {
    uint32_t shownHp = currentHp;
    uint32_t shownMax = std::max<uint16_t>(1, maxHp);
    if (battleHpAnimationActive &&
        battleHpAnimationWild == wildSide) {
        shownMax = battleHpAnimationMax;
        if (static_cast<int32_t>(nowMs - battleHpAnimationStartedMs) <= 0) {
            shownHp = battleHpAnimationFrom;
        } else {
            uint32_t elapsed = std::min<uint32_t>(
                BATTLE_HP_ANIMATION_MS,
                nowMs - battleHpAnimationStartedMs);
            if (battleHpAnimationFrom >= battleHpAnimationTo) {
                const uint32_t distance =
                    battleHpAnimationFrom - battleHpAnimationTo;
                shownHp = battleHpAnimationFrom -
                    distance * elapsed / BATTLE_HP_ANIMATION_MS;
            } else {
                const uint32_t distance =
                    battleHpAnimationTo - battleHpAnimationFrom;
                shownHp = battleHpAnimationFrom +
                    distance * elapsed / BATTLE_HP_ANIMATION_MS;
            }
        }
    }
    return static_cast<uint8_t>(std::min<uint32_t>(
        100, shownHp * 100U / shownMax));
}

void AmoledApp::startBattleExperienceAnimation(uint32_t nowMs) {
    battleExperienceVisible = true;
    battleExperienceAnimationActive = false;
    battleExperienceAnimationStartedMs = nowMs;
    nextBattleExperienceAnimationFrameMs = nowMs;
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) {
        return;
    }
    const Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const Species* species = findSpecies(player.speciesId);
    if (!species) return;
    bool hasReserve = false;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& reserve = gameState.team[slot];
        if (!reserve.fainted && reserve.hpCur > 0) {
            hasReserve = true;
            break;
        }
    }
    const BattleSystem::ExperienceAwards awards =
        BattleSystem::experienceAwards(battleRewardExp, hasReserve);
    Game::MonsterRuntime preview = player;
    Game::ExperienceService::add(preview, *species, awards.active);
    battleExperienceAnimationFrom = player.exp;
    battleExperienceAnimationTo = preview.exp;
    battleExperienceAnimationActive =
        battleExperienceAnimationTo > battleExperienceAnimationFrom;
    if (battleExperienceAnimationActive) {
        AudioManager::ins().playSfx(SfxCue::EXP_GAIN);
    }
}

uint32_t AmoledApp::battleExperienceForRender(uint32_t nowMs) const {
    if (!battleExperienceVisible ||
        battleExperienceAnimationTo <= battleExperienceAnimationFrom) {
        return battleExperienceAnimationFrom;
    }
    if (!battleExperienceAnimationActive) {
        return battleExperienceAnimationTo;
    }
    if (nowMs <= battleExperienceAnimationStartedMs) {
        return battleExperienceAnimationFrom;
    }
    const uint32_t elapsed = std::min<uint32_t>(
        BATTLE_EXP_ANIMATION_MS,
        nowMs - battleExperienceAnimationStartedMs);
    const uint64_t distance = static_cast<uint64_t>(
        battleExperienceAnimationTo - battleExperienceAnimationFrom) *
        elapsed;
    return battleExperienceAnimationFrom +
        static_cast<uint32_t>(distance / BATTLE_EXP_ANIMATION_MS);
}

void AmoledApp::performBattleBag(uint32_t nowMs) {
    if (Game::ItemInventory::homeBagItemCount(gameState) == 0) {
        setToast(Ui::Amoled::NOTHING, nowMs);
        return;
    }
    battlePhase = BattleViewModel::Phase::ACTION;
    battlePressedItem = 0xFF;
    openItemScene(AppSceneFlow::Scene::BAG);
    battleBagMode = true;
}

void AmoledApp::performBattleBagItem(Game::ItemId item, uint32_t nowMs) {
    if (!battleBagMode || sceneFlow.current() != AppSceneFlow::Scene::BAG) {
        return;
    }
    const int8_t foodIndex = Game::foodIndexForItemId(item);
    if (foodIndex < 0 && !Game::ItemInventory::usableInBattle(item)) {
        setToast(Ui::Amoled::CANNOT_USE, nowMs);
        return;
    }
    if (foodIndex >= 0) {
        const Species* wildSpecies = findSpecies(battleWild.speciesId);
        if (!wildSpecies) return;
        if (!Game::ItemInventory::remove(gameState, item)) {
            setToast(Ui::Room::FOOD_NO_STOCK, nowMs);
            return;
        }
        saveState();
        itemVelocity = 0.0f;
        itemConfirmOpen = false;
        pendingItem = Game::ItemId::COUNT;
        pendingItemAction = PendingItemAction::NONE;
        pressedItemRow = -1;
        battleBagMode = false;
        sceneFlow.closeSubScene();
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::FOOD_THROW_FMT, wildSpecies->name,
                      Ui::Room::FOOD_NAMES[foodIndex]);
        pushBattleLog(nowMs);
        const FoodTuning::ThrowClass throwClass =
            FriendshipSystem::classifyFoodThrow(
                static_cast<uint8_t>(foodIndex), battleWild.nature);
        const bool accepted = FriendshipSystem::acceptsFoodThrow(
            battleIsBoss, throwClass,
            static_cast<uint8_t>(GameRandom::range(0, 100)));
        std::snprintf(battleMessage, sizeof(battleMessage),
                      accepted
                          ? (throwClass == FoodTuning::ThrowClass::LIKED
                                 ? Ui::Explore::FOOD_ACCEPTED_LIKED_FMT
                                 : Ui::Explore::FOOD_ACCEPTED_FMT)
                          : (throwClass == FoodTuning::ThrowClass::DISLIKED
                                 ? Ui::Explore::FOOD_REFUSED_DISLIKED_FMT
                                 : Ui::Explore::FOOD_REFUSED_FMT),
                      wildSpecies->name);
        pushBattleLog(nowMs);
        if (accepted) {
            battleFoodBond = FriendshipSystem::addFoodBond(
                battleFoodBond, FriendshipSystem::throwBondGain(throwClass));
        }
        battleContinuation = accepted ? BattleContinuation::ADVANCE_TURN
                                      : BattleContinuation::WILD_TURN;
        AudioManager::ins().playSfx(SfxCue::THROW);
        requestFullRender();
        return;
    }
    if (battlePlayerSlot >= gameState.teamCount ||
        battlePlayerSlot >= Game::TEAM_CAP) return;
    Game::MonsterRuntime& player = gameState.team[battlePlayerSlot];
    const uint16_t hpBefore = player.hpCur;
    Game::ItemInventory::UseResult result = Game::ItemInventory::useOnTeam(
        gameState, item, battlePlayerSlot);
    if (result != Game::ItemInventory::UseResult::USED) {
        setToast(Ui::Amoled::CANNOT_USE, nowMs);
        return;
    }
    saveState();
    const uint16_t hpAfter = player.hpCur;
    itemVelocity = 0.0f;
    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    pressedItemRow = -1;
    battleBagMode = false;
    sceneFlow.closeSubScene();
    const uint32_t animationNowMs = Platform::clock().millis();
    std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                  Ui::Amoled::ITEM_USED);
    pushBattleLog(animationNowMs);
    battlePhase = BattleViewModel::Phase::ACTION;
    if (hpAfter != hpBefore) {
        startBattleHpAnimation(false, hpBefore, hpAfter, player.hpMax,
                               animationNowMs);
    }
    battleContinuation = BattleContinuation::WILD_TURN;
    requestFullRender();
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
    battleContinuation = BattleContinuation::WILD_TURN;
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
    const Species* wildSpecies = findSpecies(battleWild.speciesId);
    if (!species || !wildSpecies) {
        closeBattle(nowMs);
        return;
    }
    uint8_t reserveSlot = 0xFF;
    for (uint8_t slot = 0; slot < gameState.teamCount &&
         slot < Game::TEAM_CAP; ++slot) {
        if (slot == battlePlayerSlot) continue;
        const Game::MonsterRuntime& reserve = gameState.team[slot];
        if (!reserve.fainted && reserve.hpCur > 0) {
            reserveSlot = slot;
            break;
        }
    }
    const BattleSystem::ExperienceAwards awards =
        BattleSystem::experienceAwards(
            battleRewardExp, reserveSlot != 0xFF);

    Game::EffortService::grant(player, *wildSpecies, *species);
    const Game::ExperienceService::Result playerExperience =
        Game::ExperienceService::add(player, *species, awards.active);
    queueBattleProgression(battlePlayerSlot, playerExperience.oldLevel,
                           playerExperience.leveledUp);
    if (playerExperience.leveledUp) {
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::LEVEL_UP_LOG_FMT, species->name,
                      playerExperience.newLevel);
        pushBattleLog(nowMs);
    }

    if (reserveSlot != 0xFF && awards.reserve > 0) {
        Game::MonsterRuntime& reserve = gameState.team[reserveSlot];
        const Species* reserveSpecies = findSpecies(reserve.speciesId);
        if (reserveSpecies) {
            Game::EffortService::grant(
                reserve, *wildSpecies, *reserveSpecies);
            const Game::ExperienceService::Result reserveExperience =
                Game::ExperienceService::add(
                    reserve, *reserveSpecies, awards.reserve);
            if (reserveExperience.awarded > 0) {
                std::snprintf(battleMessage, sizeof(battleMessage),
                              Ui::Explore::SHARED_EXP_GAIN_FMT,
                              reserveSpecies->name,
                              static_cast<unsigned>(
                                  reserveExperience.awarded));
                pushBattleLog(nowMs);
            }
            queueBattleProgression(
                reserveSlot, reserveExperience.oldLevel,
                reserveExperience.leveledUp);
            if (reserveExperience.leveledUp) {
                std::snprintf(battleMessage, sizeof(battleMessage),
                              Ui::Explore::LEVEL_UP_LOG_FMT,
                              reserveSpecies->name,
                              reserveExperience.newLevel);
                pushBattleLog(nowMs);
            }
        }
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
    Game::FriendshipService::OfferResult offer =
        Game::FriendshipService::evaluateOffer(
            gameState, *wildSpecies, battleWild,
            battleIsBoss, allowsFriendship, battleFoodBond);
    if (offer.offered) {
        battleFriendshipPrompt = BattleViewModel::FriendshipPrompt::OFFER;
        battleFriendshipContactSlot = 0xFF;
        battlePhase = BattleViewModel::Phase::FRIENDSHIP;
        battlePressedItem = 0xFF;
        std::snprintf(battleMessage, sizeof(battleMessage),
                      Ui::Explore::FRIEND_RECOGNIZES_FMT,
                      wildSpecies->name);
        pushBattleLog(nowMs);
        std::snprintf(battleMessage, sizeof(battleMessage), "%s",
                      Ui::Explore::FRIEND_CONTACT_QUESTION);
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
    if (battleLogPlaybackBusy()) {
        battleVictoryFinalizePending = true;
        requestRenderRows(352, 448);
        return;
    }
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
        requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
                requestRenderRows(MENU_HEADER_HEIGHT, 448);
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
    if (battleProgressionCount > battleProgressionIndex) {
        autonomousExpedition = false;
        battleProgressionSequenceActive = true;
        if (startNextBattleProgression(nowMs)) return;
    }
    closeBattle(nowMs);
    if (autonomousExpedition) exploreRouteAutoWalk = true;
}

void AmoledApp::resetBattleProgressionQueue() {
    for (uint8_t slot = 0; slot < Game::TEAM_CAP; ++slot) {
        battleProgressionSlots[slot] = 0;
        battleProgressionOldLevels[slot] = 0;
    }
    battleProgressionCount = 0;
    battleProgressionIndex = 0;
    battleProgressionSequenceActive = false;
    battleVictoryFinalizePending = false;
}

void AmoledApp::queueBattleProgression(uint8_t teamSlot, uint8_t oldLevel,
                                       bool leveledUp) {
    if (!leveledUp || teamSlot >= gameState.teamCount ||
        teamSlot >= Game::TEAM_CAP ||
        battleProgressionCount >= Game::TEAM_CAP) {
        return;
    }
    battleProgressionSlots[battleProgressionCount] = teamSlot;
    battleProgressionOldLevels[battleProgressionCount] = oldLevel;
    ++battleProgressionCount;
}

bool AmoledApp::startNextBattleProgression(uint32_t nowMs) {
    while (battleProgressionIndex < battleProgressionCount) {
        const uint8_t index = battleProgressionIndex++;
        const uint8_t teamSlot = battleProgressionSlots[index];
        const uint8_t oldLevel = battleProgressionOldLevels[index];
        if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
            gameState.team[teamSlot].level <= oldLevel) {
            continue;
        }
        openProgressionScene(AppSceneFlow::Scene::EXPLORE_ROUTE,
                             teamSlot, oldLevel, nowMs);
        return true;
    }
    battleProgressionSequenceActive = false;
    return false;
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
    exploreRecoveryPendingSave = false;
    exploreSessionActive = false;
    autonomousExpedition = false;
    PokemonSprites::setDynamicSceneSpecies(nullptr, 0);
    // Defeat returns directly from battle to the room instead of using the
    // normal expedition return transition. Clear departure-only visibility
    // state so fainted team members can be rendered resting at home.
    expeditionMainHidden = false;
    expeditionCompanionHidden = false;
    expeditionCompanionDeparting = false;
    placeFaintedHomeActors(nowMs);
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
    setMusicContext(MusicContext::HOME);
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
    resetBattleProgressionQueue();
    battleAnimationActive = false;
    battleSwitchStage = BattleSwitchStage::NONE;
    pendingBattleSwitchSlot = 0xFF;
    battleSwitchConsumesTurn = false;
    battleAnimationAttackerWild = false;
    battleAnimationHit = false;
    battleAnimationDamage = 0;
    battleAnimationFrame = 0;
    battleAttackLogHeld = false;
    battleImpactAudioPlayed = false;
    battleImpactSfx = 0xFF;
    battleHpAnimationActive = false;
    battleExperienceVisible = false;
    battleExperienceAnimationActive = false;
    battleExperienceAnimationFrom = 0;
    battleExperienceAnimationTo = 0;
    battleBagMode = false;
    battleFoodBond = 0;
    battleAudioPending = false;
    battleAudioReady = false;
    battlePendingSfx = 0xFF;
    battlePendingCrySpecies = 0;
    battleTurnPlan = BattleTurnController::TurnPlan{};
    battleTurnActionIndex = 0;
    battleTurnDamaged[0] = false;
    battleTurnDamaged[1] = false;
    battleContinuation = BattleContinuation::NONE;
    clearBattleLog();
    battlePhase = BattleViewModel::Phase::ACTION;
    exploreRouteAutoWalk = false;
    exploreRoutePaused = false;
    sceneFlow.enter(returnToDebug ? AppSceneFlow::Scene::DEBUG
                                  : AppSceneFlow::Scene::EXPLORE_ROUTE);
    setMusicContext(returnToDebug ? MusicContext::HOME
                                 : MusicContext::EXPLORE);
    requestFullRender();
}

void AmoledApp::updateExploreRouteCamera() {
    int maximumX = std::max(0, EXPLORE_ROUTE_WORLD_WIDTH - AmoledUi::WIDTH / AmoledUi::RESOURCE_SCALE);
    int maximumY = std::max(0,
        EXPLORE_ROUTE_WORLD_HEIGHT - EXPLORE_ROUTE_VIEW_HEIGHT / AmoledUi::RESOURCE_SCALE);
    int cameraX = static_cast<int>(std::lround(exploreRouteWorldX)) - 92;
    int cameraY = static_cast<int>(std::lround(exploreRouteWorldY)) -
                  EXPLORE_ROUTE_VIEW_HEIGHT / (2 * AmoledUi::RESOURCE_SCALE);
    const int16_t nextCameraX = static_cast<int16_t>(
        std::clamp(cameraX, 0, maximumX));
    const int16_t nextCameraY = static_cast<int16_t>(
        std::clamp(cameraY, 0, maximumY));
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (nextCameraX != exploreRouteCameraX || nextCameraY != exploreRouteCameraY) {
        Platform::logf(
            "[AmoledExploreDiag] camera world=(%.2f,%.2f) old=(%d,%d) "
            "next=(%d,%d) max=(%d,%d)\n",
            static_cast<double>(exploreRouteWorldX),
            static_cast<double>(exploreRouteWorldY),
            static_cast<int>(exploreRouteCameraX),
            static_cast<int>(exploreRouteCameraY),
            static_cast<int>(nextCameraX), static_cast<int>(nextCameraY),
            maximumX, maximumY);
    }
#endif
    exploreRouteCameraX = nextCameraX;
    exploreRouteCameraY = nextCameraY;
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
    bool stateChanged = exploreRecoveryPendingSave;
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
    if (stateChanged && saveState()) exploreRecoveryPendingSave = false;
}

void AmoledApp::leaveExploreRoute() {
    if (expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU ||
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        beginExploreReturn(Platform::clock().millis());
    }
}

void AmoledApp::completeExploreReturn() {
    autonomousExpedition = false;
    exploreRoutePool = ExplorePool::Pool{};
    PokemonSprites::setPinnedDynamicSpecies(nullptr, 0);
    settleExploreReturn();
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugContactActive && debugContactKind == 3) {
        Platform::logf("[FriendDiag] home-return kind=3 team=%u phase=%u\n",
                       static_cast<unsigned>(gameState.teamCount),
                       static_cast<unsigned>(expeditionDeparturePhase));
        completeDebugContact(Platform::clock().millis());
    }
#endif
    exploreRouteMoving = false;
    exploreRouteAutoWalk = false;
    exploreRoutePlayerWalkActive = false;
    exploreRoutePaused = false;
    exploreRouteExitConfirm = false;
    sceneFlow.goHome();
    setMusicContext(MusicContext::HOME);
    requestFullRender();
}

void AmoledApp::placeFaintedHomeActors(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    if (gameState.teamCount > 0 &&
        (gameState.team[0].fainted || gameState.team[0].hpCur == 0)) {
        petX = petTargetX = room.available()
            ? static_cast<float>(room.bedX()) : 76.0f;
        petY = petTargetY = room.available()
            ? static_cast<float>(room.bedY()) : 99.0f;
        petResting = true;
        petScheduledSleeping = false;
        petMotion = PetMotion::IDLE;
        petDirection = PokemonSprites::WalkDirection::DOWN;
        petFrame = 0;
        nextPetFrameMs = nowMs + 700;
        homeMainActor.x = homeMainActor.targetX = petX;
        homeMainActor.y = homeMainActor.targetY = petY;
        homeMainActor.route.clear();
        homeMainActor.velocityX = homeMainActor.velocityY = 0.0f;
        homeRuntime.transition(0, Home::Task::FAINTED, nowMs, 0, true);
    }
    if (gameState.teamCount > 1 && homeCompanionActor.active &&
        (gameState.team[1].fainted || gameState.team[1].hpCur == 0)) {
        float sleepX = homeCompanionActor.x;
        float sleepY = homeCompanionActor.y;
        if (chooseCompanionSleepSpot(sleepX, sleepY)) {
            homeCompanionActor.x = sleepX;
            homeCompanionActor.y = sleepY;
        }
        homeCompanionActor.targetX = homeCompanionActor.x;
        homeCompanionActor.targetY = homeCompanionActor.y;
        homeCompanionActor.route.clear();
        homeCompanionActor.velocityX = homeCompanionActor.velocityY = 0.0f;
        homeCompanionActor.faintRestActive = true;
        homeRuntime.transition(1, Home::Task::FAINTED, nowMs, 0, true);
        companionFrame = 0;
        nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
    }
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
        const bool homeRecoveryActive =
            !exploreSessionActive &&
            sceneFlow.current() != AppSceneFlow::Scene::BATTLE;
        Game::applyCareMinutes(gameState, careAcc, elapsedMinutes,
                               gameSpeed(), homeRecoveryActive);
        requestRenderRows(HOME_STATUS_TOP, 448);
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
    syncHomeActors(nowMs);
    // Actor presentation state and the shared coordinator can be touched by
    // scene transitions, save restore, and visitor teardown. Repair stale
    // leases/routes before the next decision is allowed to claim a resource.
    homeRuntime.beginTick(nowMs);
    if (visitDeparture != VisitDeparture::NONE &&
        updateVisitDeparture(nowMs)) {
        lastPetUpdateMs = nowMs;
        return;
    }
    if (visitSession.visitorArrivalReady() &&
        homeCompanionActor.active && homeCompanionActor.hidden &&
        visitorMotion == VisitorMotion::NONE &&
        gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR) {
        Platform::logf("[VisitArrival] departed species=%u\n",
                       static_cast<unsigned>(homeCompanionActor.speciesId));
        beginVisitorEntry(nowMs);
    }
#if STICKMON_HAS_CLAW
    // External ownership must be visible to the shared policy before food
    // arbitration. An already committed feeding transaction may still finish.
    homeRuntime.setControlOwner(
        0, Stickmon::ClawRuntime::instance().autonomyActive()
               ? Home::ControlOwner::CLAW
               : Home::ControlOwner::AUTONOMOUS);
#endif
    // Keep an unowned serving actionable. A route failure releases the bowl,
    // and food restored from a save has no lease; either case must return to
    // arbitration so an actor blocking the approach can be asked to yield.
    if (!homeFoodArbitrationPending &&
        gameState.room.bowlCount > 0 &&
        homeRuntime.owner(Home::Resource::BOWL) < 0 &&
        preferredBowlEater(nowMs) >= 0) {
        homeFoodArbitrationPending = true;
        nextHomeFoodArbitrationMs = nowMs;
    }
    serviceHomeFoodArbitration(nowMs);
    updateCompanion(nowMs);
    if (visitorMotion == VisitorMotion::HOST_APPROACH ||
        visitorMotion == VisitorMotion::ENTERING ||
        visitorMotion == VisitorMotion::EXITING) {
        lastPetUpdateMs = nowMs;
        return;
    }
    float elapsedSeconds = static_cast<float>(nowMs - lastPetUpdateMs) / 1000.0f;
    lastPetUpdateMs = nowMs;
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);
    if (updatePairInteraction(nowMs, elapsedSeconds)) return;
    // Clearing a shared resource is part of the committed food transaction,
    // not an autonomous choice. It must finish even if Claw takes control.
    if (roomAction == RoomAction::FEED_FINISH &&
        updateRoomAction(nowMs, elapsedSeconds)) {
        return;
    }
#if STICKMON_HAS_CLAW
    if (!homeRuntime.autonomousAllowed(0)) {
        // Keep care/clock updates running, but do not let MonsterMind start a
        // competing walk or feed while the Agent owns the idle turn.
        cancelRoomAction(nowMs);
        lastPetUpdateMs = nowMs;
        return;
    }
#endif
    Game::MonsterRuntime& monster = gameState.team[0];
    const Home::ActorIntent initialMainIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(0, nowMs),
            gameState.room.bowlCount > 0);
    if (initialMainIntent == Home::ActorIntent::FAINT_REST) {
        cancelRoomAction(nowMs);
        if (homeRuntime.pairActive()) cancelPairInteraction(nowMs);
        homeRuntime.transition(0, Home::Task::FAINTED, nowMs, 0, true);
        petScheduledSleeping = false;
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

    const Game::SpeciesCareProfile mainCare =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool mainSleepTime = mainCare.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
    if (petScheduledSleeping) {
        const Home::ActorIntent sleepIntent =
            Home::ActorController::survivalIntent(
                homeActorObservation(0, nowMs),
                gameState.room.bowlCount > 0);
        const bool wakeForFood =
            sleepIntent == Home::ActorIntent::WAKE_FOR_FOOD;
        if (sleepIntent == Home::ActorIntent::WAKE || wakeForFood) {
            petScheduledSleeping = false;
            petResting = false;
            petMotion = PetMotion::IDLE;
            petFrame = 0;
            homeRuntime.release(Home::Resource::BED, 0);
            homeRuntime.stop(0, nowMs, wakeForFood ? 0 : 700);
            monsterMind.onRested(nowMs);
            nextPetDecisionMs = wakeForFood ? nowMs : nowMs + 700;
            nextMindUpdateMs = nowMs;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        } else {
            if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                ++petFrame;
                nextPetFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            }
            return;
        }
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
        monsterMind.update(monster, mainSleepTime,
                           mainCare.needsFood && gameState.room.bowlCount > 0,
                           nowMs);
        nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
    }

    // A sleep pose is persisted at the bed center. When the sleep window has
    // ended, the actor must leave that pose before normal idle decisions can
    // run; otherwise STARE/turning can keep it visually stranded on the bed.
    const bool atBedPose = std::fabs(petX -
        (RoomResource::ins().available()
             ? static_cast<float>(RoomResource::ins().bedX()) : 76.0f)) <= 2.5f &&
        std::fabs(petY -
        (RoomResource::ins().available()
             ? static_cast<float>(RoomResource::ins().bedY()) : 99.0f)) <= 6.0f;
    if (!mainSleepTime && petMotion == PetMotion::IDLE && atBedPose &&
        homeMainActor.task == Home::Task::IDLE) {
        float wakeX = petX;
        float wakeY = petY;
        if (chooseWakeTarget(wakeX, wakeY) &&
            beginPetMove(PetMotion::WANDERING, wakeX, wakeY, nowMs)) {
            Platform::logf(
                "[HomeSleep] event=leave_bed from=%.1f,%.1f target=%.1f,%.1f\n",
                petX, petY, wakeX, wakeY);
            return;
        }
        nextPetDecisionMs = nowMs + 1000;
        if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
            Platform::logf(
                "[HomeSleep] event=leave_bed_failed pos=%.1f,%.1f\n",
                petX, petY);
            nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
        }
        return;
    }

    const bool urgentNeed = monster.hpMax == 0 ||
        static_cast<uint32_t>(monster.hpCur) * 100UL <=
            static_cast<uint32_t>(monster.hpMax) * 25UL ||
        monster.satiety <= 25 || mainSleepTime ||
        (gameState.room.bowlCount > 0 &&
         monster.satiety < MONSTER_FEED_TARGET_SATIETY &&
         monsterMind.topDesire() == MonsterDesire::EAT);
    if (roomAction != RoomAction::NONE && urgentNeed) {
        cancelRoomAction(nowMs);
        nextPetDecisionMs = nowMs;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    updateDebugPairChase(nowMs, elapsedSeconds);
    if (debugTiltControl) {
        cancelRoomAction(nowMs);
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

    if (roomAction != RoomAction::NONE &&
        updateRoomAction(nowMs, elapsedSeconds)) {
        return;
    }
    if (ambientActionAllowed()) {
        if (startWindowGaze(nowMs)) return;
        if (static_cast<int32_t>(nowMs - nextAttentionMs) >= 0 &&
            startAttention(nowMs)) {
            return;
        }
        if (static_cast<int32_t>(nowMs - nextSpecialActionMs) >= 0 &&
            startAutonomousAction(nowMs)) {
            return;
        }
    }

    if (petMotion == PetMotion::TURNING) {
        if (static_cast<int32_t>(nowMs - petTurnUntilMs) < 0) return;
        petMotion = petStopMotion;
        petFrame = 0;
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

    if (petMotion == PetMotion::EATING) {
        bool canContinue = homeRuntime.owner(Home::Resource::BOWL) == 0 &&
                           gameState.room.bowlCount > 0 &&
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
            homeRuntime.beginBowlClearance(0, nowMs);
            // The visual state can already be IDLE when the serving ends,
            // but the shared actor must leave FEEDING as well. Otherwise it
            // remains a stale bowl obstacle and can strand the companion.
            homeRuntime.stop(0, nowMs);
            Platform::logf(
                "[HomeFood] event=release actor=0 stage=feed_complete "
                "satiety=%u bowl=%u companion=%u/%u\n",
                static_cast<unsigned>(monster.satiety),
                static_cast<unsigned>(gameState.room.bowlCount),
                gameState.teamCount > 1
                    ? static_cast<unsigned>(gameState.team[1].satiety) : 0U,
                static_cast<unsigned>(homeCompanionActor.task));
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
            startFeedFinish(nowMs);
            if (gameState.room.bowlCount > 0 &&
                preferredBowlEater(nowMs) >= 0) {
                homeFoodArbitrationPending = true;
                nextHomeFoodArbitrationMs = nowMs;
                homeCompanionActor.nextMindUpdateMs = nowMs;
                homeCompanionActor.nextDecisionMs = nowMs;
            }
            requestRenderRows(HOME_ROOM_TOP, 448);
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
            homeRuntime.transition(
                0, Home::Task::FEEDING, nowMs, 0, true);
            nextFeedBiteMs = nowMs + GameRandom::range(700, 1301);
            feedingUntilMs = nowMs + GameRandom::range(3200, 5601);
        } else {
            petMotion = PetMotion::IDLE;
            homeRuntime.stop(0, nowMs);
            monsterMind.onActivity(nowMs);
            schedulePetDecision(nowMs);
            nextPetFrameMs = nowMs + profile.idleFrameMs;
        }
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }

    if (petMotion == PetMotion::WANDERING ||
        petMotion == PetMotion::SEEKING_FOOD ||
        petMotion == PetMotion::SEEKING_SLEEP) {
        float waypointX = petTargetX;
        float waypointY = petTargetY;
        homeMainActor.route.current(waypointX, waypointY);
        const float dx = waypointX - petX;
        const float dy = waypointY - petY;
        petDirection = petDirectionForDelta(dx, dy);
        float speed = (petMotion == PetMotion::SEEKING_FOOD ? 19.0f :
                       petMotion == PetMotion::SEEKING_SLEEP ? 12.5f : 10.5f) *
                      behaviorProfile.moveSpeedScale;
        if (monster.mood < 40 || monster.satiety < 20) speed *= 0.72f;
        const float previousY = petY;
        const int previousCameraX = static_cast<int>(std::lround(cameraX));
        const int previousCameraY = static_cast<int>(std::lround(cameraY));
        Home::RouteStep routeStep = homeRuntime.advanceRoute(
            0, nowMs, speed, elapsedSeconds, 0.8f,
            homeCompanionActor.active);
        petX = homeMainActor.x;
        petY = homeMainActor.y;
        if (routeStep == Home::RouteStep::ARRIVED) {
            finishPetMove(nowMs);
        } else if (routeStep == Home::RouteStep::BLOCKED) {
            // A wander target can become invalid while the companion is also
            // moving. Drop it and let the next decision pick a fresh target;
            // replanning the same endpoint would recreate the encounter.
            if (petMotion == PetMotion::WANDERING) {
                petMotion = PetMotion::IDLE;
                petTargetX = petX;
                petTargetY = petY;
                petFrame = 0;
                nextPetFrameMs = nowMs + 520;
                homeRuntime.stop(0, nowMs, 700);
                schedulePetDecision(nowMs);
            } else if (!homeRuntime.planRoute(
                    0, petTargetX, petTargetY, false,
                    homeCompanionActor.active)) {
                if (petMotion == PetMotion::SEEKING_FOOD) {
                    Platform::logf(
                        "[HomeFood] event=blocked actor=0 stage=replan "
                        "owner=%d pos=%.1f,%.1f target=%.1f,%.1f "
                        "other=%.1f,%.1f\n",
                        static_cast<int>(homeRuntime.owner(
                            Home::Resource::BOWL)),
                        petX, petY, petTargetX, petTargetY,
                        homeCompanionActor.x, homeCompanionActor.y);
                }
                Platform::logf(
                    "[AmoledApp] movement replan failed pos=%.1f,%.1f target=%.1f,%.1f\n",
                    petX, petY, petTargetX, petTargetY);
                petMotion = PetMotion::IDLE;
                petTargetX = petX;
                petTargetY = petY;
                petFrame = 0;
                nextPetFrameMs = nowMs + 520;
                homeRuntime.stop(0, nowMs, 700);
                monsterMind.onActivity(nowMs);
                schedulePetDecision(nowMs);
            }
        } else if (routeStep == Home::RouteStep::NO_ROUTE) {
            if (petMotion == PetMotion::SEEKING_FOOD) {
                Platform::logf(
                    "[HomeFood] event=blocked actor=0 stage=no_route "
                    "owner=%d pos=%.1f,%.1f target=%.1f,%.1f\n",
                    static_cast<int>(homeRuntime.owner(
                        Home::Resource::BOWL)),
                    petX, petY, petTargetX, petTargetY);
            }
            petMotion = PetMotion::IDLE;
            petTargetX = petX;
            petTargetY = petY;
            petFrame = 0;
            homeRuntime.stop(0, nowMs, 700);
            schedulePetDecision(nowMs);
        }
        updateCamera();
        const bool cameraMoved =
            static_cast<int>(std::lround(cameraX)) != previousCameraX ||
            static_cast<int>(std::lround(cameraY)) != previousCameraY;
        if (cameraMoved) {
            // The room background is sampled in camera-space. Present it as
            // soon as the camera changes, even when the sprite animation
            // clock has not reached its next frame yet.
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        {
            static uint32_t s_petMoveSampleMs = 0;
            static float s_petSampleX = -1.0f;
            static float s_petSampleY = -1.0f;
            if (static_cast<int32_t>(nowMs - s_petMoveSampleMs) >= 0) {
                s_petMoveSampleMs = nowMs + 500;
                const bool moved = s_petSampleX >= 0.0f &&
                    (std::fabs(petX - s_petSampleX) > 0.5f ||
                     std::fabs(petY - s_petSampleY) > 0.5f);
                Platform::logf(
                    "[HomeDiag] pet-moving motion=%d pos=%.1f,%.1f frame=%u "
                    "frameIn=%ld moved=%d route=%d\n",
                    static_cast<int>(petMotion), petX, petY,
                    static_cast<unsigned>(petFrame),
                    static_cast<long>(static_cast<int32_t>(
                        nextPetFrameMs - nowMs)),
                    moved ? 1 : 0, static_cast<int>(routeStep));
                s_petSampleX = petX;
                s_petSampleY = petY;
            }
        }
#endif
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
            if (!cameraMoved) {
                requestHomeActorRows(previousY, petY);
            }
        }
        return;
    }

    if (petMotion == PetMotion::IDLE) {
        PokemonSprites::PetAnimationProfile profile{};
        if (PokemonSprites::petAnimationProfile(monster.speciesId, profile) &&
            static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            const uint8_t frameCount = std::max<uint8_t>(1, profile.idleFrames);
            do {
                petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
                nextPetFrameMs += profile.idleFrameMs;
            } while (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
    }

    if (homeFoodArbitrationPending &&
        preferredBowlEater(nowMs) == 0) return;
    if (static_cast<int32_t>(nowMs - nextPetDecisionMs) < 0) return;
    const Home::ActorObservation mainObservation =
        homeActorObservation(0, nowMs);
    const Home::ActorIntent mainIntent =
        Home::ActorController::survivalIntent(
            mainObservation, gameState.room.bowlCount > 0);
    if (mainObservation.sleepTime &&
        static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
        RoomResource& room = RoomResource::ins();
        const float bedX = room.available()
            ? static_cast<float>(room.bedX()) : 76.0f;
        const float bedY = room.available()
            ? static_cast<float>(room.bedY()) : 99.0f;
        Platform::logf(
            "[HomeSleep] event=decision intent=%u desire=%u "
            "sleepTime=%u usesBed=%u canMove=%u control=%u "
            "bedOwner=%d pos=%.1f,%.1f bed=%.1f,%.1f "
            "task=%u motion=%u\n",
            static_cast<unsigned>(mainIntent),
            static_cast<unsigned>(mainObservation.desire),
            mainObservation.sleepTime ? 1U : 0U,
            mainObservation.usesBed ? 1U : 0U,
            mainObservation.canMove ? 1U : 0U,
            mainObservation.controlAvailable ? 1U : 0U,
            static_cast<int>(homeRuntime.owner(Home::Resource::BED)),
            petX, petY, bedX, bedY,
            static_cast<unsigned>(homeMainActor.task),
            static_cast<unsigned>(petMotion));
        nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
    }
    if (mainIntent == Home::ActorIntent::SEEK_FOOD &&
        preferredBowlEater(nowMs) == 0) {
        if (startMainFoodSeek(nowMs)) {
            return;
        }
    }

    MonsterDesire desire = monsterMind.topDesire();
    if (mainIntent == Home::ActorIntent::SEEK_SLEEP) {
        const int8_t bedOwnerBefore =
            homeRuntime.owner(Home::Resource::BED);
        if (!homeRuntime.acquire(Home::Resource::BED, 0,
                                 Home::Task::SEEK_SLEEP, nowMs)) {
            nextPetDecisionMs = nowMs + 1000;
            if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
                Platform::logf(
                    "[AmoledPet] sleep blocked bedOwner=%d pos=%.1f,%.1f\n",
                    static_cast<int>(bedOwnerBefore), petX, petY);
                nextPetDebugLogMs = nowMs + PET_DEBUG_LOG_INTERVAL_MS;
            }
            return;
        }
        RoomResource& room = RoomResource::ins();
        const float sleepX = room.available()
            ? static_cast<float>(room.bedX()) : 76.0f;
        const float sleepY = room.available()
            ? static_cast<float>(room.bedY()) : 99.0f;
        const float companionDx = homeCompanionActor.x - sleepX;
        const float companionDy = homeCompanionActor.y - sleepY;
        if (homeCompanionActor.active &&
            companionDx * companionDx + companionDy * companionDy < 900.0f) {
            float yieldX = homeCompanionActor.x;
            float yieldY = homeCompanionActor.y;
            if (chooseCompanionSleepSpot(yieldX, yieldY, true)) {
                homeRuntime.transition(
                    1, Home::Task::YIELDING, nowMs, 0, true);
                beginCompanionMove(Home::Task::YIELDING, yieldX, yieldY,
                                   nowMs, false, false);
            }
        }
        // The bed pose is also used as the persisted resting position. When
        // a save restore or a previous interrupted transition already leaves
        // the actor there, asking the navigator to route to the same point
        // can produce an empty route and immediately cancel the sleep task.
        const bool atSleepPose =
            std::fabs(petX - sleepX) <= 2.5f &&
            std::fabs(petY - sleepY) <= 6.0f;
        if (atSleepPose) {
            homeMainActor.route.clear();
            homeMainActor.velocityX = 0.0f;
            homeMainActor.velocityY = 0.0f;
            petTargetX = petX;
            petTargetY = petY;
            petScheduledSleeping = true;
            petResting = true;
            petMotion = PetMotion::SLEEPING;
            petStopMotion = PetMotion::SLEEPING;
            petFrame = 0;
            homeRuntime.transition(
                0, Home::Task::SLEEPING, nowMs, 0, true);
            nextPetFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            Platform::logf(
                "[AmoledPet] sleep enter direct bed=%.1f,%.1f owner=%d\n",
                sleepX, sleepY,
                static_cast<int>(homeRuntime.owner(Home::Resource::BED)));
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            return;
        }
        if (beginPetMove(PetMotion::SEEKING_SLEEP,
                         sleepX, sleepY, nowMs)) {
            return;
        }
        homeRuntime.release(Home::Resource::BED, 0);
        Platform::logf(
            "[AmoledPet] sleep enter rejected pos=%.1f,%.1f target=%.1f,%.1f "
            "bedOwnerBefore=%d atSleepPose=%u\n",
            petX, petY, sleepX, sleepY,
            static_cast<int>(bedOwnerBefore),
            atSleepPose ? 1U : 0U);
    }
    if (static_cast<int32_t>(nowMs - nextPetDebugLogMs) >= 0) {
        int32_t decisionInMs = static_cast<int32_t>(
            nextPetDecisionMs - nowMs);
        const int8_t bowlOwner = homeRuntime.owner(Home::Resource::BOWL);
        const int8_t preferredEater = preferredBowlEater(nowMs);
        Platform::logf(
            "[AmoledPet] decision desire=%u motion=%u pos=%.1f,%.1f "
            "next=%ld walk=%u bowl=%u/%u owner=%d preferred=%d "
            "satiety=%u/%u canEat=%u/%u task=%u/%u desire2=%u "
            "near=%u/%u block=%u/%u pending=%u\n",
            static_cast<unsigned>(desire),
            static_cast<unsigned>(petMotion), petX, petY,
            static_cast<long>(decisionInMs),
            petFootprintInsideWalkArea(petX, petY) ? 1U : 0U,
            static_cast<unsigned>(gameState.room.bowlCount),
            static_cast<unsigned>(gameState.room.bowlBitesRemaining),
            static_cast<int>(bowlOwner), static_cast<int>(preferredEater),
            static_cast<unsigned>(monster.satiety),
            gameState.teamCount > 1
                ? static_cast<unsigned>(gameState.team[1].satiety) : 0U,
            homeActorCanEatFromBowl(0, nowMs) ? 1U : 0U,
            homeActorCanEatFromBowl(1, nowMs) ? 1U : 0U,
            static_cast<unsigned>(homeMainActor.task),
            static_cast<unsigned>(homeCompanionActor.task),
            static_cast<unsigned>(homeCompanionActor.mind.topDesire()),
            homeActorNearFood(0) ? 1U : 0U,
            homeActorNearFood(1) ? 1U : 0U,
            homeActorBlocksFoodApproach(0) ? 1U : 0U,
            homeActorBlocksFoodApproach(1) ? 1U : 0U,
            homeFoodArbitrationPending ? 1U : 0U);
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
        // Turning in place is still an idle decision. Do not reset the
        // inactivity timer, otherwise repeated STARE turns can starve WANDER.
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
    syncHomeActors(nowMs);
    Home::Task task = Home::Task::WANDER;
    if (motion == PetMotion::SEEKING_FOOD) task = Home::Task::SEEK_FOOD;
    else if (motion == PetMotion::SEEKING_SLEEP) task = Home::Task::SEEK_SLEEP;
    if (!homeRuntime.transition(0, task, nowMs)) return false;
    if (!petFootprintInsideWalkArea(x, y) ||
        !homeRuntime.planRoute(
            0, x, y, false, homeCompanionActor.active)) {
        homeRuntime.stop(0, nowMs, 700);
        return false;
    }
    petTargetX = x;
    petTargetY = y;
    float firstX = x;
    float firstY = y;
    homeMainActor.route.current(firstX, firstY);
    PokemonSprites::WalkDirection previousDirection = petDirection;
    petDirection = petDirectionForDelta(firstX - petX, firstY - petY);
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
    homeMainActor.route.clear();
    if (petMotion == PetMotion::SEEKING_SLEEP) {
        petScheduledSleeping = true;
        petResting = true;
        petMotion = PetMotion::SLEEPING;
        petStopMotion = PetMotion::SLEEPING;
        petFrame = 0;
        homeRuntime.transition(0, Home::Task::SLEEPING, nowMs, 0, true);
        nextPetFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }
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

void AmoledApp::scheduleAttention(uint32_t nowMs, bool initial) {
    uint32_t minimum = initial ? ATTENTION_INITIAL_MIN_MS : ATTENTION_MIN_MS;
    uint32_t maximum = initial ? ATTENTION_INITIAL_MAX_MS : ATTENTION_MAX_MS;
    uint32_t delayMs = static_cast<uint32_t>(
        GameRandom::range(minimum, maximum + 1));
    if (behaviorProfile.sociabilityBias > 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 - behaviorProfile.sociabilityBias * 14) / 100UL;
    } else if (behaviorProfile.sociabilityBias < 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 + (-behaviorProfile.sociabilityBias) * 18) / 100UL;
    }
    nextAttentionMs = nowMs + delayMs;
}

void AmoledApp::scheduleSpecialAction(uint32_t nowMs) {
    uint32_t delayMs = static_cast<uint32_t>(GameRandom::range(
        SPECIAL_ACTION_MIN_MS, SPECIAL_ACTION_MAX_MS + 1));
    if (behaviorProfile.activityBias > 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 - behaviorProfile.activityBias * 12) / 100UL;
    } else if (behaviorProfile.activityBias < 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 + (-behaviorProfile.activityBias) * 16) / 100UL;
    }
    nextSpecialActionMs = nowMs + delayMs;
}

bool AmoledApp::ambientActionAllowed() const {
    if (roomAction != RoomAction::NONE || petMotion != PetMotion::IDLE ||
        homeMainActor.task != Home::Task::IDLE || homeRuntime.pairActive() ||
        visitorMotion == VisitorMotion::HOST_APPROACH ||
        visitorMotion == VisitorMotion::ENTERING ||
        visitorMotion == VisitorMotion::EXITING || pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
        return false;
    }
    const Game::MonsterRuntime& monster = gameState.team[0];
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool sleepTime = care.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
    if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0 ||
        monster.majorStatus == Game::MajorStatus::SLEEP ||
        static_cast<uint32_t>(monster.hpCur) * 100UL <=
            static_cast<uint32_t>(monster.hpMax) * 25UL ||
        monster.satiety <= 25 || sleepTime) {
        return false;
    }
    if (gameState.room.bowlCount > 0 &&
        monster.satiety < MONSTER_FEED_TARGET_SATIETY) {
        return false;
    }
    return monsterMind.topDesire() != MonsterDesire::EAT &&
           monsterMind.topDesire() != MonsterDesire::REST;
}

bool AmoledApp::chooseNearbyPose(float originX, float originY,
                                 float minDistance, float maxDistance,
                                 float preferredAngle, float angleSpread,
                                 float& x, float& y) const {
    for (uint8_t attempt = 0; attempt < 18; ++attempt) {
        const float unit = static_cast<float>(
            GameRandom::random(-1000, 1001)) / 1000.0f;
        const float angle = preferredAngle + unit * angleSpread;
        const float distance = minDistance + (maxDistance - minDistance) *
            (static_cast<float>(GameRandom::random(0, 1001)) / 1000.0f);
        const float candidateX = originX + std::cos(angle) * distance;
        const float candidateY = originY + std::sin(angle) * distance;
        if (!petFootprintInsideWalkArea(candidateX, candidateY) ||
            !petPathInsideWalkArea(originX, originY,
                                   candidateX, candidateY)) {
            continue;
        }
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool AmoledApp::chooseAttentionPose(float& x, float& y) const {
    RoomResource& room = RoomResource::ins();
    const int minimumX = room.available() ? room.walkMinX()
                                          : FALLBACK_ROOM_MIN_X;
    const int maximumX = room.available() ? room.walkMaxX()
                                          : FALLBACK_ROOM_MAX_X;
    const int minimumY = room.available() ? room.walkMinY()
                                          : FALLBACK_ROOM_MIN_Y;
    const int maximumY = room.available() ? room.walkMaxY()
                                          : FALLBACK_ROOM_MAX_Y;
    const float desiredX = (minimumX + maximumX) * 0.5f;
    float bestScore = 1000000.0f;
    bool found = false;
    for (int candidateY = maximumY; candidateY >= minimumY;
         candidateY -= 3) {
        for (int candidateX = minimumX; candidateX <= maximumX;
             candidateX += 3) {
            if (!petFootprintInsideWalkArea(candidateX, candidateY)) continue;
            const float score = (maximumY - candidateY) * 5.0f +
                                std::fabs(candidateX - desiredX);
            if (score >= bestScore) continue;
            x = static_cast<float>(candidateX);
            y = static_cast<float>(candidateY);
            bestScore = score;
            found = true;
        }
        if (found && maximumY - candidateY >= 9) break;
    }
    return found;
}

bool AmoledApp::chooseCirclePose(uint8_t phase, float& x, float& y) const {
    static constexpr float X_SCALE[] = {1.0f, 0.0f, -1.0f, 0.0f, 0.0f};
    static constexpr float Y_SCALE[] = {0.0f, 0.62f, 0.0f, -0.62f, 0.0f};
    if (phase >= 5) return false;
    x = roomActionOriginX + X_SCALE[phase] * roomActionRadius;
    y = roomActionOriginY + Y_SCALE[phase] * roomActionRadius;
    return petFootprintInsideWalkArea(x, y);
}

bool AmoledApp::chooseWindowGazePose(
    const RoomResource::BehaviorAnchor& anchor,
    float& x, float& y) const {
    static constexpr int8_t OFFSETS[][2] = {
        {0, 0}, {-3, 0}, {3, 0}, {-6, 0}, {6, 0},
        {0, 3}, {-3, 3}, {3, 3}, {0, -3}, {-3, -3}, {3, -3},
        {-9, 0}, {9, 0}, {0, 6}, {0, -6},
    };
    for (const auto& offset : OFFSETS) {
        const float candidateX = anchor.footX + offset[0];
        const float candidateY = anchor.footY + offset[1];
        if (!petFootprintInsideWalkArea(candidateX, candidateY)) continue;
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool AmoledApp::beginRoomActionLeg(float x, float y, uint32_t nowMs) {
    if (!homeRuntime.transition(
            0, Home::Task::ROOM_ACTION, nowMs, 0, true) ||
        !homeRuntime.planRoute(
            0, x, y, false, homeCompanionActor.active)) {
        return false;
    }
    petTargetX = x;
    petTargetY = y;
    float waypointX = x;
    float waypointY = y;
    homeMainActor.route.current(waypointX, waypointY);
    petDirection = petDirectionForDelta(
        waypointX - petX, waypointY - petY);
    petMotion = PetMotion::WANDERING;
    petFrame = 0;
    nextPetFrameMs = nowMs;
    return true;
}

bool AmoledApp::beginRoomActionMove(RoomAction action, float x, float y,
                                    uint32_t nowMs) {
    roomAction = action;
    roomActionPhase = 0;
    roomActionStartedMs = nowMs;
    roomActionOriginX = petX;
    roomActionOriginY = petY;
    if (beginRoomActionLeg(x, y, nowMs)) return true;
    roomAction = RoomAction::NONE;
    homeRuntime.stop(0, nowMs, 700);
    petMotion = PetMotion::IDLE;
    petTargetX = petX;
    petTargetY = petY;
    return false;
}

void AmoledApp::startFeedFinish(uint32_t nowMs) {
    roomAction = RoomAction::FEED_FINISH;
    roomActionPhase = 0;
    roomActionStartedMs = nowMs;
    roomActionUntilMs = nowMs + FEED_FINISH_MS;
    petMotion = PetMotion::IDLE;
    petTargetX = petX;
    petTargetY = petY;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

bool AmoledApp::startAttention(uint32_t nowMs) {
    float x = 0.0f;
    float y = 0.0f;
    scheduleAttention(nowMs);
    if (!chooseAttentionPose(x, y)) return false;
    if (std::fabs(x - petX) < 3.0f && std::fabs(y - petY) < 3.0f) {
        roomAction = RoomAction::ATTENTION_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + ATTENTION_WAIT_MS;
        roomActionBaseDirection = petDirection;
        petDirection = PokemonSprites::WalkDirection::DOWN;
        petMotion = PetMotion::IDLE;
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        heartsUntil = roomActionUntilMs;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }
    return beginRoomActionMove(RoomAction::ATTENTION_APPROACH, x, y, nowMs);
}

bool AmoledApp::startWindowGaze(uint32_t nowMs) {
    const Game::MonsterRuntime& monster = gameState.team[0];
    const uint32_t gameSeconds =
        static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    if (!windowGazeDue(monster.lastExploredAt,
                       monster.lastWindowGazeAt, gameSeconds)) {
        return false;
    }
    RoomResource::BehaviorAnchor anchor{};
    RoomResource& room = RoomResource::ins();
    if (!room.available() || !room.findBehaviorAnchor(
            RoomResource::BehaviorAnchorType::WINDOW_GAZE, anchor)) {
        return false;
    }
    float x = 0.0f;
    float y = 0.0f;
    if (!chooseWindowGazePose(anchor, x, y)) return false;
    windowGazeDirection = mainDirectionFromView(anchor.facing);
    return beginRoomActionMove(RoomAction::WINDOW_APPROACH, x, y, nowMs);
}

bool AmoledApp::startAutonomousAction(uint32_t nowMs) {
    scheduleSpecialAction(nowMs);
    PokemonSprites::PetAnimationProfile animation{};
    const bool energeticAllowed =
        behaviorProfile.movementMode != MonsterMovementMode::STATIONARY &&
        PokemonSprites::petAnimationProfile(
            gameState.team[0].speciesId, animation) &&
        animation.walkingFrames >= 2;
    const int roll = static_cast<int>(GameRandom::random(100));
    RoomAction selected = RoomAction::LOOK_AROUND;
    if (behaviorProfile.activityBias >= 1) {
        if (energeticAllowed && roll < 26) selected = RoomAction::DASH;
        else if (energeticAllowed && roll < 50) selected = RoomAction::CIRCLE;
        else if (roll < 64) selected = RoomAction::STEP_BACK;
        else if (roll < 84) selected = RoomAction::LOOK_AROUND;
        else selected = RoomAction::QUIET_GAZE;
    } else if (behaviorProfile.activityBias <= -1) {
        if (roll < 38) selected = RoomAction::QUIET_GAZE;
        else if (roll < 72) selected = RoomAction::LOOK_AROUND;
        else if (roll < 92 || !energeticAllowed) selected = RoomAction::STEP_BACK;
        else if (roll < 98) selected = RoomAction::CIRCLE;
        else selected = RoomAction::DASH;
    } else {
        if (roll < 27) selected = RoomAction::LOOK_AROUND;
        else if (roll < 50) selected = RoomAction::QUIET_GAZE;
        else if (roll < 68 || !energeticAllowed) selected = RoomAction::STEP_BACK;
        else if (roll < 84) selected = RoomAction::CIRCLE;
        else selected = RoomAction::DASH;
    }

    roomActionStartedMs = nowMs;
    roomActionBaseDirection = petDirection;
    if (selected == RoomAction::LOOK_AROUND ||
        selected == RoomAction::QUIET_GAZE) {
        roomAction = selected;
        roomActionPhase = 0;
        roomActionUntilMs = nowMs + static_cast<uint32_t>(GameRandom::range(
            selected == RoomAction::LOOK_AROUND ? 1100 : 1400,
            selected == RoomAction::LOOK_AROUND ? 1801 : 2801));
        if (selected == RoomAction::QUIET_GAZE) {
            petDirection = PokemonSprites::WalkDirection::DOWN;
        }
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }

    if (selected == RoomAction::DASH || selected == RoomAction::STEP_BACK) {
        float x = 0.0f;
        float y = 0.0f;
        const bool dash = selected == RoomAction::DASH;
        const float preferred = dash
            ? static_cast<float>(GameRandom::random(0, 6284)) / 1000.0f
            : -1.57079633f;
        if (chooseNearbyPose(
                petX, petY, dash ? 20.0f : 7.0f,
                dash ? 32.0f : 12.0f, preferred,
                dash ? 2.35619449f : 0.55f, x, y) &&
            beginRoomActionMove(selected, x, y, nowMs)) {
            return true;
        }
    } else if (selected == RoomAction::CIRCLE) {
        roomAction = RoomAction::CIRCLE;
        roomActionPhase = 0;
        roomActionStartedMs = nowMs;
        roomActionOriginX = petX;
        roomActionOriginY = petY;
        for (int radius = 11; radius >= 6; --radius) {
            roomActionRadius = static_cast<float>(radius);
            bool valid = true;
            float previousX = roomActionOriginX;
            float previousY = roomActionOriginY;
            for (uint8_t phase = 0; phase < 5; ++phase) {
                float checkX = 0.0f;
                float checkY = 0.0f;
                if (!chooseCirclePose(phase, checkX, checkY) ||
                    !petPathInsideWalkArea(
                        previousX, previousY, checkX, checkY)) {
                    valid = false;
                    break;
                }
                previousX = checkX;
                previousY = checkY;
            }
            if (!valid) continue;
            float x = 0.0f;
            float y = 0.0f;
            chooseCirclePose(0, x, y);
            if (beginRoomActionLeg(x, y, nowMs)) return true;
            break;
        }
        roomAction = RoomAction::NONE;
    }

    roomAction = RoomAction::LOOK_AROUND;
    roomActionPhase = 0;
    roomActionStartedMs = nowMs;
    roomActionBaseDirection = petDirection;
    roomActionUntilMs = nowMs + 1200;
    homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    return true;
}

void AmoledApp::finishRoomActionMovement(uint32_t nowMs) {
    switch (roomAction) {
    case RoomAction::ATTENTION_APPROACH:
        roomAction = RoomAction::ATTENTION_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + ATTENTION_WAIT_MS;
        petMotion = PetMotion::IDLE;
        petDirection = PokemonSprites::WalkDirection::DOWN;
        heartsUntil = roomActionUntilMs;
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        return;
    case RoomAction::WINDOW_APPROACH:
        roomAction = RoomAction::WINDOW_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + static_cast<uint32_t>(GameRandom::range(
            WINDOW_GAZE_MIN_MS, WINDOW_GAZE_MAX_MS + 1));
        petMotion = PetMotion::IDLE;
        petDirection = windowGazeDirection;
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        gameState.team[0].lastWindowGazeAt =
            static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
        saveState();
        return;
    case RoomAction::CIRCLE:
        ++roomActionPhase;
        if (roomActionPhase < 5) {
            float x = 0.0f;
            float y = 0.0f;
            if (chooseCirclePose(roomActionPhase, x, y) &&
                beginRoomActionLeg(x, y, nowMs)) {
                return;
            }
        }
        break;
    case RoomAction::DASH:
    case RoomAction::STEP_BACK:
    default:
        break;
    }
    finishRoomAction(nowMs);
}

bool AmoledApp::updateRoomAction(uint32_t nowMs, float elapsedSeconds) {
    if (roomAction == RoomAction::NONE) return false;
    if (roomAction == RoomAction::FEED_FINISH && roomActionPhase == 0) {
        if (static_cast<int32_t>(nowMs - roomActionUntilMs) < 0) return true;
        roomActionPhase = 1;
        RoomResource& room = RoomResource::ins();
        const float foodX = room.available()
            ? static_cast<float>(room.foodX()) : FALLBACK_FOOD_APPROACH_X;
        const float foodY = room.available()
            ? static_cast<float>(room.foodY()) : FALLBACK_FOOD_APPROACH_Y;
        const float awayAngle = std::atan2(petY - foodY, petX - foodX);
        float x = 0.0f;
        float y = 0.0f;
        if (chooseNearbyPose(petX, petY, 32.0f, 48.0f, awayAngle,
                             3.14159265f * 0.42f, x, y) &&
            beginRoomActionLeg(x, y, nowMs)) {
            return true;
        }
        finishRoomAction(nowMs);
        return true;
    }
    if (roomAction == RoomAction::ATTENTION_WAIT ||
        roomAction == RoomAction::QUIET_GAZE ||
        roomAction == RoomAction::WINDOW_WAIT) {
        if (static_cast<int32_t>(nowMs - roomActionUntilMs) >= 0) {
            finishRoomAction(nowMs);
        }
        return true;
    }
    if (roomAction == RoomAction::LOOK_AROUND) {
        const uint8_t phase = static_cast<uint8_t>(std::min<uint32_t>(
            2, (nowMs - roomActionStartedMs) / 380UL));
        if (phase != roomActionPhase) {
            static constexpr int8_t STEPS[] = {-1, 1, 0};
            roomActionPhase = phase;
            petDirection = rotatePetDirection(
                roomActionBaseDirection, STEPS[phase]);
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
        if (static_cast<int32_t>(nowMs - roomActionUntilMs) >= 0) {
            petDirection = roomActionBaseDirection;
            finishRoomAction(nowMs);
        }
        return true;
    }

    float waypointX = petTargetX;
    float waypointY = petTargetY;
    homeMainActor.route.current(waypointX, waypointY);
    petDirection = petDirectionForDelta(
        waypointX - petX, waypointY - petY);
    const float previousY = petY;
    const int previousCameraX = static_cast<int>(std::lround(cameraX));
    const int previousCameraY = static_cast<int>(std::lround(cameraY));
    const float speed = (roomAction == RoomAction::DASH ? 16.3f : 10.5f) *
                        behaviorProfile.moveSpeedScale;
    const Home::RouteStep step = homeRuntime.advanceRoute(
        0, nowMs, speed, elapsedSeconds, 0.8f,
        homeCompanionActor.active);
    petX = homeMainActor.x;
    petY = homeMainActor.y;
    petTargetX = homeMainActor.targetX;
    petTargetY = homeMainActor.targetY;
    updateCamera();
    const bool cameraMoved =
        static_cast<int>(std::lround(cameraX)) != previousCameraX ||
        static_cast<int>(std::lround(cameraY)) != previousCameraY;
    if (cameraMoved) {
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
    }
    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        PokemonSprites::PetAnimationProfile animation{};
        const uint8_t frameCount =
            PokemonSprites::petAnimationProfile(
                gameState.team[0].speciesId, animation)
                ? std::max<uint8_t>(1, animation.walkingFrames)
                : 3;
        petFrame = static_cast<uint8_t>((petFrame + 1) % frameCount);
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        if (!cameraMoved) requestHomeActorRows(previousY, petY);
    }
    if (step == Home::RouteStep::ARRIVED) {
        finishRoomActionMovement(nowMs);
    } else if (step == Home::RouteStep::BLOCKED ||
               step == Home::RouteStep::NO_ROUTE) {
        finishRoomAction(nowMs);
    }
    return true;
}

void AmoledApp::finishRoomAction(uint32_t nowMs) {
    const RoomAction finishedAction = roomAction;
    if (finishedAction == RoomAction::FEED_FINISH &&
        homeActorBlocksFoodApproach(0)) {
        roomActionPhase = 0;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + 300;
        homeRuntime.stop(0, nowMs, 300);
        petMotion = PetMotion::IDLE;
        petTargetX = petX;
        petTargetY = petY;
        petFrame = 0;
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return;
    }
    roomAction = RoomAction::NONE;
    roomActionPhase = 0;
    roomActionStartedMs = 0;
    roomActionUntilMs = 0;
    homeRuntime.stop(0, nowMs, static_cast<uint32_t>(
        GameRandom::range(650, 1201)));
    if (finishedAction == RoomAction::FEED_FINISH) {
        homeRuntime.finishBowlClearance(0, nowMs);
    }
    petMotion = PetMotion::IDLE;
    petTargetX = petX;
    petTargetY = petY;
    petFrame = 0;
    monsterMind.onActivity(nowMs);
    schedulePetDecision(nowMs);
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::cancelRoomAction(uint32_t nowMs) {
    if (roomAction == RoomAction::NONE) return;
    const bool ownedHeart =
        roomAction == RoomAction::ATTENTION_APPROACH ||
        roomAction == RoomAction::ATTENTION_WAIT;
    finishRoomAction(nowMs);
    if (ownedHeart) heartsUntil = 0;
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

void AmoledApp::configureHomeRuntime() {
    RoomResource& room = RoomResource::ins();
    Home::NavigationWorld world;
    world.polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    world.polygonCount = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    world.footBounds = {
        room.available() ? static_cast<float>(room.walkMinX())
                         : FALLBACK_ROOM_MIN_X,
        room.available() ? static_cast<float>(room.walkMinY())
                         : FALLBACK_ROOM_MIN_Y,
        room.available() ? static_cast<float>(room.walkMaxX())
                         : FALLBACK_ROOM_MAX_X,
        room.available() ? static_cast<float>(room.walkMaxY())
                         : FALLBACK_ROOM_MAX_Y,
    };
    const float companionRadius = homeCompanionActor.active
        ? homeCompanionActor.geometry.footprint.radiusX : 0.0f;
    world.actorMinSeparation = std::max(
        20.0f, homeMainActor.geometry.footprint.radiusX +
                   companionRadius + 2.0f);
    // Room actors share the same walkable area, but one actor must not turn
    // the other into an A* wall. Soft steering keeps sprites readable while
    // allowing narrow-room routes and food/bed transitions to complete.
    world.actorCollisionMode = Home::ActorCollisionMode::SOFT;
    world.actorOverlapPadding = 1.0f;
    world.scratch = {
        gHomeNavParent, gHomeNavQueue, RoomNavigator::MAX_NODES};
    homeRuntime.setNavigation(world);
}

void AmoledApp::initializeCompanionActor(uint32_t nowMs) {
    const bool guest = gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR;
    homeCompanionActor.reset(
        guest ? Home::ActorRole::GUEST : Home::ActorRole::TEAMMATE,
        1, nowMs);
    homeCompanionActor.active = gameState.teamCount > 1 &&
        gameState.team[1].speciesId != 0;
    companionFrame = 0;
    companionDirection = PokemonSprites::WalkDirection::DOWN;
    companionLongMove = true;
    nextCompanionFrameMs = nowMs + 520;
    lastCompanionUpdateMs = nowMs;
    if (!homeCompanionActor.active) return;

    const Game::MonsterRuntime& monster = gameState.team[1];
    homeCompanionActor.speciesId = monster.speciesId;
    const Species* species = findSpecies(monster.speciesId);
    homeCompanionActor.behavior = behaviorProfileFor(
        species ? *species : starterSpecies(), monster);

    PokemonSprites::PetAnimationProfile animation{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    if (PokemonSprites::petAnimationProfile(monster.speciesId, animation)) {
        frame = PokemonSprites::findSpeciesSprite(
            monster.speciesId, animation.idleBase);
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            monster.speciesId, PokemonSprites::SpriteKind::FRONT);
    }
    const uint8_t width = frame ? FlashStorage::readByte(&frame->width) : 38;
    const uint8_t height = frame ? FlashStorage::readByte(&frame->height) : 42;
    const Home::GroundFootprintProfile footprint =
        Home::groundFootprintForSpecies(monster.speciesId, width, height);
    homeCompanionActor.geometry = {
        0.0f, {footprint.radiusX, footprint.radiusY}};

    homeCompanionActor.x = homeMainActor.x;
    homeCompanionActor.y = homeMainActor.y;
    homeCompanionActor.targetX = homeCompanionActor.x;
    homeCompanionActor.targetY = homeCompanionActor.y;
    homeRuntime.attach(homeMainActor, &homeCompanionActor);
    configureHomeRuntime();

    if (guest) {
        if (visitSession.active() && visitSession.viewModel().localIsHost &&
            !pendingExpedition &&
            expeditionDeparturePhase == ExpeditionDeparturePhase::NONE) {
            expeditionCompanionHidden = false;
        }
        if (visitSession.active() && visitSession.viewModel().localIsHost &&
            !visitSession.visitorArrivalReady()) {
            homeCompanionActor.hidden = true;
            homeCompanionActor.stop(nowMs, 0);
            Platform::logf("[VisitArrival] waiting species=%u\n",
                           static_cast<unsigned>(monster.speciesId));
            return;
        }
        if (pendingExpedition ||
            expeditionDeparturePhase != ExpeditionDeparturePhase::NONE) {
            // A departure is already queued: skip the door-entry animation
            // and spawn inside the room so the companion can join the
            // departure door walk.
            float spawnX = homeCompanionActor.x;
            float spawnY = homeCompanionActor.y;
            if (chooseCompanionTarget(spawnX, spawnY)) {
                homeCompanionActor.x = spawnX;
                homeCompanionActor.y = spawnY;
                homeCompanionActor.targetX = spawnX;
                homeCompanionActor.targetY = spawnY;
                homeCompanionActor.route.clear();
            }
            homeCompanionActor.stop(nowMs, 0);
            return;
        }
        beginVisitorEntry(nowMs);
        return;
    }

    const SecondarySceneViewState& saved = mainViewState.secondary;
    const bool sameMonster = saved.valid &&
        saved.speciesId == monster.speciesId &&
        saved.ivPacked == monster.ivPacked &&
        saved.metAt == monster.metAt &&
        saved.nature == monster.nature &&
        saved.metArea == monster.metArea &&
        saved.origin == static_cast<uint8_t>(monster.origin);
    if (sameMonster) {
        const float groundOffset = speciesGroundOffset(monster.speciesId);
        const float savedX = saved.x;
        const float savedY = saved.y + groundOffset;
        RoomResource& room = RoomResource::ins();
        const RoomResource::Point* polygon = room.available()
            ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
        const uint8_t polygonCount = room.available()
            ? room.walkPolygonCount()
            : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                                   sizeof(FALLBACK_WALK_POLYGON[0]));
        if (RoomMovementArea::containsFootprint(
                polygon, polygonCount, savedX, savedY,
                homeCompanionActor.geometry.footprint)) {
            homeCompanionActor.x = savedX;
            homeCompanionActor.y = savedY;
            homeCompanionActor.targetX = saved.targetX;
            homeCompanionActor.targetY = saved.targetY + groundOffset;
            companionDirection = saved.direction <= 3
                ? static_cast<PokemonSprites::WalkDirection>(saved.direction)
                : PokemonSprites::WalkDirection::DOWN;
            companionFrame = saved.frameIndex;
            homeCompanionActor.nextDecisionMs =
                nowMs + saved.stateRemainingMs;
            homeCompanionActor.foodWakeRetryAfterMs =
                saved.foodRetryRemainingMs == 0
                    ? 0 : nowMs + saved.foodRetryRemainingMs;
            if (saved.state == 1) {
                homeRuntime.transition(
                    1, Home::Task::WANDER, nowMs, 0, true);
                if (homeRuntime.planRoute(
                        1, homeCompanionActor.targetX,
                        homeCompanionActor.targetY)) {
                    return;
                }
                homeRuntime.stop(1, nowMs, 700);
            }
            const Game::SpeciesCareProfile care =
                Game::speciesCareProfileFor(monster.speciesId);
            const bool sleepTime = care.usesBed && Game::isSleepCareTime(
                gameState.gameMinutesTotal, monster.nature);
            if (saved.state == 6 && sleepTime) {
                homeRuntime.transition(
                    1, Home::Task::SLEEPING, nowMs, 0, true);
                homeCompanionActor.sleepX = saved.sleepX;
                homeCompanionActor.sleepY = saved.sleepY + groundOffset;
                homeCompanionActor.sleepSpotValid = saved.sleepSpotValid;
                return;
            }
            homeCompanionActor.stop(
                nowMs, saved.stateRemainingMs);
            return;
        }
    }

    float spawnX = homeCompanionActor.x;
    float spawnY = homeCompanionActor.y;
    if (chooseCompanionTarget(spawnX, spawnY)) {
        homeCompanionActor.x = spawnX;
        homeCompanionActor.y = spawnY;
        homeCompanionActor.targetX = spawnX;
        homeCompanionActor.targetY = spawnY;
        homeCompanionActor.route.clear();
    }
    homeCompanionActor.stop(
        nowMs, static_cast<uint32_t>(GameRandom::range(
            homeCompanionActor.behavior.idleMinMs,
            homeCompanionActor.behavior.idleMaxMs + 1)));
}

void AmoledApp::syncHomeActors(uint32_t nowMs) {
    const bool firstSetup = !homeRuntimeReady;
    const uint16_t nextMainSpecies = gameState.teamCount > 0
        ? gameState.team[0].speciesId : 0;
    const bool mainChanged = !firstSetup &&
        homeMainActor.speciesId != nextMainSpecies;
    if (firstSetup) {
        homeMainActor.reset(Home::ActorRole::LEADER, 0, nowMs);
        homeRuntimeReady = true;
    } else if (mainChanged) {
        homeMainActor.route.clear();
        homeMainActor.velocityX = 0.0f;
        homeMainActor.velocityY = 0.0f;
    }
    homeMainActor.active = gameState.teamCount > 0;
    homeMainActor.speciesId = nextMainSpecies;
    homeMainActor.x = petX;
    homeMainActor.y = petY;
    homeMainActor.geometry = {
        0.0f, {petFootprintRadiusX, petFootprintRadiusY}};
    homeMainActor.behavior = behaviorProfile;
    if (firstSetup || mainChanged) {
        homeMainActor.targetX = petTargetX;
        homeMainActor.targetY = petTargetY;
    }

    const bool wantsCompanion = gameState.teamCount > 1 &&
        gameState.team[1].speciesId != 0;
    const bool companionChanged = wantsCompanion &&
        (!homeCompanionActor.active ||
         homeCompanionActor.speciesId != gameState.team[1].speciesId);
    if (companionChanged) {
        initializeCompanionActor(nowMs);
    } else if (!wantsCompanion && homeCompanionActor.active) {
        cancelPairInteraction(nowMs);
        homeRuntime.releaseAll(1);
        homeCompanionActor = Home::Actor{};
        companionFrame = 0;
        visitorMotion = VisitorMotion::NONE;
        homeRuntime.attach(homeMainActor);
    } else {
        homeRuntime.attach(
            homeMainActor,
            homeCompanionActor.active ? &homeCompanionActor : nullptr);
    }
    configureHomeRuntime();
}

Home::ActorObservation AmoledApp::homeActorObservation(
    uint8_t teamSlot, uint32_t nowMs) const {
    Home::ActorObservation observation;
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) {
        return observation;
    }

    const Home::Actor* actor = homeRuntime.actor(teamSlot);
    const Game::MonsterRuntime& monster = gameState.team[teamSlot];
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    observation.active = actor && actor->active && !actor->hidden;
    observation.controlAvailable = homeRuntime.autonomousAllowed(teamSlot);
    observation.visitor = monster.origin == Game::Origin::VISITOR;
    observation.canMove = care.canMove;
    observation.needsFood = care.needsFood;
    observation.usesBed = care.usesBed;
    observation.fainted = monster.fainted || monster.hpCur == 0;
    observation.statusSleeping =
        monster.majorStatus == Game::MajorStatus::SLEEP;
    observation.sleepTime = observation.statusSleeping ||
        (care.usesBed && Game::isSleepCareTime(
             gameState.gameMinutesTotal, monster.nature));
    observation.sleeping = actor && actor->task == Home::Task::SLEEPING;
    if (teamSlot == 0) observation.sleeping = petScheduledSleeping;
    observation.wakeForFood = care.needsFood &&
        monsterShouldWakeForFood(monster.satiety);
    observation.foodRetryReady = teamSlot == 0 || !actor ||
        static_cast<int32_t>(nowMs - actor->foodWakeRetryAfterMs) >= 0;
    observation.satiety = monster.satiety;
    observation.feedTarget = MONSTER_FEED_TARGET_SATIETY;
    if (actor) {
        observation.task = actor->task;
        observation.resumeTask = actor->resumeTask;
        const bool foodCommitted =
            Home::ActorController::committedToFood(observation);
        observation.foodActionAvailable = foodCommitted ||
            actor->task == Home::Task::IDLE ||
            actor->task == Home::Task::SLEEPING ||
            actor->task == Home::Task::WAKING ||
            actor->task == Home::Task::LEAVING_SLEEP;
        observation.desire = teamSlot == 0
            ? monsterMind.topDesire() : actor->mind.topDesire();
    } else {
        observation.foodActionAvailable = false;
    }
    return observation;
}

Home::HouseholdObservation AmoledApp::homeHouseholdObservation(
    uint32_t nowMs) const {
    Home::HouseholdObservation observation;
    observation.actorCount = std::min<uint8_t>(
        gameState.teamCount, Home::ACTOR_CAP);
    for (uint8_t actorId = 0; actorId < observation.actorCount; ++actorId) {
        observation.actors[actorId] = homeActorObservation(actorId, nowMs);
    }
    observation.bowlHasFood = gameState.room.bowlCount > 0;
    observation.bowlClearing = homeRuntime.bowlSession().clearing();
    observation.bowlOwner = homeRuntime.owner(Home::Resource::BOWL);
    return observation;
}

bool AmoledApp::homeActorCanEatFromBowl(uint8_t teamSlot,
                                        uint32_t nowMs) const {
    return Home::ActorController::foodEligible(
        homeActorObservation(teamSlot, nowMs),
        gameState.room.bowlCount > 0);
}

int8_t AmoledApp::preferredBowlEater(uint32_t nowMs) const {
    return Home::ActorController::selectBowlActor(
        homeHouseholdObservation(nowMs)).actorId;
}

bool AmoledApp::claimBowl(uint8_t teamSlot, uint32_t nowMs) {
    if (!homeRuntime.bowlClaimAllowed(teamSlot)) return false;
    // Do not hand the bowl to the leader while the previous eater is still
    // occupying its approach. A released lease alone does not clear space.
    if (teamSlot == 0 && homeActorBlocksFoodApproach(1)) {
        return false;
    }
    int8_t owner = homeRuntime.owner(Home::Resource::BOWL);
    if (owner >= 0 &&
        !homeActorCanEatFromBowl(static_cast<uint8_t>(owner), nowMs)) {
        homeRuntime.release(Home::Resource::BOWL,
                            static_cast<uint8_t>(owner));
        owner = -1;
    }
    if (owner >= 0) return owner == static_cast<int8_t>(teamSlot);
    if (preferredBowlEater(nowMs) != static_cast<int8_t>(teamSlot)) {
        return false;
    }
    return homeRuntime.acquire(Home::Resource::BOWL, teamSlot,
                               Home::Task::SEEK_FOOD, nowMs);
}

bool AmoledApp::homeActorNearFood(uint8_t actorId) const {
    const Home::Actor* actor = homeRuntime.actor(actorId);
    if (!actor || !actor->active || actor->hidden) return false;
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const float toleranceX = std::max(
        9.0f, actor->geometry.footprint.radiusX + 8.0f);
    const float toleranceY = std::max(
        7.0f, actor->geometry.footprint.radiusY + 6.0f);
    return std::fabs(actor->x - foodX) <= toleranceX &&
           std::fabs(actor->y + actor->geometry.groundOffsetY - foodY) <=
               toleranceY;
}

bool AmoledApp::homeActorBlocksFoodApproach(uint8_t actorId) const {
    const Home::Actor* actor = homeRuntime.actor(actorId);
    if (!actor || !actor->active || actor->hidden) return false;
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const float otherRadius = actorId == 0
        ? homeCompanionActor.geometry.footprint.radiusX
        : homeMainActor.geometry.footprint.radiusX;
    const float clearance = std::max(
        26.0f, actor->geometry.footprint.radiusX + otherRadius + 6.0f);
    const float dx = actor->x - foodX;
    const float dy = actor->y - foodY;
    return dx * dx + dy * dy < clearance * clearance;
}

bool AmoledApp::companionFoodAvoidsMain() const {
    // During feeding, the leader is a real moving obstacle. Once feeding has
    // ended, however, it can remain visually parked beside the bowl for a
    // while. That idle pose must not make the companion's food route
    // impossible in the narrow room.
    const bool mainFeeding =
        petMotion == PetMotion::SEEKING_FOOD ||
        petMotion == PetMotion::EATING ||
        (petMotion == PetMotion::STOPPING && petStoppingToEat) ||
        homeMainActor.task == Home::Task::SEEK_FOOD ||
        homeMainActor.task == Home::Task::FEEDING;
    if (!mainFeeding && homeActorBlocksFoodApproach(0)) return false;
    if (homeMainActor.task == Home::Task::YIELDING &&
        !homeActorBlocksFoodApproach(0)) {
        return false;
    }
    return true;
}

bool AmoledApp::startMainFoodSeek(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    float approachX = foodX;
    float approachY = foodY;
    if (!claimBowl(0, nowMs)) {
        if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
            Platform::logf(
                "[HomeFood] event=reject actor=0 stage=claim owner=%d "
                "preferred=%d canEat=%u/%u task=%u/%u\n",
                static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
                static_cast<int>(preferredBowlEater(nowMs)),
                homeActorCanEatFromBowl(0, nowMs) ? 1U : 0U,
                homeActorCanEatFromBowl(1, nowMs) ? 1U : 0U,
                static_cast<unsigned>(homeMainActor.task),
                static_cast<unsigned>(homeCompanionActor.task));
            nextHomeFoodDebugLogMs = nowMs + 2000;
        }
        return false;
    }
    if (homeActorNearFood(0)) {
        homeRuntime.transition(0, Home::Task::FEEDING, nowMs, 0, true);
        petMotion = PetMotion::EATING;
        petStopMotion = PetMotion::IDLE;
        petStoppingToEat = false;
        petTargetX = petX;
        petTargetY = petY;
        petDirection = petDirectionForDelta(
            (room.available() ? room.foodX() : foodX) - petX,
            (room.available() ? room.foodY() : foodY) - petY);
        petFrame = 0;
        nextFeedBiteMs = nowMs + GameRandom::range(700, 1301);
        feedingUntilMs = nowMs + GameRandom::range(3200, 5601);
        Platform::logf(
            "[HomeFood] event=direct actor=0 pos=%.1f,%.1f target=%.1f,%.1f\n",
            petX, petY, foodX, foodY);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }
    if (!chooseFoodApproachTarget(foodX, foodY, approachX, approachY)) {
        Platform::logf(
            "[HomeFood] event=reject actor=0 stage=approach "
            "pos=%.1f,%.1f target=%.1f,%.1f\n",
            petX, petY, foodX, foodY);
        homeRuntime.release(Home::Resource::BOWL, 0);
        return false;
    }
    if (!beginPetMove(PetMotion::SEEKING_FOOD, approachX, approachY,
                      nowMs)) {
        Platform::logf(
            "[HomeFood] event=reject actor=0 stage=route "
            "pos=%.1f,%.1f target=%.1f,%.1f other=%.1f,%.1f\n",
            petX, petY, approachX, approachY,
            homeCompanionActor.x, homeCompanionActor.y);
        homeRuntime.release(Home::Resource::BOWL, 0);
        return false;
    }
    Platform::logf(
        "[HomeFood] event=seek actor=0 pos=%.1f,%.1f target=%.1f,%.1f\n",
        petX, petY, approachX, approachY);
    return true;
}

bool AmoledApp::startCompanionFoodSeek(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const bool avoidMain = companionFoodAvoidsMain();
    if (!claimBowl(1, nowMs)) {
        if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
            Platform::logf(
                "[HomeFood] event=reject actor=1 stage=claim owner=%d "
                "preferred=%d canEat=%u/%u task=%u/%u\n",
                static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
                static_cast<int>(preferredBowlEater(nowMs)),
                homeActorCanEatFromBowl(0, nowMs) ? 1U : 0U,
                homeActorCanEatFromBowl(1, nowMs) ? 1U : 0U,
                static_cast<unsigned>(homeMainActor.task),
                static_cast<unsigned>(homeCompanionActor.task));
            nextHomeFoodDebugLogMs = nowMs + 2000;
        }
        return false;
    }
    if (homeActorNearFood(1)) {
        homeRuntime.transition(1, Home::Task::FEEDING, nowMs,
                               static_cast<uint32_t>(GameRandom::range(
                                   3200, 5601)), true);
        homeCompanionActor.targetX = homeCompanionActor.x;
        homeCompanionActor.targetY = homeCompanionActor.y;
        companionDirection = petDirectionForDelta(
            (room.available() ? room.foodX() : foodX) -
                homeCompanionActor.x,
            (room.available() ? room.foodY() : foodY) -
                homeCompanionActor.y);
        companionFrame = 0;
        companionFoodBiteMs = nowMs + GameRandom::range(700, 1301);
        Platform::logf(
            "[HomeFood] event=direct actor=1 pos=%.1f,%.1f target=%.1f,%.1f\n",
            homeCompanionActor.x, homeCompanionActor.y, foodX, foodY);
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        return true;
    }
    float approachX = foodX;
    float approachY = foodY;
    bool routeFound = false;
    // The main actor already searches for a reachable point around the bowl.
    // Use the same idea for the companion, but test the real companion route
    // because its footprint can be wider than the leader's.
    for (int radius = 0; radius <= 36 && !routeFound; radius += 2) {
        const int yRadius = std::min(radius, 24);
        for (int dy = -yRadius; dy <= yRadius && !routeFound; dy += 2) {
            const float candidates[] = {
                foodX + static_cast<float>(radius),
                foodX - static_cast<float>(radius),
            };
            for (float candidateX : candidates) {
                const float candidateY = foodY + static_cast<float>(dy);
                if (homeRuntime.planRoute(1, candidateX, candidateY,
                                          false, avoidMain)) {
                    approachX = candidateX;
                    approachY = candidateY;
                    routeFound = true;
                    break;
                }
            }
        }
        for (int dx = -radius + 2;
             dx <= radius - 2 && !routeFound; dx += 2) {
            const float candidates[] = {
                foodY + static_cast<float>(yRadius),
                foodY - static_cast<float>(yRadius),
            };
            for (float candidateY : candidates) {
                const float candidateX = foodX + static_cast<float>(dx);
                if (homeRuntime.planRoute(1, candidateX, candidateY,
                                          false, avoidMain)) {
                    approachX = candidateX;
                    approachY = candidateY;
                    routeFound = true;
                    break;
                }
            }
        }
    }
    if (!routeFound || !beginCompanionMove(Home::Task::SEEK_FOOD,
                                           approachX, approachY, nowMs,
                                           false, avoidMain)) {
        Platform::logf(
            "[HomeFood] event=reject actor=1 stage=route "
            "pos=%.1f,%.1f target=%.1f,%.1f other=%.1f,%.1f\n",
            homeCompanionActor.x, homeCompanionActor.y, foodX, foodY,
            petX, petY);
        homeRuntime.release(Home::Resource::BOWL, 1);
        return false;
    }
    Platform::logf(
        "[HomeFood] event=seek actor=1 pos=%.1f,%.1f target=%.1f,%.1f "
        "route=%u avoidMain=%u mainTask=%u\n",
        homeCompanionActor.x, homeCompanionActor.y, approachX, approachY,
        static_cast<unsigned>(homeCompanionActor.route.count),
        avoidMain ? 1U : 0U,
        static_cast<unsigned>(homeMainActor.task));
    return true;
}

bool AmoledApp::startCompanionFoodExit(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float foodX = room.available()
        ? room.foodX() + FOOD_FEED_OFFSET_X : FALLBACK_FOOD_APPROACH_X;
    const float foodY = room.available()
        ? room.foodY() + FOOD_FEED_OFFSET_Y : FALLBACK_FOOD_APPROACH_Y;
    const float clearance = std::max(
        26.0f, homeCompanionActor.geometry.footprint.radiusX +
                   homeMainActor.geometry.footprint.radiusX + 6.0f);
    // Try points on the room side of the bowl first. The outgoing eater has
    // priority over the incoming one, so it must be able to leave even when
    // the leader is already waiting at the approach.
    constexpr float DIRECTIONS[][2] = {
        {1.0f, 0.0f}, {0.8f, -0.6f}, {0.8f, 0.6f},
        {0.0f, -1.0f}, {0.0f, 1.0f}, {-0.8f, -0.6f},
        {-0.8f, 0.6f}, {-1.0f, 0.0f},
    };
    constexpr float RADII[] = {40.0f, 52.0f, 64.0f};
    for (float radius : RADII) {
        for (const auto& direction : DIRECTIONS) {
            const float x = foodX + radius * direction[0];
            const float y = foodY + radius * direction[1];
            const float dx = x - foodX;
            const float dy = y - foodY;
            if (dx * dx + dy * dy <= clearance * clearance) continue;
            if (beginCompanionMove(Home::Task::YIELDING, x, y,
                                   nowMs, false, false)) {
                Platform::logf(
                    "[HomeFood] event=exit actor=1 pos=%.1f,%.1f "
                    "target=%.1f,%.1f\n",
                    homeCompanionActor.x, homeCompanionActor.y, x, y);
                return true;
            }
        }
    }
    if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
        Platform::logf(
            "[HomeFood] event=exit_failed actor=1 pos=%.1f,%.1f\n",
            homeCompanionActor.x, homeCompanionActor.y);
        nextHomeFoodDebugLogMs = nowMs + 2000;
    }
    return false;
}

void AmoledApp::wakeHomeFoodArbitration(uint32_t nowMs) {
    cancelRoomAction(nowMs);
    if (homeRuntime.pairActive()) cancelPairInteraction(nowMs);
    // ADDED means the previous serving was empty. End any actor animation
    // that was only lingering until its old feeding-session timer expired so
    // ownership of the new serving can be decided from current hunger.
    const int8_t previousOwner = homeRuntime.owner(Home::Resource::BOWL);
    if (previousOwner >= 0) {
        homeRuntime.release(Home::Resource::BOWL,
                            static_cast<uint8_t>(previousOwner));
    }
    if (petMotion == PetMotion::SEEKING_FOOD ||
        petMotion == PetMotion::EATING ||
        (petMotion == PetMotion::STOPPING && petStoppingToEat)) {
        homeRuntime.stop(0, nowMs);
        petMotion = PetMotion::IDLE;
        petStopMotion = PetMotion::IDLE;
        petStoppingToEat = false;
        petTargetX = petX;
        petTargetY = petY;
        petFrame = 0;
    } else if (homeMainActor.task == Home::Task::FEEDING) {
        // Recover saves or older firmware states where the visual state was
        // cleared but the coordinator lease/task was left behind.
        Platform::logf(
            "[HomeFood] event=repair actor=0 stage=stale_feeding "
            "pos=%.1f,%.1f owner=%d\n",
            petX, petY,
            static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)));
        homeRuntime.stop(0, nowMs);
    }
    if (homeCompanionActor.task == Home::Task::SEEK_FOOD ||
        homeCompanionActor.task == Home::Task::FEEDING) {
        stopCompanion(nowMs);
    }
    homeRuntime.notify(Home::WorldEvent::BOWL_CHANGED);
    homeFoodArbitrationPending = true;
    nextHomeFoodArbitrationMs = nowMs;
    nextMindUpdateMs = nowMs;
    nextPetDecisionMs = nowMs;
    if (homeCompanionActor.active) {
        homeCompanionActor.nextMindUpdateMs = nowMs;
        homeCompanionActor.nextDecisionMs = nowMs;
        homeCompanionActor.foodWakeRetryAfterMs = 0;
    }
    Platform::logf(
        "[HomeFood] event=added satiety=%u/%u pos=%.1f,%.1f companion=%.1f,%.1f\n",
        gameState.teamCount > 0
            ? static_cast<unsigned>(gameState.team[0].satiety) : 0U,
        gameState.teamCount > 1
            ? static_cast<unsigned>(gameState.team[1].satiety) : 0U,
        petX, petY, homeCompanionActor.x, homeCompanionActor.y);
}

void AmoledApp::serviceHomeFoodArbitration(uint32_t nowMs) {
    if (!homeFoodArbitrationPending ||
        static_cast<int32_t>(nowMs - nextHomeFoodArbitrationMs) < 0) {
        return;
    }
    if (gameState.room.bowlCount == 0) {
        homeFoodArbitrationPending = false;
        return;
    }

    const int8_t preferred = preferredBowlEater(nowMs);
    if (preferred < 0) {
        homeFoodArbitrationPending = false;
        return;
    }
    const Home::ActorIntent preferredIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(static_cast<uint8_t>(preferred), nowMs),
            true);
    if (preferredIntent == Home::ActorIntent::WAKE_FOR_FOOD) {
        nextHomeFoodArbitrationMs = nowMs + 80;
        return;
    }

    const bool started = preferred == 0
        ? startMainFoodSeek(nowMs)
        : startCompanionFoodSeek(nowMs);
    if (started) {
        homeFoodArbitrationPending = false;
        return;
    }
    if (static_cast<int32_t>(nowMs - nextHomeFoodDebugLogMs) >= 0) {
        Platform::logf(
            "[HomeFood] event=retry preferred=%d owner=%d "
            "main=%.1f,%.1f companion=%.1f,%.1f\n",
            static_cast<int>(preferred),
            static_cast<int>(homeRuntime.owner(Home::Resource::BOWL)),
            petX, petY, homeCompanionActor.x, homeCompanionActor.y);
        nextHomeFoodDebugLogMs = nowMs + 2000;
    }
    // Soft actor avoidance means the other pet no longer needs a separate
    // YIELDING state just to clear the bowl approach. Retry the claim/route
    // after a short cooldown and keep both actors' intent stable.
    nextHomeFoodArbitrationMs = nowMs + 300;
}

bool AmoledApp::chooseCompanionTarget(float& x, float& y) {
    if (!homeCompanionActor.active) return false;
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    const uint8_t polygonCount = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    const int minimumX = room.available() ? room.walkMinX()
                                          : FALLBACK_ROOM_MIN_X;
    const int maximumX = room.available() ? room.walkMaxX()
                                          : FALLBACK_ROOM_MAX_X;
    const int minimumY = room.available() ? room.walkMinY()
                                          : FALLBACK_ROOM_MIN_Y;
    const int maximumY = room.available() ? room.walkMaxY()
                                          : FALLBACK_ROOM_MAX_Y;
    const int radiusX = std::max<int>(
        1, homeCompanionActor.behavior.wanderRadiusX);
    const int radiusY = std::max<int>(
        1, homeCompanionActor.behavior.wanderRadiusY);
    const float visualOverlapX = petFootprintRadiusX +
        homeCompanionActor.geometry.footprint.radiusX + 1.0f;
    const float visualOverlapY = petFootprintRadiusY +
        homeCompanionActor.geometry.footprint.radiusY + 1.0f;
    for (uint8_t attempt = 0; attempt < 48; ++attempt) {
        const int candidateX = attempt < 32
            ? std::clamp(
                  static_cast<int>(std::lround(homeCompanionActor.x)) +
                      static_cast<int>(GameRandom::random(
                          -radiusX, radiusX + 1)),
                  minimumX, maximumX)
            : static_cast<int>(GameRandom::random(
                  minimumX, maximumX + 1));
        const int candidateY = attempt < 32
            ? std::clamp(
                  static_cast<int>(std::lround(homeCompanionActor.y)) +
                      static_cast<int>(GameRandom::random(
                          -radiusY, radiusY + 1)),
                  minimumY, maximumY)
            : static_cast<int>(GameRandom::random(
                  minimumY, maximumY + 1));
        const float dx = candidateX - homeMainActor.x;
        const float dy = candidateY - homeMainActor.y;
        if (std::fabs(dx) < visualOverlapX &&
            std::fabs(dy) < visualOverlapY) continue;
        if (std::fabs(candidateX - homeCompanionActor.x) < 8.0f &&
            std::fabs(candidateY - homeCompanionActor.y) < 4.0f) {
            continue;
        }
        if (!RoomMovementArea::containsFootprint(
                polygon, polygonCount, static_cast<float>(candidateX),
                static_cast<float>(candidateY),
                homeCompanionActor.geometry.footprint)) {
            continue;
        }
        x = static_cast<float>(candidateX);
        y = static_cast<float>(candidateY);
        return true;
    }
    return false;
}

float AmoledApp::companionSleepMinDistance() const {
    const float mainRadius = std::clamp(
        homeMainActor.geometry.footprint.radiusX, 10.0f, 20.0f);
    const float companionRadius = std::clamp(
        homeCompanionActor.geometry.footprint.radiusX, 10.0f, 20.0f);
    return std::clamp(mainRadius + companionRadius + 8.0f, 30.0f, 44.0f);
}

bool AmoledApp::companionSleepSpotUsableWithDistance(
    float x, float y, float minDistance) const {
    if (!homeCompanionActor.active) return false;
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    const uint8_t polygonCount = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    if (!RoomMovementArea::containsFootprint(
            polygon, polygonCount, x, y,
            homeCompanionActor.geometry.footprint)) {
        return false;
    }

    if (room.available() &&
        x >= static_cast<float>(room.bedMinX()) &&
        x <= static_cast<float>(room.bedMaxX()) &&
        y >= static_cast<float>(room.bedMinY()) &&
        y <= static_cast<float>(room.bedMaxY())) {
        return false;
    }

    const float dx = x - homeMainActor.x;
    const float dy = y - homeMainActor.y;
    const float visualOverlapX = petFootprintRadiusX +
        homeCompanionActor.geometry.footprint.radiusX + 1.0f;
    const float visualOverlapY = petFootprintRadiusY +
        homeCompanionActor.geometry.footprint.radiusY + 1.0f;
    if (std::fabs(dx) < visualOverlapX && std::fabs(dy) < visualOverlapY) {
        return false;
    }

    // Keep the companion's sleeping pose away from the leader's bed pose,
    // matching Stick's visitor sleep rule while using AMOLED ground coords.
    const float bedX = room.available()
        ? static_cast<float>(room.bedX()) : 76.0f;
    const float bedY = room.available()
        ? static_cast<float>(room.bedY()) : 99.0f;
    const float bedDx = x - bedX;
    const float bedDy = y - bedY;
    return bedDx * bedDx + bedDy * bedDy >= minDistance * minDistance;
}

bool AmoledApp::companionSleepSpotUsable(float x, float y) const {
    return companionSleepSpotUsableWithDistance(
        x, y, companionSleepMinDistance());
}

bool AmoledApp::companionPointBlocksBedRoute(float x, float y) const {
    const float clearance = std::max(
        20.0f, homeMainActor.geometry.footprint.radiusX +
                   homeCompanionActor.geometry.footprint.radiusX + 2.0f);
    const float clearanceSq = clearance * clearance;
    auto segmentDistanceSq = [x, y](float fromX, float fromY,
                                    float toX, float toY) {
        const float dx = toX - fromX;
        const float dy = toY - fromY;
        const float lengthSq = dx * dx + dy * dy;
        float t = lengthSq <= 0.001f
            ? 0.0f
            : ((x - fromX) * dx + (y - fromY) * dy) / lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
        const float nearestX = fromX + dx * t;
        const float nearestY = fromY + dy * t;
        const float distanceX = x - nearestX;
        const float distanceY = y - nearestY;
        return distanceX * distanceX + distanceY * distanceY;
    };

    float fromX = homeMainActor.x;
    float fromY = homeMainActor.y;
    if (homeMainActor.route.index >= homeMainActor.route.count) {
        return segmentDistanceSq(
            fromX, fromY, homeMainActor.targetX,
            homeMainActor.targetY) < clearanceSq;
    }
    for (uint8_t index = homeMainActor.route.index;
         index < homeMainActor.route.count; ++index) {
        const float toX = homeMainActor.route.x[index];
        const float toY = homeMainActor.route.y[index];
        if (segmentDistanceSq(fromX, fromY, toX, toY) < clearanceSq) {
            return true;
        }
        fromX = toX;
        fromY = toY;
    }
    return false;
}

bool AmoledApp::chooseCompanionSleepSpot(float& x, float& y,
                                         bool avoidBedRoute) const {
    if (!homeCompanionActor.active) return false;
    RoomResource& room = RoomResource::ins();
    const float minDistance = companionSleepMinDistance();
    const bool mainSeekingBed = avoidBedRoute ||
        homeMainActor.task == Home::Task::SEEK_SLEEP ||
        (homeMainActor.task == Home::Task::TURNING &&
         homeMainActor.resumeTask == Home::Task::SEEK_SLEEP);

    RoomResource::BehaviorAnchor anchor{};
    const bool hasAnchor = room.available() && room.findBehaviorAnchor(
        RoomResource::BehaviorAnchorType::VISITOR_SLEEP, anchor);
    const float preferredX = hasAnchor
        ? static_cast<float>(anchor.footX)
        : (room.available() ? (room.walkMinX() + room.walkMaxX()) * 0.5f
                            : (FALLBACK_ROOM_MIN_X + FALLBACK_ROOM_MAX_X) * 0.5f);
    const float preferredY = hasAnchor
        ? static_cast<float>(anchor.footY)
        : (room.available() ? (room.walkMinY() + room.walkMaxY()) * 0.5f
                            : (FALLBACK_ROOM_MIN_Y + FALLBACK_ROOM_MAX_Y) * 0.5f);

    if (hasAnchor && companionSleepSpotUsableWithDistance(
            preferredX, preferredY, minDistance) &&
        (!mainSeekingBed || !companionPointBlocksBedRoute(
            preferredX, preferredY))) {
        x = preferredX;
        y = preferredY;
        return true;
    }

    const int minimumX = room.available() ? room.walkMinX()
                                          : FALLBACK_ROOM_MIN_X;
    const int maximumX = room.available() ? room.walkMaxX()
                                          : FALLBACK_ROOM_MAX_X;
    const int minimumY = room.available() ? room.walkMinY()
                                          : FALLBACK_ROOM_MIN_Y;
    const int maximumY = room.available() ? room.walkMaxY()
                                          : FALLBACK_ROOM_MAX_Y;
    float bestScore = 1000000.0f;
    bool found = false;
    for (int candidateY = minimumY; candidateY <= maximumY; candidateY += 3) {
        for (int candidateX = minimumX; candidateX <= maximumX; candidateX += 3) {
            const float pointX = static_cast<float>(candidateX);
            const float pointY = static_cast<float>(candidateY);
            if (!companionSleepSpotUsableWithDistance(
                    pointX, pointY, minDistance)) continue;
            if (mainSeekingBed && companionPointBlocksBedRoute(pointX, pointY)) {
                continue;
            }
            const float dx = pointX - preferredX;
            const float dy = pointY - preferredY;
            const float score = dx * dx + dy * dy;
            if (score >= bestScore) continue;
            bestScore = score;
            x = pointX;
            y = pointY;
            found = true;
        }
    }
    return found;
}

bool AmoledApp::beginCompanionMove(Home::Task task, float x, float y,
                                   uint32_t nowMs, bool allowOutsideStart,
                                   bool avoidOther) {
    if (!homeCompanionActor.active ||
        !homeRuntime.transition(1, task, nowMs)) {
        return false;
    }
    // The visitor entry ends on the doorway point, which sits outside the
    // walk polygon. Routing out of it needs the ingress allowance, otherwise
    // every wander/seek plan fails and the visitor stays frozen at the door.
    if (!allowOutsideStart &&
        !companionFootprintInsideWalkArea(
            homeCompanionActor.x, homeCompanionActor.y)) {
        allowOutsideStart = true;
    }
    if (!homeRuntime.planRoute(
            1, x, y, allowOutsideStart, avoidOther)) {
        homeRuntime.stop(1, nowMs, 700);
        return false;
    }
    if (task == Home::Task::SEEK_SLEEP) {
        homeCompanionActor.sleepX = x;
        homeCompanionActor.sleepY = y;
        homeCompanionActor.sleepSpotValid = true;
    }
    const float dx = x - homeCompanionActor.x;
    const float dy = y - homeCompanionActor.y;
    companionLongMove = std::sqrt(dx * dx + dy * dy) > 14.0f;
    companionDirection = petDirectionForDelta(dx, dy);
    companionFrame = 0;
    nextCompanionFrameMs = nowMs;
    return true;
}

void AmoledApp::stopCompanion(uint32_t nowMs, uint32_t idleDelayMs) {
    homeRuntime.stop(1, nowMs, idleDelayMs);
    companionFrame = 0;
    nextCompanionFrameMs = nowMs + 520;
}

void AmoledApp::requestHomeActorRows(float previousY, float currentY) {
    const int previousScreenY = worldToScreenY(previousY);
    const int currentScreenY = worldToScreenY(currentY);
    // Room artwork is expanded from 1x source pixels into 2x2 framebuffer
    // blocks. Keep the dirty band on that same grid and include the complete
    // sprite/shadow envelope from both positions; otherwise one row from the
    // previous frame can survive at the edge of a moving actor.
    constexpr int ACTOR_TOP_MARGIN = 192;
    constexpr int ACTOR_BOTTOM_MARGIN = 48;
    const int rawTop = std::max(
        HOME_ROOM_TOP,
        std::min(previousScreenY, currentScreenY) - ACTOR_TOP_MARGIN);
    const int rawBottom = std::min(
        HOME_STATUS_TOP,
        std::max(previousScreenY, currentScreenY) + ACTOR_BOTTOM_MARGIN);
    const int scale = AmoledUi::RESOURCE_SCALE;
    const int top = std::max(
        HOME_ROOM_TOP, rawTop - rawTop % scale);
    const int bottom = std::min(
        HOME_STATUS_TOP,
        ((rawBottom + scale - 1) / scale) * scale);
    requestRenderRows(static_cast<uint16_t>(top),
                      static_cast<uint16_t>(bottom));
}

void AmoledApp::updateCompanion(uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2) {
        lastCompanionUpdateMs = nowMs;
        return;
    }
    float elapsedSeconds = static_cast<float>(
        nowMs - lastCompanionUpdateMs) / 1000.0f;
    lastCompanionUpdateMs = nowMs;
    elapsedSeconds = std::min(elapsedSeconds, 0.1f);

    updateVisitorMotion(nowMs, elapsedSeconds);
    if (homeCompanionActor.hidden && visitSession.active() &&
        visitSession.viewModel().localIsHost &&
        !visitSession.visitorArrivalReady()) return;
    if (visitorMotion == VisitorMotion::HOST_APPROACH ||
        visitorMotion == VisitorMotion::ENTERING ||
        visitorMotion == VisitorMotion::EXITING) {
        return;
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    {
        static uint32_t s_companionSampleMs = 0;
        if (static_cast<int32_t>(nowMs - s_companionSampleMs) >= 0) {
            s_companionSampleMs = nowMs + 1000;
            Platform::logf(
                "[HomeDiag] companion task=%d motion=%d pos=%.1f,%.1f "
                "target=%.1f,%.1f decisionIn=%ld moveMode=%d pair=%d "
                "visitor=%d\n",
                static_cast<int>(homeCompanionActor.task),
                static_cast<int>(homeCompanionActor.motion),
                homeCompanionActor.x, homeCompanionActor.y,
                homeCompanionActor.targetX, homeCompanionActor.targetY,
                static_cast<long>(static_cast<int32_t>(
                    homeCompanionActor.nextDecisionMs - nowMs)),
                static_cast<int>(homeCompanionActor.behavior.movementMode),
                static_cast<int>(pairPhase),
                static_cast<int>(visitorMotion));
        }
    }
#endif

    Game::MonsterRuntime& monster = gameState.team[1];
    const Home::ActorIntent initialCompanionIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(1, nowMs),
            gameState.room.bowlCount > 0);
    if (initialCompanionIntent == Home::ActorIntent::FAINT_REST) {
        cancelPairInteraction(nowMs);
        homeRuntime.transition(1, Home::Task::FAINTED, nowMs, 0, true);
        if (!homeCompanionActor.faintRestActive) {
            float sleepX = homeCompanionActor.x;
            float sleepY = homeCompanionActor.y;
            if (chooseCompanionSleepSpot(sleepX, sleepY)) {
                homeCompanionActor.x = sleepX;
                homeCompanionActor.y = sleepY;
            }
            homeCompanionActor.targetX = homeCompanionActor.x;
            homeCompanionActor.targetY = homeCompanionActor.y;
            homeCompanionActor.faintRestActive = true;
            companionFrame = 0;
            nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        }
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame;
            nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            requestHomeActorRows(
                homeCompanionActor.y, homeCompanionActor.y);
        }
        return;
    }
    if (homeCompanionActor.faintRestActive) {
        homeCompanionActor.faintRestActive = false;
        stopCompanion(nowMs, 700);
        homeCompanionActor.mind.reset(nowMs);
    }

    PokemonSprites::PetAnimationProfile animation{};
    PokemonSprites::petAnimationProfile(monster.speciesId, animation);
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool sleepTime = care.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
    if (static_cast<int32_t>(nowMs -
                             homeCompanionActor.nextMindUpdateMs) >= 0) {
        homeCompanionActor.mind.update(
            monster, sleepTime,
            care.needsFood && monster.origin != Game::Origin::VISITOR &&
                gameState.room.bowlCount > 0,
            nowMs);
        homeCompanionActor.nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
    }

    if (homeCompanionActor.task == Home::Task::PAIR_ACTION) return;

    if (homeCompanionActor.task == Home::Task::FEEDING) {
        const bool canEat =
            homeRuntime.owner(Home::Resource::BOWL) == 1 &&
            monster.origin != Game::Origin::VISITOR &&
            gameState.room.bowlCount > 0 &&
            monster.satiety < MONSTER_FEED_TARGET_SATIETY;
        if (canEat && static_cast<int32_t>(nowMs - companionFoodBiteMs) >= 0) {
            FoodConsumeResult result =
                Game::HomeCare::consumeBowlFood(gameState, 1);
            companionFoodBiteMs = nowMs + GameRandom::range(1000, 1601);
            if (result.consumed) {
                if (result.reaction == FoodReaction::LIKED ||
                    result.foodIndex == Game::ROOM_TASTY_FOOD_INDEX) {
                    heartsUntil = nowMs + 900;
                }
                saveState();
            }
        }
        if (!canEat ||
            static_cast<int32_t>(nowMs - homeCompanionActor.taskUntilMs) >= 0) {
            homeCompanionActor.mind.onAte(nowMs);
            homeRuntime.beginBowlClearance(1, nowMs);
            stopCompanion(nowMs);
            if (homeActorBlocksFoodApproach(1)) {
                startCompanionFoodExit(nowMs);
            }
            if (gameState.room.bowlCount > 0 &&
                preferredBowlEater(nowMs) >= 0) {
                homeFoodArbitrationPending = true;
                nextHomeFoodArbitrationMs = nowMs;
                nextMindUpdateMs = nowMs;
                nextPetDecisionMs = nowMs;
            }
            requestRenderRows(HOME_ROOM_TOP, 448);
        }
        return;
    }

    if (homeCompanionActor.task == Home::Task::SLEEPING) {
        const Home::ActorIntent sleepIntent =
            Home::ActorController::survivalIntent(
                homeActorObservation(1, nowMs),
                gameState.room.bowlCount > 0);
        const bool wakeForFood =
            sleepIntent == Home::ActorIntent::WAKE_FOR_FOOD;
        if (sleepIntent == Home::ActorIntent::WAKE || wakeForFood) {
            homeRuntime.release(Home::Resource::BED, 1);
            homeCompanionActor.sleepSpotValid = false;
            stopCompanion(nowMs, wakeForFood ? 0 : 700);
            homeCompanionActor.foodWakeRetryAfterMs =
                wakeForFood ? nowMs + COMPANION_NIGHT_FOOD_RETRY_MS : 0;
            homeCompanionActor.mind.onRested(nowMs);
        } else if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame;
            nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            requestHomeActorRows(
                homeCompanionActor.y, homeCompanionActor.y);
        }
        return;
    }

    const bool companionClearingBowl =
        homeRuntime.bowlSession().clearing() &&
        homeRuntime.bowlSession().actorId == 1;
    if (homeCompanionActor.task == Home::Task::IDLE &&
        homeActorBlocksFoodApproach(1) &&
        (companionClearingBowl ||
         (gameState.room.bowlCount > 0 &&
          preferredBowlEater(nowMs) == 0))) {
        if (static_cast<int32_t>(nowMs -
                                 homeCompanionActor.nextDecisionMs) >= 0) {
            startCompanionFoodExit(nowMs);
        }
        return;
    }

    const bool routedTask = homeCompanionActor.task == Home::Task::WANDER ||
        homeCompanionActor.task == Home::Task::SEEK_FOOD ||
        homeCompanionActor.task == Home::Task::SEEK_SLEEP ||
        homeCompanionActor.task == Home::Task::YIELDING;
    if (routedTask) {
        float waypointX = homeCompanionActor.targetX;
        float waypointY = homeCompanionActor.targetY;
        homeCompanionActor.route.current(waypointX, waypointY);
        companionDirection = petDirectionForDelta(
            waypointX - homeCompanionActor.x,
            waypointY - homeCompanionActor.y);
        const float previousY = homeCompanionActor.y;
        float speed = homeCompanionActor.task == Home::Task::SEEK_FOOD
            ? 19.0f : homeCompanionActor.task == Home::Task::WANDER
                ? 10.5f : 12.5f;
        speed *= homeCompanionActor.behavior.moveSpeedScale;
        if (monster.mood < 40 || monster.satiety < 20) speed *= 0.72f;
        const Home::RouteStep result = homeRuntime.advanceRoute(
            1, nowMs, std::max(3.0f, speed), elapsedSeconds, 0.8f,
            homeCompanionActor.task == Home::Task::WANDER ||
                (homeCompanionActor.task == Home::Task::SEEK_FOOD &&
                 companionFoodAvoidsMain()));
        if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            ++companionFrame;
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
            requestHomeActorRows(previousY, homeCompanionActor.y);
        }
        if (result == Home::RouteStep::ARRIVED) {
            const Home::Task arrivedTask = homeCompanionActor.task;
            if (arrivedTask == Home::Task::SEEK_FOOD) {
                homeRuntime.transition(
                    1, Home::Task::FEEDING, nowMs,
                    static_cast<uint32_t>(GameRandom::range(3200, 5601)),
                    true);
                companionFoodBiteMs = nowMs + GameRandom::range(700, 1301);
            } else if (arrivedTask == Home::Task::SEEK_SLEEP) {
                homeRuntime.transition(
                    1, Home::Task::SLEEPING, nowMs, 0, true);
                nextCompanionFrameMs = nowMs + HOME_SLEEP_FRAME_MS;
            } else {
                if (arrivedTask == Home::Task::YIELDING &&
                    homeRuntime.bowlSession().clearing() &&
                    homeRuntime.bowlSession().actorId == 1 &&
                    !homeActorBlocksFoodApproach(1)) {
                    homeRuntime.finishBowlClearance(1, nowMs);
                }
                stopCompanion(nowMs, static_cast<uint32_t>(GameRandom::range(
                    homeCompanionActor.behavior.idleMinMs,
                    homeCompanionActor.behavior.idleMaxMs + 1)));
            }
            requestHomeActorRows(previousY, homeCompanionActor.y);
        } else if (result == Home::RouteStep::BLOCKED ||
                   result == Home::RouteStep::NO_ROUTE) {
            if (homeCompanionActor.task == Home::Task::SEEK_FOOD) {
                Platform::logf(
                    "[HomeFood] event=blocked actor=1 stage=%s owner=%d "
                    "pos=%.1f,%.1f target=%.1f,%.1f other=%.1f,%.1f\n",
                    result == Home::RouteStep::BLOCKED
                        ? "advance" : "no_route",
                    static_cast<int>(homeRuntime.owner(
                        Home::Resource::BOWL)),
                    homeCompanionActor.x, homeCompanionActor.y,
                    homeCompanionActor.targetX,
                    homeCompanionActor.targetY, petX, petY);
            }
            stopCompanion(nowMs, 700);
        }
        return;
    }

    if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
        const uint8_t frameCount = std::max<uint8_t>(
            1, animation.idleFrames);
        companionFrame = static_cast<uint8_t>(
            (companionFrame + 1) % frameCount);
        nextCompanionFrameMs = nowMs +
            (animation.idleFrameMs ? animation.idleFrameMs : 520);
        requestHomeActorRows(
            homeCompanionActor.y, homeCompanionActor.y);
    }
    if (homeFoodArbitrationPending &&
        preferredBowlEater(nowMs) == 1) return;
    if (homeCompanionActor.behavior.movementMode ==
            MonsterMovementMode::STATIONARY ||
        static_cast<int32_t>(nowMs -
                             homeCompanionActor.nextDecisionMs) < 0) {
        return;
    }

    const Home::ActorIntent companionIntent =
        Home::ActorController::survivalIntent(
            homeActorObservation(1, nowMs),
            gameState.room.bowlCount > 0);
    if (companionIntent == Home::ActorIntent::SEEK_FOOD &&
        preferredBowlEater(nowMs) == 1) {
        if (startCompanionFoodSeek(nowMs)) {
            return;
        }
    }

    if (companionIntent == Home::ActorIntent::SEEK_SLEEP) {
        float sleepX = homeCompanionActor.x;
        float sleepY = homeCompanionActor.y;
        if (chooseCompanionSleepSpot(sleepX, sleepY) &&
            beginCompanionMove(Home::Task::SEEK_SLEEP,
                               sleepX, sleepY, nowMs)) {
            return;
        }
    }

    float targetX = homeCompanionActor.x;
    float targetY = homeCompanionActor.y;
    if (chooseCompanionTarget(targetX, targetY) &&
        beginCompanionMove(Home::Task::WANDER, targetX, targetY, nowMs)) {
        const float dx = targetX - homeCompanionActor.x;
        const float dy = targetY - homeCompanionActor.y;
        companionLongMove = std::sqrt(dx * dx + dy * dy) > 14.0f;
        companionDirection = petDirectionForDelta(dx, dy);
        homeCompanionActor.targetX = targetX;
        homeCompanionActor.targetY = targetY;
        homeCompanionActor.motion = Home::MotionPhase::ROUTE;
        companionFrame = 0;
        nextCompanionFrameMs = nowMs;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[HomeDiag] companion wander from=%.1f,%.1f to=%.1f,%.1f\n",
                       homeCompanionActor.x, homeCompanionActor.y,
                       targetX, targetY);
#endif
    } else {
        homeCompanionActor.nextDecisionMs = nowMs + 700;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[HomeDiag] companion decision failed pos=%.1f,%.1f "
                       "target=%.1f,%.1f intent=%d\n",
                       homeCompanionActor.x, homeCompanionActor.y,
                       targetX, targetY,
                       static_cast<int>(companionIntent));
#endif
    }
}

bool AmoledApp::pairInteractionAllowed() const {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        gameState.teamCount < 2 || !homeCompanionActor.active ||
        visitorMotion == VisitorMotion::HOST_APPROACH ||
        visitorMotion == VisitorMotion::ENTERING ||
        visitorMotion == VisitorMotion::EXITING ||
        roomAction != RoomAction::NONE ||
        petMotion != PetMotion::IDLE ||
        homeCompanionActor.task != Home::Task::IDLE) {
        return false;
    }
#if STICKMON_HAS_CLAW
    if (Stickmon::ClawRuntime::instance().autonomyActive()) return false;
#endif
    for (uint8_t slot = 0; slot < 2; ++slot) {
        const Game::MonsterRuntime& monster = gameState.team[slot];
        const Game::SpeciesCareProfile care =
            Game::speciesCareProfileFor(monster.speciesId);
        if (monster.fainted || monster.hpCur == 0 || monster.hpMax == 0 ||
            monster.majorStatus == Game::MajorStatus::SLEEP ||
            static_cast<uint32_t>(monster.hpCur) * 100UL <=
                static_cast<uint32_t>(monster.hpMax) * 20UL ||
            (care.usesBed && Game::isSleepCareTime(
                 gameState.gameMinutesTotal, monster.nature))) {
            return false;
        }
        if (care.needsFood && monster.satiety <= 25) return false;
    }
    const Game::SpeciesCareProfile first =
        Game::speciesCareProfileFor(gameState.team[0].speciesId);
    const Game::SpeciesCareProfile second =
        Game::speciesCareProfileFor(gameState.team[1].speciesId);
    return first.canMove || second.canMove;
}

void AmoledApp::schedulePairInteraction(uint32_t nowMs, bool immediate) {
    nextPairInteractionMs = immediate
        ? nowMs + 300
        : nowMs + static_cast<uint32_t>(GameRandom::range(
              PAIR_INTERACTION_MIN_INTERVAL_MS,
              PAIR_INTERACTION_MAX_INTERVAL_MS + 1));
}

bool AmoledApp::startPairInteraction(uint32_t nowMs, bool forceChase,
                                     bool forceTalk) {
    if (!forceTalk) {
        if (!pairInteractionAllowed()) return false;
    } else if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
               gameState.teamCount < 2 || !homeCompanionActor.active ||
               visitorMotion != VisitorMotion::ACTIVE ||
               roomAction != RoomAction::NONE ||
               gameState.team[0].fainted || gameState.team[0].hpCur == 0 ||
               gameState.team[1].fainted || gameState.team[1].hpCur == 0) {
        return false;
    }
    const Game::SpeciesCareProfile first =
        Game::speciesCareProfileFor(gameState.team[0].speciesId);
    const Game::SpeciesCareProfile second =
        Game::speciesCareProfileFor(gameState.team[1].speciesId);
    if (!first.canMove || !second.canMove) {
        pairActivity = Home::PairActivity::TALK;
    } else {
        pairActivity = !forceTalk && (forceChase || GameRandom::random(100) >= 45)
            ? Home::PairActivity::CHASE : Home::PairActivity::TALK;
    }
    if (!homeRuntime.beginPair(pairActivity, nowMs)) {
        pairActivity = Home::PairActivity::NONE;
        schedulePairInteraction(nowMs);
        return false;
    }

    pairLeaderMain = GameRandom::random(2) == 0;
    if (!first.canMove) pairLeaderMain = false;
    if (!second.canMove) pairLeaderMain = true;
    const float talkDistance = (pairIdleWidthPx(gameState.team[0].speciesId) +
                                pairIdleWidthPx(gameState.team[1].speciesId)) /
                                   (2.0f * AmoledUi::RESOURCE_SCALE) +
                               PAIR_TALK_MIN_GAP_PX /
                                   static_cast<float>(AmoledUi::RESOURCE_SCALE);
    const float approachDistance = pairActivity == Home::PairActivity::TALK
        ? talkDistance : PAIR_APPROACH_DISTANCE;
    pairTalkParallelApproach = false;
    pairTalkMainArrived = false;
    pairTalkCompanionArrived = false;
    pairTalkMainApproachSpeed = 0.0f;
    pairTalkCompanionApproachSpeed = 0.0f;
    bool planned = false;
    if (pairActivity == Home::PairActivity::TALK &&
        first.canMove && second.canMove) {
        // The two fixed talk points are a coordinated choreography. Plan both
        // paths without treating the other actor's starting pose as a static
        // obstacle, then scale their speeds to reach the stage together.
        pairLeaderMain = true;
        auto planFixedRoute = [&](uint8_t actorId, Home::Actor& actor,
                                  float targetX, float targetY,
                                  bool& arrived) {
            actor.targetX = targetX;
            actor.targetY = targetY;
            arrived = std::hypot(targetX - actor.x, targetY - actor.y) <=
                PAIR_APPROACH_GOAL_TOLERANCE;
            if (arrived) {
                actor.route.clear();
                return true;
            }
            return homeRuntime.planRoute(
                actorId, targetX, targetY, false, false);
        };
        const bool mainPlanned = planFixedRoute(
            0, homeMainActor, PAIR_TALK_MAIN_X, PAIR_TALK_STAGE_Y,
            pairTalkMainArrived);
        const bool companionPlanned = mainPlanned && planFixedRoute(
            1, homeCompanionActor, PAIR_TALK_COMPANION_X,
            PAIR_TALK_STAGE_Y, pairTalkCompanionArrived);
        if (mainPlanned && companionPlanned) {
            const float mainDistance = pairRouteDistance(homeMainActor);
            const float companionDistance = pairRouteDistance(homeCompanionActor);
            const float sharedDurationSeconds = std::max(
                0.1f, std::max(mainDistance, companionDistance) /
                          PAIR_APPROACH_SPEED);
            pairTalkMainApproachSpeed = pairTalkMainArrived
                ? 0.0f : mainDistance / sharedDurationSeconds;
            pairTalkCompanionApproachSpeed = pairTalkCompanionArrived
                ? 0.0f : companionDistance / sharedDurationSeconds;
            pairTalkParallelApproach = true;
            planned = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
            Platform::logf("[FriendDiag] talk fixed-stage companion=%.1f,%.1f "
                           "main=%.1f,%.1f speeds=%.2f,%.2f\n",
                           PAIR_TALK_COMPANION_X, PAIR_TALK_STAGE_Y,
                           PAIR_TALK_MAIN_X, PAIR_TALK_STAGE_Y,
                           pairTalkCompanionApproachSpeed,
                           pairTalkMainApproachSpeed);
#endif
        } else {
            homeMainActor.route.clear();
            homeCompanionActor.route.clear();
        }
    }
    const uint8_t firstMover = pairLeaderMain ? 0 : 1;
    const bool fixedTalk = pairActivity == Home::PairActivity::TALK &&
        first.canMove && second.canMove;
    for (int attempt = 0;
         attempt < ((!fixedTalk && pairActivity == Home::PairActivity::TALK) ? 2 : 1) &&
         !planned; ++attempt) {
        if (fixedTalk) break;
        const uint8_t mover = attempt == 0 ? firstMover : 1 - firstMover;
        const Home::Actor& other = mover == 0
            ? homeCompanionActor : homeMainActor;
        const float offsets[][2] = {
            {-approachDistance, 0.0f}, {approachDistance, 0.0f},
            {0.0f, -approachDistance}, {0.0f, approachDistance},
        };
        for (const auto& offset : offsets) {
            // Vertical separation must account for the full sprite height;
            // only horizontal destinations guarantee the requested visual gap.
            if (pairActivity == Home::PairActivity::TALK && offset[0] == 0.0f) {
                continue;
            }
            if (homeRuntime.planRoute(
                    mover, other.x + offset[0], other.y + offset[1])) {
                pairLeaderMain = mover == 0;
                planned = true;
                break;
            }
        }
    }
    if (!planned) {
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[FriendDiag] talk route-failed kind=%u main=%.1f,%.1f "
                       "companion=%.1f,%.1f offset=%.1f\n",
                       static_cast<unsigned>(debugContactKind),
                       homeMainActor.x, homeMainActor.y,
                       homeCompanionActor.x, homeCompanionActor.y,
                       approachDistance);
#endif
        homeRuntime.endPair(nowMs);
        pairActivity = Home::PairActivity::NONE;
        schedulePairInteraction(nowMs);
        return false;
    }

    pairPhase = PairPhase::APPROACH;
    pairPhaseStartedMs = nowMs;
    if (pairTalkParallelApproach) {
        pairPhaseUntilMs = nowMs + std::max(
            pairApproachBudgetMs(homeMainActor),
            pairApproachBudgetMs(homeCompanionActor));
    } else {
        const Home::Actor& movingActor = pairLeaderMain
            ? homeMainActor : homeCompanionActor;
        pairPhaseUntilMs = nowMs + pairApproachBudgetMs(movingActor);
    }
    pairInteractionUntilMs = nowMs +
        (pairActivity == Home::PairActivity::CHASE
             ? PAIR_CHASE_TIMEOUT_MS : PAIR_TALK_TIMEOUT_MS);
    pairChaseLegsRemaining = PAIR_CHASE_LEGS;
    pairMainRenderOffsetY = 0.0f;
    pairCompanionRenderOffsetY = 0.0f;
#if STICKMON_ENABLE_DEBUG_FEATURES
    Platform::logf("[HomeDiag] pair start activity=%d leaderMain=%d "
                   "main=%.1f,%.1f companion=%.1f,%.1f\n",
                   static_cast<int>(pairActivity), pairLeaderMain ? 1 : 0,
                   homeMainActor.x, homeMainActor.y,
                   homeCompanionActor.x, homeCompanionActor.y);
#endif
    return true;
}

bool AmoledApp::updatePairInteraction(uint32_t nowMs,
                                      float elapsedSeconds) {
    if (pairPhase == PairPhase::NONE) {
        if (static_cast<int32_t>(nowMs - nextPairInteractionMs) < 0) {
            return false;
        }
        if (!startPairInteraction(nowMs)) {
            nextPairInteractionMs = nowMs + PAIR_INTERACTION_RETRY_MS;
            return false;
        }
    }

    if (gameState.teamCount < 2 || !homeCompanionActor.active ||
        gameState.team[0].fainted || gameState.team[0].hpCur == 0 ||
        gameState.team[1].fainted || gameState.team[1].hpCur == 0 ||
        gameState.team[0].majorStatus == Game::MajorStatus::SLEEP ||
        gameState.team[1].majorStatus == Game::MajorStatus::SLEEP ||
        visitorMotion == VisitorMotion::EXITING) {
        cancelPairInteraction(nowMs);
        return false;
    }
    for (uint8_t slot = 0; slot < 2 && !pairArrivalTalk; ++slot) {
        const Game::MonsterRuntime& monster = gameState.team[slot];
        const Game::SpeciesCareProfile care =
            Game::speciesCareProfileFor(monster.speciesId);
        if ((care.usesBed && Game::isSleepCareTime(
                 gameState.gameMinutesTotal, monster.nature)) ||
            (care.needsFood && monster.satiety <= 25)) {
            cancelPairInteraction(nowMs);
            return false;
        }
    }

    auto syncMainPose = [&]() {
        petX = homeMainActor.x;
        petY = homeMainActor.y;
        petTargetX = homeMainActor.targetX;
        petTargetY = homeMainActor.targetY;
        updateCamera();
    };
    auto faceActors = [&]() {
        companionDirection = petDirectionForDelta(
            homeMainActor.x - homeCompanionActor.x,
            homeMainActor.y - homeCompanionActor.y);
        petDirection = petDirectionForDelta(
            homeCompanionActor.x - homeMainActor.x,
            homeCompanionActor.y - homeMainActor.y);
    };
    auto planChaseLeg = [&]() {
        const uint8_t leader = pairLeaderMain ? 0 : 1;
        const uint8_t follower = pairLeaderMain ? 1 : 0;
        Home::Actor& leaderActor = pairLeaderMain
            ? homeMainActor : homeCompanionActor;
        const float followX = leaderActor.x;
        const float followY = leaderActor.y;
        RoomResource& room = RoomResource::ins();
        const int minimumX = room.available() ? room.walkMinX()
                                              : FALLBACK_ROOM_MIN_X;
        const int maximumX = room.available() ? room.walkMaxX()
                                              : FALLBACK_ROOM_MAX_X;
        const int minimumY = room.available() ? room.walkMinY()
                                              : FALLBACK_ROOM_MIN_Y;
        const int maximumY = room.available() ? room.walkMaxY()
                                              : FALLBACK_ROOM_MAX_Y;
        for (uint8_t attempt = 0; attempt < 48; ++attempt) {
            const float targetX = static_cast<float>(GameRandom::random(
                minimumX, maximumX + 1));
            const float targetY = static_cast<float>(GameRandom::random(
                minimumY, maximumY + 1));
            const float dx = targetX - leaderActor.x;
            const float dy = targetY - leaderActor.y;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq < 1156.0f || distanceSq > 7056.0f) continue;
            if (!homeRuntime.planRoute(leader, targetX, targetY,
                                       false, false)) {
                continue;
            }
            if (!homeRuntime.planRoute(follower, followX, followY,
                                       false, false)) {
                leaderActor.route.clear();
                continue;
            }
            return true;
        }
        return false;
    };

    switch (pairPhase) {
    case PairPhase::APPROACH: {
        if (pairTalkParallelApproach) {
            if (!pairTalkMainArrived) {
                float waypointX = homeMainActor.targetX;
                float waypointY = homeMainActor.targetY;
                homeMainActor.route.current(waypointX, waypointY);
                petDirection = petDirectionForDelta(
                    waypointX - homeMainActor.x,
                    waypointY - homeMainActor.y);
            }
            if (!pairTalkCompanionArrived) {
                float waypointX = homeCompanionActor.targetX;
                float waypointY = homeCompanionActor.targetY;
                homeCompanionActor.route.current(waypointX, waypointY);
                companionDirection = petDirectionForDelta(
                    waypointX - homeCompanionActor.x,
                    waypointY - homeCompanionActor.y);
            }

            Home::RouteStep mainStep = Home::RouteStep::ARRIVED;
            Home::RouteStep companionStep = Home::RouteStep::ARRIVED;
            if (!pairTalkMainArrived) {
                mainStep = homeRuntime.advanceRoute(
                    0, nowMs, std::max(0.1f, pairTalkMainApproachSpeed),
                    elapsedSeconds, 1.0f, false);
            }
            if (!pairTalkCompanionArrived) {
                companionStep = homeRuntime.advanceRoute(
                    1, nowMs,
                    std::max(0.1f, pairTalkCompanionApproachSpeed),
                    elapsedSeconds, 1.0f, false);
            }
            syncMainPose();
#if STICKMON_ENABLE_DEBUG_FEATURES
            if (static_cast<int32_t>(nowMs - visitorDiagNextLogMs) >= 0) {
                visitorDiagNextLogMs = nowMs + 300;
                Platform::logf(
                    "[FriendDiag] talk approach-parallel kind=%u "
                    "main=%.1f,%.1f companion=%.1f,%.1f arrived=%u,%u\n",
                    static_cast<unsigned>(debugContactKind),
                    homeMainActor.x, homeMainActor.y,
                    homeCompanionActor.x, homeCompanionActor.y,
                    pairTalkMainArrived ? 1U : 0U,
                    pairTalkCompanionArrived ? 1U : 0U);
            }
#endif
            if (!pairTalkMainArrived &&
                static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                ++petFrame;
                nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            }
            if (!pairTalkCompanionArrived &&
                static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
                ++companionFrame;
                nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
            }
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);

            const bool phaseTimedOut =
                static_cast<int32_t>(nowMs - pairPhaseUntilMs) >= 0;
            auto resolveArrival = [&](Home::Actor& actor,
                                      Home::RouteStep step,
                                      bool& arrived,
                                      const char* role) {
                if (arrived) return true;
                const bool terminal = step == Home::RouteStep::ARRIVED ||
                    step == Home::RouteStep::BLOCKED ||
                    step == Home::RouteStep::NO_ROUTE || phaseTimedOut;
                if (!terminal) return true;
                const float goalDistance = std::hypot(
                    actor.targetX - actor.x, actor.targetY - actor.y);
                if (step == Home::RouteStep::ARRIVED ||
                    goalDistance <= PAIR_APPROACH_GOAL_TOLERANCE) {
                    actor.x = actor.targetX;
                    actor.y = actor.targetY;
                    actor.velocityX = 0.0f;
                    actor.velocityY = 0.0f;
                    actor.route.clear();
                    arrived = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
                    if (step != Home::RouteStep::ARRIVED) {
                        Platform::logf(
                            "[FriendDiag] talk approach-near-goal "
                            "kind=%u role=%s distance=%.2f timeout=%u\n",
                            static_cast<unsigned>(debugContactKind), role,
                            static_cast<double>(goalDistance),
                            phaseTimedOut ? 1U : 0U);
                    }
#endif
                    return true;
                }
#if STICKMON_ENABLE_DEBUG_FEATURES
                Platform::logf(
                    "[FriendDiag] talk approach-failed kind=%u role=%s "
                    "step=%d distance=%.2f timeout=%u\n",
                    static_cast<unsigned>(debugContactKind), role,
                    static_cast<int>(step),
                    static_cast<double>(goalDistance),
                    phaseTimedOut ? 1U : 0U);
#else
                (void)role;
#endif
                return false;
            };
            if (!resolveArrival(homeMainActor, mainStep,
                                pairTalkMainArrived, "main") ||
                !resolveArrival(homeCompanionActor, companionStep,
                                pairTalkCompanionArrived, "companion")) {
                finishPairInteraction(nowMs, false);
                return false;
            }
            syncMainPose();
            if (!pairTalkMainArrived || !pairTalkCompanionArrived) {
                return true;
            }

            faceActors();
#if STICKMON_ENABLE_DEBUG_FEATURES
            const int dxPx = std::abs(worldToScreenX(homeMainActor.x) -
                                      worldToScreenX(homeCompanionActor.x));
            const int mainWidth = pairIdleWidthPx(gameState.team[0].speciesId);
            const int companionWidth =
                pairIdleWidthPx(gameState.team[1].speciesId);
            const int visibleGapPx = dxPx -
                (mainWidth + companionWidth) / 2;
            if (visibleGapPx < PAIR_TALK_MIN_GAP_PX) {
                Platform::logf(
                    "[FriendDiag] talk configured-gap kind=%u gapXpx=%d "
                    "minimum=%d (fixed points kept)\n",
                    static_cast<unsigned>(debugContactKind), visibleGapPx,
                    PAIR_TALK_MIN_GAP_PX);
            }
            const int dy = std::abs(worldToScreenY(homeMainActor.y) -
                                    worldToScreenY(homeCompanionActor.y));
            Platform::logf("[FriendDiag] talk positioned kind=%u parallel=1 "
                           "dxPx=%d dyPx=%d widths=%d,%d gapXpx=%d "
                           "main=%.1f,%.1f companion=%.1f,%.1f\n",
                           static_cast<unsigned>(debugContactKind), dxPx, dy,
                           mainWidth, companionWidth, visibleGapPx,
                           homeMainActor.x, homeMainActor.y,
                           homeCompanionActor.x, homeCompanionActor.y);
            Platform::logf("[FriendDiag] arrival talk-ready kind=%u "
                           "mainDir=%u guestDir=%u\n",
                           static_cast<unsigned>(debugContactKind),
                           static_cast<unsigned>(petDirection),
                           static_cast<unsigned>(companionDirection));
#endif
            pairPhase = PairPhase::INVITE;
            pairPhaseStartedMs = nowMs;
            pairPhaseUntilMs = nowMs + PAIR_INVITE_MS;
            return true;
        }

        const uint8_t mover = pairLeaderMain ? 0 : 1;
        Home::Actor& movingActor = pairLeaderMain
            ? homeMainActor : homeCompanionActor;
        float waypointX = movingActor.targetX;
        float waypointY = movingActor.targetY;
        movingActor.route.current(waypointX, waypointY);
        if (mover == 0) {
            petDirection = petDirectionForDelta(
                waypointX - petX, waypointY - petY);
        } else {
            companionDirection = petDirectionForDelta(
                waypointX - homeCompanionActor.x,
                waypointY - homeCompanionActor.y);
        }
        Home::RouteStep step = homeRuntime.advanceRoute(
            mover, nowMs, PAIR_APPROACH_SPEED, elapsedSeconds, 1.0f, true);
        syncMainPose();
        // The mover walks during APPROACH too; without frame advancement the
        // sprite slides across the room in a static pose.
        if (mover == 0) {
            if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                petFrame = static_cast<uint8_t>(petFrame + 1);
                nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            }
        } else if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
            companionFrame = static_cast<uint8_t>(companionFrame + 1);
            nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
        }
        requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
        const bool phaseTimedOut =
            static_cast<int32_t>(nowMs - pairPhaseUntilMs) >= 0;
        const bool terminalStep = step == Home::RouteStep::ARRIVED ||
            step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE || phaseTimedOut;
        if (terminalStep) {
            const float goalDistance = std::hypot(
                movingActor.targetX - movingActor.x,
                movingActor.targetY - movingActor.y);
            bool reachedGoal = step == Home::RouteStep::ARRIVED;
            if (!reachedGoal &&
                goalDistance <= PAIR_APPROACH_GOAL_TOLERANCE) {
                // A frame can hit the deadline after movement reaches the
                // target but before advanceRoute reports ARRIVED. Snap only
                // within the normal arrival tolerance so the scripted talk
                // is not cancelled one frame before completion.
                movingActor.x = movingActor.targetX;
                movingActor.y = movingActor.targetY;
                movingActor.velocityX = 0.0f;
                movingActor.velocityY = 0.0f;
                movingActor.route.clear();
                syncMainPose();
                reachedGoal = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
                if (pairActivity == Home::PairActivity::TALK) {
                    Platform::logf(
                        "[FriendDiag] talk approach-near-goal kind=%u "
                        "distance=%.2f timeout=%u\n",
                        static_cast<unsigned>(debugContactKind),
                        static_cast<double>(goalDistance),
                        phaseTimedOut ? 1U : 0U);
                }
#endif
            }
            if (pairActivity == Home::PairActivity::TALK && !reachedGoal) {
#if STICKMON_ENABLE_DEBUG_FEATURES
                Platform::logf(
                    "[FriendDiag] talk approach-failed kind=%u step=%d "
                    "distance=%.2f timeout=%u\n",
                    static_cast<unsigned>(debugContactKind),
                    static_cast<int>(step),
                    static_cast<double>(goalDistance),
                    phaseTimedOut ? 1U : 0U);
#endif
                finishPairInteraction(nowMs, false);
                return false;
            }
            homeMainActor.route.clear();
            homeCompanionActor.route.clear();
#if STICKMON_ENABLE_DEBUG_FEATURES
            const int dxPx = std::abs(worldToScreenX(homeMainActor.x) -
                                      worldToScreenX(homeCompanionActor.x));
            const int visibleGapPx = dxPx -
                (pairIdleWidthPx(gameState.team[0].speciesId) +
                 pairIdleWidthPx(gameState.team[1].speciesId)) / 2;
            if (pairActivity == Home::PairActivity::TALK) {
                Platform::logf("[FriendDiag] talk approach kind=%u step=%d "
                               "gapXpx=%d dxPx=%d\n",
                               static_cast<unsigned>(debugContactKind),
                               static_cast<int>(step), visibleGapPx, dxPx);
            }
#endif
            faceActors();
#if STICKMON_ENABLE_DEBUG_FEATURES
            if (pairActivity == Home::PairActivity::TALK) {
                const int mainWidth = pairIdleWidthPx(gameState.team[0].speciesId);
                const int companionWidth = pairIdleWidthPx(gameState.team[1].speciesId);
                const int dy = std::abs(worldToScreenY(homeMainActor.y) -
                                        worldToScreenY(homeCompanionActor.y));
                Platform::logf("[FriendDiag] talk positioned kind=%u step=%d "
                               "dxPx=%d dyPx=%d widths=%d,%d gapXpx=%d "
                               "main=%.1f,%.1f companion=%.1f,%.1f\n",
                               static_cast<unsigned>(debugContactKind),
                               static_cast<int>(step), dxPx, dy,
                               mainWidth, companionWidth,
                               visibleGapPx,
                               homeMainActor.x, homeMainActor.y,
                               homeCompanionActor.x, homeCompanionActor.y);
                Platform::logf("[FriendDiag] arrival talk-ready kind=%u "
                               "mainDir=%u guestDir=%u\n",
                               static_cast<unsigned>(debugContactKind),
                               static_cast<unsigned>(petDirection),
                               static_cast<unsigned>(companionDirection));
            }
#endif
            pairPhase = PairPhase::INVITE;
            pairPhaseStartedMs = nowMs;
            pairPhaseUntilMs = nowMs + PAIR_INVITE_MS;
        }
        return true;
    }
    case PairPhase::INVITE:
        faceActors();
        if (static_cast<int32_t>(nowMs - pairPhaseUntilMs) >= 0) {
            pairPhase = PairPhase::ACTIVE;
            pairPhaseStartedMs = nowMs;
            if (pairActivity == Home::PairActivity::TALK) {
                pairPhaseUntilMs = nowMs + PAIR_TALK_TOTAL_MS;
#if STICKMON_ENABLE_DEBUG_FEATURES
                Platform::logf(
                    "[FriendDiag] talk active kind=%u durationMs=%u\n",
                    static_cast<unsigned>(debugContactKind),
                    static_cast<unsigned>(PAIR_TALK_TOTAL_MS));
#endif
            } else if (!planChaseLeg()) {
                pairActivity = Home::PairActivity::TALK;
                pairPhaseUntilMs = nowMs + PAIR_TALK_TOTAL_MS;
            }
        }
        return true;
    case PairPhase::ACTIVE:
        if (pairActivity == Home::PairActivity::TALK) {
            const uint32_t elapsed = nowMs - pairPhaseStartedMs;
            pairMainRenderOffsetY = elapsed < PAIR_TALK_HOP_MS
                ? -std::sin(static_cast<float>(elapsed) /
                            PAIR_TALK_HOP_MS * 3.14159265f) * 5.0f : 0.0f;
            const uint32_t companionStart =
                PAIR_TALK_HOP_MS + PAIR_TALK_GAP_MS;
            pairCompanionRenderOffsetY =
                elapsed >= companionStart &&
                elapsed < companionStart + PAIR_TALK_HOP_MS
                    ? -std::sin(static_cast<float>(elapsed - companionStart) /
                                PAIR_TALK_HOP_MS * 3.14159265f) * 5.0f
                    : 0.0f;
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            if (static_cast<int32_t>(nowMs - pairPhaseUntilMs) >= 0) {
                pairPhase = PairPhase::CELEBRATE;
                pairPhaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
                heartsUntil = pairPhaseUntilMs;
            }
            return true;
        } else {
            const Home::RouteStep mainStep = homeRuntime.advanceRoute(
                0, nowMs, pairLeaderMain ? PAIR_CHASE_LEADER_SPEED
                                  : PAIR_CHASE_FOLLOWER_SPEED,
                elapsedSeconds, 1.0f, false);
            const Home::RouteStep companionStep = homeRuntime.advanceRoute(
                1, nowMs, pairLeaderMain ? PAIR_CHASE_FOLLOWER_SPEED
                                  : PAIR_CHASE_LEADER_SPEED,
                elapsedSeconds, 1.0f, false);
            syncMainPose();
            petDirection = petDirectionForDelta(
                homeMainActor.velocityX, homeMainActor.velocityY);
            companionDirection = petDirectionForDelta(
                homeCompanionActor.velocityX, homeCompanionActor.velocityY);
            if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                petFrame = static_cast<uint8_t>(petFrame + 1);
                nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            }
            if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
                companionFrame = static_cast<uint8_t>(companionFrame + 1);
                nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
            }
            requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
            const bool mainDone = mainStep == Home::RouteStep::ARRIVED ||
                mainStep == Home::RouteStep::BLOCKED ||
                mainStep == Home::RouteStep::NO_ROUTE;
            const bool companionDone =
                companionStep == Home::RouteStep::ARRIVED ||
                companionStep == Home::RouteStep::BLOCKED ||
                companionStep == Home::RouteStep::NO_ROUTE;
            if (mainDone && companionDone) {
                if (pairChaseLegsRemaining > 0) --pairChaseLegsRemaining;
                if (pairChaseLegsRemaining == 0 || !planChaseLeg()) {
                    pairPhase = PairPhase::CELEBRATE;
                    pairPhaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
                    heartsUntil = pairPhaseUntilMs;
                    faceActors();
                }
            }
            if (static_cast<int32_t>(nowMs - pairInteractionUntilMs) >= 0) {
                pairPhase = PairPhase::CELEBRATE;
                pairPhaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
                heartsUntil = pairPhaseUntilMs;
            }
            return true;
        }
    case PairPhase::CELEBRATE:
        faceActors();
        pairMainRenderOffsetY = 0.0f;
        pairCompanionRenderOffsetY = 0.0f;
        if (static_cast<int32_t>(nowMs - pairPhaseUntilMs) >= 0) {
            finishPairInteraction(nowMs, true);
            return false;
        }
        return true;
    case PairPhase::NONE:
        break;
    }
    return false;
}

void AmoledApp::finishPairInteraction(uint32_t nowMs, bool reward) {
    if (pairPhase == PairPhase::NONE && !homeRuntime.pairActive()) return;
#if STICKMON_ENABLE_DEBUG_FEATURES
    Platform::logf("[HomeDiag] pair finish phase=%d reward=%d\n",
                   static_cast<int>(pairPhase), reward ? 1 : 0);
#endif
    homeMainActor.route.clear();
    homeCompanionActor.route.clear();
    homeRuntime.endPair(nowMs, 700);
    petX = homeMainActor.x;
    petY = homeMainActor.y;
    petTargetX = petX;
    petTargetY = petY;
    petMotion = PetMotion::IDLE;
    pairPhase = PairPhase::NONE;
    pairActivity = Home::PairActivity::NONE;
    pairArrivalTalk = false;
    pairTalkParallelApproach = false;
    pairTalkMainArrived = false;
    pairTalkCompanionArrived = false;
    pairTalkMainApproachSpeed = 0.0f;
    pairTalkCompanionApproachSpeed = 0.0f;
    pairChaseLegsRemaining = 0;
    pairMainRenderOffsetY = 0.0f;
    pairCompanionRenderOffsetY = 0.0f;
    monsterMind.onActivity(nowMs);
    homeCompanionActor.mind.onActivity(nowMs);
    schedulePetDecision(nowMs);
    schedulePairInteraction(nowMs);
    if (reward && gameState.pairMoodRewardsToday < 3) {
        for (uint8_t slot = 0; slot < 2; ++slot) {
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

void AmoledApp::cancelPairInteraction(uint32_t nowMs) {
    finishPairInteraction(nowMs, false);
    heartsUntil = 0;
}

void AmoledApp::beginVisitDeparture(uint32_t nowMs) {
    visitRecallConfirm = false;
    cancelPairInteraction(nowMs);
    cancelRoomAction(nowMs);
    homeRuntime.releaseAll(0);
    homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
    RoomResource& room = RoomResource::ins();
    visitDoorInsideX = room.available()
        ? static_cast<float>(room.doorwayInsideX())
        : FALLBACK_ROOM_MAX_X - 8.0f;
    visitDoorInsideY = room.available()
        ? static_cast<float>(room.doorwayInsideY())
        : FALLBACK_ROOM_MAX_Y;
    visitDoorOutsideX = room.available()
        ? static_cast<float>(room.doorwayOutsideX())
        : visitDoorInsideX + 12.0f;
    visitDoorOutsideY = room.available()
        ? static_cast<float>(room.doorwayOutsideY())
        : visitDoorInsideY + 10.0f;
    homeMainActor.hidden = false;
    homeMainActor.x = petX;
    homeMainActor.y = petY;
    petResting = false;
    petMotion = PetMotion::IDLE;
    homeMainActor.route.clear();
    if (!room.available() ||
        !homeRuntime.planRoute(0, visitDoorInsideX, visitDoorInsideY,
                               false, false)) {
        homeMainActor.targetX = visitDoorInsideX;
        homeMainActor.targetY = visitDoorInsideY;
    }
    visitDeparture = VisitDeparture::WALK_TO_DOOR;
    visitDepartureStartedMs = nowMs;
    nextPetFrameMs = nowMs;
    requestFullRender();
}

bool AmoledApp::updateVisitDeparture(uint32_t nowMs) {
    if (!visitSession.active()) {
        visitRecallConfirm = false;
        if (visitDeparture == VisitDeparture::AWAY ||
            visitDeparture == VisitDeparture::CROSS_DOOR) {
            petX = visitDoorInsideX;
            petY = visitDoorInsideY;
        }
        homeMainActor.x = petX;
        homeMainActor.y = petY;
        homeMainActor.hidden = false;
        homeRuntime.stop(0, nowMs, 0);
        petMotion = PetMotion::IDLE;
        visitDeparture = VisitDeparture::NONE;
        updateCamera();
        requestFullRender();
        return false;
    }
    if (visitDeparture == VisitDeparture::AWAY) return true;
    float elapsedSeconds = lastPetUpdateMs == 0 || nowMs < lastPetUpdateMs
        ? 0.0f
        : std::min<uint32_t>(nowMs - lastPetUpdateMs, 120U) / 1000.0f;
    const float previousY = petY;
    bool arrived = false;
    if (visitDeparture == VisitDeparture::WALK_TO_DOOR &&
        !homeMainActor.route.empty()) {
        const Home::RouteStep step = homeRuntime.advanceRoute(
            0, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, false);
        arrived = step == Home::RouteStep::ARRIVED;
        if (step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE) homeMainActor.route.clear();
    } else {
        const float targetX = visitDeparture == VisitDeparture::WALK_TO_DOOR
            ? visitDoorInsideX : visitDoorOutsideX;
        const float targetY = visitDeparture == VisitDeparture::WALK_TO_DOOR
            ? visitDoorInsideY : visitDoorOutsideY;
        const float distance = std::hypot(targetX - homeMainActor.x,
                                          targetY - homeMainActor.y);
        const float step = VISITOR_DOOR_SPEED * elapsedSeconds;
        arrived = distance <= std::max(0.8f, step);
        if (arrived) {
            homeMainActor.x = targetX;
            homeMainActor.y = targetY;
        } else if (step > 0.0f) {
            homeMainActor.x += (targetX - homeMainActor.x) / distance * step;
            homeMainActor.y += (targetY - homeMainActor.y) / distance * step;
        }
    }
    petDirection = petDirectionForDelta(homeMainActor.x - petX,
                                         homeMainActor.y - petY);
    petX = homeMainActor.x;
    petY = homeMainActor.y;
    petTargetX = homeMainActor.targetX;
    petTargetY = homeMainActor.targetY;
    if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
        ++petFrame;
        nextPetFrameMs = nowMs + MOTION_FRAME_MS;
    }
    updateCamera();
    requestHomeActorRows(previousY, petY);
    if (arrived || nowMs - visitDepartureStartedMs >
                       (visitDeparture == VisitDeparture::WALK_TO_DOOR
                            ? 12000U : 5000U)) {
        if (visitDeparture == VisitDeparture::WALK_TO_DOOR) {
            homeMainActor.x = petX = visitDoorInsideX;
            homeMainActor.y = petY = visitDoorInsideY;
            homeMainActor.route.clear();
            visitDeparture = VisitDeparture::CROSS_DOOR;
            visitDepartureStartedMs = nowMs;
        } else {
            homeMainActor.hidden = true;
            visitDeparture = VisitDeparture::AWAY;
            visitSession.markVisitorDeparted();
            requestFullRender();
        }
    }
    return true;
}

bool AmoledApp::chooseVisitorDoorPose(const Home::Actor& actor,
                                      float& x, float& y) const {
    RoomResource& room = RoomResource::ins();
    if (!room.available() || room.walkPolygonCount() < 3) return false;
    const float anchorX = static_cast<float>(room.doorwayInsideX());
    const float anchorY = static_cast<float>(room.doorwayInsideY());
    auto valid = [&](float candidateX, float candidateY) {
        return RoomMovementArea::containsFootprint(
            room.walkPolygon(), room.walkPolygonCount(),
            candidateX, candidateY + actor.geometry.groundOffsetY,
            actor.geometry.footprint);
    };
    auto tryCandidate = [&](int dx, int dy) {
        float candidateX = anchorX + static_cast<float>(dx);
        float candidateY = anchorY + static_cast<float>(dy) -
            actor.geometry.groundOffsetY;
        if (!valid(candidateX, candidateY)) return false;
        x = candidateX;
        y = candidateY;
        return true;
    };
    if (tryCandidate(0, 0)) return true;
    for (int radius = 2; radius <= 48; radius += 2) {
        for (int offset = -radius; offset <= radius; offset += 2) {
            if (tryCandidate(radius, offset) ||
                tryCandidate(-radius, offset) ||
                tryCandidate(offset, radius) ||
                tryCandidate(offset, -radius)) {
                return true;
            }
        }
    }
    return false;
}

bool AmoledApp::beginVisitorHostClear(uint32_t nowMs) {
    RoomResource& room = RoomResource::ins();
    const float centerX = room.available()
        ? (static_cast<float>(room.walkMinX()) +
           static_cast<float>(room.walkMaxX())) * 0.5f
        : (FALLBACK_ROOM_MIN_X + FALLBACK_ROOM_MAX_X) * 0.5f;
    const float centerY = room.available()
        ? (static_cast<float>(room.walkMinY()) +
           static_cast<float>(room.walkMaxY())) * 0.5f
        : (FALLBACK_ROOM_MIN_Y + FALLBACK_ROOM_MAX_Y) * 0.5f;
    static constexpr int8_t OFFSETS[][2] = {
        {0, 0}, {-18, 0}, {18, 0}, {0, -14}, {0, 14},
        {-18, -12}, {18, -12}, {-18, 12}, {18, 12},
        {-30, 0}, {30, 0}, {0, -24}, {0, 24},
    };
    for (const auto& offset : OFFSETS) {
        const float candidateX = centerX + offset[0];
        const float candidateY = centerY + offset[1] -
            homeMainActor.geometry.groundOffsetY;
        if (!RoomMovementArea::containsFootprint(
                room.walkPolygon(), room.walkPolygonCount(),
                candidateX,
                candidateY + homeMainActor.geometry.groundOffsetY,
                homeMainActor.geometry.footprint)) {
            continue;
        }
        const float dx = candidateX - visitorDoorInsideX;
        const float dy = candidateY - visitorDoorInsideY;
        if (dx * dx + dy * dy <
                VISITOR_DOOR_CLEAR_DISTANCE * VISITOR_DOOR_CLEAR_DISTANCE) {
            continue;
        }
        if (!homeRuntime.planRoute(0, candidateX, candidateY, false, false)) {
            continue;
        }
        if (!homeRuntime.transitionPreparedRoute(
                0, Home::Task::DOOR_ACTION, nowMs, 0, true)) {
            continue;
        }
        visitorHostDoorPhase = 2;
        visitorRouteBlockedSinceMs = 0;
        visitorMotionUntilMs = nowMs + VISITOR_MOTION_TIMEOUT_MS;
        Platform::logf("[VisitArrival] host_clear_begin host=%.1f,%.1f "
                       "goal=%.1f,%.1f route=%u\n",
                       petX, petY, candidateX, candidateY,
                       static_cast<unsigned>(homeMainActor.route.count));
        return true;
    }
    Platform::logf("[VisitArrival] host_clear_unavailable host=%.1f,%.1f "
                   "door=%.1f,%.1f walk=%u\n",
                   petX, petY, visitorDoorInsideX, visitorDoorInsideY,
                   petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
    return false;
}

void AmoledApp::beginVisitorEntry(uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR) {
        return;
    }
    cancelRoomAction(nowMs);
    RoomResource& room = RoomResource::ins();
    const float insideX = room.available()
        ? static_cast<float>(room.doorwayInsideX())
        : FALLBACK_ROOM_MAX_X - 8.0f;
    const float insideY = room.available()
        ? static_cast<float>(room.doorwayInsideY())
        : FALLBACK_ROOM_MAX_Y;
    visitorDoorInsideX = insideX;
    visitorDoorInsideY = insideY;
    visitorDoorOutsideX = room.available()
        ? static_cast<float>(room.doorwayOutsideX()) : insideX + 12.0f;
    visitorDoorOutsideY = room.available()
        ? static_cast<float>(room.doorwayOutsideY()) : insideY + 10.0f;
    cancelPairInteraction(nowMs);
    homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
    homeRuntime.acquire(Home::Resource::DOOR, 0,
                        Home::Task::DOOR_ACTION, nowMs);
    // The temporary team member already exists in game state, but keep its
    // actor offstage until the host has reached a clear waiting point by the
    // door. This mirrors Stick's host-to-door arrival phase.
    homeCompanionActor.hidden = true;
    homeCompanionActor.route.clear();
    visitorRouteBlockedSinceMs = 0;
    visitorWelcomeValid = false;
    float welcomeX = insideX;
    float welcomeY = insideY;
    if (!chooseVisitorDoorPose(homeMainActor, welcomeX, welcomeY)) {
        Platform::logf(
            "[VisitArrival] welcome pose unavailable inside=%.1f,%.1f\n",
            insideX, insideY);
        startVisitorDoorEntry(nowMs);
        return;
    }
    visitorDoorInsideX = welcomeX;
    visitorDoorInsideY = welcomeY;
    const bool hostStartsOutside = !petFootprintInsideWalkArea(petX, petY);
    if (hostStartsOutside) {
        const float originalX = petX;
        const float originalY = petY;
        RoomMovementArea::Footprint footprint = {
            petFootprintRadiusX, petFootprintRadiusY};
        bool recovered = false;
        for (int radius = 2; radius <= 48 && !recovered; radius += 2) {
            for (int offset = -radius; offset <= radius; offset += 2) {
                const float candidates[][2] = {
                    {originalX + radius, originalY + offset},
                    {originalX - radius, originalY + offset},
                    {originalX + offset, originalY + radius},
                    {originalX + offset, originalY - radius},
                };
                for (const auto& candidate : candidates) {
                    if (!RoomMovementArea::containsFootprint(
                            room.walkPolygon(), room.walkPolygonCount(),
                            candidate[0], candidate[1], footprint)) {
                        continue;
                    }
                    petX = candidate[0];
                    petY = candidate[1];
                    recovered = true;
                    break;
                }
                if (recovered) break;
            }
        }
        if (!recovered) {
            petX = welcomeX;
            petY = welcomeY;
        }
        homeMainActor.x = petX;
        homeMainActor.y = petY;
        homeMainActor.targetX = petX;
        homeMainActor.targetY = petY;
        homeMainActor.route.clear();
        petTargetX = petX;
        petTargetY = petY;
        Platform::logf("[VisitArrival] host_recover from=%.1f,%.1f "
                       "to=%.1f,%.1f nearby=%u walk=%u\n",
                       originalX, originalY, petX, petY,
                       recovered ? 1U : 0U,
                       petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
        requestFullRender();
    }
    const bool routePlanned = homeRuntime.planRoute(
        0, welcomeX, welcomeY, false, false);
    if (visitSession.active() && visitSession.viewModel().localIsHost) {
        Platform::logf("[VisitArrival] welcome route=%u host=%.1f,%.1f "
                       "goal=%.1f,%.1f startWalk=%u goalWalk=%u "
                       "routePoints=%u\n",
                       routePlanned ? 1U : 0U, homeMainActor.x,
                       homeMainActor.y, welcomeX, welcomeY,
                       petFootprintInsideWalkArea(petX, petY) ? 1U : 0U,
                       petFootprintInsideWalkArea(welcomeX, welcomeY) ? 1U : 0U,
                       static_cast<unsigned>(homeMainActor.route.count));
    }
    if (routePlanned) {
        // Runtime actor x/y and the renderer's petCenterX/petGroundY are
        // the same bottom-center anchor. Preserve the fixed goal so the
        // debug marker remains exact after the visitor is revealed.
        visitorWelcomeX = homeMainActor.targetX;
        visitorWelcomeY = homeMainActor.targetY;
        visitorWelcomeValid = true;
    }
    visitorHostDoorPhase = 0;
    visitorHostPauseUntilMs = 0;
    if (!routePlanned) {
        Platform::logf("[VisitArrival] host_to_door_fallback "
                       "from=%.1f,%.1f door=%.1f,%.1f\n",
                       petX, petY, welcomeX, welcomeY);
        homeMainActor.x = welcomeX;
        homeMainActor.y = welcomeY;
        homeMainActor.targetX = welcomeX;
        homeMainActor.targetY = welcomeY;
        petX = welcomeX;
        petY = welcomeY;
        petTargetX = welcomeX;
        petTargetY = welcomeY;
        homeRuntime.stop(0, nowMs, 0);
        visitorHostDoorPhase = 1;
        visitorHostPauseUntilMs = nowMs + VISITOR_DOOR_OPEN_PAUSE_MS;
        visitorMotion = VisitorMotion::HOST_APPROACH;
        visitorMotionUntilMs = visitorHostPauseUntilMs +
            VISITOR_MOTION_TIMEOUT_MS;
        visitorArrivalNextLogMs = nowMs;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[FriendDiag] arrival host-route-failed kind=%u "
                       "from=%.1f,%.1f door=%.1f,%.1f\n",
                       static_cast<unsigned>(debugContactKind),
                       homeMainActor.x, homeMainActor.y, insideX, insideY);
#endif
        return;
    }
    visitorMotion = VisitorMotion::HOST_APPROACH;
    visitorArrivalNextLogMs = nowMs;
    visitorCrossingDoor = false;
    const float distance = std::hypot(
        homeMainActor.targetX - homeMainActor.x,
        homeMainActor.targetY - homeMainActor.y);
    visitorMotionUntilMs = nowMs + static_cast<uint32_t>(std::clamp(
        distance / VISITOR_DOOR_SPEED * 1000.0f + 2500.0f,
        6000.0f, 15000.0f));
    petDirection = petDirectionForDelta(
        homeMainActor.targetX - homeMainActor.x,
        homeMainActor.targetY - homeMainActor.y);
    petFrame = 0;
    nextPetFrameMs = nowMs;
#if STICKMON_ENABLE_DEBUG_FEATURES
    visitorDiagNextLogMs = nowMs;
    Platform::logf("[FriendDiag] arrival host-approach kind=%u "
                   "from=%.1f,%.1f goal=%.1f,%.1f visitorHidden=1\n",
                   static_cast<unsigned>(debugContactKind),
                   homeMainActor.x, homeMainActor.y,
                   homeMainActor.targetX, homeMainActor.targetY);
#endif
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::startVisitorDoorEntry(uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR) {
        return;
    }
    float insideX = visitorDoorInsideX;
    float insideY = visitorDoorInsideY;
    const bool guestDoorPoseReady =
        chooseVisitorDoorPose(homeCompanionActor, insideX, insideY);
    visitorGuestDoorInsideX = insideX;
    visitorGuestDoorInsideY = insideY;
    const float outsideX = visitorDoorOutsideX;
    const float outsideY = visitorDoorOutsideY;

    homeRuntime.release(Home::Resource::DOOR, 0);
    homeRuntime.stop(0, nowMs, 700);
    petX = homeMainActor.x;
    petY = homeMainActor.y;
    petTargetX = petX;
    petTargetY = petY;
    petMotion = PetMotion::IDLE;

    homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
    homeRuntime.acquire(Home::Resource::DOOR, 1,
                        Home::Task::DOOR_ACTION, nowMs);
    homeCompanionActor.x = outsideX;
    homeCompanionActor.y = outsideY;
    homeCompanionActor.targetX = insideX;
    homeCompanionActor.targetY = insideY;
    homeCompanionActor.route.clear();
    homeCompanionActor.hidden = false;
    visitorMotion = VisitorMotion::ENTERING;
    visitorHostDoorPhase = 0;
    visitorCrossingDoor = true;
    visitorMotionUntilMs = nowMs + VISITOR_MOTION_TIMEOUT_MS;
    const float landingGoals[][2] = {
        {insideX + 12.0f, insideY - 12.0f},
        {insideX + 24.0f, insideY - 12.0f},
        {insideX + 12.0f, insideY - 24.0f},
    };
    visitorLandingValid = false;
    for (const auto& goal : landingGoals) {
        if (!companionFootprintInsideWalkArea(goal[0], goal[1])) continue;
        visitorLandingX = goal[0];
        visitorLandingY = goal[1];
        visitorLandingValid = true;
        break;
    }
    if (!visitorLandingValid) {
        visitorLandingValid = chooseCompanionTarget(
            visitorLandingX, visitorLandingY);
    }
    Platform::logf("[VisitArrival] guest_enter_begin host=%.1f,%.1f "
                   "hostWalk=%u guestDoor=%.1f,%.1f guestDoorValid=%u "
                   "landing=%.1f,%.1f landingValid=%u\n",
                   petX, petY, petFootprintInsideWalkArea(petX, petY) ? 1U : 0U,
                   insideX, insideY, guestDoorPoseReady ? 1U : 0U,
                   visitorLandingX, visitorLandingY,
                   visitorLandingValid ? 1U : 0U);
    companionDirection = petDirectionForDelta(
        insideX - outsideX, insideY - outsideY);
    companionFrame = 0;
    nextCompanionFrameMs = nowMs;
    if (visitSession.active() && visitSession.viewModel().localIsHost) {
        Platform::logf("[VisitArrival] visible species=%u outside=%.1f,%.1f inside=%.1f,%.1f\n",
                       static_cast<unsigned>(homeCompanionActor.speciesId),
                       outsideX, outsideY, insideX, insideY);
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    Platform::logf("[FriendDiag] arrival visitor-visible kind=%u "
                   "host=%.1f,%.1f from=%.1f,%.1f to=%.1f,%.1f "
                   "landing=%.1f,%.1f valid=%u\n",
                   static_cast<unsigned>(debugContactKind),
                   homeMainActor.x, homeMainActor.y,
                   outsideX, outsideY, insideX, insideY,
                   visitorLandingX, visitorLandingY,
                   visitorLandingValid ? 1U : 0U);
#endif
    requestFullRender();
}

void AmoledApp::beginVisitorExit(uint32_t nowMs, bool debugVisitor) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR ||
        visitorMotion == VisitorMotion::EXITING) {
        return;
    }
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    visitorExitIsDebug = debugVisitor;
    visitorMotion = VisitorMotion::EXITING;
    visitorCrossingDoor = false;
    homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
    homeRuntime.acquire(Home::Resource::DOOR, 1,
                        Home::Task::DOOR_ACTION, nowMs);
    RoomResource& room = RoomResource::ins();
    const float insideX = room.available()
        ? static_cast<float>(room.doorwayInsideX())
        : FALLBACK_ROOM_MAX_X - 8.0f;
    const float insideY = room.available()
        ? static_cast<float>(room.doorwayInsideY())
        : FALLBACK_ROOM_MAX_Y;
    const float fromX = homeCompanionActor.x;
    const float fromY = homeCompanionActor.y;
    const uint32_t travelBudgetMs = static_cast<uint32_t>(std::clamp(
        std::hypot(insideX - fromX, insideY - fromY) /
            VISITOR_DOOR_SPEED * 2200.0f + 3000.0f,
        static_cast<float>(VISITOR_MOTION_TIMEOUT_MS), 20000.0f));
    visitorMotionUntilMs = nowMs + travelBudgetMs;
    const float doorwayGoals[][2] = {
        {insideX, insideY}, {insideX + 12.0f, insideY - 12.0f},
        {insideX - 12.0f, insideY - 12.0f},
        {insideX + 24.0f, insideY - 12.0f},
        {insideX - 24.0f, insideY - 12.0f},
        {insideX, insideY - 24.0f},
        {insideX + 24.0f, insideY - 24.0f},
        {insideX - 24.0f, insideY - 24.0f},
    };
    bool routePlanned = false;
    for (const auto& goal : doorwayGoals) {
        if (homeRuntime.planRoute(1, goal[0], goal[1], false, true)) {
            routePlanned = true;
            break;
        }
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    visitorDiagNextLogMs = nowMs;
    Platform::logf("[FriendDiag] visitor-exit begin kind=%u debug=%u "
                   "from=%.1f,%.1f door=%.1f,%.1f route=%u goal=%.1f,%.1f "
                   "timeoutMs=%lu\n",
                   static_cast<unsigned>(debugContactKind),
                   debugVisitor ? 1U : 0U, fromX, fromY, insideX, insideY,
                   routePlanned ? 1U : 0U,
                   homeCompanionActor.targetX, homeCompanionActor.targetY,
                   static_cast<unsigned long>(travelBudgetMs));
#endif
    if (!routePlanned) {
        homeCompanionActor.route.clear();
        visitorCrossingDoor = true;
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[FriendDiag] visitor-exit route-fallback kind=%u "
                       "direct-distancePx=%.1f\n",
                       static_cast<unsigned>(debugContactKind),
                       std::hypot(insideX - fromX, insideY - fromY) *
                           AmoledUi::RESOURCE_SCALE);
#endif
    }
    companionFrame = 0;
    nextCompanionFrameMs = nowMs;
    requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
}

void AmoledApp::finishVisitorEntry(uint32_t nowMs, bool timedOut) {
    visitorMotion = VisitorMotion::ACTIVE;
    visitorHostDoorPhase = 0;
    visitorCrossingDoor = false;
    homeRuntime.release(Home::Resource::DOOR, 1);
    stopCompanion(nowMs, 700);
#if STICKMON_ENABLE_DEBUG_FEATURES
    showDebugContactEventPrompt(nowMs);
    Platform::logf("[FriendDiag] arrival visitor-entered kind=%u "
                   "pos=%.1f,%.1f timeout=%u\n",
                   static_cast<unsigned>(debugContactKind),
                   homeCompanionActor.x, homeCompanionActor.y,
                   timedOut ? 1U : 0U);
#else
    (void)timedOut;
#endif
    // Arrival conversation is a scripted beat. Start it before autonomous
    // hunger/sleep decisions can claim either actor; normal pair rules resume
    // after this one forced talk completes.
    pairArrivalTalk = true;
    const bool talkStarted = startPairInteraction(nowMs, false, true);
    Platform::logf("[VisitArrival] guest_enter_end host=%.1f,%.1f "
                   "guest=%.1f,%.1f hostWalk=%u guestWalk=%u "
                   "timeout=%u talkStarted=%u\n",
                   petX, petY, homeCompanionActor.x, homeCompanionActor.y,
                   petFootprintInsideWalkArea(petX, petY) ? 1U : 0U,
                   companionFootprintInsideWalkArea(
                       homeCompanionActor.x, homeCompanionActor.y) ? 1U : 0U,
                   timedOut ? 1U : 0U, talkStarted ? 1U : 0U);
    if (!talkStarted) {
        pairArrivalTalk = false;
        schedulePairInteraction(nowMs, true);
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[FriendDiag] arrival talk-start-failed kind=%u\n",
                       static_cast<unsigned>(debugContactKind));
#endif
    }
}

void AmoledApp::updateVisitorMotion(uint32_t nowMs,
                                    float elapsedSeconds) {
    if (visitorMotion != VisitorMotion::HOST_APPROACH &&
        visitorMotion != VisitorMotion::ENTERING &&
        visitorMotion != VisitorMotion::EXITING) {
        return;
    }
    if (visitorMotion == VisitorMotion::HOST_APPROACH) {
        if (visitorHostDoorPhase == 1) {
            if (static_cast<int32_t>(nowMs - visitorHostPauseUntilMs) < 0) {
                requestRenderRows(HOME_ROOM_TOP, HOME_STATUS_TOP);
                return;
            }
            Platform::logf("[VisitArrival] host_open_done pos=%.1f,%.1f "
                           "walk=%u\n", petX, petY,
                           petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
            if (!beginVisitorHostClear(nowMs)) {
                homeRuntime.stop(0, nowMs, 0);
                startVisitorDoorEntry(nowMs);
            }
            return;
        }
        if (visitorHostDoorPhase == 2) {
            if (elapsedSeconds <= 0.0f) return;
            const float previousY = homeMainActor.y;
            const Home::RouteStep clearStep = homeRuntime.advanceRoute(
                0, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, false);
            petX = homeMainActor.x;
            petY = homeMainActor.y;
            petTargetX = homeMainActor.targetX;
            petTargetY = homeMainActor.targetY;
            float waypointX = petTargetX;
            float waypointY = petTargetY;
            homeMainActor.route.current(waypointX, waypointY);
            petDirection = petDirectionForDelta(
                waypointX - petX, waypointY - petY);
            if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
                ++petFrame;
                nextPetFrameMs = nowMs + MOTION_FRAME_MS;
            }
            updateCamera();
            requestHomeActorRows(previousY, homeMainActor.y);
            if (static_cast<int32_t>(nowMs - visitorArrivalNextLogMs) >= 0) {
                visitorArrivalNextLogMs = nowMs + 600;
                Platform::logf("[VisitArrival] host_clear_step step=%u "
                               "pos=%.1f,%.1f goal=%.1f,%.1f walk=%u\n",
                               static_cast<unsigned>(clearStep), petX, petY,
                               petTargetX, petTargetY,
                               petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
            }
            if (clearStep == Home::RouteStep::ARRIVED ||
                clearStep == Home::RouteStep::BLOCKED ||
                clearStep == Home::RouteStep::NO_ROUTE ||
                static_cast<int32_t>(nowMs - visitorMotionUntilMs) >= 0) {
                if (clearStep != Home::RouteStep::ARRIVED &&
                    petFootprintInsideWalkArea(
                        homeMainActor.targetX, homeMainActor.targetY)) {
                    homeMainActor.x = petX = homeMainActor.targetX;
                    homeMainActor.y = petY = homeMainActor.targetY;
                }
                Platform::logf("[VisitArrival] host_clear_end step=%u "
                               "pos=%.1f,%.1f walk=%u\n",
                               static_cast<unsigned>(clearStep), petX, petY,
                               petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
                homeRuntime.stop(0, nowMs, 0);
                startVisitorDoorEntry(nowMs);
            }
            return;
        }
        // The companion actor can be attached during this same update tick.
        // A zero-duration first sample is not a routing failure and must not
        // reveal the visitor before the host has actually moved.
        if (elapsedSeconds <= 0.0f) return;
        const float previousY = homeMainActor.y;
        const Home::RouteStep step = homeRuntime.advanceRoute(
            0, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, false);
        petX = homeMainActor.x;
        petY = homeMainActor.y;
        petTargetX = homeMainActor.targetX;
        petTargetY = homeMainActor.targetY;
        float waypointX = petTargetX;
        float waypointY = petTargetY;
        homeMainActor.route.current(waypointX, waypointY);
        petDirection = petDirectionForDelta(
            waypointX - petX, waypointY - petY);
        if (static_cast<int32_t>(nowMs - nextPetFrameMs) >= 0) {
            ++petFrame;
            nextPetFrameMs = nowMs + MOTION_FRAME_MS;
        }
        updateCamera();
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (static_cast<int32_t>(nowMs - visitorDiagNextLogMs) >= 0) {
            visitorDiagNextLogMs = nowMs + 300;
            Platform::logf("[FriendDiag] arrival host-moving kind=%u "
                           "step=%d pos=%.1f,%.1f visitorHidden=%u\n",
                           static_cast<unsigned>(debugContactKind),
                           static_cast<int>(step), petX, petY,
                           homeCompanionActor.hidden ? 1U : 0U);
        }
#endif
        requestHomeActorRows(previousY, homeMainActor.y);
        if (static_cast<int32_t>(nowMs - visitorArrivalNextLogMs) >= 0) {
            visitorArrivalNextLogMs = nowMs + 600;
            Platform::logf("[VisitArrival] host_approach_step step=%u "
                           "pos=%.1f,%.1f goal=%.1f,%.1f walk=%u\n",
                           static_cast<unsigned>(step), petX, petY,
                           petTargetX, petTargetY,
                           petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
        }
        const bool timedOut =
            static_cast<int32_t>(nowMs - visitorMotionUntilMs) >= 0;
        if (step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE) {
            if (visitorRouteBlockedSinceMs == 0) {
                visitorRouteBlockedSinceMs = nowMs;
            }
        } else {
            visitorRouteBlockedSinceMs = 0;
        }
        const bool routeStalled = visitorRouteBlockedSinceMs != 0 &&
            nowMs - visitorRouteBlockedSinceMs >= 1200;
        if (step == Home::RouteStep::ARRIVED || timedOut || routeStalled) {
            if (step != Home::RouteStep::ARRIVED && visitorWelcomeValid) {
                homeMainActor.x = petX = visitorWelcomeX;
                homeMainActor.y = petY = visitorWelcomeY;
                homeMainActor.targetX = petTargetX = petX;
                homeMainActor.targetY = petTargetY = petY;
            }
            if (visitSession.active() && visitSession.viewModel().localIsHost) {
                Platform::logf("[VisitArrival] host_approach_end step=%d "
                               "timeout=%u stalled=%u pos=%.1f,%.1f walk=%u\n",
                               static_cast<int>(step), timedOut ? 1U : 0U,
                               routeStalled ? 1U : 0U, petX, petY,
                               petFootprintInsideWalkArea(petX, petY) ? 1U : 0U);
            }
#if STICKMON_ENABLE_DEBUG_FEATURES
            const float welcomeErrorPx = visitorWelcomeValid
                ? std::hypot(petX - visitorWelcomeX,
                             petY - visitorWelcomeY) *
                      AmoledUi::RESOURCE_SCALE
                : -1.0f;
            Platform::logf("[FriendDiag] arrival host-ready kind=%u "
                           "step=%d anchor=bottom-center pos=%.1f,%.1f "
                           "goal=%.1f,%.1f errorPx=%.2f timeout=%u\n",
                           static_cast<unsigned>(debugContactKind),
                           static_cast<int>(step), petX, petY,
                           visitorWelcomeX, visitorWelcomeY,
                           static_cast<double>(welcomeErrorPx),
                           timedOut ? 1U : 0U);
#endif
            visitorHostDoorPhase = 1;
            visitorHostPauseUntilMs = nowMs + VISITOR_DOOR_OPEN_PAUSE_MS;
            homeRuntime.stop(0, nowMs, 0);
        } else if (step == Home::RouteStep::BLOCKED ||
                   step == Home::RouteStep::NO_ROUTE) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            Platform::logf("[FriendDiag] arrival host-route-wait kind=%u "
                           "step=%d pos=%.1f,%.1f\n",
                           static_cast<unsigned>(debugContactKind),
                           static_cast<int>(step), petX, petY);
#endif
        }
        return;
    }
    if (static_cast<int32_t>(nowMs - visitorMotionUntilMs) >= 0) {
#if STICKMON_ENABLE_DEBUG_FEATURES
        Platform::logf("[FriendDiag] visitor-motion timeout kind=%u "
                       "motion=%d crossing=%u pos=%.1f,%.1f\n",
                       static_cast<unsigned>(debugContactKind),
                       static_cast<int>(visitorMotion),
                       visitorCrossingDoor ? 1U : 0U,
                       homeCompanionActor.x, homeCompanionActor.y);
#endif
        if (visitorMotion == VisitorMotion::ENTERING) {
            if (visitorLandingValid) {
                homeCompanionActor.x = visitorLandingX;
                homeCompanionActor.y = visitorLandingY;
                homeCompanionActor.targetX = visitorLandingX;
                homeCompanionActor.targetY = visitorLandingY;
            }
            finishVisitorEntry(nowMs, true);
        } else {
            finishVisitorExit(nowMs);
        }
        return;
    }

    RoomResource& room = RoomResource::ins();
    const float insideX = visitorMotion == VisitorMotion::ENTERING
        ? visitorGuestDoorInsideX
        : (room.available() ? static_cast<float>(room.doorwayInsideX())
                            : FALLBACK_ROOM_MAX_X - 8.0f);
    const float insideY = visitorMotion == VisitorMotion::ENTERING
        ? visitorGuestDoorInsideY
        : (room.available() ? static_cast<float>(room.doorwayInsideY())
                            : FALLBACK_ROOM_MAX_Y);
    const float outsideX = room.available()
        ? static_cast<float>(room.doorwayOutsideX()) : insideX + 12.0f;
    const float outsideY = room.available()
        ? static_cast<float>(room.doorwayOutsideY()) : insideY + 10.0f;

    if (visitorMotion == VisitorMotion::EXITING && !visitorCrossingDoor) {
        const float previousX = homeCompanionActor.x;
        const float previousY = homeCompanionActor.y;
        const Home::RouteStep step = homeRuntime.advanceRoute(
            1, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, true);
        const float movedX = homeCompanionActor.x - previousX;
        const float movedY = homeCompanionActor.y - previousY;
        const bool moved = std::hypot(movedX, movedY) > 0.001f;
        if (moved) {
            companionDirection = petDirectionForDelta(movedX, movedY);
            if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
                ++companionFrame;
                nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
            }
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (static_cast<int32_t>(nowMs - visitorDiagNextLogMs) >= 0) {
            visitorDiagNextLogMs = nowMs + 300;
            Platform::logf("[FriendDiag] visitor-exit route kind=%u "
                           "step=%d pos=%.1f,%.1f distancePx=%.1f "
                           "frame=%u dir=%u moved=%u\n",
                           static_cast<unsigned>(debugContactKind),
                           static_cast<int>(step), homeCompanionActor.x,
                           homeCompanionActor.y,
                           std::hypot(insideX - homeCompanionActor.x,
                                      insideY - homeCompanionActor.y) *
                               AmoledUi::RESOURCE_SCALE,
                           static_cast<unsigned>(companionFrame),
                           static_cast<unsigned>(companionDirection),
                           moved ? 1U : 0U);
        }
#endif
        if (step == Home::RouteStep::ARRIVED ||
            step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            Platform::logf("[FriendDiag] visitor-exit route-end kind=%u "
                           "step=%d doorDistancePx=%.1f\n",
                           static_cast<unsigned>(debugContactKind),
                           static_cast<int>(step),
                           std::hypot(insideX - homeCompanionActor.x,
                                      insideY - homeCompanionActor.y) *
                               AmoledUi::RESOURCE_SCALE);
#endif
            homeCompanionActor.route.clear();
            visitorCrossingDoor = true;
            companionDirection = petDirectionForDelta(
                outsideX - homeCompanionActor.x,
                outsideY - homeCompanionActor.y);
        }
        requestHomeActorRows(previousY, homeCompanionActor.y);
        return;
    }

    const float targetX = visitorMotion == VisitorMotion::ENTERING
        ? (visitorCrossingDoor || !visitorLandingValid
               ? insideX : visitorLandingX)
        : outsideX;
    const float targetY = visitorMotion == VisitorMotion::ENTERING
        ? (visitorCrossingDoor || !visitorLandingValid
               ? insideY : visitorLandingY)
        : outsideY;
    const float dx = targetX - homeCompanionActor.x;
    const float dy = targetY - homeCompanionActor.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float previousY = homeCompanionActor.y;
    const float step = VISITOR_DOOR_SPEED * elapsedSeconds;
    if (distance <= std::max(1.0f, step)) {
        homeCompanionActor.x = targetX;
        homeCompanionActor.y = targetY;
        if (visitorMotion == VisitorMotion::ENTERING) {
            if (visitorCrossingDoor && visitorLandingValid) {
                // Doorway crossed; continue to the walkable landing spot.
                visitorCrossingDoor = false;
                companionDirection = petDirectionForDelta(
                    visitorLandingX - insideX, visitorLandingY - insideY);
            } else {
                finishVisitorEntry(nowMs, false);
            }
        } else {
            finishVisitorExit(nowMs);
        }
    } else if (distance > 0.0f) {
        homeCompanionActor.x += dx / distance * step;
        homeCompanionActor.y += dy / distance * step;
        companionDirection = petDirectionForDelta(dx, dy);
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (visitorMotion == VisitorMotion::EXITING &&
            static_cast<int32_t>(nowMs - visitorDiagNextLogMs) >= 0) {
            visitorDiagNextLogMs = nowMs + 300;
            Platform::logf(
                "[FriendDiag] visitor-exit cross kind=%u pos=%.1f,%.1f "
                "remainingPx=%.1f frame=%u dir=%u\n",
                static_cast<unsigned>(debugContactKind),
                homeCompanionActor.x, homeCompanionActor.y,
                std::hypot(targetX - homeCompanionActor.x,
                           targetY - homeCompanionActor.y) *
                    AmoledUi::RESOURCE_SCALE,
                static_cast<unsigned>(companionFrame),
                static_cast<unsigned>(companionDirection));
        }
#endif
    }
    if (static_cast<int32_t>(nowMs - nextCompanionFrameMs) >= 0) {
        ++companionFrame;
        nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;
    }
    if (visitorMotion == VisitorMotion::ENTERING &&
        static_cast<int32_t>(nowMs - visitorArrivalNextLogMs) >= 0) {
        visitorArrivalNextLogMs = nowMs + 600;
        Platform::logf("[VisitArrival] guest_enter_step pos=%.1f,%.1f "
                       "goal=%.1f,%.1f crossing=%u hidden=%u frame=%u\n",
                       homeCompanionActor.x, homeCompanionActor.y,
                       targetX, targetY, visitorCrossingDoor ? 1U : 0U,
                       homeCompanionActor.hidden ? 1U : 0U,
                       static_cast<unsigned>(companionFrame));
    }
    requestHomeActorRows(previousY, homeCompanionActor.y);
}

void AmoledApp::finishVisitorExit(uint32_t nowMs) {
    const bool debugVisitor = visitorExitIsDebug;
#if STICKMON_ENABLE_DEBUG_FEATURES
    Platform::logf("[FriendDiag] visitor-exit finish kind=%u "
                   "pos=%.1f,%.1f debug=%u\n",
                   static_cast<unsigned>(debugContactKind),
                   homeCompanionActor.x, homeCompanionActor.y,
                   debugVisitor ? 1U : 0U);
#endif
    visitorMotion = VisitorMotion::NONE;
    visitorExitIsDebug = false;
    visitorCrossingDoor = false;
    visitorMotionUntilMs = 0;
    homeRuntime.releaseAll(1);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (debugVisitor) {
        finalizeDebugContact(nowMs);
        return;
    }
#else
    (void)debugVisitor;
#endif
    if (gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR) {
        gameState.team[1] = Game::MonsterRuntime{};
        gameState.teamCount = 1;
        gameState.activeSlot = 0;
    }
    homeCompanionActor = Home::Actor{};
    homeRuntime.attach(homeMainActor);
    configureHomeRuntime();
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    speciesIds[0] = gameState.team[0].speciesId;
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    schedulePairInteraction(nowMs);
    requestFullRender();
}

void AmoledApp::requestVisitEnd(uint32_t nowMs) {
    Game::MonsterRuntime guest{};
    const bool hasGuest = gameState.teamCount > 1 &&
        gameState.team[1].origin == Game::Origin::VISITOR;
    if (hasGuest) guest = gameState.team[1];
    visitSession.endVisit();
    if (hasGuest && gameState.teamCount < 2) {
        gameState.team[1] = guest;
        gameState.teamCount = 2;
        gameState.activeSlot = 0;
    }
    if (hasGuest) beginVisitorExit(nowMs, false);
    sceneFlow.goHome();
    requestFullRender();
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

bool AmoledApp::companionFootprintInsideWalkArea(float x, float y) const {
    RoomResource& room = RoomResource::ins();
    const RoomResource::Point* polygon = room.available()
        ? room.walkPolygon() : FALLBACK_WALK_POLYGON;
    uint8_t count = room.available()
        ? room.walkPolygonCount()
        : static_cast<uint8_t>(sizeof(FALLBACK_WALK_POLYGON) /
                               sizeof(FALLBACK_WALK_POLYGON[0]));
    return RoomMovementArea::containsFootprint(
        polygon, count, x, y, homeCompanionActor.geometry.footprint);
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

bool AmoledApp::chooseWakeTarget(float& x, float& y) const {
    RoomResource& room = RoomResource::ins();
    const float bedX = room.available() ? static_cast<float>(room.bedX()) : 76.0f;
    const float bedY = room.available() ? static_cast<float>(room.bedY()) : 99.0f;
    const float originX = petX;
    const float originY = petY;

    // Prefer a nearby point so waking does not look like a teleport, while
    // requiring enough distance to visibly clear the bed footprint.
    for (uint8_t attempt = 0; attempt < 48; ++attempt) {
        float candidateX = 0.0f;
        float candidateY = 0.0f;
        if (!chooseWanderTarget(candidateX, candidateY, true)) continue;
        const float bedDx = candidateX - bedX;
        const float bedDy = candidateY - bedY;
        if (bedDx * bedDx + bedDy * bedDy < 18.0f * 18.0f) continue;
        if (!petPathInsideWalkArea(originX, originY, candidateX, candidateY)) {
            continue;
        }
        x = candidateX;
        y = candidateY;
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
        static_cast<float>(room.width() - HOME_ROOM_WIDTH / AmoledUi::RESOURCE_SCALE));
    float minimumCameraY = static_cast<float>(room.roomY());
    float maximumCameraY = std::max<float>(minimumCameraY,
        static_cast<float>(room.roomY() + room.height() - HOME_ROOM_HEIGHT / AmoledUi::RESOURCE_SCALE));
    // The room cache is sampled at integer logical pixels. Keep the camera
    // on that same grid so the room, sprites, bowl, and dirty-region decision
    // use one coordinate system instead of mixing float and rounded values.
    cameraX = std::clamp(std::round(cameraX), 0.0f, maximumCameraX);
    cameraY = std::clamp(std::round(cameraY), minimumCameraY, maximumCameraY);
}

int AmoledApp::worldToScreenX(float worldX) const {
    if (!RoomResource::ins().available()) {
        return static_cast<int>(std::lround((worldX) * AmoledUi::RESOURCE_SCALE));
    }
    return static_cast<int>(std::lround((worldX - cameraX) * AmoledUi::RESOURCE_SCALE));
}

int AmoledApp::worldToScreenY(float worldY) const {
    if (!RoomResource::ins().available()) {
        return static_cast<int>(std::lround((worldY) * AmoledUi::RESOURCE_SCALE));
    }
    return HOME_ROOM_TOP +
           static_cast<int>(std::lround((worldY - cameraY) * AmoledUi::RESOURCE_SCALE));
}

void AmoledApp::requestRenderRows(uint16_t begin, uint16_t end) {
    begin = std::min<uint16_t>(begin, AmoledUi::HEIGHT);
    end = std::clamp<uint16_t>(end, begin, AmoledUi::HEIGHT);
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
    requestRenderRows(0, 448);
}

void AmoledApp::markRendered() {
    if (dirtyRowsValid && dirtyRowBegin == 0 && dirtyRowEnd == AmoledUi::HEIGHT) {
        expeditionFade.markPresented(Platform::clock().millis());
    }
    dirtyRowsValid = false;
    if (battleAnimationActive && battleAudioPending && !battleAudioReady &&
        sceneFlow.current() == AppSceneFlow::Scene::BATTLE) {
        battleAudioReady = true;
    }
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void AmoledApp::markEncounterFirstFramePresented(uint32_t drawUs,
                                                 uint32_t lcdUs) {
    if (!encounterFirstFramePending) return;
    encounterFirstFramePending = false;
    if (sceneFlow.current() != AppSceneFlow::Scene::BATTLE) return;
    Platform::logf(
        "[EncounterPerf] triggerToFirstBattleFrame=%lu ms prep=%lu ms "
        "sprite=%lu ms sfx=%lu ms draw=%lu us lcd=%lu us\n",
        static_cast<unsigned long>(
            Platform::clock().millis() - encounterStartedMs),
        static_cast<unsigned long>(encounterPreparedMs),
        static_cast<unsigned long>(encounterSpritePrepMs),
        static_cast<unsigned long>(encounterSfxPrepMs),
        static_cast<unsigned long>(drawUs),
        static_cast<unsigned long>(lcdUs));
}
#endif

float AmoledApp::gameSpeed() const {
    static constexpr float SPEEDS[] = {1.0f, 2.0f, 4.0f, 8.0f};
    uint8_t index = gameState.settings.speedIndex;
    return SPEEDS[index < 4 ? index : 0];
}

void AmoledApp::render(Canvas565& canvas) const {
    renderCaches_.retainForScene(sceneFlow.current());
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME) {
        const Home::RenderSnapshot homeSnapshot =
            homeRuntime.renderSnapshot();
        const Game::MonsterRuntime& monster = gameState.team[0];
        HomeViewModel model;
        model.speciesId = monster.speciesId;
        model.visitAway = visitDeparture == VisitDeparture::AWAY;
        model.recallConfirm = visitRecallConfirm && model.visitAway;
        uint8_t visibleSlots[Game::TEAM_CAP] = {};
        model.monsterCount = Game::HomeHud::visibleTeamSlots(
            gameState, visibleSlots, visitSession.active());
        for (uint8_t index = 0; index < model.monsterCount; ++index) {
            const Game::MonsterRuntime& hudMonster =
                gameState.team[visibleSlots[index]];
            model.monsters[index].hp =
                Game::HomeHud::hpPercent(hudMonster);
            model.monsters[index].fainted = hudMonster.fainted;
            model.monsters[index].faintRest =
                Game::HomeHud::faintRestPercent(
                    hudMonster, gameState.gameMinutesTotal);
            model.monsters[index].hunger =
                Game::HomeHud::hungerPercent(hudMonster);
            model.monsters[index].hpKnown =
                hudMonster.origin != Game::Origin::VISITOR ||
                !visitSession.active() || visitSession.visitorHealthKnown();
        }
        model.gameMinutesOfDay = static_cast<uint16_t>(
            gameState.gameMinutesTotal % Game::GAME_MINUTES_PER_DAY);
        model.cameraX = static_cast<int16_t>(std::lround(cameraX));
        model.cameraY = static_cast<int16_t>(std::lround(cameraY));
        model.petCenterX = static_cast<int16_t>(worldToScreenX(
            homeSnapshot.actors[0].x));
        model.petGroundY = static_cast<int16_t>(worldToScreenY(
            homeSnapshot.actors[0].y));
        model.petVisible = homeSnapshot.actors[0].active &&
            !homeSnapshot.actors[0].hidden && !expeditionMainHidden;
        model.petRenderOffsetY = static_cast<int8_t>(std::lround(
            pairMainRenderOffsetY * AmoledUi::RESOURCE_SCALE));
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
        // The main pet stays in PetMotion::IDLE while a pair interaction
        // moves it (approach leader or chase); present that as walking so
        // the sprite does not slide in its idle pose.
        const bool mainPairMoving =
            (pairPhase == PairPhase::APPROACH &&
             (pairTalkParallelApproach
                  ? !pairTalkMainArrived : pairLeaderMain)) ||
            (pairPhase == PairPhase::ACTIVE &&
             pairActivity == Home::PairActivity::CHASE);
        const bool mainWelcomingVisitor =
            visitorMotion == VisitorMotion::HOST_APPROACH;
        model.petAction = petMotion == PetMotion::STOPPING
            ? HomeViewModel::PetVisualAction::STOPPING
            : (petMotion == PetMotion::WANDERING ||
               petMotion == PetMotion::SEEKING_FOOD ||
               petMotion == PetMotion::SEEKING_SLEEP ||
               mainPairMoving || mainWelcomingVisitor ||
               visitDeparture == VisitDeparture::WALK_TO_DOOR ||
               visitDeparture == VisitDeparture::CROSS_DOOR)
                ? HomeViewModel::PetVisualAction::WALKING
                : HomeViewModel::PetVisualAction::IDLE;
        model.petResting = petResting;
        if (gameState.teamCount > 1 && gameState.team[1].speciesId != 0 &&
            homeSnapshot.actors[1].active &&
            !homeSnapshot.actors[1].hidden &&
            !expeditionCompanionHidden
#if STICKMON_ENABLE_DEBUG_FEATURES
            && !(debugPairChaseActive && gameState.teamCount > 1)
#endif
        ) {
            model.companionVisible = true;
            model.companionSpeciesId = gameState.team[1].speciesId;
            model.companionCenterX = static_cast<int16_t>(
                worldToScreenX(homeSnapshot.actors[1].x));
            model.companionGroundY = static_cast<int16_t>(
                worldToScreenY(homeSnapshot.actors[1].y));
            model.companionRenderOffsetY = static_cast<int8_t>(std::lround(
                pairCompanionRenderOffsetY * AmoledUi::RESOURCE_SCALE));
            model.companionFrame = companionFrame;
            model.companionDirection = companionDirection;
            model.companionAction =
                (homeSnapshot.actors[1].task == Home::Task::WANDER ||
                 homeSnapshot.actors[1].task == Home::Task::SEEK_FOOD ||
                 homeSnapshot.actors[1].task == Home::Task::SEEK_SLEEP ||
                 homeSnapshot.actors[1].task == Home::Task::YIELDING ||
                 homeSnapshot.actors[1].task == Home::Task::DOOR_ACTION ||
                 (homeSnapshot.actors[1].task == Home::Task::PAIR_ACTION &&
                  !homeCompanionActor.route.empty()))
                    ? HomeViewModel::PetVisualAction::WALKING
                    : HomeViewModel::PetVisualAction::IDLE;
            model.companionLongMove = companionLongMove;
            const Game::MonsterRuntime& companion = gameState.team[1];
            model.companionResting = companion.fainted ||
                companion.hpCur == 0 ||
                companion.majorStatus == Game::MajorStatus::SLEEP ||
                homeCompanionActor.task == Home::Task::SLEEPING ||
                homeCompanionActor.task == Home::Task::FAINTED;
        }
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
        model.fadeAlpha = externalSceneFade ? 0 : expeditionFade.alpha();
#if STICKMON_ENABLE_DEBUG_FEATURES
        model.debugContactPrompt = debugContactPending ||
            debugContactEventPending;
        model.debugContactPromptFade = debugContactPromptFade;
        model.debugContactPromptText =
            debugContactPromptFade > 0 ? debugPromptBuffer : nullptr;
        model.debugContactChoiceVisible = debugContactChoiceVisible;
        model.debugContactSelectedChoice = debugContactSelectedChoice;
        // Contact visitors now use the same independent secondary actor.
        model.debugContactActive = false;
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
        model.debugTalkPointsVisible = debugTalkPointsVisible;
        float debugTalkCompanionX = PAIR_TALK_COMPANION_X;
        float debugTalkCompanionY = PAIR_TALK_STAGE_Y;
        float debugTalkMainX = PAIR_TALK_MAIN_X;
        float debugTalkMainY = PAIR_TALK_STAGE_Y;
        if (pairActivity == Home::PairActivity::TALK &&
            pairPhase != PairPhase::NONE) {
            debugTalkMainX = homeMainActor.targetX;
            debugTalkMainY = homeMainActor.targetY;
            debugTalkCompanionX = homeCompanionActor.targetX;
            debugTalkCompanionY = homeCompanionActor.targetY;
        }
        model.debugTalkCompanionX = static_cast<int16_t>(
            std::lround(debugTalkCompanionX));
        model.debugTalkCompanionY = static_cast<int16_t>(
            std::lround(debugTalkCompanionY));
        model.debugTalkMainX = static_cast<int16_t>(
            std::lround(debugTalkMainX));
        model.debugTalkMainY = static_cast<int16_t>(
            std::lround(debugTalkMainY));
        RoomResource& debugRoom = RoomResource::ins();
        float welcomeX = debugRoom.available()
            ? static_cast<float>(debugRoom.doorwayInsideX()) +
                  VISITOR_WELCOME_OFFSET_X
            : FALLBACK_ROOM_MAX_X - 8.0f + VISITOR_WELCOME_OFFSET_X;
        float welcomeY = debugRoom.available()
            ? static_cast<float>(debugRoom.doorwayInsideY()) +
                  VISITOR_WELCOME_OFFSET_Y
            : FALLBACK_ROOM_MAX_Y + VISITOR_WELCOME_OFFSET_Y;
        if (visitorWelcomeValid) {
            welcomeX = visitorWelcomeX;
            welcomeY = visitorWelcomeY;
        }
        model.debugWelcomeCenterX = static_cast<int16_t>(
            worldToScreenX(welcomeX));
        model.debugWelcomeGroundY = static_cast<int16_t>(
            worldToScreenY(welcomeY));
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
        model.talkPointsVisible = debugTalkPointsVisible;
        model.battleBoundsVisible = debugBattleDrawBoundsVisible;
            model.touchDisplayEnabled = debugTouchDisplayEnabled;
        model.touchTest = &debugTouchTest;
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
        renderExploreScreen(canvas, model, renderCaches_.exploreBackground,
                            dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE ||
        sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_MENU) {
        ExploreRouteViewModel model;
        model.map = &exploreRouteMap;
        model.state = &gameState;
        const uint8_t leaderSlot = exploreRouteLeaderSlot(gameState);
        model.speciesId = gameState.team[leaderSlot].speciesId;
        model.area = selectedExploreArea;
        model.pathIndex = exploreRoutePath;
        model.routeIndex = exploreRouteIndex;
        if (exploreRoutePath < exploreRouteMap.pathCount) {
            model.routePointCount =
                exploreRouteMap.paths[exploreRoutePath].pointCount;
        }
        model.walkDirection = exploreRouteDirection;
        model.petFrame = exploreRoutePetFrame;
        model.mapFrame = exploreRouteMapFrame;
        model.animationNowMs = Platform::clock().millis();
        model.steps = exploreRouteSteps;
        model.worldX = exploreRouteWorldX;
        model.worldY = exploreRouteWorldY;
        model.companionVisible = exploreRouteHasCompanion(gameState);
        model.companionSpeciesId = model.companionVisible
            ? gameState.team[1].speciesId : 0;
        model.companionWorldX = exploreRouteFollowerWorldX;
        model.companionWorldY = exploreRouteFollowerWorldY;
        model.companionWalkDirection = exploreRouteFollowerDirection;
        model.companionFrame = exploreRouteFollowerFrame;
        model.companionWalking = exploreRouteFollowerMoving;
        model.cameraX = exploreRouteCameraX;
        model.cameraY = exploreRouteCameraY;
        model.walking = exploreRouteMoving && !exploreRoutePaused;
        model.autoWalk = exploreRouteAutoWalk && !exploreRoutePaused;
        model.sliding = exploreRouteIceSliding;
        model.complete = exploreRouteComplete;
        model.bossPending = exploreRouteBossPending;
        model.bossIndex = exploreRouteBossIndex;
        model.bossSpeciesId = exploreRouteBossSpeciesId;
        model.fadeAlpha = externalSceneFade ? 0 : expeditionFade.alpha();
        model.exitConfirm = exploreRouteExitConfirm;
        model.pickupIndex = exploreRoutePickupIndex;
        model.pickupItem = exploreRoutePickupItem;
        model.pickupAvailable = exploreRoutePickupAvailable;
        model.toast = toast;
        if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_ROUTE &&
            exploreRouteSteps == 0 && dirtyRowBegin == 0 &&
            dirtyRowEnd == AmoledUi::HEIGHT) {
            // The departure fade is black here. Build the static battle layer
            // before the route redraws the same full framebuffer.
            prepareBattleBackground(canvas, renderCaches_.battleBackground,
                                    battleBackgroundForArea(selectedExploreArea));
        }
        renderExploreRouteScreen(
            canvas, model, renderCaches_.exploreWorld, dirtyRowBegin, dirtyRowEnd);
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
        const uint32_t battleRenderNowMs = Platform::clock().millis();
        model.playerHp = battleHpPercentForRender(
            false, active.hpCur, active.hpMax, battleRenderNowMs);
        model.wildHp = battleHpPercentForRender(
            true, battleWild.hpCur, battleWild.hpMax, battleRenderNowMs);
        model.showPlayerExperience = battleExperienceVisible;
        if (battleExperienceVisible) {
            if (const Species* activeSpecies = findSpecies(active.speciesId)) {
                const uint32_t shownExp = battleExperienceForRender(
                    battleRenderNowMs);
                model.playerLevel = levelForExp(
                    activeSpecies->growthRate, shownExp);
                const uint32_t levelFloor = minimumExpForLevel(
                    activeSpecies->growthRate, model.playerLevel);
                const uint32_t levelCeiling =
                    model.playerLevel >= Game::LEVEL_MAX
                        ? levelFloor
                        : minimumExpForLevel(
                              activeSpecies->growthRate,
                              static_cast<uint8_t>(model.playerLevel + 1));
                model.playerExperience = model.playerLevel >= Game::LEVEL_MAX
                    ? 100
                    : static_cast<uint8_t>(std::min<uint32_t>(
                          100, (shownExp - levelFloor) * 100U /
                                   std::max<uint32_t>(
                                       1, levelCeiling - levelFloor)));
            }
        }
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
        if (battleSwitchStage != BattleSwitchStage::NONE) {
            const uint32_t elapsed = std::min<uint32_t>(
                BATTLE_SWITCH_PHASE_MS,
                battleRenderNowMs - battleSwitchStartedMs);
            const uint32_t progress = battleSwitchStage ==
                BattleSwitchStage::RETREATING ? elapsed
                : BATTLE_SWITCH_PHASE_MS - elapsed;
            model.playerSwitchOffsetX = static_cast<int16_t>(
                -static_cast<int32_t>(BATTLE_SWITCH_TRAVEL_X * progress * progress /
                                      (BATTLE_SWITCH_PHASE_MS * BATTLE_SWITCH_PHASE_MS)));
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        model.debugDrawBounds = debugBattleDrawBoundsVisible;
#endif
        model.playerStatus = active.majorStatus;
        model.wildStatus = battleWild.majorStatus;
        model.playerBattleState = battlePlayerState;
        model.wildBattleState = battleWildState;
        model.logCount = battleLogVisibleCount;
        for (uint8_t line = 0; line < model.logCount && line < 2; ++line) {
            model.logLines[line] = battleLogLines[line];
        }
        renderBattleScreen(canvas, model, renderCaches_.battleBackground,
                           dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::COMMUNICATION) {
        renderCommunicationScreen(canvas, visitSession.viewModel(),
                                  dirtyRowBegin, dirtyRowEnd);
        return;
    }

    if (sceneFlow.current() == AppSceneFlow::Scene::TEAM) {
        if (teamStatusOpen) {
            TeamStatusViewModel model;
            model.state = &gameState;
            model.teamSlot = teamStatusSlot;
            model.page = teamStatusPage;
            model.slideOffsetX = teamStatusSlideX;
            renderTeamStatusScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
        if (teamMovesOpen) {
            TeamMovesViewModel model;
            model.state = &gameState;
            model.teamSlot = teamMovesSlot;
            model.mode = teamMovesMode;
            model.selectedItem = teamMovesMode == TeamMovesViewModel::Mode::MANAGE
                ? teamMovesSelectedItem
                : teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT
                    ? teamMovesRecallSelected : 0xFF;
            model.scrollOffsetY = teamMovesScroll;
            model.recallCount = teamMovesRecallCount;
            for (uint8_t index = 0; index < teamMovesRecallCount; ++index) {
                model.recallIds[index] = teamMovesRecallIds[index];
            }
            model.recallSelected = teamMovesRecallSelected;
            model.detailProgress = teamMovesDetailProgress;
            model.toast = toast;
            renderTeamMovesScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
        TeamViewModel model;
        model.state = &gameState;
        model.pressedSlot = pressedTeamSlot;
        model.confirmOpen = teamConfirmOpen;
        model.actionPopupOpen = teamActionPopupOpen;
        model.actionPopupSlot = teamActionPopupSlot;
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
        if (computerPage == ComputerViewModel::Page::STATUS) {
            TeamStatusViewModel model;
            model.state = &gameState;
            model.teamSlot = teamStatusFromContact
                ? 0 : teamStatusSlot;
            model.monster = teamStatusFromContact
                ? contactMonster(teamStatusContactSlot) : nullptr;
            model.page = teamStatusPage;
            model.slideOffsetX = teamStatusSlideX;
            renderTeamStatusScreen(canvas, model, dirtyRowBegin, dirtyRowEnd);
            return;
        }
        ComputerViewModel model;
        model.state = &gameState;
        model.page = computerPage;
        model.storageScroll = computerScroll;
        model.pressedItem = computerPressedItem == 0xFF
            ? -1 : computerPressedItem;
        model.contactActionOpen = contactActionOpen;
        model.contactActionSlot = contactActionSlot;
        for (uint8_t slot = 0;
             slot < gameState.storageCount && slot < Game::STORAGE_CAP;
             ++slot) {
            if (contactIsVisiting(slot)) {
                model.contactVisitingSlot = slot;
                break;
            }
        }
        model.contactActionPressedItem = contactActionPressedItem;
        model.contactConfirmOpen = contactConfirmOpen;
        model.contactConfirmYes = contactConfirmYes;
        model.contactConfirmDelete = contactConfirmDelete;
        model.contactInTeam = contactIsInTeam(contactActionSlot);
        model.contactVisiting = contactIsVisiting(contactActionSlot);
        model.contactInviteLocked = contactInviteLocked(contactActionSlot);
        model.contactCanDelete = contactCanDelete(contactActionSlot);
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
        model.oldLevel = progressionOldLevel;
        model.level = progressionLevel;
        model.fromSpeciesId = progressionFromSpeciesId;
        model.toSpeciesId = progressionToSpeciesId;
        const uint32_t progressionNowMs = Platform::clock().millis();
        model.levelUpElapsedMs = progressionNowMs - progressionLevelUpStartedMs;
        model.evolutionPhase = progressionEvolution.phase(progressionNowMs);
        model.evolutionElapsedMs = progressionEvolution.cancelling()
            ? progressionEvolution.cancellationElapsed(progressionNowMs)
            : progressionEvolution.elapsed(progressionNowMs);
        model.evolutionReady =
            progressionEvolution.animationComplete(progressionNowMs);
        model.evolutionCancelHoldProgress = progressionEvolutionTouchHeld
            ? static_cast<uint8_t>(std::max<uint32_t>(
                  1, std::min<uint32_t>(
                         99, (progressionNowMs -
                              progressionEvolutionTouchStartedMs) * 100U /
                                 EVOLUTION_CANCEL_HOLD_MS)))
            : 0;
        model.moveId = progressionMoveId;
        model.oldMove2 = progressionOldMove2;
        model.oldMove3 = progressionOldMove3;
        model.pressedItem = progressionPressedItem;
        model.selectedItem = progressionSelectedItem;
        model.scrollOffsetY = progressionScroll;
        model.detailProgress = progressionDetailProgress;
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
        model.rinseProgress = showerRinseProgress;
        model.foam = showerFoam;
        model.foamCount = SHOWER_FOAM_CAP;
        model.atmosphereAlpha = static_cast<uint8_t>(std::clamp(
            showerAtmosphereAlpha, 0.0f, 255.0f));
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
        model.exploreOnly = false;
        model.battleMode = battleBagMode;
        model.dailyItemCount =
            Game::ItemInventory::homeBagDailyItemCount(gameState);
        model.exploreItemCount =
            Game::ItemInventory::homeBagExploreItemCount(gameState);
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

bool AmoledApp::displayLockAllowed() const {
    return !pendingExpedition &&
           (visitDeparture == VisitDeparture::NONE ||
            visitDeparture == VisitDeparture::AWAY) &&
           expeditionDeparturePhase == ExpeditionDeparturePhase::NONE &&
           !expeditionFade.active();
}

void AmoledApp::onWake(uint32_t nowMs) {
    lockRequested = false;
    pointerDown = false;
    lastInteractionMs = nowMs;
    requestFullRender();
}

bool AmoledApp::lockFocusPoint(int16_t& x, int16_t& y) const {
    x = AmoledUi::WIDTH / 2;
    y = AmoledUi::HEIGHT / 2;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME &&
        gameState.teamCount > 0) {
        x = static_cast<int16_t>(worldToScreenX(petX));
        y = static_cast<int16_t>(worldToScreenY(petY) - 64);
        return true;
    }
    return false;
}

bool AmoledApp::petIsSleeping() const {
    if (sceneFlow.current() != AppSceneFlow::Scene::HOME ||
        gameState.teamCount == 0) {
        return false;
    }

    const Game::MonsterRuntime& monster = gameState.team[0];
    if (monster.fainted || monster.hpCur == 0) return petResting;

    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(monster.speciesId);
    return care.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, monster.nature);
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
            ? HOME_STATUS_TOP : 448);
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

void AmoledApp::acceptDebugContact(uint32_t nowMs, bool autoResolve) {
    if (!debugContactPending ||
        debugContactStorageSlot >= gameState.storageCount ||
        gameState.teamCount != 1) {
        debugContactPending = false;
        debugContactEventPending = false;
        debugContactEventWaitingForEntry = false;
        debugContactChoiceVisible = false;
        debugContactSelectedChoice = -1;
        debugContactChoiceConfirmUntilMs = 0;
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
    debugContactEventPending = false;
    debugContactEventWaitingForEntry = !autoResolve;
    debugContactChoiceVisible = debugContactSelectedChoice >= 0;
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
    Platform::logf("[FriendDiag] accepted kind=%u slot=%u species=%u team=%u\n",
                   static_cast<unsigned>(debugContactKind),
                   static_cast<unsigned>(storageSlot),
                   static_cast<unsigned>(guest.speciesId),
                   static_cast<unsigned>(gameState.teamCount));
    if (autoResolve && debugContactKind == 3) {
        uint8_t unlocked = ExploreItemProgression::unlockedArea(gameState);
        selectedExploreArea = std::min<uint8_t>(selectedExploreArea, unlocked);
        if (!queueExploreDeparture(selectedExploreArea, false)) {
            Platform::logLine("[FriendDiag] departure rejected kind=3");
            completeDebugContact(nowMs);
            setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            return;
        }
        Platform::logf("[FriendDiag] departure queued kind=3 area=%u\n",
                       static_cast<unsigned>(selectedExploreArea));
    }
    if (autoResolve) showDebugContactStripMessage(debugToastBuffer, nowMs, 2000);
    requestFullRender();
}

void AmoledApp::showDebugContactEventPrompt(uint32_t nowMs) {
    if (!debugContactActive || !debugContactEventWaitingForEntry) return;
    debugContactEventWaitingForEntry = false;
    debugContactStartedMs = nowMs;
    if (debugContactKind == 3) {
        debugContactEventPending = true;
        debugContactChoiceVisible = true;
        debugContactSelectedChoice = -1;
        debugContactChoiceConfirmUntilMs = 0;
        debugContactMessageUntilMs = 0;
        const Species* species = findSpecies(gameState.team[1].speciesId);
        const char* name = species && species->name ? species->name : "";
        // The strip prompt wraps after the visitor name (EXPLORE_FMT = "%s" +
        // EXPLORE_SUFFIX).
        std::snprintf(debugPromptBuffer, sizeof(debugPromptBuffer), "%s\n%s",
                      name, Ui::ContactVisit::EXPLORE_SUFFIX);
    } else {
        debugContactEventPending = false;
        debugContactChoiceVisible = false;
        showDebugContactStripMessage(debugToastBuffer, nowMs, 2000);
    }
    requestFullRender();
}

void AmoledApp::showDebugContactStripMessage(const char* text,
                                             uint32_t nowMs,
                                             uint32_t durationMs) {
    if (!text) return;
    debugContactChoiceVisible = false;
    debugContactSelectedChoice = -1;
    debugContactChoiceConfirmUntilMs = 0;
    std::snprintf(debugPromptBuffer, sizeof(debugPromptBuffer), "%s", text);
    debugContactMessageUntilMs = nowMs + durationMs;
    requestFullRender();
}

void AmoledApp::completeDebugContact(uint32_t nowMs) {
    if (!debugContactActive) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::HOME &&
        homeCompanionActor.active) {
        if (visitorMotion != VisitorMotion::EXITING) {
            beginVisitorExit(nowMs, true);
        }
        return;
    }
    finalizeDebugContact(nowMs);
}

void AmoledApp::finalizeDebugContact(uint32_t nowMs) {
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
    debugContactEventPending = false;
    debugContactEventWaitingForEntry = false;
    debugContactChoiceVisible = false;
    debugContactSelectedChoice = -1;
    debugContactChoiceConfirmUntilMs = 0;
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
    Platform::logf("[FriendDiag] complete kind=%u team=%u slot=%u scene=%u phase=%u\n",
                   static_cast<unsigned>(kind),
                   static_cast<unsigned>(gameState.teamCount),
                   static_cast<unsigned>(visitorSlot),
                   static_cast<unsigned>(sceneFlow.current()),
                   static_cast<unsigned>(expeditionDeparturePhase));
    showDebugContactStripMessage(kind == 3 ? Ui::ContactVisit::HAPPY_RETURN
                                           : Ui::ContactVisit::HAPPY_VISIT,
                                 nowMs, 2000);
    requestFullRender();
}

void AmoledApp::startDebugPairChase(uint32_t nowMs) {
    if (gameState.teamCount < 2 || gameState.team[0].fainted ||
        gameState.team[1].fainted || gameState.team[0].hpCur == 0 ||
        gameState.team[1].hpCur == 0) {
        setToast(Ui::Debug::PAIR_NEEDS_TWO, nowMs, 1400);
        return;
    }
    sceneFlow.goHome();
    syncHomeActors(nowMs);
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
    if (!startPairInteraction(nowMs, true)) {
        setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
        return;
    }
    debugPairChaseActive = false;
    setToast(Ui::Debug::PAIR_INTERACTION, nowMs, 750);
    requestFullRender();
}

void AmoledApp::updateDebugPairChase(uint32_t nowMs,
                                     float elapsedSeconds) {
    (void)nowMs;
    (void)elapsedSeconds;
}

void AmoledApp::stopDebugPairChase(uint32_t nowMs, bool reward) {
    debugPairChaseActive = false;
    debugPairChaseUntilMs = 0;
    finishPairInteraction(nowMs, reward);
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
    requestRenderRows(0, 448);
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
    requestRenderRows(0, 448);
}

void AmoledApp::persistDebugMotionFlags() {
    uint8_t flags = 0;
    if (debugTiltControl) flags |= Game::DEBUG_MOTION_TILT;
    if (debugWalkBoundaryVisible) flags |= Game::DEBUG_MOTION_WALK_BOUNDARY;
    if (debugTalkPointsVisible) flags |= Game::DEBUG_MOTION_TALK_POINTS;
    if (gameState.debugMotionFlags == flags) return;
    gameState.debugMotionFlags = flags;
    saveState();
}

void AmoledApp::handleDebugPopupTap(int x, int y, uint32_t nowMs) {
    uint8_t digitCount = debugPopup == DebugViewModel::Popup::SET_TIME ? 4 : 3;
    int digit = debugPopupDigitAt(x, y, digitCount);
    if (digit >= 0) {
        debugFocus = static_cast<uint8_t>(digit);
        debugDigits[debugFocus] = static_cast<uint8_t>(
            (debugDigits[debugFocus] + 1) % 10);
        requestRenderRows(0, 448);
        return;
    }
    int choice = debugPopupChoiceAt(x, y);
    if (choice < 0) return;
    if (choice == 1) {
        debugPopup = DebugViewModel::Popup::NONE;
        requestRenderRows(0, 448);
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
    requestRenderRows(0, 448);
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
        case 6:
            debugTouchDisplayEnabled = !debugTouchDisplayEnabled;
            if (!debugTouchDisplayEnabled) debugTouchPointValid = false;
            requestFullRender();
            return;
        case 7:
            debugCategory = DebugViewModel::Category::TOUCH_TEST;
            debugTouchTest = {};
            debugVelocity = 0;
            pointerDown = false;
            Platform::logLine("[TouchTest] begin native=368x448 diagnostic_only=1");
            requestFullRender();
            return;
        default:
            // The Debug page is entered as a standalone scene, so there is
            // no menu stack entry for closeMenu() to pop here.
            sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);
            requestFullRender();
            return;
        }
        debugCursor = 0;
        debugScroll = 0.0f;
        requestFullRender();
        return;
    }

    const uint8_t backIndex = debugCategory == DebugViewModel::Category::TOUCH_TEST ? 0
        : debugCategory == DebugViewModel::Category::MONSTER ? 3
        : debugCategory == DebugViewModel::Category::RESOURCE ? 1
        : debugCategory == DebugViewModel::Category::ENV ? 2
        : debugCategory == DebugViewModel::Category::MOTION ? 4
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
    case DebugViewModel::Category::TOUCH_TEST:
        return;
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
                persistDebugMotionFlags();
            } else {
                float ax = 0.0f;
                float ay = 0.0f;
                float az = 0.0f;
                if (Platform::imu().readAcceleration(ax, ay, az)) {
                    debugTiltControl = true;
                    persistDebugMotionFlags();
                    stopDebugPairChase(nowMs, false);
                    sceneFlow.goHome();
                    requestFullRender();
                    return;
                }
                setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
            }
        }
        else if (debugCursor == 1) {
            debugWalkBoundaryVisible = !debugWalkBoundaryVisible;
            persistDebugMotionFlags();
        }
        else if (debugCursor == 2) {
            debugTalkPointsVisible = !debugTalkPointsVisible;
            persistDebugMotionFlags();
        } else if (debugCursor == 3) {
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
    case DebugViewModel::Category::CONTACT_EVENT:
        beginDebugContactEvent(static_cast<uint8_t>(debugCursor + 1), nowMs);
        return;
    case DebugViewModel::Category::ROOT:
    default: break;
    }
    requestRenderRows(0, 448);
}

bool AmoledApp::beginDebugContactEvent(uint8_t kind, uint32_t nowMs) {
    if (kind < 1 || kind > 3) return false;
    if (gameState.teamCount != 1) {
        setToast(Ui::Social::HOST_TEAM_REQUIRED, nowMs, 1400);
        Platform::logf("[FriendDiag] rejected kind=%u reason=team team=%u\n",
                       static_cast<unsigned>(kind),
                       static_cast<unsigned>(gameState.teamCount));
        return false;
    }
    if (debugContactPending || debugContactEventPending || debugContactActive) {
        setToast(Ui::Debug::EVENT_BUSY, nowMs, 1400);
        Platform::logf("[FriendDiag] rejected kind=%u reason=busy\n",
                       static_cast<unsigned>(kind));
        return false;
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
        Platform::logf("[FriendDiag] rejected kind=%u reason=no-contact\n",
                       static_cast<unsigned>(kind));
        return false;
    }
    debugContactStorageSlot = selected;
    debugContactKind = kind;
    debugContactPending = true;
    debugContactEventPending = false;
    debugContactEventWaitingForEntry = false;
    debugContactChoiceVisible = true;
    debugContactSelectedChoice = -1;
    debugContactChoiceConfirmUntilMs = 0;
    debugContactMessageUntilMs = 0;
    gameState.storage[selected].lastSeenAt =
        static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    saveState();
    sceneFlow.enter(AppSceneFlow::Scene::HOME);
    std::snprintf(debugPromptBuffer, sizeof(debugPromptBuffer), "%s",
                  Ui::ContactVisit::KNOCK_LINES);
    Platform::logf("[FriendDiag] begin kind=%u slot=%u species=%u bond=%u\n",
                   static_cast<unsigned>(kind), static_cast<unsigned>(selected),
                   static_cast<unsigned>(gameState.storage[selected].speciesId),
                   static_cast<unsigned>(gameState.storage[selected].bond));
    requestFullRender();
    return true;
}

void AmoledApp::renderDebugTouchOverlay(Canvas565& canvas) const {
    if (expeditionFade.active()) return;
    if (sceneFlow.current() == AppSceneFlow::Scene::DEBUG &&
        debugCategory == DebugViewModel::Category::TOUCH_TEST) return;
    if (!debugTouchDisplayEnabled || !debugTouchPointValid) return;

    canvas.clearClipRect();
    constexpr int markerRadius = 10;
    const uint16_t shadow = PixelRenderer::rgb(12, 18, 25);
    const uint16_t accent = PixelRenderer::rgb(248, 210, 105);
    const uint16_t ink = PixelRenderer::rgb(255, 255, 255);

    canvas.fillCircle(debugTouchX, debugTouchY, 4, shadow);
    canvas.drawCircle(debugTouchX, debugTouchY, markerRadius, accent);
    canvas.drawFastHLine(debugTouchX - markerRadius - 4, debugTouchY,
                         markerRadius * 2 + 9, ink);
    canvas.drawFastVLine(debugTouchX, debugTouchY - markerRadius - 4,
                         markerRadius * 2 + 9, ink);

    char coordinates[20] = {};
    std::snprintf(coordinates, sizeof(coordinates), "%d,%d",
                  debugTouchX, debugTouchY);
    constexpr int labelWidth = 120;
    constexpr int labelHeight = 36;
    int labelX = debugTouchX + 14;
    if (labelX + labelWidth > AmoledUi::WIDTH) {
        labelX = debugTouchX - labelWidth - 14;
    }
    labelX = std::clamp(labelX, 0, AmoledUi::WIDTH - labelWidth);
    int labelY = debugTouchY + 14;
    if (labelY + labelHeight > AmoledUi::HEIGHT) {
        labelY = debugTouchY - labelHeight - 14;
    }
    labelY = std::clamp(labelY, 0, AmoledUi::HEIGHT - labelHeight);
    canvas.fillRoundRect(labelX, labelY, labelWidth, labelHeight, 5, shadow);
    canvas.drawRoundRect(labelX, labelY, labelWidth, labelHeight, 5, accent);
    PixelRenderer::text(canvas, labelX + 4, labelY + 2, coordinates, ink);
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

void AmoledApp::handleDebugTouchTest(const TouchEvent& event) {
    const TouchTest::Point point{event.x, event.y};
    if (event.type == TouchEventType::DOWN) {
        debugTouchTest.down(point);
    } else if (event.type == TouchEventType::MOVE) {
        debugTouchTest.move(point);
    } else {
        const auto result = debugTouchTest.up(point);
        if (result == TouchTest::Result::BACK) {
            debugCategory = DebugViewModel::Category::ROOT;
            debugCursor = 0;
            debugScroll = debugVelocity = 0;
            pointerDown = dragging = false;
            Platform::logLine("[TouchTest] exit");
        } else if (result == TouchTest::Result::CLEAR) {
            Platform::logLine("[TouchTest] cleared");
        } else if (result == TouchTest::Result::SAMPLE) {
            const int index = debugTouchTest.count - 1;
            const auto target = TouchTest::TARGETS[index];
            const auto& sample = debugTouchTest.samples[index];
            for (int phase = 0; phase < 2; ++phase) {
                const auto actual = phase ? sample.up : sample.down;
                const int dx = actual.x - target.x, dy = actual.y - target.y;
                Platform::logf("[TouchTest] t=%lu target=%d event=%s expected=%d,%d actual=%d,%d dx=%d dy=%d error=%.1f\n",
                    static_cast<unsigned long>(event.timestampMs), index + 1,
                    phase ? "UP" : "DOWN", target.x, target.y, actual.x, actual.y,
                    dx, dy, std::sqrt(float(dx * dx + dy * dy)));
            }
            Platform::logf("[TouchTest] target=%d range_x=%d..%d range_y=%d..%d\n", index + 1,
                sample.minimum.x, sample.maximum.x, sample.minimum.y, sample.maximum.y);
            float dx, dy, maximum;
            debugTouchTest.statistics(dx, dy, maximum);
            Platform::logf("[TouchTest] completed=%d/9 down_mean_dx=%.1f down_mean_dy=%.1f down_max_error=%.1f\n",
                          debugTouchTest.count, dx, dy, maximum);
        }
    }
    requestFullRender();
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
    const uint8_t dailyCount =
        Game::ItemInventory::homeBagDailyItemCount(gameState);
    float maximum = itemListMaxScroll(
        dailyCount,
        Game::ItemInventory::homeBagExploreItemCount(gameState),
        currentItemCount(), false);
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
    selectingItemTarget = false;
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
    selectingItemTarget = false;
    pressedShopDetailAction = -1;
    shopDetailProgress = 0.0f;
    shopDetailItemIndex = -1;
    teamConfirmOpen = false;
    pressedTeamSlot = -1;
    toast = nullptr;
    AppSceneFlow::Scene destination = sceneFlow.closeSubScene();
    battleBagMode = false;
    if (destination == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        resumeExploreRoute(Platform::clock().millis());
    }
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
    contactActionOpen = false;
    contactActionSlot = 0xFF;
    contactActionPressedItem = -1;
    contactConfirmOpen = false;
    contactConfirmYes = false;
    contactConfirmDelete = false;
    teamStatusFromContact = false;
    teamStatusContactSlot = 0xFF;
    teamStatusPage = 0;
    teamStatusSlideX = 0;
    teamStatusAnimating = false;
    teamStatusDragging = false;
    clawLogView = false;
    clawLogScroll = 0.0f;
    clawLogVelocity = 0.0f;
    clawLogPinned = true;
    clawLogCount = 0;
    toast = nullptr;
    requestFullRender();
}

const Game::MonsterRuntime* AmoledApp::contactMonster(uint8_t slot) const {
    return ContactRoster::liveMonsterForContact(gameState, slot);
}

bool AmoledApp::contactIsInTeam(uint8_t slot) const {
    return ContactRoster::teamSlotForContact(gameState, slot) >= 0;
}

bool AmoledApp::contactIsVisiting(uint8_t slot) const {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if ((debugContactActive || debugContactPending || debugContactEventPending) &&
        debugContactStorageSlot == slot) return true;
#endif
    const int8_t teamSlot = ContactRoster::teamSlotForContact(gameState, slot);
    return teamSlot >= 0 &&
           gameState.team[teamSlot].origin == Game::Origin::VISITOR;
}

bool AmoledApp::contactInviteLocked(uint8_t slot) const {
    const Game::MonsterRuntime* monster = contactMonster(slot);
    return monster && Game::Bond::inviteLockedToday(
        monster->petCountToday,
        Game::Bond::invitationDay(gameState.gameMinutesTotal));
}

bool AmoledApp::contactCanDelete(uint8_t slot) const {
    return ContactRoster::canDelete(gameState, slot, contactIsVisiting(slot));
}

uint8_t AmoledApp::contactActionCount(uint8_t slot) const {
    if (!contactMonster(slot)) return 0;
    return static_cast<uint8_t>(1 + (contactIsInTeam(slot) ? 0 : 1) +
                               (contactCanDelete(slot) ? 1 : 0));
}

void AmoledApp::openContactAction(uint8_t slot) {
    if (!contactMonster(slot)) return;
    contactActionOpen = true;
    contactActionSlot = slot;
    contactActionPressedItem = -1;
    contactConfirmOpen = false;
    contactConfirmYes = false;
    contactConfirmDelete = false;
    computerPressedItem = 0xFF;
    requestFullRender();
}

void AmoledApp::openContactStatus(uint8_t slot) {
    if (!contactMonster(slot)) return;
    contactActionOpen = false;
    contactActionPressedItem = -1;
    contactConfirmOpen = false;
    teamStatusFromContact = true;
    teamStatusContactSlot = slot;
    teamStatusPage = 0;
    teamStatusSlideX = 0;
    teamStatusAnimating = false;
    teamStatusDragging = false;
    computerPage = ComputerViewModel::Page::STATUS;
    computerPressedItem = 0xFF;
    requestFullRender();
}

void AmoledApp::closeContactOverlay() {
    contactActionOpen = false;
    contactConfirmOpen = false;
    contactActionPressedItem = -1;
    contactConfirmYes = false;
    contactConfirmDelete = false;
    requestFullRender();
}

void AmoledApp::handleContactActionTap(int x, int y, uint32_t nowMs) {
    const uint8_t actionCount = contactActionCount(contactActionSlot);
    const int action = computerContactActionItemAt(
        x, y, actionCount, contactActionSlot, computerScroll);
    if (action < 0) {
        if (!computerContactMenuAt(
                x, y, actionCount, contactActionSlot, computerScroll)) {
            closeContactOverlay();
        }
        return;
    }
    contactActionPressedItem = action;
    int cursor = 0;
    if (action == cursor) {
        openContactStatus(contactActionSlot);
        return;
    }
    ++cursor;
    if (!contactIsInTeam(contactActionSlot) && action == cursor) {
        if (contactInviteLocked(contactActionSlot)) {
            setToast(Ui::Storage::INVITE_LOCKED, nowMs);
            closeContactOverlay();
        } else if (contactIsVisiting(contactActionSlot)) {
            setToast(Ui::Storage::VISITING, nowMs);
            closeContactOverlay();
        } else if (gameState.teamCount >= Game::TEAM_CAP) {
            setToast(Ui::Storage::TEAM_FULL_TOAST, nowMs);
            closeContactOverlay();
        } else {
            contactConfirmOpen = true;
            contactConfirmYes = true;
            contactConfirmDelete = false;
            requestFullRender();
        }
        return;
    }
    if (!contactIsInTeam(contactActionSlot)) ++cursor;
    if (contactCanDelete(contactActionSlot) && action == cursor) {
        contactConfirmOpen = true;
        contactConfirmYes = true;
        contactConfirmDelete = true;
        requestFullRender();
        return;
    }
    closeContactOverlay();
}

void AmoledApp::handleContactConfirmTap(int x, int y, uint32_t nowMs) {
    const int choice = computerContactConfirmChoiceAt(x, y);
    if (choice < 0) {
        closeContactOverlay();
        return;
    }
    if (choice == 1) {
        closeContactOverlay();
        return;
    }
    if (contactConfirmDelete) {
        if (ContactRoster::deleteContact(gameState, contactActionSlot,
                                         contactIsVisiting(contactActionSlot))) {
#if STICKMON_ENABLE_DEBUG_FEATURES
            if ((debugContactActive || debugContactPending || debugContactEventPending) &&
                contactActionSlot < debugContactStorageSlot) {
                --debugContactStorageSlot;
            }
#endif
            if (computerScroll > 0.0f) clampComputerScroll();
            setToast(Ui::Storage::DELETE_TOAST, nowMs);
            saveState();
            contactActionOpen = false;
            contactConfirmOpen = false;
            contactActionSlot = 0xFF;
            requestFullRender();
        } else {
            setToast(Ui::Storage::VISITING, nowMs);
            closeContactOverlay();
        }
        return;
    }

    const Game::FriendshipService::InviteResult result =
        Game::FriendshipService::inviteContact(
            gameState, contactActionSlot, gameState.gameMinutesTotal);
    switch (result) {
    case Game::FriendshipService::InviteResult::JOINED: {
        uint16_t speciesIds[Game::TEAM_CAP] = {};
        for (uint8_t slot = 0; slot < gameState.teamCount &&
             slot < Game::TEAM_CAP; ++slot) {
            speciesIds[slot] = gameState.team[slot].speciesId;
        }
        PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
        syncHomeActors(nowMs);
        saveState();
        setToast(Ui::Storage::INVITE_ACCEPTED, nowMs);
        closeContactOverlay();
        break;
    }
    case Game::FriendshipService::InviteResult::REFUSED:
        saveState();
        setToast(Ui::Storage::INVITE_REFUSED, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::LOCKED:
        setToast(Ui::Storage::INVITE_LOCKED, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::ALREADY_IN_TEAM:
        setToast(Ui::Storage::IN_TEAM, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::TEAM_FULL:
        setToast(Ui::Storage::TEAM_FULL_TOAST, nowMs);
        closeContactOverlay();
        break;
    case Game::FriendshipService::InviteResult::INVALID:
    default:
        closeContactOverlay();
        break;
    }
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
    const float maximum = static_cast<float>(
        computerMaxStorageScroll(gameState.storageCount));
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

void AmoledApp::clampTeamMovesScroll() {
    uint8_t rowCount = Game::MOVE_SLOT_COUNT;
    if (teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT) {
        rowCount = static_cast<uint8_t>(teamMovesRecallCount + 1);
    } else if (teamMovesMode == TeamMovesViewModel::Mode::RECALL_REPLACE) {
        rowCount = 3;
    }
    constexpr int ROW_STEP = 66;
    constexpr int LIST_TOP = TEAM_MOVES_HEADER_HEIGHT;
    constexpr int listBottom = AmoledUi::HEIGHT;
    int contentHeight = static_cast<int>(rowCount) * ROW_STEP;
    if (teamMovesMode == TeamMovesViewModel::Mode::MANAGE &&
        teamMovesSelectedItem < rowCount) {
        contentHeight += static_cast<int>(std::lround(
            (AmoledUi::HEIGHT / 2) * teamMovesDetailProgress));
    }
    const int maximum = std::max(0, contentHeight - (listBottom - LIST_TOP));
    teamMovesScroll = std::clamp<int16_t>(teamMovesScroll, 0,
                                           static_cast<int16_t>(maximum));
}

void AmoledApp::startTeamMovesDetailAnimation(bool visible, uint32_t nowMs) {
    teamMovesDetailTargetVisible = visible;
    teamMovesDetailAnimStartMs = nowMs;
    teamMovesDetailAnimating = true;
    if (visible && teamMovesDetailProgress >= 1.0f) {
        teamMovesDetailAnimating = false;
        teamMovesDetailProgress = 1.0f;
    }
    if (!visible && teamMovesDetailProgress <= 0.0f) {
        teamMovesDetailAnimating = false;
        teamMovesDetailProgress = 0.0f;
        teamMovesSelectedItem = 0xFF;
    }
    requestRenderRows(TEAM_MOVES_HEADER_HEIGHT, 448);
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
    progressionLevelUpStartedMs = nowMs;
    progressionLevelUpTouchAllowed = false;
    progressionFromSpeciesId = monster.speciesId;
    progressionToSpeciesId = 0;
    progressionMoveId = 0;
    progressionOldMove2 = 0;
    progressionOldMove3 = 0;
    progressionMoveCursor = 0;
    progressionPressedItem = 0xFF;
    progressionEvolution.reset();
    progressionEvolutionItem = Game::ItemId::COUNT;
    nextProgressionAnimationFrameMs = nowMs + PROGRESSION_ANIMATION_FRAME_MS;
    progressionEvolutionTouchStartedMs = 0;
    progressionEvolutionTouchResumeUntilMs = 0;
    progressionEvolutionTouchHeld = false;
    progressionEvolutionTouchConsumed = false;
    progressionMode = ProgressionViewModel::Mode::LEVEL_UP;
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
    setMusicContext(returnScene == AppSceneFlow::Scene::EXPLORE_ROUTE
                        ? MusicContext::EXPLORE : MusicContext::HOME);
    requestFullRender();
}

void AmoledApp::beginEvolutionSequence(uint16_t fromSpeciesId,
                                       uint16_t toSpeciesId,
                                       uint32_t nowMs,
                                       Game::ItemId consumedItem) {
    progressionFromSpeciesId = fromSpeciesId;
    progressionToSpeciesId = toSpeciesId;
    progressionEvolutionItem = consumedItem;
    progressionEvolution.begin(fromSpeciesId, toSpeciesId, nowMs);
    nextProgressionAnimationFrameMs = nowMs;
    progressionEvolutionTouchStartedMs = 0;
    progressionEvolutionTouchResumeUntilMs = 0;
    progressionEvolutionTouchHeld = false;
    progressionEvolutionTouchConsumed = false;
    const uint16_t species[] = {fromSpeciesId, toSpeciesId};
    PokemonSprites::preloadDynamicSpecies(species, 2, 2);
    Platform::logf("[AmoledEvolution] begin slot=%u from=%u to=%u item=%u\n",
                   progressionTeamSlot, fromSpeciesId, toSpeciesId,
                   static_cast<unsigned>(consumedItem));
}

void AmoledApp::openEvolutionProgression(AppSceneFlow::Scene returnScene,
                                          uint8_t teamSlot,
                                          uint16_t fromSpeciesId,
                                          uint16_t toSpeciesId,
                                          uint32_t nowMs,
                                          Game::ItemId consumedItem) {
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP ||
        fromSpeciesId == 0 || toSpeciesId == 0 ||
        fromSpeciesId == toSpeciesId) return;
    progressionReturnScene = returnScene;
    progressionTeamSlot = teamSlot;
    progressionOldLevel = gameState.team[teamSlot].level;
    progressionLevel = gameState.team[teamSlot].level;
    progressionMoveId = 0;
    progressionOldMove2 = 0;
    progressionOldMove3 = 0;
    progressionMoveCursor = 0;
    progressionPressedItem = 0xFF;
    progressionMode = ProgressionViewModel::Mode::EVOLUTION;
    beginEvolutionSequence(fromSpeciesId, toSpeciesId, nowMs, consumedItem);
    toast = nullptr;
    sceneFlow.enter(AppSceneFlow::Scene::PROGRESSION);
    setMusicContext(returnScene == AppSceneFlow::Scene::EXPLORE_ROUTE
                        ? MusicContext::EXPLORE : MusicContext::HOME);
    requestFullRender();
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
            progressionMode = ProgressionViewModel::Mode::EVOLUTION;
            progressionPressedItem = 0xFF;
            beginEvolutionSequence(monster.speciesId, target->id, nowMs);
            saveState();
            requestFullRender();
            return;
        }
        progressionMode = ProgressionViewModel::Mode::MOVE_LEARN;
        progressionMoveCursor = 0;
    } else if (progressionMode == ProgressionViewModel::Mode::EVOLUTION) {
        if (!progressionEvolution.animationComplete(nowMs)) return;
        const Species* target = findSpecies(progressionToSpeciesId);
        if (target && target->id != monster.speciesId) {
            if (progressionEvolutionItem != Game::ItemId::COUNT &&
                !Game::ItemInventory::remove(
                    gameState, progressionEvolutionItem, 1)) {
                setToast(Ui::Amoled::NO_STOCK, nowMs);
                return;
            }
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
            syncHomeActors(nowMs);
            saveState();

            const Species* nextTarget =
                progressionEvolutionItem == Game::ItemId::COUNT
                    ? levelUpEvolutionTarget(*target, monster) : nullptr;
            if (nextTarget && nextTarget->id != monster.speciesId) {
                progressionPressedItem = 0xFF;
                beginEvolutionSequence(
                    monster.speciesId, nextTarget->id, nowMs);
                requestFullRender();
                return;
            }
        }
        progressionEvolution.reset();
        progressionEvolutionItem = Game::ItemId::COUNT;
        nextProgressionAnimationFrameMs = 0;
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
    progressionSelectedItem = 0xFF;
    progressionScroll = 0;
    progressionDetailProgress = 0.0f;
    progressionDetailAnimating = false;
    progressionDetailTargetVisible = false;
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
    progressionEvolution.reset();
    progressionEvolutionItem = Game::ItemId::COUNT;
    nextProgressionAnimationFrameMs = 0;
    progressionEvolutionTouchStartedMs = 0;
    progressionEvolutionTouchResumeUntilMs = 0;
    progressionEvolutionTouchHeld = false;
    progressionEvolutionTouchConsumed = false;
    saveState();
    if (battleProgressionSequenceActive) {
        if (startNextBattleProgression(nowMs)) return;
        closeBattle(nowMs);
        return;
    }
    sceneFlow.enter(progressionReturnScene);
    if (progressionReturnScene == AppSceneFlow::Scene::EXPLORE_ROUTE) {
        resumeExploreRoute(nowMs);
    }
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
    showerToolX = SHOWER_PET_X;
    showerToolY = SHOWER_PET_Y;
    showerLastStrokeX = showerToolX;
    showerLastStrokeY = showerToolY;
    pressedShowerItem = -1;
    toast = nullptr;
    requestRenderRows(MENU_HEADER_HEIGHT, 448);
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

    if (showerMode == ShowerMode::SOAPING) {
        while (showerStrokeCarry >= SHOWER_FOAM_DISTANCE) {
            showerStrokeCarry -= SHOWER_FOAM_DISTANCE;
            spawnShowerFoam(nowMs);
        }
    } else if (showerMode == ShowerMode::BRUSHING) {
        if (showerStrokeCarry >= SHOWER_BRUSH_DISTANCE ||
            nowMs - showerLastRubMs >= SHOWER_BRUSH_INTERVAL_MS) {
            showerStrokeCarry = 0.0f;
            showerLastRubMs = nowMs;
            rubShowerFoamAt(x, y, nowMs);
        }
    }

    requestRenderRows(MENU_HEADER_HEIGHT, 448);
}

void AmoledApp::spawnShowerFoam(uint32_t nowMs) {
    static constexpr int8_t OFFSETS[][2] = {
        {-48, -60}, {40, -50}, {-16, -28}, {50, 0},
        {-50, 8}, {16, 28}, {-32, 58}, {40, 56},
    };
    for (uint8_t i = 0; i < SHOWER_FOAM_CAP; ++i) {
        const uint8_t index = static_cast<uint8_t>(
            (showerFoamSpawnCursor + i) % SHOWER_FOAM_CAP);
        ShowerFoamState& foam = showerFoam[index];
        if (foam.active) continue;
        const int8_t* offset = OFFSETS[index];
        foam.restX = static_cast<int16_t>(SHOWER_PET_X + offset[0]);
        foam.restYOffset = static_cast<int8_t>(offset[1]);
        foam.x = static_cast<int16_t>(foam.restX + GameRandom::random(-3, 4));
        foam.y = static_cast<int16_t>(SHOWER_PET_Y + foam.restYOffset +
                                       GameRandom::random(-3, 4));
        foam.stage = 0;
        foam.brushProgress = 0;
        foam.active = true;
        showerFoamSpawnCursor = static_cast<uint8_t>((index + 1) % SHOWER_FOAM_CAP);
        showerLastFoamGrowthMs = nowMs;
        if (!showerSoapRewarded) {
            grantShowerStage(Game::BathService::Stage::SOAP, nowMs);
        }
        requestRenderRows(MENU_HEADER_HEIGHT, 328);
        return;
    }
}

ShowerFoamState* AmoledApp::findShowerFoamPartner(
    const ShowerFoamState& source) {
    for (uint8_t i = 0; i < SHOWER_FOAM_CAP; ++i) {
        ShowerFoamState& candidate = showerFoam[i];
        if (&candidate == &source || !candidate.active ||
            candidate.stage != source.stage) {
            continue;
        }
        const int dx = candidate.x - source.x;
        const int dy = candidate.y - source.y;
        if (dx * dx + dy * dy <= 48 * 48) return &candidate;
    }
    return nullptr;
}

bool AmoledApp::tryMergeShowerFoam(ShowerFoamState& source,
                                   uint32_t nowMs) {
    if (!source.active || source.stage < 1 || source.stage >= 3) return false;
    ShowerFoamState* partner = findShowerFoamPartner(source);
    if (!partner) return false;
    partner->stage = std::min<uint8_t>(3, static_cast<uint8_t>(source.stage + 1));
    partner->brushProgress = 0;
    source.active = false;
    source.brushProgress = 0;
    showerLastFoamGrowthMs = nowMs;
    checkShowerAtmosphereThreshold(nowMs);
    return true;
}

void AmoledApp::rubShowerFoamAt(int x, int y, uint32_t nowMs) {
    ShowerFoamState* nearest = nullptr;
    int nearestDistance = 25 * 25;
    for (uint8_t i = 0; i < SHOWER_FOAM_CAP; ++i) {
        ShowerFoamState& foam = showerFoam[i];
        if (!foam.active) continue;
        const int dx = foam.x - x;
        const int dy = foam.y - y;
        const int distance = dx * dx + dy * dy;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = &foam;
        }
    }
    if (!nearest) return;
    ++nearest->brushProgress;
    const uint8_t threshold = nearest->stage < 2 ? 2 : 4;
    if (nearest->brushProgress >= threshold) {
        nearest->brushProgress = 0;
        if (nearest->stage < 3) ++nearest->stage;
        tryMergeShowerFoam(*nearest, nowMs);
        checkShowerAtmosphereThreshold(nowMs);
    }
    requestRenderRows(MENU_HEADER_HEIGHT, 328);
}

void AmoledApp::checkShowerAtmosphereThreshold(uint32_t nowMs) {
    if (showerAtmosphereTarget) return;
    bool largeFoam = false;
    for (const ShowerFoamState& foam : showerFoam) {
        if (!foam.active) continue;
        largeFoam = largeFoam || foam.stage >= 3;
    }
    if (largeFoam && showerFoamLevelTotal() >= 6) {
        for (ShowerFoamState& foam : showerFoam) foam.active = false;
        showerAtmosphereTarget = true;
        grantShowerStage(Game::BathService::Stage::BRUSH, nowMs);
    }
}

uint8_t AmoledApp::showerFoamLevelTotal() const {
    uint8_t total = 0;
    for (const ShowerFoamState& foam : showerFoam) {
        if (foam.active) total = static_cast<uint8_t>(
            std::min<int>(255, total + foam.stage + 1));
    }
    return total;
}

bool AmoledApp::anyShowerFoam() const {
    for (const ShowerFoamState& foam : showerFoam) {
        if (foam.active) return true;
    }
    return false;
}

void AmoledApp::rinseShowerFoamOneStage() {
    for (ShowerFoamState& foam : showerFoam) {
        if (!foam.active) continue;
        if (foam.stage > 0) --foam.stage;
        else foam.active = false;
    }
    if (!anyShowerFoam()) showerAtmosphereTarget = false;
}

void AmoledApp::updateShowerFoamMotion(float dtSeconds) {
    (void)dtSeconds;
    for (ShowerFoamState& foam : showerFoam) {
        if (!foam.active) continue;
        const int targetY = SHOWER_PET_Y + foam.restYOffset;
        if (foam.y < targetY) ++foam.y;
        else if (foam.y > targetY) --foam.y;
    }
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
    showerLastRinseFoamMs = nowMs;
    showerRinseProgress = 0;
    showerAtmosphereTarget = false;
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
        requestRenderRows(MENU_HEADER_HEIGHT, 448);
        return;
    }
    closeShowerScene();
}

void AmoledApp::resetShowerSession(uint32_t nowMs) {
    showerMode = ShowerMode::MENU;
    showerSoapIndex = 0;
    showerRinseProgress = 0;
    showerCompletionHearts = 0;
    showerToolX = SHOWER_PET_X;
    showerToolY = SHOWER_PET_Y;
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
    showerLastRubMs = 0;
    showerLastFoamGrowthMs = 0;
    showerLastRinseFoamMs = nowMs;
    showerFoamSpawnCursor = 0;
    showerFoamRestSlot = 0;
    showerAtmosphereAlpha = 0.0f;
    showerAtmosphereTarget = false;
    for (ShowerFoamState& foam : showerFoam) foam = ShowerFoamState{};
    pressedShowerItem = -1;
    showerToast[0] = '\0';
    toast = nullptr;
}

void AmoledApp::beginTeamStatusSlide(uint8_t targetPage, uint32_t nowMs) {
    teamStatusAnimating = true;
    teamStatusAnimStartMs = nowMs;
    teamStatusAnimFromX = teamStatusSlideX;
    teamStatusAnimToX = static_cast<int16_t>(
        (static_cast<int>(teamStatusPage) - static_cast<int>(targetPage)) *
        AmoledUi::WIDTH);
    teamStatusAnimTargetPage = targetPage;
    requestRenderRows(MENU_HEADER_HEIGHT, 448);
}

void AmoledApp::openTeamScene(bool preserveSubSceneReturn) {
    if (preserveSubSceneReturn) {
        sceneFlow.enter(AppSceneFlow::Scene::TEAM);
    } else {
        sceneFlow.openSubScene(AppSceneFlow::Scene::TEAM);
    }
    uint16_t speciesIds[Game::TEAM_CAP] = {};
    uint8_t count = Game::TeamRoster::memberCount(gameState);
    for (uint8_t slot = 0; slot < count; ++slot) {
        speciesIds[slot] = gameState.team[slot].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, count);
    pressedTeamSlot = -1;
    teamActionPopupOpen = false;
    pendingTeamSlot = 0;
    teamConfirmOpen = false;
    teamStatusOpen = false;
    teamStatusSlot = 0;
    teamStatusPage = 0;
    teamStatusDragging = false;
    teamStatusSlideX = 0;
    teamStatusAnimating = false;
    teamMovesOpen = false;
    teamMovesScroll = 0;
    teamMovesDragging = false;
    teamMovesSelectedItem = 0xFF;
    teamMovesDetailProgress = 0.0f;
    teamMovesDetailAnimating = false;
    teamMovesDetailTargetVisible = false;
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
    teamMovesScroll = 0;
    teamMovesDragging = false;
    teamMovesSelectedItem = 0xFF;
    teamMovesRecallSelected = 0xFF;
    teamMovesDetailProgress = 0.0f;
    teamMovesDetailAnimating = false;
    teamMovesDetailTargetVisible = false;
    refreshTeamMoveRecallable();
    toast = nullptr;
    requestFullRender();
}

void AmoledApp::switchTeamLeader(uint32_t nowMs) {
    cancelRoomAction(nowMs);
    cancelPairInteraction(nowMs);
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
    scheduleAttention(nowMs, true);
    scheduleSpecialAction(nowMs);
    updatePetFootprint();
    updateCamera();
    saveState();
    setToast(Ui::Amoled::LEADER_CHANGED, nowMs);
}

void AmoledApp::leaveTeamMember(uint8_t teamSlot, uint32_t nowMs) {
    if (gameState.teamCount <= 1) {
        setToast(Ui::Team::LEAVE_LAST_TOAST, nowMs);
        return;
    }
    if (teamSlot == 0 || teamSlot >= gameState.teamCount ||
        teamSlot >= Game::TEAM_CAP ||
        gameState.team[teamSlot].origin == Game::Origin::VISITOR) {
        return;
    }
    if (gameState.storageCount >= Game::STORAGE_CAP) {
        setToast(Ui::Team::CONTACTS_FULL_TOAST, nowMs);
        return;
    }

    int8_t contactSlot = -1;
    for (uint8_t index = 0;
         index < gameState.storageCount && index < Game::STORAGE_CAP; ++index) {
        if (ContactRoster::sameMonster(gameState.storage[index],
                                       gameState.team[teamSlot])) {
            contactSlot = static_cast<int8_t>(index);
            break;
        }
    }
    if (contactSlot >= 0) {
        gameState.storage[contactSlot] = gameState.team[teamSlot];
    } else {
        gameState.storage[gameState.storageCount++] = gameState.team[teamSlot];
    }
    for (uint8_t index = teamSlot;
         index + 1 < gameState.teamCount && index + 1 < Game::TEAM_CAP;
         ++index) {
        gameState.team[index] = gameState.team[index + 1];
    }
    --gameState.teamCount;
    gameState.team[gameState.teamCount] = Game::MonsterRuntime{};
    gameState.activeSlot = 0;

    uint16_t speciesIds[Game::TEAM_CAP] = {};
    for (uint8_t index = 0; index < gameState.teamCount &&
         index < Game::TEAM_CAP; ++index) {
        speciesIds[index] = gameState.team[index].speciesId;
    }
    PokemonSprites::syncTeamCache(speciesIds, gameState.teamCount);
    syncHomeActors(nowMs);
    saveState();
    teamActionPopupOpen = false;
    setToast(Ui::Team::LEAVE_TOAST, nowMs);
}

void AmoledApp::performPendingItemAction(uint32_t nowMs) {
    const uint32_t actionStartMs = Platform::clock().millis();
    const uint32_t coinsBefore = gameState.coins;
    const unsigned stockBefore = Game::ItemInventory::count(gameState, pendingItem);
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
        if (sceneFlow.current() == AppSceneFlow::Scene::BAG &&
            !Game::ItemInventory::usableFromHomeBag(pendingItem)) {
            itemConfirmOpen = false;
            pendingItem = Game::ItemId::COUNT;
            pendingItemAction = PendingItemAction::NONE;
            setToast(Ui::Amoled::NOT_READY, nowMs);
            requestRenderRows(MENU_HEADER_HEIGHT, 448);
            return;
        }
        bool explorationItem = pendingItem == Game::ItemId::MAX_REPEL ||
                               pendingItem == Game::ItemId::HONEY;
        AppSceneFlow::Scene itemReturn = sceneFlow.subSceneReturn();
        bool exploring = itemReturn == AppSceneFlow::Scene::EXPLORE_MENU ||
                         itemReturn == AppSceneFlow::Scene::EXPLORE_ROUTE;
        if (explorationItem && exploring) {
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
                        exploring ? itemReturn : AppSceneFlow::Scene::HOME,
                        0, oldLevel, nowMs);
                    return;
                }
                if (oldSpeciesId != gameState.team[0].speciesId) {
                    saveState();
                    openEvolutionProgression(
                        exploring ? itemReturn : AppSceneFlow::Scene::HOME,
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
        Platform::logf(
            "[ShopAction] action=%s item=%d changed=%d result=%s "
            "coins=%lu->%lu stock=%u->%u elapsed_ms=%lu\n",
            pendingItemAction == PendingItemAction::BUY ? "buy" : "sell",
            static_cast<int>(pendingItem), changed, resultText,
            static_cast<unsigned long>(coinsBefore),
            static_cast<unsigned long>(gameState.coins), stockBefore,
            static_cast<unsigned>(Game::ItemInventory::count(gameState, pendingItem)),
            static_cast<unsigned long>(Platform::clock().millis() - actionStartMs));
        if (changed) {
            const uint32_t saveStartMs = Platform::clock().millis();
            const bool saved = saveState();
            Platform::logf("[ShopAction] saved=%d save_ms=%lu\n", saved,
                           static_cast<unsigned long>(Platform::clock().millis() - saveStartMs));
            toast = nullptr;
            requestFullRender();
        } else {
            setToast(resultText, nowMs);
        }
        Platform::logLine("[ShopAction] feedback_render_requested");
        return;
    }

    itemConfirmOpen = false;
    pendingItem = Game::ItemId::COUNT;
    pendingItemAction = PendingItemAction::NONE;
    if (changed) saveState();
    clampItemScroll();
    setToast(resultText, nowMs);
}

void AmoledApp::persistHomeViewState(uint32_t nowMs) {
    if (!homeRuntimeReady || gameState.teamCount == 0) return;
    const Game::MonsterRuntime& mainMonster = gameState.team[0];
    const Game::SpeciesCareProfile mainCare =
        Game::speciesCareProfileFor(mainMonster.speciesId);
    const bool mainSleepTime = mainCare.usesBed && Game::isSleepCareTime(
        gameState.gameMinutesTotal, mainMonster.nature);
    const bool transientMain = roomAction != RoomAction::NONE ||
        homeRuntime.pairActive() ||
        visitorMotion == VisitorMotion::HOST_APPROACH ||
        visitorMotion == VisitorMotion::ENTERING ||
        visitorMotion == VisitorMotion::EXITING || pendingExpedition ||
        expeditionDeparturePhase != ExpeditionDeparturePhase::NONE ||
        homeMainActor.task == Home::Task::ROOM_ACTION ||
        homeMainActor.task == Home::Task::PAIR_ACTION ||
        homeMainActor.task == Home::Task::DOOR_ACTION;
    const float mainGroundOffset = speciesGroundOffset(
        gameState.team[0].speciesId);
    mainViewState.valid = true;
    mainViewState.speciesId = gameState.team[0].speciesId;
    float savedMainX = petX;
    float savedMainY = petY;
    if (!petFootprintInsideWalkArea(savedMainX, savedMainY)) {
        RoomResource& room = RoomResource::ins();
        savedMainX = room.available()
            ? static_cast<float>(room.doorwayInsideX()) : 92.0f;
        savedMainY = room.available()
            ? static_cast<float>(room.doorwayInsideY()) : 151.0f;
    }
    mainViewState.monsterX = savedMainX;
    mainViewState.monsterY = savedMainY - mainGroundOffset;
    PetMotion storedMotion = petMotion;
    if (storedMotion == PetMotion::TURNING) storedMotion = petStopMotion;
    if (transientMain || storedMotion == PetMotion::EATING ||
        storedMotion == PetMotion::STOPPING ||
        (storedMotion == PetMotion::SLEEPING && !mainSleepTime)) {
        storedMotion = PetMotion::IDLE;
    }
    const bool stableRoute = storedMotion == PetMotion::WANDERING ||
        storedMotion == PetMotion::SEEKING_FOOD ||
        storedMotion == PetMotion::SEEKING_SLEEP;
    mainViewState.targetX = stableRoute ? petTargetX : savedMainX;
    mainViewState.targetY =
        (stableRoute ? petTargetY : savedMainY) - mainGroundOffset;
    switch (storedMotion) {
    case PetMotion::WANDERING: mainViewState.aiMode = 1; break;
    case PetMotion::SEEKING_FOOD: mainViewState.aiMode = 3; break;
    case PetMotion::SEEKING_SLEEP: mainViewState.aiMode = 4; break;
    case PetMotion::SLEEPING: mainViewState.aiMode = 8; break;
    case PetMotion::TURNING:
    case PetMotion::EATING:
    case PetMotion::IDLE:
    case PetMotion::STOPPING:
    default: mainViewState.aiMode = petResting ? 8 : 0; break;
    }
    if (mainViewState.aiMode == 8 && !mainSleepTime) {
        mainViewState.aiMode = 0;
    }
    mainViewState.pmdAction = 0;
    mainViewState.pmdDirection = mainDirectionForView(petDirection);
    mainViewState.pmdFrame = transientMain ? 0 : petFrame;
    mainViewState.facingRight =
        petDirection == PokemonSprites::WalkDirection::RIGHT;
    mainViewState.faintRestActive =
        mainMonster.fainted || mainMonster.hpCur == 0;
    const bool resetMainDecision = transientMain ||
        petMotion == PetMotion::EATING || petMotion == PetMotion::STOPPING;
    mainViewState.nextDecisionRemainingMs =
        !resetMainDecision &&
        static_cast<int32_t>(nextPetDecisionMs - nowMs) > 0
            ? nextPetDecisionMs - nowMs : 0;
    mainViewState.postFeedAwakeRemainingMs = 0;

    SecondarySceneViewState& secondary = mainViewState.secondary;
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        homeCompanionActor.speciesId != gameState.team[1].speciesId ||
        gameState.team[1].origin == Game::Origin::VISITOR) {
        secondary = SecondarySceneViewState{};
        return;
    }
    const Game::MonsterRuntime& monster = gameState.team[1];
    const Game::SpeciesCareProfile companionCare =
        Game::speciesCareProfileFor(monster.speciesId);
    const bool companionSleepTime = companionCare.usesBed &&
        Game::isSleepCareTime(gameState.gameMinutesTotal, monster.nature);
    const bool transientCompanion = homeRuntime.pairActive() ||
        visitorMotion != VisitorMotion::NONE ||
        homeCompanionActor.task == Home::Task::ROOM_ACTION ||
        homeCompanionActor.task == Home::Task::PAIR_ACTION ||
        homeCompanionActor.task == Home::Task::DOOR_ACTION ||
        homeCompanionActor.task == Home::Task::SEEK_FOOD ||
        homeCompanionActor.task == Home::Task::FEEDING ||
        homeCompanionActor.task == Home::Task::SEEK_SLEEP ||
        homeCompanionActor.task == Home::Task::YIELDING;
    const float companionGroundOffset = speciesGroundOffset(
        monster.speciesId);
    secondary = SecondarySceneViewState{};
    secondary.valid = true;
    secondary.speciesId = monster.speciesId;
    secondary.ivPacked = monster.ivPacked;
    secondary.metAt = monster.metAt;
    secondary.nature = monster.nature;
    secondary.metArea = monster.metArea;
    secondary.origin = static_cast<uint8_t>(monster.origin);
    secondary.x = homeCompanionActor.x;
    secondary.y = homeCompanionActor.y - companionGroundOffset;
    const bool stableCompanionRoute = !transientCompanion &&
        homeCompanionActor.task == Home::Task::WANDER;
    secondary.targetX = stableCompanionRoute
        ? homeCompanionActor.targetX : homeCompanionActor.x;
    secondary.targetY = (stableCompanionRoute
        ? homeCompanionActor.targetY : homeCompanionActor.y) -
        companionGroundOffset;
    secondary.sleepX = homeCompanionActor.sleepX;
    secondary.sleepY = homeCompanionActor.sleepY - companionGroundOffset;
    secondary.state = stableCompanionRoute
        ? 1
        : (!transientCompanion && companionSleepTime &&
           homeCompanionActor.task == Home::Task::SLEEPING)
            ? 6 : 0;
    secondary.direction = static_cast<uint8_t>(companionDirection);
    secondary.frameIndex = transientCompanion ? 0 : companionFrame;
    secondary.facingRight =
        companionDirection == PokemonSprites::WalkDirection::RIGHT;
    secondary.sleepSpotValid = homeCompanionActor.sleepSpotValid;
    secondary.stateRemainingMs =
        !transientCompanion &&
        static_cast<int32_t>(homeCompanionActor.nextDecisionMs - nowMs) > 0
            ? homeCompanionActor.nextDecisionMs - nowMs : 0;
    secondary.foodRetryRemainingMs =
        static_cast<int32_t>(
            homeCompanionActor.foodWakeRetryAfterMs - nowMs) > 0
            ? homeCompanionActor.foodWakeRetryAfterMs - nowMs : 0;
}

bool AmoledApp::saveState() {
    if (!storageReady) return false;
    persistHomeViewState(Platform::clock().millis());
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
