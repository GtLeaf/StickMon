#include <cassert>

#include "game/HomeCoordinator.h"

int main() {
    Home::Actor first;
    Home::Actor second;
    first.reset(Home::ActorRole::LEADER, 0, 100);
    second.reset(Home::ActorRole::TEAMMATE, 1, 100);
    first.active = true;
    second.active = true;

    Home::Coordinator coordinator;
    coordinator.attach(first, &second);

    first.task = Home::Task::SEEK_FOOD;
    second.task = Home::Task::SEEK_FOOD;
    assert(coordinator.acquire(
        Home::Resource::BOWL, 0, Home::Task::SEEK_FOOD, 110));
    assert(!coordinator.acquire(
        Home::Resource::BOWL, 1, Home::Task::SEEK_FOOD, 111));
    coordinator.release(Home::Resource::BOWL, 0);
    assert(coordinator.acquire(
        Home::Resource::BOWL, 1, Home::Task::SEEK_FOOD, 112));

    coordinator.stop(1, 120);
    assert(coordinator.owner(Home::Resource::BOWL) == Home::Coordinator::NO_ACTOR);

    first.task = Home::Task::IDLE;
    second.task = Home::Task::IDLE;
    assert(coordinator.beginPair(Home::PairActivity::TALK, 200));
    assert(first.task == Home::Task::PAIR_ACTION);
    assert(second.task == Home::Task::PAIR_ACTION);
    assert(!coordinator.acquire(
        Home::Resource::BOWL, 0, Home::Task::SEEK_FOOD, 201));
    assert((coordinator.validate() & Home::INCOMPLETE_PAIR_LOCK) == 0);

    coordinator.endPair(300, 25);
    assert(coordinator.pairActivity() == Home::PairActivity::NONE);
    assert(first.task == Home::Task::IDLE);
    assert(second.task == Home::Task::IDLE);
    assert(first.nextDecisionMs == 325);

    const uint32_t routeSignature = 0x1234;
    assert(coordinator.routeRetryAllowed(
        0, Home::Resource::BOWL, routeSignature));
    coordinator.rememberRouteFailure(
        0, Home::Resource::BOWL, routeSignature);
    assert(!coordinator.routeRetryAllowed(
        0, Home::Resource::BOWL, routeSignature));
    coordinator.notify(Home::WorldEvent::ACTOR_MOVED);
    assert(coordinator.routeRetryAllowed(
        0, Home::Resource::BOWL, routeSignature));

    first.task = Home::Task::SEEK_FOOD;
    assert(coordinator.acquire(
        Home::Resource::BOWL, 0, Home::Task::SEEK_FOOD, 400));
    first.task = Home::Task::IDLE;
    assert((coordinator.validate() & Home::STALE_RESOURCE_LEASE) != 0);
    coordinator.repair(401);
    assert(coordinator.owner(Home::Resource::BOWL) == Home::Coordinator::NO_ACTOR);

    second.active = false;
    coordinator.attach(first, nullptr);
    assert(!coordinator.beginPair(Home::PairActivity::CHASE, 500));
    assert((coordinator.validate() & Home::INCOMPLETE_PAIR_LOCK) == 0);
    return 0;
}
