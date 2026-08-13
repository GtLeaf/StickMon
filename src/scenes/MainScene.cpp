#include "scenes/MainScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/GameAssets.h"
#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/ButtonDispatcher.h"
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/ProgressionUi.h"
#include "core/RoomMovementArea.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/TraceLog.h"
#include "core/UiStrings.h"
#include "core/VoiceCallService.h"
#include "game/SpeciesBehavior.h"
#include "game/GameRandom.h"
#include "game/HomeHud.h"
#include "hardware/Hal.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TutorialOverlay.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"

namespace {
static constexpr uint16_t PMD_IDLE_FRAME_MS = 520;
static constexpr uint16_t PMD_SLEEPING_FRAME_MS = 700;
static constexpr float PMD_MOVING_SPEED_EPSILON = 1.0f;
static constexpr float PI_F = 3.14159265f;
static constexpr float PMD_SHORT_MOVE_DISTANCE = 14.0f;
static constexpr float DEBUG_TILT_DEADZONE = 0.08f;
static constexpr float DEBUG_TILT_MAX = 0.62f;
static constexpr float DEBUG_TILT_SPEED = 58.0f;
static constexpr float CAMERA_FOCUS_Y = 84.0f;
static constexpr float FOOD_FEED_OFFSET_X = 12.0f;
static constexpr float FOOD_FEED_OFFSET_Y = 4.0f;
static constexpr float BED_APPROACH_TOLERANCE_X = 18.0f;
static constexpr float BED_APPROACH_TOLERANCE_Y = 14.0f;
static constexpr float BED_SLEEP_TOLERANCE_X = 2.5f;
static constexpr float BED_SLEEP_TOLERANCE_Y = 6.0f;
static constexpr uint16_t NIGHT_FEED_WAKE_DELAY_MIN_MS = 2600;
static constexpr uint16_t NIGHT_FEED_WAKE_DELAY_MAX_MS = 6500;
static constexpr uint32_t VISITOR_NIGHT_FOOD_RETRY_MS = 60000UL;
static constexpr uint16_t DAY_WAKE_DELAY_MIN_MS = 650;
static constexpr uint16_t DAY_WAKE_DELAY_MAX_MS = 1300;
static constexpr uint16_t FEED_BITE_DELAY_MIN_MS = 700;
static constexpr uint16_t FEED_BITE_DELAY_MAX_MS = 1300;
static constexpr uint16_t FEED_BITE_INTERVAL_MIN_MS = 1000;
static constexpr uint16_t FEED_BITE_INTERVAL_MAX_MS = 1600;
static constexpr uint16_t FEED_SESSION_MIN_MS = 3200;
static constexpr uint16_t FEED_SESSION_MAX_MS = 5600;
static constexpr uint16_t POST_FEED_AWAKE_MIN_MS = 7000;
static constexpr uint16_t POST_FEED_AWAKE_MAX_MS = 13000;
static constexpr uint32_t POST_FAINT_AWAKE_MS = 30000UL;
static constexpr uint16_t MIND_UPDATE_MS = 400;
static constexpr uint16_t MOVE_STUCK_MS = 1600;
static constexpr float MOVE_PROGRESS_EPSILON = 0.45f;
static constexpr uint32_t ATTENTION_INITIAL_MIN_MS = 25000UL;
static constexpr uint32_t ATTENTION_INITIAL_MAX_MS = 45000UL;
static constexpr uint32_t ATTENTION_MIN_MS = 90000UL;
static constexpr uint32_t ATTENTION_MAX_MS = 180000UL;
static constexpr uint32_t SPECIAL_ACTION_MIN_MS = 20000UL;
static constexpr uint32_t SPECIAL_ACTION_MAX_MS = 40000UL;
static constexpr uint32_t ATTENTION_WAIT_MS = 6000UL;
static constexpr uint32_t WINDOW_GAZE_MIN_MS = 6000UL;
static constexpr uint32_t WINDOW_GAZE_MAX_MS = 10000UL;
static constexpr uint32_t WINDOW_GAZE_DELAY_SECONDS = 2UL * 24UL * 60UL * 60UL;
static constexpr uint16_t HUNGER_ANIM_MS = 360;
static constexpr float DOOR_ROUTE_SPEED = 42.0f;
static constexpr float DOOR_CROSS_SPEED = 28.0f;
static constexpr uint32_t DOOR_ROUTE_TIMEOUT_MS = 8000UL;
static constexpr uint32_t DOOR_STALL_TIMEOUT_MS = 700UL;
static constexpr float DOOR_WAIT_INWARD_OFFSET = 8.0f;
static constexpr float DOOR_WAIT_SIDE_OFFSET = 34.0f;
static constexpr int NAV_CELL_PX = 8;
static constexpr uint8_t NAV_MAX_COLS = 32;
static constexpr uint8_t NAV_MAX_ROWS = 32;
static constexpr uint16_t NAV_MAX_NODES = NAV_MAX_COLS * NAV_MAX_ROWS;
static constexpr float VISITOR_DROP_SPEED = 90.0f;
static constexpr float VISITOR_DROP_HEIGHT = 64.0f;
static constexpr float VISITOR_AVOID_RADIUS = 20.0f;
static constexpr float VISITOR_SLEEP_ARRIVE_DIST = 2.0f;
static constexpr uint16_t VISITOR_SLEEP_FRAME_MS = 800;
static constexpr uint16_t VISITOR_SLEEP_ZZ_CYCLE_MS = 1600;
static constexpr uint32_t VISITOR_SLEEP_TRACE_INTERVAL_MS = 5000UL;
static constexpr uint32_t MAIN_AI_TRACE_INTERVAL_MS = 2000UL;

static_assert(!monsterShouldWakeForFood(59) &&
              monsterShouldWakeForFood(MONSTER_SLEEP_FOOD_WAKE_SATIETY),
              "normal hunger must not interrupt scheduled sleep");
static constexpr uint32_t CONTACT_GUEST_MOTION_TIMEOUT_MS = 8000UL;
static constexpr uint32_t PAIR_INTERACTION_MIN_INTERVAL_MS = 90000UL;
static constexpr uint32_t PAIR_INTERACTION_MAX_INTERVAL_MS = 180000UL;
static constexpr uint32_t PAIR_INTERACTION_RETRY_MS = 5000UL;
static constexpr uint32_t PAIR_TALK_TIMEOUT_MS = 9000UL;
static constexpr uint32_t PAIR_CHASE_TIMEOUT_MS = 24000UL;
static constexpr uint16_t PAIR_INVITE_MS = 750;
static constexpr uint16_t PAIR_CELEBRATE_MS = 1200;
static constexpr uint16_t PAIR_TALK_HOP_MS = 560;
static constexpr uint16_t PAIR_TALK_GAP_MS = 180;
static constexpr uint16_t PAIR_TALK_END_PAUSE_MS = 240;
static constexpr uint16_t PAIR_TALK_TOTAL_MS =
    PAIR_TALK_HOP_MS * 2 + PAIR_TALK_GAP_MS + PAIR_TALK_END_PAUSE_MS;
static constexpr uint16_t PAIR_FOLLOW_DELAY_MS = 500;
static constexpr float PAIR_CHASE_LEADER_SPEED = 18.0f;
static constexpr float PAIR_CHASE_FOLLOWER_SPEED = 17.0f;
static constexpr float PAIR_APPROACH_SPEED = 12.5f;
static constexpr float PAIR_SETTLE_SPEED = 11.5f;

int mainTextPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

constexpr bool doorRouteStepAllowed(bool enforceWalkArea, bool enteringWalkArea,
                                    bool currentInside, bool nextInside) {
    return !enforceWalkArea || nextInside || (enteringWalkArea && !currentInside);
}

constexpr bool visitorReachesDoorFirst(float mainDistanceSq, float visitorDistanceSq) {
    return visitorDistanceSq < mainDistanceSq;
}

constexpr bool windowGazeDue(uint32_t lastExploredAt, uint32_t lastWindowGazeAt,
                             uint32_t nowGameSeconds) {
    return lastWindowGazeAt <= lastExploredAt &&
           nowGameSeconds >= lastExploredAt &&
           nowGameSeconds - lastExploredAt >= WINDOW_GAZE_DELAY_SECONDS;
}

static_assert(doorRouteStepAllowed(true, true, false, false) &&
                  doorRouteStepAllowed(true, true, false, true) &&
                  !doorRouteStepAllowed(true, true, true, false) &&
                  !doorRouteStepAllowed(true, false, false, false),
              "door route may enter the walk area from bed but never leave it");
static_assert(visitorReachesDoorFirst(100.0f, 99.0f) &&
                  !visitorReachesDoorFirst(99.0f, 100.0f) &&
                  !visitorReachesDoorFirst(100.0f, 100.0f),
              "the nearest actor leaves first, with stable main-actor tie breaking");
static_assert(!windowGazeDue(100, 100, 100 + WINDOW_GAZE_DELAY_SECONDS - 1) &&
                  windowGazeDue(100, 100, 100 + WINDOW_GAZE_DELAY_SECONDS) &&
                  !windowGazeDue(100, 101, 100 + WINDOW_GAZE_DELAY_SECONDS) &&
                  windowGazeDue(200, 101, 200 + WINDOW_GAZE_DELAY_SECONDS) &&
                  !windowGazeDue(200, 101, 199),
              "window gaze must run once per long period without exploration");

void* gNavScratch = nullptr;
int16_t* gNavParent = nullptr;
uint16_t* gNavQueue = nullptr;

bool ensureNavScratch() {
    if (gNavScratch) return true;
    size_t parentBytes = sizeof(int16_t) * NAV_MAX_NODES;
    size_t queueBytes = sizeof(uint16_t) * NAV_MAX_NODES;
    size_t totalBytes = parentBytes + queueBytes;
    gNavScratch = Platform::memory().allocate(totalBytes, true);
    if (!gNavScratch) return false;
    gNavParent = static_cast<int16_t*>(gNavScratch);
    gNavQueue = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(gNavScratch) + parentBytes);
    return true;
}

RoomResource& room() {
    RoomResource::ins().begin();
    return RoomResource::ins();
}

uint16_t roomHeight() { return room().height(); }
int16_t roomOriginY() { return room().roomY(); }
int16_t roomWalkMinX() { return room().walkMinX(); }
int16_t roomWalkMinY() { return room().walkMinY(); }
int16_t roomWalkMaxX() { return room().walkMaxX(); }
int16_t roomWalkMaxY() { return room().walkMaxY(); }
int16_t roomBedMinX() { return room().bedMinX(); }
int16_t roomBedMinY() { return room().bedMinY(); }
int16_t roomBedMaxX() { return room().bedMaxX(); }
int16_t roomBedMaxY() { return room().bedMaxY(); }
float foodCenterX() { return (float)room().foodX(); }
float foodCenterY() { return (float)room().foodY(); }
float foodFeedX() { return foodCenterX() + FOOD_FEED_OFFSET_X; }
float foodFeedY() { return foodCenterY() + FOOD_FEED_OFFSET_Y; }
float bedCenterX() { return (float)room().bedX(); }
float bedCenterY() { return (float)room().bedY(); }
float bedSleepX() { return (float)room().bedX(); }
float bedSleepY() { return (float)room().bedY(); }

uint32_t currentGameSeconds() {
    uint64_t seconds = (uint64_t)GameEngine::ins().gameMinutesTotal() * 60ULL;
    return seconds > UINT32_MAX ? UINT32_MAX : (uint32_t)seconds;
}

uint8_t wrappedDirectionValue(uint8_t direction, int delta) {
    int value = ((int)direction + delta) % 8;
    if (value < 0) value += 8;
    return (uint8_t)value;
}

enum class PmdMotionMode : uint8_t {
    LOOP,
    START_HOLD_END,
    PINGPONG,
};

struct PmdSpriteConfig {
    uint16_t speciesId;
    uint8_t idleFrames;
    uint16_t idleFrameMs;
    uint8_t walkingFrames;
    uint8_t sleepingFrames;
    PmdMotionMode motionMode;
    PokemonSprites::SpriteKind idleBase;
    PokemonSprites::SpriteKind walkingBase;
    PokemonSprites::SpriteKind sleepingBase;
    bool mirrorRightDirections;
    float airHeight;
    float bobAmplitude;
};

static constexpr PmdSpriteConfig PMD_SPRITE_CONFIGS[] = {
    {1, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::BULBASAUR_IDLE_FRONT_0, PokemonSprites::SpriteKind::BULBASAUR_WALKING_FRONT_0, PokemonSprites::SpriteKind::BULBASAUR_SLEEPING_0, true, 0.0f, 0.0f},
    {2, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::IVYSAUR_IDLE_FRONT_0, PokemonSprites::SpriteKind::IVYSAUR_WALKING_FRONT_0, PokemonSprites::SpriteKind::IVYSAUR_SLEEPING_0, true, 0.0f, 0.0f},
    {3, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::VENUSAUR_IDLE_FRONT_0, PokemonSprites::SpriteKind::VENUSAUR_WALKING_FRONT_0, PokemonSprites::SpriteKind::VENUSAUR_SLEEPING_0, true, 0.0f, 0.0f},
    {4, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CHARMANDER_IDLE_FRONT_0, PokemonSprites::SpriteKind::CHARMANDER_WALKING_FRONT_0, PokemonSprites::SpriteKind::CHARMANDER_SLEEPING_0, true, 0.0f, 0.0f},
    {5, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CHARMELEON_IDLE_FRONT_0, PokemonSprites::SpriteKind::CHARMELEON_WALKING_FRONT_0, PokemonSprites::SpriteKind::CHARMELEON_SLEEPING_0, true, 0.0f, 0.0f},
    {6, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CHARIZARD_IDLE_FRONT_0, PokemonSprites::SpriteKind::CHARIZARD_WALKING_FRONT_0, PokemonSprites::SpriteKind::CHARIZARD_SLEEPING_0, true, 0.0f, 0.0f},
    {7, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SQUIRTLE_IDLE_FRONT_0, PokemonSprites::SpriteKind::SQUIRTLE_WALKING_FRONT_0, PokemonSprites::SpriteKind::SQUIRTLE_SLEEPING_0, true, 0.0f, 0.0f},
    {8, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::WARTORTLE_IDLE_FRONT_0, PokemonSprites::SpriteKind::WARTORTLE_WALKING_FRONT_0, PokemonSprites::SpriteKind::WARTORTLE_SLEEPING_0, false, 0.0f, 0.0f},
    {9, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::BLASTOISE_IDLE_FRONT_0, PokemonSprites::SpriteKind::BLASTOISE_WALKING_FRONT_0, PokemonSprites::SpriteKind::BLASTOISE_SLEEPING_0, true, 0.0f, 0.0f},
    {10, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CATERPIE_IDLE_FRONT_0, PokemonSprites::SpriteKind::CATERPIE_WALKING_FRONT_0, PokemonSprites::SpriteKind::CATERPIE_SLEEPING_0, true, 0.0f, 0.0f},
    {11, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::START_HOLD_END, PokemonSprites::SpriteKind::METAPOD_IDLE_FRONT_0, PokemonSprites::SpriteKind::METAPOD_WALKING_FRONT_0, PokemonSprites::SpriteKind::METAPOD_SLEEPING_0, true, 0.0f, 0.0f},
    {12, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::BUTTERFREE_IDLE_FRONT_0, PokemonSprites::SpriteKind::BUTTERFREE_WALKING_FRONT_0, PokemonSprites::SpriteKind::BUTTERFREE_SLEEPING_0, true, 10.0f, 2.5f},
    {16, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIDGEY_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIDGEY_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIDGEY_SLEEPING_0, true, 0.0f, 0.0f},
    {17, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIDGEOTTO_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIDGEOTTO_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIDGEOTTO_SLEEPING_0, true, 0.0f, 0.0f},
    {18, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIDGEOT_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIDGEOT_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIDGEOT_SLEEPING_0, true, 0.0f, 0.0f},
    {25, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PIKACHU_IDLE_FRONT_0, PokemonSprites::SpriteKind::PIKACHU_WALKING_FRONT_0, PokemonSprites::SpriteKind::PIKACHU_SLEEPING_0, true, 0.0f, 0.0f},
    {26, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::RAICHU_IDLE_FRONT_0, PokemonSprites::SpriteKind::RAICHU_WALKING_FRONT_0, PokemonSprites::SpriteKind::RAICHU_SLEEPING_0, false, 0.0f, 0.0f},
    {74, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GEODUDE_IDLE_FRONT_0, PokemonSprites::SpriteKind::GEODUDE_WALKING_FRONT_0, PokemonSprites::SpriteKind::GEODUDE_SLEEPING_0, true, 0.0f, 0.0f},
    {75, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GRAVELER_IDLE_FRONT_0, PokemonSprites::SpriteKind::GRAVELER_WALKING_FRONT_0, PokemonSprites::SpriteKind::GRAVELER_SLEEPING_0, true, 0.0f, 0.0f},
    {76, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GOLEM_IDLE_FRONT_0, PokemonSprites::SpriteKind::GOLEM_WALKING_FRONT_0, PokemonSprites::SpriteKind::GOLEM_SLEEPING_0, true, 0.0f, 0.0f},
    {92, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GASTLY_IDLE_FRONT_0, PokemonSprites::SpriteKind::GASTLY_WALKING_FRONT_0, PokemonSprites::SpriteKind::GASTLY_SLEEPING_0, true, 12.0f, 2.0f},
    {93, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::START_HOLD_END, PokemonSprites::SpriteKind::HAUNTER_IDLE_FRONT_0, PokemonSprites::SpriteKind::HAUNTER_WALKING_FRONT_0, PokemonSprites::SpriteKind::HAUNTER_SLEEPING_0, true, 10.0f, 2.0f},
    {94, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GENGAR_IDLE_FRONT_0, PokemonSprites::SpriteKind::GENGAR_WALKING_FRONT_0, PokemonSprites::SpriteKind::GENGAR_SLEEPING_0, true, 0.0f, 0.0f},
    {123, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SCYTHER_IDLE_FRONT_0, PokemonSprites::SpriteKind::SCYTHER_WALKING_FRONT_0, PokemonSprites::SpriteKind::SCYTHER_SLEEPING_0, true, 0.0f, 0.0f},
    {129, 2, PMD_IDLE_FRAME_MS, 1, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MAGIKARP_IDLE_FRONT_0, PokemonSprites::SpriteKind::MAGIKARP_WALKING_FRONT_0, PokemonSprites::SpriteKind::MAGIKARP_SLEEPING_0, false, 0.0f, 0.0f},
    {130, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GYARADOS_IDLE_FRONT_0, PokemonSprites::SpriteKind::GYARADOS_WALKING_FRONT_0, PokemonSprites::SpriteKind::GYARADOS_SLEEPING_0, false, 0.0f, 0.0f},
    {133, 2, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::EEVEE_IDLE_FRONT_0, PokemonSprites::SpriteKind::EEVEE_WALKING_FRONT_0, PokemonSprites::SpriteKind::EEVEE_SLEEPING_0, false, 0.0f, 0.0f},
    {134, 1, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::VAPOREON_IDLE_FRONT_0, PokemonSprites::SpriteKind::VAPOREON_WALKING_FRONT_0, PokemonSprites::SpriteKind::VAPOREON_SLEEPING_0, false, 0.0f, 0.0f},
    {135, 1, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::JOLTEON_IDLE_FRONT_0, PokemonSprites::SpriteKind::JOLTEON_WALKING_FRONT_0, PokemonSprites::SpriteKind::JOLTEON_SLEEPING_0, false, 0.0f, 0.0f},
    {136, 3, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::FLAREON_IDLE_FRONT_0, PokemonSprites::SpriteKind::FLAREON_WALKING_FRONT_0, PokemonSprites::SpriteKind::FLAREON_SLEEPING_0, false, 0.0f, 0.0f},
    {196, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::ESPEON_IDLE_FRONT_0, PokemonSprites::SpriteKind::ESPEON_WALKING_FRONT_0, PokemonSprites::SpriteKind::ESPEON_SLEEPING_0, true, 0.0f, 0.0f},
    {197, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::UMBREON_IDLE_FRONT_0, PokemonSprites::SpriteKind::UMBREON_WALKING_FRONT_0, PokemonSprites::SpriteKind::UMBREON_SLEEPING_0, true, 0.0f, 0.0f},
    {143, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SNORLAX_IDLE_FRONT_0, PokemonSprites::SpriteKind::SNORLAX_WALKING_FRONT_0, PokemonSprites::SpriteKind::SNORLAX_SLEEPING_0, true, 0.0f, 0.0f},
    {147, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::DRATINI_IDLE_FRONT_0, PokemonSprites::SpriteKind::DRATINI_WALKING_FRONT_0, PokemonSprites::SpriteKind::DRATINI_SLEEPING_0, false, 0.0f, 0.0f},
    {148, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::DRAGONAIR_IDLE_FRONT_0, PokemonSprites::SpriteKind::DRAGONAIR_WALKING_FRONT_0, PokemonSprites::SpriteKind::DRAGONAIR_SLEEPING_0, false, 0.0f, 0.0f},
    {149, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::DRAGONITE_IDLE_FRONT_0, PokemonSprites::SpriteKind::DRAGONITE_WALKING_FRONT_0, PokemonSprites::SpriteKind::DRAGONITE_SLEEPING_0, false, 0.0f, 0.0f},
    {151, 3, 360, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MEW_IDLE_FRONT_0, PokemonSprites::SpriteKind::MEW_WALKING_FRONT_0, PokemonSprites::SpriteKind::MEW_SLEEPING_0, false, 14.0f, 3.0f},
    {161, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SENTRET_IDLE_FRONT_0, PokemonSprites::SpriteKind::SENTRET_WALKING_FRONT_0, PokemonSprites::SpriteKind::SENTRET_SLEEPING_0, true, 0.0f, 0.0f},
    {162, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::FURRET_IDLE_FRONT_0, PokemonSprites::SpriteKind::FURRET_WALKING_FRONT_0, PokemonSprites::SpriteKind::FURRET_SLEEPING_0, true, 0.0f, 0.0f},
    {261, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::POOCHYENA_IDLE_FRONT_0, PokemonSprites::SpriteKind::POOCHYENA_WALKING_FRONT_0, PokemonSprites::SpriteKind::POOCHYENA_SLEEPING_0, true, 0.0f, 0.0f},
    {262, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MIGHTYENA_IDLE_FRONT_0, PokemonSprites::SpriteKind::MIGHTYENA_WALKING_FRONT_0, PokemonSprites::SpriteKind::MIGHTYENA_SLEEPING_0, true, 0.0f, 0.0f},
    {278, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::WINGULL_IDLE_FRONT_0, PokemonSprites::SpriteKind::WINGULL_WALKING_FRONT_0, PokemonSprites::SpriteKind::WINGULL_SLEEPING_0, true, 10.0f, 2.0f},
    {279, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PELIPPER_IDLE_FRONT_0, PokemonSprites::SpriteKind::PELIPPER_WALKING_FRONT_0, PokemonSprites::SpriteKind::PELIPPER_SLEEPING_0, true, 8.0f, 1.5f},
    {172, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::PICHU_IDLE_FRONT_0, PokemonSprites::SpriteKind::PICHU_WALKING_FRONT_0, PokemonSprites::SpriteKind::PICHU_SLEEPING_0, true, 0.0f, 0.0f},
    {183, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::MARILL_IDLE_FRONT_0, PokemonSprites::SpriteKind::MARILL_WALKING_FRONT_0, PokemonSprites::SpriteKind::MARILL_SLEEPING_0, true, 0.0f, 0.0f},
    {184, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::AZUMARILL_IDLE_FRONT_0, PokemonSprites::SpriteKind::AZUMARILL_WALKING_FRONT_0, PokemonSprites::SpriteKind::AZUMARILL_SLEEPING_0, true, 0.0f, 0.0f},
    {194, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::WOOPER_IDLE_FRONT_0, PokemonSprites::SpriteKind::WOOPER_WALKING_FRONT_0, PokemonSprites::SpriteKind::WOOPER_SLEEPING_0, true, 0.0f, 0.0f},
    {195, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::QUAGSIRE_IDLE_FRONT_0, PokemonSprites::SpriteKind::QUAGSIRE_WALKING_FRONT_0, PokemonSprites::SpriteKind::QUAGSIRE_SLEEPING_0, true, 0.0f, 0.0f},
    {285, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SHROOMISH_IDLE_FRONT_0, PokemonSprites::SpriteKind::SHROOMISH_WALKING_FRONT_0, PokemonSprites::SpriteKind::SHROOMISH_SLEEPING_0, true, 0.0f, 0.0f},
    {286, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::BRELOOM_IDLE_FRONT_0, PokemonSprites::SpriteKind::BRELOOM_WALKING_FRONT_0, PokemonSprites::SpriteKind::BRELOOM_SLEEPING_0, true, 0.0f, 0.0f},
    {212, 2, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SCIZOR_IDLE_FRONT_0, PokemonSprites::SpriteKind::SCIZOR_WALKING_FRONT_0, PokemonSprites::SpriteKind::SCIZOR_SLEEPING_0, true, 0.0f, 0.0f},
    {380, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::LATIAS_IDLE_FRONT_0, PokemonSprites::SpriteKind::LATIAS_WALKING_FRONT_0, PokemonSprites::SpriteKind::LATIAS_SLEEPING_0, true, 18.0f, 2.0f},
    {381, 1, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::LATIOS_IDLE_FRONT_0, PokemonSprites::SpriteKind::LATIOS_WALKING_FRONT_0, PokemonSprites::SpriteKind::LATIOS_SLEEPING_0, true, 18.0f, 2.0f},
    {298, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::AZURILL_IDLE_FRONT_0, PokemonSprites::SpriteKind::AZURILL_WALKING_FRONT_0, PokemonSprites::SpriteKind::AZURILL_SLEEPING_0, true, 0.0f, 0.0f},
    {322, 2, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::NUMEL_IDLE_FRONT_0, PokemonSprites::SpriteKind::NUMEL_WALKING_FRONT_0, PokemonSprites::SpriteKind::NUMEL_SLEEPING_0, true, 0.0f, 0.0f},
    {323, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CAMERUPT_IDLE_FRONT_0, PokemonSprites::SpriteKind::CAMERUPT_WALKING_FRONT_0, PokemonSprites::SpriteKind::CAMERUPT_SLEEPING_0, true, 0.0f, 0.0f},
    {361, 1, PMD_IDLE_FRAME_MS, 4, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::SNORUNT_IDLE_FRONT_0, PokemonSprites::SpriteKind::SNORUNT_WALKING_FRONT_0, PokemonSprites::SpriteKind::SNORUNT_SLEEPING_0, true, 0.0f, 0.0f},
    {362, 1, PMD_IDLE_FRAME_MS, 1, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GLALIE_IDLE_FRONT_0, PokemonSprites::SpriteKind::GLALIE_WALKING_FRONT_0, PokemonSprites::SpriteKind::GLALIE_SLEEPING_0, true, 10.0f, 2.0f},
    {280, 2, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::RALTS_IDLE_FRONT_0, PokemonSprites::SpriteKind::RALTS_WALKING_FRONT_0, PokemonSprites::SpriteKind::RALTS_SLEEPING_0, true, 0.0f, 0.0f},
    {281, 2, PMD_IDLE_FRAME_MS, 2, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::KIRLIA_IDLE_FRONT_0, PokemonSprites::SpriteKind::KIRLIA_WALKING_FRONT_0, PokemonSprites::SpriteKind::KIRLIA_SLEEPING_0, true, 0.0f, 0.0f},
    {282, 3, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::START_HOLD_END, PokemonSprites::SpriteKind::GARDEVOIR_IDLE_FRONT_0, PokemonSprites::SpriteKind::GARDEVOIR_WALKING_FRONT_0, PokemonSprites::SpriteKind::GARDEVOIR_SLEEPING_0, true, 0.0f, 0.0f},
    {41, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::ZUBAT_IDLE_FRONT_0, PokemonSprites::SpriteKind::ZUBAT_WALKING_FRONT_0, PokemonSprites::SpriteKind::ZUBAT_SLEEPING_0, true, 12.0f, 2.0f},
    {42, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::GOLBAT_IDLE_FRONT_0, PokemonSprites::SpriteKind::GOLBAT_WALKING_FRONT_0, PokemonSprites::SpriteKind::GOLBAT_SLEEPING_0, true, 14.0f, 2.0f},
    {169, 1, PMD_IDLE_FRAME_MS, 3, 2, PmdMotionMode::LOOP, PokemonSprites::SpriteKind::CROBAT_IDLE_FRONT_0, PokemonSprites::SpriteKind::CROBAT_WALKING_FRONT_0, PokemonSprites::SpriteKind::CROBAT_SLEEPING_0, true, 14.0f, 2.0f},
};

const PmdSpriteConfig* pmdSpriteConfigForSpecies(uint16_t speciesId) {
    for (const auto& config : PMD_SPRITE_CONFIGS) {
        if (config.speciesId == speciesId) return &config;
    }
    return nullptr;
}

uint8_t rgb565R(uint16_t c) {
    return (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
}

uint8_t rgb565G(uint16_t c) {
    return (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
}

uint8_t rgb565B(uint16_t c) {
    return (uint8_t)((c & 0x1F) * 255 / 31);
}

uint16_t blendRgb565(uint16_t dst, uint16_t src, uint8_t alpha) {
    uint8_t inv = 255 - alpha;
    uint8_t r = (uint8_t)((rgb565R(src) * alpha + rgb565R(dst) * inv) / 255);
    uint8_t g = (uint8_t)((rgb565G(src) * alpha + rgb565G(dst) * inv) / 255);
    uint8_t b = (uint8_t)((rgb565B(src) * alpha + rgb565B(dst) * inv) / 255);
    return PixelRenderer::rgb(r, g, b);
}

void fillRoundRectAlpha(int x, int y, int w, int h, int radius,
                        uint16_t color, uint8_t alpha) {
    if (w <= 0 || h <= 0 || alpha == 0) return;
    auto& c = PixelRenderer::canvas();
    radius = MathUtil::max(0, MathUtil::min(radius, MathUtil::min(w, h) / 2));
    for (int py = y; py < y + h; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        for (int px = x; px < x + w; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            int localX = px - x;
            int localY = py - y;
            bool inside = radius == 0 ||
                          (localX >= radius && localX < w - radius) ||
                          (localY >= radius && localY < h - radius);
            if (!inside) {
                int centerX = localX < radius ? radius : w - radius - 1;
                int centerY = localY < radius ? radius : h - radius - 1;
                int dx = localX - centerX;
                int dy = localY - centerY;
                inside = dx * dx + dy * dy <= radius * radius;
            }
            if (!inside) continue;
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

void fillRadialLightAlpha(int centerX, int centerY, int radiusX, int radiusY, uint16_t color, uint8_t maxAlpha) {
    if (radiusX <= 0 || radiusY <= 0 || maxAlpha == 0) return;
    auto& c = PixelRenderer::canvas();
    int minX = centerX - radiusX;
    int maxX = centerX + radiusX;
    int minY = centerY - radiusY;
    int maxY = centerY + radiusY;
    for (int py = minY; py <= maxY; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        float dy = (float)(py - centerY) / (float)radiusY;
        for (int px = minX; px <= maxX; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            float dx = (float)(px - centerX) / (float)radiusX;
            float distSq = dx * dx + dy * dy;
            if (distSq > 1.0f) continue;
            uint8_t alpha = (uint8_t)(maxAlpha * (1.0f - distSq));
            if (alpha == 0) continue;
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

void fillSoftEllipseAlpha(int centerX, int centerY, int radiusX, int radiusY, uint16_t color, uint8_t maxAlpha) {
    fillRadialLightAlpha(centerX, centerY, radiusX, radiusY, color, maxAlpha);
}

void fillEllipseAlpha(int centerX, int centerY, int radiusX, int radiusY, uint16_t color, uint8_t alpha) {
    if (radiusX <= 0 || radiusY <= 0 || alpha == 0) return;
    auto& c = PixelRenderer::canvas();
    int minX = centerX - radiusX;
    int maxX = centerX + radiusX;
    int minY = centerY - radiusY;
    int maxY = centerY + radiusY;
    for (int py = minY; py <= maxY; ++py) {
        if (py < 0 || py >= Hal::DISPLAY_H) continue;
        float dy = (float)(py - centerY) / (float)radiusY;
        for (int px = minX; px <= maxX; ++px) {
            if (px < 0 || px >= Hal::DISPLAY_W) continue;
            float dx = (float)(px - centerX) / (float)radiusX;
            if (dx * dx + dy * dy > 1.0f) continue;
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

void fillTopDownLightAlpha(int height, uint16_t color, uint8_t maxAlpha) {
    if (height <= 0 || maxAlpha == 0) return;
    if (height > Hal::DISPLAY_H) height = Hal::DISPLAY_H;
    auto& c = PixelRenderer::canvas();
    for (int py = 0; py < height; ++py) {
        float t = 1.0f - (float)py / (float)height;
        uint8_t alpha = (uint8_t)(maxAlpha * t * t);
        if (alpha == 0) continue;
        for (int px = 0; px < Hal::DISPLAY_W; ++px) {
            uint16_t bg = (uint16_t)c.readPixel(px, py);
            c.drawPixel(px, py, blendRgb565(bg, color, alpha));
        }
    }
}

float clampf(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

float roomMaxCameraY() {
    float bottom = (float)(roomOriginY() + roomHeight());
    float maxY = bottom - (float)Hal::DISPLAY_H;
    return maxY > 0.0f ? maxY : 0.0f;
}

float cameraForWorldY(float worldY) {
    return clampf(worldY - CAMERA_FOCUS_Y, 0.0f, roomMaxCameraY());
}

int16_t roomPointX(uint8_t index) {
    return room().walkPoint(index).x;
}

int16_t roomPointY(uint8_t index) {
    return room().walkPoint(index).y;
}

int16_t bedPointX(uint8_t index) {
    return room().bedPoint(index).x;
}

int16_t bedPointY(uint8_t index) {
    return room().bedPoint(index).y;
}

bool roomWalkContains(float x, float y) {
    return RoomMovementArea::containsPoint(
        room().walkPolygon(), room().walkPolygonCount(), x, y);
}

bool roomBedContains(float x, float y) {
    uint8_t count = room().bedPolygonCount();
    if (count < 3) return false;

    bool inside = false;
    uint8_t j = count - 1;
    for (uint8_t i = 0; i < count; ++i) {
        float xi = (float)bedPointX(i);
        float yi = (float)bedPointY(i);
        float xj = (float)bedPointX(j);
        float yj = (float)bedPointY(j);
        bool crosses = ((yi > y) != (yj > y)) &&
                       (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (crosses) inside = !inside;
        j = i;
    }
    return inside;
}

bool randomRoomWalkPoint(float& x, float& y) {
    for (uint8_t tries = 0; tries < 48; ++tries) {
        float px = (float)GameRandom::random(roomWalkMinX(), roomWalkMaxX() + 1);
        float py = (float)GameRandom::random(roomWalkMinY(), roomWalkMaxY() + 1);
        if (roomWalkContains(px, py)) {
            x = px;
            y = py;
            return true;
        }
    }

    float centerX = (roomWalkMinX() + roomWalkMaxX()) * 0.5f;
    float centerY = (roomWalkMinY() + roomWalkMaxY()) * 0.5f;
    if (roomWalkContains(centerX, centerY)) {
        x = centerX;
        y = centerY;
        return true;
    }

    if (room().walkPolygonCount() > 0) {
        x = (float)roomPointX(0);
        y = (float)roomPointY(0);
        return true;
    }
    x = centerX;
    y = centerY;
    return false;
}

PokemonSprites::WalkDirection visitorWalkDirectionForDelta(float dx, float dy) {
    if (fabsf(dx) >= fabsf(dy)) {
        return dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                          : PokemonSprites::WalkDirection::LEFT;
    }
    return dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                      : PokemonSprites::WalkDirection::UP;
}

uint16_t visitorIdleDirectionFrameIndex(
    const PmdSpriteConfig* config,
    PokemonSprites::WalkDirection direction,
    bool& flipX) {
    flipX = false;
    switch (direction) {
    case PokemonSprites::WalkDirection::DOWN:
        return 0;
    case PokemonSprites::WalkDirection::LEFT:
        return 2;
    case PokemonSprites::WalkDirection::UP:
        return 4;
    case PokemonSprites::WalkDirection::RIGHT:
        if (config && config->mirrorRightDirections) {
            flipX = true;
            return 2;
        }
        return 6;
    }
    return 0;
}

bool randomRoomWalkPointNear(float centerX, float centerY, float radiusX, float radiusY, float& x, float& y) {
    int spanX = (int)roundf(radiusX);
    int spanY = (int)roundf(radiusY);
    if (spanX < 1) spanX = 1;
    if (spanY < 1) spanY = 1;
    for (uint8_t tries = 0; tries < 32; ++tries) {
        float px = clampf(centerX + (float)GameRandom::random(-spanX, spanX + 1),
                          (float)roomWalkMinX(),
                          (float)roomWalkMaxX());
        float py = clampf(centerY + (float)GameRandom::random(-spanY, spanY + 1),
                          (float)roomWalkMinY(),
                          (float)roomWalkMaxY());
        if (roomWalkContains(px, py)) {
            x = px;
            y = py;
            return true;
        }
    }
    return randomRoomWalkPoint(x, y);
}

float pmdFloatYOffset(const PmdSpriteConfig* config, uint32_t nowMs,
                      bool sleeping = false) {
    if (!config || config->airHeight <= 0.0f || sleeping) return 0.0f;
    uint32_t phaseMs =
        (nowMs + static_cast<uint32_t>(config->speciesId) * 97UL) %
        1600UL;
    float phase = static_cast<float>(phaseMs) * 0.00392699f;
    return config->airHeight + sinf(phase) * config->bobAmplitude;
}

PmdMotionMode pmdMotionModeForConfig(const PmdSpriteConfig* config) {
    if (!config) return PmdMotionMode::LOOP;
    if (PokemonMotion::behaviorForSpecies(config->speciesId).mode ==
        PokemonMotion::Mode::SLITHER) {
        return PmdMotionMode::PINGPONG;
    }
    return config->motionMode;
}

uint8_t pmdWalkingPlaybackFrameCount(const PmdSpriteConfig* config, bool longMove) {
    if (!config || config->walkingFrames == 0) return 1;
    PmdMotionMode motionMode = pmdMotionModeForConfig(config);
    if (motionMode == PmdMotionMode::PINGPONG && config->walkingFrames == 3) return longMove ? 3 : 2;
    if (motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 2) return 2;
    return config->walkingFrames;
}

bool pmdWalkingPlaybackLoops(const PmdSpriteConfig* config) {
    return pmdMotionModeForConfig(config) == PmdMotionMode::LOOP;
}

uint8_t pmdWalkingSpriteFrameIndex(const PmdSpriteConfig* config, uint8_t playbackFrame, bool longMove) {
    if (!config || config->walkingFrames == 0) return 0;
    PmdMotionMode motionMode = pmdMotionModeForConfig(config);
    if (motionMode == PmdMotionMode::PINGPONG && config->walkingFrames == 3) {
        if (PokemonMotion::behaviorForSpecies(config->speciesId).mode ==
            PokemonMotion::Mode::SLITHER) {
            return playbackFrame % config->walkingFrames;
        }
        uint8_t maxFrame = longMove ? 2 : 1;
        return playbackFrame > maxFrame ? maxFrame : playbackFrame;
    }
    if (motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 3) {
        return playbackFrame == 0 ? 0 : 1;
    }
    return playbackFrame % config->walkingFrames;
}

uint8_t pmdStoppingPlaybackFrameCount(const PmdSpriteConfig* config) {
    if (config && pmdMotionModeForConfig(config) == PmdMotionMode::PINGPONG &&
        config->walkingFrames >= 2) return 2;
    return 1;
}

uint8_t pmdStoppingSpriteFrameIndex(const PmdSpriteConfig* config, uint8_t playbackFrame) {
    if (!config || config->walkingFrames == 0) return 0;
    PmdMotionMode motionMode = pmdMotionModeForConfig(config);
    if (motionMode == PmdMotionMode::PINGPONG && config->walkingFrames >= 2) {
        static constexpr uint8_t SEQUENCE[] = {1, 0};
        return SEQUENCE[playbackFrame < 2 ? playbackFrame : 1];
    }
    if (motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 3) {
        return (uint8_t)(config->walkingFrames - 1);
    }
    return 0;
}

bool mainSceneIsNight() {
    uint16_t minutes = GameEngine::ins().gameMinutesOfDay();
    return minutes < 6 * 60 || minutes >= 18 * 60;
}

bool mainSceneIsSleepTime() {
    return GameEngine::ins().isMonsterSleepTime();
}

bool monsterIsSleepTime(const Game::MonsterRuntime& mon) {
    return mon.majorStatus == Game::MajorStatus::SLEEP ||
           Game::isScheduledSleepMinute(
               GameEngine::ins().gameMinutesOfDay(), mon.nature);
}

}

void MainScene::onEnter() {
    VoiceCallService::ins().begin();
    if (GameEngine::ins().localContactVisitActive() &&
        GameEngine::ins().previousScene() == SceneID::EXPLORE &&
        GameEngine::ins().exploreTravelPhase() ==
            ExploreTravelPhase::NONE) {
        GameEngine::ins().restoreContactHostToFront();
    }
    active = &GameEngine::ins().activeSpecies();
    uint32_t nowMs = Hal::ins().millis();
    roomAction = RoomAction::NONE;
    heartEffect = HeartEffect::NONE;
    heartEffectUntilMs = 0;
    hungerAnimUntilMs = 0;
    feedingHadTastyBite = false;
    feedingHadDislikedBite = false;
    feedingBecameFull = false;
    feedingMoodAfter = 0;
    bowlEaterSlot = -1;
    visitorFeedingBiteMs = 0;
    visitorFeedingUntilMs = 0;
    visitorBedYieldHandled = false;
    progressionModal = ProgressionModal::NONE;
    progressionCancelledSpeciesId = 0;
    ProgressionUi::resetMoveLearnState(progressionMoveLearn);
    contactDialog = ContactDialog::NONE;
    contactGuestMotion = ContactGuestMotion::NONE;
    contactGuestMotionStartedMs = 0;
    contactDialogYes = true;
    pairInteraction = PairInteraction::NONE;
    pairInteractionPhase = PairInteractionPhase::NONE;
    pairForcedPlay = false;
    pairForcedChase = false;
    pairLegsRemaining = 0;
    pairPhaseStartedMs = 0;
    pairPhaseUntilMs = 0;
    pairInteractionUntilMs = 0;
    pairFollowerDelayUntilMs = 0;
    pairNextRouteAttemptMs = 0;
    behaviorProfile = behaviorProfileFor(*active, GameEngine::ins().activeMonster());
    mind.reset(nowMs);
    nextMindUpdateMs = nowMs;
    restoreViewState(nowMs);
    bool mayBeAtBed =
        behaviorProfile.movementMode != MonsterMovementMode::STATIONARY &&
        (aiMode == AiMode::RESTING || aiMode == AiMode::WAKING ||
         aiMode == AiMode::LEAVING_BED);
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(mayBeAtBed && (aiMode == AiMode::LEAVING_BED || monsterAtBedSleepPose()))) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
    }
    velocityX = 0.0f;
    velocityY = 0.0f;
    cameraY = cameraForWorldY(monsterY);
    pmdLongMove = false;
    pmdMotionPhase = PokemonMotion::SLITHER_IDLE_PHASE_INDEX;
    pmdRenderOffsetX = 0;
    pmdRenderOffsetY = 0;
    pmdMotionCycleMs = PokemonMotion::SLITHER_AMBIENT_MAX_CYCLE_MS;
    pmdFrameStartedMs = nowMs;
    clearMoveRoute();
    if (aiMode == AiMode::WANDER || aiMode == AiMode::SEEK_FOOD ||
        aiMode == AiMode::SEEK_BED || aiMode == AiMode::LEAVING_BED) {
        if (!buildMoveRoute(targetX, targetY)) aiMode = AiMode::IDLE;
    }
    if (nextAiDecisionMs == 0) nextAiDecisionMs = nowMs;
    scheduleAttention(nowMs, true);
    scheduleSpecialAction(nowMs);
    visitor.active = false;
#if STICKMON_ENABLE_TRACE_LOGS
    nextMainAiTraceMs = 0;
    mainAiTraceInitialized = false;
#endif
    nextVisitorSleepTraceMs = 0;
    visitorMoveBlockedSinceMs = 0;
    visitorNextReplanMs = 0;
    visitorFoodRouteFailure = FoodRouteFailure{};
    mainYieldingForVisitorFood = false;
    if (visitorHostActive()) {
        const Game::MonsterRuntime& guest = GameEngine::ins().gameState().team[1];
        const SecondarySceneViewState& saved =
            GameEngine::ins().mainSceneViewState().secondary;
        if (restoreVisitorViewState(saved, guest, nowMs)) {
            if (visitor.state == VisitorState::IDLE) {
                visitor.stateUntilMs = nowMs + 1000;
            } else if (visitor.state == VisitorState::SLEEPING &&
                       !monsterIsSleepTime(guest)) {
                logVisitorSleepEvent(
                    "resume_wake_schedule", nowMs, guest);
                visitor.state = VisitorState::IDLE;
                visitor.stateUntilMs = nowMs + 1000;
            } else if (visitor.state == VisitorState::YIELDING_BED &&
                       (!monsterIsSleepTime(guest) ||
                        !buildVisitorMoveRoute(
                            visitor.targetX, visitor.targetY))) {
                logVisitorSleepEvent(
                    monsterIsSleepTime(guest)
                        ? "resume_yield_route_invalid"
                        : "resume_yield_schedule",
                    nowMs, guest);
                visitor.state = VisitorState::IDLE;
                visitor.stateUntilMs = nowMs + 1000;
            }
            if (visitor.state == VisitorState::SLEEPING) {
                logVisitorSleepEvent("resume", nowMs, guest);
                nextVisitorSleepTraceMs =
                    nowMs + VISITOR_SLEEP_TRACE_INTERVAL_MS;
            }
        } else {
            spawnVisitor(nowMs, false);
        }
        if (guest.fainted || guest.hpCur == 0) {
            restFaintedVisitor(nowMs);
        }
    }
    visitorBedYieldHandled =
        aiMode == AiMode::RESTING || aiMode == AiMode::WAKING ||
        monsterAtBedSleepPose();
    bool teamMemberArrival =
        GameEngine::ins().consumeTeamMemberArrivalRequest();
    beginDoorTransition(nowMs);
    if (teamMemberArrival && doorTransition == DoorTransitionMode::NONE) {
        beginTeamMemberEntry(nowMs);
    }
    if (GameEngine::ins().localContactVisitActive() &&
        GameEngine::ins().contactVisitExploring() &&
        GameEngine::ins().exploreTravelPhase() == ExploreTravelPhase::NONE &&
        GameEngine::ins().previousScene() == SceneID::EXPLORE) {
        GameEngine::ins().requestContactVisitFarewell();
    }
    if (!GameEngine::ins().localContactVisitActive() &&
        (GameEngine::ins().contactKnockPending() ||
         GameEngine::ins().prepareDailyContactVisit())) {
        contactDialog = ContactDialog::KNOCK;
    }
    pairForcedChase =
        GameEngine::ins().consumeDebugPairInteractionRequest();
    schedulePairInteraction(nowMs, pairForcedChase);
}

void MainScene::onExit() {
    uint32_t nowMs = Hal::ins().millis();
    bowlEaterSlot = -1;
    cancelPairInteraction(nowMs);
    VoiceCallService::ins().stopListening();
    if (doorTransition == DoorTransitionMode::NONE) {
        persistViewState(nowMs);
    } else if (visitorDepartureSnapshotValid &&
               GameEngine::ins().exploreTravelPhase() ==
                   ExploreTravelPhase::DEPARTING) {
        MainSceneViewState saved = GameEngine::ins().mainSceneViewState();
        const Game::GameState& state = GameEngine::ins().gameState();
        if (state.teamCount >= 2 && visitorBeforeDoorDeparture.active) {
            persistVisitorViewState(saved.secondary,
                                    visitorBeforeDoorDeparture,
                                    state.team[1], nowMs);
        } else {
            saved.secondary = SecondarySceneViewState{};
        }
        GameEngine::ins().saveMainSceneViewState(saved);
    }
}

void MainScene::onBeforeSave() {
    if (doorTransition == DoorTransitionMode::NONE) {
        persistViewState(Hal::ins().millis());
    }
}

void MainScene::restoreViewState(uint32_t nowMs) {
    const MainSceneViewState& saved = GameEngine::ins().mainSceneViewState();
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool currentlyFainted = mon.fainted || mon.hpCur == 0;
    if (!saved.valid || !active || saved.speciesId != active->id) {
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        pmdAction = PmdAction::IDLE;
        pmdDirection = PmdDirection::FRONT;
        pmdFrame = 0;
        facingRight = true;
        nextAiDecisionMs = nowMs;
        postFeedAwakeUntilMs = 0;
        faintRestActive = currentlyFainted;
        return;
    }

    monsterX = saved.monsterX;
    monsterY = saved.monsterY;
    targetX = saved.targetX;
    targetY = saved.targetY;
    facingRight = saved.facingRight;
    pmdDirection = saved.pmdDirection <= (uint8_t)PmdDirection::DOWN_RIGHT
        ? (PmdDirection)saved.pmdDirection
        : PmdDirection::FRONT;
    pmdFrame = saved.pmdFrame;
    aiMode = saved.aiMode <= (uint8_t)AiMode::RESTING
        ? (AiMode)saved.aiMode
        : AiMode::IDLE;
    if (aiMode == AiMode::TURNING) aiMode = AiMode::IDLE;
    if (aiMode == AiMode::WAKING) aiMode = AiMode::RESTING;
    if (aiMode == AiMode::FEEDING) aiMode = AiMode::IDLE;
    pmdAction = aiMode == AiMode::RESTING ? PmdAction::SLEEPING : PmdAction::IDLE;
    nextAiDecisionMs = nowMs + saved.nextDecisionRemainingMs;
    postFeedAwakeUntilMs = saved.postFeedAwakeRemainingMs == 0
        ? 0
        : nowMs + saved.postFeedAwakeRemainingMs;
    faintRestActive = saved.faintRestActive || currentlyFainted;
}

bool MainScene::restoreVisitorViewState(
    const SecondarySceneViewState& saved,
    const Game::MonsterRuntime& monster,
    uint32_t nowMs) {
    bool sameMonster = saved.valid &&
        saved.speciesId == monster.speciesId &&
        saved.ivPacked == monster.ivPacked &&
        saved.metAt == monster.metAt &&
        saved.nature == monster.nature &&
        saved.metArea == monster.metArea &&
        saved.origin == static_cast<uint8_t>(monster.origin);
    if (!sameMonster || saved.state > static_cast<uint8_t>(VisitorState::SLEEPING) ||
        saved.direction > static_cast<uint8_t>(PokemonSprites::WalkDirection::RIGHT)) {
        return false;
    }

    visitor = VisitorActor{};
    visitor.active = true;
    visitor.speciesId = monster.speciesId;
    visitor.x = saved.x;
    visitor.y = saved.y;
    visitor.targetX = saved.targetX;
    visitor.targetY = saved.targetY;
    visitor.sleepX = saved.sleepX;
    visitor.sleepY = saved.sleepY;
    visitor.sleepSpotValid = saved.sleepSpotValid;
    visitor.state = static_cast<VisitorState>(saved.state);
    if (visitor.state == VisitorState::SEEK_FOOD ||
        visitor.state == VisitorState::FEEDING) {
        visitor.state = VisitorState::IDLE;
    }
    visitor.direction =
        static_cast<PokemonSprites::WalkDirection>(saved.direction);
    visitor.frameIndex = saved.frameIndex;
    visitor.facingRight = saved.facingRight;
    visitor.stateUntilMs = nowMs + saved.stateRemainingMs;
    visitor.foodRetryAfterMs = nowMs + saved.foodRetryRemainingMs;
    visitor.frameStartedMs = nowMs;
    visitor.dropOffsetY = 0.0f;
    clearVisitorMoveRoute();

    bool needsRoute = visitor.state == VisitorState::WALK ||
        visitor.state == VisitorState::GO_TO_SLEEP ||
        visitor.state == VisitorState::YIELDING_BED;
    if (needsRoute &&
        !buildVisitorMoveRoute(visitor.targetX, visitor.targetY)) {
        visitor.state = VisitorState::IDLE;
        visitor.targetX = visitor.x;
        visitor.targetY = visitor.y;
        visitor.stateUntilMs = nowMs + 1000;
    }
    return true;
}

void MainScene::persistVisitorViewState(
    SecondarySceneViewState& saved,
    const VisitorActor& actor,
    const Game::MonsterRuntime& monster,
    uint32_t nowMs) const {
    saved = SecondarySceneViewState{};
    if (!actor.active || actor.speciesId != monster.speciesId) return;
    saved.valid = true;
    saved.speciesId = monster.speciesId;
    saved.ivPacked = monster.ivPacked;
    saved.metAt = monster.metAt;
    saved.nature = monster.nature;
    saved.metArea = monster.metArea;
    saved.origin = static_cast<uint8_t>(monster.origin);
    saved.x = actor.x;
    saved.y = actor.y;
    saved.targetX = actor.targetX;
    saved.targetY = actor.targetY;
    saved.sleepX = actor.sleepX;
    saved.sleepY = actor.sleepY;
    VisitorState storedState = actor.state;
    if (storedState == VisitorState::SEEK_FOOD ||
        storedState == VisitorState::FEEDING) {
        storedState = VisitorState::IDLE;
        saved.targetX = actor.x;
        saved.targetY = actor.y;
    }
    saved.state = static_cast<uint8_t>(storedState);
    saved.direction = static_cast<uint8_t>(actor.direction);
    saved.frameIndex = actor.frameIndex;
    saved.facingRight = actor.facingRight;
    saved.sleepSpotValid = actor.sleepSpotValid;
    saved.stateRemainingMs =
        static_cast<int32_t>(actor.stateUntilMs - nowMs) > 0
            ? actor.stateUntilMs - nowMs
            : 0;
    saved.foodRetryRemainingMs =
        static_cast<int32_t>(actor.foodRetryAfterMs - nowMs) > 0
            ? actor.foodRetryAfterMs - nowMs
            : 0;
}

void MainScene::persistViewState(uint32_t nowMs) {
    if (!active) return;
    MainSceneViewState saved;
    saved.valid = true;
    saved.speciesId = active->id;
    saved.monsterX = monsterX;
    saved.monsterY = monsterY;
    AiMode storedMode = aiMode;
    if (storedMode == AiMode::TURNING) storedMode = turnNextMode;
    if (storedMode == AiMode::WAKING) storedMode = AiMode::RESTING;
    bool transientMove = storedMode == AiMode::SCRIPTED_MOVE || roomAction != RoomAction::NONE;
    if (storedMode == AiMode::FEEDING || storedMode == AiMode::SCRIPTED_MOVE) {
        storedMode = AiMode::IDLE;
    }
    saved.targetX = transientMove ? monsterX : targetX;
    saved.targetY = transientMove ? monsterY : targetY;
    saved.aiMode = (uint8_t)storedMode;
    saved.pmdAction = (uint8_t)pmdAction;
    saved.pmdDirection = (uint8_t)pmdDirection;
    saved.pmdFrame = pmdFrame;
    saved.facingRight = facingRight;
    saved.faintRestActive = faintRestActive;
    saved.nextDecisionRemainingMs = (int32_t)(nextAiDecisionMs - nowMs) > 0
        ? nextAiDecisionMs - nowMs
        : 0;
    saved.postFeedAwakeRemainingMs = (int32_t)(postFeedAwakeUntilMs - nowMs) > 0
        ? postFeedAwakeUntilMs - nowMs
        : 0;
    const Game::GameState& state = GameEngine::ins().gameState();
    if (state.teamCount >= 2 && visitor.active) {
        persistVisitorViewState(saved.secondary, visitor, state.team[1], nowMs);
    }
    GameEngine::ins().saveMainSceneViewState(saved);
}

SceneUpdateResult MainScene::update(uint32_t nowMs, float dtSeconds) {
#if STICKMON_ENABLE_TRACE_LOGS
    uint32_t perfStartedUs = Platform::clock().micros();
    uint32_t perfVisitorUs = perfStartedUs;
    uint32_t perfVoiceUs = perfStartedUs;
    uint32_t perfLogicUs = perfStartedUs;
    uint32_t perfCameraUs = perfStartedUs;
    auto logMainPerf = [&](const char* exitPhase) {
        uint32_t endedUs = Platform::clock().micros();
        uint32_t totalUs = endedUs - perfStartedUs;
        if (totalUs < 3000) return;
        STICKMON_TRACEF(
            "[MainPerf] t=%lu phase=%s total_us=%lu visitor_us=%lu "
            "voice_us=%lu logic_us=%lu camera_us=%lu mode=%u vstate=%u "
            "route=%u/%u vroute=%u/%u\n",
            static_cast<unsigned long>(nowMs), exitPhase,
            static_cast<unsigned long>(totalUs),
            static_cast<unsigned long>(perfVisitorUs - perfStartedUs),
            static_cast<unsigned long>(perfVoiceUs - perfVisitorUs),
            static_cast<unsigned long>(perfLogicUs - perfVoiceUs),
            static_cast<unsigned long>(perfCameraUs - perfLogicUs),
            static_cast<unsigned>(aiMode), static_cast<unsigned>(visitor.state),
            static_cast<unsigned>(moveRouteIndex),
            static_cast<unsigned>(moveRouteCount),
            static_cast<unsigned>(visitorMoveRouteIndex),
            static_cast<unsigned>(visitorMoveRouteCount));
    };
#endif
    const Species* nextActive = &GameEngine::ins().activeSpecies();
    if (!active || active->id != nextActive->id) {
        behaviorProfile = behaviorProfileFor(*nextActive, GameEngine::ins().activeMonster());
    }
    active = nextActive;
    if (doorTransition == DoorTransitionMode::NONE &&
        contactGuestMotion == ContactGuestMotion::NONE &&
        pairInteraction == PairInteraction::NONE) {
        updateVisitor(nowMs, dtSeconds);
    }
#if STICKMON_ENABLE_TRACE_LOGS
    perfVisitorUs = Platform::clock().micros();
#endif
    auto& voice = VoiceCallService::ins();
    const auto& settings = GameEngine::ins().gameState().settings;
    const auto& voiceMonster = GameEngine::ins().activeMonster();
    bool usesBed =
        Game::speciesCareProfileFor(voiceMonster.speciesId).usesBed;
    bool voiceCanRespond = !voiceMonster.fainted && voiceMonster.hpCur > 0 &&
                           voiceMonster.majorStatus != Game::MajorStatus::SLEEP &&
                           aiMode != AiMode::RESTING && aiMode != AiMode::WAKING &&
                           aiMode != AiMode::SEEK_BED &&
                           (!usesBed || !mainSceneIsSleepTime());
    bool voiceAvailable = settings.voiceCallEnabled && voice.profileReady() &&
                          !GameEngine::ins().idleModeActive() &&
                          voiceCanRespond &&
                          doorTransition == DoorTransitionMode::NONE &&
                          progressionModal == ProgressionModal::NONE &&
                          contactDialog == ContactDialog::NONE &&
                          contactGuestMotion == ContactGuestMotion::NONE &&
                          pairInteraction == PairInteraction::NONE &&
                          !Hal::ins().audioPlaying();
    if (voiceAvailable) {
        if (!voice.listening() && roomAction != RoomAction::VOICE_CALL_APPROACH &&
            roomAction != RoomAction::VOICE_CALL_WAIT) {
            voice.startListening(nowMs);
        }
        voice.updateListening(nowMs);
        if (voice.consumeMatch()) {
            GameEngine::ins().wakeFromIdle();
            startVoiceCallReaction(nowMs);
        }
    } else {
        voice.stopListening();
    }
#if STICKMON_ENABLE_TRACE_LOGS
    perfVoiceUs = Platform::clock().micros();
#endif
    if (doorTransition == DoorTransitionMode::NONE) {
        if (progressionModal != ProgressionModal::NONE || openPendingProgression()) {
            cancelPairInteraction(nowMs);
            velocityX = 0.0f;
            velocityY = 0.0f;
#if STICKMON_ENABLE_TRACE_LOGS
            perfLogicUs = Platform::clock().micros();
#endif
            updateCamera();
            updatePmdSpriteState(nowMs);
#if STICKMON_ENABLE_TRACE_LOGS
            perfCameraUs = Platform::clock().micros();
#endif
            traceMainAi(nowMs, MainAiTraceGate::PROGRESSION);
            logMainPerf("progression_pre");
            return SceneUpdateResult::animate(66);
        }
        updateContactVisit(nowMs, dtSeconds);
        if (contactDialog != ContactDialog::NONE ||
            contactGuestMotion != ContactGuestMotion::NONE) {
            cancelPairInteraction(nowMs);
            velocityX = 0.0f;
            velocityY = 0.0f;
#if STICKMON_ENABLE_TRACE_LOGS
            perfLogicUs = Platform::clock().micros();
#endif
            updateCamera();
            updatePmdSpriteState(nowMs);
#if STICKMON_ENABLE_TRACE_LOGS
            perfCameraUs = Platform::clock().micros();
#endif
            traceMainAi(nowMs, MainAiTraceGate::CONTACT);
            logMainPerf("contact");
            return SceneUpdateResult::animate(66);
        }
        if (updatePairInteraction(nowMs, dtSeconds)) {
#if STICKMON_ENABLE_TRACE_LOGS
            perfLogicUs = Platform::clock().micros();
#endif
            updateCamera();
            updatePmdSpriteState(nowMs);
#if STICKMON_ENABLE_TRACE_LOGS
            perfCameraUs = Platform::clock().micros();
#endif
            traceMainAi(nowMs, MainAiTraceGate::PAIR);
            logMainPerf("pair");
            return SceneUpdateResult::animate(66);
        }
        updateMonsterAi(nowMs, dtSeconds);
        if (openPendingProgression()) {
            velocityX = 0.0f;
            velocityY = 0.0f;
#if STICKMON_ENABLE_TRACE_LOGS
            perfLogicUs = Platform::clock().micros();
#endif
            updateCamera();
            updatePmdSpriteState(nowMs);
#if STICKMON_ENABLE_TRACE_LOGS
            perfCameraUs = Platform::clock().micros();
#endif
            traceMainAi(nowMs, MainAiTraceGate::PROGRESSION);
            logMainPerf("progression_post");
            return SceneUpdateResult::animate(66);
        }
    } else {
        updateDoorTransition(nowMs);
        traceMainAi(nowMs, MainAiTraceGate::DOOR_TRANSITION);
    }
#if STICKMON_ENABLE_TRACE_LOGS
    perfLogicUs = Platform::clock().micros();
#endif
    updateCamera();
    updatePmdSpriteState(nowMs);
#if STICKMON_ENABLE_TRACE_LOGS
    perfCameraUs = Platform::clock().micros();
#endif
    if (doorTransition != DoorTransitionMode::NONE) {
 #if STICKMON_ENABLE_TRACE_LOGS
        logMainPerf("door");
 #endif
        return SceneUpdateResult::animate(66);
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    bool combo =
        ButtonDispatcher::ins().isDown(Platform::InputButton::PRIMARY) &&
        ButtonDispatcher::ins().isDown(Platform::InputButton::SECONDARY);
    if (!combo) {
        comboStartMs = 0;
        comboSaved = false;
    } else if (comboStartMs == 0) {
        comboStartMs = nowMs;
    } else if (!comboSaved && nowMs - comboStartMs >= 800) {
        GameEngine::ins().saveNow();
        toast = Ui::Common::SAVED;
        toastUntil = nowMs + 1200;
        comboSaved = true;
    }
#endif
#if STICKMON_ENABLE_TRACE_LOGS
    logMainPerf("normal");
#endif
    if (toast && static_cast<int32_t>(nowMs - toastUntil) >= 0) {
        toast = nullptr;
    }
    return SceneUpdateResult::animate(66);
}

void MainScene::beginDoorTransition(uint32_t nowMs) {
    ExploreTravelPhase phase = GameEngine::ins().exploreTravelPhase();
    if (phase != ExploreTravelPhase::DEPARTING &&
        phase != ExploreTravelPhase::RETURNING &&
        phase != ExploreTravelPhase::RETURNING_FAINTED) {
        return;
    }

    cancelPairInteraction(nowMs);
    cancelRoomAction(nowMs);
    bowlEaterSlot = -1;
    visitorFeedingBiteMs = 0;
    visitorFeedingUntilMs = 0;
    clearMoveRoute();
    feedingConsumed = false;
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::IDLE;
    doorLastUpdateMs = nowMs;
    doorPhaseStartedMs = nowMs;
    doorLastProgressMs = nowMs;
    doorRouteEnteringWalkArea = false;
    visitorDoorRouteEnteringWalkArea = false;
    doorDepartureHasVisitor = false;
    doorWaitingActorReady = false;
    doorMainHidden = false;
    doorVisitorHidden = false;
    doorFirstActor = DoorActor::MAIN;
    visitorDepartureSnapshotValid = false;
    clearVisitorMoveRoute();

    const Game::MonsterRuntime& mainMonster =
        GameEngine::ins().activeMonster();
    bool mainFainted = mainMonster.fainted || mainMonster.hpCur == 0;
    if (phase == ExploreTravelPhase::RETURNING_FAINTED ||
        (phase == ExploreTravelPhase::RETURNING && mainFainted)) {
        faintRestActive = true;
        snapMonsterToBed();
        aiMode = AiMode::RESTING;
        pmdAction = PmdAction::SLEEPING;
        doorTransition = DoorTransitionMode::FAINT_WAIT_FADE;
        return;
    }

    if (phase == ExploreTravelPhase::DEPARTING && mainFainted) {
        doorMainHidden = true;
        doorVisitorHidden = true;
        finishDoorDeparture();
        return;
    }

    if (!prepareDoorAnchors()) {
        if (phase == ExploreTravelPhase::DEPARTING) {
            doorTransition = DoorTransitionMode::EXIT_FADE;
            if (!GameEngine::ins().fadeToScene(SceneID::EXPLORE)) {
                GameEngine::ins().requestScene(SceneID::EXPLORE);
            }
        } else {
            randomMonsterCenterWalkPoint(monsterX, monsterY);
            targetX = monsterX;
            targetY = monsterY;
            doorTransition = DoorTransitionMode::ENTER_WAIT_FADE;
        }
        return;
    }

    if (phase == ExploreTravelPhase::DEPARTING) {
        doorDepartureHasVisitor = visitorCanUseDoor();
        if (doorDepartureHasVisitor) {
            visitorBeforeDoorDeparture = visitor;
            visitorDepartureSnapshotValid = true;

            float mainDx = doorInsideX - monsterX;
            float mainDy = doorInsideY - monsterY;
            float visitorDx = doorInsideX - visitor.x;
            float visitorDy = visitorDoorInsideY - visitor.y;
            doorFirstActor = visitorReachesDoorFirst(
                mainDx * mainDx + mainDy * mainDy,
                visitorDx * visitorDx + visitorDy * visitorDy)
                ? DoorActor::VISITOR
                : DoorActor::MAIN;

            float waitingX = doorFirstActor == DoorActor::MAIN ? visitor.x : monsterX;
            float waitingY = doorFirstActor == DoorActor::MAIN ? visitor.y : monsterY;
            const ActorGeometry waitingGeometry =
                doorFirstActor == DoorActor::MAIN
                    ? visitorGeometry() : mainGeometry();
            if (!chooseDoorWaitPose(waitingX, waitingY, waitingGeometry,
                                    doorWaitX, doorWaitY)) {
                doorDepartureHasVisitor = false;
                doorFirstActor = DoorActor::MAIN;
            }
        }

        float mainGoalX = doorDepartureHasVisitor && doorFirstActor == DoorActor::VISITOR
            ? doorWaitX
            : doorInsideX;
        float mainGoalY = doorDepartureHasVisitor && doorFirstActor == DoorActor::VISITOR
            ? doorWaitY
            : doorInsideY;
        targetX = mainGoalX;
        targetY = mainGoalY;
        bool mainRouteReady = buildMoveRoute(mainGoalX, mainGoalY);

        bool visitorRouteReady = true;
        if (doorDepartureHasVisitor) {
            visitor.targetX = doorFirstActor == DoorActor::MAIN ? doorWaitX : doorInsideX;
            visitor.targetY = doorFirstActor == DoorActor::MAIN
                ? doorWaitY : visitorDoorInsideY;
            visitor.state = VisitorState::WALK;
            visitor.frameStartedMs = nowMs;
            visitor.frameIndex = 0;
            visitorRouteReady = buildVisitorMoveRoute(visitor.targetX, visitor.targetY);
        }

        if (!mainRouteReady) {
            monsterX = mainGoalX;
            monsterY = mainGoalY;
            clearMoveRoute();
        }
        if (!visitorRouteReady) {
            visitor.x = visitor.targetX;
            visitor.y = visitor.targetY;
            clearVisitorMoveRoute();
        }
        if (!mainRouteReady || !visitorRouteReady) {
            Platform::logf(
                "[DoorDeparture] route fallback main=%u visitor=%u paired=%u\n",
                mainRouteReady ? 1 : 0, visitorRouteReady ? 1 : 0,
                doorDepartureHasVisitor ? 1 : 0);
        }

        doorRouteEnteringWalkArea = !monsterFootprintInsideWalkArea(monsterX, monsterY);
        visitorDoorRouteEnteringWalkArea =
            doorDepartureHasVisitor &&
            !actorFootprintInsideWalkArea(
                visitor.x, visitor.y, visitorGeometry());
        doorTransition = DoorTransitionMode::EXIT_ROUTE;
        return;
    }

    monsterX = doorOutsideX;
    monsterY = doorOutsideY;
    targetX = doorInsideX;
    targetY = doorInsideY;
    pmdAction = PmdAction::IDLE;
    pmdFrame = 0;
    doorDepartureHasVisitor = visitorCanUseDoor();
    if (doorDepartureHasVisitor) {
        if (!chooseDoorWaitPose(monsterX, monsterY, mainGeometry(),
                                doorWaitX, doorWaitY)) {
            doorDepartureHasVisitor = false;
        } else {
            visitor.x = doorOutsideX;
            visitor.y = visitorDoorOutsideY;
            visitor.targetX = doorInsideX;
            visitor.targetY = visitorDoorInsideY;
            visitor.state = VisitorState::IDLE;
            visitor.frameStartedMs = nowMs;
            visitor.frameIndex = 0;
            doorVisitorHidden = true;
        }
    }
    doorTransition = DoorTransitionMode::ENTER_WAIT_FADE;
}

void MainScene::updateDoorTransition(uint32_t nowMs) {
    uint32_t elapsedMs = doorLastUpdateMs == 0 || nowMs < doorLastUpdateMs
        ? 0
        : nowMs - doorLastUpdateMs;
    doorLastUpdateMs = nowMs;
    float dtSeconds = MathUtil::min<uint32_t>(120, elapsedMs) / 1000.0f;

    switch (doorTransition) {
    case DoorTransitionMode::EXIT_ROUTE:
        if (nowMs - doorPhaseStartedMs >= DOOR_ROUTE_TIMEOUT_MS) {
            if (doorDepartureHasVisitor && !doorWaitingActorReady) {
                DoorActor waitingActor = doorFirstActor == DoorActor::MAIN
                    ? DoorActor::VISITOR
                    : DoorActor::MAIN;
                if (waitingActor == DoorActor::MAIN) {
                    monsterX = doorWaitX;
                    monsterY = doorWaitY;
                    clearMoveRoute();
                } else {
                    visitor.x = doorWaitX;
                    visitor.y = doorWaitY;
                    clearVisitorMoveRoute();
                }
                doorWaitingActorReady = true;
                doorPhaseStartedMs = nowMs;
                doorLastProgressMs = nowMs;
                Platform::logLine(
                    "[DoorDeparture] waiting route timeout fallback=wait_pose");
                return;
            }
            if (doorFirstActor == DoorActor::MAIN) {
                monsterX = doorInsideX;
                monsterY = doorInsideY;
                targetX = doorOutsideX;
                targetY = doorOutsideY;
                clearMoveRoute();
            } else {
                visitor.x = doorInsideX;
                visitor.y = visitorDoorInsideY;
                visitor.targetX = doorOutsideX;
                visitor.targetY = visitorDoorOutsideY;
                clearVisitorMoveRoute();
            }
            Platform::logLine(
                "[DoorDeparture] first route timeout fallback=cross");
            doorTransition = DoorTransitionMode::EXIT_CROSS;
            doorPhaseStartedMs = nowMs;
            return;
        }

        if (doorDepartureHasVisitor && !doorWaitingActorReady) {
            DoorActor waitingActor = doorFirstActor == DoorActor::MAIN
                ? DoorActor::VISITOR
                : DoorActor::MAIN;
            float beforeX = waitingActor == DoorActor::MAIN
                ? monsterX : visitor.x;
            float beforeY = waitingActor == DoorActor::MAIN
                ? monsterY : visitor.y;
            doorWaitingActorReady = updateDoorWaitingActor(dtSeconds);
            float afterX = waitingActor == DoorActor::MAIN
                ? monsterX : visitor.x;
            float afterY = waitingActor == DoorActor::MAIN
                ? monsterY : visitor.y;
            if (fabsf(afterX - beforeX) > 0.01f ||
                fabsf(afterY - beforeY) > 0.01f) {
                doorLastProgressMs = nowMs;
            }
            if (!doorWaitingActorReady &&
                nowMs - doorLastProgressMs >= DOOR_STALL_TIMEOUT_MS) {
                if (waitingActor == DoorActor::MAIN) {
                    monsterX = doorWaitX;
                    monsterY = doorWaitY;
                    clearMoveRoute();
                } else {
                    visitor.x = doorWaitX;
                    visitor.y = doorWaitY;
                    clearVisitorMoveRoute();
                }
                doorWaitingActorReady = true;
                Platform::logLine(
                    "[DoorDeparture] waiting route stalled fallback=wait_pose");
            }
            if (!doorWaitingActorReady) return;
            // The first actor gets its own timeout budget after the waiting
            // actor has cleared the doorway.
            doorPhaseStartedMs = nowMs;
            doorLastProgressMs = nowMs;
        }
        {
            float beforeX = doorFirstActor == DoorActor::MAIN
                ? monsterX : visitor.x;
            float beforeY = doorFirstActor == DoorActor::MAIN
                ? monsterY : visitor.y;
            bool firstReady = doorFirstActor == DoorActor::MAIN
                ? updateDoorRoute(dtSeconds)
                : updateVisitorDoorRoute(dtSeconds);
            float afterX = doorFirstActor == DoorActor::MAIN
                ? monsterX : visitor.x;
            float afterY = doorFirstActor == DoorActor::MAIN
                ? monsterY : visitor.y;
            if (fabsf(afterX - beforeX) > 0.01f ||
                fabsf(afterY - beforeY) > 0.01f) {
                doorLastProgressMs = nowMs;
            }
            if (!firstReady &&
                nowMs - doorLastProgressMs >= DOOR_STALL_TIMEOUT_MS) {
                if (doorFirstActor == DoorActor::MAIN) {
                    monsterX = doorInsideX;
                    monsterY = doorInsideY;
                    targetX = doorOutsideX;
                    targetY = doorOutsideY;
                    clearMoveRoute();
                } else {
                    visitor.x = doorInsideX;
                    visitor.y = visitorDoorInsideY;
                    visitor.targetX = doorOutsideX;
                    visitor.targetY = visitorDoorOutsideY;
                    clearVisitorMoveRoute();
                }
                Platform::logLine(
                    "[DoorDeparture] first route stalled fallback=cross");
                doorTransition = DoorTransitionMode::EXIT_CROSS;
                doorPhaseStartedMs = nowMs;
                return;
            }
            if (!firstReady) return;
            if (doorFirstActor == DoorActor::MAIN) {
                targetX = doorOutsideX;
                targetY = doorOutsideY;
            } else {
                visitor.targetX = doorOutsideX;
                visitor.targetY = visitorDoorOutsideY;
            }
            doorTransition = DoorTransitionMode::EXIT_CROSS;
            doorPhaseStartedMs = nowMs;
        }
        return;
    case DoorTransitionMode::EXIT_CROSS:
        {
            bool firstOutside = doorFirstActor == DoorActor::MAIN
                ? moveDoorToward(doorOutsideX, doorOutsideY, DOOR_CROSS_SPEED,
                                 dtSeconds, false, doorDepartureHasVisitor)
                : moveVisitorDoorToward(doorOutsideX, visitorDoorOutsideY, DOOR_CROSS_SPEED,
                                        dtSeconds, false, doorDepartureHasVisitor);
            if (!firstOutside) return;
            velocityX = velocityY = 0.0f;
            if (doorFirstActor == DoorActor::MAIN) {
                doorMainHidden = true;
            } else {
                doorVisitorHidden = true;
            }
            beginSecondDoorExit(nowMs);
        }
        return;
    case DoorTransitionMode::EXIT_SECOND_ROUTE: {
        if (nowMs - doorPhaseStartedMs >= DOOR_ROUTE_TIMEOUT_MS) {
            DoorActor secondActor = doorFirstActor == DoorActor::MAIN
                ? DoorActor::VISITOR
                : DoorActor::MAIN;
            if (secondActor == DoorActor::MAIN) {
                monsterX = doorInsideX;
                monsterY = doorInsideY;
                targetX = doorOutsideX;
                targetY = doorOutsideY;
                clearMoveRoute();
            } else {
                visitor.x = doorInsideX;
                visitor.y = visitorDoorInsideY;
                visitor.targetX = doorOutsideX;
                visitor.targetY = visitorDoorOutsideY;
                clearVisitorMoveRoute();
            }
            Platform::logLine(
                "[DoorDeparture] second route timeout fallback=cross");
            doorTransition = DoorTransitionMode::EXIT_SECOND_CROSS;
            doorPhaseStartedMs = nowMs;
            return;
        }
        DoorActor secondActor = doorFirstActor == DoorActor::MAIN
            ? DoorActor::VISITOR
            : DoorActor::MAIN;
        float beforeX = secondActor == DoorActor::MAIN
            ? monsterX : visitor.x;
        float beforeY = secondActor == DoorActor::MAIN
            ? monsterY : visitor.y;
        bool secondReady = secondActor == DoorActor::MAIN
            ? updateDoorRoute(dtSeconds)
            : updateVisitorDoorRoute(dtSeconds);
        float afterX = secondActor == DoorActor::MAIN
            ? monsterX : visitor.x;
        float afterY = secondActor == DoorActor::MAIN
            ? monsterY : visitor.y;
        if (fabsf(afterX - beforeX) > 0.01f ||
            fabsf(afterY - beforeY) > 0.01f) {
            doorLastProgressMs = nowMs;
        }
        if (!secondReady &&
            nowMs - doorLastProgressMs >= DOOR_STALL_TIMEOUT_MS) {
            if (secondActor == DoorActor::MAIN) {
                monsterX = doorInsideX;
                monsterY = doorInsideY;
                targetX = doorOutsideX;
                targetY = doorOutsideY;
                clearMoveRoute();
            } else {
                visitor.x = doorInsideX;
                visitor.y = visitorDoorInsideY;
                visitor.targetX = doorOutsideX;
                visitor.targetY = visitorDoorOutsideY;
                clearVisitorMoveRoute();
            }
            Platform::logLine(
                "[DoorDeparture] second route stalled fallback=cross");
            doorTransition = DoorTransitionMode::EXIT_SECOND_CROSS;
            doorPhaseStartedMs = nowMs;
            return;
        }
        if (!secondReady) return;
        if (secondActor == DoorActor::MAIN) {
            targetX = doorOutsideX;
            targetY = doorOutsideY;
        } else {
            visitor.targetX = doorOutsideX;
            visitor.targetY = visitorDoorOutsideY;
        }
        doorTransition = DoorTransitionMode::EXIT_SECOND_CROSS;
        doorPhaseStartedMs = nowMs;
        return;
    }
    case DoorTransitionMode::EXIT_SECOND_CROSS: {
        DoorActor secondActor = doorFirstActor == DoorActor::MAIN
            ? DoorActor::VISITOR
            : DoorActor::MAIN;
        bool secondOutside = secondActor == DoorActor::MAIN
            ? moveDoorToward(doorOutsideX, doorOutsideY, DOOR_CROSS_SPEED,
                             dtSeconds, false)
            : moveVisitorDoorToward(doorOutsideX, visitorDoorOutsideY, DOOR_CROSS_SPEED,
                                    dtSeconds, false);
        if (!secondOutside) return;
        if (secondActor == DoorActor::MAIN) {
            doorMainHidden = true;
        } else {
            doorVisitorHidden = true;
        }
        finishDoorDeparture();
        return;
    }
    case DoorTransitionMode::EXIT_FADE:
        velocityX = velocityY = 0.0f;
        return;
    case DoorTransitionMode::ENTER_WAIT_FADE:
        velocityX = velocityY = 0.0f;
        if (GameEngine::ins().sceneFadeActive()) return;
        if (!prepareDoorAnchors()) {
            doorTransition = DoorTransitionMode::NONE;
            GameEngine::ins().finishExploreReturn();
            nextAiDecisionMs = nowMs + 800;
            return;
        }
        targetX = doorInsideX;
        targetY = doorInsideY;
        doorTransition = DoorTransitionMode::ENTER_CROSS;
        doorPhaseStartedMs = nowMs;
        return;
    case DoorTransitionMode::ENTER_CROSS:
        if (moveDoorToward(doorInsideX, doorInsideY, DOOR_CROSS_SPEED, dtSeconds, false)) {
            velocityX = velocityY = 0.0f;
            monsterX = doorInsideX;
            monsterY = doorInsideY;
            if (doorDepartureHasVisitor) {
                targetX = doorWaitX;
                targetY = doorWaitY;
                if (!buildMoveRoute(targetX, targetY)) {
                    doorDepartureHasVisitor = false;
                } else {
                    doorRouteEnteringWalkArea = false;
                    doorVisitorHidden = false;
                    visitor.x = doorOutsideX;
                    visitor.y = visitorDoorOutsideY;
                    visitor.targetX = doorInsideX;
                    visitor.targetY = visitorDoorInsideY;
                    visitor.state = VisitorState::IDLE;
                    doorTransition = DoorTransitionMode::ENTER_CLEAR_ROUTE;
                    doorPhaseStartedMs = nowMs;
                    return;
                }
            }
            targetX = monsterX;
            targetY = monsterY;
            aiMode = AiMode::IDLE;
            nextAiDecisionMs = nowMs + GameRandom::random(900, 1601);
            mind.onActivity(nowMs);
            doorTransition = DoorTransitionMode::NONE;
            GameEngine::ins().finishExploreReturn();
        }
        return;
    case DoorTransitionMode::ENTER_CLEAR_ROUTE:
        if (nowMs - doorPhaseStartedMs >= DOOR_ROUTE_TIMEOUT_MS) {
            clearMoveRoute();
            monsterX = doorWaitX;
            monsterY = doorWaitY;
            velocityX = velocityY = 0.0f;
        } else if (!updateDoorRoute(dtSeconds)) {
            return;
        }
        visitor.state = VisitorState::WALK;
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        doorTransition = DoorTransitionMode::ENTER_SECOND_CROSS;
        doorPhaseStartedMs = nowMs;
        return;
    case DoorTransitionMode::ENTER_SECOND_CROSS:
        if (moveVisitorDoorToward(
                doorInsideX, visitorDoorInsideY, DOOR_CROSS_SPEED,
                dtSeconds, false, true)) {
            visitor.x = doorInsideX;
            visitor.y = visitorDoorInsideY;
            visitor.targetX = visitor.x;
            visitor.targetY = visitor.y;
            visitor.state = VisitorState::IDLE;
            velocityX = velocityY = 0.0f;
            targetX = monsterX;
            targetY = monsterY;
            aiMode = AiMode::IDLE;
            nextAiDecisionMs = nowMs + GameRandom::random(900, 1601);
            mind.onActivity(nowMs);
            doorTransition = DoorTransitionMode::NONE;
            GameEngine::ins().finishExploreReturn();
        }
        return;
    case DoorTransitionMode::FAINT_WAIT_FADE:
        velocityX = velocityY = 0.0f;
        if (!GameEngine::ins().sceneFadeActive()) {
            doorTransition = DoorTransitionMode::NONE;
            GameEngine::ins().finishExploreReturn();
        }
        return;
    case DoorTransitionMode::NONE:
        return;
    }
}

bool MainScene::updateDoorWaitingActor(float dtSeconds) {
    DoorActor waitingActor = doorFirstActor == DoorActor::MAIN
        ? DoorActor::VISITOR
        : DoorActor::MAIN;
    if (waitingActor == DoorActor::MAIN) {
        if (updateDoorRoute(dtSeconds)) {
            velocityX = velocityY = 0.0f;
            return true;
        }
    } else if (updateVisitorDoorRoute(dtSeconds)) {
        visitor.state = VisitorState::IDLE;
        return true;
    }
    return false;
}

void MainScene::beginSecondDoorExit(uint32_t nowMs) {
    if (!doorDepartureHasVisitor) {
        finishDoorDeparture();
        return;
    }

    DoorActor secondActor = doorFirstActor == DoorActor::MAIN
        ? DoorActor::VISITOR
        : DoorActor::MAIN;
    bool routeReady = false;
    if (secondActor == DoorActor::MAIN) {
        targetX = doorInsideX;
        targetY = doorInsideY;
        routeReady = buildMoveRoute(targetX, targetY);
        doorRouteEnteringWalkArea = !monsterFootprintInsideWalkArea(monsterX, monsterY);
    } else {
        visitor.targetX = doorInsideX;
        visitor.targetY = visitorDoorInsideY;
        visitor.state = VisitorState::WALK;
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        routeReady = buildVisitorMoveRoute(visitor.targetX, visitor.targetY);
        visitorDoorRouteEnteringWalkArea =
            !actorFootprintInsideWalkArea(
                visitor.x, visitor.y, visitorGeometry());
    }

    if (!routeReady) {
        if (secondActor == DoorActor::MAIN) {
            monsterX = doorInsideX;
            monsterY = doorInsideY;
            targetX = doorOutsideX;
            targetY = doorOutsideY;
            clearMoveRoute();
        } else {
            visitor.x = doorInsideX;
            visitor.y = visitorDoorInsideY;
            visitor.targetX = doorOutsideX;
            visitor.targetY = visitorDoorOutsideY;
            clearVisitorMoveRoute();
        }
        Platform::logf(
            "[DoorDeparture] second route fallback actor=%u mode=cross\n",
            secondActor == DoorActor::MAIN ? 0 : 1);
        doorTransition = DoorTransitionMode::EXIT_SECOND_CROSS;
        doorPhaseStartedMs = nowMs;
        return;
    }
    doorTransition = DoorTransitionMode::EXIT_SECOND_ROUTE;
    doorPhaseStartedMs = nowMs;
    doorLastProgressMs = nowMs;
}

void MainScene::finishDoorDeparture() {
    clearMoveRoute();
    clearVisitorMoveRoute();
    velocityX = velocityY = 0.0f;
    visitor.state = VisitorState::IDLE;
    doorTransition = DoorTransitionMode::EXIT_FADE;
    if (!GameEngine::ins().fadeToScene(SceneID::EXPLORE)) {
        GameEngine::ins().requestScene(SceneID::EXPLORE);
    }
}

bool MainScene::prepareDoorAnchors() {
    if (room().doorwayPolygonCount() < 3) return false;
    if (!chooseDoorInsidePose(doorInsideX, doorInsideY)) return false;
    doorOutsideX = (float)room().doorwayOutsideX();
    doorOutsideY = (float)room().doorwayOutsideY() - walkBoundaryOffsetY();
    float insideFootY = doorInsideY + mainGeometry().groundOffsetY;
    visitorDoorInsideY = insideFootY - visitorGeometry().groundOffsetY;
    visitorDoorOutsideY =
        (float)room().doorwayOutsideY() - visitorGeometry().groundOffsetY;
    return true;
}

bool MainScene::chooseDoorInsidePose(float& x, float& y) const {
    float anchorX = (float)room().doorwayInsideX();
    float anchorY = (float)room().doorwayInsideY();
    float offsetY = walkBoundaryOffsetY();
    auto tryCandidate = [&](int dx, int dy) {
        float candidateX = anchorX + dx;
        float candidateY = anchorY + dy - offsetY;
        if (!monsterFootprintInsideWalkArea(candidateX, candidateY)) return false;
        x = candidateX;
        y = candidateY;
        return true;
    };

    if (tryCandidate(0, 0)) return true;
    for (int radius = 2; radius <= 48; radius += 2) {
        for (int offset = -radius; offset <= radius; offset += 2) {
            if (tryCandidate(radius, offset) || tryCandidate(-radius, offset) ||
                tryCandidate(offset, radius) || tryCandidate(offset, -radius)) return true;
        }
    }
    return false;
}

bool MainScene::chooseDoorWaitPose(float actorX, float actorY,
                                   const ActorGeometry& geometry,
                                   float& x, float& y) const {
    float inwardX = doorInsideX - doorOutsideX;
    float inwardY = doorInsideY - doorOutsideY;
    float inwardLength = sqrtf(inwardX * inwardX + inwardY * inwardY);
    if (inwardLength < 0.001f) return false;
    inwardX /= inwardLength;
    inwardY /= inwardLength;
    float tangentX = -inwardY;
    float tangentY = inwardX;
    float sideOffset = MathUtil::max(DOOR_WAIT_SIDE_OFFSET, doorActorMinSeparation());

    struct Candidate {
        float x;
        float y;
        float score;
    };
    Candidate best{0.0f, 0.0f, 1000000.0f};
    static constexpr float INWARD_OFFSETS[] = {
        DOOR_WAIT_INWARD_OFFSET * 2.0f,
        DOOR_WAIT_INWARD_OFFSET,
        DOOR_WAIT_INWARD_OFFSET * 3.0f,
    };
    static constexpr float SIDE_SCALES[] = {1.0f, 1.2f, 0.82f};

    for (float inwardOffset : INWARD_OFFSETS) {
        for (float sideScale : SIDE_SCALES) {
            for (int side : {-1, 1}) {
                float candidateX =
                    doorInsideX + inwardX * inwardOffset +
                    tangentX * sideOffset * sideScale * side;
                float candidateFootY =
                    doorInsideY + mainGeometry().groundOffsetY +
                    inwardY * inwardOffset +
                    tangentY * sideOffset * sideScale * side;
                float candidateY = candidateFootY - geometry.groundOffsetY;
                if (!actorFootprintInsideWalkArea(
                        candidateX, candidateY, geometry)) continue;
                float doorDx = candidateX - doorInsideX;
                float doorDy = candidateFootY -
                    (doorInsideY + mainGeometry().groundOffsetY);
                if (doorDx * doorDx + doorDy * doorDy <
                    doorActorMinSeparation() * doorActorMinSeparation()) {
                    continue;
                }
                float actorDx = candidateX - actorX;
                float actorDy = candidateY - actorY;
                float score = actorDx * actorDx + actorDy * actorDy;
                if (score < best.score) best = {candidateX, candidateY, score};
            }
        }
    }
    if (best.score >= 1000000.0f) return false;
    x = best.x;
    y = best.y;
    return true;
}

bool MainScene::updateDoorRoute(float dtSeconds) {
    float waypointX = targetX;
    float waypointY = targetY;
    if (!currentWaypoint(waypointX, waypointY)) return true;
    if (!moveDoorToward(waypointX, waypointY, DOOR_ROUTE_SPEED, dtSeconds, true,
                        doorDepartureHasVisitor && !doorVisitorHidden)) {
        return false;
    }
    moveRouteIndex++;
    if (moveRouteIndex < moveRouteCount) return false;
    clearMoveRoute();
    return true;
}

bool MainScene::updateVisitorDoorRoute(float dtSeconds) {
    float waypointX = visitor.targetX;
    float waypointY = visitor.targetY;
    if (!currentVisitorWaypoint(waypointX, waypointY)) return true;
    if (!moveVisitorDoorToward(
            waypointX, waypointY, DOOR_ROUTE_SPEED, dtSeconds, true,
            doorDepartureHasVisitor && !doorMainHidden)) {
        return false;
    }
    visitorMoveRouteIndex++;
    if (visitorMoveRouteIndex < visitorMoveRouteCount) return false;
    clearVisitorMoveRoute();
    visitor.state = VisitorState::IDLE;
    return true;
}

bool MainScene::moveDoorToward(float x, float y, float speed, float dtSeconds,
                               bool enforceWalkArea, bool avoidVisitor) {
    float dx = x - monsterX;
    float dy = y - monsterY;
    float distance = sqrtf(dx * dx + dy * dy);
    float step = speed * dtSeconds;
    if (distance <= 0.001f || step <= 0.0f) {
        velocityX = velocityY = 0.0f;
        return distance <= 0.001f;
    }

    bool reached = distance <= 1.0f || step >= distance;
    float nextX = reached ? x : monsterX + dx / distance * speed * dtSeconds;
    float nextY = reached ? y : monsterY + dy / distance * speed * dtSeconds;
    bool currentInsideWalkArea = monsterFootprintInsideWalkArea(monsterX, monsterY);
    bool nextInsideWalkArea = monsterFootprintInsideWalkArea(nextX, nextY);
    if (!doorRouteStepAllowed(enforceWalkArea, doorRouteEnteringWalkArea,
                              currentInsideWalkArea, nextInsideWalkArea)) {
        velocityX = velocityY = 0.0f;
        return false;
    }
    if (avoidVisitor &&
        !doorStepKeepsSpacing(monsterX, monsterY, nextX, nextY,
                              visitor.x, visitor.y, false)) {
        velocityX = velocityY = 0.0f;
        return false;
    }

    velocityX = reached ? 0.0f : dx / distance * speed;
    velocityY = reached ? 0.0f : dy / distance * speed;
    monsterX = nextX;
    monsterY = nextY;
    if (fabsf(velocityX) > 4.0f) facingRight = velocityX > 0.0f;
    if (nextInsideWalkArea) doorRouteEnteringWalkArea = false;
    return reached;
}

bool MainScene::moveVisitorDoorToward(float x, float y, float speed, float dtSeconds,
                                      bool enforceWalkArea, bool avoidMain) {
    float dx = x - visitor.x;
    float dy = y - visitor.y;
    float distance = sqrtf(dx * dx + dy * dy);
    float step = speed * dtSeconds;
    if (distance <= 0.001f || step <= 0.0f) {
        return distance <= 0.001f;
    }

    bool reached = distance <= 1.0f || step >= distance;
    float nextX = reached ? x : visitor.x + dx / distance * speed * dtSeconds;
    float nextY = reached ? y : visitor.y + dy / distance * speed * dtSeconds;
    const ActorGeometry geometry = visitorGeometry();
    bool currentInsideWalkArea = actorFootprintInsideWalkArea(
        visitor.x, visitor.y, geometry);
    bool nextInsideWalkArea = actorFootprintInsideWalkArea(
        nextX, nextY, geometry);
    if (!doorRouteStepAllowed(enforceWalkArea, visitorDoorRouteEnteringWalkArea,
                              currentInsideWalkArea, nextInsideWalkArea)) {
        return false;
    }
    if (avoidMain &&
        !doorStepKeepsSpacing(visitor.x, visitor.y, nextX, nextY,
                              monsterX, monsterY, true)) {
        return false;
    }

    visitor.x = nextX;
    visitor.y = nextY;
    visitor.state = reached ? VisitorState::IDLE : VisitorState::WALK;
    visitor.direction = visitorWalkDirectionForDelta(dx, dy);
    if (fabsf(dx) > 0.5f) visitor.facingRight = dx > 0.0f;
    if (nextInsideWalkArea) visitorDoorRouteEnteringWalkArea = false;
    advanceVisitorFrames(doorLastUpdateMs, !reached);
    return reached;
}

bool MainScene::doorStepKeepsSpacing(float currentX, float currentY,
                                     float nextX, float nextY,
                                     float otherX, float otherY,
                                     bool movingVisitor) const {
    float minSeparation = doorActorMinSeparation();
    float movingOffset = movingVisitor
        ? visitorGeometry().groundOffsetY : mainGeometry().groundOffsetY;
    float otherOffset = movingVisitor
        ? mainGeometry().groundOffsetY : visitorGeometry().groundOffsetY;
    currentY += movingOffset;
    nextY += movingOffset;
    otherY += otherOffset;
    return routeSegmentKeepsSpacing(
        currentX, currentY, nextX, nextY,
        otherX, otherY, minSeparation);
}

float MainScene::doorActorMinSeparation() const {
    float mainRadius = MathUtil::clamp(
        mainGeometry().footprint.radiusX, 10.0f, 20.0f);
    float visitorRadius = MathUtil::clamp(
        visitorGeometry().footprint.radiusX, 10.0f, 20.0f);
    return MathUtil::clamp(mainRadius + visitorRadius + 8.0f, 28.0f, 44.0f);
}

void MainScene::updateCamera() {
    float focusY = pairInteraction != PairInteraction::NONE &&
                           visitor.active
        ? (monsterY + visitor.y) * 0.5f
        : (doorMainHidden && !doorVisitorHidden
               ? visitor.y
               : monsterY);
    cameraY = cameraForWorldY(focusY);
}

int16_t MainScene::worldToScreenY(float worldY) const {
    return (int16_t)roundf(worldY - cameraY);
}

float MainScene::walkBoundaryOffsetY() const {
    return mainGeometry().groundOffsetY;
}

float MainScene::walkFootprintRadiusX() const {
    return mainGeometry().footprint.radiusX;
}

float MainScene::walkFootprintRadiusY() const {
    return mainGeometry().footprint.radiusY;
}

MainScene::ActorGeometry MainScene::geometryForSpecies(
    uint16_t speciesId) const {
    const PokemonSprites::SpriteFrame* frame = nullptr;
    if (const PmdSpriteConfig* config = pmdSpriteConfigForSpecies(speciesId)) {
        frame = PokemonSprites::findSpeciesSprite(speciesId, config->idleBase);
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    }
    uint8_t width = frame ? FlashStorage::readByte(&frame->width) : 38;
    uint8_t height = frame ? FlashStorage::readByte(&frame->height) : 42;
    ActorGeometry geometry;
    geometry.groundOffsetY = frame
        ? static_cast<float>(PokemonSprites::frameGroundOffsetY(frame))
        : 18.0f;
    geometry.footprint.radiusX =
        static_cast<float>(MathUtil::clamp((int)(width * 0.24f), 7, 16));
    geometry.footprint.radiusY =
        static_cast<float>(MathUtil::clamp((int)(height * 0.10f), 4, 8));
    return geometry;
}

MainScene::ActorGeometry MainScene::mainGeometry() const {
    return geometryForSpecies(active ? active->id : 0);
}

MainScene::ActorGeometry MainScene::visitorGeometry() const {
    return geometryForSpecies(visitor.speciesId);
}

bool MainScene::actorFootprintInsideWalkArea(
    float x, float y, const ActorGeometry& geometry) const {
    return RoomMovementArea::containsFootprint(
        room().walkPolygon(), room().walkPolygonCount(),
        x, y + geometry.groundOffsetY, geometry.footprint);
}

bool MainScene::monsterFootprintInsideWalkArea(float x, float y) const {
    return actorFootprintInsideWalkArea(x, y, mainGeometry());
}

bool MainScene::randomMonsterCenterWalkPoint(float& x, float& y) const {
    return randomActorCenterWalkPoint(mainGeometry(), x, y);
}

bool MainScene::randomActorCenterWalkPoint(
    const ActorGeometry& geometry, float& x, float& y) const {
    float offsetY = geometry.groundOffsetY;
    float rx = geometry.footprint.radiusX;
    for (uint8_t tries = 0; tries < 64; ++tries) {
        float px = (float)GameRandom::random(roomWalkMinX(), roomWalkMaxX() + 1);
        float py = (float)GameRandom::random(roomWalkMinY(), roomWalkMaxY() + 1);
        float centerY = py - offsetY;
        if (px - rx < (float)roomWalkMinX() ||
            px + rx > (float)roomWalkMaxX()) {
            continue;
        }
        if (actorFootprintInsideWalkArea(px, centerY, geometry)) {
            x = px;
            y = centerY;
            return true;
        }
    }

    for (int py = roomWalkMinY(); py <= roomWalkMaxY(); py += 4) {
        for (int px = roomWalkMinX(); px <= roomWalkMaxX(); px += 4) {
            float centerY = (float)py - offsetY;
            if (actorFootprintInsideWalkArea((float)px, centerY, geometry)) {
                x = (float)px;
                y = centerY;
                return true;
            }
        }
    }

    float px = 0.0f;
    float py = 0.0f;
    if (!randomRoomWalkPoint(px, py)) return false;
    x = px;
    y = py - offsetY;
    return actorFootprintInsideWalkArea(x, y, geometry);
}

bool MainScene::pathSegmentInsideWalkArea(float fromX, float fromY,
                                          float toX, float toY,
                                          bool allowOutsideStart) const {
    return actorPathSegmentInsideWalkArea(
        fromX, fromY, toX, toY, mainGeometry(), allowOutsideStart);
}

bool MainScene::actorPathSegmentInsideWalkArea(
    float fromX, float fromY, float toX, float toY,
    const ActorGeometry& geometry, bool allowOutsideStart) const {
    return RoomMovementArea::segmentInsideFootprint(
        room().walkPolygon(), room().walkPolygonCount(),
        fromX, fromY + geometry.groundOffsetY,
        toX, toY + geometry.groundOffsetY,
        geometry.footprint, allowOutsideStart);
}

bool MainScene::routeSegmentKeepsSpacing(
    float fromX, float fromY, float toX, float toY,
    float otherX, float otherY, float minSeparation) const {
    float dx = toX - fromX;
    float dy = toY - fromY;
    float lengthSq = dx * dx + dy * dy;
    float t = lengthSq <= 0.001f
        ? 0.0f
        : ((otherX - fromX) * dx + (otherY - fromY) * dy) / lengthSq;
    t = MathUtil::clamp(t, 0.0f, 1.0f);
    float nearestX = fromX + dx * t;
    float nearestY = fromY + dy * t;
    float gapX = nearestX - otherX;
    float gapY = nearestY - otherY;
    float minSq = minSeparation * minSeparation;
    float startDx = fromX - otherX;
    float startDy = fromY - otherY;
    float startSq = startDx * startDx + startDy * startDy;
    float endDx = toX - otherX;
    float endDy = toY - otherY;
    float endSq = endDx * endDx + endDy * endDy;
    if (startSq < minSq) return endSq > startSq;
    return gapX * gapX + gapY * gapY >= minSq;
}

bool MainScene::mainTargetKeepsVisitorSpacing(
    float targetX, float targetY) const {
    if (!visitor.active || doorVisitorHidden) return true;
    return actorTargetsKeepSpacing(
        targetX, targetY, mainGeometry(),
        visitor.x, visitor.y, visitorGeometry());
}

bool MainScene::actorTargetsKeepSpacing(
    float targetX, float targetY, const ActorGeometry& movingGeometry,
    float otherX, float otherY,
    const ActorGeometry& otherGeometry) const {
    float dx = targetX - otherX;
    float dy = (targetY + movingGeometry.groundOffsetY) -
               (otherY + otherGeometry.groundOffsetY);
    float separation = doorActorMinSeparation();
    return dx * dx + dy * dy >= separation * separation;
}

void MainScene::clearMoveRoute() {
    moveRouteCount = 0;
    moveRouteIndex = 0;
}

void MainScene::clearVisitorMoveRoute() {
    visitorMoveRouteCount = 0;
    visitorMoveRouteIndex = 0;
}

bool MainScene::currentWaypoint(float& x, float& y) const {
    if (moveRouteIndex >= moveRouteCount) return false;
    x = moveRouteX[moveRouteIndex];
    y = moveRouteY[moveRouteIndex];
    return true;
}

bool MainScene::currentVisitorWaypoint(float& x, float& y) const {
    if (visitorMoveRouteIndex >= visitorMoveRouteCount) return false;
    x = visitorMoveRouteX[visitorMoveRouteIndex];
    y = visitorMoveRouteY[visitorMoveRouteIndex];
    return true;
}

bool MainScene::buildMoveRoute(float goalX, float goalY) {
    bool avoidVisitor = visitor.active && !doorVisitorHidden;
    bool bedRoute = visitor.state == VisitorState::SLEEPING &&
        fabsf(goalX - bedSleepX()) <= BED_APPROACH_TOLERANCE_X + 8.0f;
    if (bedRoute) avoidVisitor = false;
    return buildMoveRouteFrom(
        monsterX, monsterY, goalX, goalY,
        moveRouteX, moveRouteY, moveRouteCount, moveRouteIndex,
        monsterAtBedSleepPose(), mainGeometry(), avoidVisitor,
        visitor.x, visitor.y, visitorGeometry().groundOffsetY);
}

bool MainScene::buildVisitorMoveRoute(float goalX, float goalY) {
    return buildMoveRouteFrom(
        visitor.x, visitor.y, goalX, goalY,
        visitorMoveRouteX, visitorMoveRouteY,
        visitorMoveRouteCount, visitorMoveRouteIndex, false,
        visitorGeometry(), !doorMainHidden, monsterX, monsterY,
        mainGeometry().groundOffsetY);
}

bool MainScene::buildMoveRouteFrom(float startX, float startY,
                                   float goalX, float goalY,
                                   float* routeX, float* routeY,
                                   uint8_t& routeCount, uint8_t& routeIndex,
                                   bool allowOutsideStart,
                                   const ActorGeometry& geometry,
                                   bool avoidOther, float otherX, float otherY,
                                   float otherGroundOffsetY) {
    routeCount = 0;
    routeIndex = 0;
    float separation = doorActorMinSeparation();
    auto pointAllowed = [&](float x, float y) {
        if (!actorFootprintInsideWalkArea(x, y, geometry)) return false;
        if (!avoidOther) return true;
        float dx = x - otherX;
        float dy = y + geometry.groundOffsetY -
                   (otherY + otherGroundOffsetY);
        return dx * dx + dy * dy >= separation * separation;
    };
    auto segmentAllowed = [&](float fromX, float fromY, float toX, float toY,
                              bool allowOutside = false) {
        return actorPathSegmentInsideWalkArea(
                   fromX, fromY, toX, toY, geometry, allowOutside) &&
               (!avoidOther || routeSegmentKeepsSpacing(
                    fromX, fromY + geometry.groundOffsetY,
                    toX, toY + geometry.groundOffsetY,
                    otherX, otherY + otherGroundOffsetY, separation));
    };
    if (!pointAllowed(goalX, goalY)) return false;
    if (segmentAllowed(startX, startY, goalX, goalY, allowOutsideStart)) {
        routeX[0] = goalX;
        routeY[0] = goalY;
        routeCount = 1;
        return true;
    }
    if (!ensureNavScratch()) return false;

    float offsetY = geometry.groundOffsetY;
    float minX = (float)roomWalkMinX();
    float minY = (float)roomWalkMinY() - offsetY;
    float maxX = (float)roomWalkMaxX();
    float maxY = (float)roomWalkMaxY() - offsetY;
    uint8_t cols = (uint8_t)MathUtil::min<int>(NAV_MAX_COLS, (int)ceilf((maxX - minX) / NAV_CELL_PX) + 1);
    uint8_t rows = (uint8_t)MathUtil::min<int>(NAV_MAX_ROWS, (int)ceilf((maxY - minY) / NAV_CELL_PX) + 1);
    uint16_t nodeCount = (uint16_t)cols * rows;
    if (cols < 2 || rows < 2 || nodeCount > NAV_MAX_NODES) return false;

    auto nodeX = [&](uint16_t node) {
        uint8_t col = (uint8_t)(node % cols);
        return minX + (float)col * NAV_CELL_PX;
    };
    auto nodeY = [&](uint16_t node) {
        uint8_t row = (uint8_t)(node / cols);
        return minY + (float)row * NAV_CELL_PX;
    };

    int16_t startNode = -1;
    int16_t goalNode = -1;
    float bestStart = 1000000.0f;
    float bestGoal = 1000000.0f;
    for (uint16_t node = 0; node < nodeCount; ++node) {
        float x = nodeX(node);
        float y = nodeY(node);
        if (!pointAllowed(x, y)) continue;
        float startDx = x - startX;
        float startDy = y - startY;
        float startDist = startDx * startDx + startDy * startDy;
        if (startDist < bestStart &&
            segmentAllowed(startX, startY, x, y, allowOutsideStart)) {
            bestStart = startDist;
            startNode = (int16_t)node;
        }
        float goalDx = x - goalX;
        float goalDy = y - goalY;
        float goalDist = goalDx * goalDx + goalDy * goalDy;
        if (goalDist < bestGoal && segmentAllowed(x, y, goalX, goalY)) {
            bestGoal = goalDist;
            goalNode = (int16_t)node;
        }
    }
    if (startNode < 0 || goalNode < 0) return false;

    for (uint16_t node = 0; node < nodeCount; ++node) gNavParent[node] = -2;
    uint16_t queueRead = 0;
    uint16_t queueWrite = 0;
    gNavQueue[queueWrite++] = (uint16_t)startNode;
    gNavParent[startNode] = -1;
    static constexpr int8_t DIRS[8][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
    };
    while (queueRead < queueWrite && gNavParent[goalNode] == -2) {
        uint16_t node = gNavQueue[queueRead++];
        int col = node % cols;
        int row = node / cols;
        float fromX = nodeX(node);
        float fromY = nodeY(node);
        for (const auto& dir : DIRS) {
            int nextCol = col + dir[0];
            int nextRow = row + dir[1];
            if (nextCol < 0 || nextCol >= cols || nextRow < 0 || nextRow >= rows) continue;
            uint16_t next = (uint16_t)(nextRow * cols + nextCol);
            if (gNavParent[next] != -2) continue;
            float nextX = nodeX(next);
            float nextY = nodeY(next);
            if (!pointAllowed(nextX, nextY) ||
                !segmentAllowed(fromX, fromY, nextX, nextY)) {
                continue;
            }
            gNavParent[next] = (int16_t)node;
            if (queueWrite < NAV_MAX_NODES) gNavQueue[queueWrite++] = next;
        }
    }
    if (gNavParent[goalNode] == -2) return false;

    uint16_t pathCount = 0;
    for (int16_t node = goalNode; node >= 0 && pathCount < NAV_MAX_NODES;
         node = gNavParent[node]) {
        gNavQueue[pathCount++] = (uint16_t)node;
    }
    if (pathCount == 0) return false;

    float fromX = startX;
    float fromY = startY;
    int cursor = (int)pathCount - 1;
    while (cursor > 0 && routeCount + 1 < MOVE_ROUTE_CAP) {
        int selected = cursor - 1;
        for (int candidate = 0; candidate < cursor; ++candidate) {
            uint16_t node = gNavQueue[candidate];
            if (segmentAllowed(fromX, fromY, nodeX(node), nodeY(node))) {
                selected = candidate;
                break;
            }
        }
        uint16_t node = gNavQueue[selected];
        fromX = nodeX(node);
        fromY = nodeY(node);
        routeX[routeCount] = fromX;
        routeY[routeCount] = fromY;
        routeCount++;
        cursor = selected;
    }
    if (!segmentAllowed(fromX, fromY, goalX, goalY) ||
        routeCount >= MOVE_ROUTE_CAP) {
        routeCount = 0;
        routeIndex = 0;
        return false;
    }
    routeX[routeCount] = goalX;
    routeY[routeCount] = goalY;
    routeCount++;
    return true;
}

void MainScene::updateStuckWatchdog(uint32_t nowMs, float distanceToWaypoint) {
    if (distanceToWaypoint + MOVE_PROGRESS_EPSILON < lastWaypointDistance) {
        lastWaypointDistance = distanceToWaypoint;
        lastMoveProgressMs = nowMs;
        return;
    }
    if (nowMs - lastMoveProgressMs < MOVE_STUCK_MS) return;

    stuckRecoveryCount++;
    Platform::logf("[MonsterAI] stuck mode=%u count=%u pos=%.1f,%.1f target=%.1f,%.1f\n",
                  (unsigned)aiMode, stuckRecoveryCount, monsterX, monsterY, targetX, targetY);
    if (stuckRecoveryCount <= 2 && buildMoveRoute(targetX, targetY)) {
        lastWaypointDistance = 1000000.0f;
        lastMoveProgressMs = nowMs;
        return;
    }
    abortMovement(nowMs, GameRandom::random(1200, 2601));
}

void MainScene::abortMovement(uint32_t nowMs, uint32_t retryDelayMs) {
    AiMode abortedMode = aiMode == AiMode::TURNING ? turnNextMode : aiMode;
    if (abortedMode == AiMode::SEEK_FOOD) releaseBowl(0);
    if (mainYieldingForVisitorFood) {
        mainYieldingForVisitorFood = false;
        clearVisitorFoodRouteFailure();
    }
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::IDLE;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + retryDelayMs;
    lastMoveProgressMs = nowMs;
}

void MainScene::handleVisitorMoveBlocked(uint32_t nowMs,
                                         bool movingToSleep) {
    if (visitorMoveBlockedSinceMs == 0) visitorMoveBlockedSinceMs = nowMs;
    if ((int32_t)(nowMs - visitorNextReplanMs) >= 0) {
        visitorNextReplanMs = nowMs + 350;
        buildVisitorMoveRoute(visitor.targetX, visitor.targetY);
    }
    if (nowMs - visitorMoveBlockedSinceMs < MOVE_STUCK_MS) return;

    if (visitor.state == VisitorState::SEEK_FOOD) releaseBowl(1);
    clearVisitorMoveRoute();
    visitor.state = VisitorState::IDLE;
    visitor.targetX = visitor.x;
    visitor.targetY = visitor.y;
    visitor.stateUntilMs =
        nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    if (movingToSleep) visitor.sleepSpotValid = false;
    visitorMoveBlockedSinceMs = 0;
}

bool MainScene::buildFoodApproachRoute(
    bool movingMain, float& x, float& y) {
#if STICKMON_ENABLE_TRACE_LOGS
    uint32_t routeStartedUs = Platform::clock().micros();
    uint16_t routeCandidates = 0;
    uint16_t routeBuilds = 0;
#endif
    const ActorGeometry geometry =
        movingMain ? mainGeometry() : visitorGeometry();
    float offsetY = geometry.groundOffsetY;
    auto tryCandidate = [&](int dx, int dy) {
#if STICKMON_ENABLE_TRACE_LOGS
        ++routeCandidates;
#endif
        float candidateX = foodFeedX() + dx;
        float candidateY = foodFeedY() + dy - offsetY;
        if (!actorFootprintInsideWalkArea(candidateX, candidateY, geometry)) {
            return false;
        }
        if (movingMain) {
            if (!mainTargetKeepsVisitorSpacing(candidateX, candidateY)) {
                return false;
            }
        } else if (!doorMainHidden &&
                   !actorTargetsKeepSpacing(
                       candidateX, candidateY, geometry,
                       monsterX, monsterY, mainGeometry())) {
            return false;
        }
#if STICKMON_ENABLE_TRACE_LOGS
        ++routeBuilds;
#endif
        bool routeReady = movingMain
            ? buildMoveRoute(candidateX, candidateY)
            : buildVisitorMoveRoute(candidateX, candidateY);
        if (!routeReady) return false;
        x = candidateX;
        y = candidateY;
        return true;
    };

    // Navigation itself uses an 8 px grid. Testing every 2 px around the bowl
    // produced 1285 near-duplicate candidates and could block one frame for
    // hundreds of milliseconds.
    static constexpr int8_t CANDIDATES[][2] = {
        {0, 0},
        {8, 0}, {-8, 0}, {0, 8}, {0, -8},
        {8, 8}, {-8, 8}, {8, -8}, {-8, -8},
        {16, 0}, {-16, 0}, {0, 16}, {0, -16},
        {16, 8}, {-16, 8}, {16, -8}, {-16, -8},
        {8, 16}, {-8, 16}, {8, -16}, {-8, -16},
        {24, 0}, {-24, 0}, {0, 24}, {0, -24},
        {24, 12}, {-24, 12}, {24, -12}, {-24, -12},
        {12, 24}, {-12, 24}, {12, -24}, {-12, -24},
        {32, 0}, {-32, 0}, {32, 16}, {-32, 16},
        {32, -16}, {-32, -16}, {24, 24}, {-24, 24},
        {24, -24}, {-24, -24}, {36, 0}, {-36, 0},
    };
    bool found = false;
    for (const auto& candidate : CANDIDATES) {
        if (tryCandidate(candidate[0], candidate[1])) {
            found = true;
            break;
        }
    }
#if STICKMON_ENABLE_TRACE_LOGS
    uint32_t routeUs = Platform::clock().micros() - routeStartedUs;
    if (routeUs >= 3000 || routeCandidates >= 12) {
        STICKMON_TRACEF(
            "[FoodRoute] t=%lu moving=%u found=%u candidates=%u builds=%u us=%lu "
            "main=%.1f,%.1f visitor=%.1f,%.1f\n",
            static_cast<unsigned long>(Hal::ins().millis()),
            movingMain ? 1U : 0U, found ? 1U : 0U,
            static_cast<unsigned>(routeCandidates),
            static_cast<unsigned>(routeBuilds),
            static_cast<unsigned long>(routeUs),
            monsterX, monsterY, visitor.x, visitor.y);
    }
#endif
    return found;
}

bool MainScene::monsterCanUseBedSleepPose(float x, float y) const {
    float footY = y + walkBoundaryOffsetY();
    return roomBedContains(x, footY) ||
           (x >= (float)roomBedMinX() + 12.0f &&
            x <= (float)roomBedMaxX() - 12.0f &&
            footY >= (float)roomBedMinY() + 7.0f &&
            footY <= (float)roomBedMaxY() - 7.0f);
}

bool MainScene::chooseBedApproachPose(float& x, float& y) const {
    static constexpr float CANDIDATES[][2] = {
        {0.0f, 7.0f},
        {-7.0f, 6.0f},
        {7.0f, 6.0f},
        {-10.0f, 3.0f},
        {10.0f, 3.0f},
        {0.0f, 10.0f},
    };

    float offsetY = walkBoundaryOffsetY();
    for (uint8_t i = 0; i < sizeof(CANDIDATES) / sizeof(CANDIDATES[0]); ++i) {
        float candidateX = bedSleepX() + CANDIDATES[i][0];
        float candidateY = bedSleepY() + CANDIDATES[i][1] - offsetY;
        if (monsterFootprintInsideWalkArea(candidateX, candidateY)) {
            x = candidateX;
            y = candidateY;
            return true;
        }
    }

    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestDistSq = 1000000.0f;
    bool found = false;
    for (int py = roomWalkMinY(); py <= roomWalkMaxY(); py += 3) {
        for (int px = roomWalkMinX(); px <= roomWalkMaxX(); px += 3) {
            float centerY = (float)py - offsetY;
            if (!monsterFootprintInsideWalkArea((float)px, centerY)) continue;
            float dx = (float)px - bedSleepX();
            float dy = (float)py - bedSleepY();
            float distSq = dx * dx + dy * dy;
            if (!found || distSq < bestDistSq) {
                bestX = (float)px;
                bestY = centerY;
                bestDistSq = distSq;
                found = true;
            }
        }
    }
    if (!found) return false;
    x = bestX;
    y = bestY;
    return true;
}

bool MainScene::chooseBedSleepPose(float& x, float& y) const {
    static constexpr float CANDIDATES[][2] = {
        {0.0f, 0.0f},
        {-3.0f, -1.0f},
        {3.0f, -1.0f},
        {0.0f, -3.0f},
        {-2.0f, 2.0f},
        {2.0f, 2.0f},
    };

    float offsetY = walkBoundaryOffsetY();
    for (uint8_t i = 0; i < sizeof(CANDIDATES) / sizeof(CANDIDATES[0]); ++i) {
        float candidateX = bedSleepX() + CANDIDATES[i][0];
        float candidateY = bedSleepY() + CANDIDATES[i][1] - offsetY;
        if (monsterCanUseBedSleepPose(candidateX, candidateY)) {
            x = candidateX;
            y = candidateY;
            return true;
        }
    }
    x = bedSleepX();
    y = bedSleepY() - offsetY;
    return true;
}

void MainScene::scheduleAttention(uint32_t nowMs, bool initial) {
    uint32_t minDelay = initial ? ATTENTION_INITIAL_MIN_MS : ATTENTION_MIN_MS;
    uint32_t maxDelay = initial ? ATTENTION_INITIAL_MAX_MS : ATTENTION_MAX_MS;
    uint32_t delayMs = (uint32_t)GameRandom::random((long)minDelay, (long)maxDelay + 1L);
    int8_t bias = behaviorProfile.sociabilityBias;
    if (bias > 0) {
        delayMs = delayMs * (uint32_t)(100 - bias * 14) / 100UL;
    } else if (bias < 0) {
        delayMs = delayMs * (uint32_t)(100 + (-bias) * 18) / 100UL;
    }
    nextAttentionMs = nowMs + delayMs;
}

void MainScene::scheduleSpecialAction(uint32_t nowMs) {
    uint32_t delayMs = (uint32_t)GameRandom::random((long)SPECIAL_ACTION_MIN_MS,
                                        (long)SPECIAL_ACTION_MAX_MS + 1L);
    int8_t bias = behaviorProfile.activityBias;
    if (bias > 0) {
        delayMs = delayMs * (uint32_t)(100 - bias * 12) / 100UL;
    } else if (bias < 0) {
        delayMs = delayMs * (uint32_t)(100 + (-bias) * 16) / 100UL;
    }
    nextSpecialActionMs = nowMs + delayMs;
}

bool MainScene::ambientActionAllowed() const {
    if (roomAction != RoomAction::NONE || aiMode != AiMode::IDLE ||
        doorTransition != DoorTransitionMode::NONE || progressionModal != ProgressionModal::NONE) {
        return false;
    }
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if (mon.fainted || mon.hpCur == 0 || mon.majorStatus == Game::MajorStatus::SLEEP ||
        mon.hpMax == 0 || (uint32_t)mon.hpCur * 100UL <= (uint32_t)mon.hpMax * 25UL ||
        mon.satiety <= 25 || mainSceneIsSleepTime()) {
        return false;
    }
    if (GameEngine::ins().bowlHasFood() && mon.satiety < MONSTER_FEED_TARGET_SATIETY) {
        return false;
    }
    return mind.topDesire() != MonsterDesire::EAT &&
           mind.topDesire() != MonsterDesire::REST;
}

bool MainScene::chooseNearbyPose(float originX, float originY, float minDistance,
                                 float maxDistance, float preferredAngle,
                                 float angleSpread, float& x, float& y) const {
    for (uint8_t tries = 0; tries < 18; ++tries) {
        float unit = (float)GameRandom::random(-1000, 1001) / 1000.0f;
        float angle = preferredAngle + unit * angleSpread;
        float distance = minDistance + (maxDistance - minDistance) *
                         ((float)GameRandom::random(0, 1001) / 1000.0f);
        float candidateX = originX + cosf(angle) * distance;
        float candidateY = originY + sinf(angle) * distance;
        if (!monsterFootprintInsideWalkArea(candidateX, candidateY)) continue;
        if (!pathSegmentInsideWalkArea(originX, originY, candidateX, candidateY)) continue;
        x = candidateX;
        y = candidateY;
        return true;
    }

    for (uint8_t step = 0; step < 8; ++step) {
        float angle = preferredAngle + ((int)step - 3) * (PI_F / 8.0f);
        float candidateX = originX + cosf(angle) * minDistance;
        float candidateY = originY + sinf(angle) * minDistance;
        if (monsterFootprintInsideWalkArea(candidateX, candidateY) &&
            pathSegmentInsideWalkArea(originX, originY, candidateX, candidateY)) {
            x = candidateX;
            y = candidateY;
            return true;
        }
    }
    return false;
}

bool MainScene::chooseAttentionPose(float& x, float& y) const {
    float offsetY = walkBoundaryOffsetY();
    float desiredX = ((float)roomWalkMinX() + roomWalkMaxX()) * 0.5f;
    float bestScore = 1000000.0f;
    bool found = false;
    for (int footY = roomWalkMaxY(); footY >= roomWalkMinY(); footY -= 3) {
        for (int footX = roomWalkMinX(); footX <= roomWalkMaxX(); footX += 3) {
            float candidateY = (float)footY - offsetY;
            if (!monsterFootprintInsideWalkArea((float)footX, candidateY)) continue;
            float score = (float)(roomWalkMaxY() - footY) * 5.0f +
                          fabsf((float)footX - desiredX);
            if (score < bestScore) {
                x = (float)footX;
                y = candidateY;
                bestScore = score;
                found = true;
            }
        }
        if (found && roomWalkMaxY() - footY >= 9) break;
    }
    return found;
}

bool MainScene::chooseCirclePose(uint8_t phase, float& x, float& y) const {
    static constexpr float X_SCALE[] = {1.0f, 0.0f, -1.0f, 0.0f, 0.0f};
    static constexpr float Y_SCALE[] = {0.0f, 0.62f, 0.0f, -0.62f, 0.0f};
    if (phase >= 5) return false;
    x = roomActionOriginX + X_SCALE[phase] * roomActionRadius;
    y = roomActionOriginY + Y_SCALE[phase] * roomActionRadius;
    return monsterFootprintInsideWalkArea(x, y);
}

bool MainScene::chooseWindowGazePose(const RoomResource::BehaviorAnchor& anchor,
                                     float& x, float& y) const {
    static constexpr int8_t OFFSETS[][2] = {
        {0, 0}, {-3, 0}, {3, 0}, {-6, 0}, {6, 0},
        {0, 3}, {-3, 3}, {3, 3}, {0, -3}, {-3, -3}, {3, -3},
        {-9, 0}, {9, 0}, {0, 6}, {0, -6},
    };
    float offsetY = walkBoundaryOffsetY();
    for (const auto& offset : OFFSETS) {
        float candidateX = (float)anchor.footX + offset[0];
        float candidateY = (float)anchor.footY + offset[1] - offsetY;
        if (!monsterFootprintInsideWalkArea(candidateX, candidateY)) continue;
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

void MainScene::showHearts(HeartEffect effect, uint32_t nowMs, uint16_t durationMs) {
    heartEffect = effect;
    heartEffectUntilMs = nowMs + durationMs;
}

bool MainScene::beginScriptedLeg(float x, float y, uint32_t nowMs) {
    targetX = x;
    targetY = y;
    beginMovement(AiMode::SCRIPTED_MOVE, nowMs);
    return aiMode == AiMode::SCRIPTED_MOVE ||
           (aiMode == AiMode::TURNING && turnNextMode == AiMode::SCRIPTED_MOVE);
}

bool MainScene::beginScriptedMove(RoomAction action, float x, float y, uint32_t nowMs) {
    cancelRoomAction(nowMs);
    roomAction = action;
    roomActionPhase = 0;
    roomActionStartedMs = nowMs;
    roomActionOriginX = monsterX;
    roomActionOriginY = monsterY;
    if (beginScriptedLeg(x, y, nowMs)) return true;
    roomAction = RoomAction::NONE;
    return false;
}

void MainScene::finishRoomAction(uint32_t nowMs) {
    RoomAction completed = roomAction;
    roomAction = RoomAction::NONE;
    roomActionPhase = 0;
    velocityX = 0.0f;
    velocityY = 0.0f;
    if (aiMode == AiMode::SCRIPTED_MOVE ||
        (aiMode == AiMode::TURNING && turnNextMode == AiMode::SCRIPTED_MOVE)) {
        clearMoveRoute();
        aiMode = AiMode::IDLE;
    }
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + (uint32_t)GameRandom::random(650, 1201);
    if (completed == RoomAction::FEED_FINISH) {
        feedingHadTastyBite = false;
        feedingHadDislikedBite = false;
        feedingBecameFull = false;
        feedingMoodAfter = 0;
    }
    if (completed == RoomAction::VOICE_CALL_APPROACH ||
        completed == RoomAction::VOICE_CALL_WAIT) {
        VoiceCallService::ins().clearDetectionQueue(nowMs);
    }
    mind.onActivity(nowMs);
}

void MainScene::cancelRoomAction(uint32_t nowMs) {
    if (roomAction != RoomAction::NONE) {
        finishRoomAction(nowMs);
    }
    heartEffect = HeartEffect::NONE;
    heartEffectUntilMs = 0;
}

bool MainScene::startAttention(uint32_t nowMs) {
    float x = 0.0f;
    float y = 0.0f;
    scheduleAttention(nowMs);
    if (!chooseAttentionPose(x, y)) return false;
    if (fabsf(x - monsterX) < 3.0f && fabsf(y - monsterY) < 3.0f) {
        roomAction = RoomAction::ATTENTION_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + ATTENTION_WAIT_MS;
        pmdDirection = PmdDirection::FRONT;
        showHearts(HeartEffect::ONE, nowMs, ATTENTION_WAIT_MS);
        return true;
    }
    return beginScriptedMove(RoomAction::ATTENTION_APPROACH, x, y, nowMs);
}

bool MainScene::startVoiceCallReaction(uint32_t nowMs) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if (mon.fainted || mon.hpCur == 0 || mon.majorStatus == Game::MajorStatus::SLEEP ||
        aiMode == AiMode::RESTING || aiMode == AiMode::WAKING ||
        aiMode == AiMode::SEEK_BED || aiMode == AiMode::FEEDING ||
        doorTransition != DoorTransitionMode::NONE ||
        progressionModal != ProgressionModal::NONE) {
        return false;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (!chooseAttentionPose(x, y)) return false;
    cancelRoomAction(nowMs);
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    if (fabsf(x - monsterX) < 3.0f && fabsf(y - monsterY) < 3.0f) {
        aiMode = AiMode::IDLE;
        roomAction = RoomAction::VOICE_CALL_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + 1250;
        pmdDirection = PmdDirection::FRONT;
        showHearts(HeartEffect::TWO, nowMs, 1250);
        CryPlayer::ins().replay(mon.speciesId);
        return true;
    }
    return beginScriptedMove(RoomAction::VOICE_CALL_APPROACH, x, y, nowMs);
}

bool MainScene::startWindowGaze(uint32_t nowMs) {
    const Game::MonsterRuntime& current = GameEngine::ins().activeMonster();
    uint32_t nowGameSec = currentGameSeconds();
    if (!windowGazeDue(current.lastExploredAt, current.lastWindowGazeAt, nowGameSec)) return false;

    RoomResource::BehaviorAnchor anchor{};
    if (!room().findBehaviorAnchor(RoomResource::BehaviorAnchorType::WINDOW_GAZE, anchor)) {
        return false;
    }
    float x = 0.0f;
    float y = 0.0f;
    if (!chooseWindowGazePose(anchor, x, y)) return false;

    windowGazeDirection = anchor.facing <= (uint8_t)PmdDirection::DOWN_RIGHT
        ? (PmdDirection)anchor.facing
        : PmdDirection::BACK;
    return beginScriptedMove(RoomAction::WINDOW_APPROACH, x, y, nowMs);
}

bool MainScene::startAutonomousAction(uint32_t nowMs) {
    scheduleSpecialAction(nowMs);
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    bool energeticAllowed =
        behaviorProfile.movementMode != MonsterMovementMode::STATIONARY &&
        (!config || config->walkingFrames >= 2);
    int roll = (int)GameRandom::random(100);
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
    roomActionBaseDirection = pmdDirection;
    if (selected == RoomAction::LOOK_AROUND) {
        roomAction = selected;
        roomActionPhase = 0;
        roomActionUntilMs = nowMs + (uint32_t)GameRandom::random(1100, 1801);
        return true;
    }
    if (selected == RoomAction::QUIET_GAZE) {
        roomAction = selected;
        pmdDirection = PmdDirection::FRONT;
        roomActionUntilMs = nowMs + (uint32_t)GameRandom::random(1400, 2801);
        return true;
    }
    if (selected == RoomAction::DASH) {
        float x = 0.0f;
        float y = 0.0f;
        float preferred = (float)GameRandom::random(0, 6284) / 1000.0f;
        if (chooseNearbyPose(monsterX, monsterY, 20.0f, 32.0f,
                             preferred, PI_F * 0.75f, x, y) &&
            beginScriptedMove(RoomAction::DASH, x, y, nowMs)) {
            return true;
        }
    } else if (selected == RoomAction::STEP_BACK) {
        float x = 0.0f;
        float y = 0.0f;
        if (chooseNearbyPose(monsterX, monsterY, 7.0f, 12.0f,
                             -PI_F * 0.5f, 0.55f, x, y) &&
            beginScriptedMove(RoomAction::STEP_BACK, x, y, nowMs)) {
            return true;
        }
    } else if (selected == RoomAction::CIRCLE) {
        roomAction = RoomAction::CIRCLE;
        roomActionPhase = 0;
        roomActionStartedMs = nowMs;
        roomActionOriginX = monsterX;
        roomActionOriginY = monsterY;
        for (int radius = 11; radius >= 6; --radius) {
            roomActionRadius = (float)radius;
            bool valid = true;
            for (uint8_t phase = 0; phase < 5; ++phase) {
                float checkX = 0.0f;
                float checkY = 0.0f;
                if (!chooseCirclePose(phase, checkX, checkY)) {
                    valid = false;
                    break;
                }
            }
            if (!valid) continue;
            float x = 0.0f;
            float y = 0.0f;
            chooseCirclePose(0, x, y);
            if (beginScriptedLeg(x, y, nowMs)) return true;
            break;
        }
        roomAction = RoomAction::NONE;
    }

    roomAction = RoomAction::LOOK_AROUND;
    roomActionPhase = 0;
    roomActionStartedMs = nowMs;
    roomActionBaseDirection = pmdDirection;
    roomActionUntilMs = nowMs + 1200;
    return true;
}

void MainScene::finishScriptedMovement(uint32_t nowMs) {
    switch (roomAction) {
    case RoomAction::ATTENTION_APPROACH:
        roomAction = RoomAction::ATTENTION_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + ATTENTION_WAIT_MS;
        aiMode = AiMode::IDLE;
        pmdDirection = PmdDirection::FRONT;
        showHearts(HeartEffect::ONE, nowMs, ATTENTION_WAIT_MS);
        return;
    case RoomAction::VOICE_CALL_APPROACH:
        roomAction = RoomAction::VOICE_CALL_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + 1250;
        aiMode = AiMode::IDLE;
        pmdDirection = PmdDirection::FRONT;
        showHearts(HeartEffect::TWO, nowMs, 1250);
        CryPlayer::ins().replay(GameEngine::ins().activeMonster().speciesId);
        return;
    case RoomAction::WINDOW_APPROACH:
        roomAction = RoomAction::WINDOW_WAIT;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + (uint32_t)GameRandom::random((long)WINDOW_GAZE_MIN_MS,
                                                      (long)WINDOW_GAZE_MAX_MS + 1L);
        aiMode = AiMode::IDLE;
        pmdDirection = windowGazeDirection;
        GameEngine::ins().activeMonster().lastWindowGazeAt = currentGameSeconds();
        GameEngine::ins().markDirty(SaveUrgency::DEFERRED);
        return;
    case RoomAction::CIRCLE: {
        roomActionPhase++;
        if (roomActionPhase < 5) {
            float x = 0.0f;
            float y = 0.0f;
            if (chooseCirclePose(roomActionPhase, x, y) && beginScriptedLeg(x, y, nowMs)) {
                return;
            }
        }
        finishRoomAction(nowMs);
        return;
    }
    case RoomAction::PET_WITHDRAW:
    case RoomAction::DASH:
    case RoomAction::STEP_BACK:
    case RoomAction::FEED_FINISH:
        finishRoomAction(nowMs);
        return;
    default:
        finishRoomAction(nowMs);
        return;
    }
}

bool MainScene::updateRoomAction(uint32_t nowMs) {
    switch (roomAction) {
    case RoomAction::NONE:
        return false;
    case RoomAction::PET_HAPPY:
    case RoomAction::PET_CALM:
    case RoomAction::QUIET_GAZE:
    case RoomAction::ATTENTION_WAIT:
    case RoomAction::VOICE_CALL_WAIT:
    case RoomAction::WINDOW_WAIT:
        velocityX = 0.0f;
        velocityY = 0.0f;
        if ((int32_t)(nowMs - roomActionUntilMs) >= 0) finishRoomAction(nowMs);
        return true;
    case RoomAction::LOOK_AROUND: {
        velocityX = 0.0f;
        velocityY = 0.0f;
        uint8_t phase = (uint8_t)MathUtil::min<uint32_t>(2, (nowMs - roomActionStartedMs) / 380UL);
        if (phase != roomActionPhase) roomActionPhase = phase;
        static constexpr int8_t STEPS[] = {-1, 1, 0};
        pmdDirection = (PmdDirection)wrappedDirectionValue(
            (uint8_t)roomActionBaseDirection, STEPS[roomActionPhase]);
        if ((int32_t)(nowMs - roomActionUntilMs) >= 0) {
            pmdDirection = roomActionBaseDirection;
            finishRoomAction(nowMs);
        }
        return true;
    }
    case RoomAction::FEED_FINISH:
        if (roomActionPhase == 0) {
            velocityX = 0.0f;
            velocityY = 0.0f;
            if ((int32_t)(nowMs - roomActionUntilMs) < 0) return true;
            roomActionPhase = 1;
            float awayAngle = atan2f(monsterY - (foodCenterY() - walkBoundaryOffsetY()),
                                     monsterX - foodCenterX());
            float x = 0.0f;
            float y = 0.0f;
            if (chooseNearbyPose(monsterX, monsterY, 10.0f, 18.0f,
                                 awayAngle, PI_F * 0.42f, x, y) &&
                beginScriptedLeg(x, y, nowMs)) {
                return true;
            }
            finishRoomAction(nowMs);
            return true;
        }
        if (aiMode == AiMode::SCRIPTED_MOVE ||
            (aiMode == AiMode::TURNING && turnNextMode == AiMode::SCRIPTED_MOVE)) {
            return false;
        }
        finishRoomAction(nowMs);
        return true;
    case RoomAction::PET_WITHDRAW:
    case RoomAction::ATTENTION_APPROACH:
    case RoomAction::VOICE_CALL_APPROACH:
    case RoomAction::CIRCLE:
    case RoomAction::DASH:
    case RoomAction::STEP_BACK:
    case RoomAction::WINDOW_APPROACH:
        if (aiMode == AiMode::SCRIPTED_MOVE ||
            (aiMode == AiMode::TURNING && turnNextMode == AiMode::SCRIPTED_MOVE)) {
            return false;
        }
        finishRoomAction(nowMs);
        return true;
    }
    return false;
}

void MainScene::startPetReaction(uint32_t nowMs, const PetResult& result) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if (result.outcome == PetOutcome::NEEDS_REST) return;

    const Game::SpeciesCareProfile careProfile =
        Game::speciesCareProfileFor(mon.speciesId);
    bool sleeping = (careProfile.usesBed && mainSceneIsSleepTime()) ||
                    mon.majorStatus == Game::MajorStatus::SLEEP ||
                    aiMode == AiMode::RESTING || aiMode == AiMode::WAKING ||
                    aiMode == AiMode::SEEK_BED;
    if (sleeping || aiMode == AiMode::FEEDING) {
        showHearts(HeartEffect::ONE, nowMs, 900);
        return;
    }

    cancelRoomAction(nowMs);
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::IDLE;
    targetX = monsterX;
    targetY = monsterY;

    bool lowHp = mon.hpMax == 0 || (uint32_t)mon.hpCur * 100UL <= (uint32_t)mon.hpMax * 25UL;
    uint8_t reactionMood = mon.mood >= result.moodGain ? mon.mood - result.moodGain : mon.mood;
    if (careProfile.canMove && !lowHp && reactionMood >= 60) {
        roomAction = RoomAction::PET_HAPPY;
        roomActionStartedMs = nowMs;
        roomActionUntilMs = nowMs + 1150;
        pmdDirection = PmdDirection::FRONT;
        showHearts(HeartEffect::TWO, nowMs, 1250);
        CryPlayer::ins().replay(mon.speciesId);
        return;
    }
    if (careProfile.canMove && !lowHp && reactionMood < 30) {
        float x = 0.0f;
        float y = 0.0f;
        if (chooseNearbyPose(monsterX, monsterY, 8.0f, 12.0f,
                             -1.5707963f, 0.45f, x, y) &&
            beginScriptedMove(RoomAction::PET_WITHDRAW, x, y, nowMs)) {
            showHearts(HeartEffect::ONE, nowMs, 900);
            CryPlayer::ins().replay(mon.speciesId);
            return;
        }
    }

    roomAction = RoomAction::PET_CALM;
    roomActionStartedMs = nowMs;
    roomActionUntilMs = nowMs + 750;
    pmdDirection = PmdDirection::FRONT;
    showHearts(HeartEffect::ONE, nowMs, 900);
    CryPlayer::ins().replay(mon.speciesId);
}

void MainScene::startFeedFinish(uint32_t nowMs) {
    roomAction = RoomAction::FEED_FINISH;
    roomActionPhase = 0;
    roomActionStartedMs = nowMs;
    roomActionUntilMs = nowMs + 650;
    aiMode = AiMode::IDLE;
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;
}

float MainScene::actionRenderYOffset(uint32_t nowMs) const {
    bool happyFeed = roomAction == RoomAction::FEED_FINISH && roomActionPhase == 0 &&
                     !feedingHadDislikedBite &&
                     (feedingHadTastyBite || feedingBecameFull || feedingMoodAfter >= 60 ||
                      behaviorProfile.activityBias > 0);
    bool voiceHappy = roomAction == RoomAction::VOICE_CALL_WAIT;
    if (roomAction != RoomAction::PET_HAPPY && !voiceHappy && !happyFeed) return 0.0f;
    uint32_t elapsed = nowMs - roomActionStartedMs;
    uint32_t hopDuration = (roomAction == RoomAction::PET_HAPPY || voiceHappy) ? 1000UL : 600UL;
    if (elapsed >= hopDuration) return 0.0f;
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    float amplitude = config && config->airHeight > 0.0f ? 3.0f : 5.0f;
    float cycles = (roomAction == RoomAction::PET_HAPPY || voiceHappy) ? 2.0f : 1.0f;
    float phase = (float)elapsed / (float)hopDuration;
    return fabsf(sinf(phase * 3.14159265f * cycles)) * amplitude;
}

void MainScene::traceMainAi(uint32_t nowMs, MainAiTraceGate gate,
                            bool force) {
#if STICKMON_ENABLE_TRACE_LOGS
    bool changed = !mainAiTraceInitialized ||
                   gate != lastMainAiTraceGate ||
                   aiMode != lastMainAiTraceMode;
    if (!force && !changed && (int32_t)(nowMs - nextMainAiTraceMs) < 0) {
        return;
    }

    const char* gateName = "update";
    switch (gate) {
    case MainAiTraceGate::UPDATE: gateName = "update"; break;
    case MainAiTraceGate::PROGRESSION: gateName = "progression"; break;
    case MainAiTraceGate::CONTACT: gateName = "contact"; break;
    case MainAiTraceGate::PAIR: gateName = "pair"; break;
    case MainAiTraceGate::DOOR_TRANSITION: gateName = "door"; break;
    case MainAiTraceGate::STATIONARY_SPECIES: gateName = "stationary_species"; break;
    case MainAiTraceGate::FAINTED_OR_STATUS_SLEEP: gateName = "fainted_or_status_sleep"; break;
    case MainAiTraceGate::FAINT_WAKE: gateName = "faint_wake"; break;
    case MainAiTraceGate::SCHEDULE_WAKE: gateName = "schedule_wake"; break;
    case MainAiTraceGate::CANCEL_BED_ROUTE: gateName = "cancel_bed_route"; break;
    case MainAiTraceGate::DEBUG_TILT: gateName = "debug_tilt"; break;
    case MainAiTraceGate::FEEDING: gateName = "feeding"; break;
    case MainAiTraceGate::WAKING: gateName = "waking"; break;
    case MainAiTraceGate::TURNING: gateName = "turning"; break;
    case MainAiTraceGate::RESTING: gateName = "resting"; break;
    case MainAiTraceGate::ROOM_ACTION: gateName = "room_action"; break;
    case MainAiTraceGate::IDLE_WAIT: gateName = "idle_wait"; break;
    case MainAiTraceGate::ROUTE_FAILED: gateName = "route_failed"; break;
    case MainAiTraceGate::BLOCKED_BY_SECONDARY: gateName = "blocked_by_secondary"; break;
    case MainAiTraceGate::OUTSIDE_WALK_AREA: gateName = "outside_walk_area"; break;
    case MainAiTraceGate::MOVING: gateName = "moving"; break;
    }

    const char* modeName = "idle";
    switch (aiMode) {
    case AiMode::IDLE: modeName = "idle"; break;
    case AiMode::WANDER: modeName = "wander"; break;
    case AiMode::TURNING: modeName = "turning"; break;
    case AiMode::SEEK_FOOD: modeName = "seek_food"; break;
    case AiMode::SEEK_BED: modeName = "seek_bed"; break;
    case AiMode::LEAVING_BED: modeName = "leaving_bed"; break;
    case AiMode::WAKING: modeName = "waking"; break;
    case AiMode::FEEDING: modeName = "feeding"; break;
    case AiMode::RESTING: modeName = "resting"; break;
    case AiMode::SCRIPTED_MOVE: modeName = "scripted_move"; break;
    }

    auto remainingMs = [nowMs](uint32_t deadline) -> long {
        int32_t value = static_cast<int32_t>(deadline - nowMs);
        return value > 0 ? static_cast<long>(value) : 0L;
    };
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(mon.speciesId);
    float actorDistance = -1.0f;
    if (visitor.active) {
        ActorGeometry mainActor = mainGeometry();
        ActorGeometry secondaryActor = visitorGeometry();
        float dx = visitor.x - monsterX;
        float dy = (visitor.y + secondaryActor.groundOffsetY) -
                   (monsterY + mainActor.groundOffsetY);
        actorDistance = sqrtf(dx * dx + dy * dy);
    }

    Platform::logf(
        "[MainAI] t=%lu gate=%s mode=%s species=%u canMove=%u desire=%u "
        "action=%u pos=%.1f,%.1f target=%.1f,%.1f vel=%.1f,%.1f "
        "route=%u/%u inside=%u decision_in=%ldms state_in=%ldms "
        "sleepTime=%u faint=%u status=%u hp=%u/%u hun=%u mood=%u "
        "visitor=%u vstate=%u vpos=%.1f,%.1f actorDist=%.1f "
        "bowl=%u eater=%d preferred=%d vhun=%d "
        "debugTilt=%u blockers=%u/%u/%u/%u\n",
        static_cast<unsigned long>(nowMs), gateName, modeName,
        static_cast<unsigned>(mon.speciesId), care.canMove ? 1U : 0U,
        static_cast<unsigned>(mind.topDesire()),
        static_cast<unsigned>(roomAction), monsterX, monsterY,
        targetX, targetY, velocityX, velocityY,
        static_cast<unsigned>(moveRouteIndex),
        static_cast<unsigned>(moveRouteCount),
        monsterFootprintInsideWalkArea(monsterX, monsterY) ? 1U : 0U,
        remainingMs(nextAiDecisionMs), remainingMs(stateUntilMs),
        mainSceneIsSleepTime() ? 1U : 0U,
        (mon.fainted || mon.hpCur == 0) ? 1U : 0U,
        static_cast<unsigned>(mon.majorStatus),
        static_cast<unsigned>(mon.hpCur), static_cast<unsigned>(mon.hpMax),
        static_cast<unsigned>(mon.satiety), static_cast<unsigned>(mon.mood),
        visitor.active ? 1U : 0U, static_cast<unsigned>(visitor.state),
        visitor.x, visitor.y, actorDistance,
        GameEngine::ins().bowlHasFood() ? 1U : 0U,
        static_cast<int>(bowlEaterSlot),
        static_cast<int>(preferredBowlEater()),
        visitor.active && GameEngine::ins().gameState().teamCount >= 2
            ? static_cast<int>(GameEngine::ins().gameState().team[1].satiety)
            : -1,
        GameEngine::ins().debugTiltControlEnabled() ? 1U : 0U,
        static_cast<unsigned>(progressionModal),
        static_cast<unsigned>(contactDialog),
        static_cast<unsigned>(pairInteraction),
        static_cast<unsigned>(doorTransition));

    mainAiTraceInitialized = true;
    lastMainAiTraceGate = gate;
    lastMainAiTraceMode = aiMode;
    nextMainAiTraceMs = nowMs + MAIN_AI_TRACE_INTERVAL_MS;
#else
    (void)nowMs;
    (void)gate;
    (void)force;
#endif
}

void MainScene::updateMonsterAi(uint32_t nowMs, float dtSeconds) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    const Game::SpeciesCareProfile careProfile =
        Game::speciesCareProfileFor(mon.speciesId);
    bool mainFoodAction =
        aiMode == AiMode::SEEK_FOOD || aiMode == AiMode::FEEDING ||
        (aiMode == AiMode::TURNING &&
         (turnNextMode == AiMode::SEEK_FOOD ||
          turnNextMode == AiMode::FEEDING));
    if (bowlEaterSlot == 0 && !mainFoodAction) releaseBowl(0);
    if (!careProfile.canMove) {
        if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
            randomMonsterCenterWalkPoint(monsterX, monsterY);
        }
        if (roomAction != RoomAction::NONE &&
            roomAction != RoomAction::PET_CALM) {
            cancelRoomAction(nowMs);
        } else if (roomAction == RoomAction::PET_CALM) {
            updateRoomAction(nowMs);
        }
        clearMoveRoute();
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        pmdAction = PmdAction::IDLE;
        pmdDirection = PmdDirection::FRONT;
        feedingConsumed = false;
        faintRestActive = false;
        traceMainAi(nowMs, MainAiTraceGate::STATIONARY_SPECIES);
        return;
    }
    bool currentlyFainted = mon.fainted || mon.hpCur == 0;
    if (currentlyFainted || mon.majorStatus == Game::MajorStatus::SLEEP) {
        cancelRoomAction(nowMs);
        if (currentlyFainted) faintRestActive = true;
        if (!monsterAtBedSleepPose()) snapMonsterToBed();
        clearMoveRoute();
        feedingConsumed = false;
        velocityX = 0.0f;
        velocityY = 0.0f;
        aiMode = AiMode::RESTING;
        traceMainAi(nowMs, MainAiTraceGate::FAINTED_OR_STATUS_SLEEP);
        return;
    }

    bool sleepTime = mainSceneIsSleepTime();
    if (faintRestActive && !sleepTime) {
        cancelRoomAction(nowMs);
        faintRestActive = false;
        postFeedAwakeUntilMs = nowMs + POST_FAINT_AWAKE_MS;
        mind.onActivity(nowMs);
        beginWaking(nowMs, false);
        traceMainAi(nowMs, MainAiTraceGate::FAINT_WAKE);
        return;
    }

    if (!sleepTime && aiMode == AiMode::RESTING) {
        cancelRoomAction(nowMs);
        mind.onActivity(nowMs);
        beginWaking(nowMs, false);
        traceMainAi(nowMs, MainAiTraceGate::SCHEDULE_WAKE);
        return;
    }
    if (!sleepTime && aiMode == AiMode::SEEK_BED) {
        cancelRoomAction(nowMs);
        clearMoveRoute();
        velocityX = 0.0f;
        velocityY = 0.0f;
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + GameRandom::random(700, 1401);
        mind.onActivity(nowMs);
        traceMainAi(nowMs, MainAiTraceGate::CANCEL_BED_ROUTE);
        return;
    }

    bool debugTilt = GameEngine::ins().debugTiltControlEnabled();
    updateMind(nowMs);
    bool bedRestActive = !debugTilt &&
        (aiMode == AiMode::RESTING || aiMode == AiMode::WAKING || aiMode == AiMode::LEAVING_BED);
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(bedRestActive && (aiMode == AiMode::LEAVING_BED || monsterAtBedSleepPose()))) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
        targetX = monsterX;
        targetY = monsterY;
        clearMoveRoute();
        aiMode = AiMode::IDLE;
    }

    if (debugTilt) {
        cancelRoomAction(nowMs);
        if (aiMode != AiMode::IDLE && aiMode != AiMode::WANDER) {
            clearMoveRoute();
            aiMode = AiMode::IDLE;
        }
        float prevX = monsterX;
        float prevY = monsterY;
        updateDebugTiltControl(nowMs, dtSeconds);
        float walkOffsetY = walkBoundaryOffsetY();
        monsterX = clampf(monsterX, (float)roomWalkMinX(), (float)roomWalkMaxX());
        monsterY = clampf(monsterY, (float)roomWalkMinY() - walkOffsetY,
                          (float)roomWalkMaxY() - walkOffsetY);
        if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
            monsterX = prevX;
            monsterY = prevY;
            velocityX = 0.0f;
            velocityY = 0.0f;
        }
        traceMainAi(nowMs, MainAiTraceGate::DEBUG_TILT);
        return;
    }

    if (aiMode == AiMode::FEEDING) {
        updateFeeding(nowMs);
        traceMainAi(nowMs, MainAiTraceGate::FEEDING);
        return;
    }

    if (aiMode == AiMode::WAKING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        if ((int32_t)(nowMs - stateUntilMs) < 0) {
            traceMainAi(nowMs, MainAiTraceGate::WAKING);
            return;
        }
        if (!monsterFootprintInsideWalkArea(monsterX, monsterY)) {
            if (chooseBedApproachPose(targetX, targetY)) {
                beginMovement(AiMode::LEAVING_BED, nowMs);
            } else {
                randomMonsterCenterWalkPoint(monsterX, monsterY);
                aiMode = AiMode::IDLE;
                nextAiDecisionMs = nowMs + 700;
            }
            traceMainAi(nowMs, MainAiTraceGate::WAKING);
            return;
        }
        if (wakingForFood && GameEngine::ins().bowlHasFood() &&
            mon.satiety < MONSTER_FEED_TARGET_SATIETY) {
            setFoodTarget(nowMs);
        } else {
            aiMode = AiMode::IDLE;
            nextAiDecisionMs = nowMs + GameRandom::random(700, 1401);
        }
        traceMainAi(nowMs, MainAiTraceGate::WAKING);
        return;
    }

    if (aiMode == AiMode::TURNING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        if ((int32_t)(nowMs - stateUntilMs) < 0) {
            traceMainAi(nowMs, MainAiTraceGate::TURNING);
            return;
        }
        pmdDirection = turnTargetDirection;
        aiMode = turnNextMode;
        lastMoveProgressMs = nowMs;
        lastWaypointDistance = 1000000.0f;
        traceMainAi(nowMs, MainAiTraceGate::TURNING);
        return;
    }

    if (aiMode == AiMode::RESTING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        bool wantsFood = monsterShouldWakeForFood(mon.satiety) &&
                         GameEngine::ins().bowlHasFood();
        if (wantsFood) {
            beginWaking(nowMs, true);
        }
        traceMainAi(nowMs, MainAiTraceGate::RESTING);
        return;
    }

    bool contextualAction = roomAction == RoomAction::ATTENTION_APPROACH ||
                            roomAction == RoomAction::ATTENTION_WAIT ||
                            roomAction == RoomAction::VOICE_CALL_APPROACH ||
                            roomAction == RoomAction::VOICE_CALL_WAIT ||
                            roomAction == RoomAction::LOOK_AROUND ||
                            roomAction == RoomAction::CIRCLE ||
                            roomAction == RoomAction::DASH ||
                            roomAction == RoomAction::STEP_BACK ||
                            roomAction == RoomAction::QUIET_GAZE ||
                            roomAction == RoomAction::WINDOW_APPROACH ||
                            roomAction == RoomAction::WINDOW_WAIT;
    bool urgentNeed = mon.hpMax == 0 ||
                      (uint32_t)mon.hpCur * 100UL <= (uint32_t)mon.hpMax * 25UL ||
                      mon.satiety <= 25 || sleepTime ||
                      (GameEngine::ins().bowlHasFood() &&
                       mon.satiety < MONSTER_FEED_TARGET_SATIETY &&
                       mind.topDesire() == MonsterDesire::EAT);
    if (contextualAction && urgentNeed) cancelRoomAction(nowMs);

    if (roomAction != RoomAction::NONE && updateRoomAction(nowMs)) {
        traceMainAi(nowMs, MainAiTraceGate::ROOM_ACTION);
        return;
    }

    if (ambientActionAllowed()) {
        if (startWindowGaze(nowMs)) {
            traceMainAi(nowMs, MainAiTraceGate::ROOM_ACTION);
            return;
        }
        if ((int32_t)(nowMs - nextAttentionMs) >= 0 && startAttention(nowMs)) {
            traceMainAi(nowMs, MainAiTraceGate::ROOM_ACTION);
            return;
        }
        if ((int32_t)(nowMs - nextSpecialActionMs) >= 0 && startAutonomousAction(nowMs)) {
            traceMainAi(nowMs, MainAiTraceGate::ROOM_ACTION);
            return;
        }
    }

    if (aiMode == AiMode::IDLE && (int32_t)(nowMs - nextAiDecisionMs) >= 0) {
        chooseAiGoal(nowMs);
    }
    if (aiMode == AiMode::IDLE || aiMode == AiMode::TURNING || aiMode == AiMode::WAKING ||
        aiMode == AiMode::FEEDING || aiMode == AiMode::RESTING) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        traceMainAi(nowMs, MainAiTraceGate::IDLE_WAIT);
        return;
    }

    float waypointX = targetX;
    float waypointY = targetY;
    if (!currentWaypoint(waypointX, waypointY) &&
        !buildMoveRoute(targetX, targetY)) {
        abortMovement(nowMs, 1200);
        traceMainAi(nowMs, MainAiTraceGate::ROUTE_FAILED, true);
        return;
    }
    currentWaypoint(waypointX, waypointY);

    float prevX = monsterX;
    float prevY = monsterY;
    float dx = waypointX - monsterX;
    float dy = waypointY - monsterY;
    float dist = sqrtf(dx * dx + dy * dy);
    float speed = (aiMode == AiMode::SEEK_FOOD || aiMode == AiMode::SEEK_BED ||
                   aiMode == AiMode::LEAVING_BED) ? 19.0f : 10.5f;
    if (aiMode == AiMode::SCRIPTED_MOVE && roomAction == RoomAction::DASH) speed *= 1.55f;
    if (mon.mood < 40 || mon.satiety < 20) speed *= 0.72f;
    speed *= behaviorProfile.moveSpeedScale;
    float step = speed * dtSeconds;
    if (dist < 1.2f || step >= dist) {
        if (visitor.active && !doorVisitorHidden &&
            !doorStepKeepsSpacing(monsterX, monsterY,
                                  waypointX, waypointY,
                                  visitor.x, visitor.y, false)) {
            velocityX = 0.0f;
            velocityY = 0.0f;
            updateStuckWatchdog(nowMs, dist);
            traceMainAi(nowMs, MainAiTraceGate::BLOCKED_BY_SECONDARY);
            return;
        }
        monsterX = waypointX;
        monsterY = waypointY;
        moveRouteIndex++;
        if (moveRouteIndex >= moveRouteCount) {
            finishMovement(nowMs);
            return;
        }
        float nextX = moveRouteX[moveRouteIndex];
        float nextY = moveRouteY[moveRouteIndex];
        PmdDirection nextDirection = pmdDirectionForVelocity(nextX - monsterX, nextY - monsterY);
        uint8_t directionDelta = (uint8_t)abs((int)nextDirection - (int)pmdDirection);
        if (directionDelta > 4) directionDelta = 8 - directionDelta;
        if (directionDelta >= 2) beginTurn(aiMode, nextDirection, nowMs);
        return;
    }

    velocityX = dx / dist * speed;
    velocityY = dy / dist * speed * 0.75f;
    float nextMonsterX = monsterX + velocityX * dtSeconds;
    float nextMonsterY = monsterY + velocityY * dtSeconds;
    if (visitor.active && !doorVisitorHidden &&
        !doorStepKeepsSpacing(monsterX, monsterY,
                              nextMonsterX, nextMonsterY,
                              visitor.x, visitor.y, false)) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        updateStuckWatchdog(nowMs, dist);
        traceMainAi(nowMs, MainAiTraceGate::BLOCKED_BY_SECONDARY);
        return;
    }
    monsterX = nextMonsterX;
    monsterY = nextMonsterY;
    if (fabsf(dx) > 8.0f) facingRight = dx > 0.0f;

    float walkOffsetY = walkBoundaryOffsetY();
    monsterX = clampf(monsterX, (float)roomWalkMinX(), (float)roomWalkMaxX());
    monsterY = clampf(monsterY, (float)roomWalkMinY() - walkOffsetY,
                      (float)roomWalkMaxY() - walkOffsetY);
    bool wasInsideWalkArea = monsterFootprintInsideWalkArea(prevX, prevY);
    if (!monsterFootprintInsideWalkArea(monsterX, monsterY) &&
        !(aiMode == AiMode::LEAVING_BED && !wasInsideWalkArea)) {
        monsterX = prevX;
        monsterY = prevY;
        velocityX = 0.0f;
        velocityY = 0.0f;
        if (!buildMoveRoute(targetX, targetY)) {
            abortMovement(nowMs, GameRandom::random(1200, 2601));
        }
        traceMainAi(nowMs, MainAiTraceGate::OUTSIDE_WALK_AREA, true);
        return;
    }
    traceMainAi(nowMs, MainAiTraceGate::MOVING);
    updateStuckWatchdog(nowMs, sqrtf((waypointX - monsterX) * (waypointX - monsterX) +
                                     (waypointY - monsterY) * (waypointY - monsterY)));
}

bool MainScene::monsterNearFood() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    float toleranceX = MathUtil::max(9.0f, walkFootprintRadiusX() + 8.0f);
    float toleranceY = MathUtil::max(7.0f, walkFootprintRadiusY() + 6.0f);
    return fabsf(monsterX - foodFeedX()) <= toleranceX &&
           fabsf(walkY - foodFeedY()) <= toleranceY;
}

bool MainScene::monsterNearBed() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - bedCenterX()) <= BED_APPROACH_TOLERANCE_X &&
           fabsf(walkY - bedCenterY()) <= BED_APPROACH_TOLERANCE_Y;
}

bool MainScene::monsterAtBedSleepPose() const {
    float walkY = monsterY + walkBoundaryOffsetY();
    return fabsf(monsterX - bedSleepX()) <= BED_SLEEP_TOLERANCE_X &&
           fabsf(walkY - bedSleepY()) <= BED_SLEEP_TOLERANCE_Y;
}

bool MainScene::monsterNeedsBedRest() const {
    if (!Game::speciesCareProfileFor(
             GameEngine::ins().activeMonster().speciesId).usesBed) {
        return false;
    }
    uint32_t nowMs = Hal::ins().millis();
    if ((int32_t)(nowMs - postFeedAwakeUntilMs) < 0) return false;
    return mainSceneIsSleepTime() && mind.topDesire() == MonsterDesire::REST;
}

void MainScene::updateMind(uint32_t nowMs) {
    if ((int32_t)(nowMs - nextMindUpdateMs) < 0) return;
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    const Game::SpeciesCareProfile careProfile =
        Game::speciesCareProfileFor(mon.speciesId);
    mind.update(mon, careProfile.usesBed && mainSceneIsSleepTime(),
                careProfile.needsFood && GameEngine::ins().bowlHasFood(),
                nowMs);
    nextMindUpdateMs = nowMs + MIND_UPDATE_MS;
}

void MainScene::beginMovement(AiMode mode, uint32_t nowMs) {
    if (!buildMoveRoute(targetX, targetY)) {
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + GameRandom::random(1200, 2601);
        traceMainAi(nowMs, MainAiTraceGate::ROUTE_FAILED, true);
        return;
    }
    beginPreparedMovement(mode, nowMs);
}

void MainScene::beginPreparedMovement(AiMode mode, uint32_t nowMs) {
    float waypointX = targetX;
    float waypointY = targetY;
    currentWaypoint(waypointX, waypointY);
    PmdDirection direction = pmdDirectionForVelocity(waypointX - monsterX, waypointY - monsterY);
    uint8_t directionDelta = (uint8_t)abs((int)direction - (int)pmdDirection);
    if (directionDelta > 4) directionDelta = 8 - directionDelta;
    if (directionDelta >= 2) {
        beginTurn(mode, direction, nowMs);
    } else {
        aiMode = mode;
    }
    lastWaypointDistance = 1000000.0f;
    lastMoveProgressMs = nowMs;
    stuckRecoveryCount = 0;
}

void MainScene::beginTurn(AiMode nextMode, PmdDirection direction, uint32_t nowMs) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    turnNextMode = nextMode;
    turnTargetDirection = direction;
    stateUntilMs = nowMs + behaviorProfile.turnPauseMs;
    aiMode = AiMode::TURNING;
}

void MainScene::beginWaking(uint32_t nowMs, bool forFood) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    wakingForFood = forFood;
    uint16_t delayMs = mainSceneIsSleepTime()
        ? (uint16_t)GameRandom::random(NIGHT_FEED_WAKE_DELAY_MIN_MS, NIGHT_FEED_WAKE_DELAY_MAX_MS + 1)
        : (uint16_t)GameRandom::random(DAY_WAKE_DELAY_MIN_MS, DAY_WAKE_DELAY_MAX_MS + 1);
    stateUntilMs = nowMs + delayMs;
    aiMode = AiMode::WAKING;
}

void MainScene::enterResting(uint32_t nowMs) {
    cancelRoomAction(nowMs);
    snapMonsterToBed();
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    aiMode = AiMode::RESTING;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + 2000;
    mind.onRested(nowMs);
}

void MainScene::finishMovement(uint32_t nowMs) {
    AiMode completedMode = aiMode;
    if (mainYieldingForVisitorFood) {
        mainYieldingForVisitorFood = false;
        clearVisitorFoodRouteFailure();
    }
    clearMoveRoute();
    velocityX = 0.0f;
    velocityY = 0.0f;
    if (completedMode == AiMode::SCRIPTED_MOVE) {
        finishScriptedMovement(nowMs);
        return;
    }
    if (completedMode == AiMode::SEEK_FOOD) {
        if (GameEngine::ins().bowlHasFood() &&
            GameEngine::ins().activeMonster().satiety < MONSTER_FEED_TARGET_SATIETY) {
            enterFeeding(nowMs);
            return;
        }
        releaseBowl(0);
    }
    if (completedMode == AiMode::SEEK_BED) {
        enterResting(nowMs);
        return;
    }
    if (completedMode == AiMode::LEAVING_BED) {
        if (wakingForFood && GameEngine::ins().bowlHasFood() &&
            GameEngine::ins().activeMonster().satiety < MONSTER_FEED_TARGET_SATIETY) {
            setFoodTarget(nowMs);
        } else {
            aiMode = AiMode::IDLE;
            targetX = monsterX;
            targetY = monsterY;
            nextAiDecisionMs = nowMs + GameRandom::random(700, 1401);
        }
        return;
    }
    aiMode = AiMode::IDLE;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + GameRandom::random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
    mind.onActivity(nowMs);
}

void MainScene::setFoodTarget(uint32_t nowMs) {
    if (!claimBowl(0)) {
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + 900;
        startMainFoodYield(nowMs);
        return;
    }
    targetX = foodFeedX();
    targetY = foodFeedY() - walkBoundaryOffsetY();
    if (!buildFoodApproachRoute(true, targetX, targetY)) {
        releaseBowl(0);
        startVisitorFoodYield(nowMs);
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + 700;
        return;
    }
    beginPreparedMovement(AiMode::SEEK_FOOD, nowMs);
    if (aiMode == AiMode::IDLE) {
        releaseBowl(0);
        startVisitorFoodYield(nowMs);
        nextAiDecisionMs = nowMs + 700;
    }
}

void MainScene::setBedTarget(uint32_t nowMs) {
    if (!chooseBedApproachPose(targetX, targetY)) {
        randomMonsterCenterWalkPoint(targetX, targetY);
    }
    beginMovement(AiMode::SEEK_BED, nowMs);
}

void MainScene::snapMonsterToBed() {
    if (!chooseBedSleepPose(monsterX, monsterY)) {
        randomMonsterCenterWalkPoint(monsterX, monsterY);
    }
    targetX = monsterX;
    targetY = monsterY;
}

void MainScene::enterFeeding(uint32_t nowMs) {
    if (!claimBowl(0)) {
        aiMode = AiMode::IDLE;
        nextAiDecisionMs = nowMs + 900;
        startMainFoodYield(nowMs);
        return;
    }
    cancelRoomAction(nowMs);
    feedingConsumed = false;
    feedingHadTastyBite = false;
    feedingHadDislikedBite = false;
    feedingBecameFull = false;
    feedingMoodAfter = GameEngine::ins().activeMonster().mood;
    feedingBiteMs = nowMs + (uint32_t)GameRandom::random(FEED_BITE_DELAY_MIN_MS, FEED_BITE_DELAY_MAX_MS + 1);
    feedingUntilMs = nowMs + (uint32_t)GameRandom::random(FEED_SESSION_MIN_MS, FEED_SESSION_MAX_MS + 1);
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;
    PmdDirection foodDirection = pmdDirectionForVelocity(
        foodCenterX() - monsterX,
        foodCenterY() - (monsterY + walkBoundaryOffsetY()));
    facingRight = foodCenterX() > monsterX;
    uint8_t directionDelta = (uint8_t)abs((int)foodDirection - (int)pmdDirection);
    if (directionDelta > 4) directionDelta = 8 - directionDelta;
    if (directionDelta >= 2) {
        beginTurn(AiMode::FEEDING, foodDirection, nowMs);
    } else {
        pmdDirection = foodDirection;
        aiMode = AiMode::FEEDING;
    }
}

void MainScene::updateFeeding(uint32_t nowMs) {
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    if ((int32_t)(nowMs - feedingBiteMs) >= 0 &&
        GameEngine::ins().bowlHasFood() && mon.satiety < MONSTER_FEED_TARGET_SATIETY) {
        FoodConsumeResult result = GameEngine::ins().consumeBowlFood(0);
        feedingConsumed = feedingConsumed || result.consumed;
        feedingBiteMs = nowMs + (uint32_t)GameRandom::random(FEED_BITE_INTERVAL_MIN_MS,
                                                   FEED_BITE_INTERVAL_MAX_MS + 1);
        if (result.consumed) {
            hungerAnimFrom = result.satietyBefore;
            hungerAnimTo = result.satietyAfter;
            hungerAnimStartedMs = nowMs;
            hungerAnimUntilMs = nowMs + HUNGER_ANIM_MS;
            feedingMoodAfter = result.moodAfter;
            feedingHadTastyBite = feedingHadTastyBite || result.foodIndex == 1 ||
                                  result.reaction == FoodReaction::LIKED;
            feedingHadDislikedBite = feedingHadDislikedBite ||
                                     result.reaction == FoodReaction::DISLIKED;
            feedingBecameFull = feedingBecameFull || result.becameFull;
            if (result.reaction == FoodReaction::LIKED) {
                showHearts(HeartEffect::TWO, nowMs, 1250);
            } else if (result.foodIndex == 1) {
                showHearts(HeartEffect::ONE, nowMs, 900);
            } else if (result.reaction == FoodReaction::DISLIKED) {
                // 不合口味：没有爱心，下一口停顿更久，肉眼可见的犹豫。
                feedingBiteMs = nowMs + (uint32_t)GameRandom::random(FEED_BITE_INTERVAL_MIN_MS,
                                                         FEED_BITE_INTERVAL_MAX_MS + 1) + 900;
            }
        } else {
            feedingUntilMs = nowMs + 600;
        }
    }
    if (!GameEngine::ins().bowlHasFood() || mon.satiety >= MONSTER_FEED_TARGET_SATIETY) {
        if ((int32_t)(feedingUntilMs - (nowMs + 700)) > 0) feedingUntilMs = nowMs + 700;
    }

    if ((int32_t)(nowMs - feedingUntilMs) < 0) return;

    if (feedingConsumed) {
        postFeedAwakeUntilMs = nowMs + (uint32_t)GameRandom::random(POST_FEED_AWAKE_MIN_MS, POST_FEED_AWAKE_MAX_MS + 1);
    }
    mind.onAte(nowMs);
    releaseBowl(0);
    if (feedingConsumed) {
        feedingConsumed = false;
        startFeedFinish(nowMs);
        return;
    }
    aiMode = AiMode::IDLE;
    nextAiDecisionMs = nowMs + GameRandom::random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
}

void MainScene::updateDebugTiltControl(uint32_t nowMs, float dtSeconds) {
    float ax = 0.0f;
    float ay = 0.0f;
    float az = 0.0f;
    if (!Hal::ins().readAccel(ax, ay, az)) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        return;
    }

    auto applyDeadzone = [](float value) {
        if (fabsf(value) < DEBUG_TILT_DEADZONE) return 0.0f;
        return clampf(value, -DEBUG_TILT_MAX, DEBUG_TILT_MAX) / DEBUG_TILT_MAX;
    };

    // StickS3 is rendered in landscape rotation=1. Map raw IMU axes to screen
    // coordinates so positive input means right/down on the room canvas.
    float inputX = applyDeadzone(-ax);
    float inputY = applyDeadzone(ay);
    float inputLength = sqrtf(inputX * inputX + inputY * inputY);
    if (inputLength > 1.0f) {
        inputX /= inputLength;
        inputY /= inputLength;
    }

    velocityX = inputX * DEBUG_TILT_SPEED;
    velocityY = inputY * DEBUG_TILT_SPEED * 0.75f;
    if (fabsf(velocityX) < PMD_MOVING_SPEED_EPSILON &&
        fabsf(velocityY) < PMD_MOVING_SPEED_EPSILON) {
        velocityX = 0.0f;
        velocityY = 0.0f;
        targetX = monsterX;
        targetY = monsterY;
        aiMode = AiMode::IDLE;
        return;
    }

    monsterX += velocityX * dtSeconds;
    monsterY += velocityY * dtSeconds;
    targetX = monsterX + velocityX * 0.25f;
    targetY = monsterY + velocityY * 0.25f;
    aiMode = AiMode::WANDER;
    nextAiDecisionMs = nowMs + 1000;
    if (fabsf(velocityX) > 8.0f) facingRight = velocityX > 0.0f;
}

void MainScene::updatePmdSpriteState(uint32_t nowMs) {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config) return;
    const PokemonMotion::Behavior motionBehavior =
        PokemonMotion::behaviorForSpecies(config->speciesId);
    const bool slithering = motionBehavior.mode == PokemonMotion::Mode::SLITHER;

    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    bool sleeping = mon.majorStatus == Game::MajorStatus::SLEEP ||
                    mon.fainted ||
                    mon.hpCur == 0 ||
                    aiMode == AiMode::RESTING ||
                    (aiMode == AiMode::WAKING && (int32_t)(nowMs - stateUntilMs) < 0);
    float speedSq = velocityX * velocityX + velocityY * velocityY;
    bool moving = speedSq > PMD_MOVING_SPEED_EPSILON * PMD_MOVING_SPEED_EPSILON;
    PmdAction nextAction = sleeping ? PmdAction::SLEEPING : (moving ? PmdAction::WALKING : PmdAction::IDLE);
    PmdDirection nextDirection = moving ? pmdDirectionForVelocity(velocityX, velocityY) : pmdDirection;
    auto resetMoveLength = [&]() {
        float dx = targetX - monsterX;
        float dy = targetY - monsterY;
        pmdLongMove = sqrtf(dx * dx + dy * dy) > PMD_SHORT_MOVE_DISTANCE;
    };
    auto clearMotionPose = [&]() {
        pmdMotionPhase = PokemonMotion::SLITHER_IDLE_PHASE_INDEX;
        pmdRenderOffsetX = 0;
        pmdRenderOffsetY = 0;
    };
    auto applySlitherPose = [&](uint32_t elapsedMs) {
        PokemonMotion::Pose pose = PokemonMotion::slitherPose(
            motionBehavior, config->walkingFrames, elapsedMs,
            pmdMotionCycleMs, static_cast<uint8_t>(pmdDirection));
        pmdFrame = pose.frameIndex;
        pmdMotionPhase = pose.phaseIndex;
        pmdRenderOffsetX = pose.offsetX;
        pmdRenderOffsetY = pose.offsetY;
        return pose.directionChangeSafe;
    };
    auto beginWalking = [&](PmdDirection direction) {
        pmdAction = PmdAction::WALKING;
        pmdDirection = direction;
        pmdFrame = 0;
        resetMoveLength();
        pmdFrameStartedMs = nowMs;
        if (slithering) {
            pmdMotionCycleMs = PokemonMotion::cycleDurationMs(
                motionBehavior, PokemonMotion::PlaybackContext::AMBIENT,
                sqrtf(speedSq));
            applySlitherPose(0);
        } else {
            clearMotionPose();
        }
    };

    if (pmdAction == PmdAction::STOPPING) {
        if (nextAction == PmdAction::WALKING) {
            beginWalking(nextDirection);
            return;
        }
        if (nextAction == PmdAction::SLEEPING) {
            pmdAction = nextAction;
            pmdDirection = nextDirection;
            pmdFrame = 0;
            pmdLongMove = false;
            clearMotionPose();
            pmdFrameStartedMs = nowMs;
            return;
        }
        uint16_t stoppingFrameMs = slithering
            ? PokemonMotion::slitherSettleFrameMs(pmdMotionCycleMs)
            : motionBehavior.moveFrameMs;
        if (stoppingFrameMs == 0) stoppingFrameMs = 1;
        uint8_t frameCount = pmdStoppingPlaybackFrameCount(config);
        while (nowMs - pmdFrameStartedMs >= stoppingFrameMs) {
            if (pmdFrame + 1 < frameCount) {
                pmdFrame++;
                if (slithering && pmdFrame > 0) clearMotionPose();
                pmdFrameStartedMs += stoppingFrameMs;
            } else {
                pmdAction = PmdAction::IDLE;
                pmdFrame = 0;
                pmdLongMove = false;
                clearMotionPose();
                pmdFrameStartedMs = nowMs;
                break;
            }
        }
        return;
    }

    PmdMotionMode motionMode = pmdMotionModeForConfig(config);
    if (pmdAction == PmdAction::WALKING &&
        nextAction == PmdAction::IDLE &&
        ((motionMode == PmdMotionMode::START_HOLD_END && config->walkingFrames >= 3) ||
         (motionMode == PmdMotionMode::PINGPONG && config->walkingFrames >= 2))) {
        if (slithering &&
            pmdMotionPhase == PokemonMotion::SLITHER_IDLE_PHASE_INDEX) {
            pmdAction = PmdAction::IDLE;
            pmdFrame = 0;
            pmdLongMove = false;
            clearMotionPose();
            pmdFrameStartedMs = nowMs;
            return;
        }
        pmdAction = PmdAction::STOPPING;
        pmdFrame = 0;
        if (slithering) {
            uint32_t settlePhaseMs =
                PokemonMotion::slitherReturnPhaseStartMs(pmdMotionCycleMs);
            PokemonMotion::Pose settlePose = PokemonMotion::slitherPose(
                motionBehavior, config->walkingFrames, settlePhaseMs,
                pmdMotionCycleMs, static_cast<uint8_t>(pmdDirection));
            pmdMotionPhase = settlePose.phaseIndex;
            pmdRenderOffsetX = settlePose.offsetX;
            pmdRenderOffsetY = settlePose.offsetY;
        }
        pmdFrameStartedMs = nowMs;
        return;
    }

    if (nextAction != pmdAction) {
        if (nextAction == PmdAction::WALKING) {
            beginWalking(nextDirection);
        } else {
            pmdAction = nextAction;
            pmdDirection = nextDirection;
            pmdFrame = 0;
            pmdLongMove = false;
            clearMotionPose();
            pmdFrameStartedMs = nowMs;
        }
        return;
    }

    if (pmdAction == PmdAction::WALKING && slithering) {
        uint32_t elapsedMs = nowMs - pmdFrameStartedMs;
        bool directionChangeSafe = applySlitherPose(elapsedMs);
        if (nextDirection != pmdDirection && directionChangeSafe) {
            pmdDirection = nextDirection;
            applySlitherPose(elapsedMs);
        }
        return;
    }

    if (nextDirection != pmdDirection) {
        pmdDirection = nextDirection;
        pmdFrame = 0;
        if (pmdAction == PmdAction::WALKING) resetMoveLength();
        clearMotionPose();
        pmdFrameStartedMs = nowMs;
        return;
    }

    uint16_t frameMs = config->idleFrameMs;
    uint8_t frameCount = config->idleFrames;
    bool loops = true;
    if (pmdAction == PmdAction::WALKING) {
        frameMs = motionBehavior.moveFrameMs;
        frameCount = pmdWalkingPlaybackFrameCount(config, pmdLongMove);
        loops = pmdWalkingPlaybackLoops(config);
    } else if (pmdAction == PmdAction::SLEEPING) {
        frameMs = PMD_SLEEPING_FRAME_MS;
        frameCount = config->sleepingFrames;
    }
    if (frameCount == 0) frameCount = 1;
    while (nowMs - pmdFrameStartedMs >= frameMs) {
        if (loops) {
            pmdFrame = (uint8_t)((pmdFrame + 1) % frameCount);
        } else if (pmdFrame + 1 < frameCount) {
            pmdFrame++;
        }
        pmdFrameStartedMs += frameMs;
    }
}

MainScene::PmdDirection MainScene::pmdDirectionForVelocity(float vx, float vy) const {
    float degrees = atan2f(vy, vx) * 57.2957795f;
    if (degrees >= -22.5f && degrees < 22.5f) return PmdDirection::RIGHT;
    if (degrees >= 22.5f && degrees < 67.5f) return PmdDirection::DOWN_RIGHT;
    if (degrees >= 67.5f && degrees < 112.5f) return PmdDirection::FRONT;
    if (degrees >= 112.5f && degrees < 157.5f) return PmdDirection::DOWN_LEFT;
    if (degrees >= -67.5f && degrees < -22.5f) return PmdDirection::UP_RIGHT;
    if (degrees >= -112.5f && degrees < -67.5f) return PmdDirection::BACK;
    if (degrees >= -157.5f && degrees < -112.5f) return PmdDirection::UP_LEFT;
    return PmdDirection::LEFT;
}

PokemonSprites::SpriteKind MainScene::pmdSpriteKind() const {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config) return PokemonSprites::SpriteKind::FRONT;

    if (pmdAction == PmdAction::SLEEPING) {
        uint16_t base = static_cast<uint16_t>(config->sleepingBase);
        return static_cast<PokemonSprites::SpriteKind>(base + (pmdFrame % config->sleepingFrames));
    }

    uint16_t directionIndex = pmdDirectionFrameIndex();
    if (pmdAction == PmdAction::IDLE) {
        uint16_t base = static_cast<uint16_t>(config->idleBase);
        return static_cast<PokemonSprites::SpriteKind>(base + directionIndex * config->idleFrames + (pmdFrame % config->idleFrames));
    }

    uint8_t walkingFrame = pmdAction == PmdAction::STOPPING
        ? pmdStoppingSpriteFrameIndex(config, pmdFrame)
        : pmdWalkingSpriteFrameIndex(config, pmdFrame, pmdLongMove);
    uint16_t base = static_cast<uint16_t>(config->walkingBase);
    return static_cast<PokemonSprites::SpriteKind>(base + directionIndex * config->walkingFrames + walkingFrame);
}

const PokemonSprites::SpriteFrame* MainScene::currentMonsterFrame() const {
    if (!active) return nullptr;
    if (pmdSpriteConfigForSpecies(active->id)) {
        return PokemonSprites::findSpeciesSprite(active->id, pmdSpriteKind());
    }
    return PokemonSprites::findSpeciesSprite(active->id, PokemonSprites::SpriteKind::FRONT);
}

const PokemonSprites::SpriteFrame* MainScene::movementBoundsFrame() const {
    if (!active) return nullptr;
    if (const PmdSpriteConfig* config = pmdSpriteConfigForSpecies(active->id)) {
        return PokemonSprites::findSpeciesSprite(active->id, config->idleBase);
    }
    return PokemonSprites::findSpeciesSprite(active->id, PokemonSprites::SpriteKind::FRONT);
}

uint16_t MainScene::pmdDirectionFrameIndex() const {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config || !config->mirrorRightDirections) return static_cast<uint16_t>(pmdDirection);

    switch (pmdDirection) {
    case PmdDirection::FRONT: return 0;
    case PmdDirection::DOWN_LEFT: return 1;
    case PmdDirection::LEFT: return 2;
    case PmdDirection::UP_LEFT: return 3;
    case PmdDirection::BACK: return 4;
    case PmdDirection::UP_RIGHT: return 3;
    case PmdDirection::RIGHT: return 2;
    case PmdDirection::DOWN_RIGHT: return 1;
    }
    return 0;
}

bool MainScene::pmdDirectionFlipX() const {
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    if (!config || !config->mirrorRightDirections) return false;
    return pmdDirection == PmdDirection::UP_RIGHT ||
           pmdDirection == PmdDirection::RIGHT ||
           pmdDirection == PmdDirection::DOWN_RIGHT;
}

void MainScene::chooseAiGoal(uint32_t nowMs) {
    const Game::MonsterRuntime& mon = GameEngine::ins().activeMonster();
    switch (mind.topDesire()) {
    case MonsterDesire::EAT:
        if (GameEngine::ins().bowlHasFood() && mon.satiety < MONSTER_FEED_TARGET_SATIETY) {
            if (monsterNearFood()) enterFeeding(nowMs);
            else setFoodTarget(nowMs);
            if (aiMode != AiMode::IDLE ||
                visitor.state != VisitorState::IDLE) {
                return;
            }
            // A teammate can be closer to the bowl while this actor blocks
            // its approach. Move away instead of retrying forever in place.
            if (startWander(nowMs)) return;
        }
        break;
    case MonsterDesire::REST:
        if (monsterNeedsBedRest()) {
            if (monsterNearBed()) enterResting(nowMs);
            else setBedTarget(nowMs);
            return;
        }
        if (startWander(nowMs)) return;
        break;
    case MonsterDesire::WANDER:
        if (startWander(nowMs)) return;
        break;
    case MonsterDesire::STARE:
        if (GameRandom::random(100) < 32) {
            int direction = (int)pmdDirection + (GameRandom::random(2) == 0 ? -1 : 1);
            if (direction < 0) direction += 8;
            if (direction >= 8) direction -= 8;
            nextAiDecisionMs = nowMs + GameRandom::random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
            beginTurn(AiMode::IDLE, (PmdDirection)direction, nowMs);
            mind.onActivity(nowMs);
            return;
        }
        break;
    }

    aiMode = AiMode::IDLE;
    targetX = monsterX;
    targetY = monsterY;
    nextAiDecisionMs = nowMs + GameRandom::random(behaviorProfile.idleMinMs, behaviorProfile.idleMaxMs + 1);
    if (mind.topDesire() != MonsterDesire::STARE) {
        mind.onActivity(nowMs);
    }
}

bool MainScene::startWander(uint32_t nowMs) {
    if (behaviorProfile.movementMode ==
        MonsterMovementMode::STATIONARY) {
        return false;
    }
    float walkOffsetY = walkBoundaryOffsetY();
    for (uint8_t tries = 0; tries < 18; ++tries) {
        int radiusX = behaviorProfile.wanderRadiusX;
        int radiusY = behaviorProfile.wanderRadiusY;
        float candidateX = clampf(monsterX + (float)GameRandom::random(-radiusX, radiusX + 1),
                                  (float)roomWalkMinX(),
                                  (float)roomWalkMaxX());
        float candidateY = clampf(monsterY + (float)GameRandom::random(-radiusY, radiusY + 1),
                                  (float)roomWalkMinY() - walkOffsetY,
                                  (float)roomWalkMaxY() - walkOffsetY);
        if (!monsterFootprintInsideWalkArea(candidateX, candidateY)) continue;
        if (fabsf(candidateX - monsterX) < 8.0f &&
            fabsf(candidateY - monsterY) < 4.0f) continue;
        // Wander targets must satisfy the same secondary-actor clearance as
        // the route planner, otherwise every attempted route can be rejected
        // after the target has already been selected.
        if (!mainTargetKeepsVisitorSpacing(candidateX, candidateY)) continue;
        targetX = candidateX;
        targetY = candidateY;
        beginMovement(AiMode::WANDER, nowMs);
        if (aiMode == AiMode::WANDER || aiMode == AiMode::TURNING) return true;
    }
    traceMainAi(nowMs, MainAiTraceGate::ROUTE_FAILED, true);
    return false;
}

bool MainScene::openPendingProgression() {
    if (progressionModal != ProgressionModal::NONE) return true;
    if (GameEngine::ins().hasPendingLevelUp()) {
        progressionModal = ProgressionModal::LEVEL_UP;
        return true;
    }
    if (GameEngine::ins().hasPendingEvolution()) {
        progressionModal = ProgressionModal::EVOLUTION;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveReplacement()) {
        progressionModal = ProgressionModal::MOVE_REPLACED;
        return true;
    }
    if (GameEngine::ins().hasPendingMoveLearn()) {
        ProgressionUi::resetMoveLearnState(progressionMoveLearn);
        progressionModal = ProgressionModal::LEARN_MOVE;
        return true;
    }
    return false;
}

void MainScene::schedulePairInteraction(uint32_t nowMs, bool immediate) {
    nextPairInteractionMs = immediate
        ? nowMs + 300
        : nowMs + static_cast<uint32_t>(GameRandom::random(
              PAIR_INTERACTION_MIN_INTERVAL_MS,
              PAIR_INTERACTION_MAX_INTERVAL_MS + 1));
}

bool MainScene::pairInteractionAllowed() const {
    if (!visitorHostActive() || !visitor.active ||
        visitor.dropOffsetY > 0.0f ||
        GameEngine::ins().idleModeActive() ||
        GameEngine::ins().debugTiltControlEnabled() ||
        GameEngine::ins().contactVisitFarewellPending() ||
        roomAction != RoomAction::NONE ||
        aiMode != AiMode::IDLE ||
        visitor.state != VisitorState::IDLE ||
        doorTransition != DoorTransitionMode::NONE ||
        contactDialog != ContactDialog::NONE ||
        contactGuestMotion != ContactGuestMotion::NONE ||
        progressionModal != ProgressionModal::NONE) {
        return false;
    }

    const Game::GameState& state = GameEngine::ins().gameState();
    if (state.teamCount < 2) return false;
    const Game::MonsterRuntime& mainMon = state.team[0];
    const Game::MonsterRuntime& guestMon = state.team[1];
    if (monsterIsSleepTime(mainMon) || monsterIsSleepTime(guestMon)) {
        return false;
    }
    auto ready = [](const Game::MonsterRuntime& mon) {
        if (mon.fainted || mon.hpCur == 0 ||
            mon.majorStatus == Game::MajorStatus::SLEEP ||
            mon.hpMax == 0 ||
            static_cast<uint32_t>(mon.hpCur) * 100UL <=
                static_cast<uint32_t>(mon.hpMax) * 20UL) {
            return false;
        }
        const Game::SpeciesCareProfile care =
            Game::speciesCareProfileFor(mon.speciesId);
        return !care.needsFood || mon.satiety > 25;
    };
    if (!ready(mainMon) || !ready(guestMon)) return false;

    const Game::SpeciesCareProfile mainCare =
        Game::speciesCareProfileFor(mainMon.speciesId);
    const Game::SpeciesCareProfile guestCare =
        Game::speciesCareProfileFor(guestMon.speciesId);
    return mainCare.canMove || guestCare.canMove;
}

bool MainScene::choosePairApproachPose(bool moverMain,
                                       float& x, float& y) const {
    float moverX = moverMain ? monsterX : visitor.x;
    float moverY = moverMain ? monsterY : visitor.y;
    float otherX = moverMain ? visitor.x : monsterX;
    float otherY = moverMain ? visitor.y : monsterY;
    const ActorGeometry moverGeometry =
        moverMain ? mainGeometry() : visitorGeometry();
    const ActorGeometry otherGeometry =
        moverMain ? visitorGeometry() : mainGeometry();
    float moverFootY = moverY + moverGeometry.groundOffsetY;
    float otherFootY = otherY + otherGeometry.groundOffsetY;
    float dx = moverX - otherX;
    float dy = moverFootY - otherFootY;
    float distance = sqrtf(dx * dx + dy * dy);
    float desired = doorActorMinSeparation() + 3.0f;
    if (distance >= desired && distance <= desired + 9.0f) {
        x = moverX;
        y = moverY;
        return true;
    }

    float baseAngle = distance > 0.01f ? atan2f(dy, dx) : 0.0f;
    static constexpr float ANGLE_OFFSETS[] = {
        0.0f, 0.785398f, -0.785398f, 1.570796f,
        -1.570796f, 2.356194f, -2.356194f, 3.141593f,
    };
    for (float offset : ANGLE_OFFSETS) {
        float angle = baseAngle + offset;
        float candidateX = otherX + cosf(angle) * desired;
        float candidateY = otherFootY + sinf(angle) * desired -
                           moverGeometry.groundOffsetY;
        if (!actorFootprintInsideWalkArea(
                candidateX, candidateY, moverGeometry)) continue;
        if (!actorPathSegmentInsideWalkArea(
                moverX, moverY, candidateX, candidateY,
                moverGeometry, false)) continue;
        if (!routeSegmentKeepsSpacing(
                moverX, moverFootY,
                candidateX, candidateY + moverGeometry.groundOffsetY,
                otherX, otherFootY, doorActorMinSeparation())) continue;
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool MainScene::choosePairLeaderGoal(bool leaderMain,
                                     float& x, float& y) const {
    float leaderX = leaderMain ? monsterX : visitor.x;
    float leaderY = leaderMain ? monsterY : visitor.y;
    float followerX = leaderMain ? visitor.x : monsterX;
    float followerY = leaderMain ? visitor.y : monsterY;
    const ActorGeometry leaderGeometry =
        leaderMain ? mainGeometry() : visitorGeometry();
    const ActorGeometry followerGeometry =
        leaderMain ? visitorGeometry() : mainGeometry();
    for (uint8_t attempt = 0; attempt < 24; ++attempt) {
        float candidateX = 0.0f;
        float candidateY = 0.0f;
        if (!randomActorCenterWalkPoint(
                leaderGeometry, candidateX, candidateY)) return false;
        float leaderDx = candidateX - leaderX;
        float candidateFootY = candidateY + leaderGeometry.groundOffsetY;
        float leaderDy = candidateFootY -
            (leaderY + leaderGeometry.groundOffsetY);
        float followerDx = candidateX - followerX;
        float followerDy = candidateFootY -
            (followerY + followerGeometry.groundOffsetY);
        float leaderDistanceSq =
            leaderDx * leaderDx + leaderDy * leaderDy;
        float followerDistanceSq =
            followerDx * followerDx + followerDy * followerDy;
        if (leaderDistanceSq < 34.0f * 34.0f ||
            leaderDistanceSq > 96.0f * 96.0f ||
            followerDistanceSq < 24.0f * 24.0f) {
            continue;
        }
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool MainScene::buildPairActorRoute(bool mainActor, float x, float y,
                                    bool avoidOther) {
    if (mainActor) {
        targetX = x;
        targetY = y;
        if (avoidOther) return buildMoveRoute(x, y);
        return buildMoveRouteFrom(
            monsterX, monsterY, x, y,
            moveRouteX, moveRouteY, moveRouteCount, moveRouteIndex,
            false, mainGeometry(), false,
            visitor.x, visitor.y, visitorGeometry().groundOffsetY);
    }
    visitor.targetX = x;
    visitor.targetY = y;
    visitor.state = VisitorState::WALK;
    visitor.frameStartedMs = Hal::ins().millis();
    visitor.frameIndex = 0;
    bool built = avoidOther
        ? buildVisitorMoveRoute(x, y)
        : buildMoveRouteFrom(
              visitor.x, visitor.y, x, y,
              visitorMoveRouteX, visitorMoveRouteY,
              visitorMoveRouteCount, visitorMoveRouteIndex,
              false, visitorGeometry(), false,
              monsterX, monsterY, mainGeometry().groundOffsetY);
    if (!built) visitor.state = VisitorState::IDLE;
    return built;
}

bool MainScene::beginPairChaseLeg(uint32_t nowMs) {
    for (uint8_t attempt = 0; attempt < 12; ++attempt) {
        float goalX = 0.0f;
        float goalY = 0.0f;
        if (!choosePairLeaderGoal(pairLeaderMain, goalX, goalY) ||
            !buildPairActorRoute(pairLeaderMain, goalX, goalY, true)) {
            continue;
        }
        STICKMON_TRACEF(
            "[PairAI] event=chase_leg t=%lu leader=%s legs=%u "
            "goal=%.1f,%.1f route=%u/%u\n",
            static_cast<unsigned long>(nowMs),
            pairLeaderMain ? "main" : "secondary",
            static_cast<unsigned>(pairLegsRemaining), goalX, goalY,
            static_cast<unsigned>(pairLeaderMain
                ? moveRouteIndex : visitorMoveRouteIndex),
            static_cast<unsigned>(pairLeaderMain
                ? moveRouteCount : visitorMoveRouteCount));
        return true;
    }
    STICKMON_TRACEF(
        "[PairAI] event=chase_route_failed t=%lu leader=%s legs=%u\n",
        static_cast<unsigned long>(nowMs),
        pairLeaderMain ? "main" : "secondary",
        static_cast<unsigned>(pairLegsRemaining));
    return false;
}

bool MainScene::updatePairActorRoute(bool mainActor, float speed,
                                     float dtSeconds, uint32_t nowMs,
                                     bool keepSpacing) {
    speed *= mainActor
        ? behaviorProfile.moveSpeedScale
        : visitorBehaviorProfile().moveSpeedScale;
    float waypointX = mainActor ? targetX : visitor.targetX;
    float waypointY = mainActor ? targetY : visitor.targetY;
    bool hasWaypoint = mainActor
        ? currentWaypoint(waypointX, waypointY)
        : currentVisitorWaypoint(waypointX, waypointY);
    if (!hasWaypoint) {
        if (mainActor) {
            velocityX = velocityY = 0.0f;
        } else {
            visitor.state = VisitorState::IDLE;
        }
        return true;
    }

    if (keepSpacing) {
        float actorX = mainActor ? monsterX : visitor.x;
        float actorY = mainActor ? monsterY : visitor.y;
        float otherX = mainActor ? visitor.x : monsterX;
        float otherY = mainActor ? visitor.y : monsterY;
        actorY += mainActor
            ? mainGeometry().groundOffsetY
            : visitorGeometry().groundOffsetY;
        otherY += mainActor
            ? visitorGeometry().groundOffsetY
            : mainGeometry().groundOffsetY;
        float waypointFootY = waypointY + (mainActor
            ? mainGeometry().groundOffsetY
            : visitorGeometry().groundOffsetY);
        float actorDx = actorX - otherX;
        float actorDy = actorY - otherY;
        float waypointDx = waypointX - otherX;
        float waypointDy = waypointFootY - otherY;
        float minSeparation = doorActorMinSeparation();
        float stopDistance = minSeparation + 1.5f;
        if (actorDx * actorDx + actorDy * actorDy <=
                stopDistance * stopDistance &&
            waypointDx * waypointDx + waypointDy * waypointDy <
                minSeparation * minSeparation) {
            if (mainActor) {
                clearMoveRoute();
                velocityX = velocityY = 0.0f;
            } else {
                clearVisitorMoveRoute();
                visitor.state = VisitorState::IDLE;
                visitor.frameStartedMs = nowMs;
                visitor.frameIndex = 0;
            }
            return true;
        }
    }

    doorLastUpdateMs = nowMs;
    bool reached = mainActor
        ? moveDoorToward(waypointX, waypointY, speed, dtSeconds,
                         true, keepSpacing)
        : moveVisitorDoorToward(waypointX, waypointY, speed, dtSeconds,
                                true, keepSpacing);
    if (!reached) return false;

    if (mainActor) {
        ++moveRouteIndex;
        if (moveRouteIndex < moveRouteCount) return false;
        clearMoveRoute();
        velocityX = velocityY = 0.0f;
    } else {
        ++visitorMoveRouteIndex;
        if (visitorMoveRouteIndex < visitorMoveRouteCount) return false;
        clearVisitorMoveRoute();
        visitor.state = VisitorState::IDLE;
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
    }
    return true;
}

void MainScene::facePairActors() {
    float dx = visitor.x - monsterX;
    float dy = visitor.y + visitorGeometry().groundOffsetY -
               (monsterY + mainGeometry().groundOffsetY);
    pmdDirection = pmdDirectionForVelocity(dx, dy);
    if (fabsf(dx) > 0.5f) facingRight = dx > 0.0f;
    visitor.direction = visitorWalkDirectionForDelta(-dx, -dy);
    if (fabsf(dx) > 0.5f) visitor.facingRight = dx < 0.0f;
    velocityX = velocityY = 0.0f;
    visitor.state = VisitorState::IDLE;
}

float MainScene::pairConversationHopOffset(
    bool mainActor, uint32_t nowMs) const {
    if (pairInteraction != PairInteraction::TALK ||
        pairInteractionPhase != PairInteractionPhase::CONVERSATION) {
        return 0.0f;
    }
    uint32_t elapsed = nowMs - pairPhaseStartedMs;
    uint32_t hopStart = mainActor
        ? 0UL : PAIR_TALK_HOP_MS + PAIR_TALK_GAP_MS;
    if (elapsed < hopStart || elapsed >= hopStart + PAIR_TALK_HOP_MS) {
        return 0.0f;
    }
    float progress = static_cast<float>(elapsed - hopStart) /
                     static_cast<float>(PAIR_TALK_HOP_MS);
    return sinf(progress * PI_F) * 5.0f;
}

bool MainScene::startPairInteraction(uint32_t nowMs) {
    if (!pairInteractionAllowed()) return false;

    const Game::GameState& state = GameEngine::ins().gameState();
    bool mainCanMove =
        Game::speciesCareProfileFor(state.team[0].speciesId).canMove;
    bool guestCanMove =
        Game::speciesCareProfileFor(state.team[1].speciesId).canMove;
    bool forcedPlay = pairForcedPlay;
    bool forcedChase = pairForcedChase;
    uint8_t roll = static_cast<uint8_t>(GameRandom::random(100));
    if (!mainCanMove || !guestCanMove) {
        pairInteraction = PairInteraction::TALK;
    } else if (forcedChase) {
        pairInteraction = PairInteraction::CHASE;
    } else if (forcedPlay) {
        pairInteraction =
            roll < 75 ? PairInteraction::CHASE : PairInteraction::TALK;
    } else {
        pairInteraction = roll < 45
            ? PairInteraction::TALK : PairInteraction::CHASE;
    }
    STICKMON_TRACEF(
        "[PairAI] event=selected t=%lu type=%s forcedPlay=%u "
        "forcedChase=%u roll=%u "
        "mainCanMove=%u secondaryCanMove=%u\n",
        static_cast<unsigned long>(nowMs),
        pairInteraction == PairInteraction::CHASE ? "chase" : "talk",
        forcedPlay ? 1U : 0U, forcedChase ? 1U : 0U,
        static_cast<unsigned>(roll),
        mainCanMove ? 1U : 0U, guestCanMove ? 1U : 0U);
    pairLeaderMain = GameRandom::random(2) == 0;
    if (!mainCanMove) pairLeaderMain = false;
    if (!guestCanMove) pairLeaderMain = true;

    cancelRoomAction(nowMs);
    clearMoveRoute();
    clearVisitorMoveRoute();
    velocityX = velocityY = 0.0f;
    aiMode = AiMode::IDLE;
    visitor.state = VisitorState::IDLE;
    targetX = monsterX;
    targetY = monsterY;
    visitor.targetX = visitor.x;
    visitor.targetY = visitor.y;

    float approachX = 0.0f;
    float approachY = 0.0f;
    if (!choosePairApproachPose(
            pairLeaderMain, approachX, approachY)) {
        STICKMON_TRACEF(
            "[PairAI] event=approach_pose_failed t=%lu type=%u\n",
            static_cast<unsigned long>(nowMs),
            static_cast<unsigned>(pairInteraction));
        pairInteraction = PairInteraction::NONE;
        schedulePairInteraction(nowMs);
        return false;
    }

    pairInteractionPhase = PairInteractionPhase::APPROACH;
    pairPhaseStartedMs = nowMs;
    pairPhaseUntilMs = nowMs + 4000;
    pairInteractionUntilMs = nowMs +
        (pairInteraction == PairInteraction::CHASE
             ? PAIR_CHASE_TIMEOUT_MS : PAIR_TALK_TIMEOUT_MS);
    bool alreadyThere = pairLeaderMain
        ? fabsf(approachX - monsterX) < 1.5f &&
              fabsf(approachY - monsterY) < 1.5f
        : fabsf(approachX - visitor.x) < 1.5f &&
              fabsf(approachY - visitor.y) < 1.5f;
    if (!alreadyThere &&
        !buildPairActorRoute(pairLeaderMain, approachX, approachY)) {
        STICKMON_TRACEF(
            "[PairAI] event=approach_route_failed t=%lu type=%u "
            "goal=%.1f,%.1f\n",
            static_cast<unsigned long>(nowMs),
            static_cast<unsigned>(pairInteraction), approachX, approachY);
        pairInteraction = PairInteraction::NONE;
        pairInteractionPhase = PairInteractionPhase::NONE;
        schedulePairInteraction(nowMs);
        return false;
    }
    if (alreadyThere) {
        facePairActors();
        pairInteractionPhase = PairInteractionPhase::INVITE;
        pairPhaseStartedMs = nowMs;
        pairPhaseUntilMs = nowMs + PAIR_INVITE_MS;
    }
    pairForcedPlay = false;
    pairForcedChase = false;
    return true;
}

void MainScene::beginPairActive(uint32_t nowMs) {
    if (pairInteraction == PairInteraction::TALK) {
        clearMoveRoute();
        clearVisitorMoveRoute();
        facePairActors();
        pairInteractionPhase = PairInteractionPhase::CONVERSATION;
        pairPhaseStartedMs = nowMs;
        pairPhaseUntilMs = nowMs + PAIR_TALK_TOTAL_MS;
        STICKMON_TRACEF(
            "[PairAI] event=talk_begin t=%lu mainFirst=1\n",
            static_cast<unsigned long>(nowMs));
        return;
    }

    pairInteractionPhase = PairInteractionPhase::ACTIVE;
    pairPhaseStartedMs = nowMs;
    pairLegsRemaining = 4;
    pairTrailX = pairLeaderMain ? monsterX : visitor.x;
    pairTrailY = pairLeaderMain ? monsterY : visitor.y;
    pairFollowerDelayUntilMs = nowMs + PAIR_FOLLOW_DELAY_MS;
    pairNextRouteAttemptMs = nowMs;
    if (!beginPairChaseLeg(nowMs)) {
        pairNextRouteAttemptMs = nowMs + 250;
    }
}

void MainScene::beginPairSettle(uint32_t nowMs) {
    clearMoveRoute();
    clearVisitorMoveRoute();
    velocityX = velocityY = 0.0f;
    bool followerMain = !pairLeaderMain;
    float settleX = 0.0f;
    float settleY = 0.0f;
    pairInteractionPhase = PairInteractionPhase::SETTLE;
    pairPhaseStartedMs = nowMs;
    pairPhaseUntilMs = nowMs + 3000;
    if (!choosePairApproachPose(followerMain, settleX, settleY) ||
        !buildPairActorRoute(followerMain, settleX, settleY)) {
        beginPairCelebrate(nowMs);
    }
}

void MainScene::beginPairCelebrate(uint32_t nowMs) {
    clearMoveRoute();
    clearVisitorMoveRoute();
    targetX = monsterX;
    targetY = monsterY;
    visitor.targetX = visitor.x;
    visitor.targetY = visitor.y;
    facePairActors();
    showHearts(HeartEffect::TWO, nowMs, PAIR_CELEBRATE_MS);
    pairInteractionPhase = PairInteractionPhase::CELEBRATE;
    pairPhaseStartedMs = nowMs;
    pairPhaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
}

void MainScene::finishPairInteraction(uint32_t nowMs, bool reward) {
    STICKMON_TRACEF(
        "[PairAI] event=finish t=%lu type=%u phase=%u reward=%u "
        "legsRemaining=%u\n",
        static_cast<unsigned long>(nowMs),
        static_cast<unsigned>(pairInteraction),
        static_cast<unsigned>(pairInteractionPhase), reward ? 1U : 0U,
        static_cast<unsigned>(pairLegsRemaining));
    clearMoveRoute();
    clearVisitorMoveRoute();
    velocityX = velocityY = 0.0f;
    targetX = monsterX;
    targetY = monsterY;
    visitor.targetX = visitor.x;
    visitor.targetY = visitor.y;
    visitor.state = VisitorState::IDLE;
    visitor.stateUntilMs =
        nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    aiMode = AiMode::IDLE;
    nextAiDecisionMs = nowMs + static_cast<uint32_t>(GameRandom::random(700, 1401));
    mind.onActivity(nowMs);
    if (reward) GameEngine::ins().rewardPairInteractionMood();
    pairInteraction = PairInteraction::NONE;
    pairInteractionPhase = PairInteractionPhase::NONE;
    pairLegsRemaining = 0;
    pairNextRouteAttemptMs = 0;
    schedulePairInteraction(nowMs);
}

void MainScene::cancelPairInteraction(uint32_t nowMs) {
    if (pairInteraction == PairInteraction::NONE) return;
    finishPairInteraction(nowMs, false);
    heartEffect = HeartEffect::NONE;
    heartEffectUntilMs = 0;
}

bool MainScene::updatePairInteraction(uint32_t nowMs,
                                      float dtSeconds) {
    if (pairInteraction == PairInteraction::NONE) {
        if ((int32_t)(nowMs - nextPairInteractionMs) < 0) return false;
        if (!startPairInteraction(nowMs)) {
            nextPairInteractionMs = nowMs + PAIR_INTERACTION_RETRY_MS;
            return false;
        }
    }

    const Game::GameState& state = GameEngine::ins().gameState();
    bool interrupted =
        state.teamCount < 2 || !visitor.active ||
        GameEngine::ins().idleModeActive() ||
        GameEngine::ins().contactVisitFarewellPending() ||
        doorTransition != DoorTransitionMode::NONE ||
        contactDialog != ContactDialog::NONE ||
        contactGuestMotion != ContactGuestMotion::NONE ||
        progressionModal != ProgressionModal::NONE ||
        state.team[0].fainted || state.team[0].hpCur == 0 ||
        state.team[1].fainted || state.team[1].hpCur == 0 ||
        state.team[0].majorStatus == Game::MajorStatus::SLEEP ||
        state.team[1].majorStatus == Game::MajorStatus::SLEEP;
    if (interrupted) {
        cancelPairInteraction(nowMs);
        return false;
    }
    if ((int32_t)(nowMs - pairInteractionUntilMs) >= 0 &&
        pairInteractionPhase != PairInteractionPhase::SETTLE &&
        pairInteractionPhase != PairInteractionPhase::CELEBRATE) {
        beginPairSettle(nowMs);
    }

    switch (pairInteractionPhase) {
    case PairInteractionPhase::APPROACH: {
        bool arrived = updatePairActorRoute(
            pairLeaderMain, PAIR_APPROACH_SPEED,
            dtSeconds, nowMs, true);
        if (arrived ||
            (int32_t)(nowMs - pairPhaseUntilMs) >= 0) {
            clearMoveRoute();
            clearVisitorMoveRoute();
            facePairActors();
            pairInteractionPhase = PairInteractionPhase::INVITE;
            pairPhaseStartedMs = nowMs;
            pairPhaseUntilMs = nowMs + PAIR_INVITE_MS;
        }
        return true;
    }
    case PairInteractionPhase::INVITE:
        facePairActors();
        if ((int32_t)(nowMs - pairPhaseUntilMs) >= 0) {
            beginPairActive(nowMs);
        }
        return true;
    case PairInteractionPhase::CONVERSATION:
        facePairActors();
        if ((int32_t)(nowMs - pairPhaseUntilMs) >= 0) {
            beginPairCelebrate(nowMs);
        }
        return true;
    case PairInteractionPhase::ACTIVE: {
        bool leaderHasRoute = pairLeaderMain
            ? moveRouteIndex < moveRouteCount
            : visitorMoveRouteIndex < visitorMoveRouteCount;
        if (!leaderHasRoute) {
            if ((int32_t)(nowMs - pairNextRouteAttemptMs) < 0) {
                return true;
            }
            if (!beginPairChaseLeg(nowMs)) {
                pairNextRouteAttemptMs = nowMs + 250;
                return true;
            }
        }

        bool followerMain = !pairLeaderMain;
        float dx = visitor.x - monsterX;
        float dy = visitor.y - monsterY;
        float separation = sqrtf(dx * dx + dy * dy);
        float leaderSpeed = PAIR_CHASE_LEADER_SPEED;
        float followerSpeed = PAIR_CHASE_FOLLOWER_SPEED;
        if (separation > 54.0f) {
            leaderSpeed *= 0.84f;
            followerSpeed *= 1.24f;
        }

        bool leaderArrived = updatePairActorRoute(
            pairLeaderMain, leaderSpeed, dtSeconds, nowMs, true);
        dx = visitor.x - monsterX;
        dy = visitor.y - monsterY;
        separation = sqrtf(dx * dx + dy * dy);
        float leaderX = pairLeaderMain ? monsterX : visitor.x;
        float leaderY = pairLeaderMain ? monsterY : visitor.y;
        float trailDx = leaderX - pairTrailX;
        float trailDy = leaderY - pairTrailY;
        if (trailDx * trailDx + trailDy * trailDy >= 36.0f) {
            pairTrailX = leaderX;
            pairTrailY = leaderY;
        }
        if ((int32_t)(nowMs - pairFollowerDelayUntilMs) >= 0) {
            bool followerHasRoute = followerMain
                ? moveRouteIndex < moveRouteCount
                : visitorMoveRouteIndex < visitorMoveRouteCount;
            if (!followerHasRoute &&
                separation > doorActorMinSeparation() + 2.0f) {
                buildPairActorRoute(
                    followerMain, pairTrailX, pairTrailY, false);
            }
            updatePairActorRoute(
                followerMain, followerSpeed, dtSeconds, nowMs, true);
        }

        if (!leaderArrived) return true;
        if (pairLegsRemaining > 0) --pairLegsRemaining;
        if (pairLegsRemaining == 0) {
            beginPairSettle(nowMs);
            return true;
        }

        pairTrailX = pairLeaderMain ? monsterX : visitor.x;
        pairTrailY = pairLeaderMain ? monsterY : visitor.y;
        if (!beginPairChaseLeg(nowMs)) {
            pairNextRouteAttemptMs = nowMs + 250;
        }
        return true;
    }
    case PairInteractionPhase::SETTLE: {
        bool arrived = updatePairActorRoute(
            !pairLeaderMain, PAIR_SETTLE_SPEED,
            dtSeconds, nowMs, true);
        if (arrived ||
            (int32_t)(nowMs - pairPhaseUntilMs) >= 0) {
            beginPairCelebrate(nowMs);
        }
        return true;
    }
    case PairInteractionPhase::CELEBRATE:
        facePairActors();
        if ((int32_t)(nowMs - pairPhaseUntilMs) >= 0) {
            finishPairInteraction(nowMs, true);
        }
        return true;
    case PairInteractionPhase::NONE:
        break;
    }
    return pairInteraction != PairInteraction::NONE;
}

void MainScene::updateContactVisit(uint32_t nowMs, float dtSeconds) {
    GameEngine& engine = GameEngine::ins();

    if (contactGuestMotion != ContactGuestMotion::NONE &&
        nowMs - contactGuestMotionStartedMs >=
            CONTACT_GUEST_MOTION_TIMEOUT_MS) {
        if (contactGuestMotion == ContactGuestMotion::TEAM_ENTER_CROSS) {
            visitor.x = doorInsideX;
            visitor.y = visitorDoorInsideY;
            visitor.targetX = visitor.x;
            visitor.targetY = visitor.y;
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs =
                nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
            contactGuestMotion = ContactGuestMotion::NONE;
            return;
        }
        clearVisitorMoveRoute();
        visitor.active = false;
        contactGuestMotion = ContactGuestMotion::NONE;
        engine.completeContactVisit();
        return;
    }

    switch (contactGuestMotion) {
    case ContactGuestMotion::TEAM_ENTER_CROSS:
        doorLastUpdateMs = nowMs;
        if (!moveVisitorDoorToward(
                doorInsideX, visitorDoorInsideY, DOOR_CROSS_SPEED,
                dtSeconds, false)) {
            return;
        }
        visitor.x = doorInsideX;
        visitor.y = visitorDoorInsideY;
        visitor.targetX = visitor.x;
        visitor.targetY = visitor.y;
        visitor.state = VisitorState::IDLE;
        visitor.stateUntilMs =
            nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        contactGuestMotion = ContactGuestMotion::NONE;
        return;
    case ContactGuestMotion::ENTER_CROSS:
        doorLastUpdateMs = nowMs;
        if (!moveVisitorDoorToward(
                doorInsideX, visitorDoorInsideY, DOOR_CROSS_SPEED,
                dtSeconds, false)) {
            return;
        }
        visitor.x = doorInsideX;
        visitor.y = visitorDoorInsideY;
        visitor.targetX = visitor.x;
        visitor.targetY = visitor.y;
        visitor.state = VisitorState::IDLE;
        visitor.stateUntilMs =
            nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        contactGuestMotion = ContactGuestMotion::NONE;
        switch (engine.contactVisitKind()) {
        case ContactVisitKind::PLAY:
            contactDialog = ContactDialog::PLAY_ARRIVAL;
            break;
        case ContactVisitKind::GIFT:
            contactDialog = ContactDialog::GIFT_ARRIVAL;
            break;
        case ContactVisitKind::EXPLORE:
            contactDialog = ContactDialog::EXPLORE_INVITE;
            contactDialogYes = true;
            break;
        default:
            engine.requestContactVisitFarewell();
            break;
        }
        return;
    case ContactGuestMotion::EXIT_ROUTE:
        doorLastUpdateMs = nowMs;
        if (!updateVisitorDoorRoute(dtSeconds)) return;
        visitor.targetX = doorOutsideX;
        visitor.targetY = visitorDoorOutsideY;
        visitor.state = VisitorState::WALK;
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        contactGuestMotion = ContactGuestMotion::EXIT_CROSS;
        contactGuestMotionStartedMs = nowMs;
        return;
    case ContactGuestMotion::EXIT_CROSS:
        doorLastUpdateMs = nowMs;
        if (!moveVisitorDoorToward(
                doorOutsideX, visitorDoorOutsideY, DOOR_CROSS_SPEED,
                dtSeconds, false)) {
            return;
        }
        visitor.active = false;
        contactGuestMotion = ContactGuestMotion::NONE;
        engine.completeContactVisit();
        return;
    case ContactGuestMotion::NONE:
        break;
    }

    if (contactDialog != ContactDialog::NONE ||
        !engine.localContactVisitActive()) {
        return;
    }
    if (engine.contactVisitFarewellPending()) {
        contactDialog = engine.contactVisitExploring()
            ? ContactDialog::HAPPY_RETURN
            : ContactDialog::HAPPY_VISIT;
    } else if (engine.contactVisitTimedOut(nowMs)) {
        engine.requestContactVisitFarewell();
        contactDialog = ContactDialog::HAPPY_VISIT;
    }
}

void MainScene::beginContactGuestEntry(uint32_t nowMs) {
    spawnVisitor(nowMs, false);
    if (!prepareDoorAnchors()) {
        switch (GameEngine::ins().contactVisitKind()) {
        case ContactVisitKind::PLAY:
            contactDialog = ContactDialog::PLAY_ARRIVAL;
            break;
        case ContactVisitKind::GIFT:
            contactDialog = ContactDialog::GIFT_ARRIVAL;
            break;
        case ContactVisitKind::EXPLORE:
            contactDialog = ContactDialog::EXPLORE_INVITE;
            contactDialogYes = true;
            break;
        default:
            GameEngine::ins().requestContactVisitFarewell();
            break;
        }
        return;
    }

    visitor.x = doorOutsideX;
    visitor.y = visitorDoorOutsideY;
    visitor.targetX = doorInsideX;
    visitor.targetY = visitorDoorInsideY;
    visitor.state = VisitorState::WALK;
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    visitorDoorRouteEnteringWalkArea = true;
    contactGuestMotion = ContactGuestMotion::ENTER_CROSS;
    contactGuestMotionStartedMs = nowMs;
}

void MainScene::beginTeamMemberEntry(uint32_t nowMs) {
    if (!visitorCanUseDoor() || !prepareDoorAnchors()) return;

    visitor.x = doorOutsideX;
    visitor.y = visitorDoorOutsideY;
    visitor.targetX = doorInsideX;
    visitor.targetY = visitorDoorInsideY;
    visitor.state = VisitorState::WALK;
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    visitor.direction = visitorWalkDirectionForDelta(
        doorInsideX - doorOutsideX,
        visitorDoorInsideY - visitorDoorOutsideY);
    doorLastUpdateMs = nowMs;
    contactGuestMotion = ContactGuestMotion::TEAM_ENTER_CROSS;
    contactGuestMotionStartedMs = nowMs;
}

void MainScene::beginContactGuestExit(uint32_t nowMs) {
    clearVisitorMoveRoute();
    if (!visitor.active || !prepareDoorAnchors()) {
        visitor.active = false;
        contactGuestMotion = ContactGuestMotion::NONE;
        GameEngine::ins().completeContactVisit();
        return;
    }

    visitor.targetX = doorInsideX;
    visitor.targetY = visitorDoorInsideY;
    visitor.state = VisitorState::WALK;
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    if (!buildVisitorMoveRoute(visitor.targetX, visitor.targetY)) {
        visitor.x = doorInsideX;
        visitor.y = visitorDoorInsideY;
        visitor.targetX = doorOutsideX;
        visitor.targetY = visitorDoorOutsideY;
        contactGuestMotion = ContactGuestMotion::EXIT_CROSS;
    } else {
        visitorDoorRouteEnteringWalkArea =
            !actorFootprintInsideWalkArea(
                visitor.x, visitor.y, visitorGeometry());
        contactGuestMotion = ContactGuestMotion::EXIT_ROUTE;
    }
    contactGuestMotionStartedMs = nowMs;
}

bool MainScene::handleContactDialogButton(const ButtonEvent& event) {
    if (contactGuestMotion != ContactGuestMotion::NONE) return true;
    if (contactDialog == ContactDialog::NONE) return false;
    if (event.action != BtnAction::PRESSED) return true;

    bool choiceDialog =
        contactDialog == ContactDialog::KNOCK ||
        contactDialog == ContactDialog::EXPLORE_INVITE;
    if (choiceDialog && event.btn == 1) {
        contactDialogYes = !contactDialogYes;
        return true;
    }
    if (event.btn != 0) return true;

    GameEngine& engine = GameEngine::ins();
    uint32_t nowMs = Hal::ins().millis();
    switch (contactDialog) {
    case ContactDialog::KNOCK:
        contactDialog = ContactDialog::NONE;
        if (contactDialogYes && engine.acceptContactKnock()) {
            beginContactGuestEntry(nowMs);
        } else {
            engine.declineContactKnock();
        }
        contactDialogYes = true;
        return true;
    case ContactDialog::PLAY_ARRIVAL:
        contactDialog = ContactDialog::NONE;
        pairForcedPlay = true;
        schedulePairInteraction(nowMs, true);
        return true;
    case ContactDialog::GIFT_ARRIVAL:
        contactDialog = ContactDialog::NONE;
        return true;
    case ContactDialog::EXPLORE_INVITE:
        if (contactDialogYes) {
            contactDialog = ContactDialog::NONE;
            engine.acceptContactExploreInvitation();
        } else {
            engine.requestContactVisitFarewell();
            contactDialog = ContactDialog::HAPPY_VISIT;
        }
        contactDialogYes = true;
        return true;
    case ContactDialog::HAPPY_RETURN:
        contactDialog = ContactDialog::BYE_RETURN;
        return true;
    case ContactDialog::BYE_RETURN:
        contactDialog = ContactDialog::NONE;
        beginContactGuestExit(nowMs);
        return true;
    case ContactDialog::HAPPY_VISIT:
        contactDialog = ContactDialog::BYE_VISIT;
        return true;
    case ContactDialog::BYE_VISIT:
        contactDialog = ContactDialog::NONE;
        beginContactGuestExit(nowMs);
        return true;
    case ContactDialog::NONE:
        return false;
    }
    return true;
}

void MainScene::drawContactDialog() {
    if (contactDialog == ContactDialog::NONE) return;

    char line[64] = {};
    const char* text = nullptr;
    const Species* guest =
        findSpecies(GameEngine::ins().contactVisitSpeciesId());
    const char* guestName = guest ? guest->name : "";
    switch (contactDialog) {
    case ContactDialog::KNOCK:
        text = Ui::ContactVisit::KNOCK;
        break;
    case ContactDialog::PLAY_ARRIVAL:
        snprintf(line, sizeof(line), Ui::ContactVisit::PLAY_FMT, guestName);
        text = line;
        break;
    case ContactDialog::GIFT_ARRIVAL:
        snprintf(line, sizeof(line), Ui::ContactVisit::GIFT_FMT, guestName);
        text = line;
        break;
    case ContactDialog::EXPLORE_INVITE:
        snprintf(line, sizeof(line), Ui::ContactVisit::EXPLORE_FMT, guestName);
        text = line;
        break;
    case ContactDialog::HAPPY_RETURN:
        text = Ui::ContactVisit::HAPPY_RETURN;
        break;
    case ContactDialog::BYE_RETURN:
        text = Ui::ContactVisit::BYE_RETURN;
        break;
    case ContactDialog::HAPPY_VISIT:
        text = Ui::ContactVisit::HAPPY_VISIT;
        break;
    case ContactDialog::BYE_VISIT:
        text = Ui::ContactVisit::BYE_VISIT;
        break;
    case ContactDialog::NONE:
        return;
    }

    auto& c = PixelRenderer::canvas();
    static constexpr int BOX_X = 5;
    static constexpr int BOX_Y = 86;
    static constexpr int BOX_W = 230;
    static constexpr int BOX_H = 45;
    c.fillRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 4,
                    PixelRenderer::rgb(20, 24, 31));
    c.drawRoundRect(BOX_X, BOX_Y, BOX_W, BOX_H, 4,
                    PixelRenderer::rgb(236, 239, 230));
    int textX = BOX_X + MathUtil::max(6, (BOX_W - mainTextPixelWidth(text)) / 2);
    PixelRenderer::text(textX, BOX_Y + 7, text,
                        PixelRenderer::rgb(241, 242, 232), 1);

    bool choiceDialog =
        contactDialog == ContactDialog::KNOCK ||
        contactDialog == ContactDialog::EXPLORE_INVITE;
    if (choiceDialog) {
        uint16_t selected = PixelRenderer::rgb(255, 216, 72);
        uint16_t normal = PixelRenderer::rgb(148, 156, 169);
        PixelRenderer::text(BOX_X + 71, BOX_Y + 27,
                            Ui::ContactVisit::YES,
                            contactDialogYes ? selected : normal, 1);
        PixelRenderer::text(BOX_X + 149, BOX_Y + 27,
                            Ui::ContactVisit::NO,
                            contactDialogYes ? normal : selected, 1);
    }
}

bool MainScene::visitorHostActive() const {
    const GameEngine& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    if (state.teamCount < 2) return false;
    if (state.team[1].origin != Game::Origin::VISITOR) return true;
    return (engine.visitActive() && engine.visitAsHost()) ||
           engine.localContactVisitActive();
}

bool MainScene::visitorCanUseDoor() const {
    if (!visitor.active || !visitorHostActive()) return false;
    const Game::GameState& state = GameEngine::ins().gameState();
    if (state.teamCount < 2) return false;
    const Game::MonsterRuntime& mon = state.team[1];
    return !mon.fainted && mon.hpCur > 0;
}

bool MainScene::teamMemberCanEatFromBowl(uint8_t teamSlot) const {
    const GameEngine& engine = GameEngine::ins();
    const Game::GameState& state = engine.gameState();
    if (!engine.bowlHasFood() || teamSlot >= state.teamCount ||
        teamSlot >= Game::TEAM_CAP) {
        return false;
    }
    const Game::MonsterRuntime& mon = state.team[teamSlot];
    const Game::SpeciesCareProfile care =
        Game::speciesCareProfileFor(mon.speciesId);
    return mon.origin != Game::Origin::VISITOR &&
           care.needsFood && care.canMove &&
           !mon.fainted && mon.hpCur > 0 &&
           mon.majorStatus != Game::MajorStatus::SLEEP &&
           mon.satiety < MONSTER_FEED_TARGET_SATIETY;
}

bool MainScene::visitorCanSeekFood() const {
    if (!visitor.active || visitor.dropOffsetY > 0.0f ||
        !teamMemberCanEatFromBowl(1)) {
        return false;
    }
    return visitor.state == VisitorState::IDLE ||
           visitor.state == VisitorState::SEEK_FOOD ||
           visitor.state == VisitorState::FEEDING;
}

int8_t MainScene::preferredBowlEater() const {
    bool mainFoodAction =
        aiMode == AiMode::SEEK_FOOD || aiMode == AiMode::FEEDING ||
        (aiMode == AiMode::TURNING &&
         (turnNextMode == AiMode::SEEK_FOOD ||
          turnNextMode == AiMode::FEEDING));
    bool mainReadyToEat =
        mainFoodAction ||
        ((aiMode == AiMode::IDLE || aiMode == AiMode::RESTING ||
          aiMode == AiMode::WAKING || aiMode == AiMode::LEAVING_BED) &&
         mind.topDesire() == MonsterDesire::EAT);
    bool mainEligible = mainReadyToEat && teamMemberCanEatFromBowl(0);
    bool visitorEligible = visitorCanSeekFood();
    if (!mainEligible) return visitorEligible ? 1 : -1;
    if (!visitorEligible) return 0;

    float targetX = foodFeedX();
    float mainTargetY = foodFeedY() - mainGeometry().groundOffsetY;
    float visitorTargetY = foodFeedY() - visitorGeometry().groundOffsetY;
    float mainDx = monsterX - targetX;
    float mainDy = monsterY - mainTargetY;
    float visitorDx = visitor.x - targetX;
    float visitorDy = visitor.y - visitorTargetY;
    float mainDistanceSq = mainDx * mainDx + mainDy * mainDy;
    float visitorDistanceSq = visitorDx * visitorDx + visitorDy * visitorDy;
    return visitorDistanceSq < mainDistanceSq ? 1 : 0;
}

bool MainScene::claimBowl(uint8_t teamSlot) {
    if (bowlEaterSlot >= 0) return bowlEaterSlot == (int8_t)teamSlot;
    if (preferredBowlEater() != (int8_t)teamSlot) return false;
    bowlEaterSlot = (int8_t)teamSlot;
    return true;
}

void MainScene::releaseBowl(uint8_t teamSlot) {
    if (bowlEaterSlot == (int8_t)teamSlot) bowlEaterSlot = -1;
}

bool MainScene::startMainFoodYield(uint32_t nowMs) {
    if (!visitor.active || doorVisitorHidden || doorMainHidden ||
        visitor.state != VisitorState::IDLE ||
        pairInteraction != PairInteraction::NONE ||
        contactDialog != ContactDialog::NONE ||
        contactGuestMotion != ContactGuestMotion::NONE) {
        return false;
    }

    const ActorGeometry geometry = mainGeometry();
    float foodX = foodFeedX();
    float foodY = foodFeedY();
    float clearance = doorActorMinSeparation() + 6.0f;
    float clearanceSq = clearance * clearance;
    for (uint8_t tries = 0; tries < 48; ++tries) {
        float candidateX = 0.0f;
        float candidateY = 0.0f;
        if (!randomActorCenterWalkPoint(
                geometry, candidateX, candidateY)) {
            break;
        }
        float foodDx = candidateX - foodX;
        float foodDy = candidateY + geometry.groundOffsetY - foodY;
        if (foodDx * foodDx + foodDy * foodDy < clearanceSq ||
            !mainTargetKeepsVisitorSpacing(candidateX, candidateY) ||
            !buildMoveRoute(candidateX, candidateY)) {
            continue;
        }

        targetX = candidateX;
        targetY = candidateY;
        beginPreparedMovement(AiMode::WANDER, nowMs);
        mainYieldingForVisitorFood = true;
        STICKMON_TRACEF(
            "[MainAI] event=main_food_yield t=%lu from=%.1f,%.1f "
            "to=%.1f,%.1f\n",
            static_cast<unsigned long>(nowMs), monsterX, monsterY,
            targetX, targetY);
        return true;
    }
    return false;
}

bool MainScene::startVisitorFoodSeek(uint32_t nowMs) {
    if (mainYieldingForVisitorFood) {
        bool yieldInProgress = aiMode == AiMode::WANDER ||
            (aiMode == AiMode::TURNING && turnNextMode == AiMode::WANDER);
        if (yieldInProgress) return false;
        mainYieldingForVisitorFood = false;
        clearVisitorFoodRouteFailure();
    }
    if (visitorFoodRouteFailureStillValid()) {
        return false;
    }
    clearVisitorFoodRouteFailure();
    if (bowlEaterSlot == 0 && teamMemberCanEatFromBowl(1)) {
        visitor.stateUntilMs = nowMs + 500;
        return true;
    }
    if (teamMemberCanEatFromBowl(0) &&
        (int32_t)(nowMs - nextMindUpdateMs) >= 0) {
        return false;
    }
    if (!claimBowl(1)) return false;

    visitor.targetX = foodFeedX();
    visitor.targetY = foodFeedY() - visitorGeometry().groundOffsetY;
    if (!buildFoodApproachRoute(
            false, visitor.targetX, visitor.targetY)) {
        releaseBowl(1);
        rememberVisitorFoodRouteFailure();
#if STICKMON_ENABLE_TRACE_LOGS
        STICKMON_TRACEF(
            "[FoodRoute] moving=0 retry=state_change reason=route_failed\n");
#endif
        return false;
    }

    clearVisitorFoodRouteFailure();
    visitor.state = VisitorState::SEEK_FOOD;
    visitor.stateUntilMs = nowMs + DOOR_ROUTE_TIMEOUT_MS;
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    return true;
}

bool MainScene::visitorFoodRouteFailureStillValid() const {
    if (!visitorFoodRouteFailure.valid) return false;
    const GameEngine& engine = GameEngine::ins();
    auto navCell = [](float value) {
        return static_cast<int16_t>(floorf(value / NAV_CELL_PX));
    };
    return visitorFoodRouteFailure.bowlFood == engine.bowlFoodIndex() &&
           visitorFoodRouteFailure.bowlCount == engine.bowlFoodCount() &&
           visitorFoodRouteFailure.bowlBites ==
               engine.bowlFoodBitesRemaining() &&
           visitorFoodRouteFailure.mainCellX == navCell(monsterX) &&
           visitorFoodRouteFailure.mainCellY == navCell(monsterY) &&
           visitorFoodRouteFailure.visitorCellX == navCell(visitor.x) &&
           visitorFoodRouteFailure.visitorCellY == navCell(visitor.y);
}

void MainScene::clearVisitorFoodRouteFailure() {
    visitorFoodRouteFailure = FoodRouteFailure{};
}

void MainScene::rememberVisitorFoodRouteFailure() {
    const GameEngine& engine = GameEngine::ins();
    auto navCell = [](float value) {
        return static_cast<int16_t>(floorf(value / NAV_CELL_PX));
    };
    visitorFoodRouteFailure.valid = true;
    visitorFoodRouteFailure.bowlFood = engine.bowlFoodIndex();
    visitorFoodRouteFailure.bowlCount = engine.bowlFoodCount();
    visitorFoodRouteFailure.bowlBites = engine.bowlFoodBitesRemaining();
    visitorFoodRouteFailure.mainCellX = navCell(monsterX);
    visitorFoodRouteFailure.mainCellY = navCell(monsterY);
    visitorFoodRouteFailure.visitorCellX = navCell(visitor.x);
    visitorFoodRouteFailure.visitorCellY = navCell(visitor.y);
}

bool MainScene::startVisitorFoodYield(uint32_t nowMs) {
    if (!visitor.active || doorVisitorHidden || doorMainHidden ||
        visitor.dropOffsetY > 0.0f || visitor.state != VisitorState::IDLE ||
        pairInteraction != PairInteraction::NONE ||
        contactDialog != ContactDialog::NONE ||
        contactGuestMotion != ContactGuestMotion::NONE) {
        return false;
    }

    const ActorGeometry geometry = visitorGeometry();
    float foodX = foodFeedX();
    float foodY = foodFeedY();
    float clearance = doorActorMinSeparation() + 6.0f;
    float clearanceSq = clearance * clearance;
    for (uint8_t tries = 0; tries < 48; ++tries) {
        float candidateX = 0.0f;
        float candidateY = 0.0f;
        if (!randomActorCenterWalkPoint(
                geometry, candidateX, candidateY)) {
            break;
        }
        float foodDx = candidateX - foodX;
        float foodDy = candidateY + geometry.groundOffsetY - foodY;
        if (foodDx * foodDx + foodDy * foodDy < clearanceSq ||
            !actorTargetsKeepSpacing(
                candidateX, candidateY, geometry,
                monsterX, monsterY, mainGeometry()) ||
            !buildVisitorMoveRoute(candidateX, candidateY)) {
            continue;
        }

        visitor.targetX = candidateX;
        visitor.targetY = candidateY;
        visitor.state = VisitorState::WALK;
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        STICKMON_TRACEF(
            "[MainAI] event=visitor_food_yield t=%lu from=%.1f,%.1f "
            "to=%.1f,%.1f\n",
            static_cast<unsigned long>(nowMs), visitor.x, visitor.y,
            visitor.targetX, visitor.targetY);
        return true;
    }
    return false;
}

void MainScene::enterVisitorFeeding(uint32_t nowMs) {
    clearVisitorMoveRoute();
    visitor.state = VisitorState::FEEDING;
    visitor.targetX = visitor.x;
    visitor.targetY = visitor.y;
    visitor.direction = visitorWalkDirectionForDelta(
        foodCenterX() - visitor.x,
        foodCenterY() -
            (visitor.y + visitorGeometry().groundOffsetY));
    if (fabsf(foodCenterX() - visitor.x) > 0.5f) {
        visitor.facingRight = foodCenterX() > visitor.x;
    }
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    visitorFeedingBiteMs =
        nowMs + (uint32_t)GameRandom::random(FEED_BITE_DELAY_MIN_MS,
                                 FEED_BITE_DELAY_MAX_MS + 1);
    visitorFeedingUntilMs =
        nowMs + (uint32_t)GameRandom::random(FEED_SESSION_MIN_MS,
                                 FEED_SESSION_MAX_MS + 1);
}

void MainScene::updateVisitorFoodSeek(uint32_t nowMs, float dtSeconds) {
    if (bowlEaterSlot != 1 || !teamMemberCanEatFromBowl(1) ||
        (int32_t)(nowMs - visitor.stateUntilMs) >= 0) {
        clearVisitorMoveRoute();
        releaseBowl(1);
        visitor.state = VisitorState::IDLE;
        visitor.stateUntilMs =
            nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        return;
    }

    float waypointX = visitor.targetX;
    float waypointY = visitor.targetY;
    if (!currentVisitorWaypoint(waypointX, waypointY)) {
        float dx = visitor.targetX - visitor.x;
        float dy = visitor.targetY - visitor.y;
        if (dx * dx + dy * dy <= 2.25f) {
            enterVisitorFeeding(nowMs);
        } else if (!buildVisitorMoveRoute(
                       visitor.targetX, visitor.targetY)) {
            handleVisitorMoveBlocked(nowMs, false);
        }
        return;
    }

    doorLastUpdateMs = nowMs;
    float speed = visitorMoveSpeed(visitorBehaviorProfile(), true);
    if (!moveVisitorDoorToward(waypointX, waypointY, speed,
                               dtSeconds, true, true)) {
        visitor.state = VisitorState::SEEK_FOOD;
        handleVisitorMoveBlocked(nowMs, false);
        return;
    }
    visitorMoveBlockedSinceMs = 0;
    visitor.state = VisitorState::SEEK_FOOD;
    ++visitorMoveRouteIndex;
    if (visitorMoveRouteIndex < visitorMoveRouteCount) return;
    enterVisitorFeeding(nowMs);
}

void MainScene::updateVisitorFeeding(uint32_t nowMs) {
    const Game::GameState& state = GameEngine::ins().gameState();
    if (bowlEaterSlot != 1 || state.teamCount < 2 ||
        state.team[1].origin == Game::Origin::VISITOR ||
        state.team[1].fainted || state.team[1].hpCur == 0) {
        releaseBowl(1);
        visitor.state = VisitorState::IDLE;
        visitor.stateUntilMs =
            nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        return;
    }

    const Game::MonsterRuntime& mon = state.team[1];
    if ((int32_t)(nowMs - visitorFeedingBiteMs) >= 0 &&
        GameEngine::ins().bowlHasFood() &&
        mon.satiety < MONSTER_FEED_TARGET_SATIETY) {
        FoodConsumeResult result = GameEngine::ins().consumeBowlFood(1);
        visitorFeedingBiteMs =
            nowMs + (uint32_t)GameRandom::random(FEED_BITE_INTERVAL_MIN_MS,
                                     FEED_BITE_INTERVAL_MAX_MS + 1);
        if (!result.consumed) visitorFeedingUntilMs = nowMs + 600;
        if (result.reaction == FoodReaction::DISLIKED) {
            visitorFeedingBiteMs += 900;
        }
    }
    if (!GameEngine::ins().bowlHasFood() ||
        mon.satiety >= MONSTER_FEED_TARGET_SATIETY) {
        if ((int32_t)(visitorFeedingUntilMs - (nowMs + 700)) > 0) {
            visitorFeedingUntilMs = nowMs + 700;
        }
    }
    if ((int32_t)(nowMs - visitorFeedingUntilMs) < 0) {
        advanceVisitorFrames(nowMs, false);
        return;
    }

    releaseBowl(1);
    visitor.state = VisitorState::IDLE;
    visitor.stateUntilMs =
        nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
}

void MainScene::restFaintedVisitor(uint32_t nowMs) {
    releaseBowl(1);
    clearVisitorMoveRoute();
    visitor.dropOffsetY = 0.0f;

    if (!visitor.sleepSpotValid ||
        !visitorSleepSpotUsable(visitor.sleepX, visitor.sleepY)) {
        float sleepX = visitor.x;
        float sleepY = visitor.y;
        visitor.sleepSpotValid = pickVisitorSleepSpot(sleepX, sleepY);
        visitor.sleepX = sleepX;
        visitor.sleepY = sleepY;
    }

    bool enteringRest =
        visitor.state != VisitorState::SLEEPING ||
        fabsf(visitor.x - visitor.sleepX) > 0.5f ||
        fabsf(visitor.y - visitor.sleepY) > 0.5f;
    visitor.x = visitor.sleepX;
    visitor.y = visitor.sleepY;
    visitor.targetX = visitor.x;
    visitor.targetY = visitor.y;
    visitor.state = VisitorState::SLEEPING;
    if (enteringRest) {
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        logVisitorSleepEvent(
            "enter_fainted", nowMs,
            GameEngine::ins().gameState().team[1]);
        nextVisitorSleepTraceMs =
            nowMs + VISITOR_SLEEP_TRACE_INTERVAL_MS;
    }
}

void MainScene::spawnVisitor(uint32_t nowMs, bool dropIn) {
    const Game::MonsterRuntime& guest = GameEngine::ins().gameState().team[1];
    visitor.active = true;
    visitor.speciesId = guest.speciesId;
    float x = monsterX;
    float y = monsterY;
    pickVisitorPoint(x, y);
    visitor.x = x;
    visitor.y = y;
    visitor.targetX = x;
    visitor.targetY = y;
    visitor.dropOffsetY = dropIn ? VISITOR_DROP_HEIGHT : 0.0f;
    visitor.sleepSpotValid = false;
    visitor.sleepX = x;
    visitor.sleepY = y;
    visitor.foodRetryAfterMs = 0;
    clearVisitorFoodRouteFailure();
    mainYieldingForVisitorFood = false;
    visitor.state = VisitorState::IDLE;
    visitor.stateUntilMs =
        nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    visitor.direction = PokemonSprites::WalkDirection::DOWN;
    visitor.facingRight = true;
}

bool MainScene::pickVisitorPoint(float& x, float& y) const {
    const ActorGeometry geometry = visitorGeometry();
    float minSeparation = MathUtil::max(
        VISITOR_AVOID_RADIUS, doorActorMinSeparation());
    float bestX = x;
    float bestY = y;
    float bestDistanceSq = -1.0f;
    for (uint8_t tries = 0; tries < 64; ++tries) {
        float px = 0.0f;
        float py = 0.0f;
        if (!randomActorCenterWalkPoint(geometry, px, py)) break;
        float dx = px - monsterX;
        float dy = py + geometry.groundOffsetY -
                   (monsterY + mainGeometry().groundOffsetY);
        float distanceSq = dx * dx + dy * dy;
        if (distanceSq > bestDistanceSq) {
            bestX = px;
            bestY = py;
            bestDistanceSq = distanceSq;
        }
        if (distanceSq >= minSeparation * minSeparation) {
            x = px;
            y = py;
            return true;
        }
    }
    if (bestDistanceSq < 0.0f) return false;
    x = bestX;
    y = bestY;
    return true;
}

float MainScene::visitorSleepMinDistance() const {
    float mainRadius = MathUtil::clamp(
        mainGeometry().footprint.radiusX, 10.0f, 20.0f);
    float visitorRadius = MathUtil::clamp(
        visitorGeometry().footprint.radiusX, 10.0f, 20.0f);
    return MathUtil::clamp(
        mainRadius + visitorRadius + 8.0f, 30.0f, 44.0f);
}

bool MainScene::visitorSleepSpotUsableWithDistance(
    float x, float y, float minDistance) const {
    const ActorGeometry geometry = visitorGeometry();
    if (!actorFootprintInsideWalkArea(x, y, geometry)) return false;

    float footY = y + geometry.groundOffsetY;
    if (roomBedContains(x, footY)) return false;

    float dx = x - bedSleepX();
    float dy = footY - bedSleepY();
    float distanceSq = dx * dx + dy * dy;
    return distanceSq >= minDistance * minDistance;
}

bool MainScene::visitorSleepSpotUsable(float x, float y) const {
    return visitorSleepSpotUsableWithDistance(
        x, y, visitorSleepMinDistance());
}

bool MainScene::pickVisitorSleepSpot(float& x, float& y) const {
    float offsetY = visitorGeometry().groundOffsetY;
    float minDistance = visitorSleepMinDistance();
    bool mainSeekingBed = aiMode == AiMode::SEEK_BED ||
        (aiMode == AiMode::TURNING && turnNextMode == AiMode::SEEK_BED);
    float bestScore = 1000000.0f;
    bool found = false;

    RoomResource::BehaviorAnchor sleepAnchor{};
    bool hasSleepAnchor = room().findBehaviorAnchor(
        RoomResource::BehaviorAnchorType::VISITOR_SLEEP, sleepAnchor);
    float preferredX = hasSleepAnchor
        ? static_cast<float>(sleepAnchor.footX)
        : (roomWalkMinX() + roomWalkMaxX()) * 0.5f;
    float preferredFootY = hasSleepAnchor
        ? static_cast<float>(sleepAnchor.footY)
        : (roomWalkMinY() + roomWalkMaxY()) * 0.5f;

    float anchoredY = preferredFootY - offsetY;
    if (hasSleepAnchor &&
        visitorSleepSpotUsableWithDistance(
            preferredX, anchoredY, minDistance) &&
        (!mainSeekingBed ||
         !visitorPointBlocksBedRoute(preferredX, anchoredY))) {
        x = preferredX;
        y = anchoredY;
        return true;
    }

    // Older room packs have no sleep anchor. Search toward the activity-area
    // center while retaining bed separation and path clearance.
    for (int footY = roomWalkMinY(); footY <= roomWalkMaxY(); footY += 2) {
        float centerY = (float)footY - offsetY;
        for (int px = roomWalkMinX(); px <= roomWalkMaxX(); px += 2) {
            float centerX = (float)px;
            if (!visitorSleepSpotUsableWithDistance(
                    centerX, centerY, minDistance)) {
                continue;
            }
            if (mainSeekingBed &&
                visitorPointBlocksBedRoute(centerX, centerY)) {
                continue;
            }

            float dx = centerX - preferredX;
            float dy = (float)footY - preferredFootY;
            float score = dx * dx + dy * dy;
            if (score >= bestScore) continue;

            bestScore = score;
            x = centerX;
            y = centerY;
            found = true;
        }
    }
    return found;
}

bool MainScene::visitorPointBlocksBedRoute(float x, float y) const {
    float clearance = doorActorMinSeparation() + 2.0f;
    float clearanceSq = clearance * clearance;
    float visitorFootY = y + visitorGeometry().groundOffsetY;
    float mainOffsetY = mainGeometry().groundOffsetY;
    auto segmentDistanceSq = [x, visitorFootY, mainOffsetY](
                                    float fromX, float fromY,
                                    float toX, float toY) {
        fromY += mainOffsetY;
        toY += mainOffsetY;
        float dx = toX - fromX;
        float dy = toY - fromY;
        float lengthSq = dx * dx + dy * dy;
        float t = lengthSq <= 0.001f
            ? 0.0f
            : ((x - fromX) * dx + (visitorFootY - fromY) * dy) /
                  lengthSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float nearestX = fromX + dx * t;
        float nearestY = fromY + dy * t;
        float distanceX = x - nearestX;
        float distanceY = visitorFootY - nearestY;
        return distanceX * distanceX + distanceY * distanceY;
    };

    float fromX = monsterX;
    float fromY = monsterY;
    if (moveRouteIndex >= moveRouteCount) {
        return segmentDistanceSq(fromX, fromY, targetX, targetY) < clearanceSq;
    }
    for (uint8_t index = moveRouteIndex; index < moveRouteCount; ++index) {
        float toX = moveRouteX[index];
        float toY = moveRouteY[index];
        if (segmentDistanceSq(fromX, fromY, toX, toY) < clearanceSq) return true;
        fromX = toX;
        fromY = toY;
    }
    return false;
}

bool MainScene::startVisitorBedYield(uint32_t nowMs) {
    clearVisitorMoveRoute();
    float candidateX = visitor.x;
    float candidateY = visitor.y;
    if (!pickVisitorSleepSpot(candidateX, candidateY) ||
        visitorPointBlocksBedRoute(candidateX, candidateY) ||
        !buildVisitorMoveRoute(candidateX, candidateY)) {
        clearVisitorMoveRoute();
        return false;
    }

    visitor.targetX = candidateX;
    visitor.targetY = candidateY;
    visitor.sleepSpotValid = false;
    visitor.state = VisitorState::YIELDING_BED;
    visitor.frameStartedMs = nowMs;
    visitor.frameIndex = 0;
    logVisitorSleepEvent(
        "yield_bed", nowMs,
        GameEngine::ins().gameState().team[1]);
    return true;
}

void MainScene::logVisitorSleepEvent(
    const char* event, uint32_t nowMs,
    const Game::MonsterRuntime& mon) const {
#if STICKMON_ENABLE_TRACE_LOGS
    uint16_t gameMinute = GameEngine::ins().gameMinutesOfDay();
    Game::MonsterSleepSchedule schedule =
        Game::sleepScheduleForNature(mon.nature);
    int32_t wakeInMs = static_cast<int32_t>(visitor.stateUntilMs - nowMs);
    if (wakeInMs < 0) wakeInMs = 0;
    int32_t retryInMs =
        static_cast<int32_t>(visitor.foodRetryAfterMs - nowMs);
    if (retryInMs < 0) retryInMs = 0;
    bool spotUsable = visitorSleepSpotUsable(visitor.x, visitor.y);
    Platform::logf(
        "[VisitorSleep] event=%s t=%u game=%02u:%02u species=%u state=%u "
        "schedule=%02u:%02u-%02u:%02u due=%u status=%u faint=%u "
        "hp=%u/%u hun=%u urgent=%u bowl=%u canEat=%u eater=%d "
        "pos=%.1f,%.1f sleep=%.1f,%.1f spot=%u "
        "wake_in=%ldms retry_in=%ldms\n",
        event ? event : "unknown", nowMs,
        static_cast<unsigned>(gameMinute / 60),
        static_cast<unsigned>(gameMinute % 60),
        static_cast<unsigned>(mon.speciesId),
        static_cast<unsigned>(visitor.state),
        static_cast<unsigned>(schedule.startMinute / 60),
        static_cast<unsigned>(schedule.startMinute % 60),
        static_cast<unsigned>(schedule.endMinute / 60),
        static_cast<unsigned>(schedule.endMinute % 60),
        monsterIsSleepTime(mon) ? 1U : 0U,
        static_cast<unsigned>(mon.majorStatus), mon.fainted ? 1U : 0U,
        static_cast<unsigned>(mon.hpCur), static_cast<unsigned>(mon.hpMax),
        static_cast<unsigned>(mon.satiety),
        monsterShouldWakeForFood(mon.satiety) ? 1U : 0U,
        GameEngine::ins().bowlHasFood() ? 1U : 0U,
        teamMemberCanEatFromBowl(1) ? 1U : 0U,
        static_cast<int>(bowlEaterSlot),
        visitor.x, visitor.y, visitor.sleepX, visitor.sleepY,
        spotUsable ? 1U : 0U, static_cast<long>(wakeInMs),
        static_cast<long>(retryInMs));
#else
    (void)event;
    (void)nowMs;
    (void)mon;
#endif
}

MonsterBehaviorProfile MainScene::visitorBehaviorProfile() const {
    const Game::GameState& state = GameEngine::ins().gameState();
    if (state.teamCount < 2) return MonsterBehaviorProfile{};
    const Game::MonsterRuntime& monster = state.team[1];
    const Species* species = findSpecies(monster.speciesId);
    return behaviorProfileFor(species ? *species : starterSpecies(), monster);
}

uint32_t MainScene::visitorIdleDelayMs(
    const MonsterBehaviorProfile& profile) const {
    return static_cast<uint32_t>(GameRandom::random(
        profile.idleMinMs, profile.idleMaxMs + 1));
}

float MainScene::visitorMoveSpeed(
    const MonsterBehaviorProfile& profile, bool purposeful) const {
    float speed = (purposeful ? 19.0f : 10.5f) * profile.moveSpeedScale;
    const Game::GameState& state = GameEngine::ins().gameState();
    if (state.teamCount >= 2) {
        const Game::MonsterRuntime& monster = state.team[1];
        if (monster.mood < 40 || monster.satiety < 20) speed *= 0.72f;
    }
    return MathUtil::max(3.0f, speed);
}

void MainScene::advanceVisitorFrames(uint32_t nowMs, bool walking) {
    const PmdSpriteConfig* config =
        pmdSpriteConfigForSpecies(visitor.speciesId);
    const PokemonMotion::Behavior motion =
        PokemonMotion::behaviorForSpecies(visitor.speciesId);
    if (walking && config && motion.mode == PokemonMotion::Mode::SLITHER) {
        if (visitor.motionCycleMs == 0) {
            visitor.motionCycleMs = PokemonMotion::cycleDurationMs(
                motion, PokemonMotion::PlaybackContext::AMBIENT,
                visitorMoveSpeed(visitorBehaviorProfile(), false));
        }
        uint8_t direction = 0;
        switch (visitor.direction) {
        case PokemonSprites::WalkDirection::DOWN: direction = 0; break;
        case PokemonSprites::WalkDirection::LEFT: direction = 2; break;
        case PokemonSprites::WalkDirection::UP: direction = 4; break;
        case PokemonSprites::WalkDirection::RIGHT: direction = 6; break;
        }
        PokemonMotion::Pose pose = PokemonMotion::slitherPose(
            motion, config->walkingFrames, nowMs - visitor.frameStartedMs,
            visitor.motionCycleMs, direction);
        visitor.frameIndex = pose.frameIndex;
        visitor.renderOffsetX = pose.offsetX;
        visitor.renderOffsetY = pose.offsetY;
        return;
    }

    visitor.renderOffsetX = 0;
    visitor.renderOffsetY = 0;
    visitor.motionCycleMs = 0;
    uint16_t frameMs = walking
        ? motion.moveFrameMs
        : (config ? config->idleFrameMs : PMD_IDLE_FRAME_MS);
    if (frameMs == 0) frameMs = PMD_IDLE_FRAME_MS;
    while (nowMs - visitor.frameStartedMs >= frameMs) {
        visitor.frameIndex++;
        visitor.frameStartedMs += frameMs;
    }
}

void MainScene::updateVisitor(uint32_t nowMs, float dtSeconds) {
    if (!visitorHostActive()) {
        releaseBowl(1);
        visitor.active = false;
        return;
    }
    const Game::MonsterRuntime& visitorMonster =
        GameEngine::ins().gameState().team[1];
    uint16_t speciesId = visitorMonster.speciesId;
    if (!visitor.active || visitor.speciesId != speciesId) {
        releaseBowl(1);
        spawnVisitor(nowMs, true);
        if (visitorMonster.fainted || visitorMonster.hpCur == 0) {
            restFaintedVisitor(nowMs);
        }
        return;
    }

    if (visitorMonster.fainted || visitorMonster.hpCur == 0) {
        restFaintedVisitor(nowMs);
        while (nowMs - visitor.frameStartedMs >= VISITOR_SLEEP_FRAME_MS) {
            visitor.frameIndex++;
            visitor.frameStartedMs += VISITOR_SLEEP_FRAME_MS;
        }
        return;
    }

    const Game::SpeciesCareProfile careProfile =
        Game::speciesCareProfileFor(speciesId);
    if (visitor.dropOffsetY > 0.0f) {
        visitor.dropOffsetY -= VISITOR_DROP_SPEED * dtSeconds;
        if (visitor.dropOffsetY < 0.0f) visitor.dropOffsetY = 0.0f;
        advanceVisitorFrames(nowMs, false);
        return;
    }

    if (!careProfile.canMove) {
        releaseBowl(1);
        visitor.state = VisitorState::IDLE;
        visitor.targetX = visitor.x;
        visitor.targetY = visitor.y;
        visitor.frameIndex = 0;
        visitor.frameStartedMs = nowMs;
        return;
    }

    bool sleepTime = monsterIsSleepTime(visitorMonster);

    bool mainSeekingBed = aiMode == AiMode::SEEK_BED ||
        (aiMode == AiMode::TURNING && turnNextMode == AiMode::SEEK_BED);
    bool mainOccupyingBed = aiMode == AiMode::RESTING ||
        aiMode == AiMode::WAKING || monsterAtBedSleepPose();
    if (mainOccupyingBed) {
        visitorBedYieldHandled = true;
    } else if (!mainSeekingBed) {
        visitorBedYieldHandled = false;
    }
    if (visitor.state == VisitorState::SLEEPING && mainSeekingBed &&
        !visitorBedYieldHandled && !monsterAtBedSleepPose() &&
        visitorMonster.majorStatus != Game::MajorStatus::SLEEP &&
        visitorPointBlocksBedRoute(visitor.x, visitor.y) &&
        startVisitorBedYield(nowMs)) {
        visitorBedYieldHandled = true;
        return;
    }

    if (visitor.state == VisitorState::SEEK_FOOD) {
        updateVisitorFoodSeek(nowMs, dtSeconds);
        return;
    }
    if (visitor.state == VisitorState::FEEDING) {
        updateVisitorFeeding(nowMs);
        return;
    }

    if (visitor.state == VisitorState::SLEEPING) {
        if ((int32_t)(nowMs - nextVisitorSleepTraceMs) >= 0) {
            logVisitorSleepEvent("heartbeat", nowMs, visitorMonster);
            nextVisitorSleepTraceMs =
                nowMs + VISITOR_SLEEP_TRACE_INTERVAL_MS;
        }
        if (!sleepTime) {
            logVisitorSleepEvent("wake_schedule", nowMs, visitorMonster);
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs =
                nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
            visitor.frameStartedMs = nowMs;
            visitor.frameIndex = 0;
            return;
        }
        if (!visitorSleepSpotUsable(visitor.x, visitor.y)) {
            logVisitorSleepEvent("wake_spot_invalid", nowMs, visitorMonster);
            clearVisitorMoveRoute();
            visitor.sleepSpotValid = false;
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs = nowMs;
            visitor.frameStartedMs = nowMs;
            visitor.frameIndex = 0;
            return;
        }
        if (monsterShouldWakeForFood(visitorMonster.satiety) &&
            (int32_t)(nowMs - visitor.foodRetryAfterMs) >= 0 &&
            teamMemberCanEatFromBowl(1) &&
            (int32_t)(nowMs - visitor.stateUntilMs) >= 0) {
            logVisitorSleepEvent("wake_food_check", nowMs, visitorMonster);
            visitor.foodRetryAfterMs =
                nowMs + VISITOR_NIGHT_FOOD_RETRY_MS;
            visitor.state = VisitorState::IDLE;
            startVisitorFoodSeek(nowMs);
            if (visitor.state == VisitorState::SEEK_FOOD) {
                logVisitorSleepEvent("wake_food_seek", nowMs, visitorMonster);
                return;
            }
            visitor.state = VisitorState::SLEEPING;
            visitor.stateUntilMs = nowMs + 1000;
            logVisitorSleepEvent("food_seek_blocked", nowMs, visitorMonster);
        }
        while (nowMs - visitor.frameStartedMs >= VISITOR_SLEEP_FRAME_MS) {
            visitor.frameIndex++;
            visitor.frameStartedMs += VISITOR_SLEEP_FRAME_MS;
        }
        return;
    }

    if (sleepTime && visitor.state != VisitorState::GO_TO_SLEEP &&
        visitor.state != VisitorState::YIELDING_BED) {
        if (visitor.state == VisitorState::IDLE &&
            (int32_t)(nowMs - visitor.stateUntilMs) < 0) {
            advanceVisitorFrames(nowMs, false);
            return;
        }
        if (!visitor.sleepSpotValid ||
            !visitorSleepSpotUsable(visitor.sleepX, visitor.sleepY)) {
            visitor.sleepSpotValid =
                pickVisitorSleepSpot(visitor.sleepX, visitor.sleepY);
        }
        if (!visitor.sleepSpotValid ||
            !buildVisitorMoveRoute(visitor.sleepX, visitor.sleepY)) {
            clearVisitorMoveRoute();
            visitor.sleepSpotValid = false;
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs = nowMs + 1000;
            advanceVisitorFrames(nowMs, false);
            return;
        }
        visitor.targetX = visitor.sleepX;
        visitor.targetY = visitor.sleepY;
        visitor.state = VisitorState::GO_TO_SLEEP;
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
    } else if (!sleepTime && visitor.state == VisitorState::GO_TO_SLEEP) {
        visitor.state = VisitorState::IDLE;
        visitor.stateUntilMs =
            nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        return;
    }

    if (visitor.state == VisitorState::IDLE) {
        advanceVisitorFrames(nowMs, false);
        if (startVisitorFoodSeek(nowMs)) return;
        if ((int32_t)(nowMs - visitor.stateUntilMs) < 0) return;
        float tx = 0.0f;
        float ty = 0.0f;
        if (pickVisitorPoint(tx, ty)) {
            visitor.targetX = tx;
            visitor.targetY = ty;
            if (buildVisitorMoveRoute(tx, ty)) {
                visitor.state = VisitorState::WALK;
                visitor.frameStartedMs = nowMs;
                visitor.frameIndex = 0;
            } else {
                visitor.stateUntilMs =
                    nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
            }
        } else {
            visitor.stateUntilMs =
                nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        }
        return;
    }

    float movementTargetX = visitor.targetX;
    float movementTargetY = visitor.targetY;
    bool movingToSleep = visitor.state == VisitorState::GO_TO_SLEEP ||
                         visitor.state == VisitorState::YIELDING_BED;
    bool followingRoute =
        currentVisitorWaypoint(movementTargetX, movementTargetY);
    float targetDx = visitor.targetX - visitor.x;
    float targetDy = visitor.targetY - visitor.y;
    float targetDistanceSq = targetDx * targetDx + targetDy * targetDy;
    float routeArrival = movingToSleep ? VISITOR_SLEEP_ARRIVE_DIST : 1.0f;
    if (!followingRoute && targetDistanceSq > routeArrival * routeArrival) {
        if (!buildVisitorMoveRoute(visitor.targetX, visitor.targetY) ||
            !currentVisitorWaypoint(movementTargetX, movementTargetY)) {
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs =
                nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
            if (movingToSleep) visitor.sleepSpotValid = false;
            return;
        }
        followingRoute = true;
    }
    float dx = movementTargetX - visitor.x;
    float dy = movementTargetY - visitor.y;
    float distance = sqrtf(dx * dx + dy * dy);
    const MonsterBehaviorProfile profile = visitorBehaviorProfile();
    float step = visitorMoveSpeed(profile, movingToSleep) * dtSeconds;
    float arriveDist = routeArrival;
    if (distance <= step || distance < arriveDist) {
        if (!doorMainHidden &&
            !doorStepKeepsSpacing(visitor.x, visitor.y,
                                  movementTargetX, movementTargetY,
                                  monsterX, monsterY, true)) {
            handleVisitorMoveBlocked(nowMs, movingToSleep);
            return;
        }
        visitor.x = movementTargetX;
        visitor.y = movementTargetY;
        visitorMoveBlockedSinceMs = 0;
        if (followingRoute) {
            ++visitorMoveRouteIndex;
            if (visitorMoveRouteIndex < visitorMoveRouteCount) {
                visitor.frameStartedMs = nowMs;
                visitor.frameIndex = 0;
                return;
            }
            clearVisitorMoveRoute();
        }
        if (movingToSleep) {
            VisitorState previousState = visitor.state;
            if (visitor.state == VisitorState::YIELDING_BED) {
                visitor.sleepX = visitor.x;
                visitor.sleepY = visitor.y;
                visitor.sleepSpotValid = true;
            }
            visitor.state = VisitorState::SLEEPING;
            visitor.stateUntilMs =
                nowMs + (uint32_t)GameRandom::random(NIGHT_FEED_WAKE_DELAY_MIN_MS,
                                         NIGHT_FEED_WAKE_DELAY_MAX_MS + 1);
            logVisitorSleepEvent(
                previousState == VisitorState::YIELDING_BED
                    ? "enter_after_yield"
                    : "enter_schedule",
                nowMs, visitorMonster);
            nextVisitorSleepTraceMs =
                nowMs + VISITOR_SLEEP_TRACE_INTERVAL_MS;
        } else {
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs =
                nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
        }
        visitor.frameStartedMs = nowMs;
        visitor.frameIndex = 0;
        return;
    }
    float nextX = visitor.x + dx / distance * step;
    float nextY = visitor.y + dy / distance * step;
    if (!doorStepKeepsSpacing(visitor.x, visitor.y, nextX, nextY,
                              monsterX, monsterY, true)) {
        handleVisitorMoveBlocked(nowMs, movingToSleep);
        return;
    }
    if (!actorFootprintInsideWalkArea(
            nextX, nextY, visitorGeometry())) {
        if (movingToSleep) {
            clearVisitorMoveRoute();
            visitor.sleepSpotValid = false;
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs = nowMs + 1000;
            visitor.frameStartedMs = nowMs;
            visitor.frameIndex = 0;
        } else {
            visitor.state = VisitorState::IDLE;
            visitor.stateUntilMs =
                nowMs + visitorIdleDelayMs(visitorBehaviorProfile());
            visitor.frameStartedMs = nowMs;
        }
        return;
    }
    visitor.x = nextX;
    visitor.y = nextY;
    visitorMoveBlockedSinceMs = 0;
    visitor.direction = visitorWalkDirectionForDelta(dx, dy);
    if (fabsf(dx) > 0.5f) visitor.facingRight = dx > 0.0f;
    advanceVisitorFrames(nowMs, true);
}

const PokemonSprites::SpriteFrame* MainScene::visitorCurrentFrame(bool& flipX) const {
    flipX = false;
    if (!visitor.active) return nullptr;
    bool sleeping = visitor.state == VisitorState::SLEEPING && visitor.dropOffsetY <= 0.0f;
    if (sleeping) {
        if (const PmdSpriteConfig* config = pmdSpriteConfigForSpecies(visitor.speciesId)) {
            uint8_t frameCount = config->sleepingFrames == 0 ? 1 : config->sleepingFrames;
            const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
                visitor.speciesId,
                static_cast<PokemonSprites::SpriteKind>(
                    static_cast<uint16_t>(config->sleepingBase) + visitor.frameIndex % frameCount));
            if (frame) return frame;
        }
        flipX = visitor.facingRight;
        return PokemonSprites::findSpeciesSprite(visitor.speciesId, PokemonSprites::SpriteKind::FRONT);
    }
    bool walking = (visitor.state == VisitorState::WALK ||
                    visitor.state == VisitorState::SEEK_FOOD ||
                    visitor.state == VisitorState::GO_TO_SLEEP ||
                    visitor.state == VisitorState::YIELDING_BED) &&
                   visitor.dropOffsetY <= 0.0f;
    if (walking) {
        PokemonSprites::WalkingAnimation animation{};
        if (PokemonSprites::walkingAnimation(visitor.speciesId, visitor.direction, animation)) {
            uint8_t frameCount = animation.frameCount == 0 ? 1 : animation.frameCount;
            const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
                visitor.speciesId,
                static_cast<PokemonSprites::SpriteKind>(
                    static_cast<uint16_t>(animation.base) + visitor.frameIndex % frameCount));
            if (frame) {
                flipX = animation.flipX;
                return frame;
            }
        }
    } else if (const PmdSpriteConfig* config = pmdSpriteConfigForSpecies(visitor.speciesId)) {
        uint8_t frameCount = config->idleFrames == 0 ? 1 : config->idleFrames;
        uint16_t directionIndex = visitorIdleDirectionFrameIndex(
            config, visitor.direction, flipX);
        const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
            visitor.speciesId,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(config->idleBase) +
                directionIndex * frameCount +
                visitor.frameIndex % frameCount));
        if (frame) return frame;
    }
    flipX = visitor.facingRight;
    return PokemonSprites::findSpeciesSprite(visitor.speciesId, PokemonSprites::SpriteKind::FRONT);
}

void MainScene::drawVisitorShadow() {
    if (doorVisitorHidden) return;
    if (visitor.state == VisitorState::SLEEPING) return;
    bool flipX = false;
    const PokemonSprites::SpriteFrame* frame = visitorCurrentFrame(flipX);
    if (!frame) return;

    bool night = mainSceneIsNight();
    const PmdSpriteConfig* config =
        pmdSpriteConfigForSpecies(visitor.speciesId);
    bool floating = config && config->airHeight > 0.0f;
    uint8_t frameW = FlashStorage::readByte(&frame->width);
    uint8_t frameH = FlashStorage::readByte(&frame->height);
    int rx = MathUtil::clamp(
        static_cast<int>(frameW * (floating ? 0.25f : 0.44f)),
        floating ? 10 : 16, floating ? 22 : 40);
    int ry = MathUtil::clamp(
        static_cast<int>(frameH * (floating ? 0.055f : 0.14f)),
        floating ? 3 : 6, floating ? 6 : 14);
    if (night) {
        rx = (rx * 11 + 5) / 10;
        ry = (ry * 11 + 5) / 10;
    }
    int shadowX = (int)roundf(visitor.x) + visitor.renderOffsetX;
    int shadowY = worldToScreenY(
        visitor.y + PokemonSprites::frameGroundOffsetY(frame) +
        visitor.renderOffsetY);
    uint16_t shadowColor = night ? PixelRenderer::rgb(18, 16, 24) : PixelRenderer::rgb(36, 29, 24);
    uint8_t outerAlpha =
        night ? (floating ? 84 : 122) : (floating ? 68 : 116);
    uint8_t coreAlpha =
        night ? (floating ? 0 : 92) : (floating ? 0 : 86);
    fillSoftEllipseAlpha(shadowX, shadowY, rx, ry, shadowColor, outerAlpha);
    if (!floating) {
        fillEllipseAlpha(shadowX, shadowY, MathUtil::max(5, rx / 2),
                         MathUtil::max(2, ry / 2), shadowColor, coreAlpha);
    }
}

void MainScene::drawVisitor() {
    if (doorVisitorHidden) return;
    bool flipX = false;
    const PokemonSprites::SpriteFrame* frame = visitorCurrentFrame(flipX);
    if (!frame) return;

    int x = (int)roundf(visitor.x) + visitor.renderOffsetX;
    bool sleeping =
        visitor.state == VisitorState::SLEEPING &&
        visitor.dropOffsetY <= 0.0f;
    const PmdSpriteConfig* config =
        pmdSpriteConfigForSpecies(visitor.speciesId);
    float floatOffset = pmdFloatYOffset(
        config, Hal::ins().millis(),
        sleeping || visitor.dropOffsetY > 0.0f);
    int y = worldToScreenY(
        visitor.y - visitor.dropOffsetY - floatOffset +
        visitor.renderOffsetY -
        pairConversationHopOffset(false, Hal::ins().millis()));
    uint8_t w = FlashStorage::readByte(&frame->width);
    uint8_t h = FlashStorage::readByte(&frame->height);
    PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2, flipX);
    if (sleeping) {
        drawVisitorSleepZz(x, y - h / 2);
    }
}

void MainScene::drawVisitorSleepZz(int screenX, int spriteTopY) const {
    uint32_t nowMs = Hal::ins().millis();
    for (uint8_t i = 0; i < 2; ++i) {
        uint32_t phase = (nowMs + i * (VISITOR_SLEEP_ZZ_CYCLE_MS / 2)) % VISITOR_SLEEP_ZZ_CYCLE_MS;
        float t = (float)phase / (float)VISITOR_SLEEP_ZZ_CYCLE_MS;
        int zx = screenX + 8 + (int)(t * 4.0f) + (int)i * 3;
        int zy = spriteTopY - 7 - (int)(t * 9.0f) - (int)i * 2;
        uint16_t color = t < 0.34f ? PixelRenderer::rgb(255, 255, 255)
            : (t < 0.67f ? PixelRenderer::rgb(176, 176, 184)
                         : PixelRenderer::rgb(112, 112, 124));
        PixelRenderer::text(zx, zy, "Z", color, 1);
    }
}

void MainScene::render() {
    int16_t depthZ = (int16_t)(monsterY - 78.0f);
    int16_t visitorDepthZ = (int16_t)(visitor.y - 78.0f);
    RenderItem items[] = {
        {0, &MainScene::drawBackground},
        {10, &MainScene::drawFloor},
        {18, &MainScene::drawFood},
        {(int16_t)(20 + depthZ), &MainScene::drawShadow},
        {(int16_t)(20 + visitorDepthZ), &MainScene::drawVisitorShadow},
        {(int16_t)(30 + depthZ), &MainScene::drawMonster},
        {(int16_t)(30 + visitorDepthZ), &MainScene::drawVisitor},
        {(int16_t)(40 + depthZ), &MainScene::drawStateEffect},
        {85, &MainScene::drawNightOverlay},
#if STICKMON_ENABLE_DEBUG_FEATURES
        {88, &MainScene::drawWalkBoundary},
#endif
        {90, &MainScene::drawHud},
        {100, &MainScene::drawToast},
        {105, &MainScene::drawContactDialog},
        {107, &MainScene::drawTutorial},
        {110, &MainScene::drawProgressionPopup},
    };
    sortAndDraw(items, sizeof(items) / sizeof(items[0]));
}

bool MainScene::onButton(const ButtonEvent& event) {
    if (contactDialog != ContactDialog::NONE ||
        contactGuestMotion != ContactGuestMotion::NONE) {
        return handleContactDialogButton(event);
    }
    if (progressionModal != ProgressionModal::NONE) {
        if (event.action != BtnAction::PRESSED) return true;
        if (progressionModal == ProgressionModal::LEVEL_UP) {
            if (event.btn == 0) {
                GameEngine::ins().acknowledgePendingLevelUp();
                progressionModal = ProgressionModal::NONE;
                openPendingProgression();
            }
            return true;
        }
        if (progressionModal == ProgressionModal::EVOLUTION) {
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
                    progressionModal =
                        ProgressionModal::EVOLUTION_CANCELLED;
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
                progressionModal = ProgressionModal::NONE;
                openPendingProgression();
            }
            return true;
        }
        if (progressionModal ==
            ProgressionModal::EVOLUTION_CANCELLED) {
            if (event.btn == 0) {
                if (!ProgressionUi::evolutionCancellationComplete(
                        Hal::ins().millis())) {
                    return true;
                }
                ProgressionUi::resetEvolutionAnimation();
                progressionCancelledSpeciesId = 0;
                progressionModal = ProgressionModal::NONE;
                openPendingProgression();
            }
            return true;
        }
        if (progressionModal == ProgressionModal::MOVE_REPLACED) {
            if (event.btn == 0) {
                GameEngine::ins().acknowledgePendingMoveReplacement();
                progressionModal = ProgressionModal::NONE;
                openPendingProgression();
            }
            return true;
        }
        if (progressionModal == ProgressionModal::LEARN_MOVE &&
            ProgressionUi::handleMoveLearnInput(
                progressionMoveLearn, event.btn)) {
            progressionModal = ProgressionModal::NONE;
            openPendingProgression();
        }
        return true;
    }
    if (doorTransition != DoorTransitionMode::NONE) return true;
    if (event.action != BtnAction::PRESSED && event.action != BtnAction::LONG_PRESS) {
        return false;
    }

    cancelPairInteraction(Hal::ins().millis());
    VoiceCallService::ins().stopListening();

    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        uint32_t nowMs = Hal::ins().millis();
        PetResult result = GameEngine::ins().petMonster();
        startPetReaction(nowMs, result);
        if (result.outcome == PetOutcome::REWARDED) {
            GameEngine::ins().completeTutorial(Game::TutorialStep::ROOM_PET);
        }
        switch (result.outcome) {
        case PetOutcome::REWARDED:
            toast = Ui::Menu::PET_TOAST;
            break;
        case PetOutcome::DAILY_LIMIT:
            toast = Ui::Menu::PET_LIMIT;
            break;
        case PetOutcome::NEEDS_REST:
            toast = Ui::Menu::PET_REST;
            break;
        }
        toastUntil = nowMs + 1200;
        return true;
    }

    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        uint32_t nowMs = Hal::ins().millis();
        cancelRoomAction(nowMs);
        FoodPlacementResult result = GameEngine::ins().placeSelectedFoodInBowl();
        if (result == FoodPlacementResult::ADDED) {
            GameEngine::ins().completeTutorial(Game::TutorialStep::ROOM_FEED);
        }
        switch (result) {
        case FoodPlacementResult::ADDED:
            toast = Ui::Menu::FOOD_ADDED;
            nextMindUpdateMs = nowMs;
            if (aiMode == AiMode::IDLE && (int32_t)(nextAiDecisionMs - (nowMs + 1200)) > 0) {
                nextAiDecisionMs = nowMs + 1200;
            }
            break;
        case FoodPlacementResult::BOWL_FULL:
            toast = Ui::Menu::FOOD_FULL;
            break;
        case FoodPlacementResult::DIFFERENT_FOOD:
            toast = Ui::Menu::FOOD_MIXED;
            break;
        case FoodPlacementResult::NO_STOCK:
        default:
            toast = Ui::Menu::NO_FOOD;
            break;
        }
        toastUntil = nowMs + 1200;
        return true;
    }

    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        cancelRoomAction(Hal::ins().millis());
        GameEngine::ins().completeTutorial(Game::TutorialStep::OPEN_MENU);
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    return false;
}

void MainScene::drawProgressionPopup() {
    if (progressionModal == ProgressionModal::LEVEL_UP) {
        ProgressionUi::renderLevelUp(GameEngine::ins().pendingLevelUpLevel());
    } else if (progressionModal == ProgressionModal::EVOLUTION) {
        ProgressionUi::renderEvolution(
            GameEngine::ins().pendingEvolutionFromSpeciesId(),
            GameEngine::ins().pendingEvolutionToSpeciesId(),
            Hal::ins().millis());
    } else if (progressionModal ==
               ProgressionModal::EVOLUTION_CANCELLED) {
        ProgressionUi::renderEvolutionCancelled(
            progressionCancelledSpeciesId, Hal::ins().millis());
    } else if (progressionModal == ProgressionModal::LEARN_MOVE) {
        ProgressionUi::renderMoveLearn(progressionMoveLearn);
    } else if (progressionModal == ProgressionModal::MOVE_REPLACED) {
        ProgressionUi::renderMoveReplacement();
    }
}

void MainScene::drawBackground() {
    PixelRenderer::clear(PixelRenderer::rgb(5, 6, 18));
    RoomRenderer::draw(cameraY, mainSceneIsNight());
}

void MainScene::drawFloor() {
}

void MainScene::drawFood() {
    if (!GameEngine::ins().bowlHasFood()) return;
    auto& c = PixelRenderer::canvas();
    uint8_t foodIndex = GameEngine::ins().bowlFoodIndex();
    uint8_t maxBites = Game::roomFoodBitesPerServing(foodIndex);
    uint8_t remainingBites = MathUtil::min<uint8_t>(
        maxBites,
        GameEngine::ins().bowlFoodBitesRemaining());
    int cx = (int)foodCenterX();
    int foodY = worldToScreenY(foodCenterY());

    static constexpr int FOOD_LEFT = -9;
    static constexpr int FOOD_WIDTH = 18;
    int visibleWidth = maxBites == 0
        ? 0
        : (FOOD_WIDTH * remainingBites + maxBites - 1) / maxBites;
    if (visibleWidth <= 0) return;
    c.setClipRect(cx + FOOD_LEFT, foodY - 12, visibleWidth, 20);
    if (!GameAssets::drawCentered(
            GameAssets::itemKind(Game::itemIdForFoodIndex(foodIndex)),
            cx, foodY - 3, 0.5f)) {
        c.fillEllipse(cx, foodY, 8, 3, PixelRenderer::rgb(238, 76, 56));
    }
    c.clearClipRect();
}

void MainScene::drawShadow() {
    if (doorMainHidden) return;
    if (pmdAction == PmdAction::SLEEPING) return;
    const PokemonSprites::SpriteFrame* frame = currentMonsterFrame();
    if (!frame) return;

    bool night = mainSceneIsNight();
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    bool floating = config && config->airHeight > 0.0f;
    uint8_t frameW = FlashStorage::readByte(&frame->width);
    uint8_t frameH = FlashStorage::readByte(&frame->height);

    int rx = MathUtil::clamp((int)(frameW * (floating ? 0.25f : 0.44f)), floating ? 10 : 16, floating ? 22 : 40);
    int ry = MathUtil::clamp((int)(frameH * (floating ? 0.055f : 0.14f)), floating ? 3 : 6, floating ? 6 : 14);
    if (night) {
        rx = (rx * 11 + 5) / 10;
        ry = (ry * 11 + 5) / 10;
    }

    int shadowX = (int)monsterX + pmdRenderOffsetX;
    int shadowY = worldToScreenY(
        monsterY + PokemonSprites::frameGroundOffsetY(frame) + pmdRenderOffsetY);
    uint16_t shadowColor = night ? PixelRenderer::rgb(18, 16, 24) : PixelRenderer::rgb(36, 29, 24);
    uint8_t outerAlpha = night ? (floating ? 84 : 122) : (floating ? 68 : 116);
    uint8_t coreAlpha = night ? (floating ? 0 : 92) : (floating ? 0 : 86);
    fillSoftEllipseAlpha(shadowX, shadowY, rx, ry, shadowColor, outerAlpha);
    if (!floating) {
        fillEllipseAlpha(shadowX, shadowY, MathUtil::max(5, rx / 2), MathUtil::max(2, ry / 2),
                         shadowColor, coreAlpha);
    }
}

void MainScene::drawMonster() {
    if (doorMainHidden) return;
    int x = (int)monsterX;
    uint32_t nowMs = Hal::ins().millis();
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    int y = worldToScreenY(monsterY - pmdFloatYOffset(
                               config, nowMs,
                               pmdAction == PmdAction::SLEEPING) -
                           actionRenderYOffset(nowMs) -
                           pairConversationHopOffset(true, nowMs));
    if (active && pmdSpriteConfigForSpecies(active->id) && drawPmdMonster(x, y)) {
        return;
    }

    const PokemonSprites::SpriteFrame* frame = currentMonsterFrame();
    if (frame) {
        uint8_t w = FlashStorage::readByte(&frame->width);
        uint8_t h = FlashStorage::readByte(&frame->height);
        if (PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2, facingRight)) {
            return;
        }
    }
}

bool MainScene::drawPmdMonster(int x, int y) {
    if (!active) return false;
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(active->id, pmdSpriteKind());
    if (!frame) return false;

    x += pmdRenderOffsetX;
    y += pmdRenderOffsetY;
    uint8_t w = FlashStorage::readByte(&frame->width);
    uint8_t h = FlashStorage::readByte(&frame->height);
    if (PokemonSprites::drawFrame(frame, x - w / 2, y - h / 2,
                                  pmdAction == PmdAction::SLEEPING ? false : pmdDirectionFlipX())) {
        return true;
    }
    return false;
}

void MainScene::drawStateEffect() {
    if (doorMainHidden) return;
    uint32_t nowMs = Hal::ins().millis();
    if (heartEffect == HeartEffect::NONE ||
        (int32_t)(nowMs - heartEffectUntilMs) >= 0) return;
    if (!currentMonsterFrame()) return;
    auto& c = PixelRenderer::canvas();
    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    bool pulse = roomAction == RoomAction::ATTENTION_WAIT && ((nowMs / 260UL) & 1U) != 0;
    int baseX = (int)monsterX + 17;
    int baseY = worldToScreenY(monsterY - pmdFloatYOffset(
                                   config, nowMs,
                                   pmdAction == PmdAction::SLEEPING) -
                               actionRenderYOffset(nowMs)) - 30 - (pulse ? 2 : 0);
    uint16_t color = PixelRenderer::rgb(255, 103, 135);
    uint16_t outline = PixelRenderer::rgb(204, 204, 204);
    auto fillHeart = [&](int x, int y, bool small, uint16_t fill) {
        int radius = small ? 2 : 3;
        int spread = small ? 4 : 5;
        int tipY = small ? y + 7 : y + 9;
        c.fillCircle(x, y, radius, fill);
        c.fillCircle(x + spread, y, radius, fill);
        c.fillTriangle(x - radius, y + 2, x + spread + radius, y + 2,
                       x + spread / 2, tipY, fill);
    };
    auto drawHeart = [&](int x, int y, bool small) {
        for (int dy = -2; dy <= 2; ++dy) {
            int horizontal = 2 - abs(dy);
            for (int dx = -horizontal; dx <= horizontal; ++dx) {
                fillHeart(x + dx, y + dy, small, outline);
            }
        }
        fillHeart(x, y, small, color);
    };
    drawHeart(baseX, baseY, false);
    if (heartEffect == HeartEffect::TWO) {
        drawHeart(baseX - 11, baseY + 7, true);
    }
}

void MainScene::drawNightOverlay() {
    bool night = mainSceneIsNight();
    uint8_t lightSource = GameEngine::ins().debugLightSourceIndex();
    if (lightSource == 0) return;

    const PmdSpriteConfig* config = active ? pmdSpriteConfigForSpecies(active->id) : nullptr;
    int followX = (int)monsterX;
    int followY = worldToScreenY(
        monsterY -
        pmdFloatYOffset(config, Hal::ins().millis(),
                        pmdAction == PmdAction::SLEEPING) -
        10.0f);
    int lightX = followX;
    int lightY = followY;
    switch (lightSource) {
    case 1:
        lightX = 44;
        lightY = 30;
        break;
    case 2:
        lightX = Hal::DISPLAY_W / 2;
        lightY = 20;
        break;
    case 3:
        lightX = Hal::DISPLAY_W - 44;
        lightY = 30;
        break;
    case 4:
        lightX = 34;
        lightY = Hal::DISPLAY_H / 2;
        break;
    case 5:
        lightX = Hal::DISPLAY_W - 34;
        lightY = Hal::DISPLAY_H / 2;
        break;
    default:
        break;
    }

    if (night) {
        int coreRadiusX = 58;
        int coreRadiusY = 42;
        int haloRadiusX = 112;
        int haloRadiusY = 76;
        fillRadialLightAlpha(lightX, lightY, coreRadiusX, coreRadiusY,
                             PixelRenderer::rgb(255, 210, 128), 88);
        fillRadialLightAlpha(lightX, lightY, haloRadiusX, haloRadiusY,
                             PixelRenderer::rgb(255, 151, 92), 30);
    } else {
        fillRadialLightAlpha(lightX, lightY, 68, 46,
                             PixelRenderer::rgb(255, 226, 150), 30);
        fillRadialLightAlpha(lightX, lightY, 118, 76,
                             PixelRenderer::rgb(255, 176, 96), 10);
    }

    if (lightSource != 0) {
        auto& c = PixelRenderer::canvas();
        c.fillCircle(lightX, lightY, 3, PixelRenderer::rgb(255, 236, 158));
        c.drawCircle(lightX, lightY, 5, PixelRenderer::rgb(255, 169, 79));
    }
}

#if STICKMON_ENABLE_DEBUG_FEATURES
void MainScene::drawWalkBoundary() {
    if (!GameEngine::ins().debugWalkBoundaryVisible()) return;

    uint8_t count = room().walkPolygonCount();
    if (count < 2) return;

    auto& c = PixelRenderer::canvas();
    uint16_t outline = PixelRenderer::rgb(0, 0, 0);
    uint16_t red = PixelRenderer::rgb(255, 32, 32);

    int16_t firstX = roomPointX(0);
    int16_t firstY = worldToScreenY((float)roomPointY(0));
    int16_t prevX = firstX;
    int16_t prevY = firstY;
    for (uint8_t i = 1; i <= count; ++i) {
        uint8_t index = i == count ? 0 : i;
        int16_t x = roomPointX(index);
        int16_t y = worldToScreenY((float)roomPointY(index));

        c.drawLine(prevX - 1, prevY, x - 1, y, outline);
        c.drawLine(prevX + 1, prevY, x + 1, y, outline);
        c.drawLine(prevX, prevY - 1, x, y - 1, outline);
        c.drawLine(prevX, prevY + 1, x, y + 1, outline);
        c.drawLine(prevX, prevY, x, y, red);

        c.fillCircle(x, y, 2, red);
        prevX = x;
        prevY = y;
    }

    uint16_t routeColor = PixelRenderer::rgb(255, 216, 72);
    int routePrevX = (int)roundf(monsterX);
    int routePrevY = worldToScreenY(monsterY);
    for (uint8_t i = moveRouteIndex; i < moveRouteCount; ++i) {
        int routeX = (int)roundf(moveRouteX[i]);
        int routeY = worldToScreenY(moveRouteY[i]);
        c.drawLine(routePrevX, routePrevY, routeX, routeY, routeColor);
        c.fillCircle(routeX, routeY, 2, routeColor);
        routePrevX = routeX;
        routePrevY = routeY;
    }
    c.drawCircle((int)roundf(targetX), worldToScreenY(targetY), 4,
                 PixelRenderer::rgb(64, 210, 255));
    char debugText[24];
    snprintf(debugText, sizeof(debugText), "AI:%u D:%u R:%u B:%u",
             (unsigned)aiMode, (unsigned)mind.topDesire(), (unsigned)moveRouteCount,
             (unsigned)GameEngine::ins().bowlFoodCount());
    PixelRenderer::text(2, 2, debugText, 0xFFFF, 1);
}
#endif

void MainScene::drawHud() {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 168;
    static constexpr int PANEL_Y = 2;
    static constexpr int PANEL_W = 68;
    static constexpr int PANEL_RADIUS = 5;

    const Game::GameState& state = GameEngine::ins().gameState();
    uint8_t visibleSlots[Game::TEAM_CAP] = {};
    uint8_t visibleCount = Game::HomeHud::visibleTeamSlots(
        state, visibleSlots);
    int panelH = visibleCount > 1 ? 57 : 38;
    fillRoundRectAlpha(PANEL_X, PANEL_Y, PANEL_W, panelH, PANEL_RADIUS,
                       PixelRenderer::rgb(8, 10, 14), 150);
    c.drawRoundRect(PANEL_X, PANEL_Y, PANEL_W, panelH, PANEL_RADIUS,
                    PixelRenderer::rgb(72, 83, 98));

    char clock[8];
    uint16_t gameMinutes = GameEngine::ins().gameMinutesOfDay();
    snprintf(clock, sizeof(clock), "%02u:%02u", gameMinutes / 60, gameMinutes % 60);
    PixelRenderer::text(PANEL_X + 20, PANEL_Y + 3, clock, PixelRenderer::rgb(245, 246, 232));

    uint32_t nowMs = Hal::ins().millis();
    for (uint8_t visibleIndex = 0; visibleIndex < visibleCount; ++visibleIndex) {
        uint8_t slot = visibleSlots[visibleIndex];
        const Game::MonsterRuntime& mon = state.team[slot];
        uint8_t hunger = Game::HomeHud::hungerPercent(mon);
        if (slot == 0 && (int32_t)(nowMs - hungerAnimUntilMs) < 0 &&
            hungerAnimUntilMs > hungerAnimStartedMs) {
            uint32_t elapsed = nowMs - hungerAnimStartedMs;
            uint32_t duration = hungerAnimUntilMs - hungerAnimStartedMs;
            if (elapsed > duration) elapsed = duration;
            int delta = (int)hungerAnimTo - (int)hungerAnimFrom;
            hunger = (uint8_t)MathUtil::clamp(
                (int)hungerAnimFrom +
                    (int)((int64_t)delta * elapsed / duration),
                0, 100);
        }

        uint8_t hpPct = Game::HomeHud::hpPercent(mon);

        int rowY = PANEL_Y + 18 + visibleIndex * 19;
        HudRenderer::drawHungerIcon(
            c, PANEL_X + 6, rowY + 2, hunger);
        int barX = PANEL_X + 29;
        int barW = 31;

        auto drawBar = [&](int y, uint8_t value, uint16_t fillColor) {
            c.fillRect(barX, y, barW, 6, PixelRenderer::rgb(39, 45, 50));
            int fillW = ((barW - 2) * value) / 100;
            if (fillW > 0) c.fillRect(barX + 1, y + 1, fillW, 4, fillColor);
            c.drawRect(barX, y, barW, 6, PixelRenderer::rgb(220, 224, 218));
        };
        uint16_t hpColor = hpPct > 50
            ? PixelRenderer::rgb(92, 222, 112)
            : (hpPct > 20 ? PixelRenderer::rgb(246, 204, 72)
                          : PixelRenderer::rgb(232, 80, 84));
        drawBar(rowY + 6, hpPct, hpColor);
    }
}

void MainScene::drawToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(68, 6, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(76, 8, toast, PixelRenderer::rgb(255, 255, 255));
}

void MainScene::drawTutorial() {
    GameEngine& engine = GameEngine::ins();
    if (!engine.gameState().oobeDone || engine.idleModeActive() ||
        doorTransition != DoorTransitionMode::NONE ||
        progressionModal != ProgressionModal::NONE ||
        contactDialog != ContactDialog::NONE ||
        contactGuestMotion != ContactGuestMotion::NONE ||
        roomAction != RoomAction::NONE) {
        return;
    }

    if (!engine.tutorialComplete(Game::TutorialStep::ROOM_FEED)) {
        TutorialOverlay::draw(
            TutorialOverlay::Button::B, Ui::Tutorial::ROOM_FEED);
        return;
    }
    if (!engine.tutorialComplete(Game::TutorialStep::ROOM_PET)) {
        char line[48];
        snprintf(line, sizeof(line), Ui::Tutorial::ROOM_PET_FMT,
                 active ? active->name : "");
        TutorialOverlay::draw(TutorialOverlay::Button::A, line);
        return;
    }
    if (!engine.tutorialComplete(Game::TutorialStep::OPEN_MENU)) {
        TutorialOverlay::draw(
            TutorialOverlay::Button::A, Ui::Tutorial::OPEN_MENU, true);
    }
}

void MainScene::sortAndDraw(RenderItem* items, uint8_t count) {
    std::sort(items, items + count, [](const RenderItem& a, const RenderItem& b) {
        return a.z < b.z;
    });
    for (uint8_t i = 0; i < count; ++i) {
        (this->*items[i].draw)();
    }
}
