#include "app/HomePresenter.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

#include "game/GameRandom.h"
#include "platform/desktop/DesktopPlatform.h"

using Presenter = AmoledV1::HomePresenter;

namespace {
void closeTo(float actual, float expected) {
    assert(std::fabs(actual - expected) < 0.00001f);
}

void cameraKeepsTheSafeAreaAndRoundsBeforeClamping() {
    Presenter presenter;
    const Presenter::RoomLayout room{true, 400, 240, 20};
    struct Step { float x, y, cameraX, cameraY; };
    const Step steps[] = {
        {100, 100, 0, 20},
        {122, 146, 0, 20},
        {122.25f, 146.25f, 0, 20},
        {122.5f, 146.5f, 1, 21},
        {200.5f, 180.5f, 79, 55},
        {140.5f, 112.5f, 79, 55},
        {140, 112, 78, 54},
        {-100, -100, 0, 20},
        {500, 500, 216, 112},
    };
    for (const auto& step : steps) {
        presenter.updateCamera(room, step.x, step.y);
        assert(presenter.cameraX() == step.cameraX);
        assert(presenter.cameraY() == step.cameraY);
    }
    assert(presenter.worldToScreenX(room, 215.75f) == -1);
    assert(presenter.worldToScreenY(room, 111.75f) == 47);
    assert(presenter.worldToScreenX(room, 216.25f) == 1);
    assert(presenter.worldToScreenY(room, 112.25f) == 49);

    const Presenter::RoomLayout small{true, 100, 100, 17};
    presenter.updateCamera(small, 500, 500);
    assert(presenter.cameraX() == 0 && presenter.cameraY() == 17);
    assert(presenter.worldToScreenX(small, 100) == 200);
    assert(presenter.worldToScreenY(small, 100) == 214);
}

void unavailableRoomProjectsWithoutTheHeaderAndResetsCamera() {
    Presenter presenter;
    const Presenter::RoomLayout room{true, 400, 240, 20};
    const Presenter::RoomLayout unavailable{};
    presenter.updateCamera(room, 500, 500);
    // Projection follows the supplied layout immediately, even before update.
    assert(presenter.worldToScreenX(unavailable, 12.25f) == 25);
    assert(presenter.worldToScreenY(unavailable, -4.25f) == -9);
    presenter.updateCamera(unavailable, 500, 500);
    assert(presenter.cameraX() == 0 && presenter.cameraY() == 0);
    presenter.updateCamera(room, 100, 100);
    assert(presenter.worldToScreenX(room, 100) == 200);
    assert(presenter.worldToScreenY(room, 100) == 208);

    Presenter other;
    other.updateCamera(room, 500, 500);
    assert(presenter.cameraX() == 0 && presenter.cameraY() == 20);
    assert(other.cameraX() == 216 && other.cameraY() == 112);
}

void sleepSelectsTheTallestThenWidestVisibleFrame() {
    Presenter presenter;
    struct Case { uint16_t species; uint8_t expanded; };
    // The resource fixtures have equal canvas sizes and these visible bounds:
    // 1: (10, 5), (4, 6); 4: (4, 6), (8, 6);
    // 7: (4, 6), (4, 6); 25: (4, 7), (10, 6).
    const Case cases[] = {{1, 1}, {4, 1}, {7, 0}, {25, 0}};
    for (const auto& fixture : cases) {
        presenter.startSleep(fixture.species, 1000, 800);
        assert(presenter.sleepFrame(1000) == 0);
        assert(presenter.sleepFrame(1799) == 0);
        assert(presenter.sleepFrame(1800) == 1);
        assert(presenter.sleepFrame(2599) == 1);
        assert(presenter.sleepFrame(2600) == 0);
        closeTo(presenter.sleepExpansion(1400), fixture.expanded == 0 ? 1 : 0);
        closeTo(presenter.sleepExpansion(2200), fixture.expanded == 1 ? 1 : 0);
        closeTo(presenter.sleepExpansion(1800), 0.5f);
    }
}

void sleepWrapRestartAndIndependentInstancesKeepTheirOwnPhase() {
    Presenter presenter, other;
    const uint32_t start = std::numeric_limits<uint32_t>::max() - 399;
    presenter.startSleep(1, start, 800);
    assert(presenter.sleepFrame(399) == 0);
    assert(presenter.sleepFrame(400) == 1);
    closeTo(presenter.sleepExpansion(800), 1);
    other.startSleep(7, 2000, 700);
    closeTo(other.sleepExpansion(2350), 1);
    closeTo(presenter.sleepExpansion(800), 1);

    presenter.startSleep(1, 5000, 0);
    assert(presenter.sleepFrame(5000) == 0);
    assert(presenter.sleepFrame(5001) == 1);
    assert(presenter.sleepFrame(5002) == 0);
    // Missing species retains the profile defaults and first-frame expansion.
    presenter.startSleep(0, 6000, 800);
    assert(presenter.sleepFrame(6800) == 1);
    closeTo(presenter.sleepExpansion(6400), 1);
    closeTo(presenter.sleepExpansion(7200), 0);
    assert(other.sleepFrame(2700) == 1);
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    GameRandom::seed(1234);
    cameraKeepsTheSafeAreaAndRoundsBeforeClamping();
    unavailableRoomProjectsWithoutTheHeaderAndResetsCamera();
    sleepSelectsTheTallestThenWidestVisibleFrame();
    sleepWrapRestartAndIndependentInstancesKeepTheirOwnPhase();
    assert(GameRandom::next() == 332584831u);
    std::puts("home presenter camera, resource bounds, sleep phase, wrap and RNG passed");
}
