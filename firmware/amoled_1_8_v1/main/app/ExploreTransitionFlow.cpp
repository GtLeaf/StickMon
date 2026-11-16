#include "ExploreTransitionFlow.h"
#include "core/RoomMovementArea.h"
#include "game/ExploreDestinations.h"
#include <algorithm>
#include <cmath>

namespace AmoledV1 {
namespace {
constexpr uint32_t PHASE_TIMEOUT_MS = 8000;
constexpr float CROSS_SPEED = 42.0f, WALK_SPEED = 30.0f;
bool moveTowards(Home::Actor &actor, float step) {
    const float dx = actor.targetX - actor.x, dy = actor.targetY - actor.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.8f || step >= distance) {
        actor.x = actor.targetX;
        actor.y = actor.targetY;
        return true;
    }
    if (step > 0) {
        actor.x += dx / distance * step;
        actor.y += dy / distance * step;
    }
    return false;
}
} // namespace

bool ExploreTransitionFlow::mainFainted() const {
    return game_.team[0].fainted || game_.team[0].hpCur == 0;
}
bool ExploreTransitionFlow::companionHealthy() const {
    return game_.teamCount > 1 && companion_.active && game_.team[1].speciesId != 0 &&
           !game_.team[1].fainted && game_.team[1].hpCur > 0;
}
void ExploreTransitionFlow::face(Visual &v, float dx, float dy) {
    v.directionChanged = true;
    v.direction =
        std::fabs(dx) >= std::fabs(dy)
            ? (dx >= 0 ? PokemonSprites::WalkDirection::RIGHT : PokemonSprites::WalkDirection::LEFT)
            : (dy >= 0 ? PokemonSprites::WalkDirection::DOWN : PokemonSprites::WalkDirection::UP);
}
void ExploreTransitionFlow::advanceFrame(Visual &v) {
    v.frame = v.frame == Frame::RESET ? Frame::RESET_ADVANCE : Frame::ADVANCE;
}
ExploreTransitionFlow::Effects ExploreTransitionFlow::takeEffects() {
    const Effects result = effects_;
    effects_ = Effects{};
    return result;
}
void ExploreTransitionFlow::reset() {
    state_ = State{};
    fade_.reset();
    effects_ = Effects{};
    awaiting_ = Request::NONE;
}
bool ExploreTransitionFlow::queue(uint8_t area, bool autoWalk) {
    if (fade_.active())
        return false;
    fade_.reset();
    state_.area = area;
    state_.autoWalk = autoWalk;
    state_.pending = true;
    state_.phase = Phase::NONE;
    clearVisibility();
    awaiting_ = Request::NONE;
    return true;
}
void ExploreTransitionFlow::clearVisibility() {
    state_.mainHidden = state_.companionHidden = state_.companionDeparting = false;
}
void ExploreTransitionFlow::revealCompanionIfIdle() {
    if (!state_.pending && state_.phase == Phase::NONE)
        state_.companionHidden = false;
}
void ExploreTransitionFlow::cancel(uint32_t nowMs) {
    fade_.reset();
    awaiting_ = Request::NONE;
    // A superseded, unconsumed request must not run after cancellation.
    effects_ = Effects{};
    if (state_.phase == Phase::CROSS_DOOR || state_.phase == Phase::WALK_COMPANION_TO_DOOR ||
        state_.phase == Phase::CROSS_COMPANION_DOOR) {
        main_.x = state_.insideX;
        main_.y = state_.insideY;
    }
    if (state_.companionDeparting)
        simulation_.stop(1, nowMs);
    clearVisibility();
    state_.pending = false;
    state_.phase = Phase::NONE;
    simulation_.stop(0, nowMs);
    main_.motion = Home::MotionPhase::STILL;
    main_.targetX = main_.x;
    main_.targetY = main_.y;
    effects_.stopMainAnimation = true;
    effects_.main.frame = Frame::IDLE;
    effects_.endSession = effects_.camera = effects_.roomRender = true;
}
bool ExploreTransitionFlow::beginReturn() {
    if (state_.phase != Phase::NONE)
        return false;
    state_.phase = Phase::RETURN_FADE_OUT;
    awaiting_ = Request::NONE;
    fade_.beginOut();
    return true;
}
void ExploreTransitionFlow::startFadeOut() {
    state_.phase = Phase::FADE_OUT;
    fade_.beginOut();
    effects_.fullRender = true;
}
void ExploreTransitionFlow::startCompanion(uint32_t nowMs) {
    companion_.targetX = state_.companionInsideX;
    companion_.targetY = state_.companionInsideY;
    simulation_.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
    companion_.route.clear();
    face(effects_.companion, companion_.targetX - companion_.x, companion_.targetY - companion_.y);
    effects_.companion.frame = Frame::RESET;
    effects_.companion.clockNow = effects_.motionClock = true;
    state_.phase = Phase::WALK_COMPANION_TO_DOOR;
    effects_.roomRender = true;
}
void ExploreTransitionFlow::destinationStarted(bool started) {
    if (awaiting_ != Request::START_DESTINATION)
        return;
    awaiting_ = Request::NONE;
    state_.pending = false;
    state_.phase = Phase::FADE_IN;
    fade_.beginIn();
    if (!started) {
        state_.mainHidden = state_.companionHidden = false;
        main_.x = main_.targetX = state_.insideX;
        main_.y = main_.targetY = state_.insideY;
        effects_.camera = true;
    }
    effects_.fullRender = true;
}
bool ExploreTransitionFlow::contains(const Layout &layout, const Home::Actor &actor, float x,
                                     float y) {
    return RoomMovementArea::containsFootprint(layout.polygon, layout.polygonCount, x, y,
                                               actor.geometry.footprint);
}
void ExploreTransitionFlow::returnSettled(const Layout &layout, uint32_t nowMs) {
    if (awaiting_ != Request::SETTLE_RETURN)
        return;
    awaiting_ = Request::NONE;
    state_.mainHidden = state_.companionHidden = false;
    const float outsideX = layout.available ? layout.outsideX : main_.x;
    const float outsideY = layout.available ? layout.outsideY : main_.y;
    state_.insideX = layout.available ? layout.insideX : main_.x;
    state_.insideY = layout.available ? layout.insideY : main_.y;
    auto chooseInside = [&](const Home::Actor &actor, float &x, float &y) {
        if (contains(layout, actor, x, y))
            return;
        const float anchorX = x, anchorY = y;
        for (int radius = 2; radius <= 48; radius += 2) {
            for (int offset = -radius; offset <= radius; offset += 2) {
                const float candidates[][2] = {{anchorX + radius, anchorY + offset},
                                               {anchorX - radius, anchorY + offset},
                                               {anchorX + offset, anchorY + radius},
                                               {anchorX + offset, anchorY - radius}};
                for (const auto &point : candidates)
                    if (contains(layout, actor, point[0], point[1])) {
                        x = point[0];
                        y = point[1];
                        return;
                    }
            }
        }
    };
    chooseInside(main_, state_.insideX, state_.insideY);
    if (!mainFainted()) {
        main_.faintRestActive = false;
        main_.x = outsideX;
        main_.y = outsideY;
        main_.targetX = state_.insideX;
        main_.targetY = state_.insideY;
        face(effects_.main, main_.targetX - main_.x, main_.targetY - main_.y);
        main_.route.clear();
        main_.velocityX = main_.velocityY = 0;
        simulation_.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
    }
    const bool pair = companionHealthy();
    if (pair) {
        state_.companionInsideX = layout.available ? layout.insideX : state_.insideX;
        state_.companionInsideY = layout.available ? layout.insideY : state_.insideY;
        chooseInside(companion_, state_.companionInsideX, state_.companionInsideY);
        companion_.x = outsideX;
        companion_.y = outsideY;
        companion_.targetX = state_.companionInsideX;
        companion_.targetY = state_.companionInsideY;
        companion_.route.clear();
        companion_.velocityX = companion_.velocityY = 0;
        simulation_.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
        face(effects_.companion, companion_.targetX - outsideX, companion_.targetY - outsideY);
        effects_.companion.frame = Frame::RESET;
        effects_.companion.clockNow = true;
    }
    state_.companionDeparting = pair;
    effects_.placeFainted = true;
    state_.companionHidden = pair;
    if (!mainFainted()) {
        main_.motion = Home::MotionPhase::ROUTE;
        effects_.stopMainAnimation = true;
        effects_.main.frame = Frame::RESET;
    }
    state_.phase = Phase::RETURN_FADE_IN;
    state_.phaseStartedMs = nowMs;
    fade_.beginIn();
    effects_.camera = effects_.fullRender = true;
}
void ExploreTransitionFlow::finishReturn(uint32_t nowMs, bool pair) {
    state_.phase = Phase::NONE;
    effects_.endSession = effects_.interactionClock = effects_.scheduleMain = true;
    main_.mind.onActivity(nowMs);
    effects_.schedulePair = pair;
    effects_.trace = pair ? Trace::RETURNED_PAIR : Trace::RETURNED_SOLO;
}
void ExploreTransitionFlow::prepareClearance(bool found, float x, float y, uint32_t nowMs) {
    const bool ready =
        found && simulation_.planRoute(0, x, y, true, false) &&
        simulation_.transitionPreparedRoute(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
    main_.targetX = x;
    main_.targetY = y;
    state_.phaseStartedMs = nowMs;
    if (ready)
        state_.phase = Phase::RETURN_CLEAR_MAIN;
    else {
        if (found) {
            main_.x = x;
            main_.y = y;
        }
        simulation_.stop(0, nowMs, 700);
        main_.motion = Home::MotionPhase::STILL;
        effects_.stopMainAnimation = true;
        state_.companionHidden = false;
        state_.phase = Phase::RETURN_COMPANION_WALK_IN;
        effects_.trace = Trace::CLEARANCE_FALLBACK;
    }
    effects_.camera = effects_.fullRender = true;
}
void ExploreTransitionFlow::clearanceChosen(bool found, float x, float y, uint32_t nowMs) {
    if (awaiting_ != Request::FIND_CLEARANCE)
        return;
    awaiting_ = Request::NONE;
    prepareClearance(found, x, y, nowMs);
}
void ExploreTransitionFlow::chooseClearance(const Layout &layout, uint32_t nowMs) {
    float inwardX = state_.insideX - state_.outsideX, inwardY = state_.insideY - state_.outsideY;
    const float length = std::sqrt(inwardX * inwardX + inwardY * inwardY);
    if (length > 0.001f) {
        inwardX /= length;
        inwardY /= length;
    } else {
        inwardX = 0;
        inwardY = -1;
    }
    const float separation = std::max(26.0f, main_.geometry.footprint.radiusX +
                                                 companion_.geometry.footprint.radiusX + 8.0f);
    for (float offset : {14.0f, 20.0f, 28.0f})
        for (float scale : {1.0f, -1.0f, 1.25f, -1.25f}) {
            const float x = state_.insideX + inwardX * offset - inwardY * separation * scale;
            const float y = state_.insideY + inwardY * offset + inwardX * separation * scale;
            if (contains(layout, main_, x, y)) {
                prepareClearance(true, x, y, nowMs);
                return;
            }
        }
    // App invokes its existing random goal picker only here, preserving RNG order.
    awaiting_ = effects_.request = Request::FIND_CLEARANCE;
    effects_.clearanceSeparation = separation;
}

bool ExploreTransitionFlow::update(const Layout &layout, const Environment &env, uint32_t nowMs) {
    if (!state_.pending && state_.phase == Phase::NONE)
        return false;
    if (awaiting_ != Request::NONE)
        return true;
    fade_.update(nowMs);
    const auto phase = state_.phase;
    const bool departing = phase == Phase::NONE || phase == Phase::WALK_TO_DOOR ||
                           phase == Phase::CROSS_DOOR || phase == Phase::WALK_COMPANION_TO_DOOR ||
                           phase == Phase::CROSS_COMPANION_DOOR || phase == Phase::FADE_OUT;
    if (departing && !env.atHome)
        effects_.goHome = effects_.fullRender = true;
    if (phase == Phase::RETURN_FADE_OUT || phase == Phase::FADE_OUT) {
        if (fade_.complete()) {
            awaiting_ = effects_.request =
                phase == Phase::FADE_OUT ? Request::START_DESTINATION : Request::SETTLE_RETURN;
        } else
            effects_.fullRender = true;
        return true;
    }
    if (phase == Phase::FADE_IN || phase == Phase::RETURN_FADE_IN) {
        if (fade_.complete()) {
            fade_.reset();
            if (phase == Phase::FADE_IN) {
                state_.phase = Phase::NONE;
                effects_.interactionClock = true;
                effects_.endSession = env.atHome;
            } else {
                if (mainFainted()) {
                    state_.companionHidden = false;
                    state_.phase =
                        state_.companionDeparting ? Phase::RETURN_COMPANION_WALK_IN : Phase::NONE;
                    if (!state_.companionDeparting)
                        effects_.endSession = effects_.interactionClock = true;
                } else
                    state_.phase = Phase::RETURN_WALK_IN;
                effects_.motionClock = effects_.main.clockNow = true;
                state_.phaseStartedMs = nowMs;
            }
        }
        effects_.fullRender = true;
        return true;
    }
    if (phase == Phase::RETURN_WALK_IN || phase == Phase::RETURN_CLEAR_MAIN ||
        phase == Phase::RETURN_COMPANION_WALK_IN) {
        const float seconds = std::min<uint32_t>(nowMs - env.lastMotionMs, 120) / 1000.0f;
        effects_.motionClock = true;
        const bool timedOut = nowMs - state_.phaseStartedMs >= PHASE_TIMEOUT_MS;
        if (phase == Phase::RETURN_WALK_IN) {
            bool arrived = moveTowards(main_, CROSS_SPEED * seconds);
            if (!arrived && timedOut) {
                main_.x = main_.targetX;
                main_.y = main_.targetY;
                arrived = true;
                effects_.trace = Trace::LEADER_TIMEOUT;
            }
            advanceFrame(effects_.main);
            if (arrived && !state_.companionDeparting) {
                main_.motion = Home::MotionPhase::STILL;
                effects_.stopMainAnimation = true;
                simulation_.stop(0, nowMs, 700);
                finishReturn(nowMs, false);
            } else if (arrived)
                chooseClearance(layout, nowMs);
        } else if (phase == Phase::RETURN_CLEAR_MAIN) {
            const float x = main_.x, y = main_.y;
            auto step = simulation_.advanceRoute(0, nowMs, WALK_SPEED, seconds, 1.0f, false);
            if (step == Home::RouteStep::BLOCKED || step == Home::RouteStep::NO_ROUTE || timedOut) {
                main_.x = main_.targetX;
                main_.y = main_.targetY;
                main_.route.clear();
                step = Home::RouteStep::ARRIVED;
                effects_.trace = Trace::CLEARANCE_TIMEOUT;
            }
            if (std::fabs(main_.x - x) > 0.01f || std::fabs(main_.y - y) > 0.01f)
                face(effects_.main, main_.x - x, main_.y - y);
            advanceFrame(effects_.main);
            if (step == Home::RouteStep::ARRIVED) {
                simulation_.stop(0, nowMs, 700);
                main_.motion = Home::MotionPhase::STILL;
                effects_.stopMainAnimation = true;
                state_.companionHidden = false;
                state_.phase = Phase::RETURN_COMPANION_WALK_IN;
                state_.phaseStartedMs = nowMs;
            }
        } else {
            const float dx = companion_.targetX - companion_.x,
                        dy = companion_.targetY - companion_.y;
            const float distance = std::sqrt(dx * dx + dy * dy), step = CROSS_SPEED * seconds;
            bool arrived = distance <= 0.8f || step >= distance;
            if (!arrived && timedOut) {
                arrived = true;
                effects_.trace = Trace::COMPANION_TIMEOUT;
            }
            if (arrived) {
                companion_.x = companion_.targetX;
                companion_.y = companion_.targetY;
            } else if (distance > 0 && step > 0) {
                companion_.x += dx / distance * step;
                companion_.y += dy / distance * step;
                face(effects_.companion, dx, dy);
            }
            companion_.velocityX = companion_.velocityY = 0;
            advanceFrame(effects_.companion);
            if (arrived) {
                simulation_.stop(1, nowMs, 700);
                effects_.companion.frame = Frame::IDLE;
                state_.companionDeparting = false;
                finishReturn(nowMs, true);
            }
        }
        // Clearance fallback must be acknowledged before presentation effects.
        if (awaiting_ != Request::FIND_CLEARANCE)
            effects_.camera = effects_.fullRender = true;
        return true;
    }
    if (phase == Phase::NONE) {
        if (!layout.available || !layout.doorValid) {
            startFadeOut();
            return true;
        }
        state_.insideX = layout.insideX;
        state_.insideY = layout.insideY;
        state_.outsideX = layout.outsideX;
        state_.outsideY = layout.outsideY;
        state_.companionDeparting = companionHealthy();
        if (state_.companionDeparting) {
            state_.companionInsideX = state_.insideX;
            state_.companionInsideY = state_.insideY;
            state_.companionOutsideX = state_.outsideX;
            state_.companionOutsideY = state_.outsideY;
            simulation_.transition(1, Home::Task::DOOR_ACTION, nowMs, 0, true);
        }
        if (mainFainted()) {
            if (state_.companionDeparting)
                startCompanion(nowMs);
            else if (state_.area == ExploreDestinations::MARKET)
                startFadeOut();
            else {
                cancel(nowMs);
                effects_.placeFainted = effects_.petRestNotice = true;
            }
            return true;
        }
        main_.faintRestActive = false;
        main_.taskUntilMs = 0;
        effects_.clearFeedingClock = true;
        main_.targetX = state_.insideX;
        main_.targetY = state_.insideY;
        main_.route.clear();
        main_.velocityX = main_.velocityY = 0;
        simulation_.transition(0, Home::Task::DOOR_ACTION, nowMs, 0, true);
        face(effects_.main, main_.targetX - main_.x, main_.targetY - main_.y);
        effects_.mainLongMove = effects_.stopMainAnimation = true;
        main_.motion = Home::MotionPhase::ROUTE;
        effects_.main.frame = Frame::RESET;
        state_.phase = Phase::WALK_TO_DOOR;
        effects_.roomRender = true;
    }
    const bool companion = state_.phase == Phase::WALK_COMPANION_TO_DOOR ||
                           state_.phase == Phase::CROSS_COMPANION_DOOR;
    const bool crossing =
        state_.phase == Phase::CROSS_DOOR || state_.phase == Phase::CROSS_COMPANION_DOOR;
    const float speed = crossing ? CROSS_SPEED : WALK_SPEED;
    uint32_t elapsed = nowMs - env.lastMotionMs;
    if (env.lastMotionMs == 0 || nowMs < env.lastMotionMs)
        elapsed = 0;
    effects_.motionClock = true;
    const float step = speed * (std::min<uint32_t>(elapsed, 120) / 1000.0f);
    auto &actor = companion ? companion_ : main_;
    auto &visual = companion ? effects_.companion : effects_.main;
    const float dx = actor.targetX - actor.x, dy = actor.targetY - actor.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.8f || step >= distance) {
        actor.x = actor.targetX;
        actor.y = actor.targetY;
        if (companion)
            actor.velocityX = actor.velocityY = 0;
        if (!crossing) {
            actor.targetX = companion ? state_.companionOutsideX : state_.outsideX;
            actor.targetY = companion ? state_.companionOutsideY : state_.outsideY;
            face(visual, actor.targetX - actor.x, actor.targetY - actor.y);
            state_.phase = companion ? Phase::CROSS_COMPANION_DOOR : Phase::CROSS_DOOR;
            visual.frame = Frame::RESET;
            effects_.roomRender = true;
            return true;
        }
        if (companion) {
            state_.companionHidden = true;
            companion_.route.clear();
            state_.companionDeparting = false;
            startFadeOut();
        } else {
            state_.mainHidden = true;
            main_.motion = Home::MotionPhase::STILL;
            effects_.stopMainAnimation = true;
            main_.targetX = main_.x;
            main_.targetY = main_.y;
            if (state_.companionDeparting)
                startCompanion(nowMs);
            else
                startFadeOut();
        }
        return true;
    }
    if (distance > 0 && step > 0) {
        actor.x += dx / distance * step;
        actor.y += dy / distance * step;
        if (companion)
            face(visual, dx, dy);
    }
    if (!companion || step > 0) {
        actor.velocityX = distance > 0 ? dx / distance * speed : 0;
        actor.velocityY = distance > 0 ? dy / distance * speed : 0;
    }
    advanceFrame(visual);
    effects_.camera = effects_.roomRender = true;
    return true;
}
} // namespace AmoledV1
