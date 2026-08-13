#pragma once

#include <cstdint>

#include "game/HomeActor.h"

namespace Home {

enum class PairActivity : uint8_t {
    NONE,
    TALK,
    CHASE,
};

enum class WorldEvent : uint8_t {
    ACTOR_MOVED,
    BOWL_CHANGED,
    TIME_THRESHOLD,
    ROOM_CHANGED,
    TASK_COMPLETED,
};

enum InvariantError : uint16_t {
    INVARIANT_OK = 0,
    INVALID_LEASE_OWNER = 1U << 0,
    STALE_RESOURCE_LEASE = 1U << 1,
    INCOMPLETE_PAIR_LOCK = 1U << 2,
    IDLE_ACTOR_HAS_ROUTE = 1U << 3,
    HIDDEN_ACTOR_HAS_VELOCITY = 1U << 4,
};

class Coordinator {
public:
    static constexpr int8_t NO_ACTOR = -1;

    void attach(Actor& first, Actor* second = nullptr);
    void reset(uint32_t nowMs);

    bool transition(uint8_t actorId, Task task, uint32_t nowMs,
                    uint32_t durationMs = 0, bool force = false);
    void stop(uint8_t actorId, uint32_t nowMs,
              uint32_t idleDelayMs = 0);

    bool acquire(Resource resource, uint8_t actorId, Task forTask,
                 uint32_t nowMs);
    void release(Resource resource, uint8_t actorId);
    void releaseAll(uint8_t actorId);
    int8_t owner(Resource resource) const;

    bool beginPair(PairActivity activity, uint32_t nowMs);
    void endPair(uint32_t nowMs, uint32_t idleDelayMs = 0);
    PairActivity pairActivity() const { return pairActivity_; }
    bool pairActive() const { return pairActivity_ != PairActivity::NONE; }

    uint32_t notify(WorldEvent event);
    uint32_t worldRevision() const { return worldRevision_; }
    bool routeRetryAllowed(uint8_t actorId, Resource resource,
                           uint32_t signature) const;
    void rememberRouteFailure(uint8_t actorId, Resource resource,
                              uint32_t signature);
    void clearRouteFailure(uint8_t actorId, Resource resource);

    uint16_t validate() const;
    uint16_t repair(uint32_t nowMs);

private:
    struct Lease {
        int8_t owner = NO_ACTOR;
        Task task = Task::IDLE;
        uint32_t acquiredAtMs = 0;
    };

    struct RouteFailure {
        bool valid = false;
        uint32_t signature = 0;
        uint32_t worldRevision = 0;
    };

    bool validActor(uint8_t actorId) const;

    Actor* actors_[ACTOR_CAP] = {};
    uint8_t actorCount_ = 0;
    Lease leases_[static_cast<uint8_t>(Resource::COUNT)];
    RouteFailure routeFailures_[ACTOR_CAP]
                               [static_cast<uint8_t>(Resource::COUNT)];
    PairActivity pairActivity_ = PairActivity::NONE;
    uint32_t pairStartedAtMs_ = 0;
    uint32_t worldRevision_ = 1;
};

}  // namespace Home
