#pragma once

#include "assets/PokemonSprites.h"
#include "game/HomeSimulation.h"
#include "game/HomeVisitDeparture.h"

namespace AmoledV1 {

// Local pet's visit departure. Communication and presentation stay with caller.
class VisitAwayFlow {
public:
    using Phase = Home::VisitDeparturePhase;
    using Direction = PokemonSprites::WalkDirection;
    struct Door {
        bool roomAvailable = false;
        float insideX = 0.0f, insideY = 0.0f;
        float outsideX = 0.0f, outsideY = 0.0f;
    };
    struct State {
        Phase phase = Phase::NONE;
        uint32_t startedMs = 0;
        Door door;
    };
    struct Effects {
        bool stopAnimation = false, resetAnimation = false, startWalk = false, camera = false;
        bool fullRender = false, actorChanged = false, departed = false;
        bool clearRecall = false;
        float previousY = 0.0f;
        bool directionChanged = false;
        Direction direction = Direction::DOWN;
    };
    VisitAwayFlow(Home::Actor& actor, Home::Simulation& simulation)
        : actor_(actor), simulation_(simulation) {}
    VisitAwayFlow(const VisitAwayFlow&) = delete;
    VisitAwayFlow& operator=(const VisitAwayFlow&) = delete;
    const State& state() const { return state_; }
    void begin(const Door& door, uint32_t nowMs);
    bool update(bool sessionActive, uint32_t nowMs, float elapsedSeconds);
    // Resolve door phase after caller applies movement/camera/frame damage.
    void completeMovement(uint32_t nowMs);
    Effects takeEffects();
private:
    friend struct AmoledAppTestAccess;
    friend struct AmoledCareTestAccess;
    Home::Actor& actor_;
    Home::Simulation& simulation_;
    State state_;
    Effects effects_;
    bool pending_ = false, arrived_ = false;
};

}  // namespace AmoledV1
