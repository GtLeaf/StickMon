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

void manualPanClampsHoldsAndEasesBack() {
    // The production room: 240 logical px wide, 184 visible on AMOLED.
    const Presenter::RoomLayout room{true, 240, 161, 0};
    Presenter presenter;
    presenter.updateCamera(room, 70, 100);
    assert(presenter.cameraX() == 0 && presenter.canPan(room));
    assert(!presenter.panBy(room, 30) && "a pan needs an active drag");
    presenter.beginPan();
    assert(presenter.panBy(room, 30) && presenter.cameraX() == 30);
    assert(presenter.panBy(room, 100) && presenter.cameraX() == 56);
    closeTo(presenter.panOffset(), 56);  // no invisible overshoot
    assert(presenter.panBy(room, -6.5f) && presenter.cameraX() == 50);
    assert(!presenter.panBy(room, 0.25f) && presenter.cameraX() == 50);
    // Held finger never returns, even for story events.
    assert(!presenter.updatePan(room, 900000, 70, true) && presenter.cameraX() == 50);
    presenter.endPan(1000, 70);
    // Follow updates keep the manual offset on top of the follow camera.
    presenter.updateCamera(room, 70, 100);
    assert(presenter.cameraX() == 50);
    assert(!presenter.updatePan(room, 1000 + Presenter::PAN_IDLE_RETURN_MS - 1, 70, false));
    assert(presenter.cameraX() == 50 && !presenter.panReturning());
    const uint32_t start = 1000 + Presenter::PAN_IDLE_RETURN_MS;
    assert(!presenter.updatePan(room, start, 70, false) && presenter.panReturning());
    assert(presenter.updatePan(room, start + Presenter::PAN_RETURN_MS / 2, 70, false));
    assert(presenter.cameraX() > 0 && presenter.cameraX() < 50);
    assert(presenter.updatePan(room, start + Presenter::PAN_RETURN_MS, 70, false));
    assert(presenter.cameraX() == 0 && presenter.panOffset() == 0 && !presenter.panReturning());
    assert(!presenter.updatePan(room, start + 99999, 70, true));

    // A resting actor may be looked away from; walking off-view returns.
    presenter.beginPan();
    presenter.panBy(room, 56);
    presenter.endPan(20000, 62);
    presenter.updateCamera(room, 62, 100);  // screen x 6: deliberately looked away
    assert(!presenter.updatePan(room, 21000, 62, false) && presenter.cameraX() == 56);
    presenter.updateCamera(room, 73, 100);  // moved, but comfortably visible
    assert(!presenter.updatePan(room, 21100, 73, false) && !presenter.panReturning());
    presenter.updateCamera(room, 66, 100);  // walked back toward the left edge
    presenter.updatePan(room, 21200, 66, false);
    assert(presenter.panReturning());
    assert(presenter.updatePan(room, 21200 + Presenter::PAN_RETURN_MS, 66, false));
    assert(presenter.cameraX() == 0);

    // Story events force the return; leaving the room snaps immediately.
    presenter.beginPan();
    presenter.panBy(room, 40);
    presenter.endPan(30000, 70);
    presenter.updatePan(room, 30010, 70, true);
    assert(presenter.panReturning());
    presenter.beginPan();  // a new drag interrupts the return
    assert(!presenter.panReturning());
    presenter.panBy(room, 10);
    presenter.endPan(30020, 70);
    assert(presenter.cameraX() > 0 && presenter.clearPan(room));
    assert(presenter.cameraX() == 0 && !presenter.clearPan(room));

    // Follow at the right edge: only looking left is possible.
    presenter.updateCamera(room, 230, 100);
    assert(presenter.cameraX() == 56);
    presenter.beginPan();
    assert(!presenter.panBy(room, 10) && presenter.cameraX() == 56);
    assert(presenter.panBy(room, -30) && presenter.cameraX() == 26);
    presenter.endPan(40000, 230);
    presenter.clearPan(room);

    // A room that fits the screen and a missing room cannot pan.
    const Presenter::RoomLayout narrow{true, 100, 100, 17};
    Presenter fixed;
    fixed.updateCamera(narrow, 50, 50);
    assert(!fixed.canPan(narrow));
    fixed.beginPan();
    assert(!fixed.panBy(narrow, 20) && fixed.cameraX() == 0);
    assert(!fixed.canPan(Presenter::RoomLayout{}));
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
    manualPanClampsHoldsAndEasesBack();
    sleepSelectsTheTallestThenWidestVisibleFrame();
    sleepWrapRestartAndIndependentInstancesKeepTheirOwnPhase();
    assert(GameRandom::next() == 332584831u);
    std::puts("home presenter camera, resource bounds, sleep phase, wrap and RNG passed");
}
