#include "AmoledApp.h"
#include "game/MonsterFactory.h"
#include "game/CareExperience.h"
#include "game/HatchService.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include <cassert>
#include <cstdio>
#include <vector>

namespace AmoledV1 {
struct AmoledPowerTestAccess {
    static void run(DesktopPlatform& desktop, Canvas565& canvas) {
        desktop.advanceMs(1000);
        AmoledApp app;
        app.begin(desktop.millis());
        app.gameState = {};
        app.gameState.oobeDone = true;
        app.gameState.teamCount = 1;
        auto& monster = app.gameState.team[0];
        monster = Game::MonsterFactory::create(8, 20);
        monster.satiety = 10;
        app.sceneFlow.goHome();
        app.syncHomeActors(desktop.millis());
        const auto brightness = desktop.brightness();
        assert(app.prepareDisplayLock(desktop.millis()));
        assert(desktop.brightness() == brightness);
        app.setDisplayLocked(true);
        assert(app.lockedHomePowerAllowed());
        // A committed serving is still eaten and rewards the same fixed XP
        // under 100ms ticks with occasional scheduler jitter.
        const uint32_t before = monster.exp;
        app.gameState.room.bowlFood = 0;
        app.gameState.room.bowlCount = 1;
        app.gameState.room.bowlBitesRemaining = 3;
        app.homeRuntime.releaseAll(0);
        assert(app.homeRuntime.acquire(Home::Resource::BOWL, 0,
            Home::Task::FEEDING, desktop.millis()));
        app.homeRuntime.transition(0, Home::Task::FEEDING, desktop.millis(), 10000, true);
        app.nextFeedBiteMs = desktop.millis();
        for (int i = 0; i < 60; ++i) {
            desktop.advanceMs(i % 3 == 0 ? 110 : 100);
            app.update(desktop.millis());
        }
        assert(app.gameState.room.bowlCount == 0);
        assert(monster.exp == before + 39);
        assert(monster.satiety == 85);

        // One game hour across midnight: clock, recovery and daily budgets
        // continue while presentation is throttled. No wall-clock sleep.
        app.gameState.gameMinutesTotal = 1439;
        app.gameClock.set(desktop.millis(), 1439);
        app.lastCareMs = desktop.millis();
        app.gameState.settings.speedIndex = 0;
        monster.petCountToday = 4;
        monster.hpCur = 1;
        const uint8_t initialSatiety = monster.satiety;
        for (int i = 0; i < 36000; ++i) {
            desktop.advanceMs(100);
            app.update(desktop.millis());
            app.markRendered();
        }
        assert(app.gameState.gameMinutesTotal == 1499);
        assert(app.gameState.careRewardDay == 1);
        assert(monster.petCountToday == 0);
        assert(monster.hpCur > 1);
        assert(monster.satiety < initialSatiety);
        assert(monster.exp == before + 39);
        assert(app.saveState());
        AmoledApp rebooted;
        rebooted.begin(desktop.millis());
        assert(rebooted.gameState.gameMinutesTotal == 1499);
        assert(rebooted.gameState.team[0].exp == monster.exp);

        // Native rectangular rendering matches a complete room, leaving the
        // masked region untouched. Brightness is never touched by locking.
        app.forceFullRender();
        app.render(canvas);
        app.markRendered();
        const std::vector<uint16_t> full(canvas.rawPixels(), canvas.rawPixels() + canvas.pixelCount());
        constexpr uint16_t SENTINEL = 0x1234;
        canvas.fillSprite(SENTINEL);
        app.forceRenderRegion(100, 140, 240, 300);
        assert(app.renderColumnBegin() == 100 && app.renderColumnEnd() == 240);
        assert(app.renderRowBegin() == 140 && app.renderRowEnd() == 300);
        app.render(canvas);
        for (unsigned i = 0; i < canvas.pixelCount(); ++i) {
            const int x = i % AmoledUi::WIDTH, y = i / AmoledUi::WIDTH;
            const bool inside = x >= 100 && x < 240 && y >= 140 && y < 300;
            assert(canvas.rawPixels()[i] == (inside ? full[i] : canvas.encodeColor(SENTINEL)));
        }
        app.setDisplayLocked(false);
        app.onWake(desktop.millis());
        assert(app.needsRender() && app.renderRowEnd() == AmoledUi::HEIGHT);
        assert(desktop.brightness() == brightness);
        // Real sprite orders differ: Wartortle expands on frame 1,
        // Snorlax on frame 0. Both stay synchronized across lock and wake.
        for (uint16_t species : {8, 143}) {
            monster = Game::MonsterFactory::create(species, 20);
            app.syncHomeActors(desktop.millis());
            app.homeMainActor.faintRestActive = false;
            app.homeRuntime.transition(
                0, Home::Task::SLEEPING, desktop.millis(), 0, true);
            app.petStopping = false;
            const uint32_t start = desktop.millis();
            app.startSleepAnimation(start, 800);
            const uint8_t expanded = species == 8 ? 1 : 0;
            assert(app.petSleepExpansion(start + expanded * 800 + 400) > 0.999f);
            int16_t x = 0, y = 0;
            assert(app.lockFocusPoint(x, y));
            for (uint32_t offset : {400U, 799U, 800U, 1200U, 5001U}) {
                desktop.advanceMs(offset + 1);
                const uint32_t now = desktop.millis();
                app.updateSleepAnimation(now);
                assert(app.petSleepFrame() == (now - start) / 800 % 2);
                int16_t nextX = 0, nextY = 0;
                assert(app.lockFocusPoint(nextX, nextY));
                assert(x == nextX && y == nextY);
                const float phase = app.petSleepExpansion(now);
                assert(app.prepareDisplayLock(now));
                app.setDisplayLocked(true);
                app.setDisplayLocked(false);
                app.onWake(now);
                assert(app.petSleepExpansion(now) == phase);
            }
            assert(app.petIsSleeping());
            app.homeRuntime.stop(0, desktop.millis());
            assert(!app.petIsSleeping()); // Night alone is insufficient.
            monster.fainted = 1;
            monster.hpCur = 0;
            app.placeFaintedHomeActors(desktop.millis());
            assert(app.petIsSleeping());
            app.updateSleepAnimation(desktop.millis() + 700);
            assert(app.petFrame == 1);
        }
        // Panel-off cadence still finishes meals, heals, crosses midnight and
        // saves real state. Rendering and touch must leave the hidden frame intact.
        monster = Game::MonsterFactory::create(8, 20);
        monster.satiety = 10;
        monster.hpCur = 1;
        app.sceneFlow.goHome();
        app.homeMainActor.faintRestActive = false;
        app.syncHomeActors(desktop.millis());
        app.gameState.gameMinutesTotal = 1439;
        app.gameClock.set(desktop.millis(), 1439);
        app.lastCareMs = desktop.millis();
        app.lastPetUpdateMs = desktop.millis();
        app.gameState.settings.speedIndex = 0;
        app.gameState.room.bowlFood = 0;
        app.gameState.room.bowlCount = 1;
        app.gameState.room.bowlBitesRemaining = 3;
        app.homeRuntime.releaseAll(0);
        assert(app.homeRuntime.acquire(Home::Resource::BOWL, 0,
            Home::Task::FEEDING, desktop.millis()));
        app.homeRuntime.transition(0, Home::Task::FEEDING, desktop.millis(), 10000, true);
        app.nextFeedBiteMs = desktop.millis();
        const uint32_t standbyXp = monster.exp;
        const auto standbyScene = app.sceneFlow.current();
        assert(app.prepareDisplayStandby(desktop.millis()));
        assert(app.displayStandby());
        canvas.fillSprite(SENTINEL);
        app.render(canvas);
        for (unsigned i = 0; i < canvas.pixelCount(); ++i)
            assert(canvas.rawPixels()[i] == canvas.encodeColor(SENTINEL));
        app.handleTouch({TouchEventType::DOWN, 200, 200, 0, 0, desktop.millis()});
        app.handleTouch({TouchEventType::UP, 200, 200, 0, 0, desktop.millis()});
        assert(!app.pointerDown && app.sceneFlow.current() == standbyScene);
        for (int i = 0; i < 3600; ++i) {
            desktop.advanceMs(1001);
            app.update(desktop.millis());
        }
        assert(app.displayStandby());
        assert(app.gameState.room.bowlCount == 0);
        assert(monster.exp == standbyXp + 39 && monster.hpCur > 1);
        assert(app.gameState.gameMinutesTotal >= 1499);
        assert(app.gameState.careRewardDay == 1);
        assert(app.saveState());
        app.setDisplayStandby(false, desktop.millis());
        assert(!app.displayStandby() && app.needsRender());
        assert(app.renderRowBegin() == 0 && app.renderRowEnd() == AmoledUi::HEIGHT);
        assert(desktop.brightness() == brightness);
        app.render(canvas);
        assert(canvas.rawPixels()[0] != canvas.encodeColor(SENTINEL));
        AmoledApp standbyReload;
        standbyReload.begin(desktop.millis());
        assert(standbyReload.gameState.team[0].exp == monster.exp);
        assert(standbyReload.gameState.gameMinutesTotal == app.gameState.gameMinutesTotal);

        // Sleeping from a menu preserves its page and consumes no hidden input.
        app.sceneFlow.openMenu();
        const auto menuScene = app.sceneFlow.current();
        assert(app.prepareDisplayStandby(desktop.millis()));
        desktop.advanceMs(60000);
        app.update(desktop.millis());
        assert(app.sceneFlow.current() == menuScene);
        app.setDisplayStandby(false, desktop.millis());
        assert(app.sceneFlow.current() == menuScene);

        // Eggs keep their countdown without accepting taps or shake input.
        Game::HatchService::initializeNewGame(app.gameState);
        app.gameState.hatchSeconds = 100;
        app.gameState.settings.speedIndex = 0;
        app.sceneFlow = AppSceneFlow::Controller(AppSceneFlow::Scene::HATCH);
        app.hatchController_.reset(desktop.millis());
        app.hatchController_.restoreProgress({}, 100, desktop.millis());
        assert(app.prepareDisplayStandby(desktop.millis()));
        for (int i = 0; i < 5; ++i) {
            desktop.advanceMs(1000);
            app.update(desktop.millis());
        }
        assert(app.displayStandby() && app.hatchController_.state().elapsedSeconds >= 5);
        assert(!app.hatchController_.state().imuEnabled);
        assert(app.hatchController_.state().pokeCount == 0 &&
               app.hatchController_.state().wipeCount == 0);
        for (int i = 0; i < 100 && !app.gameState.oobeDone; ++i) {
            desktop.advanceMs(1000);
            app.update(desktop.millis());
        }
        assert(app.gameState.oobeDone && app.gameState.teamCount == 1);
        assert(app.displayStandby()); // Hatch completion rebuilds the app in-place.
        app.setDisplayStandby(false, desktop.millis());
        std::puts("locked room: actual feeding XP, 1h clock/recovery, midnight, save and wake redraw verified");
    }
};
} // namespace AmoledV1

void checkLockedRoomPower(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledPowerTestAccess::run(desktop, canvas);
}
