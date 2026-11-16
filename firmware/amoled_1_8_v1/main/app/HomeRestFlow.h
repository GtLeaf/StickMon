#pragma once
#include "app/HomeSleepFlow.h"
#include "game/GameState.h"

namespace AmoledV1 {
// Faint-rest lifecycle, distinct from normal sleep and the shared HP care rules.
// All authoritative flags, routes and leases remain on actors/Simulation.
class HomeRestFlow {
  public:
    HomeRestFlow(Home::Actor &main, Home::Actor &companion, Home::Simulation &simulation,
                 const HomeSleepFlow &sleep)
        : main_(main), companion_(companion), simulation_(simulation), sleep_(sleep) {}
    HomeRestFlow(const HomeRestFlow &) = delete;
    HomeRestFlow &operator=(const HomeRestFlow &) = delete;

    // Forced home placement returns a bit per placed actor, including re-placement.
    uint8_t place(const Game::GameState &game, const HomeSleepFlow::Layout &layout, uint32_t nowMs);
    // Ongoing rest returns true only when presentation needs to start/restart.
    bool restMain(float bedX, float bedY, uint32_t nowMs);
    bool restCompanion(const HomeSleepFlow::Layout &layout, uint32_t nowMs);
    // Caller has already observed recovery. Main random idle scheduling stays in App.
    bool recoverMain(uint32_t nowMs);
    bool recoverCompanion(uint32_t nowMs);

  private:
    void positionMain(float x, float y);
    void positionCompanion(const HomeSleepFlow::Layout &layout);
    Home::Actor &main_;
    Home::Actor &companion_;
    Home::Simulation &simulation_;
    const HomeSleepFlow &sleep_;
};
} // namespace AmoledV1
