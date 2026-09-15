#include <cassert>
#include <cmath>

#include "game/HomeRuntime.h"

int main() {
    const RoomResource::Point polygon[] = {
        {0, 0}, {100, 0}, {100, 100}, {0, 100},
    };
    int16_t parent[RoomNavigator::MAX_NODES] = {};
    uint16_t queue[RoomNavigator::MAX_NODES] = {};

    Home::Actor leader;
    Home::Actor companion;
    leader.reset(Home::ActorRole::LEADER, 0, 0);
    companion.reset(Home::ActorRole::TEAMMATE, 1, 0);
    leader.active = true;
    companion.active = true;
    leader.x = 12.0f;
    leader.y = 50.0f;
    companion.x = 50.0f;
    companion.y = 50.0f;
    leader.geometry.groundOffsetY = 0.0f;
    leader.geometry.footprint.radiusX = 3.0f;
    leader.geometry.footprint.radiusY = 3.0f;
    companion.geometry = leader.geometry;

    Home::Runtime runtime;
    runtime.attach(leader, &companion);
    Home::NavigationWorld world;
    world.polygon = polygon;
    world.polygonCount = 4;
    world.footBounds = {0.0f, 0.0f, 100.0f, 100.0f};
    world.actorMinSeparation = 12.0f;
    world.scratch.parent = parent;
    world.scratch.queue = queue;
    world.scratch.capacity = RoomNavigator::MAX_NODES;
    runtime.setNavigation(world);

    assert(runtime.planRoute(0, 88.0f, 50.0f));
    assert(leader.route.count >= 2);
    Home::RouteStep result = Home::RouteStep::MOVING;
    for (int frame = 0; frame < 300 &&
         result != Home::RouteStep::ARRIVED; ++frame) {
        result = runtime.advanceRoute(
            0, static_cast<uint32_t>(frame * 50), 20.0f, 0.05f);
        assert(result != Home::RouteStep::BLOCKED);
        const float dx = leader.x - companion.x;
        const float dy = leader.y - companion.y;
        assert(std::sqrt(dx * dx + dy * dy) >= 11.9f);
    }
    assert(result == Home::RouteStep::ARRIVED);
    assert(std::fabs(leader.x - 88.0f) < 0.01f);
    assert(std::fabs(leader.y - 50.0f) < 0.01f);

    // AMOLED uses soft actor avoidance: the companion is not an A* wall and
    // the moving actor steers around the rendered footprint at runtime.
    leader.stop(1000, 0);
    companion.stop(1000, 0);
    leader.x = 12.0f;
    leader.y = 50.0f;
    companion.x = 50.0f;
    companion.y = 50.0f;
    world.actorCollisionMode = Home::ActorCollisionMode::SOFT;
    world.actorOverlapPadding = 1.0f;
    runtime.setNavigation(world);
    assert(runtime.planRoute(0, 88.0f, 50.0f));
    result = Home::RouteStep::MOVING;
    for (int frame = 0; frame < 300 &&
         result != Home::RouteStep::ARRIVED; ++frame) {
        result = runtime.advanceRoute(
            0, 1000U + static_cast<uint32_t>(frame * 50), 20.0f, 0.05f);
        assert(result != Home::RouteStep::BLOCKED);
    }
    assert(result == Home::RouteStep::ARRIVED);
    assert(std::fabs(leader.x - 88.0f) < 0.01f);
    assert(std::fabs(leader.y - 50.0f) < 0.01f);

    // A delayed frame must not let a large step tunnel through the other
    // actor. The soft solver should choose a side step instead.
    leader.stop(1050, 0);
    companion.stop(1050, 0);
    leader.x = 20.0f;
    leader.y = 100.0f;
    companion.x = 100.0f;
    companion.y = 100.0f;
    const RoomResource::Point largePolygon[] = {
        {0, 0}, {200, 0}, {200, 200}, {0, 200},
    };
    world.polygon = largePolygon;
    world.polygonCount = 4;
    world.footBounds = {0.0f, 0.0f, 200.0f, 200.0f};
    world.actorOverlapPadding = 1.0f;
    runtime.setNavigation(world);
    assert(runtime.planRoute(0, 180.0f, 100.0f));
    result = runtime.advanceRoute(0, 2000, 600.0f, 0.1f);
    assert(result != Home::RouteStep::BLOCKED);
    assert(!runtime.overlapsOther(0, leader.x, leader.y));
    assert(leader.y > 100.0f || leader.x < 100.0f);

    // Two ordinary wander routes may cross. Neither actor should turn a
    // temporary visual encounter into a permanent BLOCKED state.
    leader.stop(1100, 0);
    companion.stop(1100, 0);
    leader.x = 20.0f;
    leader.y = 50.0f;
    companion.x = 80.0f;
    companion.y = 50.0f;
    assert(runtime.planRoute(0, 68.0f, 50.0f));
    assert(runtime.planRoute(1, 32.0f, 50.0f));
    Home::RouteStep leaderResult = Home::RouteStep::MOVING;
    Home::RouteStep companionResult = Home::RouteStep::MOVING;
    for (int frame = 0; frame < 400 &&
         (leaderResult != Home::RouteStep::ARRIVED ||
          companionResult != Home::RouteStep::ARRIVED); ++frame) {
        if (leaderResult != Home::RouteStep::ARRIVED) {
            leaderResult = runtime.advanceRoute(
                0, 3000U + static_cast<uint32_t>(frame * 50),
                20.0f, 0.05f);
            assert(leaderResult != Home::RouteStep::BLOCKED);
        }
        if (companionResult != Home::RouteStep::ARRIVED) {
            companionResult = runtime.advanceRoute(
                1, 3000U + static_cast<uint32_t>(frame * 50),
                20.0f, 0.05f);
            assert(companionResult != Home::RouteStep::BLOCKED);
        }
    }
    assert(leaderResult == Home::RouteStep::ARRIVED);
    assert(companionResult == Home::RouteStep::ARRIVED);

    // Reproduce the AMOLED lockstep encounter captured on device: full-size
    // footprints, the standard room polygon, and the companion updated first.
    const RoomResource::Point roomPolygon[] = {
        {120, 70}, {110, 74}, {93, 90}, {65, 92},
        {61, 115}, {45, 124}, {133, 155}, {231, 108},
    };
    leader.stop(23000, 0);
    companion.stop(23000, 0);
    leader.geometry.groundOffsetY = 0.0f;
    leader.geometry.footprint = {12.0f, 5.0f};
    companion.geometry = leader.geometry;
    leader.x = 85.2f;
    leader.y = 122.7f;
    companion.x = 60.1f;
    companion.y = 124.7f;
    world.polygon = roomPolygon;
    world.polygonCount = 8;
    world.footBounds = {45.0f, 70.0f, 231.0f, 155.0f};
    world.actorCollisionMode = Home::ActorCollisionMode::SOFT;
    world.actorOverlapPadding = 1.0f;
    runtime.setNavigation(world);
    assert(runtime.transition(0, Home::Task::WANDER, 23000));
    assert(runtime.transition(1, Home::Task::WANDER, 23000));
    assert(runtime.planRoute(0, 78.0f, 122.0f));
    assert(runtime.planRoute(1, 134.0f, 92.0f));
    leaderResult = Home::RouteStep::MOVING;
    companionResult = Home::RouteStep::MOVING;
    bool leaderResolved = false;
    bool companionResolved = false;
    for (int frame = 0;
         frame < 800 && (!leaderResolved || !companionResolved); ++frame) {
        const uint32_t nowMs = 23000U + static_cast<uint32_t>(frame * 50);
        if (!companionResolved) {
            companionResult = runtime.advanceRoute(
                1, nowMs, 7.56f, 0.05f, 0.8f, true);
            companionResolved =
                companionResult == Home::RouteStep::ARRIVED ||
                companionResult == Home::RouteStep::BLOCKED;
            if (companionResult == Home::RouteStep::BLOCKED) {
                runtime.stop(1, nowMs, 700);
            }
        }
        if (!leaderResolved) {
            leaderResult = runtime.advanceRoute(
                0, nowMs, 7.56f, 0.05f, 0.8f, true);
            leaderResolved = leaderResult == Home::RouteStep::ARRIVED ||
                leaderResult == Home::RouteStep::BLOCKED;
            if (leaderResult == Home::RouteStep::BLOCKED) {
                runtime.stop(0, nowMs, 700);
            }
        }
        assert(!runtime.overlapsOther(0, leader.x, leader.y));
    }
    assert(leaderResolved);
    assert(companionResolved);

    // A route ending inside a stationary actor can never arrive. Soft
    // steering must eventually report BLOCKED instead of circling forever.
    leader.stop(65000, 0);
    companion.stop(65000, 0);
    leader.x = 20.0f;
    leader.y = 100.0f;
    companion.x = 100.0f;
    companion.y = 100.0f;
    world.polygon = largePolygon;
    world.polygonCount = 4;
    world.footBounds = {0.0f, 0.0f, 200.0f, 200.0f};
    runtime.setNavigation(world);
    assert(runtime.transition(0, Home::Task::WANDER, 65000));
    assert(runtime.planRoute(0, companion.x, companion.y));
    result = Home::RouteStep::MOVING;
    for (int frame = 0; frame < 240 &&
         result == Home::RouteStep::MOVING; ++frame) {
        result = runtime.advanceRoute(
            0, 65000U + static_cast<uint32_t>(frame * 50),
            10.0f, 0.05f, 0.8f, true);
    }
    assert(result == Home::RouteStep::BLOCKED);

    leader.stop(1000, 0);
    companion.stop(1000, 0);
    assert(runtime.acquire(Home::Resource::BOWL, 0,
                           Home::Task::SEEK_FOOD, 1000));
    assert(!runtime.acquire(Home::Resource::BOWL, 1,
                            Home::Task::SEEK_FOOD, 1000));
    runtime.release(Home::Resource::BOWL, 0);
    assert(runtime.beginPair(Home::PairActivity::TALK, 1100));
    assert(runtime.pairActive());
    assert(runtime.transition(1, Home::Task::SEEK_FOOD, 1200));
    assert(!runtime.pairActive());
    assert(companion.task == Home::Task::SEEK_FOOD);
    assert(leader.task == Home::Task::IDLE);
    assert(runtime.validate() == Home::INVARIANT_OK);

    leader.beginTask(Home::Task::WANDER, 1200);
    leader.route.x[0] = 40.0f;
    leader.route.y[0] = 40.0f;
    leader.route.count = 1;
    assert(!leader.route.empty());
    assert(runtime.transition(0, Home::Task::ROOM_ACTION, 1201));
    assert(leader.route.empty());

    leader.beginTask(Home::Task::FEEDING, 1300);
    leader.route.x[0] = leader.x;
    leader.route.y[0] = leader.y;
    leader.route.count = 1;
    assert((runtime.validate() & Home::IDLE_ACTOR_HAS_ROUTE) != 0);
    runtime.repair(1300);
    assert(leader.route.empty());
    assert(runtime.validate() == Home::INVARIANT_OK);
    return 0;
}
