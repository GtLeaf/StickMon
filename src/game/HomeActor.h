#pragma once

#include <cstdint>

#include "core/RoomMovementArea.h"
#include "game/MonsterMind.h"

namespace Home {

static constexpr uint8_t ACTOR_CAP = 2;
static constexpr uint8_t ROUTE_CAP = 20;

enum class ActorRole : uint8_t {
    LEADER,
    TEAMMATE,
    GUEST,
};

// A task describes the actor's gameplay intent. Turning and sprite playback
// are presentation details and deliberately do not belong in this enum.
enum class Task : uint8_t {
    IDLE,
    WANDER,
    WALK = WANDER,
    TURNING,
    SEEK_FOOD,
    FEEDING,
    SEEK_SLEEP,
    SEEK_BED = SEEK_SLEEP,
    GO_TO_SLEEP = SEEK_SLEEP,
    SLEEPING,
    RESTING = SLEEPING,
    WAKING,
    LEAVING_SLEEP,
    LEAVING_BED = LEAVING_SLEEP,
    YIELDING,
    YIELDING_BED = YIELDING,
    ROOM_ACTION,
    SCRIPTED_MOVE = ROOM_ACTION,
    PAIR_ACTION,
    DOOR_ACTION,
    FAINTED,
};

enum class MotionPhase : uint8_t {
    STILL,
    TURNING,
    ROUTE,
};

enum class Resource : uint8_t {
    BOWL,
    BED,
    DOOR,
    COUNT,
};

struct Geometry {
    float groundOffsetY = 18.0f;
    RoomMovementArea::Footprint footprint = {9.0f, 5.0f};
};

struct Route {
    float x[ROUTE_CAP] = {};
    float y[ROUTE_CAP] = {};
    uint8_t count = 0;
    uint8_t index = 0;

    void clear();
    bool current(float& outX, float& outY) const;
    bool advance();
    bool empty() const { return index >= count; }
};

struct Actor {
    bool active = false;
    ActorRole role = ActorRole::LEADER;
    uint8_t teamSlot = 0;
    uint16_t speciesId = 0;

    float x = 98.0f;
    float y = 91.0f;
    float targetX = 98.0f;
    float targetY = 91.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float sleepX = 0.0f;
    float sleepY = 0.0f;
    Geometry geometry;
    float dropOffsetY = 0.0f;
    bool sleepSpotValid = false;
    bool hidden = false;

    Task task = Task::IDLE;
    Task resumeTask = Task::IDLE;
    MotionPhase motion = MotionPhase::STILL;
    uint32_t taskUntilMs = 0;
    uint32_t nextDecisionMs = 0;
    uint32_t nextMindUpdateMs = 0;
    uint32_t postFeedAwakeUntilMs = 0;
    uint32_t foodWakeRetryAfterMs = 0;
    uint32_t blockedSinceMs = 0;
    uint32_t nextReplanMs = 0;
    float lastWaypointDistance = 0.0f;
    uint32_t lastMoveProgressMs = 0;
    uint8_t stuckRecoveryCount = 0;
    bool wakingForFood = false;
    bool faintRestActive = false;

    Route route;
    MonsterMind mind;
    MonsterBehaviorProfile behavior;

    void reset(ActorRole nextRole, uint8_t nextTeamSlot, uint32_t nowMs);
    void beginTask(Task nextTask, uint32_t nowMs,
                   uint32_t durationMs = 0);
    void stop(uint32_t nowMs, uint32_t idleDelayMs = 0);
};

uint8_t taskPriority(Task task);
bool taskUsesRoute(Task task);
bool taskUsesResource(Task task, Resource resource);
const char* taskName(Task task);

}  // namespace Home
