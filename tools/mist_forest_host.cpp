#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GameAssets.h"
#include "game/ExploreEcology.h"
#include "game/ExploreGrass.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "ui/ExploreMapRenderer.h"
#include "ui/models/ScreenModels.h"
#define private public
#include "app/ExploreSession.h"
#undef private

using namespace ExploreMapGenerator;

int main(int argc,char** argv) {
    if (argc==2 && std::strcmp(argv[1],"--fingerprints")==0) {
        for (uint32_t seed : {0U,1U,38U,91U,256U,294U,0xffffffffU})
            for (uint8_t count : {6,7,8})
                for (uint8_t level=0;level<count;++level)
                    for (uint8_t edge=0;edge<4;++edge) {
                        Map map;
                        assert(generate(seed,Edge(edge),4,map,{level,count,false}));
                        std::printf("%u %u %u %u %08x\n",seed,count,level,edge,fingerprint(map));
                    }
        return 0;
    }
    assert(argc==2 || argc==3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin() && Platform::resources().mount() && GameAssets::begin());
    std::array<uint16_t,416*312> pixels{};
    Canvas565 canvas;
    canvas.attach({pixels.data(),416,312,false});
    unsigned maps=0,grassContacts=0;
    ExplorePool::Pool pool{};
    pool.count=1;
    pool.entries[0]={1,1,ExplorePool::Rarity::COMMON};
    for (uint32_t seed : {38U,294U})
        for (uint8_t stage=0;stage<7;++stage)
            for (uint8_t edge=0;edge<4;++edge) {
                Map map;
                assert(generate(seed,Edge(edge),4,map,{stage,7,false}));
                assert(map.hasForest && !map.hasCoast && !map.hasCliff);
                ExploreEcology::Simulation ecology;
                ecology.reset(map,0,pool,0,0);
                canvas.fillSprite(0);
                for (uint8_t layer=0;layer<3;++layer)
                    for (uint16_t cell=0;cell<192;++cell) {
                        const auto tile=map.layers[layer][cell];
                        if (!tile) continue;
                        assert(GameAssets::drawExploreTileTo(canvas,tile,cell%16*26,cell/16*26,0,1));
                        if (ExploreMistForestData::flag(tile,64) && cell%16>0 && cell%16<15 && cell/16>0 && cell/16<11)
                            assert(ecology.surfaceAt(cell%16,cell/16)==ExploreEcology::Surface::GRASS);
                        if (!layer && (tile==390 || tile==399)) {
                            ExploreGrass::Simulation grass;
                            ExploreGrass::Contact contact{float(cell%16*26+9),float(cell/16*26+13),true};
                            grass.reset(0);
                            grass.update(map,0,true,&contact,1);
                            contact.x+=8;
                            grass.update(map,50,true,&contact,1);
                            assert(grass.frame(cell).phase);
                            ++grassContacts;
                        }
                    }
                if (argc==3 && ((seed==38U && (edge==2 || stage==5)) || (seed==294U && edge==2 && stage==3))) {
                    const auto name=std::string(argv[2])+"/forest-"+std::to_string(seed)+"-"+
                        std::to_string(stage)+"-"+std::to_string(edge)+".rgb565";
                    FILE* file=std::fopen(name.c_str(),"wb");
                    assert(file && std::fwrite(pixels.data(),2,pixels.size(),file)==pixels.size());
                    std::fclose(file);
                }
                ++maps;
            }
    // Restore each new canopy only inside an actor's clipped rectangle and
    // only while its feet are behind the object's shared ground anchor.
    std::vector<uint16_t> screen(AmoledUi::WIDTH*AmoledUi::HEIGHT);
    canvas.attach({screen.data(),AmoledUi::WIDTH,AmoledUi::HEIGHT,false});
    unsigned objects=0;
    for (uint16_t id=10000;ExploreMistForestData::tile(id);++id) {
        const auto spec=*ExploreMistForestData::tile(id);
        if (!spec.depth) continue;
        Map map{};
        map.areaIndex=4;
        map.layers[1][3*16+3]=id;
        AmoledV1::ExploreRouteViewModel model{};
        model.map=&map;
        constexpr int x=156,y=156,size=52;
        constexpr uint16_t empty=0xf81f,actor=0x07ff;
        canvas.clearClipRect();
        canvas.fillSprite(empty);
        assert(GameAssets::drawExploreTileTo(canvas,id,x,y,0,2));
        const auto original=screen;
        unsigned restored=0;
        for (bool behind : {true,false}) {
            canvas.clearClipRect();
            canvas.fillSprite(actor);
            const float ground=(3+spec.depth)*26;
            AmoledV1::drawExploreRouteMapVegetationForActor(canvas,model,
                behind ? ground-.01f : ground,x+4,y+4,x+size-4,y+size-4,0,
                AmoledV1::EXPLORE_ROUTE_MAP_BOTTOM,false,false,false);
            for (int py=0;py<AmoledUi::HEIGHT;++py)
                for (int px=0;px<AmoledUi::WIDTH;++px) {
                    const auto i=py*AmoledUi::WIDTH+px;
                    const bool cover=behind && px>=x+4 && px<x+size-4 &&
                        py>=y+4 && py<y+size-4 && original[i]!=empty;
                    assert(screen[i]==(cover ? original[i] : actor));
                    restored+=cover;
                }
        }
        assert(restored);
        ++objects;
    }
    canvas.clearClipRect();
    unsigned journeys=0;
    for (uint8_t count : {6,7,8})
        for (uint8_t edge=0;edge<4;++edge) {
            Game::GameState game{};
            AmoledV1::ExploreSession session;
            auto& s=session.state_;
            s.exploreRouteExpeditionSeed=38;
            s.exploreRouteMapBlockCount=count;
            s.exploreRoutePendingEntryEdge=Edge(edge);
            assert(session.generateMap(game,4));
            for (uint8_t level=0;level<count;++level) {
                assert(s.exploreRouteMapBlock==level);
                assert(s.exploreRouteMap.hasForest);
                s.exploreRoutePath=s.exploreRouteMap.pathCount-1;
                s.exploreRouteIndex=s.exploreRouteMap.paths[s.exploreRoutePath].pointCount-1;
                const bool final=level+1==count;
                assert(session.finishAtEnd(game)!=final);
                assert(s.exploreRouteComplete==final);
            }
            ++journeys;
        }
    std::printf("mist-native=ok maps=%u objects=%u grass=%u journeys=%u\n",
                maps,objects,grassContacts,journeys);
}
