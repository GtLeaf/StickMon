#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Export authoritative C++ content through the portable field codecs."""
import argparse
import hashlib
import tempfile
import json
import os
from pathlib import Path
import subprocess
import sys
from content_tables import SCHEMAS

ROOT=Path(__file__).resolve().parents[1]

def exporter_source():
    includes='\n'.join(f'#include "game/{key}.h"' for key in SCHEMAS)
    main=(ROOT/'src/scenes/MainScene.cpp').read_text()
    room=main[main.index('using PmdMotionMode='):main.index('const PmdSpriteConfig* pmdSpriteConfigForSpecies')]
    calls='\n'.join(f'    emit({ident}, {ns}::{name});' for ns,_,tables in SCHEMAS.values()
                    for name,_,_,_,ident in tables)
    source=r'''#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <fstream>
#include "game/ContentData.h"
#include "game/Species.cpp"
#include "game/ShopService.cpp"
#include "game/ExploreEcology.cpp"
#include "game/ExploreEcology.h"
#include "assets/PokemonSprites.cpp"
#include "assets/PokemonMotion.h"
#include "assets/PokemonBattleSizes.h"
#include "game/ExploreAreaCatalog.h"
#include "game/MarketMapData.h"
#include "game/ExplorePickupRules.h"
''' + includes + '\nstatic constexpr uint16_t PMD_IDLE_FRAME_MS=520;\n'+room+r'''
std::filesystem::path outputRoot;
template<class T> void emitSpan(uint16_t id,const T* values,size_t count) {
    std::vector<uint8_t> payload;
    ContentData::Writer writer{&payload,[](void* p,uint8_t b){static_cast<std::vector<uint8_t>*>(p)->push_back(b);}};
    for(size_t i=0;i<count;++i) ContentData::field(writer,const_cast<T&>(values[i]));
    if(!writer.good || !count) std::abort();
    std::vector<uint8_t> header;
    ContentData::Writer h{&header,[](void* p,uint8_t b){static_cast<std::vector<uint8_t>*>(p)->push_back(b);}};
    uint32_t magic=0x44434d53,n=uint32_t(count),size=uint32_t(payload.size()); uint16_t schema=1;
    h(magic,schema,id,n,size);
    auto path=outputRoot/(std::to_string(id)+".smondata");
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(header.data()),header.size());
    file.write(reinterpret_cast<const char*>(payload.data()),payload.size());
    if(!file.good()) std::abort();
}
template<class T,size_t N> void emit(uint16_t id,const T (&values)[N]) { emitSpan(id,values,N); }
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    outputRoot=argv[1];
    std::vector<ExplorePickupRules::CoinRange> coins;
    for(uint8_t i=0;i<6;++i) {
        const auto& t=ExplorePickupRules::tableForArea(i);
        emitSpan(40+i,t.entries,t.count); coins.push_back({t.minCoin,t.maxCoin});
    }
    emitSpan(46,coins.data(),coins.size());
    emit(1,SPECIES); emit(2,OFFICIAL_MOVE_EFFECTS); emit(3,OFFICIAL_MOVES);
    emit(4,OFFICIAL_LEARNSET_ENTRIES); emit(5,OFFICIAL_LEARNSETS);
    emit(54,TUTOR_MOVE_ENTRIES); emit(55,TUTOR_MOVE_SETS);
    emit(6,PokemonSprites::WALKING_CONFIGS);
    std::vector<PokemonSprites::PetAnimationProfile> profiles;
    for(const auto& c:PokemonSprites::WALKING_CONFIGS) {
        PokemonSprites::PetAnimationProfile p{};
        if(!PokemonSprites::petAnimationProfile(c.speciesId,p)) return 3;
        profiles.push_back(p);
    }
    emitSpan(7,profiles.data(),profiles.size());
    emit(8,PMD_SPRITE_CONFIGS);
    std::vector<SpeciesAbility> abilities;
    for(const auto& s:SPECIES) abilities.push_back({s.id,abilityForSpecies(s)});
    emitSpan(9,abilities.data(),abilities.size());
    std::vector<ItemCatalog::Entry> items;
    for(uint8_t i=0;i<uint8_t(Game::ItemId::COUNT);++i) {
        auto id=Game::ItemId(i);
        items.push_back({id,Game::ShopService::buyPrice(id),Game::ShopService::sellPrice(id),ExploreItemProgression::unlockAreaForItem(id),Game::ShopService::shortName(id),Game::ShopService::shortDescription(id)});
    }
    emitSpan(50,items.data(),items.size());
    emit(51,Game::ShopService::DAILY_ITEMS);emit(52,Game::ShopService::EXPLORE_ITEMS);emit(53,Game::ShopService::SELL_ITEMS);
    emit(29,ExploreEcology::GROUP_FAMILIES);
    std::vector<ExploreEcology::Profile> ecology;
    for(const auto& c:PokemonSprites::WALKING_CONFIGS) ecology.push_back({c.speciesId,ExploreEcology::temperamentForSpecies(c.speciesId),ExploreEcology::locomotionForSpecies(c.speciesId),ExploreEcology::wideFlightSpecies(c.speciesId),c.speciesId==129 || c.speciesId==130});
    emitSpan(28,ecology.data(),ecology.size());
    emit(25,PokemonBattleSizes::FRONT);
    std::vector<PokemonMotion::Profile> motion;
    for(const auto& c:PokemonSprites::WALKING_CONFIGS) motion.push_back({c.speciesId,PokemonMotion::modeForSpecies(c.speciesId),PokemonMotion::behaviorForSpecies(c.speciesId).registeredSlither,PokemonMotion::airProfileForSpecies(c.speciesId)});
    emitSpan(26,motion.data(),motion.size());
    std::vector<ExploreAreaCatalog::Area> areas;
    for(uint8_t i=0;i<6;++i) areas.push_back({ExploreAreaCatalog::minMapCount(i),ExploreAreaCatalog::maxMapCount(i),ExploreAreaCatalog::recommendedLevel(i),ExploreAreaCatalog::depthSpread(i),ExploreAreaCatalog::encounterChance(i),ExploreAreaCatalog::fieldColor(i),ExploreAreaCatalog::battleBackground(i)});
    emitSpan(27,areas.data(),areas.size());
    const Market::Data market[]={MarketMapData::get()}; emit(24,market);
''' + calls + r'''
    return 0;
}
'''
    return source

def generate(output):
    output=Path(output).resolve(); output.mkdir(parents=True,exist_ok=True)
    source=exporter_source()
    inputs=[*ROOT.glob('src/game/*.h'),*ROOT.glob('src/game/*.cpp'),ROOT/'src/game/OfficialMoveData.inc',ROOT/'src/game/MoveTutorData.inc',
            *ROOT.glob('src/assets/*.h'),ROOT/'src/assets/PokemonSprites.cpp',ROOT/'src/core/UiStrings.h']
    fingerprint=hashlib.sha256(source.encode())
    for p in sorted(inputs):fingerprint.update(p.read_bytes())
    scratch=ROOT/'.stick-local/scratch/sdgoods-catalog-exporter'/fingerprint.hexdigest()[:20]
    scratch.mkdir(parents=True,exist_ok=True)
    cpp=scratch/'export.cpp';binary=scratch/'export'
    if not cpp.exists():cpp.write_text(source)
    if not binary.exists():
        # Concurrent firmware builds publish only complete host executables.
        fd,temporary=tempfile.mkstemp(prefix='export-',dir=scratch);os.close(fd)
        try:
            cmd=[os.environ.get('STICKMON_HOST_CXX','/usr/bin/c++'),'-std=c++17','-O1','-ffunction-sections','-fdata-sections',
                 '-I'+str(ROOT/'src'),str(cpp),str(ROOT/'src/game/ExploreEcologyFlight.cpp'),'-o',temporary]
            cmd+=['-Wl,-dead_strip'] if sys.platform=='darwin' else ['-Wl,--gc-sections']
            subprocess.run(cmd,check=True)
            os.replace(temporary,binary)
        finally:
            if os.path.exists(temporary):os.unlink(temporary)
    subprocess.run([str(binary),str(output)],check=True)
    inventory={p.name:p.stat().st_size for p in sorted(output.glob('*.smondata'))}
    (scratch/'inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
    print(f'Content catalog: {len(inventory)} tables, {sum(inventory.values())} bytes')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,required=True)
    generate(p.parse_args().output)
