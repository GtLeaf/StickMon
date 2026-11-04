#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#define private public
#include "platform/desktop/DesktopPlatform.h"
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private
#include "core/FontResource.h"
#include "presentation/PixelRenderer.h"
#include "game/MonsterFactory.h"
#include "game/ExploreDestinations.h"
#include "ui/common/ExploreRouteUiContent.h"

int main(int argc, char** argv) {
    assert(argc == 2);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& state = engine.gameState();
    state = {};
    state.oobeDone = true;
    state.tutorialFlags = 0xff;
    state.teamCount = 2;
    state.team[0] = Game::MonsterFactory::create(7, 20);
    state.team[1] = Game::MonsterFactory::create(25, 20);
    state.team[0].satiety = 20;
    state.team[1].satiety = 19;
    assert(!engine.beginExploreDeparture(0));
    ExploreScene scene;
    scene.activateExplore(Ui::ExploreAction::DEPART, ExploreDestinations::visibleIndex(0,state));
    assert(scene.departureToastUntil && std::strstr(scene.departureToast, "皮卡丘"));
    assert(scene.exploreRouteFrame().toast);
    scene.update(scene.departureToastUntil, 0);
    assert(!scene.departureToastUntil && !scene.exploreRouteFrame().toast);
    state.team[1].satiety = 20;
    assert(engine.beginExploreDeparture(0));
    engine.currentId = SceneID::EXPLORE;
    engine.exploreTravel = ExploreTravelPhase::ACTIVE;
    engine.gameClock.set(desktop.millis(), 1438);
    state.gameMinutesTotal = 1438;
    engine.lastCareGameMinutes = 1438;
    engine.lastCareMs = desktop.millis();
    engine.saveCoordinator.reset(desktop.millis());
    scene.phase = ExploreScene::Phase::WALKING;
    scene.exploreCosts.begin();
    scene.generatedMap.pathCount = 1;
    auto& path = scene.generatedMap.paths[0];
    path.pointCount = 20;
    for (uint8_t i = 0; i < 20; ++i) path.points[i] = {static_cast<uint8_t>(i + 1), 2};
    scene.encounterCooldownSteps = 255;
    scene.mapBlockCount = 1;
    for (unsigned i = 0; i < 7; ++i) {
        scene.walk();
        const uint32_t before = state.gameMinutesTotal;
        scene.updateRouteMovement(scene.routeMoveStarted + scene.routeMoveDurationMs - 1);
        assert(state.gameMinutesTotal == before);
        scene.updateRouteMovement(scene.routeMoveStarted + scene.routeMoveDurationMs);
        assert(state.gameMinutesTotal == before + 1);
        scene.updateRouteMovement(scene.routeMoveStarted + scene.routeMoveDurationMs);
        assert(state.gameMinutesTotal == before + 1);
    }
    assert(state.team[0].satiety == 20 && state.team[1].satiety == 20);
    assert(!engine.saveCoordinator.soon_);
    const uint16_t hp = state.team[0].hpCur;
    scene.beginEncounter(engine.speciesFor(state.team[0]), 3, true);
    scene.beginBattleSwitch(1, false);
    scene.updateBattleSwitch(scene.battleSwitchStarted + 1000);
    scene.resumeWalk();
    assert(state.gameMinutesTotal == 1455);
    assert(state.team[0].satiety == 20 && state.team[1].satiety == 20);
    scene.resumeWalk();
    assert(state.gameMinutesTotal == 1455);
    scene.beginExploreEnding();
    assert(scene.phase != ExploreScene::Phase::ENDING);
    assert(engine.exploreTravel == ExploreTravelPhase::RETURNING);
    assert(state.team[0].satiety == 16 && state.team[1].satiety == 16);
    scene.beginExploreEnding();
    assert(state.team[0].satiety == 16);
    assert(state.team[0].hpCur == hp); // Extra time never grants home recovery.
    assert(engine.gameMinutesTotal() == 1455);
    engine.currentId = SceneID::MAIN;
    engine.exploreTravel = ExploreTravelPhase::NONE;
    engine.tickCare(desktop.millis());
    assert(state.team[0].hpCur == hp);
    engine.saveNow();
    Game::GameState restored;
    MainSceneViewState view;
    assert(engine.saveManager.load(restored, view));
    assert(restored.gameMinutesTotal == 1455 && restored.team[1].satiety == 16);
    std::puts("actual horizontal: departure gate, completion only, boss switch, return once, clock and saved costs passed");
}
