// SPDX-License-Identifier: Apache-2.0
#include "core/ContentValidation.h"
#include "game/ContentData.h"
#include <cstdio>
#include <memory>
#include <new>
#if STICKMON_EXTERNAL_CONTENT
#include "game/Species.h"
#include "core/ResourceFS.h"
#include "game/ExploreGrassPathData.h"
#include "game/ExploreMistForestData.h"
#include "game/ExploreValleyData.h"
#include "game/ExploreFrostLayouts.h"
#include "game/ExploreAncientValleyLayouts.h"
#include "game/ExploreEditorTiles.h"
#include "game/ExploreRotatedTiles.h"
#include "game/ExploreEncounters.h"
#include "game/ExploreBoss.h"
#include "game/ExplorePickupRules.h"
#include "game/MarketMapData.h"
#include "assets/PokemonSprites.h"
#include "assets/PokemonBattleSizes.h"
#include "assets/PokemonMotion.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreEcology.h"
#include "game/ItemCatalog.h"
#include "game/MarketSession.h"

namespace {
bool invalid(unsigned line) { std::fprintf(stderr,"[Content] invalid catalog at %u\n",line); return false; }
template<class P> bool point(const P& p) { return p.x<16 && p.y<12; }
}
bool validateContentCatalog() {
    if(!ResourceFS::ins().begin()) return invalid(__LINE__);
    if(!speciesTable() || !speciesCount() || starterSpecies().id!=1) return invalid(__LINE__);
    for(unsigned i=0;i<speciesCount();++i) {
        const auto& s=speciesTable()[i];
        if(!s.id || !s.name || !*s.name || uint8_t(s.type1)>17 ||
           (uint8_t(s.type2)>17 && s.type2!=TypeId::NONE) || uint8_t(s.growthRate)>5 ||
           uint8_t(s.evolveMethod)>uint8_t(EvolutionMethod::BRANCH) || s.evolveLevel>Game::LEVEL_MAX ||
           !s.stats.hp || !s.stats.atk || !s.stats.def || !s.stats.spa || !s.stats.spd || !s.stats.spe ||
           (s.evolveTo && !findSpecies(s.evolveTo))) return invalid(__LINE__);
        for(unsigned j=0;j<i;++j) if(speciesTable()[j].id==s.id) return invalid(__LINE__);
        if(!findLearnset(s.id) || !findMove(basicMoveIdForSpecies(s))) return invalid(__LINE__);
    }
    ContentData::Table<MoveInfo,2048,false> moves{3};
    ContentData::Table<MoveEffectSpec,4096,false> effects{2};
    ContentData::Table<LearnsetEntry,16384,false> learned{4};
    ContentData::Table<SpeciesLearnset,1024,false> sets{5};
    ContentData::Table<SpeciesAbility,1024,false> abilities{9};
    ContentData::Table<PokemonSprites::RoomSpriteProfile,1024,false> roomProfiles{8};
    if(!moves.ready() || !effects.ready() || !learned.ready() || !sets.ready() ||
       !abilities.ready() || !roomProfiles.ready()) return invalid(__LINE__);
    uint16_t previous=0;
    for(const auto& m:moves) {
        if(!m.id || m.id<=previous || uint8_t(m.type)>17 || uint8_t(m.damageClass)>2 ||
           m.effectOffset+m.effectCount>effects.size() || !m.minHits || m.minHits>m.maxHits) return invalid(__LINE__);
        previous=m.id;
    }
    for(const auto& e:effects) if(uint8_t(e.kind)>11 || uint8_t(e.target)>1 || e.chance>100 || e.minTurns>e.maxTurns) return invalid(__LINE__);
    previous=0;
    for(const auto& set:sets) {
        if(set.speciesId<=previous || size_t(set.offset)+set.count>learned.size()) return invalid(__LINE__);
        previous=set.speciesId;
    }
    for(const auto& e:learned) if(e.level>Game::LEVEL_MAX || !findMove(e.moveId)) return invalid(__LINE__);
    {
        ContentData::Table<Game::MoveId,16384,false> tutor{54};
        ContentData::Table<SpeciesTutorMoves,1024,false> tutorSets{55};
        if(!tutor.ready() || !tutorSets.ready()) return invalid(__LINE__);
        previous=0;
        for(const auto& set:tutorSets) {
            if(set.speciesId<=previous || size_t(set.offset)+set.count>tutor.size()) return invalid(__LINE__);
            previous=set.speciesId;
        }
        for(const auto id:tutor) if(!findMove(id)) {
            std::fprintf(stderr,"[Content] unknown tutor move %u (table 54)\n",unsigned(id));
            return invalid(__LINE__);
        }
    }
    for(const auto& a:abilities) if(uint8_t(a.ability)>uint8_t(AbilityId::SYNCHRONIZE)) return invalid(__LINE__);
    for(const auto& p:roomProfiles) if(!p.idleFrames || !p.walkingFrames || !p.sleepingFrames ||
        uint8_t(p.motionMode)>2 || !(p.airHeight>=0 && p.airHeight<=64) || !(p.bobAmplitude>=0 && p.bobAmplitude<=16)) return invalid(__LINE__);
    if(!PokemonBattleSizes::FRONT.ready() || !PokemonMotion::PROFILES.ready() || !ExploreAreaCatalog::AREAS.ready()) return invalid(__LINE__);
    previous=0;
    for(const auto& b:PokemonBattleSizes::FRONT) {
        if(b.speciesId<=previous || !b.width || !b.height || b.width>160 || b.height>160) return invalid(__LINE__);
        previous=b.speciesId;
    }
    for(const auto& p:PokemonMotion::PROFILES) if(uint8_t(p.mode)>2 ||
        !(p.air.height>=0 && p.air.height<=64) || !(p.air.bobAmplitude>=0 && p.air.bobAmplitude<=16)) return invalid(__LINE__);
    for(const auto& a:ExploreAreaCatalog::AREAS) if(!a.minMapCount || a.minMapCount>a.maxMapCount || a.maxMapCount>9 ||
        !a.recommendedLevel || a.recommendedLevel>Game::LEVEL_MAX || a.depthSpread>10 || a.encounterChance>10000 ||
        uint16_t(a.battleBackground)>=uint16_t(GameAssets::Kind::COUNT)) return invalid(__LINE__);
    for(unsigned i=0;i<speciesCount();++i) {
        const auto id=speciesTable()[i].id;
        PokemonSprites::WalkingAnimation animation{};PokemonSprites::PetAnimationProfile profile{};
        if(!PokemonSprites::walkingAnimation(id,PokemonSprites::WalkDirection::DOWN,animation) ||
           !PokemonSprites::petAnimationProfile(id,profile) || !PokemonMotion::profile(id) || !PokemonBattleSizes::front(id).width) return invalid(__LINE__);
        bool found=false;for(const auto& p:roomProfiles) if(p.speciesId==id)found=true;
        if(!found) return invalid(__LINE__);
    }
    if(!ExploreEcology::PROFILES.ready() || !ExploreEcology::GROUP_FAMILIES.ready() || !ItemCatalog::ITEMS.ready()) return invalid(__LINE__);
    for(const auto& p:ExploreEcology::PROFILES) if(uint8_t(p.temperament)>3 || uint8_t(p.locomotion)>2) return invalid(__LINE__);
    for(const auto& f:ExploreEcology::GROUP_FAMILIES) {
        if(f.count<2 || f.count>3) return invalid(__LINE__);
        for(unsigned i=0;i<f.count;++i) if(!findSpecies(f.species[i])) return invalid(__LINE__);
    }
    bool items[size_t(Game::ItemId::COUNT)]{};
    for(const auto& e:ItemCatalog::ITEMS) {
        const auto id=size_t(e.id);
        if(id>=size_t(Game::ItemId::COUNT) || items[id] || e.unlockArea>=6 || !e.name || !*e.name) return invalid(__LINE__);
        items[id]=true;
    }
    for(unsigned id=51;id<=53;++id) {
        ContentData::Table<Game::ItemId,32,false> list{uint16_t(id)};
        if(!list.ready()) return invalid(__LINE__);
        bool seen[size_t(Game::ItemId::COUNT)]{};
        for(auto item:list) {
            auto index=size_t(item);
            if(index>=size_t(Game::ItemId::COUNT) || seen[index]) return invalid(__LINE__);
            seen[index]=true;
        }
    }
    PokemonSprites::WalkingAnimation walk{}; PokemonSprites::PetAnimationProfile pet{};
    if(!PokemonSprites::walkingAnimation(1,PokemonSprites::WalkDirection::DOWN,walk) ||
       !PokemonSprites::petAnimationProfile(1,pet)) return invalid(__LINE__);
    if(!ExploreGrassPathData::TILES.ready() || !ExploreGrassPathData::ROOMS.ready() ||
       !ExploreMistForestData::TILES.ready() || !ExploreMistForestData::ROOMS.ready() || !ExploreMistForestData::TRANSITIONS.ready() ||
       !ExploreValleyData::TILES.ready() || !ExploreValleyData::FORESTS.ready() || !ExploreValleyData::CAVES.ready() || !ExploreValleyData::BRIDGES.ready() ||
       !ExploreFrostLayouts::LAYOUTS.ready() || !ExploreAncientValley::LAYOUTS.ready() ||
       !ExploreEditorTiles::TILES.ready() || !ExploreEditorTiles::EXTRA_TILES.ready() ||
       !ExploreRotatedTiles::TILES.ready() || !ExploreBoss::CONFIGS.ready() || !MarketMapData::CONTENT.ready()) return invalid(__LINE__);
    for(const auto& r:ExploreGrassPathData::ROOMS) {
        if(!point(r.entry) || !point(r.junction)) return invalid(__LINE__);
        for(const auto& layer:r.layers) for(auto tile:layer) if(tile>=49) return invalid(__LINE__);
        for(const auto& path:r.paths) {
            if(!path.count || path.count>48 || path.edge>3 || !point(path.exit)) return invalid(__LINE__);
            for(unsigned i=0;i<path.count;++i) if(!point(path.points[i])) return invalid(__LINE__);
        }
    }
    auto mistRoom=[](const ExploreMistForestData::Room& r) {
        if(r.count>3 || !point(r.entry) || !point(r.exit)) return false;
        for(unsigned i=0;i<r.count;++i) {
            if(!r.paths[i].count || r.paths[i].count>48) return false;
            for(unsigned j=0;j<r.paths[i].count;++j) if(!point(r.paths[i].points[j])) return false;
        }
        return true;
    };
    for(const auto& bank:ExploreMistForestData::ROOMS) for(const auto& group:bank) for(const auto& r:group) if(!mistRoom(r)) return invalid(__LINE__);
    for(const auto& bank:ExploreMistForestData::TRANSITIONS) for(const auto& group:bank) for(const auto& r:group) if(!mistRoom(r)) return invalid(__LINE__);
    auto valleyRoom=[](const ExploreValleyData::Room& r) {
        if(r.count>4) return false;
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
    ContentData::Table<ExplorePickupRules::CoinRange,6> coins{46};
    // tableForArea indexes coin ranges, so validate the exact shape first.
    if(!coins.ready()) return invalid(__LINE__);
    for(const auto& boss:ExploreBoss::CONFIGS) {
        if(!boss.level || boss.level>Game::LEVEL_MAX || !boss.experiencePercent) return invalid(__LINE__);
        for(auto id:boss.speciesIds) if(!findSpecies(id)) return invalid(__LINE__);
    }
    for(unsigned i=0;i<6;++i) {
        const auto table=ExploreEncounters::tableForArea(i);
        if(!table.entries || !table.count) return invalid(__LINE__);
        unsigned sum=0;
        for(unsigned j=0;j<table.count;++j) {
            const auto& e=table.entries[j];
            if(!findSpecies(e.speciesId) || !e.minLevel || e.minLevel>e.maxLevel || e.maxLevel>Game::LEVEL_MAX || uint8_t(e.rarity)>5) return invalid(__LINE__);
            sum+=e.weight;
        }
        if(sum!=100) return invalid(__LINE__);
        const auto& pickups=ExplorePickupRules::tableForArea(i);
        if(!pickups.entries || !pickups.count || pickups.minCoin>pickups.maxCoin) return invalid(__LINE__);
        unsigned pickupWeight=0;
        for(unsigned j=0;j<pickups.count;++j) {
            if(!pickups.entries[j].id || pickups.entries[j].id>ExplorePickupRules::HEART_SCALE || !pickups.entries[j].weight) return invalid(__LINE__);
            pickupWeight+=pickups.entries[j].weight;
        }
        if(pickupWeight>UINT16_MAX) return invalid(__LINE__);
    }
    // The market session already owns the detailed geometry/navigation validator.
    std::unique_ptr<Market::Session> probe(new(std::nothrow) Market::Session);
    return probe && probe->reset(MarketMapData::get(),1,1,0,0);
}
#else
bool validateContentCatalog() { return true; }
#endif
