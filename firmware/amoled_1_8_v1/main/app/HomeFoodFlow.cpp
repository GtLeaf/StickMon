#include "HomeFoodFlow.h"
#include "game/GameRandom.h"
#include <algorithm>
#include <cmath>

namespace AmoledV1 {
void HomeFoodFlow::reset() {
    state_ = State{};
    effects_ = Effects{};
}
HomeFoodFlow::Effects HomeFoodFlow::takeEffects() {
    const auto result = effects_;
    effects_ = Effects{};
    return result;
}
void HomeFoodFlow::scheduleArbitration(uint32_t atMs) {
    state_.pending = true;
    state_.retryAtMs = atMs;
}
Home::HouseholdObservation
HomeFoodFlow::currentObservation(Home::HouseholdObservation observation) const {
    observation.bowlHasFood = game_.room.bowlCount > 0;
    observation.bowlClearing = simulation_.bowlSession().clearing();
    observation.bowlOwner = simulation_.owner(Home::Resource::BOWL);
    return observation;
}
bool HomeFoodFlow::eligible(uint8_t actor, const Home::HouseholdObservation &observation) const {
    return actor < observation.actorCount && actor < Home::ACTOR_CAP &&
           Home::ActorController::foodEligible(observation.actors[actor], game_.room.bowlCount > 0);
}
int8_t HomeFoodFlow::preferred(const Home::HouseholdObservation &observation) const {
    return Home::ActorController::selectBowlActor(currentObservation(observation)).actorId;
}
bool HomeFoodFlow::nearFood(uint8_t id, const Layout &layout) const {
    const auto *actor = simulation_.actor(id);
    if (!actor || !actor->active || actor->hidden)
        return false;
    const float toleranceX = std::max(9.0f, actor->geometry.footprint.radiusX + 8.0f);
    const float toleranceY = std::max(7.0f, actor->geometry.footprint.radiusY + 6.0f);
    return std::fabs(actor->x - layout.foodX) <= toleranceX &&
           std::fabs(actor->y + actor->geometry.groundOffsetY - layout.foodY) <= toleranceY;
}
bool HomeFoodFlow::blocksApproach(uint8_t id, const Layout &layout) const {
    const auto *actor = simulation_.actor(id);
    if (!actor || !actor->active || actor->hidden)
        return false;
    const float otherRadius =
        id == 0 ? companion_.geometry.footprint.radiusX : main_.geometry.footprint.radiusX;
    const float clearance = std::max(26.0f, actor->geometry.footprint.radiusX + otherRadius + 6.0f);
    const float dx = actor->x - layout.foodX, dy = actor->y - layout.foodY;
    return dx * dx + dy * dy < clearance * clearance;
}
bool HomeFoodFlow::claim(uint8_t actor, const Layout &layout,
                         const Home::HouseholdObservation &observation, uint32_t nowMs) {
    if (!simulation_.bowlClaimAllowed(actor))
        return false;
    if (actor == 0 && blocksApproach(1, layout))
        return false;
    int8_t owner = simulation_.owner(Home::Resource::BOWL);
    if (owner >= 0 && !eligible(static_cast<uint8_t>(owner), observation)) {
        simulation_.release(Home::Resource::BOWL, static_cast<uint8_t>(owner));
        owner = -1;
    }
    if (owner >= 0)
        return owner == static_cast<int8_t>(actor);
    if (preferred(observation) != static_cast<int8_t>(actor))
        return false;
    return simulation_.acquire(Home::Resource::BOWL, actor, Home::Task::SEEK_FOOD, nowMs);
}
void HomeFoodFlow::enterFeeding(uint8_t id, uint32_t nowMs) {
    auto &actor = id == 0 ? main_ : companion_;
    if (id == 0) {
        simulation_.transition(0, Home::Task::FEEDING, nowMs, 0, true);
        actor.motion = Home::MotionPhase::STILL;
        state_.biteAtMs[0] = nowMs + GameRandom::range(700, 1301);
        actor.taskUntilMs = nowMs + GameRandom::range(3200, 5601);
    } else {
        simulation_.transition(1, Home::Task::FEEDING, nowMs,
                               static_cast<uint32_t>(GameRandom::range(3200, 5601)), true);
        state_.biteAtMs[1] = nowMs + GameRandom::range(700, 1301);
    }
    effects_.feedingMask |= 1U << id;
}
bool HomeFoodFlow::startSeeking(uint8_t id, const Layout &layout,
                                const Home::HouseholdObservation &observation, Movement &movement,
                                uint32_t nowMs) {
    effects_.seekActor = id;
    effects_.seekTrace = SeekTrace::CLAIM_REJECTED;
    if (!claim(id, layout, observation, nowMs))
        return false;
    if (nearFood(id, layout)) {
        enterFeeding(id, nowMs);
        auto& actor = id == 0 ? main_ : companion_;
        actor.targetX = actor.x;
        actor.targetY = actor.y;
        effects_.roomRender = true;
        effects_.seekTrace = SeekTrace::DIRECT;
        return true;
    }
    float x = layout.foodX, y = layout.foodY;
    if (id == 0) {
        effects_.seekTrace = SeekTrace::APPROACH_REJECTED;
        if (movement.chooseMainApproach(layout.foodX, layout.foodY, x, y)) {
            effects_.seekTrace = SeekTrace::ROUTE_REJECTED;
            if (movement.begin(0, Home::Task::SEEK_FOOD, x, y, nowMs)) {
                effects_.seekTrace = SeekTrace::ROUTED;
                return true;
            }
        }
    } else {
        bool found = false;
        // Search with the companion's actual footprint and route constraints.
        for (int radius = 0; radius <= 36 && !found; radius += 2) {
            const int yRadius = std::min(radius, 24);
            for (int dy = -yRadius; dy <= yRadius && !found; dy += 2) {
                for (float candidateX : {layout.foodX + radius, layout.foodX - radius}) {
                    const float candidateY = layout.foodY + dy;
                    if (simulation_.planRoute(1, candidateX, candidateY, false, true)) {
                        x = candidateX;
                        y = candidateY;
                        found = true;
                        break;
                    }
                }
            }
            for (int dx = -radius + 2; dx <= radius - 2 && !found; dx += 2) {
                for (float candidateY : {layout.foodY + yRadius, layout.foodY - yRadius}) {
                    const float candidateX = layout.foodX + dx;
                    if (simulation_.planRoute(1, candidateX, candidateY, false, true)) {
                        x = candidateX;
                        y = candidateY;
                        found = true;
                        break;
                    }
                }
            }
        }
        effects_.seekTrace = SeekTrace::ROUTE_REJECTED;
        if (found && movement.begin(1, Home::Task::SEEK_FOOD, x, y, nowMs)) {
            effects_.seekTrace = SeekTrace::ROUTED;
            return true;
        }
    }
    simulation_.release(Home::Resource::BOWL, id);
    return false;
}
HomeFoodFlow::FeedingUpdate HomeFoodFlow::tickFeeding(uint8_t id, uint32_t nowMs) {
    FeedingUpdate result;
    auto &actor = id == 0 ? main_ : companion_;
    const auto &monster = game_.team[id];
    const bool canContinue = simulation_.owner(Home::Resource::BOWL) == id &&
                             (id == 0 || monster.origin != Game::Origin::VISITOR) &&
                             game_.room.bowlCount > 0 &&
                             monster.satiety < MONSTER_FEED_TARGET_SATIETY;
    if (canContinue && static_cast<int32_t>(nowMs - state_.biteAtMs[id]) >= 0) {
        result.bite = Game::HomeCare::consumeBowlFood(game_, id);
        state_.biteAtMs[id] = nowMs + GameRandom::range(1000, 1601);
        // Preserve the existing leader/reserve asymmetry on a rejected bite.
        if (id == 0 && !result.bite.consumed)
            actor.taskUntilMs = nowMs;
    }
    result.finished = !canContinue || static_cast<int32_t>(nowMs - actor.taskUntilMs) >= 0;
    return result;
}
void HomeFoodFlow::finishFeeding(uint8_t id, uint32_t nowMs) {
    auto &actor = id == 0 ? main_ : companion_;
    if (id == 1)
        actor.mind.onAte(nowMs);
    simulation_.beginBowlClearance(id, nowMs);
    simulation_.stop(id, nowMs);
    if (id == 0) {
        actor.motion = Home::MotionPhase::STILL;
        actor.mind.onAte(nowMs);
    }
    effects_.finishedMask |= 1U << id;
}
bool HomeFoodFlow::startCompanionExit(const Layout &layout, Movement &movement, uint32_t nowMs) {
    const float clearance = std::max(26.0f, companion_.geometry.footprint.radiusX +
                                                main_.geometry.footprint.radiusX + 6.0f);
    constexpr float directions[][2] = {{1, 0}, {0.8f, -0.6f},  {0.8f, 0.6f},  {0, -1},
                                       {0, 1}, {-0.8f, -0.6f}, {-0.8f, 0.6f}, {-1, 0}};
    for (float radius : {clearance + 2.0f, clearance + 8.0f, 40.0f, 52.0f, 64.0f}) {
        for (const auto &direction : directions) {
            const float x = layout.foodX + radius * direction[0],
                        y = layout.foodY + radius * direction[1];
            const float dx = x - layout.foodX, dy = y - layout.foodY;
            if (dx * dx + dy * dy <= clearance * clearance)
                continue;
            if (movement.begin(1, Home::Task::YIELDING, x, y, nowMs))
                return true;
        }
    }
    return false;
}
void HomeFoodFlow::reconsiderRemaining(uint8_t finishedActor,
                                       const Home::HouseholdObservation &observation,
                                       uint32_t nowMs) {
    if (game_.room.bowlCount > 0 && preferred(observation) >= 0) {
        scheduleArbitration(nowMs);
        auto &other = finishedActor == 0 ? companion_ : main_;
        other.nextMindUpdateMs = other.nextDecisionMs = nowMs;
    }
}
void HomeFoodFlow::servingAdded(bool mainFoodMotion, uint32_t nowMs) {
    const int8_t owner = simulation_.owner(Home::Resource::BOWL);
    if (owner >= 0)
        simulation_.release(Home::Resource::BOWL, static_cast<uint8_t>(owner));
    if (mainFoodMotion) {
        simulation_.stop(0, nowMs);
        main_.motion = Home::MotionPhase::STILL;
        main_.targetX = main_.x;
        main_.targetY = main_.y;
        effects_.stopMain = true;
    }
    if (companion_.task == Home::Task::SEEK_FOOD || companion_.task == Home::Task::FEEDING) {
        simulation_.stop(1, nowMs);
        effects_.stopCompanion = true;
    }
    simulation_.notify(Home::WorldEvent::BOWL_CHANGED);
    scheduleArbitration(nowMs);
    main_.nextMindUpdateMs = main_.nextDecisionMs = nowMs;
    if (companion_.active) {
        companion_.nextMindUpdateMs = companion_.nextDecisionMs = nowMs;
        companion_.foodWakeRetryAfterMs = 0;
    }
}
void HomeFoodFlow::service(const Layout &layout, const Home::HouseholdObservation &observation,
                           Movement &movement, uint32_t nowMs) {
    if (!state_.pending || static_cast<int32_t>(nowMs - state_.retryAtMs) < 0)
        return;
    if (game_.room.bowlCount == 0) {
        state_.pending = false;
        return;
    }
    const int8_t selected = preferred(observation);
    if (selected < 0) {
        state_.pending = false;
        return;
    }
    if (Home::ActorController::survivalIntent(observation.actors[selected], true) ==
        Home::ActorIntent::WAKE_FOR_FOOD) {
        state_.retryAtMs = nowMs + 80;
        return;
    }
    if (startSeeking(static_cast<uint8_t>(selected), layout, observation, movement, nowMs)) {
        state_.pending = false;
        return;
    }
    effects_.yieldActor = selected;
    state_.retryAtMs = nowMs + 1000;
}
void HomeFoodFlow::companionYieldArrived(const Layout &layout, uint32_t nowMs) {
    if (simulation_.bowlSession().clearing() && simulation_.bowlSession().actorId == 1 &&
        !blocksApproach(1, layout))
        simulation_.finishBowlClearance(1, nowMs);
}
} // namespace AmoledV1
