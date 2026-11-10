"""Reviewed market rotations from original pixels; append-only runtime IDs.

Freeze each source matrix here independently of editable catalog prefabs.
Angles are clockwise. Never reorder families, angles, or source cells.
"""
from functools import lru_cache
from pathlib import Path
from PIL import Image
from asset_paths import essentials_dir
from slateport_market import SHEET, tile_image as market_tile

BASE = 13000
FAMILIES = [
    ('table-check', '格纹长桌', 'Interior general.png', ((1904,1905,1906),(1912,1913,1914))),
    ('table-stripe', '条纹长桌', 'Interior general.png', ((1908,1909,1910),(1916,1917,1918))),
    ('flower-box', '花苗长箱', 'Outside.png', ((1368,1369,1370),(1376,1377,1378))),
    ('market-crate', '木货箱', SHEET, ((814,),)),
    ('market-deckchair-green', '绿条纹躺椅', SHEET, ((736,),(744,),(752,))),
    ('market-deckchair-blue', '蓝条纹躺椅', SHEET, ((737,),(745,),(753,))),
]


def rotate_cell(x, y, width, height, angle):
    for _ in range(angle // 90):
        x, y, width, height = height-1-y, x, height, width
    return x, y, width, height


TILES = []
VARIANTS = []
for family, name, sheet, rows in FAMILIES:
    h, w = len(rows), len(rows[0])
    for angle in (90,180,270):
        _, _, width, height = rotate_cell(0,0,w,h,angle)
        cells = []
        for y,row in enumerate(rows):
            for x,source in enumerate(row):
                if not source: continue
                rx,ry,_,_ = rotate_cell(x,y,w,h,angle)
                tid = BASE+len(TILES)
                key = f'rotated:{tid}'
                TILES.append(dict(kind=f'EXPLORE_ROTATED_{len(TILES)}',id=tid,key=key,
                                  sheet=sheet,source=source,angle=angle,depth=height-ry))
                cells.append(dict(x=rx,y=ry,layer=1,asset=key))
        VARIANTS.append(dict(id=f'{family}-r{angle}',name=f'{name} · {angle}°',category='集市',
                             width=width,height=height,cells=cells,anchor=[width/2,height],
                             footprint=[[c['x'],c['y']] for c in cells if c['y']==height-1],hidden=True))

# Keep all released tile IDs and pixels above unchanged. The compact flower box
# is a pixel-exact crop of the original 3x2 matrix, without its 16px margins.
LEGACY_COUNT = len(TILES)
COMPACT_FAMILY = 'flower-box-compact'
COMPACT_IDS = [COMPACT_FAMILY, *(f'{COMPACT_FAMILY}-r{a}' for a in (90,180,270))]
for angle, variant in zip((0,90,180,270), COMPACT_IDS):
    width, height = (1,2) if angle in (90,270) else (2,1)
    cells = []
    for y in range(height):
        for x in range(width):
            tid = BASE+len(TILES)
            key = f'rotated:{tid}'
            TILES.append(dict(kind=f'EXPLORE_FLOWER_BOX_{len(TILES)-LEGACY_COUNT}',id=tid,key=key,
                              sheet='Outside.png',source=1368,angle=angle,depth=height-y,
                              crop=[16,3952,80,3984],cell=[x,y]))
            cells.append(dict(x=x,y=y,layer=1,asset=key))
    VARIANTS.append(dict(id=variant,name='花苗长箱'+(f' · {angle}°' if angle else ''),category='集市',
                         width=width,height=height,cells=cells,anchor=[width/2,height],
                         footprint=[[x,height-1] for x in range(width)],hidden=angle!=0))

DECKCHAIR_OFFSET = len(TILES)
COMPACT_CHAIRS = [
    ('market-deckchair-green-compact', '绿条纹躺椅', [[736],[744],[752]]),
    ('market-deckchair-blue-compact', '蓝条纹躺椅', [[737],[745],[753]]),
]
for family, name, matrix in COMPACT_CHAIRS:
    for angle in (0,90,180,270):
        width,height = (2,1) if angle in (90,270) else (1,2)
        cells=[]
        for y in range(height):
            for x in range(width):
                tid=BASE+len(TILES);key=f'rotated:{tid}'
                TILES.append(dict(kind=f'EXPLORE_DECKCHAIR_{len(TILES)-DECKCHAIR_OFFSET}',id=tid,key=key,
                                  sheet=SHEET,source=matrix[0][0],angle=angle,depth=height-y,
                                  matrix=matrix,crop=[0,16,32,80],cell=[x,y]))
                cells.append(dict(x=x,y=y,layer=1,asset=key))
        VARIANTS.append(dict(id=family+(f'-r{angle}' if angle else ''),
                             name=name+(f' · {angle}°' if angle else ''),category='集市',
                             width=width,height=height,cells=cells,anchor=[width/2,height],
                             footprint=[[x,height-1] for x in range(width)],hidden=angle!=0))


STACK_OFFSET = len(TILES)
STACK_FAMILIES = {}
for family, name, top, body in [('market-crate','木货箱',814,822),
                               ('market-label-crate','标签货箱',815,823)]:
    STACK_FAMILIES[family] = [family]
    for count in (2,3):
        variant=f'{family}-stack{count}'
        STACK_FAMILIES[family].append(variant)
        cells=[]
        for y in range(2):
            tid=BASE+len(TILES);key=f'rotated:{tid}'
            TILES.append(dict(kind=f'EXPLORE_CRATE_STACK_{len(TILES)-STACK_OFFSET}',id=tid,key=key,
                              sheet=SHEET,source=top,angle=0,depth=2-y,matrix=[[top],[body]],
                              crop=[0,0,32,64],cell=[0,y],stackLayers=count))
            cells.append(dict(x=0,y=y,layer=2 if y==0 else 1,asset=key))
        VARIANTS.append(dict(id=variant,name=f'{name} · {count} 层',category='集市',
                             width=1,height=2,cells=cells,anchor=[.5,2],
                             footprint=[[0,1]],hidden=True))


def families():
    return {**{key:[key,*(f'{key}-r{angle}' for angle in (90,180,270))] for key,*_ in FAMILIES},
            COMPACT_FAMILY:COMPACT_IDS,
            **{key:[key,*(f'{key}-r{angle}' for angle in (90,180,270))] for key,*_ in COMPACT_CHAIRS}}


@lru_cache(None)
def source_sheet(name):
    return Image.open(essentials_dir()/'Graphics/Tilesets'/name).convert('RGBA')


def tile_image(spec):
    if 'crop' in spec:
        if 'matrix' in spec:
            matrix=spec['matrix']
            image=Image.new('RGBA',(len(matrix[0])*32,len(matrix)*32))
            for y,row in enumerate(matrix):
                for x,tile in enumerate(row):image.alpha_composite(market_tile(tile),(x*32,y*32))
            if spec.get('stackLayers') == 2:
                # Two crates add one 16px face; keep the ground at y=64.
                compact=Image.new('RGBA',(32,64))
                compact.alpha_composite(image.crop((0,0,32,32)),(0,16))
                compact.alpha_composite(image.crop((0,48,32,64)),(0,48))
                image=compact
            image=image.crop(spec['crop'])
        else:
            image = source_sheet(spec['sheet']).crop(spec['crop'])
        if spec['angle']:
            image = image.transpose({90:Image.Transpose.ROTATE_270,180:Image.Transpose.ROTATE_180,
                                     270:Image.Transpose.ROTATE_90}[spec['angle']])
        x,y = spec['cell']
        return image.crop((x*32,y*32,(x+1)*32,(y+1)*32))
    if spec['sheet'] == SHEET:
        image = market_tile(spec['source'])
    else:
        sheet = source_sheet(spec['sheet'])
        i = spec['source']-384
        x,y = i%8*32,i//8*32
        image = sheet.crop((x,y,x+32,y+32))
    # Multiples of 90 use an exact pixel permutation, with no resampling.
    return image.transpose({90:Image.Transpose.ROTATE_270,
                            180:Image.Transpose.ROTATE_180,
                            270:Image.Transpose.ROTATE_90}[spec['angle']])


def cpp_table():
    lines=['#pragma once','#include <cstdint>','',
           '// Generated by tools/map_editor_rotations.py. Append-only IDs.',
           'namespace ExploreRotatedTiles {',
           'struct Tile { uint8_t depth; };',
           f'inline constexpr uint16_t BASE = {BASE};',
           f'inline constexpr uint16_t LEGACY_COUNT = {LEGACY_COUNT};',
           f'inline constexpr uint16_t DECKCHAIR_OFFSET = {DECKCHAIR_OFFSET};',
           f'inline constexpr uint16_t STACK_OFFSET = {STACK_OFFSET};',
           'inline constexpr Tile TILES[] = {']
    lines += [f'    {{{s["depth"]}}},' for s in TILES]
    lines += ['};','inline constexpr uint16_t COUNT = sizeof(TILES) / sizeof(TILES[0]);',
              'inline constexpr const Tile* tile(uint16_t id) {',
              '    return id >= BASE && id < BASE + COUNT ? &TILES[id - BASE] : nullptr;',
              '}','}  // namespace ExploreRotatedTiles','']
    return __import__('content_tables').adapt_header('\n'.join(lines), 'ExploreRotatedTiles')


if __name__ == '__main__':
    (Path(__file__).resolve().parents[1]/'src/game/ExploreRotatedTiles.h').write_text(cpp_table())
