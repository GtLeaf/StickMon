#pragma once

#include <cstdint>

#include "core/RoomNavigator.h"
#include "game/HomeCoordinator.h"

namespace Home {

// Actor-to-actor navigation can either treat the other actor as a hard
// obstacle (the legacy Stick behavior) or let routes cross and steer only
// when the rendered footprints would overlap.
enum class ActorCollisionMode : uint8_t {
    HARD,
    SOFT,
};

enum class RouteStep : uint8_t {
    NO_ROUTE,
    MOVING,
    ARRIVED,
    BLOCKED,
};

struct NavigationWorld {
    const RoomResource::Point* polygon = nullptr;
    uint8_t polygonCount = 0;
    RoomNavigator::Bounds footBounds = {};
    float actorMinSeparation = 0.0f;
    ActorCollisionMode actorCollisionMode = ActorCollisionMode::HARD;
    float actorOverlapPadding = 1.0f;
    RoomNavigator::Scratch scratch = {};
};

// Shared room-motion runtime. Gameplay scenes own the actors and presentation
// state; this class owns coordination, route planning and collision-safe steps.
class Runtime {
public:
    void attach(Actor& first, Actor* second = nullptr);
    void reset(uint32_t nowMs);
    void setNavigation(const NavigationWorld& world) { world_ = world; }

    Actor* actor(uint8_t actorId);
    const Actor* actor(uint8_t actorId) const;

    bool planRoute(uint8_t actorId, float goalX, float goalY,
                   bool allowOutsideStart = false,
                   bool avoidOther = true);
    RouteStep advanceRoute(uint8_t actorId, uint32_t nowMs, float speed,
                           float dtSeconds, float arrivalDistance = 1.0f,
                           bool avoidOther = true);

    // Used by target selection and diagnostics. This is intentionally a
    // visual-overlap test, not a physical collision test.
    bool overlapsOther(uint8_t actorId, float x, float y) const;

    bool transition(uint8_t actorId, Task task, uint32_t nowMs,
                    uint32_t durationMs = 0, bool force = false) {
        return coordinator_.transition(
            actorId, task, nowMs, durationMs, force);
    }
    bool transitionPreparedRoute(uint8_t actorId, Task task,
                                 uint32_t nowMs,
                                 uint32_t durationMs = 0,
                                 bool force = false) {
        return coordinator_.transitionPreparedRoute(
            actorId, task, nowMs, durationMs, force);
    }
    bool beginTurn(uint8_t actorId, Task resumeTask, uint32_t nowMs,
                   uint32_t durationMs, bool force = false) {
        return coordinator_.beginTurn(
            actorId, resumeTask, nowMs, durationMs, force);
    }
    bool finishTurn(uint8_t actorId, uint32_t nowMs,
                    bool force = false) {
        return coordinator_.finishTurn(actorId, nowMs, force);
    }
    void stop(uint8_t actorId, uint32_t nowMs,
              uint32_t idleDelayMs = 0) {
        coordinator_.stop(actorId, nowMs, idleDelayMs);
    }
    bool acquire(Resource resource, uint8_t actorId, Task forTask,
                 uint32_t nowMs) {
        return coordinator_.acquire(resource, actorId, forTask, nowMs);
    }
    void release(Resource resource, uint8_t actorId) {
        coordinator_.release(resource, actorId);
    }
    void releaseAll(uint8_t actorId) {
        coordinator_.releaseAll(actorId);
    }
    int8_t owner(Resource resource) const {
        return coordinator_.owner(resource);
    }
    bool beginPair(PairActivity activity, uint32_t nowMs) {
        return coordinator_.beginPair(activity, nowMs);
    }
    void endPair(uint32_t nowMs, uint32_t idleDelayMs = 0) {
        coordinator_.endPair(nowMs, idleDelayMs);
    }
    bool pairActive() const { return coordinator_.pairActive(); }
    PairActivity pairActivity() const {
        return coordinator_.pairActivity();
    }
    uint32_t notify(WorldEvent event) {
        return coordinator_.notify(event);
    }
    uint16_t validate() const { return coordinator_.validate(); }
    uint16_t repair(uint32_t nowMs) {
        return coordinator_.repair(nowMs);
    }

private:
    RoomNavigator::Obstacle obstacleFor(uint8_t actorId,
                                        bool avoidOther) const;
    bool pointAllowed(const Actor& actor, float x, float y) const;
    const Actor* otherActor(uint8_t actorId) const;
    bool segmentOverlapsOther(uint8_t actorId, float fromX, float fromY,
                              float toX, float toY) const;
    bool shouldYieldToOther(uint8_t actorId) const;
    bool chooseSoftStep(uint8_t actorId, float waypointX, float waypointY,
                        float proposedX, float proposedY, float step,
                        bool avoidOther, bool maintainClearance,
                        float& outX, float& outY) const;

    Actor* actors_[ACTOR_CAP] = {};
    uint8_t actorCount_ = 0;
    NavigationWorld world_;
    Coordinator coordinator_;
};

}  // namespace Home
