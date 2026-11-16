// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "game/ContentData.h"
#if STICKMON_EXTERNAL_CONTENT || STICKMON_EXTERNAL_MAP_LAYOUTS
#include <cstdio>
#include "game/ExploreGrassPathData.h"
#include "game/ExploreMistForestData.h"
#include "game/ExploreValleyData.h"
#include "game/ExploreFrostLayouts.h"
#include "game/ExploreAncientValleyLayouts.h"
namespace MapLayoutValidation {
inline bool invalid(unsigned line) { std::fprintf(stderr,"[MapLayouts] invalid table at %u\n",line);return false; }
template<class P> inline bool point(const P& p) {return p.x<16 && p.y<12;}
inline bool validate() {
    if(!ExploreGrassPathData::TILES.ready() || !ExploreGrassPathData::ROOMS.ready() ||
       !ExploreMistForestData::TILES.ready() || !ExploreMistForestData::ROOMS.ready() || !ExploreMistForestData::TRANSITIONS.ready() ||
       !ExploreValleyData::TILES.ready() || !ExploreValleyData::FORESTS.ready() || !ExploreValleyData::CAVES.ready() || !ExploreValleyData::BRIDGES.ready() ||
       !ExploreFrostLayouts::LAYOUTS.ready() || !ExploreAncientValley::LAYOUTS.ready()) return invalid(__LINE__);
    for(const auto& tile:ExploreMistForestData::TILES)
        if(tile.frame>=ExploreMistForestData::LEGACY_FRAME_COUNT+ExploreMistForestData::FRAME_COUNT ||
           tile.depth>12 || (tile.flags & 0x80)) return invalid(__LINE__);
    for(const auto& tile:ExploreValleyData::TILES)
        if(!tile.frames || tile.frames>8 ||
           unsigned(tile.offset)+tile.frames>ExploreMistForestData::LEGACY_FRAME_COUNT ||
           tile.depth>12 || (tile.flags & 0x80)) return invalid(__LINE__);
    for(const auto& r:ExploreGrassPathData::ROOMS) {
        if(!point(r.entry) || !point(r.junction)) return invalid(__LINE__);
        for(const auto& layer:r.layers) for(auto tile:layer) if(tile>=49) return invalid(__LINE__);
        for(const auto& path:r.paths) {
            if(path.count<2 || path.count>48 || path.edge>3 || !point(path.exit)) return invalid(__LINE__);
            for(unsigned i=0;i<path.count;++i) if(!point(path.points[i])) return invalid(__LINE__);
        }
    }
    auto mistRoom=[](const ExploreMistForestData::Room& r) {
        if(!r.count || r.count>3 || !point(r.entry) || !point(r.exit)) return false;
        for(unsigned i=0;i<r.count;++i) {
            if(r.paths[i].count<2 || r.paths[i].count>48) return false;
            for(unsigned j=0;j<r.paths[i].count;++j) if(!point(r.paths[i].points[j])) return false;
        }
        return true;
    };
    for(const auto& bank:ExploreMistForestData::ROOMS) for(const auto& group:bank) for(const auto& r:group) if(!mistRoom(r)) return invalid(__LINE__);
    for(const auto& bank:ExploreMistForestData::TRANSITIONS) for(const auto& group:bank) for(const auto& r:group) if(!mistRoom(r)) return invalid(__LINE__);
    auto valleyRoom=[](const ExploreValleyData::Room& r) {
        if(r.count<2 || r.count>4) return false;
        for(unsigned i=0;i<r.count;++i) if(!point(r.portals[i].point) || r.portals[i].edge>3 || (r.portals[i].target!=255 && r.portals[i].target>=14) || r.portals[i].entrance>=4) return false;
        return true;
    };
    for(const auto& bank:ExploreValleyData::FORESTS) for(const auto& r:bank) if(!valleyRoom(r)) return invalid(__LINE__);
    for(const auto& bank:ExploreValleyData::CAVES) for(const auto& r:bank) if(!valleyRoom(r)) return invalid(__LINE__);
    for(const auto& bank:ExploreValleyData::BRIDGES) for(const auto& r:bank) if(!valleyRoom(r)) return invalid(__LINE__);
    for(const auto& r:ExploreFrostLayouts::LAYOUTS) {
        if(!point(r.junction) || r.iceRect[0]>r.iceRect[2] || r.iceRect[1]>r.iceRect[3] || r.iceRect[2]>=16 || r.iceRect[3]>=12) return invalid(__LINE__);
        for(unsigned i=0;i<2;++i) {
            if(r.controlCounts[i]<2 || r.controlCounts[i]>8) return invalid(__LINE__);
            for(unsigned j=0;j<r.controlCounts[i];++j) if(!point(r.controls[i][j])) return invalid(__LINE__);
        }
    }
    for(const auto& r:ExploreAncientValley::LAYOUTS) {
        if(!point(r.entry.point) || uint8_t(r.entry.edge)>3 || !point(r.junction)) return invalid(__LINE__);
        for(const auto& path:r.routes) {
            if(path.count<2 || path.count>12 || !point(path.exit.point) || uint8_t(path.exit.edge)>3) return invalid(__LINE__);
            for(unsigned i=0;i<path.count;++i) if(!point(path.controls[i])) return invalid(__LINE__);
        }
    }
    return true;
}
} // namespace MapLayoutValidation
#endif
