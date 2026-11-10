#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Stable content table IDs and generated-header adapters for external targets.

Generators call adapt_header before writing their output. Other targets retain
the exact static tables; SDGOODS decodes the same explicit wire fields instead.
"""
import re

# key: namespace, type field lists, (table, type, outer capacity, inner dims, ID)
SCHEMAS = {
 'ExploreGrassPathData': ('ExploreGrassPathData', {
  'Point':'x,y', 'Route':'exit,edge,count,points', 'Room':'layers,entry,junction,paths'},
  [('TILES','uint16_t',49,'',10),('ROOMS','Room',96,'',11)]),
 'ExploreMistForestData': ('ExploreMistForestData', {
  'Point':'x,y','Route':'count,points','Room':'layers,entry,exit,count,paths','Tile':'frame,depth,flags'},
  [('TILES','Tile',141,'',12),('ROOMS','Room',2,'[7][4]',13),('TRANSITIONS','Room',2,'[2][4]',14)]),
 'ExploreValleyData': ('ExploreValleyData', {
  'Point':'x,y','Portal':'point,edge,target,entrance,warp','Room':'layers,roads,stairs,count,portals,forest',
  'Tile':'offset,frames,depth,flags'},
  [('TILES','Tile',132,'',15),('FORESTS','Room',2,'[6]',16),('CAVES','Room',1,'[4]',17),('BRIDGES','Room',16,'[3]',18)]),
 'ExploreFrostLayouts': ('ExploreFrostLayouts', {'Layout':'floorRows,iceRect,junction,controls,controlCounts'},
  [('LAYOUTS','Layout',384,'',19)]),
 'ExploreAncientValleyLayouts': ('ExploreAncientValley', {'Route':'count,controls,exit',
  'Layout':'entry,junction,routes,layers,water,solid,stairs'}, [('LAYOUTS','Layout',4,'',20)]),
 'ExploreEditorTiles': ('ExploreEditorTiles', {'Tile':'source,depth,frame,frames'},
  [('TILES','Tile',156,'',21),('EXTRA_TILES','Tile',6,'',22)]),
 'ExploreRotatedTiles': ('ExploreRotatedTiles', {'Tile':'depth'}, [('TILES','Tile',107,'',23)]),
 'ExploreEncounters': ('ExploreEncounters', {'Entry':'speciesId,weight,minLevel,maxLevel,rarity'},
  [(n,'Entry',c,'',30+i) for i,(n,c) in enumerate(zip(
   ('GRASS_PATH','CREEK_SLOPE','TALL_GRASS_PARK','FROST_CRYSTAL_CAVE','MIST_FOREST_PATH','ANCIENT_WATERFALL_VALLEY'),(10,15,19,13,13,14)))]),
 'ExploreBoss': ('ExploreBoss', {'Config':'speciesIds,level,experiencePercent'}, [('CONFIGS','Config',6,'',36)]),
}

def fields_code(types):
    return '\n'.join(f'template<class V> inline void contentFields(V& v, {typ}& x) {{ v('+','.join('x.'+s for s in fields.split(','))+'); }'
                     for typ,fields in types.items())+'\n'

def adapt_header(text, key):
    if 'CONTENT_TABLES_ADAPTED' in text: return text
    namespace, types, tables = SCHEMAS[key]
    text=text.replace('#pragma once','#pragma once\n// CONTENT_TABLES_ADAPTED\n#include "game/ContentData.h"',1)
    # Insert codecs after the last record definition, before its first table.
    first=min(text.index(re.search(r'(?m)^(?:inline )?(?:static )?constexpr '+re.escape(typ)+r' '+name+r'\[',text).group())
              for name,typ,_,_,_ in tables)
    text=text[:first]+fields_code(types)+text[first:]
    for name,typ,limit,dims,ident in tables:
        pattern=r'(?m)^(?:inline )?(?:static )?constexpr '+re.escape(typ)+r' '+name+r'\[[^=]+?=\s*\{'
        m=re.search(pattern,text)
        if not m: raise ValueError((key,name))
        start=m.start(); pos=m.end(); depth=1
        while depth:
            if text[pos]=='{':depth+=1
            elif text[pos]=='}':depth-=1
            pos+=1
        end=text.index(';',pos)+1
        original=text[start:end]
        alias=f'{name}_ContentItem'
        external=f'using {alias} = {typ}{dims};\ninline const ContentData::Table<{alias},{limit}> {name}{{{ident}}};'
        text=text[:start]+f'#if STICKMON_EXTERNAL_CONTENT\n{external}\n#else\n{original}\n#endif'+text[end:]
        text=text.replace(f'sizeof({name}) / sizeof({name}[0])',str(limit))
    # Functions which access a table cease to be constexpr in the external build.
    text=re.sub(r'inline constexpr (?=[^;\n]+\()', 'inline STICKMON_CONTENT_CONSTEXPR ',text)
    # Static validation still runs in the producer/default build.
    def assertion(m):
        value=m.group()
        return '#if !STICKMON_EXTERNAL_CONTENT\n'+value+'\n#endif' if any(re.search(r'\b'+n+r'\b',value) for n,*_ in tables) else value
    text=re.sub(r'static_assert\([\s\S]*?;',assertion,text)
    return text

def adapt_market(text):
    if 'CONTENT_TABLES_ADAPTED' in text: return text
    start=text.index('inline const Market::Data& get()')
    end=text.rindex('} // namespace MarketMapData')
    external='inline const ContentData::Table<Market::Data,1> CONTENT{24};\ninline const Market::Data& get() { return CONTENT[0]; }\n'
    return text[:start]+'// CONTENT_TABLES_ADAPTED\n#if STICKMON_EXTERNAL_CONTENT\n'+external+'#else\n'+text[start:end]+'#endif\n'+text[end:]

def adapt_sprites(text, header=False):
    if header:
        marker='struct SpriteFrame {'
        return text.replace(marker,'struct RoomSpriteProfile {\n    uint16_t speciesId;\n    uint8_t idleFrames;\n    uint16_t idleFrameMs;\n    uint8_t walkingFrames;\n    uint8_t sleepingFrames;\n    PetMotionMode motionMode;\n    SpriteKind idleBase;\n    SpriteKind walkingBase;\n    SpriteKind sleepingBase;\n    bool mirrorRightDirections;\n    float airHeight;\n    float bobAmplitude;\n};\ntemplate<class V> inline void contentFields(V& v, RoomSpriteProfile& x) { v(x.speciesId,x.idleFrames,x.idleFrameMs,x.walkingFrames,x.sleepingFrames,x.motionMode,x.idleBase,x.walkingBase,x.sleepingBase,x.mirrorRightDirections,x.airHeight,x.bobAmplitude); }\n\n'+fields_code({'PetAnimationProfile':'speciesId,idleFrames,idleFrameMs,walkingFrames,sleepingFrames,motionMode,idleBase,walkingBase,sleepingBase,mirrorRightDirections'})+'\n'+marker)
    text=text.replace('#include "assets/PokemonSprites.h"','#include "assets/PokemonSprites.h"\n#include "game/ContentData.h"',1)
    start=text.index('static constexpr WalkingConfig WALKING_CONFIGS[]'); end=text.index('\n};',start)+3
    code=fields_code({'WalkingConfig':'speciesId,downBase,leftBase,upBase,rightBase,downLeftBase,upLeftBase,upRightBase,downRightBase,frameCount,rightFlipX,upRightFlipX,downRightFlipX'})
    text=text[:start]+code+'#if STICKMON_EXTERNAL_CONTENT\ninline const ContentData::Table<WalkingConfig,1024,false> WALKING_CONFIGS{6};\n#else\n'+text[start:end]+'\n#endif'+text[end:]
    start=text.index('    WalkingAnimation walking{};',text.index('bool petAnimationProfile'));end=text.index('\n}\n',start)
    text=text[:start]+'''#if STICKMON_EXTERNAL_CONTENT
    static const ContentData::Table<PetAnimationProfile,1024,false> profiles{7};
    for(const auto& p:profiles) if(p.speciesId==speciesId) { profile=p; return true; }
    return false;
#else
'''+text[start:end]+'\n#endif'+text[end:]
    return text

def adapt_battle_sizes(text):
    if 'ContentData::Table' in text:return text
    text=text.replace('#include <cstdint>','#include <cstdint>\n#include "game/ContentData.h"')
    start=text.index('inline constexpr Entry FRONT[]');end=text.index('\n};',start)+3
    text=text[:start]+fields_code({'Entry':'speciesId,width,height'})+'#if STICKMON_EXTERNAL_CONTENT\ninline const ContentData::Table<Entry,1024,false> FRONT{25};\n#else\n'+text[start:end]+'\n#endif'+text[end:]
    text=text.replace('unsigned low = 0, high = sizeof(FRONT) / sizeof(FRONT[0]);','''unsigned low = 0;
#if STICKMON_EXTERNAL_CONTENT
    unsigned high = FRONT.size();
#else
    unsigned high = sizeof(FRONT) / sizeof(FRONT[0]);
#endif''')
    return text
