#pragma once

#include "game/HomeActorController.h"
#include "game/HomeSimulation.h"

namespace AmoledV1 {

// Ordinary sleep orchestration. Simulation remains authoritative for actor state
// and leases; the application retains animation, faint recovery and persistence.
class HomeSleepFlow {
  public:
    struct Layout {
        const RoomResource::Point *polygon = nullptr;
        uint8_t polygonCount = 0;
        int minX = 0, maxX = 0, minY = 0, maxY = 0;
        float bedX = 76, bedY = 99;
        bool hasBedBounds = false;
        float bedMinX = 0, bedMaxX = 0, bedMinY = 0, bedMaxY = 0;
        bool hasAnchor = false;
        float preferredX = 0, preferredY = 0;
    };
    // Synchronous callback preserves the application's existing turn/route setup.
    // It may remember a companion target but must not start another sleep decision.
    struct Movement {
        virtual ~Movement() = default;
        virtual bool begin(uint8_t actor, Home::Task task, float x, float y, uint32_t nowMs) = 0;
    };
    enum class StartResult : uint8_t { BLOCKED, DIRECT, ROUTED, FAILED };

    HomeSleepFlow(Home::Actor &main, Home::Actor &companion, Home::Simulation &simulation)
        : main_(main), companion_(companion), simulation_(simulation) {}
    HomeSleepFlow(const HomeSleepFlow &) = delete;
    HomeSleepFlow &operator=(const HomeSleepFlow &) = delete;

    float companionMinDistance() const;
    bool companionSpotUsable(const Layout &layout, float x, float y, float minDistance) const;
    bool pointBlocksBedRoute(float x, float y) const;
    bool chooseCompanionSpot(const Layout &layout, float &x, float &y,
                             bool avoidBedRoute = false) const;
    bool atBedPose(const Layout &layout) const;
    bool atBedPose(float bedX, float bedY) const;
    StartResult startMain(const Layout &layout, Movement &movement, uint32_t nowMs);
    bool startCompanion(const Layout &layout, Movement &movement, uint32_t nowMs);
    void enterSleep(uint8_t actor, uint32_t nowMs);
    void rememberCompanionTarget(float x, float y);
    // Only WAKE/WAKE_FOR_FOOD change state. Caller preserves its own tick return
    // semantics and applies the corresponding frame reset after a successful wake.
    bool wake(uint8_t actor, Home::ActorIntent intent, uint32_t nowMs);

  private:
    Home::Actor &main_;
    Home::Actor &companion_;
    Home::Simulation &simulation_;
};
} // namespace AmoledV1
