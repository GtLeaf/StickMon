#include "game/HomeCoordinator.h"

namespace Home {

void Coordinator::attach(Actor& first, Actor* second) {
    actors_[0] = &first;
    actors_[1] = second;
    actorCount_ = second ? ACTOR_CAP : 1;
}

void Coordinator::reset(uint32_t nowMs) {
    (void)nowMs;
    for (Lease& lease : leases_) lease = Lease{};
    for (auto& actorFailures : routeFailures_) {
        for (RouteFailure& failure : actorFailures) {
            failure = RouteFailure{};
        }
    }
    pairActivity_ = PairActivity::NONE;
    pairStartedAtMs_ = 0;
    ++worldRevision_;
    if (worldRevision_ == 0) worldRevision_ = 1;
}

bool Coordinator::validActor(uint8_t actorId) const {
    return actorId < actorCount_ && actors_[actorId] &&
           actors_[actorId]->active;
}

bool Coordinator::transition(uint8_t actorId, Task task, uint32_t nowMs,
                             uint32_t durationMs, bool force) {
    if (!validActor(actorId)) return false;
    Actor& actor = *actors_[actorId];
    if (!force && pairActive() && task != Task::PAIR_ACTION &&
        taskPriority(task) < taskPriority(actor.task)) {
        return false;
    }
    if (!force && taskPriority(task) < taskPriority(actor.task) &&
        actor.task != Task::IDLE && actor.task != Task::WANDER) {
        return false;
    }

    for (uint8_t value = 0;
         value < static_cast<uint8_t>(Resource::COUNT); ++value) {
        Resource resource = static_cast<Resource>(value);
        if (!taskUsesResource(task, resource) &&
            !(actor.task == Task::TURNING &&
              taskUsesResource(actor.resumeTask, resource))) {
            release(resource, actorId);
        }
    }
    if (!taskUsesRoute(task)) actor.route.clear();
    actor.beginTask(task, nowMs, durationMs);
    return true;
}

void Coordinator::stop(uint8_t actorId, uint32_t nowMs,
                       uint32_t idleDelayMs) {
    if (!validActor(actorId)) return;
    releaseAll(actorId);
    actors_[actorId]->stop(nowMs, idleDelayMs);
    notify(WorldEvent::TASK_COMPLETED);
}

bool Coordinator::acquire(Resource resource, uint8_t actorId, Task forTask,
                          uint32_t nowMs) {
    if (pairActive() || !validActor(actorId) ||
        !taskUsesResource(forTask, resource)) {
        return false;
    }
    Lease& lease = leases_[static_cast<uint8_t>(resource)];
    if (lease.owner == actorId) {
        lease.task = forTask;
        return true;
    }
    if (lease.owner != NO_ACTOR) return false;
    lease.owner = static_cast<int8_t>(actorId);
    lease.task = forTask;
    lease.acquiredAtMs = nowMs;
    return true;
}

void Coordinator::release(Resource resource, uint8_t actorId) {
    Lease& lease = leases_[static_cast<uint8_t>(resource)];
    if (lease.owner != static_cast<int8_t>(actorId)) return;
    lease = Lease{};
}

void Coordinator::releaseAll(uint8_t actorId) {
    for (uint8_t value = 0;
         value < static_cast<uint8_t>(Resource::COUNT); ++value) {
        release(static_cast<Resource>(value), actorId);
    }
}

int8_t Coordinator::owner(Resource resource) const {
    return leases_[static_cast<uint8_t>(resource)].owner;
}

bool Coordinator::beginPair(PairActivity activity, uint32_t nowMs) {
    if (activity == PairActivity::NONE || pairActive() ||
        !validActor(0) || !validActor(1)) {
        return false;
    }
    if (taskPriority(actors_[0]->task) > taskPriority(Task::PAIR_ACTION) ||
        taskPriority(actors_[1]->task) > taskPriority(Task::PAIR_ACTION)) {
        return false;
    }
    releaseAll(0);
    releaseAll(1);
    actors_[0]->beginTask(Task::PAIR_ACTION, nowMs);
    actors_[1]->beginTask(Task::PAIR_ACTION, nowMs);
    pairActivity_ = activity;
    pairStartedAtMs_ = nowMs;
    return true;
}

void Coordinator::endPair(uint32_t nowMs, uint32_t idleDelayMs) {
    if (!pairActive()) return;
    pairActivity_ = PairActivity::NONE;
    pairStartedAtMs_ = 0;
    for (uint8_t actorId = 0; actorId < actorCount_; ++actorId) {
        if (validActor(actorId) &&
            actors_[actorId]->task == Task::PAIR_ACTION) {
            actors_[actorId]->stop(nowMs, idleDelayMs);
        }
    }
    notify(WorldEvent::TASK_COMPLETED);
}

uint32_t Coordinator::notify(WorldEvent event) {
    (void)event;
    ++worldRevision_;
    if (worldRevision_ == 0) worldRevision_ = 1;
    return worldRevision_;
}

bool Coordinator::routeRetryAllowed(uint8_t actorId, Resource resource,
                                    uint32_t signature) const {
    if (actorId >= ACTOR_CAP) return false;
    const RouteFailure& failure =
        routeFailures_[actorId][static_cast<uint8_t>(resource)];
    return !failure.valid || failure.signature != signature ||
           failure.worldRevision != worldRevision_;
}

void Coordinator::rememberRouteFailure(uint8_t actorId, Resource resource,
                                       uint32_t signature) {
    if (actorId >= ACTOR_CAP) return;
    RouteFailure& failure =
        routeFailures_[actorId][static_cast<uint8_t>(resource)];
    failure.valid = true;
    failure.signature = signature;
    failure.worldRevision = worldRevision_;
}

void Coordinator::clearRouteFailure(uint8_t actorId, Resource resource) {
    if (actorId >= ACTOR_CAP) return;
    routeFailures_[actorId][static_cast<uint8_t>(resource)] = RouteFailure{};
}

uint16_t Coordinator::validate() const {
    uint16_t errors = INVARIANT_OK;
    for (uint8_t value = 0;
         value < static_cast<uint8_t>(Resource::COUNT); ++value) {
        const Lease& lease = leases_[value];
        if (lease.owner == NO_ACTOR) continue;
        if (lease.owner < 0 || !validActor(static_cast<uint8_t>(lease.owner))) {
            errors |= INVALID_LEASE_OWNER;
            continue;
        }
        const Actor& actor = *actors_[lease.owner];
        bool usesResource = taskUsesResource(
            actor.task, static_cast<Resource>(value));
        if (!usesResource && actor.task == Task::TURNING) {
            usesResource = taskUsesResource(
                actor.resumeTask, static_cast<Resource>(value));
        }
        if (!usesResource) {
            errors |= STALE_RESOURCE_LEASE;
        }
    }
    if (pairActive() &&
        (!validActor(0) || !validActor(1) ||
         actors_[0]->task != Task::PAIR_ACTION ||
         actors_[1]->task != Task::PAIR_ACTION)) {
        errors |= INCOMPLETE_PAIR_LOCK;
    }
    if (actors_[0]) {
        for (uint8_t actorId = 0; actorId < actorCount_; ++actorId) {
            const Actor& actor = *actors_[actorId];
            if (!actor.active) continue;
            if (actor.task == Task::IDLE && !actor.route.empty()) {
                errors |= IDLE_ACTOR_HAS_ROUTE;
            }
            if (actor.hidden &&
                (actor.velocityX != 0.0f || actor.velocityY != 0.0f)) {
                errors |= HIDDEN_ACTOR_HAS_VELOCITY;
            }
        }
    }
    return errors;
}

uint16_t Coordinator::repair(uint32_t nowMs) {
    uint16_t errors = validate();
    if (errors == INVARIANT_OK) return errors;
    for (uint8_t value = 0;
         value < static_cast<uint8_t>(Resource::COUNT); ++value) {
        Lease& lease = leases_[value];
        if (lease.owner == NO_ACTOR) continue;
        if (lease.owner < 0 || !validActor(static_cast<uint8_t>(lease.owner)) ||
            !taskUsesResource(
                actors_[lease.owner]->task, static_cast<Resource>(value)) &&
            !(actors_[lease.owner]->task == Task::TURNING &&
              taskUsesResource(actors_[lease.owner]->resumeTask,
                               static_cast<Resource>(value)))) {
            lease = Lease{};
        }
    }
    if ((errors & INCOMPLETE_PAIR_LOCK) != 0) endPair(nowMs);
    if (actors_[0]) {
        for (uint8_t actorId = 0; actorId < actorCount_; ++actorId) {
            Actor& actor = *actors_[actorId];
            if (!actor.active) continue;
            if (actor.task == Task::IDLE) actor.route.clear();
            if (actor.hidden) {
                actor.velocityX = 0.0f;
                actor.velocityY = 0.0f;
            }
        }
    }
    return errors;
}

}  // namespace Home
