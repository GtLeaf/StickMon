#include "PairFlow.h"
#include "game/GameRandom.h"
#include "game/HomeActorController.h"
#include "game/CareTicker.h"
#include "game/SpeciesBehavior.h"
#include <algorithm>
#include <cmath>

namespace AmoledV1 {
namespace {
constexpr uint32_t PAIR_INTERACTION_MIN_INTERVAL_MS = 90000UL;
constexpr uint32_t PAIR_INTERACTION_MAX_INTERVAL_MS = 180000UL;
constexpr uint32_t PAIR_INTERACTION_RETRY_MS = 5000UL;
constexpr uint32_t PAIR_TALK_TIMEOUT_MS = 9000UL;
constexpr uint32_t PAIR_CHASE_TIMEOUT_MS = 24000UL;
constexpr uint16_t PAIR_INVITE_MS = 750;
constexpr uint16_t PAIR_CELEBRATE_MS = 1200;
constexpr uint16_t PAIR_TALK_HOP_MS = 560;
constexpr uint16_t PAIR_TALK_GAP_MS = 180;
constexpr uint16_t PAIR_TALK_END_PAUSE_MS = 240;
constexpr uint16_t PAIR_TALK_TOTAL_MS =
    PAIR_TALK_HOP_MS * 2 + PAIR_TALK_GAP_MS + PAIR_TALK_END_PAUSE_MS;
constexpr uint8_t PAIR_CHASE_LEGS = 4;
constexpr float PAIR_APPROACH_DISTANCE = 20.0f;
constexpr float PAIR_APPROACH_SPEED = 12.5f;
constexpr float PAIR_APPROACH_GOAL_TOLERANCE = 1.0f;
constexpr float PAIR_CHASE_LEADER_SPEED = 18.0f;
constexpr float PAIR_CHASE_FOLLOWER_SPEED = 17.0f;
float pairRouteDistance(const Home::Actor& actor) {
    float routeDistance = 0.0f;
    float fromX = actor.x;
    float fromY = actor.y;
    for (uint8_t index = actor.route.index;
         index < actor.route.count; ++index) {
        const float toX = actor.route.x[index];
        const float toY = actor.route.y[index];
        routeDistance += std::hypot(toX - fromX, toY - fromY);
        fromX = toX;
        fromY = toY;
    }
    if (actor.route.index >= actor.route.count) {
        routeDistance = std::hypot(
            actor.targetX - actor.x, actor.targetY - actor.y);
    }
    return routeDistance;
}

uint32_t pairApproachBudgetMs(const Home::Actor& actor) {
    const float routeDistance = pairRouteDistance(actor);
    return static_cast<uint32_t>(std::clamp(
        routeDistance / PAIR_APPROACH_SPEED * 1000.0f + 1800.0f,
        4000.0f, 20000.0f));
}
}  // namespace

bool PairFlow::eligible(const Environment& environment) const {
    if (!environment.ordinaryStartAllowed || gameState.teamCount < 2 ||
        !homeCompanionActor.active || homeCompanionActor.task != Home::Task::IDLE) return false;
    const auto first = Game::speciesCareProfileFor(gameState.team[0].speciesId);
    const auto second = Game::speciesCareProfileFor(gameState.team[1].speciesId);
    return Home::ActorController::pairEligible(
        gameState.team[0], gameState.team[1],
        first.usesBed && Game::isSleepCareTime(gameState.gameMinutesTotal, gameState.team[0].nature),
        second.usesBed && Game::isSleepCareTime(gameState.gameMinutesTotal, gameState.team[1].nature));
}

bool PairFlow::forcedTalkAllowed(const Environment& environment) const {
    return environment.forcedTalkAllowed && gameState.teamCount >= 2 &&
        homeCompanionActor.active && !gameState.team[0].fainted && gameState.team[0].hpCur > 0 &&
        !gameState.team[1].fainted && gameState.team[1].hpCur > 0;
}

bool PairFlow::needsStart(uint32_t nowMs) const {
    return state_.phase == Phase::NONE && static_cast<int32_t>(nowMs - state_.nextInteractionMs) >= 0;
}

void PairFlow::schedule(uint32_t nowMs, bool immediate) {
    state_.nextInteractionMs = immediate ? nowMs + 300 : nowMs + static_cast<uint32_t>(
        GameRandom::range(PAIR_INTERACTION_MIN_INTERVAL_MS, PAIR_INTERACTION_MAX_INTERVAL_MS + 1));
}

void PairFlow::face(uint8_t slot, float dx, float dy) {
    effects_.directionMask |= 1U << slot;
    effects_.direction[slot] = std::fabs(dx) >= std::fabs(dy)
        ? (dx >= 0.0f ? Direction::RIGHT : Direction::LEFT)
        : (dy >= 0.0f ? Direction::DOWN : Direction::UP);
}

void PairFlow::faceActors() {
    face(1, homeMainActor.x - homeCompanionActor.x, homeMainActor.y - homeCompanionActor.y);
    face(0, homeCompanionActor.x - homeMainActor.x, homeCompanionActor.y - homeMainActor.y);
}

void PairFlow::trace(Trace kind, uint8_t role, Home::RouteStep step, float distance, bool timedOut) {
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (effects_.traceCount < 4) effects_.traces[effects_.traceCount++] = {
        kind, role, step, distance, timedOut, state_.phase};
#else
    (void)kind; (void)role; (void)step; (void)distance; (void)timedOut;
#endif
}

PairFlow::Effects PairFlow::takeEffects() {
    const Effects result = effects_;
    effects_ = {};
    return result;
}

bool PairFlow::start(const Layout& layout, const Environment& environment,
                     uint32_t nowMs, bool forceChase, bool forceTalk, bool arrivalTalk) {
    if (forceTalk ? !forcedTalkAllowed(environment) : !eligible(environment)) return false;
    state_.arrivalTalk = arrivalTalk;
    const Game::SpeciesCareProfile first =
        Game::speciesCareProfileFor(gameState.team[0].speciesId);
    const Game::SpeciesCareProfile second =
        Game::speciesCareProfileFor(gameState.team[1].speciesId);
    const uint8_t roll = forceTalk
        ? 0 : static_cast<uint8_t>(GameRandom::random(100));
    state_.activity = Home::ActorController::pairActivity(
        first.canMove, second.canMove, forceChase, false, forceTalk, roll);
    if (!homeRuntime.beginPair(state_.activity, nowMs)) {
        state_.activity = Home::PairActivity::NONE;
        schedule(nowMs);
        return false;
    }

    state_.leaderMain = Home::ActorController::pairLeaderMain(
        first.canMove, second.canMove,
        static_cast<uint8_t>(GameRandom::random(2)));
    const float approachDistance = state_.activity == Home::PairActivity::TALK
        ? layout.talkDistance : PAIR_APPROACH_DISTANCE;
    state_.parallelApproach = false;
    state_.mainArrived = false;
    state_.companionArrived = false;
    state_.mainApproachSpeed = 0.0f;
    state_.companionApproachSpeed = 0.0f;
    bool planned = false;
    if (state_.activity == Home::PairActivity::TALK &&
        first.canMove && second.canMove) {
        // The two fixed talk points are a coordinated choreography. Plan both
        // paths without treating the other actor's starting pose as a static
        // obstacle, then scale their speeds to reach the stage together.
        state_.leaderMain = true;
        auto planFixedRoute = [&](uint8_t actorId, Home::Actor& actor,
                                  float targetX, float targetY,
                                  bool& arrived) {
            actor.targetX = targetX;
            actor.targetY = targetY;
            arrived = std::hypot(targetX - actor.x, targetY - actor.y) <=
                PAIR_APPROACH_GOAL_TOLERANCE;
            if (arrived) {
                actor.route.clear();
                return true;
            }
            return homeRuntime.planRoute(
                actorId, targetX, targetY, false, false);
        };
        const bool mainPlanned = planFixedRoute(
            0, homeMainActor, TALK_MAIN_X, TALK_STAGE_Y,
            state_.mainArrived);
        const bool companionPlanned = mainPlanned && planFixedRoute(
            1, homeCompanionActor, TALK_COMPANION_X,
            TALK_STAGE_Y, state_.companionArrived);
        if (mainPlanned && companionPlanned) {
            const float mainDistance = pairRouteDistance(homeMainActor);
            const float companionDistance = pairRouteDistance(homeCompanionActor);
            const float sharedDurationSeconds = std::max(
                0.1f, std::max(mainDistance, companionDistance) /
                          PAIR_APPROACH_SPEED);
            state_.mainApproachSpeed = state_.mainArrived
                ? 0.0f : mainDistance / sharedDurationSeconds;
            state_.companionApproachSpeed = state_.companionArrived
                ? 0.0f : companionDistance / sharedDurationSeconds;
            state_.parallelApproach = true;
            planned = true;
            trace(Trace::FIXED_STAGE);
        } else {
            homeMainActor.route.clear();
            homeCompanionActor.route.clear();
        }
    }
    const uint8_t firstMover = state_.leaderMain ? 0 : 1;
    const bool fixedTalk = state_.activity == Home::PairActivity::TALK &&
        first.canMove && second.canMove;
    for (int attempt = 0;
         attempt < ((!fixedTalk && state_.activity == Home::PairActivity::TALK) ? 2 : 1) &&
         !planned; ++attempt) {
        if (fixedTalk) break;
        const uint8_t mover = attempt == 0 ? firstMover : 1 - firstMover;
        const Home::Actor& other = mover == 0
            ? homeCompanionActor : homeMainActor;
        const float offsets[][2] = {
            {-approachDistance, 0.0f}, {approachDistance, 0.0f},
            {0.0f, -approachDistance}, {0.0f, approachDistance},
        };
        for (const auto& offset : offsets) {
            // Vertical separation must account for the full sprite height;
            // only horizontal destinations guarantee the requested visual gap.
            if (state_.activity == Home::PairActivity::TALK && offset[0] == 0.0f) {
                continue;
            }
            if (homeRuntime.planRoute(
                    mover, other.x + offset[0], other.y + offset[1])) {
                state_.leaderMain = mover == 0;
                planned = true;
                break;
            }
        }
    }
    if (!planned) {
        trace(Trace::ROUTE_FAILED);
        homeRuntime.endPair(nowMs);
        state_.activity = Home::PairActivity::NONE;
        schedule(nowMs);
        return false;
    }

    state_.phase = Phase::APPROACH;
    state_.phaseStartedMs = nowMs;
    if (state_.parallelApproach) {
        state_.phaseUntilMs = nowMs + std::max(
            pairApproachBudgetMs(homeMainActor),
            pairApproachBudgetMs(homeCompanionActor));
    } else {
        const Home::Actor& movingActor = state_.leaderMain
            ? homeMainActor : homeCompanionActor;
        state_.phaseUntilMs = nowMs + pairApproachBudgetMs(movingActor);
    }
    state_.interactionUntilMs = nowMs +
        (state_.activity == Home::PairActivity::CHASE
             ? PAIR_CHASE_TIMEOUT_MS : PAIR_TALK_TIMEOUT_MS);
    state_.chaseLegsRemaining = PAIR_CHASE_LEGS;
    state_.mainRenderOffsetY = 0.0f;
    state_.companionRenderOffsetY = 0.0f;
    trace(Trace::START);
    return true;
}

bool PairFlow::update(const Layout& layout, const Environment& environment,
                      uint32_t nowMs, float elapsedSeconds) {
    if (state_.phase == Phase::NONE) {
        if (static_cast<int32_t>(nowMs - state_.nextInteractionMs) < 0) {
            return false;
        }
        if (!start(layout, environment, nowMs)) {
            state_.nextInteractionMs = nowMs + PAIR_INTERACTION_RETRY_MS;
            return false;
        }
    }

    if (gameState.teamCount < 2 || !homeCompanionActor.active ||
        gameState.team[0].fainted || gameState.team[0].hpCur == 0 ||
        gameState.team[1].fainted || gameState.team[1].hpCur == 0 ||
        gameState.team[0].majorStatus == Game::MajorStatus::SLEEP ||
        gameState.team[1].majorStatus == Game::MajorStatus::SLEEP ||
        environment.visitorExiting) {
        cancel(nowMs);
        return false;
    }
    for (uint8_t slot = 0; slot < 2 && !state_.arrivalTalk; ++slot) {
        const Game::MonsterRuntime& monster = gameState.team[slot];
        const Game::SpeciesCareProfile care =
            Game::speciesCareProfileFor(monster.speciesId);
        if ((care.usesBed && Game::isSleepCareTime(
                 gameState.gameMinutesTotal, monster.nature)) ||
            (care.needsFood && monster.satiety <= 25)) {
            cancel(nowMs);
            return false;
        }
    }

    switch (state_.phase) {
    case Phase::APPROACH: {
        if (state_.parallelApproach) {
            if (!state_.mainArrived) {
                float waypointX = homeMainActor.targetX;
                float waypointY = homeMainActor.targetY;
                homeMainActor.route.current(waypointX, waypointY);
                face(0,
                    waypointX - homeMainActor.x,
                    waypointY - homeMainActor.y);
            }
            if (!state_.companionArrived) {
                float waypointX = homeCompanionActor.targetX;
                float waypointY = homeCompanionActor.targetY;
                homeCompanionActor.route.current(waypointX, waypointY);
                face(1,
                    waypointX - homeCompanionActor.x,
                    waypointY - homeCompanionActor.y);
            }

            Home::RouteStep mainStep = Home::RouteStep::ARRIVED;
            Home::RouteStep companionStep = Home::RouteStep::ARRIVED;
            if (!state_.mainArrived) {
                mainStep = homeRuntime.advanceRoute(
                    0, nowMs, std::max(0.1f, state_.mainApproachSpeed),
                    elapsedSeconds, 1.0f, false);
            }
            if (!state_.companionArrived) {
                companionStep = homeRuntime.advanceRoute(
                    1, nowMs,
                    std::max(0.1f, state_.companionApproachSpeed),
                    elapsedSeconds, 1.0f, false);
            }
            effects_.camera = true;
            effects_.frameMask = (!state_.mainArrived ? 1 : 0) |
                (!state_.companionArrived ? 2 : 0);
            effects_.roomChanged = true;
            pending_ = Pending::PARALLEL;
            mainStep_ = mainStep;
            companionStep_ = companionStep;
            return true;
        }

        const uint8_t mover = state_.leaderMain ? 0 : 1;
        Home::Actor& movingActor = state_.leaderMain
            ? homeMainActor : homeCompanionActor;
        float waypointX = movingActor.targetX;
        float waypointY = movingActor.targetY;
        movingActor.route.current(waypointX, waypointY);
        if (mover == 0) {
            face(0,
                waypointX - homeMainActor.x, waypointY - homeMainActor.y);
        } else {
            face(1,
                waypointX - homeCompanionActor.x,
                waypointY - homeCompanionActor.y);
        }
        Home::RouteStep step = homeRuntime.advanceRoute(
            mover, nowMs, PAIR_APPROACH_SPEED, elapsedSeconds, 1.0f, true);
        effects_.camera = true;
        effects_.frameMask = 1U << mover;
        effects_.roomChanged = true;
        pending_ = Pending::SINGLE;
        mainStep_ = step;
        return true;
    }
    case Phase::INVITE:
        faceActors();
        if (static_cast<int32_t>(nowMs - state_.phaseUntilMs) >= 0) {
            state_.phase = Phase::ACTIVE;
            state_.phaseStartedMs = nowMs;
            if (state_.activity == Home::PairActivity::TALK) {
                if (state_.arrivalTalk) {
                    visitArrival.advance(Home::VisitArrivalEvent::TALK_STARTED,
                                         nowMs);
                }
                state_.phaseUntilMs = nowMs + PAIR_TALK_TOTAL_MS;
                trace(Trace::ACTIVE);
            } else if (!planChaseLeg(layout)) {
                state_.activity = Home::PairActivity::TALK;
                state_.phaseUntilMs = nowMs + PAIR_TALK_TOTAL_MS;
            }
        }
        return true;
    case Phase::ACTIVE:
        if (state_.activity == Home::PairActivity::TALK) {
            const uint32_t elapsed = nowMs - state_.phaseStartedMs;
            state_.mainRenderOffsetY = elapsed < PAIR_TALK_HOP_MS
                ? -std::sin(static_cast<float>(elapsed) /
                            PAIR_TALK_HOP_MS * 3.14159265f) * 5.0f : 0.0f;
            const uint32_t companionStart =
                PAIR_TALK_HOP_MS + PAIR_TALK_GAP_MS;
            state_.companionRenderOffsetY =
                elapsed >= companionStart &&
                elapsed < companionStart + PAIR_TALK_HOP_MS
                    ? -std::sin(static_cast<float>(elapsed - companionStart) /
                                PAIR_TALK_HOP_MS * 3.14159265f) * 5.0f
                    : 0.0f;
            effects_.roomChanged = true;
            if (static_cast<int32_t>(nowMs - state_.phaseUntilMs) >= 0) {
                state_.phase = Phase::CELEBRATE;
                state_.phaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
                effects_.heart = Heart::SHOW;
                effects_.heartsUntilMs = state_.phaseUntilMs;
            }
            return true;
        } else {
            const Home::RouteStep mainStep = homeRuntime.advanceRoute(
                0, nowMs, state_.leaderMain ? PAIR_CHASE_LEADER_SPEED
                                  : PAIR_CHASE_FOLLOWER_SPEED,
                elapsedSeconds, 1.0f, false);
            const Home::RouteStep companionStep = homeRuntime.advanceRoute(
                1, nowMs, state_.leaderMain ? PAIR_CHASE_FOLLOWER_SPEED
                                  : PAIR_CHASE_LEADER_SPEED,
                elapsedSeconds, 1.0f, false);
            effects_.camera = true;
            face(0, homeMainActor.velocityX, homeMainActor.velocityY);
            face(1, homeCompanionActor.velocityX, homeCompanionActor.velocityY);
            effects_.frameMask = 3;
            effects_.roomChanged = true;
            pending_ = Pending::CHASE;
            mainStep_ = mainStep;
            companionStep_ = companionStep;
            return true;
        }
    case Phase::CELEBRATE:
        faceActors();
        state_.mainRenderOffsetY = 0.0f;
        state_.companionRenderOffsetY = 0.0f;
        if (static_cast<int32_t>(nowMs - state_.phaseUntilMs) >= 0) {
            finish(nowMs, true);
            return false;
        }
        return true;
    case Phase::NONE:
        break;
    }
    return false;
}

void PairFlow::finish(uint32_t nowMs, bool reward) {
    if (state_.phase == Phase::NONE && !homeRuntime.pairActive()) return;
    trace(Trace::FINISH);
    if (state_.arrivalTalk) {
        const Home::VisitArrivalEvent event =
            reward && visitArrival.phase() == Home::VisitArrivalPhase::TALK
                ? Home::VisitArrivalEvent::TALK_FINISHED
                : Home::VisitArrivalEvent::TALK_SKIPPED;
        visitArrival.advance(event, nowMs);
    }
    pending_ = Pending::NONE;
    homeMainActor.route.clear();
    homeCompanionActor.route.clear();
    homeRuntime.endPair(nowMs, 700);
    homeMainActor.targetX = homeMainActor.x;
    homeMainActor.targetY = homeMainActor.y;
    homeMainActor.motion = Home::MotionPhase::STILL;
    effects_.stopAnimation = true;
    state_.phase = Phase::NONE;
    state_.activity = Home::PairActivity::NONE;
    state_.arrivalTalk = false;
    state_.parallelApproach = false;
    state_.mainArrived = false;
    state_.companionArrived = false;
    state_.mainApproachSpeed = 0.0f;
    state_.companionApproachSpeed = 0.0f;
    state_.chaseLegsRemaining = 0;
    state_.mainRenderOffsetY = 0.0f;
    state_.companionRenderOffsetY = 0.0f;
    homeMainActor.mind.onActivity(nowMs);
    homeCompanionActor.mind.onActivity(nowMs);
    effects_.finished = true;
    effects_.reward = reward;
    effects_.roomChanged = true;
}

void PairFlow::cancel(uint32_t nowMs) {
    finish(nowMs, false);
    effects_.heart = Heart::CLEAR;
}


bool PairFlow::completeMovement(const Layout& layout, uint32_t nowMs) {
    const Pending pending = pending_;
    pending_ = Pending::NONE;
    switch (pending) {
    case Pending::PARALLEL: return completeParallel(nowMs);
    case Pending::SINGLE: return completeSingle(nowMs);
    case Pending::CHASE: return completeChase(layout, nowMs);
    case Pending::NONE: return state_.phase != Phase::NONE;
    }
    return false;
}

bool PairFlow::completeParallel(uint32_t nowMs) {
    const auto mainStep = mainStep_, companionStep = companionStep_;
    const bool phaseTimedOut =
        static_cast<int32_t>(nowMs - state_.phaseUntilMs) >= 0;
    auto resolveArrival = [&](Home::Actor& actor,
                              Home::RouteStep step,
                              bool& arrived,
                              uint8_t role) {
        if (arrived) return true;
        const bool terminal = step == Home::RouteStep::ARRIVED ||
            step == Home::RouteStep::BLOCKED ||
            step == Home::RouteStep::NO_ROUTE || phaseTimedOut;
        if (!terminal) return true;
        const float goalDistance = std::hypot(
            actor.targetX - actor.x, actor.targetY - actor.y);
        if (step == Home::RouteStep::ARRIVED ||
            goalDistance <= PAIR_APPROACH_GOAL_TOLERANCE) {
            actor.x = actor.targetX;
            actor.y = actor.targetY;
            actor.velocityX = 0.0f;
            actor.velocityY = 0.0f;
            actor.route.clear();
            arrived = true;
            if (step != Home::RouteStep::ARRIVED) trace(Trace::NEAR_GOAL, role, step, goalDistance, phaseTimedOut);
            return true;
        }
        trace(Trace::APPROACH_FAILED, role, step, goalDistance, phaseTimedOut);
        return false;
    };
    if (!resolveArrival(homeMainActor, mainStep,
                        state_.mainArrived, 0) ||
        !resolveArrival(homeCompanionActor, companionStep,
                        state_.companionArrived, 1)) {
        finish(nowMs, false);
        return false;
    }
    effects_.camera = true;
    if (!state_.mainArrived || !state_.companionArrived) {
        return true;
    }

    faceActors();
    trace(Trace::POSITIONED);
    state_.phase = Phase::INVITE;
    state_.phaseStartedMs = nowMs;
    state_.phaseUntilMs = nowMs + PAIR_INVITE_MS;
    return true;
}

bool PairFlow::completeSingle(uint32_t nowMs) {
    const uint8_t mover = state_.leaderMain ? 0 : 1;
    Home::Actor& movingActor = state_.leaderMain ? homeMainActor : homeCompanionActor;
    const auto step = mainStep_;
    const bool phaseTimedOut =
        static_cast<int32_t>(nowMs - state_.phaseUntilMs) >= 0;
    const bool terminalStep = step == Home::RouteStep::ARRIVED ||
        step == Home::RouteStep::BLOCKED ||
        step == Home::RouteStep::NO_ROUTE || phaseTimedOut;
    if (terminalStep) {
        const float goalDistance = std::hypot(
            movingActor.targetX - movingActor.x,
            movingActor.targetY - movingActor.y);
        bool reachedGoal = step == Home::RouteStep::ARRIVED;
        if (!reachedGoal &&
            goalDistance <= PAIR_APPROACH_GOAL_TOLERANCE) {
            // A frame can hit the deadline after movement reaches the
            // target but before advanceRoute reports ARRIVED. Snap only
            // within the normal arrival tolerance so the scripted talk
            // is not cancelled one frame before completion.
            movingActor.x = movingActor.targetX;
            movingActor.y = movingActor.targetY;
            movingActor.velocityX = 0.0f;
            movingActor.velocityY = 0.0f;
            movingActor.route.clear();
            effects_.camera = true;
            reachedGoal = true;
            if (state_.activity == Home::PairActivity::TALK) trace(Trace::NEAR_GOAL, mover, step, goalDistance, phaseTimedOut);
        }
        if (state_.activity == Home::PairActivity::TALK && !reachedGoal) {
            trace(Trace::APPROACH_FAILED, mover, step, goalDistance, phaseTimedOut);
            finish(nowMs, false);
            return false;
        }
        homeMainActor.route.clear();
        homeCompanionActor.route.clear();
        faceActors();
        if (state_.activity == Home::PairActivity::TALK) trace(Trace::POSITIONED);
        state_.phase = Phase::INVITE;
        state_.phaseStartedMs = nowMs;
        state_.phaseUntilMs = nowMs + PAIR_INVITE_MS;
    }
    return true;
}

bool PairFlow::completeChase(const Layout& layout, uint32_t nowMs) {
    const auto mainStep = mainStep_, companionStep = companionStep_;
    const bool mainDone = mainStep == Home::RouteStep::ARRIVED ||
        mainStep == Home::RouteStep::BLOCKED ||
        mainStep == Home::RouteStep::NO_ROUTE;
    const bool companionDone =
        companionStep == Home::RouteStep::ARRIVED ||
        companionStep == Home::RouteStep::BLOCKED ||
        companionStep == Home::RouteStep::NO_ROUTE;
    if (mainDone && companionDone) {
        if (state_.chaseLegsRemaining > 0) --state_.chaseLegsRemaining;
        if (state_.chaseLegsRemaining == 0 || !planChaseLeg(layout)) {
            state_.phase = Phase::CELEBRATE;
            state_.phaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
            effects_.heart = Heart::SHOW;
            effects_.heartsUntilMs = state_.phaseUntilMs;
            faceActors();
        }
    }
    if (static_cast<int32_t>(nowMs - state_.interactionUntilMs) >= 0) {
        state_.phase = Phase::CELEBRATE;
        state_.phaseUntilMs = nowMs + PAIR_CELEBRATE_MS;
        effects_.heart = Heart::SHOW;
        effects_.heartsUntilMs = state_.phaseUntilMs;
    }
    return true;
}

bool PairFlow::planChaseLeg(const Layout& layout) {
    const uint8_t leader = state_.leaderMain ? 0 : 1;
    const uint8_t follower = state_.leaderMain ? 1 : 0;
    Home::Actor& leaderActor = state_.leaderMain
        ? homeMainActor : homeCompanionActor;
    const float followX = leaderActor.x;
    const float followY = leaderActor.y;
    const int minimumX = static_cast<int>(layout.bounds.minX);
    const int maximumX = static_cast<int>(layout.bounds.maxX);
    const int minimumY = static_cast<int>(layout.bounds.minY);
    const int maximumY = static_cast<int>(layout.bounds.maxY);
    for (uint8_t attempt = 0; attempt < 48; ++attempt) {
        const float targetX = static_cast<float>(GameRandom::random(
            minimumX, maximumX + 1));
        const float targetY = static_cast<float>(GameRandom::random(
            minimumY, maximumY + 1));
        const float dx = targetX - leaderActor.x;
        const float dy = targetY - leaderActor.y;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < 1156.0f || distanceSq > 7056.0f) continue;
        if (!homeRuntime.planRoute(leader, targetX, targetY,
                                   false, false)) {
            continue;
        }
        if (!homeRuntime.planRoute(follower, followX, followY,
                                   false, false)) {
            leaderActor.route.clear();
            continue;
        }
        return true;
    }
    return false;
}

}  // namespace AmoledV1
