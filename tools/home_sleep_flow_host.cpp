#include "app/HomeSleepFlow.h"
#include "game/GameRandom.h"

#include <cassert>
#include <cstdio>

using Flow = AmoledV1::HomeSleepFlow;

namespace {
constexpr RoomResource::Point ROOM[] = {{0, 0}, {120, 0}, {120, 120}, {0, 120}};

struct Fixture : Flow::Movement {
    Home::Actor main, companion;
    Home::Simulation simulation;
    Flow flow{main, companion, simulation};
    Flow::Layout layout{};
    int16_t parents[RoomNavigator::MAX_NODES]{};
    uint16_t queue[RoomNavigator::MAX_NODES]{};
    bool allowRoute[2] = {true, true};
    struct Call {
        uint8_t actor;
        Home::Task task;
        Home::Task priorTask;
        float x, y;
        uint32_t now;
    };
    Call calls[8]{};
    unsigned callCount = 0;

    Fixture() {
        main.reset(Home::ActorRole::LEADER, 0, 100);
        companion.reset(Home::ActorRole::TEAMMATE, 1, 100);
        main.active = companion.active = true;
        main.geometry.groundOffsetY = companion.geometry.groundOffsetY = 0;
        main.geometry.footprint = companion.geometry.footprint = {5, 3};
        main.x = 20;
        main.y = 60;
        main.targetX = 100;
        main.targetY = 60;
        companion.x = companion.targetX = 20;
        companion.y = companion.targetY = 100;
        layout.polygon = ROOM;
        layout.polygonCount = 4;
        layout.minX = layout.minY = 0;
        layout.maxX = layout.maxY = 120;
        layout.bedX = 100;
        layout.bedY = 60;
        layout.preferredX = layout.preferredY = 60;
        simulation.attach(main, &companion);
        simulation.reset(100);
        Home::NavigationWorld world{};
        world.polygon = ROOM;
        world.polygonCount = 4;
        world.footBounds = {0, 0, 120, 120};
        world.actorCollisionMode = Home::ActorCollisionMode::SOFT;
        world.scratch = {parents, queue, RoomNavigator::MAX_NODES};
        simulation.setNavigation(world);
    }

    // Only the presentation-facing movement boundary is adapted. Route
    // planning, task transitions and leases use the production simulation.
    bool begin(uint8_t actor, Home::Task task, float x, float y,
               uint32_t now) override {
        assert(actor < 2 && callCount < 8);
        calls[callCount++] = {actor, task, simulation.actor(actor)->task, x, y, now};
        if (!allowRoute[actor]) return false;
        if (!simulation.planRoute(actor, x, y) ||
            !simulation.transitionPreparedRoute(actor, task, now)) return false;
        if (actor == 1 && task == Home::Task::SEEK_SLEEP)
            flow.rememberCompanionTarget(x, y);
        return true;
    }
};

void assertStopped(const Home::Actor& actor, uint32_t decision) {
    assert(actor.task == Home::Task::IDLE);
    assert(actor.resumeTask == Home::Task::IDLE);
    assert(actor.motion == Home::MotionPhase::STILL);
    assert(actor.route.empty());
    assert(actor.velocityX == 0 && actor.velocityY == 0);
    assert(actor.targetX == actor.x && actor.targetY == actor.y);
    assert(actor.nextDecisionMs == decision);
}

void directSleepSkipsMovementAndClearsStaleRoute() {
    Fixture f;
    f.main.x = 102.5f;
    f.main.y = 66;
    assert(f.flow.atBedPose(f.layout));
    f.main.x = 102.51f;
    assert(!f.flow.atBedPose(f.layout));
    f.main.x = 97.5f;
    f.main.y = 53.99f;
    assert(!f.flow.atBedPose(f.layout));
    f.main.y = 54;
    assert(f.flow.atBedPose(f.layout));
    f.main.task = Home::Task::FAINTED;
    f.main.route.count = 2;
    f.main.velocityX = 4;
    f.main.velocityY = -2;
    f.main.motion = Home::MotionPhase::ROUTE;
    assert(f.flow.startMain(f.layout, f, 200) == Flow::StartResult::DIRECT);
    assert(f.callCount == 0);
    assert(f.main.task == Home::Task::SLEEPING);
    assert(f.main.motion == Home::MotionPhase::STILL);
    assert(f.main.route.empty());
    assert(f.main.velocityX == 0 && f.main.velocityY == 0);
    assert(f.main.targetX == 97.5f && f.main.targetY == 54);
    assert(f.simulation.owner(Home::Resource::BED) == 0);
    assert(f.simulation.validate() == 0);
}

void routedSleepKeepsLeaseAndFailureReleasesIt() {
    Fixture routed;
    assert(routed.flow.startMain(routed.layout, routed, 200) ==
           Flow::StartResult::ROUTED);
    assert(routed.main.task == Home::Task::SEEK_SLEEP);
    assert(!routed.main.route.empty());
    assert(routed.main.targetX == 100 && routed.main.targetY == 60);
    assert(routed.simulation.owner(Home::Resource::BED) == 0);
    assert(routed.callCount == 1 && routed.calls[0].actor == 0);
    assert(routed.calls[0].task == Home::Task::SEEK_SLEEP);
    assert(routed.calls[0].x == 100 && routed.calls[0].y == 60);
    assert(routed.calls[0].now == 200);
    routed.main.x = 99;
    routed.flow.enterSleep(0, 400);
    assert(routed.main.task == Home::Task::SLEEPING);
    assert(routed.main.targetX == 100 && routed.main.targetY == 60);
    assert(routed.simulation.owner(Home::Resource::BED) == 0);
    assert(routed.simulation.validate() == 0);

    Fixture failed;
    failed.allowRoute[0] = false;
    assert(failed.flow.startMain(failed.layout, failed, 200) ==
           Flow::StartResult::FAILED);
    assert(failed.callCount == 1);
    assert(failed.simulation.owner(Home::Resource::BED) == -1);
    assert(failed.main.task == Home::Task::IDLE);

    Fixture blocked;
    assert(blocked.simulation.acquire(Home::Resource::BED, 1,
                                      Home::Task::SLEEPING, 100));
    blocked.flow.enterSleep(1, 100);
    assert(blocked.flow.startMain(blocked.layout, blocked, 0xffffff00U) ==
           Flow::StartResult::BLOCKED);
    assert(blocked.callCount == 0);
    assert(blocked.simulation.owner(Home::Resource::BED) == 1);
    assert(blocked.main.task == Home::Task::IDLE);
    assert(blocked.main.nextDecisionMs == 744);
}

void nearbyCompanionYieldsBeforeLeaderStarts() {
    Fixture f;
    f.layout.hasAnchor = true;
    f.layout.preferredX = 60;
    f.layout.preferredY = 90;
    f.companion.x = 80;
    f.companion.y = 60;
    f.companion.task = Home::Task::SLEEPING;
    assert(f.flow.startMain(f.layout, f, 200) == Flow::StartResult::ROUTED);
    assert(f.callCount == 2);
    assert(f.calls[0].actor == 1 && f.calls[0].task == Home::Task::YIELDING);
    assert(f.calls[0].priorTask == Home::Task::YIELDING);
    assert(f.calls[0].x == 60 && f.calls[0].y == 90);
    assert(f.calls[1].actor == 0);
    assert(f.companion.task == Home::Task::YIELDING);
    assert(f.companion.targetX == 60 && f.companion.targetY == 90);
    assert(f.simulation.owner(Home::Resource::BED) == 0);

    Fixture occupied;
    occupied.companion.x = 95;
    occupied.companion.y = 60;
    assert(occupied.flow.startMain(occupied.layout, occupied, 200) ==
           Flow::StartResult::FAILED);
    assert(occupied.companion.task == Home::Task::YIELDING);
    assert(occupied.simulation.owner(Home::Resource::BED) == -1);

    Fixture boundary;
    boundary.main.x = 100;
    boundary.main.y = 60;
    boundary.companion.x = 100;
    boundary.companion.y = 90; // Exactly 30px does not trigger yielding.
    assert(boundary.flow.startMain(boundary.layout, boundary, 200) ==
           Flow::StartResult::DIRECT);
    assert(boundary.callCount == 0);
}

void companionSpotsRespectFootprintBedBoundsAndSpacing() {
    Fixture f;
    assert(f.flow.companionSpotUsable(f.layout, 60, 90, 30));
    assert(!f.flow.companionSpotUsable(f.layout, 2, 90, 30));
    assert(!f.flow.companionSpotUsable(f.layout, 60, 2, 30));
    assert(!f.flow.companionSpotUsable(f.layout, 29, 65, 30));
    assert(f.flow.companionSpotUsable(f.layout, 30, 60, 30));
    assert(f.flow.companionSpotUsable(f.layout, 20, 66, 30));
    assert(f.flow.companionSpotUsable(f.layout, 70, 60, 30));
    assert(!f.flow.companionSpotUsable(f.layout, 70.01f, 60, 30));
    f.layout.hasBedBounds = true;
    f.layout.bedMinX = 50;
    f.layout.bedMaxX = 70;
    f.layout.bedMinY = 80;
    f.layout.bedMaxY = 100;
    assert(!f.flow.companionSpotUsable(f.layout, 50, 80, 30));
    assert(!f.flow.companionSpotUsable(f.layout, 70, 100, 30));
    assert(f.flow.companionSpotUsable(f.layout, 49.5f, 90, 30));
    f.layout.hasBedBounds = false;
    assert(f.flow.companionSpotUsable(f.layout, 60, 90, 30));
    f.companion.active = false;
    assert(!f.flow.companionSpotUsable(f.layout, 60, 90, 30));

    Fixture scaled;
    assert(scaled.flow.companionMinDistance() == 30);
    scaled.main.geometry.footprint.radiusX = 12;
    scaled.companion.geometry.footprint.radiusX = 14;
    assert(scaled.flow.companionMinDistance() == 34);
    assert(!scaled.flow.companionSpotUsable(
        scaled.layout, 67, 60, scaled.flow.companionMinDistance()));
    assert(scaled.flow.companionSpotUsable(
        scaled.layout, 66, 60, scaled.flow.companionMinDistance()));
    scaled.main.geometry.footprint.radiusX = 100;
    scaled.companion.geometry.footprint.radiusX = 100;
    assert(scaled.flow.companionMinDistance() == 44);
}

void deterministicSelectionHonorsAnchorAndAvoidsActiveBedRoutes() {
    Fixture f;
    float x = -1, y = -2;
    f.layout.hasAnchor = true;
    f.layout.preferredX = 61;
    f.layout.preferredY = 91;
    assert(f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 61 && y == 91); // Valid anchors need not lie on the 3px grid.
    f.layout.hasAnchor = false;
    f.layout.preferredX = f.layout.preferredY = 61.5f;
    assert(f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 60 && y == 60); // Equal scores keep the first row, then column.
    f.layout.preferredX = f.layout.preferredY = 60;
    assert(f.flow.chooseCompanionSpot(f.layout, x, y, true));
    assert(x == 60 && y == 39);
    f.layout.hasAnchor = true; // Anchor on the bed route must also be rejected.
    assert(f.flow.chooseCompanionSpot(f.layout, x, y, true));
    assert(x == 60 && y == 39);
    f.main.task = Home::Task::SEEK_SLEEP;
    assert(f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 60 && y == 39);
    f.main.task = Home::Task::TURNING;
    f.main.resumeTask = Home::Task::SEEK_SLEEP;
    assert(f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 60 && y == 39);
    f.main.resumeTask = Home::Task::WANDER;
    assert(f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 60 && y == 60);

    f.layout.hasAnchor = false;
    f.layout.minX = f.layout.minY = 30;
    f.layout.maxX = f.layout.maxY = 36;
    f.layout.bedX = f.layout.bedY = 33;
    x = 123;
    y = 456;
    assert(!f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 123 && y == 456);
    assert(!f.flow.startCompanion(f.layout, f, 200));
    assert(f.callCount == 0);
    f.companion.active = false;
    assert(!f.flow.chooseCompanionSpot(f.layout, x, y));
    assert(x == 123 && y == 456);
}

void routeClearanceUsesRemainingSegmentsAndStrictBoundary() {
    Fixture f;
    assert(f.flow.pointBlocksBedRoute(60, 79.9f));
    assert(!f.flow.pointBlocksBedRoute(60, 80));
    assert(!f.flow.pointBlocksBedRoute(60, 90));
    assert(!f.flow.pointBlocksBedRoute(121, 60));
    assert(!f.flow.pointBlocksBedRoute(-1, 60));
    f.main.route.count = 2;
    f.main.route.x[0] = 60;
    f.main.route.y[0] = 60;
    f.main.route.x[1] = 60;
    f.main.route.y[1] = 100;
    assert(f.flow.pointBlocksBedRoute(60, 90));
    assert(!f.flow.pointBlocksBedRoute(40, 80));
    f.main.y = 100;
    f.main.route.index = 1;
    f.main.route.x[0] = 100;
    f.main.route.y[0] = 20;
    f.main.route.x[1] = 100;
    f.main.route.y[1] = 100;
    assert(!f.flow.pointBlocksBedRoute(100, 20));
    assert(f.flow.pointBlocksBedRoute(80, 100));
    f.main.route.clear();
    f.main.targetX = f.main.x;
    f.main.targetY = f.main.y;
    assert(f.flow.pointBlocksBedRoute(39.9f, 100));
    assert(!f.flow.pointBlocksBedRoute(40, 100));
    f.main.geometry.footprint.radiusX = 15;
    f.companion.geometry.footprint.radiusX = 15;
    assert(f.flow.pointBlocksBedRoute(51.9f, 100));
    assert(!f.flow.pointBlocksBedRoute(52, 100));
}

void companionRouteAndRememberedTargetUseActorState() {
    Fixture f;
    f.flow.rememberCompanionTarget(61, 91);
    assert(f.companion.sleepSpotValid);
    assert(f.companion.sleepX == 61 && f.companion.sleepY == 91);
    assert(!f.main.sleepSpotValid);
    f.layout.hasAnchor = true;
    f.layout.preferredX = 60;
    f.layout.preferredY = 90;
    assert(f.flow.startCompanion(f.layout, f, 200));
    assert(f.companion.task == Home::Task::SEEK_SLEEP);
    assert(f.companion.targetX == 60 && f.companion.targetY == 90);
    assert(f.companion.sleepSpotValid);
    assert(f.companion.sleepX == 60 && f.companion.sleepY == 90);
    assert(f.simulation.owner(Home::Resource::BED) == -1);
    f.flow.enterSleep(1, 400);
    assert(f.companion.task == Home::Task::SLEEPING);
    assert(f.companion.motion == Home::MotionPhase::STILL);
    assert(f.companion.route.empty());
    assert(f.companion.sleepSpotValid);
    assert(f.simulation.validate() == 0);

    Fixture failed;
    failed.allowRoute[1] = false;
    assert(!failed.flow.startCompanion(failed.layout, failed, 200));
    assert(!failed.companion.sleepSpotValid);
    assert(failed.simulation.owner(Home::Resource::BED) == -1);
}

void holdIntentDoesNotMutateSleepState() {
    for (uint8_t id = 0; id < 2; ++id) {
        Fixture f;
        auto& actor = *f.simulation.actor(id);
        assert(f.simulation.acquire(Home::Resource::BED, id,
                                    Home::Task::SLEEPING, 100));
        f.flow.enterSleep(id, 100);
        actor.nextDecisionMs = 111;
        actor.nextMindUpdateMs = 222;
        actor.foodWakeRetryAfterMs = 333;
        actor.faintRestActive = true;
        actor.sleepSpotValid = true;
        actor.sleepX = 61;
        actor.sleepY = 91;
        actor.velocityX = 3;
        actor.velocityY = -4;
        actor.targetX = 70;
        actor.targetY = 80;
        for (Home::ActorIntent intent : {Home::ActorIntent::HOLD_SLEEP,
                                        Home::ActorIntent::NONE,
                                        Home::ActorIntent::SEEK_FOOD}) {
            assert(!f.flow.wake(id, intent, 60001));
            assert(actor.task == Home::Task::SLEEPING);
            assert(actor.nextDecisionMs == 111 && actor.nextMindUpdateMs == 222);
            assert(actor.foodWakeRetryAfterMs == 333);
            assert(actor.faintRestActive && actor.sleepSpotValid);
            assert(actor.sleepX == 61 && actor.sleepY == 91);
            assert(actor.velocityX == 3 && actor.velocityY == -4);
            assert(actor.targetX == 70 && actor.targetY == 80);
            assert(f.simulation.owner(Home::Resource::BED) == id);
        }
        Game::MonsterRuntime monster{};
        monster.satiety = 100;
        monster.mood = 0;
        actor.mind.update(monster, false, false, 60001);
        assert(actor.mind.topDesire() == MonsterDesire::WANDER);
    }
}

void wakingCleansStateAndPreservesActorSpecificTimers() {
    struct WakeCase {
        uint32_t now;
        Home::ActorIntent intent;
        uint32_t decision;
        uint32_t companionRetry;
    };
    constexpr WakeCase cases[] = {
        {60001, Home::ActorIntent::WAKE, 60701, 0},
        {60001, Home::ActorIntent::WAKE_FOR_FOOD, 60001, 120001},
        {0xffffff00U, Home::ActorIntent::WAKE, 444, 0},
        {0xffffff00U, Home::ActorIntent::WAKE_FOR_FOOD, 0xffffff00U, 59744},
    };
    for (uint8_t id = 0; id < 2; ++id) {
        for (const auto& test : cases) {
            Fixture f;
            auto& actor = *f.simulation.actor(id);
            assert(f.simulation.acquire(Home::Resource::BED, id,
                                        Home::Task::SLEEPING, 100));
            f.flow.enterSleep(id, 100);
            actor.route.count = 1;
            actor.velocityX = 3;
            actor.velocityY = -4;
            actor.targetX = 111;
            actor.targetY = 112;
            actor.faintRestActive = true;
            actor.sleepSpotValid = true;
            actor.nextMindUpdateMs = 123;
            actor.foodWakeRetryAfterMs = 456;
            actor.mind.reset(test.now - 60000U);
            assert(f.flow.wake(id, test.intent, test.now));
            assertStopped(actor, test.decision);
            assert(f.simulation.owner(Home::Resource::BED) == -1);
            if (id == 0) {
                assert(!actor.faintRestActive);
                assert(actor.nextMindUpdateMs == test.now);
            } else {
                assert(!actor.sleepSpotValid);
                assert(actor.nextMindUpdateMs == 123);
                assert(actor.foodWakeRetryAfterMs == test.companionRetry);
            }
            // Sixty idle seconds would favor wandering. Resting resets that
            // boredom through the real mind, without replacing its state.
            Game::MonsterRuntime monster{};
            monster.satiety = 100;
            monster.mood = 0;
            actor.mind.update(monster, false, false, test.now);
            assert(actor.mind.topDesire() == MonsterDesire::STARE);
            assert(f.simulation.validate() == 0);
        }
    }
}

void wakingPreservesTheMindsCurrentDesire() {
    for (uint8_t id = 0; id < 2; ++id) {
        Fixture f;
        auto& actor = *f.simulation.actor(id);
        f.flow.enterSleep(id, 100);
        Game::MonsterRuntime monster{};
        monster.satiety = 100;
        monster.mood = 50;
        actor.mind.update(monster, true, false, 200);
        assert(actor.mind.topDesire() == MonsterDesire::REST);
        assert(f.flow.wake(id, Home::ActorIntent::WAKE, 300));
        assert(actor.mind.topDesire() == MonsterDesire::REST);
    }
}
} // namespace

int main() {
    GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
    directSleepSkipsMovementAndClearsStaleRoute();
    routedSleepKeepsLeaseAndFailureReleasesIt();
    nearbyCompanionYieldsBeforeLeaderStarts();
    companionSpotsRespectFootprintBedBoundsAndSpacing();
    deterministicSelectionHonorsAnchorAndAvoidsActiveBedRoutes();
    routeClearanceUsesRemainingSegmentsAndStrictBoundary();
    companionRouteAndRememberedTargetUseActorState();
    holdIntentDoesNotMutateSleepState();
    wakingCleansStateAndPreservesActorSpecificTimers();
    wakingPreservesTheMindsCurrentDesire();
    GameRandom::setRangeProvider(nullptr);
    std::puts("sleep flow: direct/routed entry, bed leases, companion placement, "
              "route clearance, yielding, wake/hold and rollover pass");
}
