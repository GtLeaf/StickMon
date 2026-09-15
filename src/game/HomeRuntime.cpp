#include "game/HomeRuntime.h"

#include <algorithm>
#include <cmath>

namespace Home {

namespace {

constexpr float ROUTE_PROGRESS_EPSILON = 0.45f;
constexpr float ROUTE_PROGRESS_UNSET = 1000000.0f;
constexpr uint32_t SOFT_AVOID_STUCK_MS = 1600;
constexpr uint32_t SOFT_AVOID_WINNER_STUCK_MS = 3200;

float lengthOf(float x, float y) {
    return std::sqrt(x * x + y * y);
}

bool normalize(float& x, float& y) {
    const float length = lengthOf(x, y);
    if (length <= 0.001f) return false;
    x /= length;
    y /= length;
    return true;
}

}  // namespace

void Runtime::attach(Actor& first, Actor* second) {
    actors_[0] = &first;
    actors_[1] = second;
    actorCount_ = second ? ACTOR_CAP : 1;
    coordinator_.attach(first, second);
}

void Runtime::reset(uint32_t nowMs) {
    coordinator_.reset(nowMs);
}

Actor* Runtime::actor(uint8_t actorId) {
    return actorId < actorCount_ ? actors_[actorId] : nullptr;
}

const Actor* Runtime::actor(uint8_t actorId) const {
    return actorId < actorCount_ ? actors_[actorId] : nullptr;
}

RoomNavigator::Obstacle Runtime::obstacleFor(
    uint8_t actorId, bool avoidOther) const {
    RoomNavigator::Obstacle obstacle;
    if (!avoidOther || actorCount_ < ACTOR_CAP ||
        world_.actorCollisionMode == ActorCollisionMode::SOFT) {
        return obstacle;
    }
    const Actor* other = otherActor(actorId);
    if (!other || !other->active || other->hidden) return obstacle;
    obstacle.active = true;
    obstacle.x = other->x;
    obstacle.footY = other->y + other->geometry.groundOffsetY;
    obstacle.minSeparation = world_.actorMinSeparation;
    return obstacle;
}

const Actor* Runtime::otherActor(uint8_t actorId) const {
    if (actorId >= ACTOR_CAP || actorCount_ < ACTOR_CAP) return nullptr;
    return actor(actorId == 0 ? 1 : 0);
}

bool Runtime::overlapsOther(uint8_t actorId, float x, float y) const {
    const Actor* moving = actor(actorId);
    const Actor* other = otherActor(actorId);
    if (!moving || !moving->active || moving->hidden ||
        !other || !other->active || other->hidden) {
        return false;
    }

    const float movingFootY = y + moving->geometry.groundOffsetY;
    const float otherFootY = other->y + other->geometry.groundOffsetY;
    const float allowedX = moving->geometry.footprint.radiusX +
                           other->geometry.footprint.radiusX +
                           world_.actorOverlapPadding;
    const float allowedY = moving->geometry.footprint.radiusY +
                           other->geometry.footprint.radiusY +
                           world_.actorOverlapPadding;
    return std::fabs(x - other->x) < std::max(1.0f, allowedX) &&
           std::fabs(movingFootY - otherFootY) < std::max(1.0f, allowedY);
}

bool Runtime::segmentOverlapsOther(uint8_t actorId, float fromX,
                                   float fromY, float toX, float toY) const {
    const Actor* moving = actor(actorId);
    const Actor* other = otherActor(actorId);
    if (!moving || !moving->active || moving->hidden ||
        !other || !other->active || other->hidden) {
        return false;
    }

    const float otherFootY = other->y + other->geometry.groundOffsetY;
    const float fromFootY = fromY + moving->geometry.groundOffsetY;
    const float toFootY = toY + moving->geometry.groundOffsetY;
    // overlapsOther() uses strict interior bounds, so a segment travelling
    // exactly along the footprint edge must remain legal as well.
    constexpr float EDGE_EPSILON = 0.001f;
    const float halfX = std::max(
        1.0f, moving->geometry.footprint.radiusX +
                  other->geometry.footprint.radiusX +
                  world_.actorOverlapPadding) - EDGE_EPSILON;
    const float halfY = std::max(
        1.0f, moving->geometry.footprint.radiusY +
                  other->geometry.footprint.radiusY +
                  world_.actorOverlapPadding) - EDGE_EPSILON;
    const float minX = other->x - halfX;
    const float maxX = other->x + halfX;
    const float minY = otherFootY - halfY;
    const float maxY = otherFootY + halfY;

    // Slab intersection for the swept foot point. The rectangle is
    // deliberately conservative; the padding keeps a one-pixel visual gap.
    float tMin = 0.0f;
    float tMax = 1.0f;
    const float delta[2] = {toX - fromX, toFootY - fromFootY};
    const float start[2] = {fromX, fromFootY};
    const float boundsMin[2] = {minX, minY};
    const float boundsMax[2] = {maxX, maxY};
    for (uint8_t axis = 0; axis < 2; ++axis) {
        if (std::fabs(delta[axis]) <= 0.0001f) {
            if (start[axis] < boundsMin[axis] ||
                start[axis] > boundsMax[axis]) {
                return false;
            }
            continue;
        }
        float nearT = (boundsMin[axis] - start[axis]) / delta[axis];
        float farT = (boundsMax[axis] - start[axis]) / delta[axis];
        if (nearT > farT) std::swap(nearT, farT);
        tMin = std::max(tMin, nearT);
        tMax = std::min(tMax, farT);
        if (tMin > tMax) return false;
    }
    return tMax >= 0.0f && tMin <= 1.0f;
}

bool Runtime::shouldYieldToOther(uint8_t actorId) const {
    const Actor* moving = actor(actorId);
    const Actor* other = otherActor(actorId);
    if (!moving || !other || !other->active || other->hidden) return true;

    const Task movingTask = moving->task == Task::TURNING
        ? moving->resumeTask : moving->task;
    const Task otherTask = other->task == Task::TURNING
        ? other->resumeTask : other->task;
    if (!taskUsesRoute(otherTask) || other->route.empty()) return false;

    const uint8_t movingPriority = taskPriority(movingTask);
    const uint8_t otherPriority = taskPriority(otherTask);
    if (movingPriority != otherPriority) {
        return movingPriority < otherPriority;
    }
    // The teammate yields equal-priority encounters. A stable tie-break keeps
    // both actors from making the same decision on consecutive updates.
    return actorId == 1;
}

bool Runtime::pointAllowed(const Actor& moving, float x, float y) const {
    return world_.polygon && world_.polygonCount >= 3 &&
           RoomMovementArea::containsFootprint(
               world_.polygon, world_.polygonCount,
               x, y + moving.geometry.groundOffsetY,
               moving.geometry.footprint);
}

bool Runtime::chooseSoftStep(uint8_t actorId, float waypointX,
                             float waypointY, float proposedX,
                             float proposedY, float step, bool avoidOther,
                             bool maintainClearance,
                             float& outX, float& outY) const {
    outX = proposedX;
    outY = proposedY;
    const Actor* moving = actor(actorId);
    const Actor* other = otherActor(actorId);
    if (world_.actorCollisionMode != ActorCollisionMode::SOFT ||
        !avoidOther || !moving || !other || !other->active ||
        other->hidden) {
        return moving && pointAllowed(*moving, proposedX, proposedY);
    }
    const bool alreadyOverlapping = overlapsOther(actorId, moving->x, moving->y);
    const bool proposedOverlapping = overlapsOther(actorId, proposedX, proposedY);
    const bool crossesOther = !alreadyOverlapping &&
        segmentOverlapsOther(actorId, moving->x, moving->y,
                             proposedX, proposedY);
    if (!proposedOverlapping && !crossesOther && !maintainClearance) {
        return pointAllowed(*moving, proposedX, proposedY);
    }

    float movementX = proposedX - moving->x;
    float movementY = proposedY - moving->y;
    if (!normalize(movementX, movementY)) {
        movementX = waypointX - moving->x;
        movementY = waypointY - moving->y;
        if (!normalize(movementX, movementY)) {
            movementX = actorId == 0 ? 1.0f : -1.0f;
            movementY = 0.0f;
        }
    }
    const float moveDistance = std::max(0.05f, step);

    float awayX = proposedX - other->x;
    float awayY = proposedY + moving->geometry.groundOffsetY -
                  (other->y + other->geometry.groundOffsetY);
    if (!normalize(awayX, awayY)) {
        awayX = moving->x - other->x;
        awayY = moving->y + moving->geometry.groundOffsetY -
                (other->y + other->geometry.groundOffsetY);
        if (!normalize(awayX, awayY)) {
            awayX = actorId == 0 ? 1.0f : -1.0f;
            awayY = 0.0f;
        }
    }
    // Keep the passing side tied to the route heading. A tangent derived from
    // the changing actor-to-actor vector rotates every frame and can make two
    // actors orbit at the overlap boundary instead of passing one another.
    const float tangentX = -movementY;
    const float tangentY = movementX;
    // Opposing route headings produce opposing world-space tangents when both
    // actors use the same handedness.
    constexpr float preferredSide = 1.0f;

    struct CandidateDirection {
        float x;
        float y;
    };
    CandidateDirection candidates[8] = {
        {movementX, movementY},
        {preferredSide * tangentX, preferredSide * tangentY},
        {-preferredSide * tangentX, -preferredSide * tangentY},
        {awayX, awayY},
        {movementX + preferredSide * tangentX,
         movementY + preferredSide * tangentY},
        {movementX - preferredSide * tangentX,
         movementY - preferredSide * tangentY},
        {awayX + preferredSide * tangentX,
         awayY + preferredSide * tangentY},
        {awayX - preferredSide * tangentX,
         awayY - preferredSide * tangentY},
    };

    float desiredX = waypointX - moving->x;
    float desiredY = waypointY - moving->y;
    if (!normalize(desiredX, desiredY)) {
        desiredX = movementX;
        desiredY = movementY;
    }

    bool found = false;
    float bestScore = -1000000.0f;
    float fallbackScore = -1000000.0f;
    float fallbackX = proposedX;
    float fallbackY = proposedY;
    const float currentSeparationX = moving->x - other->x;
    const float currentSeparationY =
        moving->y + moving->geometry.groundOffsetY -
        (other->y + other->geometry.groundOffsetY);
    const float currentSeparation =
        currentSeparationX * currentSeparationX +
        currentSeparationY * currentSeparationY;
    for (const CandidateDirection& candidate : candidates) {
        float directionX = candidate.x;
        float directionY = candidate.y;
        if (!normalize(directionX, directionY)) continue;
        const float candidateX = moving->x + directionX * moveDistance;
        const float candidateY = moving->y + directionY * moveDistance;
        if (!pointAllowed(*moving, candidateX, candidateY)) continue;

        const float progress = directionX * desiredX + directionY * desiredY;
        const float separationX = candidateX - other->x;
        const float separationY = candidateY + moving->geometry.groundOffsetY -
                                 (other->y + other->geometry.groundOffsetY);
        const float separation = separationX * separationX +
                                 separationY * separationY;
        if (maintainClearance) {
            if (separation + 0.01f < currentSeparation) continue;
        }
        const float score = progress * 100.0f + separation * 0.01f;
        if (score > fallbackScore) {
            fallbackScore = score;
            fallbackX = candidateX;
            fallbackY = candidateY;
        }
        const bool candidateOverlapping =
            overlapsOther(actorId, candidateX, candidateY) ||
            (!alreadyOverlapping && segmentOverlapsOther(
                actorId, moving->x, moving->y, candidateX, candidateY));
        if (candidateOverlapping) continue;
        if (!found || score > bestScore) {
            found = true;
            bestScore = score;
            outX = candidateX;
            outY = candidateY;
        }
    }

    if (found) return true;
    // When actors start inside one another, requiring a fully separated point
    // in one frame would recreate the old deadlock. Move in the best available
    // separating direction and let later frames finish the separation.
    if (alreadyOverlapping && fallbackScore > -1000000.0f) {
        outX = fallbackX;
        outY = fallbackY;
        return true;
    }
    return false;
}

bool Runtime::planRoute(uint8_t actorId, float goalX, float goalY,
                        bool allowOutsideStart, bool avoidOther) {
    Actor* moving = actor(actorId);
    if (!moving || !moving->active || !world_.polygon ||
        world_.polygonCount < 3) {
        return false;
    }

    RoomNavigator::Request request;
    request.polygon = world_.polygon;
    request.polygonCount = world_.polygonCount;
    request.bounds = {
        world_.footBounds.minX,
        world_.footBounds.minY - moving->geometry.groundOffsetY,
        world_.footBounds.maxX,
        world_.footBounds.maxY - moving->geometry.groundOffsetY,
    };
    request.geometry = moving->geometry;
    request.startX = moving->x;
    request.startY = moving->y;
    request.goalX = goalX;
    request.goalY = goalY;
    request.allowOutsideStart = allowOutsideStart;
    request.obstacle = obstacleFor(actorId, avoidOther);

    Route proposed;
    if (!RoomNavigator::build(request, proposed, world_.scratch)) {
        return false;
    }
    moving->targetX = goalX;
    moving->targetY = goalY;
    moving->route = proposed;
    moving->blockedSinceMs = 0;
    moving->lastWaypointDistance = 1000000.0f;
    return true;
}

RouteStep Runtime::advanceRoute(uint8_t actorId, uint32_t nowMs, float speed,
                                float dtSeconds, float arrivalDistance,
                                bool avoidOther) {
    Actor* moving = actor(actorId);
    if (!moving || !moving->active || moving->route.empty()) {
        return RouteStep::NO_ROUTE;
    }
    if (speed <= 0.0f || dtSeconds <= 0.0f) {
        moving->velocityX = 0.0f;
        moving->velocityY = 0.0f;
        return RouteStep::BLOCKED;
    }

    float waypointX = 0.0f;
    float waypointY = 0.0f;
    if (!moving->route.current(waypointX, waypointY)) {
        return RouteStep::NO_ROUTE;
    }
    const float dx = waypointX - moving->x;
    const float dy = waypointY - moving->y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float step = speed * dtSeconds;
    const bool reachesWaypoint =
        distance <= std::max(arrivalDistance, step);
    const float nextX = reachesWaypoint
        ? waypointX : moving->x + dx / distance * step;
    const float nextY = reachesWaypoint
        ? waypointY : moving->y + dy / distance * step;

    const RoomNavigator::Obstacle obstacle =
        obstacleFor(actorId, avoidOther);
    const float fromFootY = moving->y + moving->geometry.groundOffsetY;
    const float nextFootY = nextY + moving->geometry.groundOffsetY;
    const bool currentlyOverlapping = overlapsOther(
        actorId, moving->x, moving->y);
    const bool directStepConflicts =
        overlapsOther(actorId, nextX, nextY) ||
        (!currentlyOverlapping && segmentOverlapsOther(
            actorId, moving->x, moving->y, nextX, nextY));
    const bool routeStillCrossesOther = !currentlyOverlapping &&
        segmentOverlapsOther(
            actorId, moving->x, moving->y, waypointX, waypointY);
    const bool softAvoidingOther =
        world_.actorCollisionMode == ActorCollisionMode::SOFT && avoidOther &&
        (directStepConflicts ||
         (moving->blockedSinceMs != 0 && routeStillCrossesOther));
    float acceptedX = nextX;
    float acceptedY = nextY;
    const bool accepted = world_.actorCollisionMode == ActorCollisionMode::SOFT
        ? chooseSoftStep(actorId, waypointX, waypointY, nextX, nextY,
                         reachesWaypoint ? distance : step, avoidOther,
                         softAvoidingOther,
                         acceptedX, acceptedY)
        : pointAllowed(*moving, nextX, nextY) &&
          RoomNavigator::segmentKeepsSpacing(
              moving->x, fromFootY, nextX, nextFootY, obstacle);
    if (!accepted) {
        moving->velocityX = 0.0f;
        moving->velocityY = 0.0f;
        return RouteStep::BLOCKED;
    }

    moving->velocityX = acceptedX - moving->x;
    moving->velocityY = acceptedY - moving->y;
    moving->x = acceptedX;
    moving->y = acceptedY;
    const float acceptedDistance = lengthOf(
        waypointX - acceptedX, waypointY - acceptedY);
    const bool madeProgress =
        moving->lastWaypointDistance >= ROUTE_PROGRESS_UNSET ||
        acceptedDistance + ROUTE_PROGRESS_EPSILON <
            moving->lastWaypointDistance;
    if (madeProgress) {
        moving->lastWaypointDistance = acceptedDistance;
        moving->lastMoveProgressMs = nowMs;
        moving->stuckRecoveryCount = 0;
    }
    if (softAvoidingOther) {
        if (moving->blockedSinceMs == 0) moving->blockedSinceMs = nowMs;
        const uint32_t stallLimit = shouldYieldToOther(actorId)
            ? SOFT_AVOID_STUCK_MS : SOFT_AVOID_WINNER_STUCK_MS;
        if (!madeProgress &&
            nowMs - moving->lastMoveProgressMs >= stallLimit) {
            moving->velocityX = 0.0f;
            moving->velocityY = 0.0f;
            if (moving->stuckRecoveryCount < 0xFF) {
                ++moving->stuckRecoveryCount;
            }
            return RouteStep::BLOCKED;
        }
    } else {
        moving->blockedSinceMs = 0;
    }
    const bool reachedAcceptedWaypoint = reachesWaypoint &&
        lengthOf(waypointX - acceptedX, waypointY - acceptedY) <=
            std::max(arrivalDistance, 0.01f);
    if (!reachedAcceptedWaypoint) return RouteStep::MOVING;

    if (moving->route.advance()) {
        moving->lastWaypointDistance = ROUTE_PROGRESS_UNSET;
        moving->lastMoveProgressMs = nowMs;
        moving->blockedSinceMs = 0;
        return RouteStep::MOVING;
    }
    moving->velocityX = 0.0f;
    moving->velocityY = 0.0f;
    return RouteStep::ARRIVED;
}

}  // namespace Home
