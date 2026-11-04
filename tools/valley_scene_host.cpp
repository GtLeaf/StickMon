#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "game/GameRandom.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#define private public
#include "core/GameEngine.h"
#include "scenes/ExploreScene.h"
#undef private

int main(int argc, char** argv) {
    assert(argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& engine = GameEngine::ins();
    assert(engine.begin());
    auto& game = engine.gameState();
    game = Game::GameState{};
    game.oobeDone = true;
    game.tutorialFlags = 0xff;
    game.teamCount = 1;
    game.team[0] = Game::MonsterFactory::create(1, 35);
    for (uint8_t a = 0; a < 5; ++a) game.explorePoolRerollCounts[a] = 1;
    GameRandom::seed(38);
    ExploreScene scene;
    scene.onEnter();
    scene.activeArea = ExploreScene::Area(5);
    for (const auto sequence : {std::vector<uint8_t>{1, 2, 5, 10, 11, 13, 12},
                               std::vector<uint8_t>{1, 4, 6, 7, 8}}) {
        scene.mapBlockCount = 9;
        scene.currentMapBlock = 0;
        for (auto& area : scene.mapBlocks) area = 5;
        scene.expeditionSeed = 38;
        scene.pendingValley = {};
        scene.expeditionBossScheduled = false;
        assert(scene.resetRouteSegment());
        for (uint8_t target : sequence) {
            const auto before = scene.currentMapBlock;
            bool found = false;
            for (uint8_t p = 0; p < scene.generatedMap.pathCount; ++p) {
                if (scene.generatedMap.paths[p].nextValleyNode != target) continue;
                scene.currentRoutePath = p;
                const bool internal = ExploreMapGenerator::internalValleyTransfer(scene.generatedMap, p);
                scene.advanceMapBlock(5);
                assert(scene.generatedMap.valleyJourney && scene.generatedMap.valleyNode == target);
                assert(scene.currentMapBlock == before + (internal ? 0 : 1));
                found = true;
                break;
            }
            assert(found);
        }
        bool terminal = false;
        for (uint8_t p = 0; p < scene.generatedMap.pathCount; ++p) {
            if (scene.generatedMap.paths[p].nextValleyNode != 255) continue;
            scene.currentRoutePath = p;
            assert(scene.finalRoute());
            terminal = true;
        }
        assert(terminal);
    }
    // Starting the bridge on the last available block still visits all rooms.
    scene.currentMapBlock = scene.mapBlockCount - 1;
    scene.pendingValley = {10, 0};
    assert(scene.resetRouteSegment());
    for (uint8_t target : {11, 13, 12}) {
        assert(!scene.finalRoute());
        scene.advanceMapBlock(5);
        assert(scene.generatedMap.valleyNode == target);
        assert(scene.currentMapBlock == scene.mapBlockCount - 1);
    }
    assert(scene.finalRoute());
    std::puts("valley-scene=ok forest=6 caves=4 bridge=3 journeys=2 quota=ok");
}
