#include "HomeSleepFlow.h"
#include "core/RoomMovementArea.h"
#include <algorithm>
#include <cmath>

namespace AmoledV1 {
float HomeSleepFlow::companionMinDistance() const {
    const float mainRadius = std::clamp(main_.geometry.footprint.radiusX, 10.0f, 20.0f);
    const float companionRadius = std::clamp(companion_.geometry.footprint.radiusX, 10.0f, 20.0f);
    return std::clamp(mainRadius + companionRadius + 8.0f, 30.0f, 44.0f);
}

bool HomeSleepFlow::companionSpotUsable(const Layout &layout, float x, float y,
                                        float minDistance) const {
    if (!companion_.active)
        return false;
    if (!RoomMovementArea::containsFootprint(layout.polygon, layout.polygonCount, x, y,
                                             companion_.geometry.footprint))
        return false;
    if (layout.hasBedBounds && x >= layout.bedMinX && x <= layout.bedMaxX && y >= layout.bedMinY &&
        y <= layout.bedMaxY)
        return false;
    const float dx = x - main_.x;
    const float dy = y - main_.y;
    const float visualOverlapX =
        main_.geometry.footprint.radiusX + companion_.geometry.footprint.radiusX;
    const float visualOverlapY =
        main_.geometry.footprint.radiusY + companion_.geometry.footprint.radiusY;
    if (std::fabs(dx) < visualOverlapX && std::fabs(dy) < visualOverlapY)
        return false;
    const float bedDx = x - layout.bedX;
    const float bedDy = y - layout.bedY;
    return bedDx * bedDx + bedDy * bedDy >= minDistance * minDistance;
}

bool HomeSleepFlow::pointBlocksBedRoute(float x, float y) const {
    const float clearance = std::max(20.0f, main_.geometry.footprint.radiusX +
                                                companion_.geometry.footprint.radiusX + 2.0f);
    const float clearanceSq = clearance * clearance;
    auto segmentDistanceSq = [x, y](float fromX, float fromY, float toX, float toY) {
        const float dx = toX - fromX;
        const float dy = toY - fromY;
        const float lengthSq = dx * dx + dy * dy;
        float t = lengthSq <= 0.001f ? 0.0f : ((x - fromX) * dx + (y - fromY) * dy) / lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
        const float nearestX = fromX + dx * t;
        const float nearestY = fromY + dy * t;
        const float distanceX = x - nearestX;
        const float distanceY = y - nearestY;
        return distanceX * distanceX + distanceY * distanceY;
    };

    float fromX = main_.x;
    float fromY = main_.y;
    if (main_.route.index >= main_.route.count) {
        return segmentDistanceSq(fromX, fromY, main_.targetX, main_.targetY) < clearanceSq;
    }
    for (uint8_t index = main_.route.index; index < main_.route.count; ++index) {
        const float toX = main_.route.x[index];
        const float toY = main_.route.y[index];
        if (segmentDistanceSq(fromX, fromY, toX, toY) < clearanceSq) {
            return true;
        }
        fromX = toX;
        fromY = toY;
    }
    return false;
}

bool HomeSleepFlow::chooseCompanionSpot(const Layout &layout, float &x, float &y,
                                        bool avoidBedRoute) const {
    if (!companion_.active)
        return false;
    const float minDistance = companionMinDistance();
    const bool mainSeekingBed =
        avoidBedRoute || main_.task == Home::Task::SEEK_SLEEP ||
        (main_.task == Home::Task::TURNING && main_.resumeTask == Home::Task::SEEK_SLEEP);
    const bool hasAnchor = layout.hasAnchor;
    const float preferredX = layout.preferredX, preferredY = layout.preferredY;

    if (hasAnchor && companionSpotUsable(layout, preferredX, preferredY, minDistance) &&
        (!mainSeekingBed || !pointBlocksBedRoute(preferredX, preferredY))) {
        x = preferredX;
        y = preferredY;
        return true;
    }

    const int minimumX = layout.minX, maximumX = layout.maxX;
    const int minimumY = layout.minY, maximumY = layout.maxY;
    float bestScore = 1000000.0f;
    bool found = false;
    for (int candidateY = minimumY; candidateY <= maximumY; candidateY += 3) {
        for (int candidateX = minimumX; candidateX <= maximumX; candidateX += 3) {
            const float pointX = static_cast<float>(candidateX);
            const float pointY = static_cast<float>(candidateY);
            if (!companionSpotUsable(layout, pointX, pointY, minDistance))
                continue;
            if (mainSeekingBed && pointBlocksBedRoute(pointX, pointY)) {
                continue;
            }
            const float dx = pointX - preferredX;
            const float dy = pointY - preferredY;
            const float score = dx * dx + dy * dy;
            if (score >= bestScore)
                continue;
            bestScore = score;
            x = pointX;
            y = pointY;
            found = true;
        }
    }
    return found;
}

bool HomeSleepFlow::atBedPose(const Layout &layout) const {
    return atBedPose(layout.bedX, layout.bedY);
}
bool HomeSleepFlow::atBedPose(float bedX, float bedY) const {
    return std::fabs(main_.x - bedX) <= 2.5f && std::fabs(main_.y - bedY) <= 6.0f;
}
HomeSleepFlow::StartResult HomeSleepFlow::startMain(const Layout &layout, Movement &movement,
                                                    uint32_t nowMs) {
    if (!simulation_.acquire(Home::Resource::BED, 0, Home::Task::SEEK_SLEEP, nowMs)) {
        main_.nextDecisionMs = nowMs + 1000;
        return StartResult::BLOCKED;
    }
    const float dx = companion_.x - layout.bedX, dy = companion_.y - layout.bedY;
    if (companion_.active && dx * dx + dy * dy < 900.0f) {
        float x = companion_.x, y = companion_.y;
        if (chooseCompanionSpot(layout, x, y, true)) {
            simulation_.transition(1, Home::Task::YIELDING, nowMs, 0, true);
            movement.begin(1, Home::Task::YIELDING, x, y, nowMs);
        }
    }
    if (atBedPose(layout)) {
        main_.route.clear();
        main_.velocityX = main_.velocityY = 0.0f;
        main_.targetX = main_.x;
        main_.targetY = main_.y;
        main_.motion = Home::MotionPhase::STILL;
        enterSleep(0, nowMs);
        return StartResult::DIRECT;
    }
    if (movement.begin(0, Home::Task::SEEK_SLEEP, layout.bedX, layout.bedY, nowMs))
        return StartResult::ROUTED;
    simulation_.release(Home::Resource::BED, 0);
    return StartResult::FAILED;
}
bool HomeSleepFlow::startCompanion(const Layout &layout, Movement &movement, uint32_t nowMs) {
    float x = companion_.x, y = companion_.y;
    return chooseCompanionSpot(layout, x, y) &&
           movement.begin(1, Home::Task::SEEK_SLEEP, x, y, nowMs);
}
void HomeSleepFlow::enterSleep(uint8_t actor, uint32_t nowMs) {
    simulation_.transition(actor, Home::Task::SLEEPING, nowMs, 0, true);
}
void HomeSleepFlow::rememberCompanionTarget(float x, float y) {
    companion_.sleepX = x;
    companion_.sleepY = y;
    companion_.sleepSpotValid = true;
}
bool HomeSleepFlow::wake(uint8_t actor, Home::ActorIntent intent, uint32_t nowMs) {
    const bool food = intent == Home::ActorIntent::WAKE_FOR_FOOD;
    if (intent != Home::ActorIntent::WAKE && !food)
        return false;
    if (actor == 0) {
        main_.faintRestActive = false;
        main_.motion = Home::MotionPhase::STILL;
        simulation_.release(Home::Resource::BED, 0);
        simulation_.stop(0, nowMs, food ? 0 : 700);
        main_.mind.onRested(nowMs);
        main_.nextDecisionMs = food ? nowMs : nowMs + 700;
        main_.nextMindUpdateMs = nowMs;
    } else {
        simulation_.release(Home::Resource::BED, 1);
        companion_.sleepSpotValid = false;
        simulation_.stop(1, nowMs, food ? 0 : 700);
        companion_.foodWakeRetryAfterMs = food ? nowMs + 60000UL : 0;
        companion_.mind.onRested(nowMs);
    }
    return true;
}

} // namespace AmoledV1
