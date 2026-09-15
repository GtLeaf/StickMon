#include "game/HomeSimulation.h"

namespace Home {

void Simulation::attach(Actor& first, Actor* second) {
    actors_[0] = &first;
    actors_[1] = second;
    actorCount_ = second ? ACTOR_CAP : 1;
    runtime_.attach(first, second);
    reconcileBowl(bowl_.phaseStartedMs);
}

void Simulation::reset(uint32_t nowMs) {
    runtime_.reset(nowMs);
    resetBowl(nowMs);
    for (ControlOwner& owner : controlOwners_) {
        owner = ControlOwner::AUTONOMOUS;
    }
}

uint16_t Simulation::beginTick(uint32_t nowMs) {
    const uint16_t repaired = runtime_.repair(nowMs);
    reconcileBowl(nowMs);
    return repaired;
}

bool Simulation::transition(uint8_t actorId, Task task, uint32_t nowMs,
                            uint32_t durationMs, bool force) {
    if (!runtime_.transition(actorId, task, nowMs, durationMs, force)) {
        return false;
    }
    if (bowl_.actorId == static_cast<int8_t>(actorId)) {
        if (task == Task::SEEK_FOOD) {
            setBowlPhase(BowlPhase::APPROACHING, bowl_.actorId, nowMs);
        } else if (task == Task::FEEDING) {
            setBowlPhase(BowlPhase::FEEDING, bowl_.actorId, nowMs);
        } else if (bowl_.phase != BowlPhase::CLEARING) {
            resetBowl(nowMs);
        }
    }
    return true;
}

bool Simulation::transitionPreparedRoute(uint8_t actorId, Task task,
                                         uint32_t nowMs,
                                         uint32_t durationMs,
                                         bool force) {
    if (!runtime_.transitionPreparedRoute(
            actorId, task, nowMs, durationMs, force)) {
        return false;
    }
    if (bowl_.actorId == static_cast<int8_t>(actorId)) {
        if (task == Task::SEEK_FOOD) {
            setBowlPhase(BowlPhase::APPROACHING, bowl_.actorId, nowMs);
        } else if (bowl_.phase != BowlPhase::CLEARING) {
            resetBowl(nowMs);
        }
    }
    return true;
}

bool Simulation::beginTurn(uint8_t actorId, Task resumeTask,
                           uint32_t nowMs, uint32_t durationMs,
                           bool force) {
    if (!runtime_.beginTurn(
            actorId, resumeTask, nowMs, durationMs, force)) {
        return false;
    }
    if (bowl_.actorId == static_cast<int8_t>(actorId) &&
        resumeTask == Task::SEEK_FOOD) {
        setBowlPhase(BowlPhase::APPROACHING, bowl_.actorId, nowMs);
    }
    return true;
}

bool Simulation::finishTurn(uint8_t actorId, uint32_t nowMs,
                            bool force) {
    if (!runtime_.finishTurn(actorId, nowMs, force)) return false;
    reconcileBowl(nowMs);
    return true;
}

void Simulation::stop(uint8_t actorId, uint32_t nowMs,
                      uint32_t idleDelayMs) {
    const bool preserveClearance = bowl_.clearing() &&
        bowl_.actorId == static_cast<int8_t>(actorId);
    runtime_.stop(actorId, nowMs, idleDelayMs);
    if (!preserveClearance &&
        bowl_.actorId == static_cast<int8_t>(actorId)) {
        resetBowl(nowMs);
    }
}

bool Simulation::acquire(Resource resource, uint8_t actorId,
                         Task forTask, uint32_t nowMs) {
    if (resource == Resource::BOWL && !bowlClaimAllowed(actorId)) {
        return false;
    }
    if (!runtime_.acquire(resource, actorId, forTask, nowMs)) {
        return false;
    }
    if (resource == Resource::BOWL) {
        setBowlPhase(BowlPhase::RESERVED,
                     static_cast<int8_t>(actorId), nowMs);
    }
    return true;
}

void Simulation::release(Resource resource, uint8_t actorId) {
    runtime_.release(resource, actorId);
    if (resource == Resource::BOWL && !bowl_.clearing() &&
        bowl_.actorId == static_cast<int8_t>(actorId)) {
        resetBowl(bowl_.phaseStartedMs);
    }
}

void Simulation::releaseAll(uint8_t actorId) {
    const bool preserveClearance = bowl_.clearing() &&
        bowl_.actorId == static_cast<int8_t>(actorId);
    runtime_.releaseAll(actorId);
    if (!preserveClearance &&
        bowl_.actorId == static_cast<int8_t>(actorId)) {
        resetBowl(bowl_.phaseStartedMs);
    }
}

bool Simulation::beginBowlClearance(uint8_t actorId, uint32_t nowMs) {
    const int8_t id = static_cast<int8_t>(actorId);
    if (actorId >= actorCount_ || !actors_[actorId] ||
        !actors_[actorId]->active) {
        return false;
    }
    const int8_t leaseOwner = runtime_.owner(Resource::BOWL);
    if (bowl_.actorId != id && leaseOwner != id) return false;
    runtime_.release(Resource::BOWL, actorId);
    setBowlPhase(BowlPhase::CLEARING, id, nowMs);
    return true;
}

void Simulation::finishBowlClearance(uint8_t actorId, uint32_t nowMs) {
    if (bowl_.clearing() &&
        bowl_.actorId == static_cast<int8_t>(actorId)) {
        resetBowl(nowMs);
        runtime_.notify(WorldEvent::ACTOR_MOVED);
    }
}

bool Simulation::bowlClaimAllowed(uint8_t actorId) const {
    if (actorId >= actorCount_) return false;
    if (!bowl_.active()) return true;
    return !bowl_.clearing() &&
           bowl_.actorId == static_cast<int8_t>(actorId);
}

uint16_t Simulation::repair(uint32_t nowMs) {
    const uint16_t repaired = runtime_.repair(nowMs);
    reconcileBowl(nowMs);
    return repaired;
}

void Simulation::setControlOwner(uint8_t actorId, ControlOwner owner) {
    if (actorId < ACTOR_CAP) controlOwners_[actorId] = owner;
}

ControlOwner Simulation::controlOwner(uint8_t actorId) const {
    return actorId < ACTOR_CAP ? controlOwners_[actorId]
                               : ControlOwner::FROZEN;
}

RenderSnapshot Simulation::renderSnapshot() const {
    RenderSnapshot snapshot;
    snapshot.actorCount = actorCount_;
    snapshot.bowl = bowl_;
    snapshot.pairActivity = runtime_.pairActivity();
    for (uint8_t actorId = 0; actorId < actorCount_; ++actorId) {
        const Actor* source = actors_[actorId];
        if (!source) continue;
        ActorRenderSnapshot& target = snapshot.actors[actorId];
        target.active = source->active;
        target.hidden = source->hidden;
        target.teamSlot = source->teamSlot;
        target.speciesId = source->speciesId;
        target.x = source->x;
        target.y = source->y;
        target.velocityX = source->velocityX;
        target.velocityY = source->velocityY;
        target.task = source->task;
    }
    return snapshot;
}

void Simulation::reconcileBowl(uint32_t nowMs) {
    if (bowl_.clearing()) {
        if (bowl_.actorId < 0 || bowl_.actorId >= actorCount_ ||
            !actors_[bowl_.actorId] || !actors_[bowl_.actorId]->active ||
            actors_[bowl_.actorId]->hidden) {
            resetBowl(nowMs);
        }
        return;
    }

    const int8_t owner = runtime_.owner(Resource::BOWL);
    if (owner < 0 || owner >= actorCount_ || !actors_[owner] ||
        !actors_[owner]->active) {
        resetBowl(nowMs);
        return;
    }
    const Task task = actors_[owner]->task;
    if (task == Task::FEEDING) {
        setBowlPhase(BowlPhase::FEEDING, owner, nowMs);
    } else if (task == Task::SEEK_FOOD || task == Task::TURNING) {
        setBowlPhase(BowlPhase::APPROACHING, owner, nowMs);
    } else {
        resetBowl(nowMs);
    }
}

void Simulation::resetBowl(uint32_t nowMs) {
    bowl_.phase = BowlPhase::IDLE;
    bowl_.actorId = Coordinator::NO_ACTOR;
    bowl_.phaseStartedMs = nowMs;
}

void Simulation::setBowlPhase(BowlPhase phase, int8_t actorId,
                              uint32_t nowMs) {
    if (bowl_.phase == phase && bowl_.actorId == actorId) return;
    bowl_.phase = phase;
    bowl_.actorId = actorId;
    bowl_.phaseStartedMs = nowMs;
}

}  // namespace Home
