#pragma once

#include "game/HomeActorController.h"
#include "game/HomeCare.h"
#include "game/HomeSimulation.h"

namespace AmoledV1 {

// Owns food scheduling and decisions. Actor tasks/leases remain authoritative in
// Simulation; App applies rewards, saves and presentation before completing a bite.
class HomeFoodFlow {
  public:
    struct Layout {
        float foodX = 0, foodY = 0;
    };
    struct State {
        uint32_t biteAtMs[2]{};
        bool pending = false;
        uint32_t retryAtMs = 0;
    };
    // Synchronous movement boundary: preserve the caller's turning/animation
    // setup. Implementations must not reenter this flow or consume its effects.
    struct Movement {
        virtual ~Movement() = default;
        virtual bool chooseMainApproach(float desiredX, float desiredY, float &x, float &y) = 0;
        virtual bool begin(uint8_t actor, Home::Task task, float x, float y, uint32_t nowMs) = 0;
    };
    enum class SeekTrace : uint8_t {
        NONE,
        CLAIM_REJECTED,
        APPROACH_REJECTED,
        ROUTE_REJECTED,
        DIRECT,
        ROUTED
    };
    struct Effects {
        SeekTrace seekTrace = SeekTrace::NONE;
        uint8_t seekActor = 0;
        uint8_t feedingMask = 0, finishedMask = 0;
        bool stopMain = false, stopCompanion = false, roomRender = false;
        int8_t yieldActor = -1;
    };
    struct FeedingUpdate {
        FoodConsumeResult bite;
        bool finished = false;
    };

    HomeFoodFlow(Game::GameState &game, Home::Actor &main, Home::Actor &companion,
                 Home::Simulation &simulation)
        : game_(game), main_(main), companion_(companion), simulation_(simulation) {}
    HomeFoodFlow(const HomeFoodFlow &) = delete;
    HomeFoodFlow &operator=(const HomeFoodFlow &) = delete;
    const State &state() const { return state_; }
    Effects takeEffects();
    void reset();
    void clearMainBiteClock() { state_.biteAtMs[0] = 0; }
    void scheduleArbitration(uint32_t atMs);
    bool eligible(uint8_t actor, const Home::HouseholdObservation &observation) const;
    int8_t preferred(const Home::HouseholdObservation &observation) const;
    bool nearFood(uint8_t actor, const Layout &layout) const;
    bool blocksApproach(uint8_t actor, const Layout &layout) const;
    bool claim(uint8_t actor, const Layout &layout, const Home::HouseholdObservation &observation,
               uint32_t nowMs);
    bool startSeeking(uint8_t actor, const Layout &layout,
                      const Home::HouseholdObservation &observation, Movement &movement,
                      uint32_t nowMs);
    void enterFeeding(uint8_t actor, uint32_t nowMs);
    FeedingUpdate tickFeeding(uint8_t actor, uint32_t nowMs);
    void finishFeeding(uint8_t actor, uint32_t nowMs);
    bool startCompanionExit(const Layout &layout, Movement &movement, uint32_t nowMs);
    void reconsiderRemaining(uint8_t finishedActor, const Home::HouseholdObservation &observation,
                             uint32_t nowMs);
    void servingAdded(bool mainFoodMotion, uint32_t nowMs);
    void service(const Layout &layout, const Home::HouseholdObservation &observation,
                 Movement &movement, uint32_t nowMs);
    void companionYieldArrived(const Layout &layout, uint32_t nowMs);

  private:
    Home::HouseholdObservation currentObservation(Home::HouseholdObservation observation) const;
    Game::GameState &game_;
    Home::Actor &main_;
    Home::Actor &companion_;
    Home::Simulation &simulation_;
    State state_;
    Effects effects_;
    friend struct HomeFoodTestAccess;
};
} // namespace AmoledV1
