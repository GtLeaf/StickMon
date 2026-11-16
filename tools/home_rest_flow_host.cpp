#include "app/HomeRestFlow.h"
#include "game/GameRandom.h"
#include <cassert>
#include <cstdio>

namespace {
using Flow = AmoledV1::HomeRestFlow;
constexpr RoomResource::Point ROOM[] = {{0, 0}, {200, 0}, {200, 160}, {0, 160}};
struct Fixture {
    Home::Actor main, companion;
    Home::Simulation simulation;
    AmoledV1::HomeSleepFlow sleep{main, companion, simulation};
    Flow rest{main, companion, simulation, sleep};
    AmoledV1::HomeSleepFlow::Layout layout{};
    Game::GameState game{};
    Fixture() {
        main.reset(Home::ActorRole::LEADER, 0, 100);
        companion.reset(Home::ActorRole::TEAMMATE, 1, 100);
        main.active = companion.active = true;
        main.geometry.footprint = companion.geometry.footprint = {5, 3};
        main.x = 20;
        main.y = 40;
        companion.x = 40;
        companion.y = 80;
        simulation.attach(main, &companion);
        simulation.reset(100);
        layout.polygon = ROOM;
        layout.polygonCount = 4;
        layout.minX = 0;
        layout.maxX = 200;
        layout.minY = 0;
        layout.maxY = 160;
        layout.bedX = 80;
        layout.bedY = 60;
        layout.hasAnchor = true;
        layout.preferredX = 140;
        layout.preferredY = 100;
        game.teamCount = 2;
        game.team[0].hpCur = game.team[1].hpCur = 10;
    }
};
void placementAndRepeatRest() {
    Fixture f;
    assert(f.rest.place(f.game, f.layout, 100) == 0);
    f.game.team[0].hpCur = 0;
    f.game.team[1].fainted = true;
    f.main.velocityX = 3;
    f.companion.velocityY = -2;
    assert(f.rest.place(f.game, f.layout, 110) == 3);
    assert(f.main.x == 80 && f.main.y == 60 && f.main.targetX == 80);
    assert(f.companion.x == 140 && f.companion.y == 100);
    assert(f.main.task == Home::Task::FAINTED && f.companion.task == Home::Task::FAINTED);
    assert(f.main.faintRestActive && f.companion.faintRestActive);
    assert(f.main.route.empty() && f.companion.route.empty());
    assert(f.main.velocityX == 0 && f.companion.velocityY == 0);
    assert(!f.rest.restMain(80, 60, 120));
    assert(!f.rest.restCompanion(f.layout, 120));
    f.main.x = 80.02f;
    assert(f.rest.restMain(80, 60, 130));
    assert(f.main.x == 80);
    // Ordinary rest does not continuously teleport the companion, even if geometry changes.
    f.layout.preferredX = 170;
    assert(!f.rest.restCompanion(f.layout, 140));
    assert(f.companion.x == 140);
    assert(f.rest.place(f.game, f.layout, 150) == 3);
    assert(f.companion.x == 170);
}
void failedPlacementAndInactiveCompanion() {
    Fixture f;
    f.layout.hasBedBounds = true;
    f.layout.bedMinX = f.layout.bedMinY = 0;
    f.layout.bedMaxX = 200;
    f.layout.bedMaxY = 160;
    f.game.team[1].hpCur = 0;
    assert(f.rest.place(f.game, f.layout, 100) == 2);
    assert(f.companion.x == 40 && f.companion.y == 80);
    assert(f.companion.targetX == 40 && f.companion.faintRestActive);
    f.companion.active = false;
    assert(f.rest.place(f.game, f.layout, 110) == 0);
    f.game.teamCount = 1;
    f.companion.active = true;
    assert(f.rest.place(f.game, f.layout, 120) == 0);
}
void recoveryAndWrap() {
    Fixture f;
    assert(!f.rest.recoverMain(100));
    assert(!f.rest.recoverCompanion(100));
    assert(f.rest.restMain(80, 60, 110));
    assert(f.rest.restCompanion(f.layout, 110));
    f.main.mind.reset(110);
    f.main.mind.onRested(110);
    constexpr uint32_t now = 0xffffff00U;
    assert(f.rest.recoverMain(now));
    assert(!f.main.faintRestActive && f.main.task == Home::Task::IDLE);
    assert(f.main.nextDecisionMs == now && f.main.motion == Home::MotionPhase::STILL);
    assert(f.main.targetX == f.main.x && f.main.targetY == f.main.y);
    assert(!f.rest.recoverMain(now + 1));
    assert(f.rest.recoverCompanion(now));
    assert(!f.companion.faintRestActive && f.companion.task == Home::Task::IDLE);
    assert(f.companion.nextDecisionMs == 444);
    assert(!f.rest.recoverCompanion(now + 1));
    // A sleep task without faintRestActive belongs to ordinary sleep flow.
    f.simulation.transition(0, Home::Task::SLEEPING, 20, 0, true);
    assert(!f.rest.recoverMain(20));
    assert(f.main.task == Home::Task::SLEEPING);
}
} // namespace
int main() {
    GameRandom::setRangeProvider(+[](uint32_t, uint32_t) -> uint32_t {
        assert(false && "rest must not consume RNG");
        return 0;
    });
    placementAndRepeatRest();
    failedPlacementAndInactiveCompanion();
    recoveryAndWrap();
    GameRandom::setRangeProvider(nullptr);
    std::puts(
        "rest flow: placement, repeat entry, failed geometry, recovery, idempotence and wrap pass");
}
