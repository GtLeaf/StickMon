#include <cassert>
#include <initializer_list>

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

    // Default turn completion must resume even an intent below TURNING's
    // priority. Route ownership stays with the intent throughout the pause.
    for (Home::Task task : {Home::Task::WANDER, Home::Task::IDLE,
                            Home::Task::YIELDING}) {
        coordinator.stop(0, 410);
        const bool usesRoute = Home::taskUsesRoute(task);
        first.route.count = usesRoute ? 1 : 0;
        first.route.index = 0;
        first.route.x[0] = 125.0f;
        first.route.y[0] = 110.0f;
        assert(coordinator.beginTurn(0, task, 420, 150));
        assert(first.task == Home::Task::TURNING);
        assert(first.resumeTask == task);
        assert(first.motion == Home::MotionPhase::TURNING);
        assert(first.taskUntilMs == 570);
        assert(coordinator.finishTurn(0, 570));
        assert(first.task == task && first.resumeTask == task);
        assert(first.motion == (usesRoute ? Home::MotionPhase::ROUTE
                                         : Home::MotionPhase::STILL));
        assert(first.route.count == (usesRoute ? 1 : 0));
        if (usesRoute) {
            assert(first.route.x[0] == 125.0f && first.route.y[0] == 110.0f);
        }
        assert(coordinator.validate() == Home::INVARIANT_OK);
    }

    coordinator.stop(0, 600);
    assert(coordinator.transition(0, Home::Task::SEEK_FOOD, 610));
    assert(coordinator.acquire(
        Home::Resource::BOWL, 0, Home::Task::SEEK_FOOD, 610));
    first.route.count = 1;
    first.route.x[0] = 135.0f;
    first.route.y[0] = 100.0f;
    assert(coordinator.beginTurn(0, Home::Task::SEEK_FOOD, 620, 150));
    assert(coordinator.owner(Home::Resource::BOWL) == 0);
    assert(coordinator.finishTurn(0, 770));
    assert(first.task == Home::Task::SEEK_FOOD && first.route.count == 1);
    assert(coordinator.owner(Home::Resource::BOWL) == 0);
    assert(coordinator.validate() == Home::INVARIANT_OK);

    // Turn completion cannot resume after an interrupt, and it must not
    // weaken priority checks on normal transitions or preempt an active pair.
    assert(coordinator.beginTurn(0, Home::Task::SEEK_FOOD, 780, 150));
    assert(coordinator.transition(0, Home::Task::FAINTED, 790));
    assert(!coordinator.finishTurn(0, 930));
    assert(!coordinator.transition(0, Home::Task::WANDER, 930));
    assert(first.task == Home::Task::FAINTED);
    coordinator.stop(0, 940);
    coordinator.stop(1, 940);
    assert(coordinator.beginPair(Home::PairActivity::TALK, 950));
    assert(!coordinator.transition(0, Home::Task::WANDER, 960));
    first.beginTask(Home::Task::TURNING, 960, 150);
    first.resumeTask = Home::Task::IDLE;
    assert(!coordinator.finishTurn(0, 1110));
    assert(coordinator.pairActivity() == Home::PairActivity::TALK);
    coordinator.endPair(1120);
    coordinator.stop(0, 1120);

    second.active = false;
    coordinator.attach(first, nullptr);
    assert(!coordinator.beginPair(Home::PairActivity::CHASE, 500));
    assert((coordinator.validate() & Home::INCOMPLETE_PAIR_LOCK) == 0);
    return 0;
}
