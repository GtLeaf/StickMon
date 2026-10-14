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

    static void run(DesktopPlatform& desktop, Canvas565& canvas, const Game::GameState& state) {
        auto& original = desktop.serviceBundle();
        ShowerBlobSpy blobs(original.blobs);
        Platform::Services guarded{original.lifecycle, original.clock, original.logger, original.input,
            original.display, original.audio, original.imu, original.power, blobs,
            original.resources, original.memory, original.peers};
        Platform::bind(guarded);
        desktop.advanceMs(1000);
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
            }
            assert(smallDamage && app.showerSoapRewarded && app.showerHopHeight > 0);
            assert(app.showerSavePending);
            send(app, desktop, TouchEventType::UP, app.showerToolX, app.showerToolY);
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
            const auto bathBefore = app.showerBath;
            desktop.advanceMs(200);
            send(app, desktop, TouchEventType::MOVE, 184, 212);
            for (int i = 0; i < 8; ++i)
                assert(app.showerBath.foam[i].brushProgress == bathBefore.foam[i].brushProgress);
            send(app, desktop, TouchEventType::UP, 184, 212);
            present(app, canvas);
            // Alpha continues animating after the gesture and stops at its endpoint.
            app.resetShowerSession(desktop.millis());
            app.showerBath.atmosphereTarget = true;
            app.forceFullRender(); present(app, canvas);
            for (int i = 0; i < 55; ++i) {
                desktop.advanceMs(20); app.update(desktop.millis()); present(app, canvas);
            }
            assert(app.showerAtmosphereAlpha == 255);
            desktop.advanceMs(40); app.update(desktop.millis());
            assert(!app.needsRender());
            for (int score : {1, 2, 3}) {
                app.resetShowerSession(desktop.millis());
                app.gameState.team[0].hpMax = 100;
                app.gameState.team[0].hpCur = 10;
                app.gameState.team[0].fainted = false;
                app.gameState.careExpToday = 100;
                app.showerSoapRewarded = score >= 2;
                app.showerBrushRewarded = score >= 3;
                app.showerAtmosphereAlpha = 255;
                app.startShowerRinse(desktop.millis());
                present(app, canvas);
                bool shook = false;
                for (int i = 0; i < 105; ++i) {
                    desktop.advanceMs(20); app.update(desktop.millis());
                    shook = shook || app.showerWiggleStartedMs != 0;
                    present(app, canvas);
                }
                assert(shook);
                assert(app.showerMode == ShowerMode::COMPLETE);
                assert(app.showerCompletionHearts == score);
                assert(app.gameState.team[0].hpCur == (score == 1 ? 55 : score == 2 ? 80 : 100));
                const auto hp = app.gameState.team[0].hpCur;
                desktop.advanceMs(100); app.update(desktop.millis());
                assert(app.gameState.team[0].hpCur == hp);
            }
            // Resting offsets are actual landing positions and remain mergeable.
            app.resetShowerSession(desktop.millis());
            app.startShowerTool(ShowerMode::BRUSHING, desktop.millis());
            auto& first = app.showerBath.foam[0];
            first.active = true; first.stage = 2; first.brushProgress = 4;
            first.restYOffset = -12;
            first.x = first.restX = 180;
            first.y = Game::BathInteraction::restY(app.showerBathLayout(), 2) - 12;
            auto& second = app.showerBath.foam[1];
            second = first; second.x = second.restX = 200;
            assert(app.tryMergeShowerFoam(first, desktop.millis()));
            assert(first.stage == 3 && !second.active);
            // Lower-layer weight >6 triggers the full-body effect and brush reward.
            app.showerBath = Game::BathInteraction::State{};
            for (int i = 0; i < 3; ++i) {
                auto& foam = app.showerBath.foam[i];
                foam.active = true; foam.x = foam.restX = 140 + i * 35;
                foam.y = 280; foam.stage = i == 0 ? 3 : i == 1 ? 2 : 0;
            }
            app.checkShowerAtmosphereThreshold(desktop.millis());
            assert(app.showerBath.atmosphereTarget && app.showerBrushRewarded);
            assert(!app.anyShowerFoam());
            // A qualifying rub can produce the Stick turn reaction.
            app.resetShowerSession(desktop.millis());
            app.showerBath.foam[0].active = true;
            app.showerBath.foam[0].x = 184; app.showerBath.foam[0].y = 212;
            app.showerLastReactionMs = desktop.millis() - 1600;
            GameRandom::setRangeProvider(+[](uint32_t minimum, uint32_t) { return minimum; });
            app.rubShowerFoamAt(184, 212, desktop.millis());
            GameRandom::setRangeProvider(nullptr);
            assert(app.showerTurnUntilMs > desktop.millis() && app.showerTurnFlip);
            app.forceFullRender(); present(app, canvas);
            // Failed saves remain queued until a later successful retry.
            blobs.fail = true;
            app.showerSavePending = true;
            app.showerSaveAfterMs = desktop.millis();
            app.flushShowerSave(desktop.millis());
            assert(app.showerSavePending);
            blobs.fail = false;
            desktop.advanceMs(1000);
            app.flushShowerSave(desktop.millis());
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
