#pragma once

#include <cstdint>

#include "game/HomeRuntime.h"

namespace Home {

enum class ControlOwner : uint8_t {
    AUTONOMOUS,
    CLAW,
    SCRIPTED,
    FROZEN,
};

enum class BowlPhase : uint8_t {
    IDLE,
    RESERVED,
    APPROACHING,
    FEEDING,
    CLEARING,
};

struct BowlSession {
    BowlPhase phase = BowlPhase::IDLE;
    int8_t actorId = Coordinator::NO_ACTOR;
    uint32_t phaseStartedMs = 0;

    bool active() const { return phase != BowlPhase::IDLE; }
    bool clearing() const { return phase == BowlPhase::CLEARING; }
};

struct ActorRenderSnapshot {
    bool active = false;
    bool hidden = false;
    uint8_t teamSlot = 0;
    uint16_t speciesId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    Task task = Task::IDLE;
};

struct RenderSnapshot {
    ActorRenderSnapshot actors[ACTOR_CAP];
    uint8_t actorCount = 0;
    BowlSession bowl;
    PairActivity pairActivity = PairActivity::NONE;
};

// Authoritative room-simulation facade. Scene adapters still own visual
// animation state, but all gameplay tasks, shared resources and navigation
// pass through this object.
class Simulation {
public:
    void attach(Actor& first, Actor* second = nullptr);
    void reset(uint32_t nowMs);
    uint16_t beginTick(uint32_t nowMs);
    void setNavigation(const NavigationWorld& world) {
        runtime_.setNavigation(world);
    }

    Actor* actor(uint8_t actorId) { return runtime_.actor(actorId); }
    const Actor* actor(uint8_t actorId) const {
        return runtime_.actor(actorId);
    }

    bool planRoute(uint8_t actorId, float goalX, float goalY,
                   bool allowOutsideStart = false,
                   bool avoidOther = true) {
        return runtime_.planRoute(actorId, goalX, goalY,
                                  allowOutsideStart, avoidOther);
    }
    RouteStep advanceRoute(uint8_t actorId, uint32_t nowMs, float speed,
                           float dtSeconds,
                           float arrivalDistance = 1.0f,
                           bool avoidOther = true) {
        return runtime_.advanceRoute(actorId, nowMs, speed, dtSeconds,
                                     arrivalDistance, avoidOther);
    }
    bool overlapsOther(uint8_t actorId, float x, float y) const {
        return runtime_.overlapsOther(actorId, x, y);
    }

    bool transition(uint8_t actorId, Task task, uint32_t nowMs,
                    uint32_t durationMs = 0, bool force = false);
    bool transitionPreparedRoute(uint8_t actorId, Task task,
                                 uint32_t nowMs,
                                 uint32_t durationMs = 0,
                                 bool force = false);
    bool beginTurn(uint8_t actorId, Task resumeTask, uint32_t nowMs,
                   uint32_t durationMs, bool force = false);
    bool finishTurn(uint8_t actorId, uint32_t nowMs,
                    bool force = false);
    void stop(uint8_t actorId, uint32_t nowMs,
              uint32_t idleDelayMs = 0);
    bool acquire(Resource resource, uint8_t actorId, Task forTask,
                 uint32_t nowMs);
    void release(Resource resource, uint8_t actorId);
    void releaseAll(uint8_t actorId);
    int8_t owner(Resource resource) const {
        return runtime_.owner(resource);
    }

    bool beginBowlClearance(uint8_t actorId, uint32_t nowMs);
    void finishBowlClearance(uint8_t actorId, uint32_t nowMs);
    bool bowlClaimAllowed(uint8_t actorId) const;
    const BowlSession& bowlSession() const { return bowl_; }

    bool beginPair(PairActivity activity, uint32_t nowMs) {
        return runtime_.beginPair(activity, nowMs);
    }
    void endPair(uint32_t nowMs, uint32_t idleDelayMs = 0) {
        runtime_.endPair(nowMs, idleDelayMs);
    }
    bool pairActive() const { return runtime_.pairActive(); }
    PairActivity pairActivity() const {
        return runtime_.pairActivity();
    }

    uint32_t notify(WorldEvent event) { return runtime_.notify(event); }
    uint16_t validate() const { return runtime_.validate(); }
    uint16_t repair(uint32_t nowMs);

    void setControlOwner(uint8_t actorId, ControlOwner owner);
    ControlOwner controlOwner(uint8_t actorId) const;
    bool autonomousAllowed(uint8_t actorId) const {
        return controlOwner(actorId) == ControlOwner::AUTONOMOUS;
    }

    RenderSnapshot renderSnapshot() const;

private:
    void reconcileBowl(uint32_t nowMs);
    void resetBowl(uint32_t nowMs);
    void setBowlPhase(BowlPhase phase, int8_t actorId,
                      uint32_t nowMs);

    Runtime runtime_;
    Actor* actors_[ACTOR_CAP] = {};
    uint8_t actorCount_ = 0;
    ControlOwner controlOwners_[ACTOR_CAP] = {
        ControlOwner::AUTONOMOUS,
        ControlOwner::AUTONOMOUS,
    };
    BowlSession bowl_;
};

}  // namespace Home
