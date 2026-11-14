#include "AmoledApp.h"
#include "core/AudioManager.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/PixelRenderer.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void run(AmoledApp& app, DesktopPlatform& desktop, const std::string& mode) {
        const bool solo = mode == "solo";
        const bool automatic = mode != "manual" && mode != "tutor";
        auto& state = app.gameState;
        state = {};
        state.oobeDone = true;
        state.teamCount = solo ? 1 : 2;
        state.team[0] = Game::MonsterFactory::create(25, 10);
        state.team[1] = Game::MonsterFactory::create(133, 10);
        state.team[0].satiety = state.team[1].satiety = 100;
        const bool muted = mode == "muted";
        desktop.setVolume(muted ? 0 : 30);
        state.settings.idleTimeoutIndex = mode == "idle120" ? 1 : mode == "never" ? 4 : 0;
        app.syncHomeActors(desktop.millis());
        const auto step = [&] {
            desktop.advanceMs(100);
            app.update(desktop.millis());
            if (app.sceneFadeActive()) {
                app.render(PixelRenderer::canvas());
                app.markRendered();
            }
        };
        assert(app.queueExploreDeparture(ExploreDestinations::MARKET, false));
        for (int n = 0; n < 1200 && (app.pendingExpedition ||
             app.expeditionDeparturePhase != AmoledApp::ExpeditionDeparturePhase::NONE); ++n) step();
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::MARKET && app.market_);
        assert(AudioManager::ins().requestedMusic() == MusicTrack::MARKET);
        assert(AudioManager::ins().playingMusic() == (muted ? MusicTrack::NONE : MusicTrack::MARKET));
        assert(!app.exploreSessionActive && app.expeditionMainHidden);
        assert(app.expeditionCompanionHidden == !solo);
        const uint32_t arrival = desktop.millis();
        if (mode == "never") {
            for (int n = 0; n < 1300; ++n) {
                step();
                assert(!app.consumeLockRequest());
            }
            assert(app.sceneFlow.current() == AppSceneFlow::Scene::MARKET);
            assert(app.market_ && !app.market_->exiting());
            app.market_->leave();
        } else {
            if (mode == "tutor") {
                app.openMarketTutor(desktop.millis());
                assert(AudioManager::ins().requestedMusic() == MusicTrack::MARKET);
                assert(AudioManager::ins().playingMusic() == MusicTrack::MARKET);
            }
            if (automatic) {
                bool requested = false;
                for (int n = 0; n < 1300 && !requested; ++n) {
                    step();
                    requested = app.consumeLockRequest();
                }
                assert(requested);
                const uint32_t expected = mode == "idle120" ? 120000 : 30000;
                assert(desktop.millis() - arrival == expected);
            }
            // Match main.cpp: retain the pending lock until the room return
            // and doorway animation finish. Never lock an invisible actor.
            assert(!app.prepareDisplayLock(desktop.millis()));
            assert(app.sceneFlow.current() == AppSceneFlow::Scene::MARKET);
            assert(app.expeditionDeparturePhase == AmoledApp::ExpeditionDeparturePhase::RETURN_FADE_OUT);
            assert(!app.marketTutorUi_.active() && !app.marketTutorPending_);
        }
        bool sawRoom = false;
        bool ready = false;
        for (int n = 0; n < 1200 && !ready; ++n) {
            step();
            sawRoom |= app.sceneFlow.current() == AppSceneFlow::Scene::HOME;
            if (mode == "never" && app.market_) continue;
            if (app.displayLockAllowed()) {
                ready = app.prepareDisplayLock(desktop.millis());
                assert(ready);
            } else {
                assert(!app.prepareDisplayLock(desktop.millis()));
            }
        }
        assert(sawRoom && ready);
        assert(AudioManager::ins().requestedMusic() == MusicTrack::HOME);
        assert(AudioManager::ins().playingMusic() == (muted ? MusicTrack::NONE : MusicTrack::HOME));
        assert(desktop.volume() == (muted ? 0 : 30));
        assert(!app.market_ && !app.exploreSessionActive);
        assert(!app.expeditionMainHidden && !app.expeditionCompanionHidden);
        assert(app.expeditionDeparturePhase == AmoledApp::ExpeditionDeparturePhase::NONE);
        assert(state.teamCount == (solo ? 1 : 2));
        assert(state.team[0].speciesId == 25 && state.team[0].lastExploredAt == 0);
        assert(!state.normalBossMissCount[0] && !state.explorePoolRerollCounts[0]);
        HomeViewModel model;
        app.fillHomePetModel(model);
        assert(model.petVisible);
        if (!solo) assert(app.homeCompanionActor.active && !app.homeCompanionActor.hidden);
        int16_t x = 0, y = 0;
        assert(app.lockFocusPoint(x, y));
        app.setDisplayLocked(true);
        assert(app.lockedHomePowerAllowed());
        for (int n = 0; n < 100; ++n) step();
        app.setDisplayLocked(false);
        app.onWake(desktop.millis());
        app.fillHomePetModel(model);
        assert(model.petVisible && app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
        assert(!app.market_ && !app.expeditionMainHidden && !app.expeditionCompanionHidden);
        assert(AudioManager::ins().requestedMusic() == MusicTrack::HOME);
        std::printf("market-lock %s: room return, visible party, lock focus and wake OK\n", mode.c_str());
    }
};
} // namespace AmoledV1

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin() && Platform::resources().mount() && GameAssets::begin());
    FontResource::ins().begin();
    std::vector<uint16_t> pixels(368 * 448);
    auto& canvas = PixelRenderer::canvas();
    canvas.attach({pixels.data(), 368, 448, false});
    canvas.setCoordinateScale(1);
    canvas.setLayoutScale(1);
    canvas.setAssetScale(1);
    canvas.setNativeText(true);
    AmoledV1::AmoledApp app;
    app.begin(desktop.millis());
    AmoledV1::AmoledAppTestAccess::run(app, desktop, argv[2]);
}
