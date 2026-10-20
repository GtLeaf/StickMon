#include "AmoledApp.h"
#include "game/MonsterFactory.h"
#include "game/GameRandom.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "ui/RenderCaches.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

namespace {
class ShowerBlobSpy final : public Platform::IBlobStore {
public:
    explicit ShowerBlobSpy(Platform::IBlobStore& backing) : backing_(backing) {}
    bool initialize() override { return backing_.initialize(); }
    size_t blobSize(const char* ns, const char* key) override { return backing_.blobSize(ns, key); }
    bool readBlob(const char* ns, const char* key, void* out, size_t size) override {
        return backing_.readBlob(ns, key, out, size);
    }
    bool writeBlob(const char* ns, const char* key, const void* data, size_t size) override {
        ++writes;
        return !fail && backing_.writeBlob(ns, key, data, size);
    }
    bool removeBlob(const char* ns, const char* key) override { return backing_.removeBlob(ns, key); }
    bool clearNamespace(const char* ns) override { return backing_.clearNamespace(ns); }
    bool fail = false;
    size_t writes = 0;
private:
    Platform::IBlobStore& backing_;
};
}

namespace AmoledV1 {
struct ShowerControllerTestAccess {
    static ShowerController::State& fixture(ShowerController& controller) {
        return controller.state_;
    }
};
struct AmoledShowerTestAccess {
    static void send(AmoledApp& app, DesktopPlatform& desktop, TouchEventType type, int x, int y) {
        TouchEvent event{};
        event.type = type; event.x = x; event.y = y; event.timestampMs = desktop.millis();
        app.handleTouch(event);
    }

    static void present(AmoledApp& app, Canvas565& canvas) {
        if (!app.needsRender()) return;
        const auto before = std::vector<uint16_t>(canvas.rawPixels(), canvas.rawPixels() + 368 * 448);
        const int left = app.renderColumnBegin(), right = app.renderColumnEnd();
        const int top = app.renderRowBegin(), bottom = app.renderRowEnd();
        app.render(canvas);
        const auto partial = std::vector<uint16_t>(canvas.rawPixels(), canvas.rawPixels() + 368 * 448);
        for (int y = 0; y < 448; ++y) {
            for (int x = 0; x < 368; ++x) {
                if (x >= left && x < right && y >= top && y < bottom) continue;
                assert(partial[y * 368 + x] == before[y * 368 + x]);
            }
        }
        app.forceFullRender();
        app.render(canvas);
        if (!std::equal(partial.begin(), partial.end(), canvas.rawPixels())) {
            for (int i = 0; i < 368 * 448; ++i) {
                if (partial[i] != canvas.rawPixels()[i]) {
                    std::fprintf(stderr, "shower damage mismatch (%d,%d), rect=[%d,%d,%d,%d]\n",
                                 i % 368, i / 368, left, top, right, bottom);
                    break;
                }
            }
            assert(false);
        }
        app.markRendered();
    }

    static void checkRoomReturnPath(DesktopPlatform& desktop, const Game::GameState& state) {
        using Scene = AppSceneFlow::Scene;
        for (int foodVisits : {0, 1, 3}) {
          for (int bath : {0, 1, 2}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.oobeDone = true;
            app.gameState.bag.soap[0] = 1;
            app.sceneFlow.goHome();
            app.sceneFlow.openMenu();
            app.openRoomScene();
            const auto tap = [&](int x, int y) {
                desktop.advanceMs(100);
                send(app, desktop, TouchEventType::DOWN, x, y);
                desktop.advanceMs(40);
                send(app, desktop, TouchEventType::UP, x, y);
            };
            for (int visit = 0; visit < foodVisits; ++visit) {
                tap(180, 120);
                assert(app.sceneFlow.current() == Scene::ROOM_FOOD);
                tap(180, 100);
                tap(40, 36);
                assert(app.sceneFlow.current() == Scene::ROOM);
                assert(app.sceneFlow.subSceneReturn() == Scene::MAIN_MENU);
            }
            if (bath) {
                tap(180, 240);
                assert(app.sceneFlow.current() == Scene::SHOWER);
                if (bath == 2) app.startShowerSoap(0, desktop.millis());
                tap(40, 36);
                if (bath == 2) {
                    assert(ShowerControllerTestAccess::fixture(app.showerController_).showerMode == ShowerMode::EXIT_CONFIRM);
                    tap(250, 380); // Stay, then request exit again.
                    assert(app.sceneFlow.current() == Scene::SHOWER);
                    assert(ShowerControllerTestAccess::fixture(app.showerController_).showerMode == ShowerMode::MENU);
                    tap(40, 36);
                    tap(100, 380);
                }
                assert(app.sceneFlow.current() == Scene::ROOM);
            }
            tap(40, 36);
            assert(app.sceneFlow.current() == Scene::MAIN_MENU);
            app.sceneFlow.closeMenu();
            assert(app.sceneFlow.current() == Scene::HOME);
          }
        }
        std::puts("room navigation: nine food/direct/repeated and shower/confirmation/back paths reach menu/home");
    }

    static void checkGrowthRoundTrip(DesktopPlatform& desktop, Canvas565& canvas) {
        using Scene = AppSceneFlow::Scene;
        for (bool rinse : {false, true}) {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = {};
            app.gameState.oobeDone = true;
            app.gameState.teamCount = 1;
            auto& monster = app.gameState.team[0];
            monster = Game::MonsterFactory::create(129, 5);
            monster.exp = minimumExpForLevel(findSpecies(129)->growthRate, 6) - 1;
            monster.hpCur = 1;
            app.gameState.bag.soap[0] = 1;
            app.openShowerScene(desktop.millis());
            if (rinse) {
                // Rinsing completes while its stage reward opens the level-up page.
                app.startShowerRinse(desktop.millis());
                desktop.advanceMs(1800);
                app.update(desktop.millis());
                assert(ShowerControllerTestAccess::fixture(app.showerController_).showerMode == ShowerMode::COMPLETE);
                assert(ShowerControllerTestAccess::fixture(app.showerController_).showerCompletionHearts == 1);
                assert(monster.hpCur > 2);
            } else {
                app.startShowerSoap(0, desktop.millis());
                send(app, desktop, TouchEventType::DOWN, 184, 212);
                desktop.advanceMs(100);
                send(app, desktop, TouchEventType::MOVE, 140, 212);
                send(app, desktop, TouchEventType::UP, 140, 212);
                assert(app.gameState.bag.soap[0] == 0);
                assert(ShowerControllerTestAccess::fixture(app.showerController_).showerSoapRewarded);
                assert(app.anyShowerFoam());
            }
            assert(app.sceneFlow.current() == Scene::PROGRESSION);
            assert(monster.level == 6 && app.gameState.pendingLevelUp);
            const auto earnedExperience = monster.exp;
            const auto recoveredHp = monster.hpCur;
            present(app, canvas);
            desktop.advanceMs(1000);
            app.update(desktop.millis());
            present(app, canvas);
            send(app, desktop, TouchEventType::DOWN, 184, 400);
            desktop.advanceMs(40);
            send(app, desktop, TouchEventType::UP, 184, 400);
            assert(app.sceneFlow.current() == Scene::SHOWER);
            assert(!app.gameState.pendingLevelUp);
            present(app, canvas);
            if (!rinse) {
                // More strokes after returning continue the same bath without a second reward.
                app.startShowerTool(ShowerMode::SOAPING, desktop.millis());
                send(app, desktop, TouchEventType::DOWN, 184, 212);
                desktop.advanceMs(300);
                send(app, desktop, TouchEventType::MOVE, 240, 212);
                send(app, desktop, TouchEventType::UP, 240, 212);
            }
            desktop.advanceMs(20);
            app.update(desktop.millis());
            assert(monster.exp == earnedExperience && monster.hpCur == recoveredHp);
            present(app, canvas);
            assert(app.flushSave());
            AmoledApp restored;
            restored.begin(desktop.millis());
            assert(restored.gameState.team[0].level == 6);
            assert(restored.gameState.team[0].exp == earnedExperience);
            assert(restored.gameState.team[0].hpCur == recoveredHp);
        }
        std::puts("shower growth: soap/rinse level-up returns retain progress, single rewards, completion recovery and saved state");
    }

    static void run(DesktopPlatform& desktop, Canvas565& canvas, const Game::GameState& state) {
        auto& original = desktop.serviceBundle();
        ShowerBlobSpy blobs(original.blobs);
        Platform::Services guarded{original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, original.power, blobs,
            original.resources, original.memory, original.peers};
        Platform::bind(guarded);
        desktop.advanceMs(1000);
        checkRoomReturnPath(desktop, state);
        {
            AmoledApp app;
            app.begin(desktop.millis());
            app.gameState = state;
            app.gameState.oobeDone = true;
            app.gameState.teamCount = 1;
            app.gameState.team[0] = Game::MonsterFactory::create(4, 30);
            app.gameState.bag.soap[0] = 2;
            app.gameState.careExpToday = 0;
            app.openShowerScene(desktop.millis());
            app.startShowerSoap(0, desktop.millis());
            assert(app.gameState.bag.soap[0] == 1);
            present(app, canvas);
            assert(app.renderCaches_.showerBackground.allocatedBytes() == 368 * 448 * 2);
            const size_t saved = blobs.writes;
            send(app, desktop, TouchEventType::DOWN, 184, 212);
            present(app, canvas);
            bool smallDamage = false;
            bool soapRewardSeen = false;
            uint32_t rewardedExperience = 0;
            for (int i = 0; i < 100; ++i) {
                desktop.advanceMs(20);
                const int x = 116 + (i % 35) * 4;
                send(app, desktop, TouchEventType::MOVE, x, 180 + (i % 4) * 8);
                app.update(desktop.millis());
                if (app.needsRender()) {
                    smallDamage = smallDamage ||
                        (app.renderColumnEnd() - app.renderColumnBegin()) *
                        (app.renderRowEnd() - app.renderRowBegin()) < 40000;
                }
                present(app, canvas);
                assert(blobs.writes == saved);
                if (ShowerControllerTestAccess::fixture(app.showerController_).showerSoapRewarded) {
                    if (soapRewardSeen) {
                        assert(app.gameState.team[0].exp == rewardedExperience);
                    } else {
                        soapRewardSeen = true;
                        rewardedExperience = app.gameState.team[0].exp;
                    }
                }
            }
            assert(smallDamage && soapRewardSeen && ShowerControllerTestAccess::fixture(app.showerController_).showerHopHeight > 0);
            assert(app.showerSavePending);
            send(app, desktop, TouchEventType::UP, ShowerControllerTestAccess::fixture(app.showerController_).showerToolX, ShowerControllerTestAccess::fixture(app.showerController_).showerToolY);
            assert(!app.showerSavePending && blobs.writes > saved);
            present(app, canvas);
            for (int i = 0; i < 65; ++i) {
                desktop.advanceMs(20); app.update(desktop.millis()); present(app, canvas);
            }
            desktop.advanceMs(20); app.update(desktop.millis());
            assert(!app.needsRender());
            // A stationary pointer does not create foam or brush progress.
            app.startShowerTool(ShowerMode::BRUSHING, desktop.millis());
            send(app, desktop, TouchEventType::DOWN, 184, 212);
            const auto bathBefore = ShowerControllerTestAccess::fixture(app.showerController_).showerBath;
            desktop.advanceMs(200);
            send(app, desktop, TouchEventType::MOVE, 184, 212);
            for (int i = 0; i < 8; ++i)
                assert(ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[i].brushProgress == bathBefore.foam[i].brushProgress);
            send(app, desktop, TouchEventType::UP, 184, 212);
            present(app, canvas);
            // Pointer cancellation ends the tool gesture, including later UP/MOVE events.
            send(app, desktop, TouchEventType::DOWN, 184, 212);
            desktop.advanceMs(100);
            send(app, desktop, TouchEventType::MOVE, 220, 212);
            app.cancelTouch();
            const auto stoppedX = ShowerControllerTestAccess::fixture(app.showerController_).showerToolX;
            const auto stoppedY = ShowerControllerTestAccess::fixture(app.showerController_).showerToolY;
            const auto stoppedExperience = app.gameState.team[0].exp;
            send(app, desktop, TouchEventType::MOVE, 100, 250);
            send(app, desktop, TouchEventType::UP, 100, 250);
            assert(!ShowerControllerTestAccess::fixture(app.showerController_).showerToolDragging);
            assert(ShowerControllerTestAccess::fixture(app.showerController_).showerToolX == stoppedX);
            assert(ShowerControllerTestAccess::fixture(app.showerController_).showerToolY == stoppedY);
            assert(app.gameState.team[0].exp == stoppedExperience);
            assert(app.sceneFlow.current() == AppSceneFlow::Scene::SHOWER);
            present(app, canvas);
            // Alpha continues animating after the gesture and stops at its endpoint.
            app.resetShowerSession(desktop.millis());
            ShowerControllerTestAccess::fixture(app.showerController_).showerBath.atmosphereTarget = true;
            app.forceFullRender(); present(app, canvas);
            for (int i = 0; i < 55; ++i) {
                desktop.advanceMs(20); app.update(desktop.millis()); present(app, canvas);
            }
            assert(ShowerControllerTestAccess::fixture(app.showerController_).showerAtmosphereAlpha == 255);
            desktop.advanceMs(40); app.update(desktop.millis());
            assert(!app.needsRender());
            for (int score : {1, 2, 3}) {
                app.resetShowerSession(desktop.millis());
                app.gameState.team[0].hpMax = 100;
                app.gameState.team[0].hpCur = 10;
                app.gameState.team[0].fainted = false;
                auto* record = Game::CareExperience::recordFor(app.gameState, app.gameState.team[0]);
                record->bathStageMask = 7;
                ShowerControllerTestAccess::fixture(app.showerController_).showerSoapRewarded = score >= 2;
                ShowerControllerTestAccess::fixture(app.showerController_).showerBrushRewarded = score >= 3;
                ShowerControllerTestAccess::fixture(app.showerController_).showerAtmosphereAlpha = 255;
                app.startShowerRinse(desktop.millis());
                present(app, canvas);
                bool shook = false;
                for (int i = 0; i < 105; ++i) {
                    desktop.advanceMs(20); app.update(desktop.millis());
                    shook = shook || ShowerControllerTestAccess::fixture(app.showerController_).showerWiggleStartedMs != 0;
                    present(app, canvas);
                }
                assert(shook);
                assert(ShowerControllerTestAccess::fixture(app.showerController_).showerMode == ShowerMode::COMPLETE);
                assert(ShowerControllerTestAccess::fixture(app.showerController_).showerCompletionHearts == score);
                assert(app.gameState.team[0].hpCur == (score == 1 ? 55 : score == 2 ? 80 : 100));
                const auto hp = app.gameState.team[0].hpCur;
                desktop.advanceMs(100); app.update(desktop.millis());
                assert(app.gameState.team[0].hpCur == hp);
            }
            // Resting offsets are actual landing positions and remain mergeable.
            app.resetShowerSession(desktop.millis());
            app.startShowerTool(ShowerMode::BRUSHING, desktop.millis());
            auto& first = ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[0];
            first.active = true; first.stage = 2; first.brushProgress = 4;
            first.restYOffset = -12;
            first.x = first.restX = 180;
            first.y = Game::BathInteraction::restY(app.showerBathLayout(), 2) - 12;
            auto& second = ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[1];
            second = first; second.x = second.restX = 200;
            assert(app.tryMergeShowerFoam(first, desktop.millis()));
            assert(first.stage == 3 && !second.active);
            // Lower-layer weight >6 triggers the full-body effect and brush reward.
            ShowerControllerTestAccess::fixture(app.showerController_).showerBath = Game::BathInteraction::State{};
            for (int i = 0; i < 3; ++i) {
                auto& foam = ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[i];
                foam.active = true; foam.x = foam.restX = 140 + i * 35;
                foam.y = 280; foam.stage = i == 0 ? 3 : i == 1 ? 2 : 0;
            }
            app.checkShowerAtmosphereThreshold(desktop.millis());
            assert(ShowerControllerTestAccess::fixture(app.showerController_).showerBath.atmosphereTarget && ShowerControllerTestAccess::fixture(app.showerController_).showerBrushRewarded);
            assert(!app.anyShowerFoam());
            // A qualifying rub can produce the Stick turn reaction.
            app.resetShowerSession(desktop.millis());
            ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[0].active = true;
            ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[0].x = 184; ShowerControllerTestAccess::fixture(app.showerController_).showerBath.foam[0].y = 212;
            ShowerControllerTestAccess::fixture(app.showerController_).showerLastReactionMs = desktop.millis() - 1600;
            GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
            app.rubShowerFoamAt(184, 212, desktop.millis());
            GameRandom::setRangeProvider(nullptr);
            assert(ShowerControllerTestAccess::fixture(app.showerController_).showerTurnUntilMs > desktop.millis() && ShowerControllerTestAccess::fixture(app.showerController_).showerTurnFlip);
            app.forceFullRender(); present(app, canvas);
            // Failed saves remain queued until a later successful retry.
            blobs.fail = true;
            app.showerSavePending = true;
            app.showerSaveAfterMs = desktop.millis();
            app.flushShowerSave(desktop.millis());
            assert(app.showerSavePending);
            const auto failedWrites = blobs.writes;
            blobs.fail = false;
            desktop.advanceMs(1000);
            app.flushShowerSave(desktop.millis());
            assert(app.showerSavePending && blobs.writes == failedWrites);
            desktop.advanceMs(1000);
            app.update(desktop.millis());
            assert(!app.showerSavePending);
        }
        Platform::bind(original);
        std::puts("shower: damage rectangles, idle, feedback, deferred saves and 1/2/3-heart recovery verified");
    }
};
}

void checkShowerFlow(DesktopPlatform& desktop, Canvas565& canvas, const Game::GameState& state) {
    AmoledV1::AmoledShowerTestAccess::run(desktop, canvas, state);
}

void checkShowerGrowthRoundTrip(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledShowerTestAccess::checkGrowthRoundTrip(desktop, canvas);
}
