#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "AmoledApp.h"
#include "core/FontResource.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"
#include "ui/ExploreScreens.h"

namespace AmoledV1 {
struct AmoledAppTestAccess {
    static void check(DesktopPlatform& desktop) {
        AmoledApp app;
        app.begin(desktop.millis());
        auto& game = app.gameState;
        game = {};
        game.oobeDone = true;
        game.tutorialFlags = 0xff;
        game.teamCount = 2;
        game.team[0] = Game::MonsterFactory::create(7, 20);
        game.team[1] = Game::MonsterFactory::create(25, 20);
        game.team[0].satiety = 20;
        game.team[1].satiety = 19;
        assert(!app.queueExploreDeparture(0, false));
        assert(app.toast && std::strstr(app.toast, "皮卡丘"));
        assert(!app.pendingExpedition);
        assert(!app.exploreSession_.begin(game, 0));
        game.team[1].satiety = 20;
        assert(app.exploreSession_.begin(game, 0));
        app.exploreSessionActive = true;
        app.sceneFlow.enterExploreRoute();
        app.gameClock.set(desktop.millis(), 1438);
        game.gameMinutesTotal = 1438;
        app.lastCareMs = desktop.millis();
        auto& route = app.exploreSession_.state_;
        route.exploreRouteMap.pathCount = 1;
        auto& path = route.exploreRouteMap.paths[0];
        path.pointCount = 20;
        for (uint8_t i = 0; i < 20; ++i) path.points[i] = {static_cast<uint8_t>(i + 1), 2};
        route.exploreRouteEncounterCooldownSteps = 255;
        route.exploreRouteMapBlockCount = 1;
        route.exploreRouteBossPending = false;
        route.exploreRouteGuaranteedEncounterPending = false;
        for (unsigned i = 0; i < 7; ++i) {
            assert(app.exploreSession_.beginStep(game));
            assert(game.gameMinutesTotal == 1438 + i);
            app.exploreSession_.completeStep(game, desktop.millis());
            app.applyExploreEffects(desktop.millis());
            assert(game.gameMinutesTotal == 1439 + i);
            app.exploreSession_.completeStep(game, desktop.millis());
            app.applyExploreEffects(desktop.millis());
            assert(game.gameMinutesTotal == 1439 + i);
        }
        assert(game.team[0].satiety == 20 && game.team[1].satiety == 20);
        const uint16_t hp = game.team[0].hpCur;
        auto wild = Game::MonsterFactory::create(41, 3);
        app.battleSession_.begin(game, wild, 0, true, 100,
                                ExploreSpecial::Kind::NONE, 0, desktop.millis());
        app.battleSession_.performBattleSwitch(game, 1, false, desktop.millis());
        app.battleSession_.updateBattleSwitch(game, desktop.millis() + 1000);
        assert(app.battleSession_.state().battlePlayerSlot == 1);
        app.closeBattle(desktop.millis());
        assert(game.gameMinutesTotal == 1455);
        assert(game.team[0].satiety == 20 && game.team[1].satiety == 20);
        app.closeBattle(desktop.millis());
        assert(game.gameMinutesTotal == 1455);
        route.exploreRouteExiting = true;
        app.exploreSession_.finishExit(game);
        app.applyExploreEffects(desktop.millis());
        assert(app.expeditionDeparturePhase == AmoledApp::ExpeditionDeparturePhase::RETURN_FADE_OUT);
        assert(!app.exploreRouteModel().complete);
        assert(game.team[0].satiety == 16 && game.team[1].satiety == 16);
        assert(game.team[0].hpCur == hp);
        app.completeExploreReturn();
        assert(app.sceneFlow.current() == AppSceneFlow::Scene::HOME);
        assert(game.team[0].satiety == 16 && game.team[1].satiety == 16);
        app.expeditionDeparturePhase = AmoledApp::ExpeditionDeparturePhase::NONE;
        // Defeat also consumes the pending battle and partial walking once,
        // before the home scene can recover fainted monsters.
        game.team[0].satiety = game.team[1].satiety = 20;
        assert(app.exploreSession_.begin(game, 0));
        app.exploreSessionActive = true;
        app.sceneFlow.enterExploreRoute();
        app.battleSession_.begin(game, wild, 0, false, 100,
                                ExploreSpecial::Kind::NONE, 0, desktop.millis());
        app.battleSession_.state_.battlePhase = BattleViewModel::Phase::DEFEAT;
        app.finishBattleDefeat(desktop.millis());
        assert(game.gameMinutesTotal == 1460 && game.team[0].satiety == 19);
        assert(game.team[1].satiety == 20 && game.team[0].fainted);
        assert(game.team[0].lastSeenAt == 1460 * 60);
        app.finishBattleDefeat(desktop.millis());
        assert(game.gameMinutesTotal == 1460 && game.team[0].satiety == 19);
        std::puts("actual AMOLED: departure, completed steps, switch, boss, defeat, no home recovery and return passed");
    }
};
}
int main(int argc, char** argv) {
    assert(argc >= 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    assert(Platform::resources().mount());
    FontResource::ins().begin();
    AmoledV1::AmoledAppTestAccess::check(desktop);
}
