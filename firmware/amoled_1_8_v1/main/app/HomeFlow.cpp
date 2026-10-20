#include "HomeFlow.h"

#include <algorithm>
#include <cmath>
#include "core/RoomMovementArea.h"
#include "game/GameRandom.h"

namespace AmoledV1 {
namespace {
constexpr uint32_t ATTENTION_INITIAL_MIN_MS = 25000UL;
constexpr uint32_t ATTENTION_INITIAL_MAX_MS = 45000UL;
constexpr uint32_t ATTENTION_MIN_MS = 90000UL;
constexpr uint32_t ATTENTION_MAX_MS = 180000UL;
constexpr uint32_t SPECIAL_ACTION_MIN_MS = 20000UL;
constexpr uint32_t SPECIAL_ACTION_MAX_MS = 40000UL;
constexpr uint32_t ATTENTION_WAIT_MS = 6000UL;
constexpr uint32_t WINDOW_GAZE_MIN_MS = 6000UL;
constexpr uint32_t WINDOW_GAZE_MAX_MS = 10000UL;
constexpr uint32_t WINDOW_GAZE_DELAY_SECONDS =
    2UL * 24UL * 60UL * 60UL;
constexpr uint32_t FEED_FINISH_MS = 650;

constexpr bool windowGazeDue(uint32_t lastExploredAt,
                             uint32_t lastWindowGazeAt,
                             uint32_t nowGameSeconds) {
    return lastWindowGazeAt <= lastExploredAt &&
           nowGameSeconds >= lastExploredAt &&
           nowGameSeconds - lastExploredAt >=
               WINDOW_GAZE_DELAY_SECONDS;
}

static_assert(
    !windowGazeDue(100, 100,
                   100 + WINDOW_GAZE_DELAY_SECONDS - 1) &&
        windowGazeDue(100, 100,
                      100 + WINDOW_GAZE_DELAY_SECONDS) &&
        !windowGazeDue(100, 101,
                       100 + WINDOW_GAZE_DELAY_SECONDS),
    "window gaze runs once after two days without exploration");


PokemonSprites::WalkDirection rotatePetDirection(
    PokemonSprites::WalkDirection direction, int8_t step) {
    int value = static_cast<int>(direction) + step;
    while (value < 0) value += 4;
    while (value >= 4) value -= 4;
    return static_cast<PokemonSprites::WalkDirection>(value);
}


PokemonSprites::WalkDirection petDirectionForDelta(float dx, float dy) {
    if (std::fabs(dx) >= std::fabs(dy)) {
        return dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                          : PokemonSprites::WalkDirection::LEFT;
    }
    return dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                     : PokemonSprites::WalkDirection::UP;
}

}  // namespace

void HomeFlow::reset() {
    state_.action = Action::NONE;
    state_.phase = 0;
    state_.startedMs = state_.untilMs = 0;
    effects_ = {};
}

HomeFlow::Effects HomeFlow::takeEffects() {
    const Effects result = effects_;
    effects_ = {};
    return result;
}

void HomeFlow::face(Direction direction) {
    effects_.directionChanged = true;
    effects_.direction = direction;
}

bool HomeFlow::contains(const Layout& layout, float x, float y) const {
    return RoomMovementArea::containsFootprint(layout.polygon, layout.polygonCount,
        x, y, homeMainActor.geometry.footprint);
}

bool HomeFlow::pathInside(const Layout& layout, float fromX, float fromY,
                          float toX, float toY) const {
    return RoomMovementArea::segmentInsideFootprint(layout.polygon, layout.polygonCount,
        fromX, fromY, toX, toY, homeMainActor.geometry.footprint);
}

bool HomeFlow::tryAmbient(const Layout& layout, Direction direction, uint32_t nowMs) {
    if (state_.action != Action::NONE) return false;
    if (startWindowGaze(layout, nowMs)) return true;
    if (static_cast<int32_t>(nowMs - state_.nextAttentionMs) >= 0 &&
        startAttention(layout, direction, nowMs)) return true;
    return static_cast<int32_t>(nowMs - state_.nextSpecialActionMs) >= 0 &&
        startAutonomousAction(layout, direction, nowMs);
}

void HomeFlow::scheduleAttention(uint32_t nowMs, bool initial) {
    uint32_t minimum = initial ? ATTENTION_INITIAL_MIN_MS : ATTENTION_MIN_MS;
    uint32_t maximum = initial ? ATTENTION_INITIAL_MAX_MS : ATTENTION_MAX_MS;
    uint32_t delayMs = static_cast<uint32_t>(
        GameRandom::range(minimum, maximum + 1));
    if (homeMainActor.behavior.sociabilityBias > 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 - homeMainActor.behavior.sociabilityBias * 14) / 100UL;
    } else if (homeMainActor.behavior.sociabilityBias < 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 + (-homeMainActor.behavior.sociabilityBias) * 18) / 100UL;
    }
    state_.nextAttentionMs = nowMs + delayMs;
}

void HomeFlow::scheduleSpecialAction(uint32_t nowMs) {
    uint32_t delayMs = static_cast<uint32_t>(GameRandom::range(
        SPECIAL_ACTION_MIN_MS, SPECIAL_ACTION_MAX_MS + 1));
    if (homeMainActor.behavior.activityBias > 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 - homeMainActor.behavior.activityBias * 12) / 100UL;
    } else if (homeMainActor.behavior.activityBias < 0) {
        delayMs = delayMs * static_cast<uint32_t>(
            100 + (-homeMainActor.behavior.activityBias) * 16) / 100UL;
    }
    state_.nextSpecialActionMs = nowMs + delayMs;
}

bool HomeFlow::chooseNearbyPose(const Layout& layout, float originX, float originY,
                                 float minDistance, float maxDistance,
                                 float preferredAngle, float angleSpread,
                                 float& x, float& y) const {
    for (uint8_t attempt = 0; attempt < 18; ++attempt) {
        const float unit = static_cast<float>(
            GameRandom::random(-1000, 1001)) / 1000.0f;
        const float angle = preferredAngle + unit * angleSpread;
        const float distance = minDistance + (maxDistance - minDistance) *
            (static_cast<float>(GameRandom::random(0, 1001)) / 1000.0f);
        const float candidateX = originX + std::cos(angle) * distance;
        const float candidateY = originY + std::sin(angle) * distance;
        if (!homeRuntime.targetAvailable(0, candidateX, candidateY) ||
            !pathInside(layout, originX, originY,
                                   candidateX, candidateY)) {
            continue;
        }
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool HomeFlow::chooseAttentionPose(const Layout& layout, float& x, float& y) const {
    const int minimumX = static_cast<int>(layout.bounds.minX);
    const int maximumX = static_cast<int>(layout.bounds.maxX);
    const int minimumY = static_cast<int>(layout.bounds.minY);
    const int maximumY = static_cast<int>(layout.bounds.maxY);
    const float desiredX = (minimumX + maximumX) * 0.5f;
    float bestScore = 1000000.0f;
    bool found = false;
    for (int candidateY = maximumY; candidateY >= minimumY;
         candidateY -= 3) {
        for (int candidateX = minimumX; candidateX <= maximumX;
             candidateX += 3) {
            if (!contains(layout, candidateX, candidateY) ||
                !homeRuntime.targetAvailable(0, candidateX, candidateY)) continue;
            const float score = (maximumY - candidateY) * 5.0f +
                                std::fabs(candidateX - desiredX);
            if (score >= bestScore) continue;
            x = static_cast<float>(candidateX);
            y = static_cast<float>(candidateY);
            bestScore = score;
            found = true;
        }
        if (found && maximumY - candidateY >= 9) break;
    }
    return found;
}

bool HomeFlow::chooseCirclePose(const Layout& layout, uint8_t phase, float& x, float& y) const {
    static constexpr float X_SCALE[] = {1.0f, 0.0f, -1.0f, 0.0f, 0.0f};
    static constexpr float Y_SCALE[] = {0.0f, 0.62f, 0.0f, -0.62f, 0.0f};
    if (phase >= 5) return false;
    x = state_.originX + X_SCALE[phase] * state_.radius;
    y = state_.originY + Y_SCALE[phase] * state_.radius;
    return contains(layout, x, y);
}

bool HomeFlow::chooseWindowGazePose(
    const Layout& layout, float& x, float& y) const {
    static constexpr int8_t OFFSETS[][2] = {
        {0, 0}, {-3, 0}, {3, 0}, {-6, 0}, {6, 0},
        {0, 3}, {-3, 3}, {3, 3}, {0, -3}, {-3, -3}, {3, -3},
        {-9, 0}, {9, 0}, {0, 6}, {0, -6},
    };
    for (const auto& offset : OFFSETS) {
        const float candidateX = layout.windowX + offset[0];
        const float candidateY = layout.windowY + offset[1];
        if (!contains(layout, candidateX, candidateY)) continue;
        x = candidateX;
        y = candidateY;
        return true;
    }
    return false;
}

bool HomeFlow::beginLeg(float x, float y, uint32_t nowMs) {
    if (!homeRuntime.transition(
            0, Home::Task::ROOM_ACTION, nowMs, 0, true) ||
        !homeRuntime.planRoute(
            0, x, y, false, homeCompanionActor.active)) {
        return false;
    }
    homeMainActor.targetX = x;
    homeMainActor.targetY = y;
    float waypointX = x;
    float waypointY = y;
    homeMainActor.route.current(waypointX, waypointY);
    face(petDirectionForDelta(
        waypointX - homeMainActor.x, waypointY - homeMainActor.y));
    homeMainActor.motion = Home::MotionPhase::ROUTE;
    effects_.stopAnimation = true;
    effects_.resetFrame = true;
    effects_.startWalk = true;
    return true;
}

bool HomeFlow::beginMove(Action action, float x, float y,
                                    uint32_t nowMs) {
    state_.action = action;
    state_.phase = 0;
    state_.startedMs = nowMs;
    state_.originX = homeMainActor.x;
    state_.originY = homeMainActor.y;
    if (beginLeg(x, y, nowMs)) return true;
    state_.action = Action::NONE;
    homeRuntime.stop(0, nowMs, 700);
    homeMainActor.motion = Home::MotionPhase::STILL;
    effects_.stopAnimation = true;
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    return false;
}

void HomeFlow::startFeedFinish(uint32_t nowMs) {
    state_.action = Action::FEED_FINISH;
    state_.phase = 0;
    state_.startedMs = nowMs;
    state_.untilMs = nowMs + FEED_FINISH_MS;
    homeMainActor.motion = Home::MotionPhase::STILL;
    effects_.stopAnimation = true;
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    effects_.roomChanged = true;
}

bool HomeFlow::startAttention(const Layout& layout, Direction direction, uint32_t nowMs) {
    float x = 0.0f;
    float y = 0.0f;
    scheduleAttention(nowMs);
    if (!chooseAttentionPose(layout, x, y)) return false;
    if (std::fabs(x - homeMainActor.x) < 3.0f && std::fabs(y - homeMainActor.y) < 3.0f) {
        state_.action = Action::ATTENTION_WAIT;
        state_.startedMs = nowMs;
        state_.untilMs = nowMs + ATTENTION_WAIT_MS;
        state_.baseDirection = direction;
        face(PokemonSprites::WalkDirection::DOWN);
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.stopAnimation = true;
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.heart = Heart::SHOW;
        effects_.heartsUntilMs = state_.untilMs;
        effects_.roomChanged = true;
        return true;
    }
    return beginMove(Action::ATTENTION_APPROACH, x, y, nowMs);
}

bool HomeFlow::startWindowGaze(const Layout& layout, uint32_t nowMs) {
    if (!needsWindowAnchor() || !layout.hasWindow) return false;
    float x = 0.0f;
    float y = 0.0f;
    if (!chooseWindowGazePose(layout, x, y)) return false;
    state_.windowDirection = layout.windowDirection;
    return beginMove(Action::WINDOW_APPROACH, x, y, nowMs);
}

bool HomeFlow::needsWindowAnchor() const {
    const Game::MonsterRuntime& monster = gameState.team[0];
    const uint32_t gameSeconds =
        static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
    return windowGazeDue(monster.lastExploredAt,
                         monster.lastWindowGazeAt, gameSeconds);
}

bool HomeFlow::startAutonomousAction(const Layout& layout, Direction direction, uint32_t nowMs) {
    scheduleSpecialAction(nowMs);
    PokemonSprites::PetAnimationProfile animation{};
    const bool energeticAllowed =
        homeMainActor.behavior.movementMode != MonsterMovementMode::STATIONARY &&
        PokemonSprites::petAnimationProfile(
            gameState.team[0].speciesId, animation) &&
        animation.walkingFrames >= 2;
    const int roll = static_cast<int>(GameRandom::random(100));
    Action selected = Action::LOOK_AROUND;
    if (homeMainActor.behavior.activityBias >= 1) {
        if (energeticAllowed && roll < 26) selected = Action::DASH;
        else if (energeticAllowed && roll < 50) selected = Action::CIRCLE;
        else if (roll < 64) selected = Action::STEP_BACK;
        else if (roll < 84) selected = Action::LOOK_AROUND;
        else selected = Action::QUIET_GAZE;
    } else if (homeMainActor.behavior.activityBias <= -1) {
        if (roll < 38) selected = Action::QUIET_GAZE;
        else if (roll < 72) selected = Action::LOOK_AROUND;
        else if (roll < 92 || !energeticAllowed) selected = Action::STEP_BACK;
        else if (roll < 98) selected = Action::CIRCLE;
        else selected = Action::DASH;
    } else {
        if (roll < 27) selected = Action::LOOK_AROUND;
        else if (roll < 50) selected = Action::QUIET_GAZE;
        else if (roll < 68 || !energeticAllowed) selected = Action::STEP_BACK;
        else if (roll < 84) selected = Action::CIRCLE;
        else selected = Action::DASH;
    }

    state_.startedMs = nowMs;
    state_.baseDirection = direction;
    if (selected == Action::LOOK_AROUND ||
        selected == Action::QUIET_GAZE) {
        state_.action = selected;
        state_.phase = 0;
        state_.untilMs = nowMs + static_cast<uint32_t>(GameRandom::range(
            selected == Action::LOOK_AROUND ? 1100 : 1400,
            selected == Action::LOOK_AROUND ? 1801 : 2801));
        if (selected == Action::QUIET_GAZE) {
            face(PokemonSprites::WalkDirection::DOWN);
        }
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.roomChanged = true;
        return true;
    }

    if (selected == Action::DASH || selected == Action::STEP_BACK) {
        float x = 0.0f;
        float y = 0.0f;
        const bool dash = selected == Action::DASH;
        const float preferred = dash
            ? static_cast<float>(GameRandom::random(0, 6284)) / 1000.0f
            : -1.57079633f;
        if (chooseNearbyPose(
                layout,                 homeMainActor.x, homeMainActor.y, dash ? 20.0f : 7.0f,
                dash ? 32.0f : 12.0f, preferred,
                dash ? 2.35619449f : 0.55f, x, y) &&
            beginMove(selected, x, y, nowMs)) {
            return true;
        }
    } else if (selected == Action::CIRCLE) {
        state_.action = Action::CIRCLE;
        state_.phase = 0;
        state_.startedMs = nowMs;
        state_.originX = homeMainActor.x;
        state_.originY = homeMainActor.y;
        for (int radius = 11; radius >= 6; --radius) {
            state_.radius = static_cast<float>(radius);
            bool valid = true;
            float previousX = state_.originX;
            float previousY = state_.originY;
            for (uint8_t phase = 0; phase < 5; ++phase) {
                float checkX = 0.0f;
                float checkY = 0.0f;
                if (!chooseCirclePose(layout, phase, checkX, checkY) ||
                    !pathInside(
                        layout,                         previousX, previousY, checkX, checkY)) {
                    valid = false;
                    break;
                }
                previousX = checkX;
                previousY = checkY;
            }
            if (!valid) continue;
            float x = 0.0f;
            float y = 0.0f;
            chooseCirclePose(layout, 0, x, y);
            if (beginLeg(x, y, nowMs)) return true;
            break;
        }
        state_.action = Action::NONE;
    }

    state_.action = Action::LOOK_AROUND;
    state_.phase = 0;
    state_.startedMs = nowMs;
    state_.baseDirection = direction;
    state_.untilMs = nowMs + 1200;
    homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
    homeMainActor.motion = Home::MotionPhase::STILL;
    effects_.roomChanged = true;
    return true;
}

void HomeFlow::finishMovement(const Layout& layout, bool blocksFood, uint32_t nowMs) {
    switch (state_.action) {
    case Action::ATTENTION_APPROACH:
        state_.action = Action::ATTENTION_WAIT;
        state_.startedMs = nowMs;
        state_.untilMs = nowMs + ATTENTION_WAIT_MS;
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.stopAnimation = true;
        face(PokemonSprites::WalkDirection::DOWN);
        effects_.heart = Heart::SHOW;
        effects_.heartsUntilMs = state_.untilMs;
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        homeMainActor.motion = Home::MotionPhase::STILL;
        return;
    case Action::WINDOW_APPROACH:
        state_.action = Action::WINDOW_WAIT;
        state_.startedMs = nowMs;
        state_.untilMs = nowMs + static_cast<uint32_t>(GameRandom::range(
            WINDOW_GAZE_MIN_MS, WINDOW_GAZE_MAX_MS + 1));
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.stopAnimation = true;
        face(state_.windowDirection);
        homeRuntime.transition(0, Home::Task::ROOM_ACTION, nowMs, 0, true);
        homeMainActor.motion = Home::MotionPhase::STILL;
        gameState.team[0].lastWindowGazeAt =
            static_cast<uint32_t>(gameState.gameMinutesTotal * 60UL);
        effects_.save = true;
        return;
    case Action::CIRCLE:
        ++state_.phase;
        if (state_.phase < 5) {
            float x = 0.0f;
            float y = 0.0f;
            if (chooseCirclePose(layout, state_.phase, x, y) &&
                beginLeg(x, y, nowMs)) {
                return;
            }
        }
        break;
    case Action::DASH:
    case Action::STEP_BACK:
    default:
        break;
    }
    finish(blocksFood, nowMs);
}

HomeFlow::Update HomeFlow::update(const Layout& layout, uint32_t nowMs, float elapsedSeconds, bool blocksFood) {
    if (state_.action == Action::NONE) return {};
    if (state_.action == Action::FEED_FINISH && state_.phase == 0) {
        if (static_cast<int32_t>(nowMs - state_.untilMs) < 0) return {true};
        state_.phase = 1;
        const float awayAngle = std::atan2(homeMainActor.y - layout.foodY, homeMainActor.x - layout.foodX);
        float x = 0.0f;
        float y = 0.0f;
        if (chooseNearbyPose(layout, homeMainActor.x, homeMainActor.y, 32.0f, 48.0f, awayAngle,
                             3.14159265f * 0.42f, x, y) &&
            beginLeg(x, y, nowMs)) {
            return {true};
        }
        finish(blocksFood, nowMs);
        return {true};
    }
    if (state_.action == Action::ATTENTION_WAIT ||
        state_.action == Action::QUIET_GAZE ||
        state_.action == Action::WINDOW_WAIT) {
        if (static_cast<int32_t>(nowMs - state_.untilMs) >= 0) {
            finish(blocksFood, nowMs);
        }
        return {true};
    }
    if (state_.action == Action::LOOK_AROUND) {
        const uint8_t phase = static_cast<uint8_t>(std::min<uint32_t>(
            2, (nowMs - state_.startedMs) / 380UL));
        if (phase != state_.phase) {
            static constexpr int8_t STEPS[] = {-1, 1, 0};
            state_.phase = phase;
            face(rotatePetDirection(
                state_.baseDirection, STEPS[phase]));
            effects_.roomChanged = true;
        }
        if (static_cast<int32_t>(nowMs - state_.untilMs) >= 0) {
            face(state_.baseDirection);
            finish(blocksFood, nowMs);
        }
        return {true};
    }

    float waypointX = homeMainActor.targetX;
    float waypointY = homeMainActor.targetY;
    homeMainActor.route.current(waypointX, waypointY);
    face(petDirectionForDelta(
        waypointX - homeMainActor.x, waypointY - homeMainActor.y));
    const float previousY = homeMainActor.y;
    const float speed = (state_.action == Action::DASH ? 16.3f : 10.5f) *
                        homeMainActor.behavior.moveSpeedScale;
    const Home::RouteStep step = homeRuntime.advanceRoute(
        0, nowMs, speed, elapsedSeconds, 0.8f,
        homeCompanionActor.active);
    return {true, true, previousY, step};
}

void HomeFlow::completeMovement(const Layout& layout, Home::RouteStep step,
                                bool blocksFood, uint32_t nowMs) {
    if (step == Home::RouteStep::ARRIVED) {
        finishMovement(layout, blocksFood, nowMs);
    } else if (step == Home::RouteStep::BLOCKED ||
               step == Home::RouteStep::NO_ROUTE) {
        finish(blocksFood, nowMs);
    }
}

void HomeFlow::finish(bool blocksFood, uint32_t nowMs) {
    const Action finishedAction = state_.action;
    if (finishedAction == Action::FEED_FINISH &&
        blocksFood) {
        state_.phase = 0;
        state_.startedMs = nowMs;
        state_.untilMs = nowMs + 300;
        homeRuntime.stop(0, nowMs, 300);
        homeMainActor.motion = Home::MotionPhase::STILL;
        effects_.stopAnimation = true;
        homeMainActor.targetX = homeMainActor.x;
        homeMainActor.targetY = homeMainActor.y;
        effects_.resetFrame = true;
        effects_.roomChanged = true;
        return;
    }
    state_.action = Action::NONE;
    state_.phase = 0;
    state_.startedMs = 0;
    state_.untilMs = 0;
    homeRuntime.stop(0, nowMs, static_cast<uint32_t>(
        GameRandom::range(650, 1201)));
    if (finishedAction == Action::FEED_FINISH) {
        homeRuntime.finishBowlClearance(0, nowMs);
    }
    homeMainActor.motion = Home::MotionPhase::STILL;
    effects_.stopAnimation = true;
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    effects_.resetFrame = true;
    homeMainActor.mind.onActivity(nowMs);
    effects_.scheduleDecision = true;
    effects_.roomChanged = true;
}

void HomeFlow::cancel(bool blocksFood, uint32_t nowMs) {
    if (state_.action == Action::NONE) return;
    const bool ownedHeart =
        state_.action == Action::ATTENTION_APPROACH ||
        state_.action == Action::ATTENTION_WAIT;
    finish(blocksFood, nowMs);
    if (ownedHeart) effects_.heart = Heart::CLEAR;
}

}  // namespace AmoledV1
