#include <cassert>

#include "game/HomeSimulation.h"

int main() {
    const RoomResource::Point polygon[] = {
        {0, 0}, {100, 0}, {100, 100}, {0, 100},
    };
    int16_t parent[RoomNavigator::MAX_NODES] = {};
    uint16_t queue[RoomNavigator::MAX_NODES] = {};

    Home::Actor leader;
    Home::Actor companion;
    leader.reset(Home::ActorRole::LEADER, 0, 100);
    companion.reset(Home::ActorRole::TEAMMATE, 1, 100);
    leader.active = true;
    companion.active = true;
    leader.x = 20.0f;
    leader.y = 50.0f;
    companion.x = 50.0f;
    companion.y = 50.0f;

    Home::Simulation simulation;
    simulation.attach(leader, &companion);
    Home::NavigationWorld world;
    world.polygon = polygon;
    world.polygonCount = 4;
    world.footBounds = {0.0f, 0.0f, 100.0f, 100.0f};
    world.actorCollisionMode = Home::ActorCollisionMode::SOFT;
    world.scratch = {
        parent, queue, RoomNavigator::MAX_NODES,
    };
    simulation.setNavigation(world);
    simulation.reset(100);

    assert(simulation.acquire(Home::Resource::BOWL, 1,
                              Home::Task::SEEK_FOOD, 110));
    assert(simulation.bowlSession().phase == Home::BowlPhase::RESERVED);
    assert(simulation.transition(1, Home::Task::SEEK_FOOD, 111));
    assert(simulation.bowlSession().phase == Home::BowlPhase::APPROACHING);
    assert(simulation.transition(1, Home::Task::FEEDING, 112, 5000, true));
    assert(simulation.bowlSession().phase == Home::BowlPhase::FEEDING);

    assert(simulation.beginBowlClearance(1, 200));
    assert(simulation.owner(Home::Resource::BOWL) ==
           Home::Coordinator::NO_ACTOR);
    assert(simulation.bowlSession().clearing());
    assert(!simulation.bowlClaimAllowed(0));
    assert(!simulation.acquire(Home::Resource::BOWL, 0,
                               Home::Task::SEEK_FOOD, 201));

    assert(simulation.transition(1, Home::Task::YIELDING, 202, 0, true));
    simulation.stop(1, 203);
    assert(simulation.bowlSession().clearing());
    simulation.finishBowlClearance(1, 204);
    assert(!simulation.bowlSession().active());
    assert(simulation.bowlClaimAllowed(0));
    assert(simulation.acquire(Home::Resource::BOWL, 0,
                              Home::Task::SEEK_FOOD, 205));
    assert(simulation.planRoute(0, 80.0f, 80.0f));
    const uint8_t foodRouteCount = leader.route.count;
    assert(foodRouteCount > 0);
    assert(simulation.transitionPreparedRoute(
        0, Home::Task::SEEK_FOOD, 206));
    assert(leader.route.count == foodRouteCount);
    assert(simulation.beginTurn(
        0, Home::Task::SEEK_FOOD, 207, 120));
    assert(leader.task == Home::Task::TURNING);
    assert(leader.resumeTask == Home::Task::SEEK_FOOD);
    assert(simulation.owner(Home::Resource::BOWL) == 0);
    assert(simulation.finishTurn(0, 327));
    assert(leader.task == Home::Task::SEEK_FOOD);
    assert(leader.route.count == foodRouteCount);
    assert(simulation.owner(Home::Resource::BOWL) == 0);

    simulation.stop(0, 400);
    assert(simulation.acquire(Home::Resource::BED, 0,
                              Home::Task::SEEK_SLEEP, 401));
    assert(simulation.transition(
        0, Home::Task::SLEEPING, 402, 0, true));
    assert(simulation.owner(Home::Resource::BED) == 0);
    simulation.stop(0, 403);
    assert(simulation.owner(Home::Resource::BED) ==
           Home::Coordinator::NO_ACTOR);

    simulation.setControlOwner(0, Home::ControlOwner::CLAW);
    assert(!simulation.autonomousAllowed(0));
    assert(simulation.autonomousAllowed(1));

    const Home::RenderSnapshot snapshot = simulation.renderSnapshot();
    assert(snapshot.actorCount == 2);
    assert(snapshot.actors[0].active);
    assert(snapshot.actors[1].teamSlot == 1);
    assert(snapshot.bowl.actorId == Home::Coordinator::NO_ACTOR);
    return 0;
}
