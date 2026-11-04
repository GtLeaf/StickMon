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

int main(int argc,char** argv) {
    assert(argc==3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin());
    auto& engine=GameEngine::ins();
    assert(engine.begin());
    auto& game=engine.gameState();
    game=Game::GameState{};
    game.oobeDone=true;
    game.tutorialFlags=0xff;
    game.teamCount=1;
    game.team[0]=Game::MonsterFactory::create(1,35);
    for (uint8_t a=0;a<4;++a) game.explorePoolRerollCounts[a]=1;
    GameRandom::seed(38);
    ExploreScene scene;
    scene.onEnter();
    scene.activeArea=ExploreScene::Area(4);
    unsigned journeys=0;
    for (uint8_t count : {6,7,8})
        for (uint8_t edge=0;edge<4;++edge) {
            scene.mapBlockCount=count;
            scene.currentMapBlock=0;
            for (auto& area:scene.mapBlocks) area=4;
            scene.expeditionSeed=38;
            scene.pendingEntryEdge=ExploreMapGenerator::Edge(edge);
            scene.expeditionBossScheduled=false;
            assert(scene.resetRouteSegment());
            for (uint8_t level=0;level<count;++level) {
                assert(scene.currentMapBlock==level);
                assert(scene.generatedMap.areaIndex==4 && scene.generatedMap.hasForest);
                scene.currentRoutePath=scene.generatedMap.pathCount-1;
                assert(scene.finalRoute()==(level+1==count));
                if (level+1==count) break;
                const auto exit=scene.generatedMap.paths[scene.currentRoutePath].exit;
                scene.advanceMapBlock(4);
                const auto entry=scene.generatedMap.entry;
                assert(entry.edge==ExploreMapGenerator::opposite(exit.edge));
                assert(edge%2 ? entry.point.y==exit.point.y : entry.point.x==exit.point.x);
            }
            ++journeys;
        }
    std::printf("mist-scene=ok journeys=%u\n",journeys);
}
