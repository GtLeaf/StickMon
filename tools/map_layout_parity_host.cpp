#include "core/MapLayoutResources.h"
#include "game/ContentData.h"
#include "game/ExploreMapGenerator.h"
#include "game/ExploreTileSemantics.h"
#include "game/ExploreGrassPathData.h"
#include "game/ExploreMistForestData.h"
#include "game/ExploreValleyData.h"
#include "game/ExploreFrostLayouts.h"
#include "game/ExploreAncientValleyLayouts.h"
#include "platform/desktop/DesktopPlatform.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace ExploreMapGenerator {
template<class V> void contentFields(V& v,Path::Offset& p) {v(p.x,p.y);}
template<class V> void contentFields(V& v,Path& p) {
    v(p.points,p.pointCount,p.exit,p.fallsToNextLevel,p.cellCentered,p.nextValleyNode,
      p.nextValleyEntrance,p.terminal,p.interiorExit,p.visualOffsets);
}
template<class V> void contentFields(V& v,Map& m) {
    v(m.layers,m.entry,m.junction,m.paths,m.pathCount,m.seed,m.areaIndex,m.hasCoast,
      m.hasForest,m.hasCreek,m.hasCliff,m.hasWaterfall,m.valleyJourney,m.valleyNode);
}
}
template<class T,size_t N> void emitTable(ContentData::Writer& w,const T (&rows)[N]) {
    uint32_t count=N;w(count);
    for(const auto& row:rows) ContentData::field(w,const_cast<T&>(row));
}
template<class T,size_t N,bool Exact> void emitTable(ContentData::Writer& w,
        const ContentData::Table<T,N,Exact>& rows) {
    uint32_t count=rows.size();w(count);
    for(const auto& row:rows) ContentData::field(w,const_cast<T&>(row));
}
int main(int argc,char** argv) {
    assert(argc==4);DesktopPlatform platform(argv[1]);Platform::bind(platform.serviceBundle());
    assert(platform.begin());const bool wanted=std::atoi(argv[3])!=0;
    assert(beginMapLayoutResources()==wanted);if(!wanted)return 0;
#if STICKMON_EXTERNAL_MAP_LAYOUTS
    // Preloaded typed objects must remain valid with every backing file gone.
    std::filesystem::rename(std::filesystem::path(argv[1])/"packs/dev/catalog",
                            std::filesystem::path(argv[1])/"cached-catalog");
    assert(beginMapLayoutResources());
#endif
    FILE* file=std::fopen(argv[2],"wb");assert(file);
    ContentData::Writer w{file,[](void* context,uint8_t b){assert(std::fwrite(&b,1,1,static_cast<FILE*>(context))==1);}};
    // Compare every field of every record, including layouts not selected by
    // the seed sweep, without comparing native structure padding.
    emitTable(w,ExploreGrassPathData::TILES);emitTable(w,ExploreGrassPathData::ROOMS);
    emitTable(w,ExploreMistForestData::TILES);emitTable(w,ExploreMistForestData::ROOMS);
    emitTable(w,ExploreMistForestData::TRANSITIONS);emitTable(w,ExploreValleyData::TILES);
    emitTable(w,ExploreValleyData::FORESTS);emitTable(w,ExploreValleyData::CAVES);
    emitTable(w,ExploreValleyData::BRIDGES);emitTable(w,ExploreFrostLayouts::LAYOUTS);
    emitTable(w,ExploreAncientValley::LAYOUTS);
    using namespace ExploreMapGenerator;
    for(unsigned sample=0;sample<34;++sample) {
        const uint32_t seed=sample<32?sample:sample==32?0x80000000U:0xffffffffU;
        for(uint8_t area=0;area<6;++area) for(uint8_t edge=0;edge<4;++edge)
            for(uint8_t level: {uint8_t(0),uint8_t(3),uint8_t(7)}) {
                Map map{};bool ok=generate(seed,static_cast<Edge>(edge),area,map,{level,8,level!=0});
                w(ok,map);
            }
        for(uint8_t node=0;node<14;++node) for(uint8_t entry=0;entry<4;++entry)
            for(bool final: {false,true}) {
                Map map{};bool ok=generateValleyRegion(seed,{node,entry},final,map);w(ok,map);
            }
    }
    for(uint16_t tile=0;tile<10000;++tile) for(uint8_t layer=0;layer<3;++layer) {
        int depth=ExploreTileSemantics::objectDepthOffsetRows(tile,layer);
        bool ground=ExploreTileSemantics::groundBehindDecoration(tile,layer);
        bool ladder=ExploreTileSemantics::upwardLadder(tile,layer);
        w(depth,ground,ladder);
    }
    assert(w.good);assert(std::fclose(file)==0);
}
