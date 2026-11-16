#include "app/HomeFoodFlow.h"
#include "game/GameRandom.h"
#include <cassert>
#include <cstdio>
#include <vector>

using Flow = AmoledV1::HomeFoodFlow;
struct Fixture : Flow::Movement {
    Game::GameState game{};
    Home::Actor main, companion;
    Home::Simulation simulation;
    Flow flow{game, main, companion, simulation};
    Flow::Layout layout{50, 50};
    Home::HouseholdObservation observation{};
    bool routes = false;
    Fixture() {
        game.teamCount = 2;
        game.room.bowlFood = 0;
        game.room.bowlCount = 1;
        game.room.bowlBitesRemaining = 3;
        for (unsigned i = 0; i < 2; ++i) {
            game.team[i].speciesId = i ? 4 : 1;
            game.team[i].hpCur = 10;
            game.team[i].satiety = i ? 0 : 25;
        }
        main.reset(Home::ActorRole::LEADER, 0, 100);
        companion.reset(Home::ActorRole::TEAMMATE, 1, 100);
        main.active = companion.active = true;
        main.geometry.groundOffsetY = companion.geometry.groundOffsetY = 0;
        main.x = 80;
        main.y = 50;
        companion.x = 50;
        companion.y = 50;
        simulation.attach(main, &companion);
        simulation.reset(100);
        observe();
    }
    void observe() {
        observation.actorCount = 2;
        for (unsigned i = 0; i < 2; ++i) {
            auto &o = observation.actors[i];
            const auto &a = i ? companion : main;
            o.active = true;
            o.controlAvailable = true;
            o.canMove = true;
            o.needsFood = true;
            o.foodActionAvailable = true;
            o.foodRetryReady = true;
            o.feedTarget = 70;
            o.satiety = game.team[i].satiety;
            o.task = a.task;
            o.resumeTask = a.resumeTask;
            o.visitor = game.team[i].origin == Game::Origin::VISITOR;
        }
    }
    bool chooseMainApproach(float x, float y, float &outX, float &outY) override {
        outX = x;
        outY = y;
        return routes;
    }
    bool begin(uint8_t actor, Home::Task task, float x, float y, uint32_t now) override {
        if (!routes)
            return false;
        return simulation.planRoute(actor, x, y) &&
               simulation.transitionPreparedRoute(actor, task, now);
    }
};
int main() {
    GameRandom::setRangeProvider(+[](uint32_t min, uint32_t) { return min; });
    Fixture f;
    assert(f.flow.preferred(f.observation) == 1);
    assert(!f.flow.claim(0, f.layout, f.observation, 100));
    assert(f.flow.startSeeking(1, f.layout, f.observation, f, 100));
    assert(f.simulation.owner(Home::Resource::BOWL) == 1);
    assert(f.companion.task == Home::Task::FEEDING);
    auto effects = f.flow.takeEffects();
    assert(effects.feedingMask == 2);
    assert(f.flow.takeEffects().feedingMask == 0);
    auto early = f.flow.tickFeeding(1, 799);
    assert(!early.bite.consumed);
    auto bite = f.flow.tickFeeding(1, 800);
    assert(bite.bite.consumed);
    assert(f.game.team[1].satiety == 25 && f.game.team[0].satiety == 25);
    assert(f.game.room.bowlBitesRemaining == 2);
    assert(!f.flow.tickFeeding(1, 800).bite.consumed);
    assert(f.flow.tickFeeding(1, 1800).bite.consumed);
    assert(f.flow.tickFeeding(1, 2800).bite.consumed);
    assert(f.flow.tickFeeding(1, 2801).finished);
    f.flow.finishFeeding(1, 2801);
    assert(f.simulation.owner(Home::Resource::BOWL) == -1);
    assert(f.simulation.bowlSession().clearing());
    assert(!f.flow.claim(0, f.layout, f.observation, 2801));
    assert(f.flow.takeEffects().finishedMask == 2);

    Fixture visitor;
    visitor.game.team[1].origin = Game::Origin::VISITOR;
    visitor.observe();
    assert(visitor.flow.preferred(visitor.observation) == 0);
    assert(!visitor.flow.startSeeking(1, visitor.layout, visitor.observation, visitor, 100));
    visitor.companion.hidden = true;
    visitor.flow.scheduleArbitration(100);
    visitor.flow.service(visitor.layout, visitor.observation, visitor, 100);
    assert(visitor.simulation.owner(Home::Resource::BOWL) == -1); // Failed approach releases lease.
    assert(visitor.flow.state().pending && visitor.flow.state().retryAtMs == 1100);
    assert(visitor.flow.takeEffects().yieldActor == 0);
    visitor.flow.service(visitor.layout, visitor.observation, visitor, 1099);
    assert(visitor.flow.takeEffects().yieldActor == -1);

    Fixture wrap;
    wrap.game.team[0].satiety = 100;
    wrap.observe();
    assert(wrap.flow.startSeeking(1, wrap.layout, wrap.observation, wrap, 0xffffff00U));
    assert(!wrap.flow.tickFeeding(1, 443).bite.consumed);
    assert(wrap.flow.tickFeeding(1, 444).bite.consumed);
    wrap.flow.servingAdded(false, 445);
    assert(wrap.simulation.owner(Home::Resource::BOWL) == -1);
    assert(wrap.flow.state().pending && wrap.flow.state().retryAtMs == 445);
    assert(wrap.companion.task == Home::Task::IDLE);
    assert(wrap.flow.takeEffects().stopCompanion);
    // Preserve different RNG ordering for the two actors.
    static std::vector<uint32_t> delays;
    GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) {
        delays.push_back(minimum);
        return minimum;
    });
    Fixture leader;
    leader.companion.hidden = true;
    leader.observation.actors[1].active = false;
    leader.main.x = leader.layout.foodX;
    leader.main.y = leader.layout.foodY;
    assert(leader.flow.startSeeking(0, leader.layout, leader.observation, leader, 100));
    assert((delays == std::vector<uint32_t>{700, 3200}));
    assert(leader.flow.takeEffects().seekTrace == Flow::SeekTrace::DIRECT);
    assert(!leader.flow.tickFeeding(0, 799).bite.consumed);
    assert(leader.flow.tickFeeding(0, 800).bite.consumed);
    assert(leader.game.team[0].satiety == 50);
    assert(!leader.flow.tickFeeding(0, 800).bite.consumed);
    leader.flow.servingAdded(true, 801);
    assert(leader.main.task == Home::Task::IDLE);
    assert(leader.flow.takeEffects().stopMain);
    assert(leader.main.nextMindUpdateMs == 801 && leader.main.nextDecisionMs == 801);
    delays.clear();
    Fixture reserve;
    assert(reserve.flow.startSeeking(1, reserve.layout, reserve.observation, reserve, 100));
    assert((delays == std::vector<uint32_t>{3200, 700}));

    // A sleeping hungry actor wakes through App before arbitration retries.
    Fixture sleeping;
    sleeping.observation.actors[1].sleeping = true;
    sleeping.observation.actors[1].sleepTime = true;
    sleeping.observation.actors[1].wakeForFood = true;
    sleeping.flow.scheduleArbitration(100);
    sleeping.flow.service(sleeping.layout, sleeping.observation, sleeping, 100);
    assert(sleeping.flow.state().pending && sleeping.flow.state().retryAtMs == 180);
    assert(sleeping.simulation.owner(Home::Resource::BOWL) == -1);
    assert(sleeping.flow.takeEffects().yieldActor == -1);
    sleeping.observation.actors[1].sleeping = false;
    sleeping.flow.service(sleeping.layout, sleeping.observation, sleeping, 179);
    assert(sleeping.simulation.owner(Home::Resource::BOWL) == -1);
    sleeping.flow.service(sleeping.layout, sleeping.observation, sleeping, 180);
    assert(!sleeping.flow.state().pending);
    assert(sleeping.simulation.owner(Home::Resource::BOWL) == 1);

    f.companion.x = 100;
    f.flow.companionYieldArrived(f.layout, 3000);
    assert(!f.simulation.bowlSession().active());
    f.game.room.bowlCount = 1;
    f.observe();
    assert(f.flow.claim(0, f.layout, f.observation, 3001));
    assert(f.simulation.owner(Home::Resource::BOWL) == 0);

    // Routed arrival retains its route goal; only direct feeding retargets in place.
    Fixture arrived;
    arrived.main.targetX = 49.5f;
    arrived.main.targetY = 50.5f;
    arrived.flow.enterFeeding(0, 100);
    assert(arrived.main.targetX == 49.5f && arrived.main.targetY == 50.5f);

    Fixture empty;
    empty.game.room.bowlCount = 0;
    empty.flow.scheduleArbitration(100);
    empty.flow.service(empty.layout, empty.observation, empty, 100);
    assert(!empty.flow.state().pending);
    assert(empty.simulation.owner(Home::Resource::BOWL) == -1);
    GameRandom::setRangeProvider(nullptr);
    std::puts("food flow: arbitration, exclusive owner, bites, clearance, visitor, failed route "
              "and rollover pass");
}
