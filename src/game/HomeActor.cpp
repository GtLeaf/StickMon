#include "game/HomeActor.h"

namespace Home {

void Route::clear() {
    count = 0;
    index = 0;
}

bool Route::current(float& outX, float& outY) const {
    if (index >= count) return false;
    outX = x[index];
    outY = y[index];
    return true;
}

bool Route::advance() {
    if (index < count) ++index;
    return index < count;
}

void Actor::reset(ActorRole nextRole, uint8_t nextTeamSlot,
                  uint32_t nowMs) {
    *this = Actor{};
    role = nextRole;
    teamSlot = nextTeamSlot;
    mind.reset(nowMs);
    nextMindUpdateMs = nowMs;
    nextDecisionMs = nowMs;
}

void Actor::beginTask(Task nextTask, uint32_t nowMs,
                      uint32_t durationMs) {
    task = nextTask;
    resumeTask = nextTask;
    motion = taskUsesRoute(nextTask)
        ? MotionPhase::ROUTE : MotionPhase::STILL;
    taskUntilMs = durationMs == 0 ? 0 : nowMs + durationMs;
    blockedSinceMs = 0;
    nextReplanMs = nowMs;
}

void Actor::stop(uint32_t nowMs, uint32_t idleDelayMs) {
    route.clear();
    velocityX = 0.0f;
    velocityY = 0.0f;
    targetX = x;
    targetY = y;
    task = Task::IDLE;
    resumeTask = Task::IDLE;
    motion = MotionPhase::STILL;
    taskUntilMs = 0;
    blockedSinceMs = 0;
    nextReplanMs = nowMs;
    nextDecisionMs = nowMs + idleDelayMs;
}

uint8_t taskPriority(Task task) {
    switch (task) {
    case Task::FAINTED: return 100;
    case Task::DOOR_ACTION: return 90;
    case Task::SEEK_SLEEP:
    case Task::SLEEPING:
    case Task::WAKING:
    case Task::LEAVING_SLEEP: return 80;
    case Task::SEEK_FOOD:
    case Task::FEEDING: return 70;
    case Task::ROOM_ACTION: return 60;
    case Task::TURNING: return 55;
    case Task::YIELDING: return 50;
    case Task::PAIR_ACTION: return 40;
    case Task::WANDER: return 20;
    case Task::IDLE: return 0;
    }
    return 0;
}

bool taskUsesRoute(Task task) {
    return task == Task::WANDER || task == Task::SEEK_FOOD ||
           task == Task::SEEK_SLEEP || task == Task::LEAVING_SLEEP ||
           task == Task::YIELDING ||
           task == Task::ROOM_ACTION || task == Task::PAIR_ACTION ||
           task == Task::DOOR_ACTION;
}

bool taskUsesResource(Task task, Resource resource) {
    switch (resource) {
    case Resource::BOWL:
        return task == Task::SEEK_FOOD || task == Task::FEEDING;
    case Resource::BED:
        return task == Task::SEEK_SLEEP || task == Task::SLEEPING ||
               task == Task::WAKING;
    case Resource::DOOR:
        return task == Task::DOOR_ACTION;
    case Resource::COUNT:
        return false;
    }
    return false;
}

const char* taskName(Task task) {
    switch (task) {
    case Task::IDLE: return "idle";
    case Task::WANDER: return "wander";
    case Task::TURNING: return "turning";
    case Task::SEEK_FOOD: return "seek_food";
    case Task::FEEDING: return "feeding";
    case Task::SEEK_SLEEP: return "seek_sleep";
    case Task::SLEEPING: return "sleeping";
    case Task::WAKING: return "waking";
    case Task::LEAVING_SLEEP: return "leaving_sleep";
    case Task::YIELDING: return "yielding";
    case Task::ROOM_ACTION: return "room_action";
    case Task::PAIR_ACTION: return "pair_action";
    case Task::DOOR_ACTION: return "door_action";
    case Task::FAINTED: return "fainted";
    }
    return "unknown";
}

}  // namespace Home
