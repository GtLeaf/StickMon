#pragma once

#include "assets/PokemonSprites.h"
#include "core/BuildConfig.h"
#include "core/RoomNavigator.h"
#include "game/GameState.h"
#include "game/HomeSimulation.h"
#include "game/HomeVisitArrival.h"

namespace AmoledV1 {

// Owns the pair lifecycle; actors and resource arbitration stay in Simulation.
class PairFlow {
public:
    using Direction = PokemonSprites::WalkDirection;
    static constexpr float TALK_COMPANION_X = 97.0f;
    static constexpr float TALK_MAIN_X = 149.0f;
    static constexpr float TALK_STAGE_Y = 118.0f;
    enum class Phase : uint8_t { NONE, APPROACH, INVITE, ACTIVE, CELEBRATE };
    struct State {
        Phase phase = Phase::NONE;
        Home::PairActivity activity = Home::PairActivity::NONE;
        bool leaderMain = true, arrivalTalk = false, parallelApproach = false;
        bool mainArrived = false, companionArrived = false;
        float mainApproachSpeed = 0.0f, companionApproachSpeed = 0.0f;
        uint32_t nextInteractionMs = 0, phaseStartedMs = 0, phaseUntilMs = 0;
        uint32_t interactionUntilMs = 0;
        uint8_t chaseLegsRemaining = 0;
        float mainRenderOffsetY = 0.0f, companionRenderOffsetY = 0.0f;
    };
    struct Environment {
        bool ordinaryStartAllowed = false, forcedTalkAllowed = false;
        bool visitorExiting = false;
    };
    struct Layout {
        RoomNavigator::Bounds bounds{};
        float talkDistance = 0.0f;
    };
    enum class Heart : uint8_t { NONE, SHOW, CLEAR };
    enum class Trace : uint8_t {
        FIXED_STAGE, ROUTE_FAILED, START, NEAR_GOAL, APPROACH_FAILED,
        POSITIONED, ACTIVE, FINISH,
    };
#if STICKMON_ENABLE_DEBUG_FEATURES
    struct TraceEvent {
        Trace kind;
        uint8_t role;
        Home::RouteStep step;
        float distance;
        bool timedOut;
        Phase phase;
    };
#endif
    struct Effects {
        bool camera = false, roomChanged = false, stopAnimation = false;
        bool finished = false, reward = false;
        uint8_t frameMask = 0, directionMask = 0;
        Direction direction[2]{};
        Heart heart = Heart::NONE;
        uint32_t heartsUntilMs = 0;
#if STICKMON_ENABLE_DEBUG_FEATURES
        TraceEvent traces[4]{};
        uint8_t traceCount = 0;
#endif
    };

    PairFlow(Game::GameState& game, Home::Actor& main, Home::Actor& companion,
             Home::Simulation& simulation, Home::VisitArrivalFlow& arrival)
        : gameState(game), homeMainActor(main), homeCompanionActor(companion),
          homeRuntime(simulation), visitArrival(arrival) {}
    PairFlow(const PairFlow&) = delete;
    PairFlow& operator=(const PairFlow&) = delete;
    const State& state() const { return state_; }
    bool eligible(const Environment& environment) const;
    bool forcedTalkAllowed(const Environment& environment) const;
    bool needsStart(uint32_t nowMs) const;
    void schedule(uint32_t nowMs, bool immediate = false);
    bool start(const Layout& layout, const Environment& environment,
               uint32_t nowMs, bool forceChase = false, bool forceTalk = false,
               bool arrivalTalk = false);
    bool update(const Layout& layout, const Environment& environment,
                uint32_t nowMs, float elapsedSeconds);
    // Apply camera/frame effects before resolving arrivals or replanning.
    bool completeMovement(const Layout& layout, uint32_t nowMs);
    void finish(uint32_t nowMs, bool reward);
    void cancel(uint32_t nowMs);
    Effects takeEffects();

private:
    enum class Pending : uint8_t { NONE, PARALLEL, SINGLE, CHASE };
    bool completeParallel(uint32_t nowMs);
    bool completeSingle(uint32_t nowMs);
    bool completeChase(const Layout& layout, uint32_t nowMs);
    bool planChaseLeg(const Layout& layout);
    void face(uint8_t slot, float dx, float dy);
    void faceActors();
    void trace(Trace kind, uint8_t role = 0,
               Home::RouteStep step = Home::RouteStep::NO_ROUTE,
               float distance = 0.0f, bool timedOut = false);
    friend struct AmoledCareTestAccess;
    friend struct AmoledAppTestAccess;
    Game::GameState& gameState;
    Home::Actor& homeMainActor;
    Home::Actor& homeCompanionActor;
    Home::Simulation& homeRuntime;
    Home::VisitArrivalFlow& visitArrival;
    State state_;
    Effects effects_;
    Pending pending_ = Pending::NONE;
    Home::RouteStep mainStep_ = Home::RouteStep::NO_ROUTE;
    Home::RouteStep companionStep_ = Home::RouteStep::NO_ROUTE;
};

}  // namespace AmoledV1
