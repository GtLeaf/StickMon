"""Compile approved mist forest layouts into shared original-art runtime tiles."""
from functools import lru_cache
from pathlib import Path
import random

from PIL import Image

from asset_paths import essentials_dir
from generate_explore_map import regular_tile
from mist_forest_layouts import design, extra_art
from valley_region import ASSETS as VALLEY_ASSETS, FRAMES as VALLEY_FRAMES, tile_image as valley_image

ROOT = Path(__file__).resolve().parents[1]
SEEDS = (38,91)
FRAME_COUNT = 75  # Keep published Kind IDs after the forest frame range stable.
ASSETS, IMAGES = [], []
_assets, _frames = {}, {}
_art = {}
LEGACY_FRAME_COUNT = len(VALLEY_FRAMES)


@lru_cache(None)
def existing_frames():
    return {valley_image(i).tobytes():a['offset'] for i,a in enumerate(VALLEY_ASSETS)}


@lru_cache(None)
def outside(tile):
    sheet = Image.open(essentials_dir()/'Graphics/Tilesets/Outside.png').convert('RGBA')
    return regular_tile(sheet,tile)


def asset(image, source, depth=0, flags=0):
    pixels = image.tobytes()
    if pixels not in _frames:
        if pixels in existing_frames():
            _frames[pixels] = existing_frames()[pixels]
        else:
            _frames[pixels] = LEGACY_FRAME_COUNT+len(IMAGES)
            IMAGES.append(image)
    frame = _frames[pixels]
    _art[frame] = image
    key = frame,depth,flags
    if key not in _assets:
        _assets[key] = 10000+len(ASSETS)
        ASSETS.append(dict(frame=frame,depth=depth,flags=flags,source=source))
    return _assets[key]


def native(tile, depth=0, flags=0):
    return asset(outside(tile),dict(sheet='Outside.png',tile=tile),depth,flags)


def line_points(controls):
    points = [tuple(controls[0])]
    for xx,yy in controls[1:]:
        x,y = points[-1]
        while (x,y) != (xx,yy):
            x += (xx>x)-(xx<x)
            y += (yy>y)-(yy<y)
            points.append((x,y))
    return points


def routes(data):
    main = line_points(data['controls'])
    result = [main]
    # Closed forest loops are selectable alternatives. Blind side trails remain
    # scenery and wildlife habitat rather than becoming unexplained map warps.
    for branch in data['branches']:
        points = line_points(branch)
        if points[0] not in main or points[-1] not in main:
            continue
        a,b = main.index(points[0]),main.index(points[-1])
        if a>b:
            a,b = b,a
            points.reverse()
        alternate = main[:a]+points+main[b+1:]
        if alternate not in result:
            result.append(alternate)
    assert all(2 <= len(p) <= 48 for p in result)
    return result


def compile_room(index, seed, edge, entry_lane=None):
    # Imported lazily so the runtime preview can use the compiled room tables.
    from generate_runtime_tile_map import road_tile
    from types import SimpleNamespace
    data = design(index,seed,edge,entry_lane)
    rng = random.Random(seed+index)
    layers = [[rng.choice((385,385,385,386,387)) for _ in range(192)], [0]*192,[0]*192]
    roads = set(data['roads'])
    extended = set(roads)
    for key in ('entry','exit'):
        x,y = data[f'{key}_point']
        side = data[f'{key}_edge']
        for i in (1,2):
            for width in (0,1):
                extended.add((x+width,y-i) if side==0 else (x+i,y+width) if side==1 else
                             (x+width,y+i) if side==2 else (x-i,y+width))
    context = SimpleNamespace(paths=[],entry=SimpleNamespace(point=(-100,-100),edge=0))
    for x,y in roads:
        layers[0][y*16+x] = road_tile(context,extended,x,y)
    for x,y in data['grass']:
        layers[0][y*16+x] = 390
    flower_kind = 'red_flowers' if index==0 else 'white_flowers'
    for x,y in data['flowers']:
        under = layers[0][y*16+x]
        image = outside(under).copy()
        image.alpha_composite(extra_art(flower_kind))
        layers[0][y*16+x] = asset(image,dict(extra=flower_kind,under=under),flags=64)
    for x,y in data['long_grass']:
        layers[1][y*16+x] = 391
        layers[0][(y+1)*16+x] = 399
    for x,y in data['bushes']:
        layers[1][y*16+x] = native(990,1,1) if (x+y)%2 else 859
    for obj in data['decorations']:
        x,y,height = obj['left'],obj['top'],obj['height']
        if obj['kind']=='stone':
            layers[1][y*16+x] = 1532
            continue
        image = extra_art(obj['kind'])
        for dy in range(height):
            piece = image.crop((0,dy*32,32,(dy+1)*32))
            layers[1][(y+dy)*16+x] = asset(piece,dict(extra=obj['kind'],row=dy),height-dy,
                                          32|(1 if dy<height-1 else 0))
    parts = {}
    for obj in sorted(data['objects'],key=lambda o:o['top']+o['height']):
        for tile,dx,dy in obj['parts']:
            x,y = obj['left']+dx,obj['top']+dy
            if not (0<=x<16 and 0<=y<12):
                continue
            cell = parts.setdefault((x,y),dict(tiles=[],ground=0,root=False))
            cell['tiles'].append(tile)
            cell['ground'] = max(cell['ground'],obj['top']+obj['height'])
            cell['root'] |= dy==obj['height']-1
    for (x,y),part in sorted(parts.items()):
        assert layers[1][y*16+x]==0, ('tree overlaps a plant or decoration',index,edge,x,y)
        image = Image.new('RGBA',(32,32))
        for tile in part['tiles']:
            image.alpha_composite(outside(tile))
        layers[1][y*16+x] = asset(image,dict(parts=part['tiles']),part['ground']-y,
                                 32|(0 if part['root'] else 1))
    return dict(layers=layers,paths=routes(data),entry=data['entry_point'],
                exit=data['exit_point'],data=data)


@lru_cache(None)
def rooms():
    return [[[compile_room(i,seed,edge) for edge in range(4)] for i in range(7)] for seed in SEEDS]


@lru_cache(None)
def transitions():
    rooms()
    # Short runs omit one deep-forest room; long runs repeat a deep-room style.
    # Refit only those incoming corridors so adjacent boundary lanes still meet.
    return [[[compile_room(i,seed,edge,5) for edge in range(4)] for i in (6,4)] for seed in SEEDS]


def compile_all():
    rooms()
    transitions()
    assert len(IMAGES) <= FRAME_COUNT, 'append new Kind IDs before extending forest art'


def stage(level, count):
    if count<=1:
        return 0
    if count<6:
        return min(6,level*6//(count-1))
    return level if level<4 else min(6,4+(level-4)*2//max(1,count-5))


def room_for(seed, edge, level=0, count=1):
    if count in (6,8) and level==5:
        return transitions()[(seed>>8)&1][count==8][int(edge)]
    return rooms()[(seed>>8)&1][stage(level,count)][int(edge)]


def tile_image(index):
    compile_all()
    # Unused published slots retain valid original art; no room references them.
    return IMAGES[index] if index < len(IMAGES) else IMAGES[0]


def asset_image(tile):
    compile_all()
    return _art[ASSETS[tile-10000]['frame']]


def cpp_table():
    groups = rooms()
    extras = transitions()
    assert len(IMAGES) <= FRAME_COUNT
    lines = ['// Generated by tools/mist_forest.py; edit mist_forest_layouts.py.',
        '#pragma once','#include <cstdint>','namespace ExploreMistForestData {',
        'struct Point { uint8_t x,y; };',
        'struct Route { uint8_t count; Point points[48]; };',
        'struct Room { uint16_t layers[3][192]; Point entry,exit; uint8_t count; Route paths[3]; };',
        'struct Tile { uint16_t frame; uint8_t depth,flags; };',
        f'constexpr uint16_t FRAME_COUNT = {FRAME_COUNT};',
        f'constexpr uint16_t LEGACY_FRAME_COUNT = {LEGACY_FRAME_COUNT};',
        f'constexpr Tile TILES[{len(ASSETS)}] = {{']
    lines += ['    {'+','.join(str(a[k]) for k in ('frame','depth','flags'))+'},' for a in ASSETS]
    lines += ['};','inline constexpr const Tile* tile(uint16_t id) {',
        f'    return id>=10000 && id<{10000+len(ASSETS)} ? &TILES[id-10000] : nullptr;',
        '}','inline constexpr bool flag(uint16_t id,uint8_t flag) { return tile(id) && (tile(id)->flags&flag); }',
        'constexpr Room ROOMS[2][7][4] = {']
    for group in groups:
        lines.append('  {')
        for oriented in group:
            lines.append('   {')
            for room in oriented:
                lines.append('    {{')
                lines += ['     {'+','.join(map(str,layer))+'},' for layer in room['layers']]
                lines.append('    }, {'+','.join(map(str,room['entry']))+'}, {'+
                             ','.join(map(str,room['exit']))+'}, '+str(len(room['paths']))+', {')
                for path in room['paths']:
                    lines.append('      {'+str(len(path))+', {'+
                        ','.join('{'+str(x)+','+str(y)+'}' for x,y in path)+'}},')
                lines.append('    }},')
            lines.append('   },')
        lines.append('  },')
    lines.append('};')
    lines.append('constexpr Room TRANSITIONS[2][2][4] = {')
    for group in extras:
        lines.append('  {')
        for oriented in group:
            lines.append('   {')
            for room in oriented:
                lines.append('    {{')
                lines += ['     {'+','.join(map(str,layer))+'},' for layer in room['layers']]
                lines.append('    }, {'+','.join(map(str,room['entry']))+'}, {'+
                             ','.join(map(str,room['exit']))+'}, '+str(len(room['paths']))+', {')
                for path in room['paths']:
                    lines.append('      {'+str(len(path))+', {'+
                        ','.join('{'+str(x)+','+str(y)+'}' for x,y in path)+'}},')
                lines.append('    }},')
            lines.append('   },')
        lines.append('  },')
    lines += ['};','inline uint8_t stage(uint8_t level,uint8_t count) {',
        '    if (count<=1) return 0;',
        '    if (count<6) return static_cast<uint8_t>(level*6/(count-1));',
        '    if (level<4) return level;',
        '    const uint8_t value = 4+(level-4)*2/(count>5 ? count-5 : 1);',
        '    return value>6 ? 6 : value;', '}',
        'inline const Room& room(uint32_t seed,uint8_t edge,uint8_t level,uint8_t count) {',
        '    if ((count==6 || count==8) && level==5) return TRANSITIONS[(seed>>8)&1][count==8][edge];',
        '    return ROOMS[(seed>>8)&1][stage(level,count)][edge];',
        '}','} // namespace ExploreMistForestData','']
    return __import__('content_tables').adapt_header('\n'.join(lines), 'ExploreMistForestData')


if __name__=='__main__':
    text = cpp_table()
    path = ROOT/'src/game/ExploreMistForestData.h'
    path.write_text(text)
    print(f'{path}: {len(ASSETS)} semantic tiles, {len(IMAGES)} distinct original-art frames')
