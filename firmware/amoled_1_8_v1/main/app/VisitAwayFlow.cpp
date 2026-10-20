#include "VisitAwayFlow.h"
#include <algorithm>
#include <cmath>

namespace AmoledV1 {
namespace {
constexpr float DOOR_SPEED = 18.0f;
}

void VisitAwayFlow::begin(const Door& door, uint32_t nowMs) {
    state_.door = door;
    pending_ = false;
    simulation_.releaseAll(0);
    simulation_.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
    actor_.hidden = false;
    actor_.faintRestActive = false;
    actor_.motion = Home::MotionPhase::STILL;
    actor_.route.clear();
    if (!door.roomAvailable ||
        !simulation_.planRoute(0, door.insideX, door.insideY, false, false)) {
        actor_.targetX = door.insideX;
        actor_.targetY = door.insideY;
    }
    state_.phase = Phase::WALK_TO_DOOR;
    state_.startedMs = nowMs;
    effects_.resetAnimation = true;
    effects_.fullRender = true;
}

bool VisitAwayFlow::update(bool sessionActive, uint32_t nowMs, float elapsedSeconds) {
    if (!sessionActive) {
        effects_.clearRecall = true;
        if (state_.phase == Phase::AWAY || state_.phase == Phase::CROSS_DOOR) {
            actor_.x = state_.door.insideX;
            actor_.y = state_.door.insideY;
        }
        actor_.hidden = false;
        simulation_.stop(0, nowMs, 0);
        actor_.motion = Home::MotionPhase::STILL;
        effects_.stopAnimation = true;
        effects_.camera = true;
        effects_.fullRender = true;
        state_.phase = Phase::NONE;
        pending_ = false;
        return false;
    }
    if (state_.phase == Phase::AWAY || state_.phase == Phase::NONE) return state_.phase == Phase::AWAY;
    const float previousX = actor_.x, previousY = actor_.y;
    bool arrived = false;
    if (state_.phase == Phase::WALK_TO_DOOR && !actor_.route.empty()) {
        const auto step = simulation_.advanceRoute(0, nowMs, DOOR_SPEED, elapsedSeconds, 1.0f, false);
        arrived = step == Home::RouteStep::ARRIVED;
        if (step == Home::RouteStep::BLOCKED || step == Home::RouteStep::NO_ROUTE) actor_.route.clear();
    } else {
        const float targetX = state_.phase == Phase::WALK_TO_DOOR ? state_.door.insideX : state_.door.outsideX;
        const float targetY = state_.phase == Phase::WALK_TO_DOOR ? state_.door.insideY : state_.door.outsideY;
        const float distance = std::hypot(targetX - actor_.x, targetY - actor_.y);
        const float step = DOOR_SPEED * elapsedSeconds;
        arrived = distance <= std::max(0.8f, step);
        if (arrived) { actor_.x = targetX; actor_.y = targetY; }
        else if (step > 0.0f) {
            actor_.x += (targetX - actor_.x) / distance * step;
            actor_.y += (targetY - actor_.y) / distance * step;
        }
    }
    const float dx = actor_.x - previousX, dy = actor_.y - previousY;
    effects_.directionChanged = true;
    effects_.direction = std::fabs(dx) >= std::fabs(dy)
        ? (dx >= 0.0f ? Direction::RIGHT : Direction::LEFT)
        : (dy >= 0.0f ? Direction::DOWN : Direction::UP);
    effects_.startWalk = effects_.camera = effects_.actorChanged = true;
    effects_.previousY = previousY;
    arrived_ = arrived;
    pending_ = true;
    return true;
}

void VisitAwayFlow::completeMovement(uint32_t nowMs) {
    if (!pending_) return;
    pending_ = false;
    const auto next = Home::VisitDepartureFlow::advance(state_.phase, arrived_, nowMs - state_.startedMs);
    if (next == state_.phase) return;
    state_.phase = next;
    if (next == Phase::CROSS_DOOR) {
        actor_.x = state_.door.insideX;
        actor_.y = state_.door.insideY;
        actor_.route.clear();
        state_.startedMs = nowMs;
    } else {
        actor_.hidden = true;
        effects_.departed = effects_.fullRender = true;
    }
}

VisitAwayFlow::Effects VisitAwayFlow::takeEffects() {
    const auto result = effects_;
    effects_ = {};
    return result;
}
}  // namespace AmoledV1
