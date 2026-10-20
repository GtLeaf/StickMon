#pragma once

#include "assets/PokemonSprites.h"
#include "core/RoomNavigator.h"
#include "game/GameState.h"
#include "game/HomeSimulation.h"
#include "game/HomeVisitArrival.h"

namespace AmoledV1 {

class VisitorFlow {
public:
    using Direction = PokemonSprites::WalkDirection;
    enum class Motion : uint8_t { NONE, HOST_APPROACH, ENTERING, ACTIVE, EXITING };
    struct State {
        Motion motion = Motion::NONE;
        bool exitIsDebug = false, crossingDoor = false;
        uint8_t hostDoorPhase = 0;
        uint32_t untilMs = 0, blockedSinceMs = 0;
        float doorInsideX = 0.0f, doorInsideY = 0.0f;
        float doorOutsideX = 0.0f, doorOutsideY = 0.0f;
        float guestDoorInsideX = 0.0f, guestDoorInsideY = 0.0f;
        float welcomeX = 0.0f, welcomeY = 0.0f;
        bool welcomeValid = false;
        float landingX = 0.0f, landingY = 0.0f;
        bool landingValid = false;
    };
    struct Layout {
        bool available = false;
        const RoomResource::Point* polygon = nullptr;
        uint8_t polygonCount = 0;
        const RoomResource::Point* walkPolygon = nullptr;
        uint8_t walkCount = 0;
        RoomNavigator::Bounds bounds{};
        float insideX = 0.0f, insideY = 0.0f;
        float outsideX = 0.0f, outsideY = 0.0f;
    };
    enum class Trace : uint8_t {
        HOST_CLEAR, CLEAR_UNAVAILABLE, WELCOME_UNAVAILABLE, RECOVER,
        HOST_FALLBACK, HOST_APPROACH, GUEST_VISIBLE, EXIT_BEGIN,
        ENTERED, EXITED, DOOR_OPEN, CLEAR_END, HOST_READY,
    };
    struct TraceEvent {
        Trace kind{};
        float mainX = 0.0f, mainY = 0.0f, guestX = 0.0f, guestY = 0.0f;
        float targetX = 0.0f, targetY = 0.0f;
    };
    struct Effects {
        uint8_t directionMask = 0, resetFrameMask = 0, frameMask = 0;
        Direction direction[2]{};
        bool stopAnimation = false, guestIdle = false, camera = false;
        bool fullRender = false, roomChanged = false, actorChanged = false;
        uint8_t actorId = 0;
        float previousY = 0.0f;
        bool needLanding = false, entryFinished = false, exitFinished = false;
        bool debugExit = false, timedOut = false;
        TraceEvent traces[3]{};
        uint8_t traceCount = 0;
    };
    VisitorFlow(Game::GameState& game, Home::Actor& main, Home::Actor& companion,
                Home::Simulation& simulation, Home::VisitArrivalFlow& arrival)
        : gameState(game), homeMainActor(main), homeCompanionActor(companion),
          homeRuntime(simulation), visitArrival(arrival) {}
    VisitorFlow(const VisitorFlow&) = delete;
    VisitorFlow& operator=(const VisitorFlow&) = delete;
    const State& state() const { return state_; }
    void beginVisitorEntry(const Layout& layout, uint32_t nowMs);
    void beginVisitorExit(const Layout& layout, uint32_t nowMs, bool debugVisitor);
    void update(const Layout& layout, uint32_t nowMs, float elapsedSeconds);
    void completeMovement(const Layout& layout, uint32_t nowMs);
    void completeLanding(bool valid, float x, float y);
    void cancelEntryForDeparture();
    void resetMotion();
    Effects takeEffects();
private:
    enum class Pending : uint8_t { NONE, HOST_CLEAR, HOST_APPROACH, DIRECT_TAIL };
    bool chooseVisitorDoorPose(const Layout& layout, const Home::Actor& actor, float& x, float& y) const;
    bool beginVisitorHostClear(const Layout& layout, uint32_t nowMs);
    void startVisitorDoorEntry(const Layout& layout, uint32_t nowMs);
    void finishEntry(uint32_t nowMs, bool timedOut);
    void finishExit(uint32_t nowMs);
    bool mainInside(const Layout& layout, float x, float y) const;
    bool guestInside(const Layout& layout, float x, float y) const;
    void face(uint8_t slot, float dx, float dy);
    void damageActor(uint8_t slot, float previousY);
    void trace(Trace kind);
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
    Home::RouteStep pendingStep_ = Home::RouteStep::NO_ROUTE;
    float previousY_ = 0.0f;
    bool landingPending_ = false;
};

}  // namespace AmoledV1
