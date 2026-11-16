#include "app/ExploreTransitionFlow.h"
#include "game/ExploreDestinations.h"
#include <cassert>
#include <cstdio>

using Flow = AmoledV1::ExploreTransitionFlow;
using Phase = Flow::Phase;
using Request = Flow::Request;

struct Fixture {
    Game::GameState game{};
    Home::Actor main, companion;
    Home::Simulation simulation;
    Flow flow{game, main, companion, simulation};
    RoomResource::Point polygon[4]{{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    Flow::Layout layout{true, true, 50, 80, 50, 110, polygon, 4};
    uint32_t now = 100, lastMotion = 100;
    Fixture(bool pair = true) {
        game.teamCount = pair ? 2 : 1;
        game.team[0].speciesId = 1;
        game.team[0].hpCur = 10;
        game.team[1].speciesId = 4;
        game.team[1].hpCur = 10;
        main.reset(Home::ActorRole::LEADER, 0, now);
        companion.reset(Home::ActorRole::TEAMMATE, 1, now);
        main.active = true;
        companion.active = pair;
        main.x = 50;
        main.y = 79;
        companion.x = 40;
        companion.y = 70;
        simulation.attach(main, pair ? &companion : nullptr);
        simulation.reset(now);
    }
    Flow::Effects tick(uint32_t delta = 50, bool present = true, bool home = true) {
        now += delta;
        flow.update(layout, {home, lastMotion}, now);
        auto effects = flow.takeEffects();
        if (effects.motionClock)
            lastMotion = now;
        if (present)
            flow.markPresented(now);
        return effects;
    }
    void reach(Phase phase) {
        for (unsigned i = 0; flow.state().phase != phase && i < 500; ++i)
            tick();
        assert(flow.state().phase == phase);
    }
};

int main() {
    // Leader exits before companion; neither destination loading nor settlement
    // may be requested until the final black frame was acknowledged.
    Fixture f;
    assert(f.flow.queue(0, false));
    f.reach(Phase::WALK_COMPANION_TO_DOOR);
    assert(f.flow.state().mainHidden && !f.flow.state().companionHidden);
    f.reach(Phase::FADE_OUT);
    assert(f.flow.state().companionHidden);
    while (f.flow.fade().alpha() != 255) {
        f.flow.markPresented(f.now);
        assert(f.tick(50, false).request == Request::NONE);
    }
    assert(f.tick(1000, false).request == Request::NONE);
    f.flow.markPresented(f.now);
    assert(f.tick(1, false).request == Request::START_DESTINATION);
    assert(f.tick(100, false).request == Request::NONE);
    assert(f.flow.takeEffects().request == Request::NONE);
    f.flow.destinationStarted(true);
    assert(f.flow.fade().alpha() == 255);
    assert(f.tick(2000, false, false).request == Request::NONE);
    assert(f.flow.fade().alpha() == 255);
    f.reach(Phase::NONE);
    assert(f.flow.beginReturn());
    assert(!f.flow.beginReturn());
    bool settled = false;
    for (unsigned i = 0; i < 30; ++i)
        if (f.tick().request == Request::SETTLE_RETURN) {
            settled = true;
            break;
        }
    assert(settled);
    assert(f.tick().request == Request::NONE);
    f.flow.returnSettled(f.layout, f.now);
    assert(f.flow.state().companionHidden);
    assert(f.flow.takeEffects().placeFainted);
    f.reach(Phase::RETURN_WALK_IN);
    // A blocked/missing clearance route still lets the companion enter.
    f.reach(Phase::RETURN_COMPANION_WALK_IN);
    assert(!f.flow.state().companionHidden);
    f.reach(Phase::NONE);
    assert(f.main.task == Home::Task::IDLE && f.companion.task == Home::Task::IDLE);
    assert(!f.flow.state().pending);

    Fixture cancel;
    cancel.flow.queue(0, true);
    cancel.reach(Phase::CROSS_DOOR);
    cancel.tick();
    cancel.flow.cancel(cancel.now);
    assert(cancel.flow.state().phase == Phase::NONE && !cancel.flow.state().pending);
    assert(cancel.main.x == 50 && cancel.main.y == 80);
    assert(!cancel.flow.state().mainHidden && !cancel.flow.state().companionHidden);
    assert(cancel.flow.takeEffects().endSession);
    assert(!cancel.flow.takeEffects().endSession);

    Fixture failed(false);
    failed.layout.doorValid = false;
    failed.flow.queue(0, false);
    failed.reach(Phase::FADE_OUT);
    for (unsigned i = 0; i < 20; ++i)
        if (failed.tick().request == Request::START_DESTINATION)
            break;
    failed.flow.destinationStarted(false);
    assert(!failed.flow.state().mainHidden);
    failed.reach(Phase::NONE);

    for (unsigned mask = 0; mask < 4; ++mask) {
        Fixture faint;
        faint.game.team[0].fainted = (mask & 1) != 0;
        faint.game.team[1].fainted = (mask & 2) != 0;
        faint.flow.queue(ExploreDestinations::MARKET,
                         false); // Market permits an entirely fainted party.
        faint.reach(Phase::FADE_OUT);
        assert(faint.flow.state().mainHidden == !(mask & 1));
        assert(faint.flow.state().companionHidden == !(mask & 2));
    }
    // Return setup is fenced by black presentation and settlement acknowledgement.
    const auto returnToDoor = [](Fixture &value) {
        assert(value.flow.beginReturn());
        bool requested = false;
        for (unsigned i = 0; i < 30; ++i)
            if (value.tick().request == Request::SETTLE_RETURN) {
                requested = true;
                break;
            }
        assert(requested);
        value.flow.returnSettled(value.layout, value.now);
        const float x = value.main.x, y = value.main.y;
        value.flow.returnSettled(value.layout, value.now + 500);
        assert(value.main.x == x && value.main.y == y); // Repeated ACK is inert.
        value.flow.takeEffects();
        value.reach(Phase::RETURN_WALK_IN);
    };
    Fixture leaderTimeout(false);
    returnToDoor(leaderTimeout);
    leaderTimeout.main.targetY = 10000;
    leaderTimeout.tick(7999);
    assert(leaderTimeout.flow.state().phase == Phase::RETURN_WALK_IN);
    auto leaderEnd = leaderTimeout.tick(1);
    assert(leaderTimeout.flow.state().phase == Phase::NONE);
    assert(leaderTimeout.main.y == 10000 && leaderEnd.endSession && leaderEnd.scheduleMain);

    Fixture companionTimeout;
    returnToDoor(companionTimeout);
    companionTimeout.reach(Phase::RETURN_COMPANION_WALK_IN);
    companionTimeout.companion.targetY = 10000;
    auto pairEnd = companionTimeout.tick(8000);
    assert(companionTimeout.flow.state().phase == Phase::NONE);
    assert(companionTimeout.companion.y == 10000 && pairEnd.schedulePair);
    assert(pairEnd.companion.frame == Flow::Frame::IDLE);

    Fixture clearanceTimeout;
    int16_t parent[RoomNavigator::MAX_NODES]{};
    uint16_t queue[RoomNavigator::MAX_NODES]{};
    Home::NavigationWorld world;
    world.polygon = clearanceTimeout.polygon;
    world.polygonCount = 4;
    world.footBounds = {0, 0, 100, 100};
    world.scratch = {parent, queue, RoomNavigator::MAX_NODES};
    clearanceTimeout.simulation.setNavigation(world);
    returnToDoor(clearanceTimeout);
    clearanceTimeout.reach(Phase::RETURN_CLEAR_MAIN);
    auto clearanceEnd = clearanceTimeout.tick(8000);
    assert(clearanceTimeout.flow.state().phase == Phase::RETURN_COMPANION_WALK_IN);
    assert(clearanceTimeout.main.x == clearanceTimeout.main.targetX);
    assert(clearanceTimeout.main.y == clearanceTimeout.main.targetY);
    assert(clearanceEnd.trace == Flow::Trace::CLEARANCE_TIMEOUT);

    for (bool cancelRequest : {false, true}) {
        Fixture fallback;
        returnToDoor(fallback);
        // No valid deterministic clearance point: App must supply a random goal.
        fallback.polygon[0] = {49,79}; fallback.polygon[1] = {51,79};
        fallback.polygon[2] = {51,81}; fallback.polygon[3] = {49,81};
        Flow::Effects request;
        for (unsigned i = 0; i < 100; ++i) {
            request = fallback.tick();
            if (request.request == Request::FIND_CLEARANCE)
                break;
        }
        assert(request.request == Request::FIND_CLEARANCE);
        assert(request.clearanceSeparation == 26.0f);
        const float x = fallback.main.x, y = fallback.main.y;
        assert(fallback.tick().request == Request::NONE);
        assert(fallback.main.x == x && fallback.main.y == y);
        if (cancelRequest)
            fallback.flow.cancel(fallback.now);
        fallback.flow.clearanceChosen(true, 70, 50, fallback.now);
        if (cancelRequest) {
            assert(fallback.flow.state().phase == Phase::NONE);
            assert(fallback.main.x == x && fallback.main.y == y);
        } else {
            assert(fallback.main.x == 70 && fallback.main.y == 50);
            assert(fallback.flow.state().phase == Phase::RETURN_COMPANION_WALK_IN);
            fallback.flow.clearanceChosen(true, 10, 10, fallback.now);
            assert(fallback.main.x == 70 && fallback.main.y == 50);
            fallback.flow.takeEffects();
            fallback.reach(Phase::NONE);
        }
    }
    Fixture cancelledLoad(false);
    cancelledLoad.layout.doorValid = false;
    cancelledLoad.flow.queue(0, false);
    bool load = false;
    for (unsigned i = 0; i < 30; ++i)
        if (cancelledLoad.tick().request == Request::START_DESTINATION) {
            load = true;
            break;
        }
    assert(load);
    cancelledLoad.flow.cancel(cancelledLoad.now);
    cancelledLoad.flow.destinationStarted(true);
    assert(cancelledLoad.flow.state().phase == Phase::NONE && !cancelledLoad.flow.fade().active());
    Fixture wrap(false);
    wrap.now = 0xffffff00U;
    wrap.lastMotion = wrap.now;
    wrap.flow.queue(0, false);
    wrap.reach(Phase::FADE_OUT);
    assert(wrap.now < 0xffffff00U);
    std::puts("transition flow: serial movement, presentation fences, one-shot effects, "
              "cancellation, failure, fainting and wrap verified");
}
