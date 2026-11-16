#pragma once

#include "assets/PokemonSprites.h"
#include "game/GameState.h"
#include "game/HomeSimulation.h"
#include "ui/SceneFade.h"

namespace AmoledV1 {

// Door movement and transition lifetime. App supplies geometry and consumes
// effects; gameplay settlement and resource loading require explicit acknowledgements.
class ExploreTransitionFlow {
  public:
    enum class Phase : uint8_t {
        NONE,
        WALK_TO_DOOR,
        CROSS_DOOR,
        WALK_COMPANION_TO_DOOR,
        CROSS_COMPANION_DOOR,
        FADE_OUT,
        FADE_IN,
        RETURN_FADE_OUT,
        RETURN_WALK_IN,
        RETURN_CLEAR_MAIN,
        RETURN_COMPANION_WALK_IN,
        RETURN_FADE_IN,
    };
    struct State {
        Phase phase = Phase::NONE;
        bool pending = false, autoWalk = false;
        uint8_t area = 0;
        float insideX = 0, insideY = 0, outsideX = 0, outsideY = 0;
        float companionInsideX = 0, companionInsideY = 0;
        float companionOutsideX = 0, companionOutsideY = 0;
        bool mainHidden = false, companionHidden = false, companionDeparting = false;
        uint32_t phaseStartedMs = 0;
    };
    struct Layout {
        bool available = false, doorValid = false;
        float insideX = 0, insideY = 0, outsideX = 0, outsideY = 0;
        const RoomResource::Point *polygon = nullptr;
        uint8_t polygonCount = 0;
    };
    struct Environment {
        bool atHome = true;
        uint32_t lastMotionMs = 0;
    };
    enum class Request : uint8_t { NONE, START_DESTINATION, SETTLE_RETURN, FIND_CLEARANCE };
    enum class Frame : uint8_t { KEEP, RESET, ADVANCE, RESET_ADVANCE, IDLE };
    struct Visual {
        Frame frame = Frame::KEEP;
        bool clockNow = false, directionChanged = false;
        PokemonSprites::WalkDirection direction = PokemonSprites::WalkDirection::DOWN;
    };
    enum class Trace : uint8_t {
        NONE,
        LEADER_TIMEOUT,
        CLEARANCE_FALLBACK,
        CLEARANCE_TIMEOUT,
        COMPANION_TIMEOUT,
        RETURNED_SOLO,
        RETURNED_PAIR
    };
    struct Effects {
        Request request = Request::NONE;
        Visual main, companion;
        bool goHome = false, fullRender = false, roomRender = false, camera = false;
        bool motionClock = false, interactionClock = false, endSession = false;
        bool stopMainAnimation = false, mainLongMove = false, clearFeedingClock = false;
        bool placeFainted = false, petRestNotice = false;
        bool scheduleMain = false, schedulePair = false;
        Trace trace = Trace::NONE;
        float clearanceSeparation = 0;
    };

    ExploreTransitionFlow(const Game::GameState &game, Home::Actor &main, Home::Actor &companion,
                          Home::Simulation &simulation)
        : game_(game), main_(main), companion_(companion), simulation_(simulation) {}
    ExploreTransitionFlow(const ExploreTransitionFlow &) = delete;
    ExploreTransitionFlow &operator=(const ExploreTransitionFlow &) = delete;
    const State &state() const { return state_; }
    const SceneFade &fade() const { return fade_; }
    void markPresented(uint32_t nowMs) { fade_.markPresented(nowMs); }
    void reset();
    bool queue(uint8_t area, bool autoWalk);
    void cancel(uint32_t nowMs);
    bool beginReturn();
    bool update(const Layout &layout, const Environment &environment, uint32_t nowMs);
    Effects takeEffects();
    void destinationStarted(bool started);
    void returnSettled(const Layout &layout, uint32_t nowMs);
    void clearanceChosen(bool found, float x, float y, uint32_t nowMs);
    // Direct battle defeat bypasses the animated return; a newly attached guest
    // may also become visible while there is no transition.
    void clearVisibility();
    void revealCompanionIfIdle();

  private:
    bool companionHealthy() const;
    bool mainFainted() const;
    void face(Visual &visual, float dx, float dy);
    void advanceFrame(Visual &visual);
    void startCompanion(uint32_t nowMs);
    void startFadeOut();
    void finishReturn(uint32_t nowMs, bool pair);
    void chooseClearance(const Layout &layout, uint32_t nowMs);
    void prepareClearance(bool found, float x, float y, uint32_t nowMs);
    static bool contains(const Layout &layout, const Home::Actor &actor, float x, float y);
    const Game::GameState &game_;
    Home::Actor &main_;
    Home::Actor &companion_;
    Home::Simulation &simulation_;
    State state_;
    SceneFade fade_;
    Effects effects_;
    Request awaiting_ = Request::NONE;
    friend struct ExploreTransitionTestAccess;
};

} // namespace AmoledV1
