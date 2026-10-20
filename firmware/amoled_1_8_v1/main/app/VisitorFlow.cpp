#include "VisitorFlow.h"
#include <algorithm>
#include <cmath>

namespace AmoledV1 {
namespace {
constexpr float VISITOR_DOOR_SPEED = 18.0f;
constexpr uint32_t VISITOR_MOTION_TIMEOUT_MS = Home::VisitArrivalFlow::MOTION_TIMEOUT_MS;
constexpr float VISITOR_DOOR_CLEAR_DISTANCE = 28.0f;
}

bool VisitorFlow::mainInside(const Layout& layout, float x, float y) const {
    return RoomMovementArea::containsFootprint(layout.walkPolygon, layout.walkCount,
        x, y, homeMainActor.geometry.footprint);
}

bool VisitorFlow::guestInside(const Layout& layout, float x, float y) const {
    return RoomMovementArea::containsFootprint(layout.walkPolygon, layout.walkCount,
        x, y, homeCompanionActor.geometry.footprint);
}

void VisitorFlow::face(uint8_t slot, float dx, float dy) {
    effects_.directionMask |= 1U << slot;
    effects_.direction[slot] = std::fabs(dx) >= std::fabs(dy)
        ? (dx >= 0.0f ? Direction::RIGHT : Direction::LEFT)
        : (dy >= 0.0f ? Direction::DOWN : Direction::UP);
}

void VisitorFlow::damageActor(uint8_t slot, float previousY) {
    effects_.actorChanged = true;
    effects_.actorId = slot;
    effects_.previousY = previousY;
}

void VisitorFlow::trace(Trace kind) {
    if (effects_.traceCount < 3) effects_.traces[effects_.traceCount++] = {
        kind, homeMainActor.x, homeMainActor.y, homeCompanionActor.x, homeCompanionActor.y,
        homeMainActor.targetX, homeMainActor.targetY};
}

VisitorFlow::Effects VisitorFlow::takeEffects() {
    const auto result = effects_;
    effects_ = {};
    return result;
}

void VisitorFlow::resetMotion() {
    state_.motion = Motion::NONE;
    visitArrival.reset();
    pending_ = Pending::NONE;
    landingPending_ = false;
}

void VisitorFlow::cancelEntryForDeparture() {
    if (state_.motion != Motion::HOST_APPROACH && state_.motion != Motion::ENTERING) return;
    resetMotion();
    state_.crossingDoor = false;
    state_.untilMs = 0;
    homeRuntime.release(Home::Resource::DOOR, 0);
    homeRuntime.release(Home::Resource::DOOR, 1);
}

bool VisitorFlow::chooseVisitorDoorPose(const Layout& layout, const Home::Actor& actor,
                                      float& x, float& y) const {
    if (!layout.available || layout.polygonCount < 3) return false;
    const float anchorX = static_cast<float>(layout.insideX);
    const float anchorY = static_cast<float>(layout.insideY);
    auto valid = [&](float candidateX, float candidateY) {
        return RoomMovementArea::containsFootprint(
            layout.polygon, layout.polygonCount,
            candidateX, candidateY + actor.geometry.groundOffsetY,
            actor.geometry.footprint);
    };
    auto tryCandidate = [&](int dx, int dy) {
        float candidateX = anchorX + static_cast<float>(dx);
        float candidateY = anchorY + static_cast<float>(dy) -
            actor.geometry.groundOffsetY;
        if (!valid(candidateX, candidateY)) return false;
        x = candidateX;
        y = candidateY;
        return true;
    };
    if (tryCandidate(0, 0)) return true;
    for (int radius = 2; radius <= 48; radius += 2) {
        for (int offset = -radius; offset <= radius; offset += 2) {
            if (tryCandidate(radius, offset) ||
                tryCandidate(-radius, offset) ||
                tryCandidate(offset, radius) ||
                tryCandidate(offset, -radius)) {
                return true;
            }
        }
    }
    return false;
}

bool VisitorFlow::beginVisitorHostClear(const Layout& layout, uint32_t nowMs) {
    const float centerX = (layout.bounds.minX + layout.bounds.maxX) * 0.5f;
    const float centerY = (layout.bounds.minY + layout.bounds.maxY) * 0.5f;
    static constexpr int8_t OFFSETS[][2] = {
        {0, 0}, {-18, 0}, {18, 0}, {0, -14}, {0, 14},
        {-18, -12}, {18, -12}, {-18, 12}, {18, 12},
        {-30, 0}, {30, 0}, {0, -24}, {0, 24},
    };
    for (const auto& offset : OFFSETS) {
        const float candidateX = centerX + offset[0];
        const float candidateY = centerY + offset[1] -
            homeMainActor.geometry.groundOffsetY;
        if (!RoomMovementArea::containsFootprint(
                layout.polygon, layout.polygonCount,
                candidateX,
                candidateY + homeMainActor.geometry.groundOffsetY,
                homeMainActor.geometry.footprint)) {
            continue;
        }
        const float dx = candidateX - state_.doorInsideX;
        const float dy = candidateY - state_.doorInsideY;
        if (dx * dx + dy * dy <
                VISITOR_DOOR_CLEAR_DISTANCE * VISITOR_DOOR_CLEAR_DISTANCE) {
            continue;
        }
        if (!homeRuntime.planRoute(0, candidateX, candidateY, false, false)) {
            continue;
        }
        if (!homeRuntime.transitionPreparedRoute(
                0, Home::Task::DOOR_ACTION, nowMs, 0, true)) {
            continue;
        }
        state_.hostDoorPhase = 2;
        state_.blockedSinceMs = 0;
        state_.untilMs = nowMs + VISITOR_MOTION_TIMEOUT_MS;
        trace(Trace::HOST_CLEAR);
        return true;
    }
    trace(Trace::CLEAR_UNAVAILABLE);
    return false;
}

void VisitorFlow::beginVisitorEntry(const Layout& layout, uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR) {
        return;
    }
    const float insideX = layout.insideX;
    const float insideY = layout.insideY;
    pending_ = Pending::NONE;
    landingPending_ = false;
    state_.doorInsideX = insideX;
    state_.doorInsideY = insideY;
    state_.doorOutsideX = layout.available
        ? static_cast<float>(layout.outsideX) : insideX + 12.0f;
    state_.doorOutsideY = layout.available
        ? static_cast<float>(layout.outsideY) : insideY + 10.0f;
    visitArrival.begin(nowMs);
    homeRuntime.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
    homeRuntime.acquire(Home::Resource::DOOR, 0,
                        Home::Task::DOOR_ACTION, nowMs);
    // The temporary team member already exists in game state, but keep its
    // actor offstage until the host has reached a clear waiting point by the
    // door. This mirrors Stick's host-to-door arrival phase.
    homeCompanionActor.hidden = true;
    homeCompanionActor.route.clear();
    state_.blockedSinceMs = 0;
    state_.welcomeValid = false;
    float welcomeX = insideX;
    float welcomeY = insideY;
    if (!chooseVisitorDoorPose(layout, homeMainActor, welcomeX, welcomeY)) {
        trace(Trace::WELCOME_UNAVAILABLE);
        visitArrival.advance(Home::VisitArrivalEvent::HOST_READY, nowMs);
        state_.hostDoorPhase = 1;
        state_.motion = Motion::HOST_APPROACH;
        state_.untilMs = nowMs +
            Home::VisitArrivalFlow::DOOR_OPEN_MS + VISITOR_MOTION_TIMEOUT_MS;
        return;
    }
    state_.doorInsideX = welcomeX;
    state_.doorInsideY = welcomeY;
    const bool hostStartsOutside = !mainInside(layout, homeMainActor.x, homeMainActor.y);
    if (hostStartsOutside) {
        const float originalX = homeMainActor.x;
        const float originalY = homeMainActor.y;
        RoomMovementArea::Footprint footprint = {
            homeMainActor.geometry.footprint.radiusX, homeMainActor.geometry.footprint.radiusY};
        bool recovered = false;
        for (int radius = 2; radius <= 48 && !recovered; radius += 2) {
            for (int offset = -radius; offset <= radius; offset += 2) {
                const float candidates[][2] = {
                    {originalX + radius, originalY + offset},
                    {originalX - radius, originalY + offset},
                    {originalX + offset, originalY + radius},
                    {originalX + offset, originalY - radius},
                };
                for (const auto& candidate : candidates) {
                    if (!RoomMovementArea::containsFootprint(
                            layout.polygon, layout.polygonCount,
                            candidate[0], candidate[1], footprint)) {
                        continue;
                    }
                    homeMainActor.x = candidate[0];
                    homeMainActor.y = candidate[1];
                    recovered = true;
                    break;
                }
                if (recovered) break;
            }
        }
        if (!recovered) {
            homeMainActor.x = welcomeX;
            homeMainActor.y = welcomeY;
        }
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        homeMainActor.route.clear();
        trace(Trace::RECOVER);
        effects_.fullRender = true;
    }
    const bool routePlanned = homeRuntime.planRoute(
        0, welcomeX, welcomeY, false, false);
    if (routePlanned) {
        // Runtime actor x/y and the renderer's petCenterX/petGroundY are
        // the same bottom-center anchor. Preserve the fixed goal so the
        // debug marker remains exact after the visitor is revealed.
        state_.welcomeX = homeMainActor.targetX;
        state_.welcomeY = homeMainActor.targetY;
        state_.welcomeValid = true;
    }
    state_.hostDoorPhase = 0;
    if (!routePlanned) {
        trace(Trace::HOST_FALLBACK);
        homeMainActor.x = welcomeX;
        homeMainActor.y = welcomeY;
        homeMainActor.targetX = welcomeX;
        homeMainActor.targetY = welcomeY;
        homeRuntime.stop(0, nowMs, 0);
        visitArrival.advance(Home::VisitArrivalEvent::HOST_READY, nowMs);
        state_.hostDoorPhase = 1;
        state_.motion = Motion::HOST_APPROACH;
        state_.untilMs = nowMs +
            Home::VisitArrivalFlow::DOOR_OPEN_MS + VISITOR_MOTION_TIMEOUT_MS;
        return;
    }
    state_.motion = Motion::HOST_APPROACH;
    state_.crossingDoor = false;
    const float distance = std::hypot(
        homeMainActor.targetX - homeMainActor.x,
        homeMainActor.targetY - homeMainActor.y);
    state_.untilMs = nowMs + static_cast<uint32_t>(std::clamp(
        distance / VISITOR_DOOR_SPEED * 1000.0f + 2500.0f,
        6000.0f, 15000.0f));
    face(0,
        homeMainActor.targetX - homeMainActor.x,
        homeMainActor.targetY - homeMainActor.y);
    effects_.resetFrameMask |= 1;
    trace(Trace::HOST_APPROACH);
    effects_.roomChanged = true;
}

void VisitorFlow::startVisitorDoorEntry(const Layout& layout, uint32_t nowMs) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR) {
        return;
    }
    if (!visitArrival.advance(Home::VisitArrivalEvent::GUEST_ENTERING,
                              nowMs)) return;
    float insideX = state_.doorInsideX;
    float insideY = state_.doorInsideY;
    chooseVisitorDoorPose(layout, homeCompanionActor, insideX, insideY);
    state_.guestDoorInsideX = insideX;
    state_.guestDoorInsideY = insideY;
    const float outsideX = state_.doorOutsideX;
    const float outsideY = state_.doorOutsideY;

    homeRuntime.release(Home::Resource::DOOR, 0);
    homeRuntime.stop(0, nowMs, 700);
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    homeMainActor.motion = Home::MotionPhase::STILL;
    effects_.stopAnimation = true;

    homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
    homeRuntime.acquire(Home::Resource::DOOR, 1,
                        Home::Task::DOOR_ACTION, nowMs);
    homeCompanionActor.x = outsideX;
    homeCompanionActor.y = outsideY;
    homeCompanionActor.targetX = insideX;
    homeCompanionActor.targetY = insideY;
    homeCompanionActor.route.clear();
    homeCompanionActor.hidden = false;
    state_.motion = Motion::ENTERING;
    state_.hostDoorPhase = 0;
    state_.crossingDoor = true;
    state_.untilMs = nowMs + VISITOR_MOTION_TIMEOUT_MS;
    const float landingGoals[][2] = {
        {insideX + 12.0f, insideY - 12.0f},
        {insideX + 24.0f, insideY - 12.0f},
        {insideX + 12.0f, insideY - 24.0f},
    };
    state_.landingValid = false;
    for (const auto& goal : landingGoals) {
        if (!guestInside(layout, goal[0], goal[1])) continue;
        state_.landingX = goal[0];
        state_.landingY = goal[1];
        state_.landingValid = true;
        break;
    }
    landingPending_ = true;
    effects_.needLanding = true;
}

void VisitorFlow::completeLanding(bool valid, float x, float y) {
    if (!landingPending_) return;
    landingPending_ = false;
    if (!state_.landingValid) {
        state_.landingValid = valid;
        state_.landingX = x;
        state_.landingY = y;
    }
    trace(Trace::GUEST_VISIBLE);
    face(1,
        state_.guestDoorInsideX - state_.doorOutsideX,
        state_.guestDoorInsideY - state_.doorOutsideY);
    effects_.resetFrameMask |= 2;
    effects_.fullRender = true;
}

void VisitorFlow::beginVisitorExit(const Layout& layout, uint32_t nowMs, bool debugVisitor) {
    if (!homeCompanionActor.active || gameState.teamCount < 2 ||
        gameState.team[1].origin != Game::Origin::VISITOR ||
        state_.motion == Motion::EXITING) {
        return;
    }
    if (state_.motion == Motion::HOST_APPROACH) {
        // An early cancellation can still leave the host owning the door.
        homeRuntime.release(Home::Resource::DOOR, 0);
        homeRuntime.stop(0, nowMs, 0);
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.stopAnimation = true;
    }
    pending_ = Pending::NONE;
    landingPending_ = false;
    visitArrival.reset();
    state_.exitIsDebug = debugVisitor;
    state_.motion = Motion::EXITING;
    state_.crossingDoor = false;
    homeRuntime.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
    homeRuntime.acquire(Home::Resource::DOOR, 1,
                        Home::Task::DOOR_ACTION, nowMs);
    const float insideX = layout.insideX;
    const float insideY = layout.insideY;
    const float fromX = homeCompanionActor.x;
    const float fromY = homeCompanionActor.y;
    const uint32_t travelBudgetMs = static_cast<uint32_t>(std::clamp(
        std::hypot(insideX - fromX, insideY - fromY) /
            VISITOR_DOOR_SPEED * 2200.0f + 3000.0f,
        static_cast<float>(VISITOR_MOTION_TIMEOUT_MS), 20000.0f));
    state_.untilMs = nowMs + travelBudgetMs;
    const float doorwayGoals[][2] = {
        {insideX, insideY}, {insideX + 12.0f, insideY - 12.0f},
        {insideX - 12.0f, insideY - 12.0f},
        {insideX + 24.0f, insideY - 12.0f},
        {insideX - 24.0f, insideY - 12.0f},
        {insideX, insideY - 24.0f},
        {insideX + 24.0f, insideY - 24.0f},
        {insideX - 24.0f, insideY - 24.0f},
    };
    bool routePlanned = false;
    for (const auto& goal : doorwayGoals) {
        if (homeRuntime.planRoute(1, goal[0], goal[1], false, true)) {
            routePlanned = true;
            break;
        }
    }
    if (!routePlanned) {
        homeCompanionActor.route.clear();
        state_.crossingDoor = true;
    }
    effects_.resetFrameMask |= 2;
    trace(Trace::EXIT_BEGIN);
    effects_.roomChanged = true;
}

void VisitorFlow::finishEntry(uint32_t nowMs, bool timedOut) {
    visitArrival.advance(Home::VisitArrivalEvent::GUEST_INSIDE, nowMs);
    state_.motion = Motion::ACTIVE;
    state_.hostDoorPhase = 0;
    state_.crossingDoor = false;
    homeRuntime.release(Home::Resource::DOOR, 1);
    homeRuntime.stop(1, nowMs, 700);
    effects_.guestIdle = true;
    trace(Trace::ENTERED);
    effects_.entryFinished = true;
    effects_.timedOut = timedOut;
}

void VisitorFlow::update(const Layout& layout, uint32_t nowMs, float elapsedSeconds) {
    if (state_.motion != Motion::HOST_APPROACH &&
        state_.motion != Motion::ENTERING &&
        state_.motion != Motion::EXITING) {
        return;
    }
    if (state_.motion == Motion::HOST_APPROACH) {
        if (state_.hostDoorPhase == 1) {
            if (!visitArrival.advance(
                    Home::VisitArrivalEvent::DOOR_OPENED, nowMs)) {
                effects_.roomChanged = true;
                return;
            }
            trace(Trace::DOOR_OPEN);
            if (!beginVisitorHostClear(layout, nowMs)) {
                homeRuntime.stop(0, nowMs, 0);
                startVisitorDoorEntry(layout, nowMs);
            }
            return;
        }
        if (state_.hostDoorPhase == 2) {
            if (elapsedSeconds <= 0.0f) return;
            const float previousY = homeMainActor.y;
            const Home::RouteStep clearStep = homeRuntime.advanceRoute(
                0, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, false);
            float waypointX = homeMainActor.targetX;
            float waypointY = homeMainActor.targetY;
            homeMainActor.route.current(waypointX, waypointY);
            face(0,
                waypointX - homeMainActor.x, waypointY - homeMainActor.y);
            effects_.frameMask |= 1;
            effects_.camera = true;
            damageActor(0, previousY);
            pending_ = Pending::HOST_CLEAR;
            pendingStep_ = clearStep;
            return;
        }
        // The companion actor can be attached during this same update tick.
        // A zero-duration first sample is not a routing failure and must not
        // reveal the visitor before the host has actually moved.
        if (elapsedSeconds <= 0.0f) return;
        const float previousY = homeMainActor.y;
        const Home::RouteStep step = homeRuntime.advanceRoute(
            0, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, false);
        float waypointX = homeMainActor.targetX;
        float waypointY = homeMainActor.targetY;
        homeMainActor.route.current(waypointX, waypointY);
        face(0,
            waypointX - homeMainActor.x, waypointY - homeMainActor.y);
        effects_.frameMask |= 1;
        effects_.camera = true;
        damageActor(0, previousY);
        pending_ = Pending::HOST_APPROACH;
        pendingStep_ = step;
        return;
    }
    if (static_cast<int32_t>(nowMs - state_.untilMs) >= 0) {
        if (state_.motion == Motion::ENTERING) {
            if (state_.landingValid) {
                homeCompanionActor.x = state_.landingX;
                homeCompanionActor.y = state_.landingY;
                homeCompanionActor.targetX = state_.landingX;
                homeCompanionActor.targetY = state_.landingY;
            }
            finishEntry(nowMs, true);
        } else {
            finishExit(nowMs);
        }
        return;
    }

    const float insideX = state_.motion == Motion::ENTERING
        ? state_.guestDoorInsideX
        : (layout.insideX);
    const float insideY = state_.motion == Motion::ENTERING
        ? state_.guestDoorInsideY
        : (layout.insideY);
    const float outsideX = layout.available
        ? static_cast<float>(layout.outsideX) : insideX + 12.0f;
    const float outsideY = layout.available
        ? static_cast<float>(layout.outsideY) : insideY + 10.0f;

    if (state_.motion == Motion::EXITING && !state_.crossingDoor) {
        const float previousX = homeCompanionActor.x;
        const float previousY = homeCompanionActor.y;
        const Home::RouteStep step = homeRuntime.advanceRoute(
            1, nowMs, VISITOR_DOOR_SPEED, elapsedSeconds, 1.0f, true);
        const float movedX = homeCompanionActor.x - previousX;
        const float movedY = homeCompanionActor.y - previousY;
        const bool moved = std::hypot(movedX, movedY) > 0.001f;
        if (moved) {
            face(1, movedX, movedY);
            effects_.frameMask |= 2;
        }
        if (step == Home::RouteStep::ARRIVED ||
            step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE) {
            homeCompanionActor.route.clear();
            state_.crossingDoor = true;
            face(1,
                outsideX - homeCompanionActor.x,
                outsideY - homeCompanionActor.y);
        }
        damageActor(1, previousY);
        return;
    }

    const float targetX = state_.motion == Motion::ENTERING
        ? (state_.crossingDoor || !state_.landingValid
               ? insideX : state_.landingX)
        : outsideX;
    const float targetY = state_.motion == Motion::ENTERING
        ? (state_.crossingDoor || !state_.landingValid
               ? insideY : state_.landingY)
        : outsideY;
    const float dx = targetX - homeCompanionActor.x;
    const float dy = targetY - homeCompanionActor.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float previousY = homeCompanionActor.y;
    const float step = VISITOR_DOOR_SPEED * elapsedSeconds;
    if (distance <= std::max(1.0f, step)) {
        homeCompanionActor.x = targetX;
        homeCompanionActor.y = targetY;
        if (state_.motion == Motion::ENTERING) {
            if (state_.crossingDoor && state_.landingValid) {
                // Doorway crossed; continue to the walkable landing spot.
                state_.crossingDoor = false;
                face(1,
                    state_.landingX - insideX, state_.landingY - insideY);
            } else {
                finishEntry(nowMs, false);
                pending_ = Pending::DIRECT_TAIL;
                previousY_ = previousY;
                return;
            }
        } else {
            finishExit(nowMs);
            pending_ = Pending::DIRECT_TAIL;
            previousY_ = previousY;
            return;
        }
    } else if (distance > 0.0f) {
        homeCompanionActor.x += dx / distance * step;
        homeCompanionActor.y += dy / distance * step;
        face(1, dx, dy);
    }
    effects_.frameMask |= 2;
    damageActor(1, previousY);
}

void VisitorFlow::finishExit(uint32_t nowMs) {
    (void)nowMs;
    const bool debugVisitor = state_.exitIsDebug;
    trace(Trace::EXITED);
    state_.motion = Motion::NONE;
    visitArrival.reset();
    state_.exitIsDebug = false;
    state_.crossingDoor = false;
    state_.untilMs = 0;
    homeRuntime.releaseAll(1);
    effects_.exitFinished = true;
    effects_.debugExit = debugVisitor;
}

void VisitorFlow::completeMovement(const Layout& layout, uint32_t nowMs) {
    const auto pending = pending_;
    pending_ = Pending::NONE;
    if (pending == Pending::HOST_CLEAR) {
        const auto clearStep = pendingStep_;
        if (clearStep == Home::RouteStep::ARRIVED ||
            clearStep == Home::RouteStep::BLOCKED ||
            clearStep == Home::RouteStep::NO_ROUTE ||
            static_cast<int32_t>(nowMs - state_.untilMs) >= 0) {
            if (clearStep != Home::RouteStep::ARRIVED &&
                mainInside(layout,
                    homeMainActor.targetX, homeMainActor.targetY)) {
                homeMainActor.x = homeMainActor.targetX;
                homeMainActor.y = homeMainActor.targetY;
            }
            trace(Trace::CLEAR_END);
            homeRuntime.stop(0, nowMs, 0);
            startVisitorDoorEntry(layout, nowMs);
        }
    } else if (pending == Pending::HOST_APPROACH) {
        const auto step = pendingStep_;
        const bool timedOut =
            static_cast<int32_t>(nowMs - state_.untilMs) >= 0;
        if (step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE) {
            if (state_.blockedSinceMs == 0) {
                state_.blockedSinceMs = nowMs;
            }
        } else {
            state_.blockedSinceMs = 0;
        }
        const bool routeStalled = state_.blockedSinceMs != 0 &&
            nowMs - state_.blockedSinceMs >= 1200;
        if (step == Home::RouteStep::ARRIVED || timedOut || routeStalled) {
            if (step != Home::RouteStep::ARRIVED && state_.welcomeValid) {
                homeMainActor.x = state_.welcomeX;
                homeMainActor.y = state_.welcomeY;
                homeMainActor.targetX = homeMainActor.x;
                homeMainActor.targetY = homeMainActor.y;
            }
            trace(Trace::HOST_READY);
            state_.hostDoorPhase = 1;
            visitArrival.advance(Home::VisitArrivalEvent::HOST_READY, nowMs);
            homeRuntime.stop(0, nowMs, 0);
        }
    } else if (pending == Pending::DIRECT_TAIL) {
        effects_.frameMask |= 2;
        damageActor(1, previousY_);
    }
}


}  // namespace AmoledV1
