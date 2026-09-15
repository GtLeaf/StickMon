#include "AmoledApp.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"

#include <cassert>
#include <cstdio>

namespace AmoledV1 {

// Exercise real application transitions; only initial state and diagnostics need access.
struct AmoledAppTestAccess {
    using Phase = AmoledApp::ExpeditionDeparturePhase;
    using Scene = AppSceneFlow::Scene;

    static void tap(AmoledApp& app, DesktopPlatform& desktop, int x, int y) {
        TouchEvent event;
        event.x = x;
        event.y = y;
        event.timestampMs = desktop.millis();
        event.type = TouchEventType::DOWN;
        app.handleTouch(event);
        event.type = TouchEventType::UP;
        app.handleTouch(event);
    }

    static void present(AmoledApp& app, Canvas565& canvas, bool acknowledge = true) {
        assert(app.needsRender());
        app.render(canvas);
        if (app.expeditionFade.alpha() == 255) {
            assert(app.renderRowBegin() == 0 && app.renderRowEnd() == AmoledUi::HEIGHT);
            for (int y = 0; y < AmoledUi::HEIGHT; ++y) {
                for (int x = 0; x < AmoledUi::WIDTH; ++x) {
                    assert(canvas.readPixel(x, y) == 0);
                }
            }
        }
        if (acknowledge) app.markRendered();
    }

    static void tick(AmoledApp& app, DesktopPlatform& desktop, uint32_t delay = 50) {
        desktop.advanceMs(delay);
        app.update(desktop.millis());
        assert(app.sceneFlow.current() != Scene::EXPLORE_AREAS);
    }

    static void fadeOut(AmoledApp& app, DesktopPlatform& desktop, Canvas565& canvas,
                        Scene source) {
        assert(app.expeditionFade.alpha() == 0);
        tick(app, desktop, 1200);  // No first frame submitted yet, despite elapsed time.
        assert(app.expeditionFade.alpha() == 0);
        present(app, canvas);
        int shades = 0;
        while (app.expeditionFade.alpha() != 255) {
            const int before = app.expeditionFade.alpha();
            tick(app, desktop, 500);  // Simulate a slow display/resource frame.
            assert(app.sceneFlow.current() == source);
            assert(app.expeditionFade.alpha() > before);
            assert(app.expeditionFade.alpha() - before <= 43);
            ++shades;
            if (app.expeditionFade.alpha() != 255) present(app, canvas);
            assert(shades < 20);
        }
        assert(shades >= 6);
        present(app, canvas, false);  // A failed transfer must not allow a scene switch.
        tick(app, desktop, 1000);
        assert(app.sceneFlow.current() == source);
        assert(!app.expeditionFade.complete());
        // A partial submission is not evidence that the entire screen is black.
        app.forceRenderRows(0, 100);
        app.render(canvas);
        app.markRendered();
        tick(app, desktop);
        assert(app.sceneFlow.current() == source);
        present(app, canvas);
        assert(app.expeditionFade.complete());
        tick(app, desktop);
        assert(app.sceneFlow.current() != source);
        assert(app.expeditionFade.alpha() == 255);
    }

    static void fadeIn(AmoledApp& app, DesktopPlatform& desktop, Canvas565& canvas,
                       Scene destination) {
        const float petX = app.petX;
        const float petY = app.petY;
        tick(app, desktop, 2000);  // Destination loading cannot consume its fade-in.
        assert(app.expeditionFade.alpha() == 255);
        present(app, canvas);
        int shades = 0;
        while (app.expeditionFade.active()) {
            const int before = app.expeditionFade.alpha();
            tick(app, desktop);
            assert(app.sceneFlow.current() == destination);
            assert(app.expeditionFade.alpha() <= before);
            assert(before - app.expeditionFade.alpha() <= 43);
            if (app.expeditionFade.active()) {
                assert(app.petX == petX && app.petY == petY);
                ++shades;
            }
            present(app, canvas);
            assert(shades < 20);
        }
        assert(shades >= 6);
    }

    static void checkImmediateNextFrame(DesktopPlatform& desktop, Canvas565& canvas,
                                        const Game::GameState& state) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = state;
        app.gameState.settings.idleTimeoutIndex = 4;
        app.sceneFlow.enter(Scene::EXPLORE_AREAS);
        tap(app, desktop, 98, 412);
        int frames = 0;
        while (app.expeditionDeparturePhase != Phase::FADE_OUT) {
            tick(app, desktop);
            present(app, canvas);
            assert(++frames < 1000);
        }
        const uint32_t fadeStarted = desktop.millis();
        frames = 0;
        do {
            // Real boards may begin the next update in the same millisecond
            // as transfer completion. Rendering/transmission consumes the time;
            // there is no extra delay between markRendered and update.
            app.update(desktop.millis());
            assert(app.sceneFlow.current() != Scene::EXPLORE_AREAS);
            present(app, canvas, false);
            desktop.advanceMs(35);
            app.markRendered();
            assert(++frames < 100);
        } while (app.expeditionDeparturePhase != Phase::NONE);
        assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
        assert(desktop.millis() - fadeStarted < 2000);
        std::puts("immediate next frame: departure and both fade endpoints complete");
    }

    static void run(DesktopPlatform& desktop, Canvas565& canvas,
                    const Game::GameState& state) {
        checkImmediateNextFrame(desktop, canvas, state);
        for (int returnMode = 0; returnMode < 3; ++returnMode) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.settings.idleTimeoutIndex = 4;
            app.sceneFlow.enter(Scene::EXPLORE_AREAS);
            app.forceFullRender();
            present(app, canvas);
            tap(app, desktop, 98, 412);  // Actual departure button and touch handler.
            assert(app.sceneFlow.current() == Scene::HOME);
            present(app, canvas);
            int walkingFrames = 0;
            while (app.expeditionDeparturePhase != Phase::FADE_OUT) {
                tick(app, desktop);
                assert(app.sceneFlow.current() == Scene::HOME);
                if (app.expeditionDeparturePhase != Phase::FADE_OUT) present(app, canvas);
                assert(++walkingFrames < 1000);
            }
            fadeOut(app, desktop, canvas, Scene::HOME);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
            fadeIn(app, desktop, canvas, Scene::EXPLORE_ROUTE);

            app.exploreRouteMoving = false;
            app.exploreRouteBossPending = false;
            app.exploreRouteMapBlockCount = 1;
            app.exploreRouteMapBlock = 0;
            app.exploreRouteSteps = 10;
            if (returnMode == 1) {
                app.sceneFlow.openExploreMenu();
                int endY = -1;
                for (int y = 0; y < 448; ++y) {
                    if (exploreRouteMenuItemAt(340, y) == 2) { endY = y; break; }
                }
                assert(endY >= 0);
                tap(app, desktop, 340, endY);
            } else {
                app.autonomousExpedition = returnMode == 2;
                app.finishExploreRouteAtEnd(desktop.millis());
                if (returnMode == 0) tap(app, desktop, 184, 200);
            }
            assert(app.expeditionDeparturePhase == Phase::RETURN_FADE_OUT);
            assert(app.sceneFlow.current() == Scene::EXPLORE_ROUTE);
            tap(app, desktop, 98, 412);  // Repeated taps cannot cancel a return.
            app.leaveExploreRoute();   // Repeated return requests cannot settle twice.
            assert(app.expeditionDeparturePhase == Phase::RETURN_FADE_OUT);
            fadeOut(app, desktop, canvas, Scene::EXPLORE_ROUTE);
            assert(app.sceneFlow.current() == Scene::HOME);
            const auto settledBond = app.gameState.team[0].bond;
            fadeIn(app, desktop, canvas, Scene::HOME);
            assert(app.expeditionDeparturePhase == Phase::RETURN_WALK_IN);
            int enteringFrames = 0;
            while (app.expeditionDeparturePhase != Phase::NONE) {
                tick(app, desktop);
                assert(app.sceneFlow.current() == Scene::HOME);
                assert(app.expeditionFade.alpha() == 0);
                present(app, canvas);
                assert(++enteringFrames < 1000);
            }
            assert(app.gameState.team[0].bond == settledBond);
            assert(!app.exploreRouteMoving && !app.exploreRouteAutoWalk);
            assert(app.petX == app.petTargetX && app.petY == app.petTargetY);
            assert(app.homeMainActor.task == Home::Task::IDLE);
            if (app.gameState.teamCount > 1 &&
                app.homeCompanionActor.active &&
                !app.gameState.team[1].fainted &&
                app.gameState.team[1].hpCur > 0) {
                assert(app.homeCompanionActor.task == Home::Task::IDLE);
                const float dx = app.homeCompanionActor.x - app.petX;
                const float dy = app.homeCompanionActor.y - app.petY;
                assert(dx * dx + dy * dy > 16.0f);
                assert(!app.expeditionMainHidden);
                assert(!app.expeditionCompanionHidden);
            }
            std::printf("expedition return mode %d: black endpoints and direct home return verified\n",
                        returnMode);
        }
    }
};

}  // namespace AmoledV1

void checkExpeditionTransitions(DesktopPlatform& desktop, Canvas565& canvas,
                               const Game::GameState& state) {
    AmoledV1::AmoledAppTestAccess::run(desktop, canvas, state);
}
