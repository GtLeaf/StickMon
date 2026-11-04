"""Original artwork and runtime aliases. No generated art or source mutations."""
from pathlib import Path
from functools import lru_cache
import hashlib
import io
import json
import math
import struct
import subprocess
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from asset_paths import essentials_dir
import generate_game_assets as ga
import generate_runtime_tile_map as runtime
from generate_explore_map import regular_tile, autotile_variant
from map_generation_rules import CUSTOM_TILE_SOURCE_FLIP_Y, CUSTOM_TILE_SOURCE_ROTATIONS
from mist_forest import ASSETS as MIST, asset_image
from valley_region import ASSETS as VALLEY, tile_image
from ancient_valley_layouts import water_frame, WATER_TILES
from map_editor_assets import TILES as EDITOR_TILES, EXTRA_TILES, asset_key as editor_asset_key
from map_editor_assets import RED_FLOWER_SOURCE, RED_FLOWER_SWAY, animate_tile
from map_editor_objects import OBJECTS as OUTDOOR_OBJECTS
from slateport_market import OBJECTS as MARKET_OBJECTS, SHEET as MARKET_SHEET, BALLOON, tile_image as market_tile, source_hash as market_hash
from slateport_market import LEGACY_OBJECTS
import lake_autotile as lake
import map_editor_rotations as rotations

LOCAL = ROOT / '.stick-local' / 'artifacts' / 'map-editor-20261026'
SHEETS = {'outside': 'Outside.png', 'caves': 'Caves.png', 'interior': 'Interior general.png'}
EDGES = ['top', 'right', 'bottom', 'left']
CATEGORY_ALIASES = {'树木': '自然景物', '草花': '自然景物', '岩石': '自然景物',
                    '交通': '集市', '围栏': '围栏设施', '洞穴': '围栏设施'}
ROAD_TILES = [(546, '路面'), (538, '上边'), (554, '下边'), (545, '左边'), (547, '右边'),
              (537, '左上外角'), (539, '右上外角'), (553, '左下外角'), (555, '右下外角'),
              (558, '左上内凹角'), (556, '右上内凹角'), (542, '左下内凹角'), (540, '右下内凹角')]


def png(image):
    out = io.BytesIO()
    image.save(out, format='PNG')
    return out.getvalue()


def native_binary():
    target = LOCAL / 'map-editor-native'
    dependencies = [Path(__file__).with_name('native.cpp'), Path(__file__).with_name('wire.h'), *list((ROOT / 'src/game').glob('Explore*.h')),
                    ROOT / 'src/game/ExploreMapGenerator.cpp']
    if not target.exists() or any(p.stat().st_mtime > target.stat().st_mtime for p in dependencies):
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['c++', '-std=c++17', '-O1', '-I', str(ROOT / 'src'),
                        str(dependencies[0]), str(ROOT / 'src/game/ExploreMapGenerator.cpp'),
                        '-o', str(target)], check=True, capture_output=True, text=True)
    return target


@lru_cache(None)
def sheet(key):
    return Image.open(essentials_dir() / 'Graphics/Tilesets' / SHEETS[key]).convert('RGBA')


@lru_cache(None)
def auto(name):
    return Image.open(essentials_dir() / 'Graphics/Autotiles' / f'{name}.png').convert('RGBA')


def matrix_prefab(key, name, category, rows, source='outside', layer=1, blocked=True):
    h, w = len(rows), max(map(len, rows))
    cells = [dict(x=x, y=y, layer=layer, asset=f'{source}:{tile}')
             for y, row in enumerate(rows) for x, tile in enumerate(row) if tile]
    return dict(id=key, name=name, category=category, width=w, height=h, cells=cells,
                anchor=[w / 2, h], footprint=[[x, h-1] for x,t in enumerate(rows[-1]) if t] if blocked else [])


def prefabs():
    result = [
        matrix_prefab('tree', '完整阔叶树', '树木', [[804,805],[812,813],[820,821]]),
        matrix_prefab('tree-wide', '完整大树', '树木', [[824,825,826],[832,833,834],[840,841,842]]),
        matrix_prefab('stall-blue', '蓝白遮棚', '集市', [[1384,1385,1386],[1392,1393,1394],[1400,0,1402]]),
        matrix_prefab('stall-yellow', '黄色遮棚', '集市', [[1387,1388,1389],[1395,1396,1397],[1403,0,1405]]),
        matrix_prefab('flower-box', '花苗长箱', '集市', [[1368,1369,1370],[1376,1377,1378]]),
        matrix_prefab('sign', '告示牌', '集市', [[1363]]),
        matrix_prefab('lamp', '弧形路灯', '集市', [[1408,1409],[1416,0],[1424,1425]]),
        matrix_prefab('table-check', '格纹长桌', '集市', [[1904,1905,1906],[1912,1913,1914]], 'interior'),
        matrix_prefab('table-stripe', '条纹长桌', '集市', [[1908,1909,1910],[1916,1917,1918]], 'interior'),
        matrix_prefab('bush', '灌木', '草花', [[859]]),
        matrix_prefab('flowers-single', '单格鲜花', '草花', [[415]], layer=0, blocked=False),
        matrix_prefab('flowers', '鲜花三格', '草花', [[415,415],[415,0]], layer=0, blocked=False),
        matrix_prefab('ladder', '向上梯子', '洞穴', [[4772],[4773]], 'runtime', blocked=False),
    ]
    result[-1]['cells'][0]['layer'] = 2
    result[-1]['anchor'] = [.5, 1.5]
    grass = matrix_prefab('tall-grass', '完整长草', '草花', [[391],[399]], blocked=False)
    grass['cells'][1]['layer'] = 0
    result.append(grass)
    for objects, source, source_sheet in [(MARKET_OBJECTS,'slateport',MARKET_SHEET),
                                         (LEGACY_OBJECTS,'slateport',MARKET_SHEET),
                                         (OUTDOOR_OBJECTS,'outside','Outside.png')]:
        for obj in objects:
            key, name, category, matrix = obj if len(obj)==4 else (obj[0],obj[1],'集市',obj[2])
            pref = matrix_prefab(key,name,category,matrix,source)
            for cell in pref['cells']:
                tile = matrix[cell['y']][cell['x']]
                cell['asset'] = editor_asset_key(source_sheet,tile,len(matrix)-cell['y'])
            if objects is LEGACY_OBJECTS: pref['hidden'] = True
            result.append(pref)
    for key, name, matrix in [
        ('lake-small','小湖泊',[[84,68,80],[64,48,72],[5001,5002,5000]]),
        ('lake-large','大湖泊',[[84,68,68,68,80],[64,48,48,48,72],
                                 [64,48,48,48,72],[5001,5002,5002,5002,5000]]),
    ]:
        result.append(matrix_prefab(key,name,'湖泊',matrix,'runtime',layer=0,blocked=False))
    for tile, name in [(48,'水面'),(68,'上岸'),(5002,'下岸'),(64,'左岸'),(72,'右岸'),
                       (84,'左上岸角'),(80,'右上岸角'),(5001,'左下岸角'),(5000,'右下岸角')]:
        result.append(matrix_prefab(f'lake-{tile}',f'湖泊 · {name}','湖泊',[[tile]],'runtime',layer=0,blocked=False))
    result += [matrix_prefab('rock-small','小岩石','岩石',[[1532]]),
               matrix_prefab('rock-large','大岩石','岩石',[[1506,1507],[1514,1515]])]
    result.extend(rotations.VARIANTS)
    for tile, name in ROAD_TILES + [(385, '草地 · 基础'), (386, '草地 · 碎草'),
                                   (387, '草地 · 稀疏'), (388, '草地 · 繁茂'), (389, '草地 · 平整')]:
        name = f'土路 · {name}' if tile >= 537 else name
        pref = matrix_prefab(f'ground-{tile}', name, '道路地面', [[tile]], layer=0, blocked=False)
        pref['brushAsset'] = f'outside:{tile}'
        result.append(pref)
    for pref in result:
        if pref['id'] == 'lamp':
            pref['footprint'] = [[0,2]]
        if pref['id'] in ('flower-box','market-deckchair-green','market-deckchair-blue'):
            pref['hidden'] = True
        pref['category'] = CATEGORY_ALIASES.get(pref['category'], pref['category'])
    return result


class Catalog:
    def __init__(self):
        self.assets, self.images, self.runtime_keys = {}, {}, {}
        self.sources = {}
        self.sources['slateport'] = dict(path='origin_asset/source_maps/slateport_emerald',sha256=market_hash())
        for key, filename in SHEETS.items():
            path = essentials_dir() / 'Graphics/Tilesets' / filename
            image = sheet(key)
            self.sources[key] = dict(path=f'Graphics/Tilesets/{filename}', sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                     width=image.width, height=image.height)
            for tile in range(384, 384 + image.width // 32 * (image.height // 32)):
                im = regular_tile(image, tile)
                colors = list(im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata())
                red = sum(1 for r,g,b,a in colors if a and r > 180 and g < 100 and b < 100)
                white = sum(1 for r,g,b,a in colors if a and min(r,g,b) > 235)
                empty = im.getbbox() is None or (red > 60 and red + white > 900)
                self.assets[f'{key}:{tile}'] = dict(key=f'{key}:{tile}', source=key, tile=tile,
                    label=f'{filename} · {tile}', rect=[(tile-384)%8*32,(tile-384)//8*32,32,32],
                    empty=empty, hasTransparency=any(a<255 for _,_,_,a in colors),
                    runtimeId=None, frames=1, depth=[0,0,0], ladder=[False]*3)
        def add(tid, im, source, frames=1, source_key=None):
            key = source_key or f'runtime:{tid}'
            if key not in self.assets:
                self.assets[key] = dict(key=key, source='runtime', tile=tid, label=f'运行时 · {tid}', empty=False)
            self.assets[key].update(runtimeId=tid, provenance=source, frames=frames,
                                    hasTransparency=im.getchannel('A').getextrema()[0]<255)
            self.images[tid] = im
            self.runtime_keys[tid] = key
        for _, tid in ga.EXPLORE_TILES + ga.TALL_GRASS_PARK_TILES + ga.CREEK_TILES:
            im = regular_tile(sheet('outside'), tid) if tid >= 384 else autotile_variant(auto(ga.OUTSIDE_AUTOTILES[tid//48-1]), tid%48)
            add(tid, im, dict(sheet='Outside.png', tile=tid), source_key=f'outside:{tid}' if tid >= 384 else None)
        for _, tid, name, source_id in ga.EXTERNAL_EXPLORE_TILES:
            key = next((k for k,v in SHEETS.items() if v == name), None)
            im = regular_tile(sheet(key) if key else Image.open(essentials_dir()/'Graphics/Tilesets'/name).convert('RGBA'), source_id)
            if tid in CUSTOM_TILE_SOURCE_FLIP_Y: im = im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            if tid in CUSTOM_TILE_SOURCE_ROTATIONS: im = im.rotate(CUSTOM_TILE_SOURCE_ROTATIONS[tid])
            add(tid, im, dict(sheet=name, tile=source_id, flipY=tid in CUSTOM_TILE_SOURCE_FLIP_Y,
                             rotation=CUSTOM_TILE_SOURCE_ROTATIONS.get(tid, 0)))
        self.animations = {}
        for _, tid, source, frame in ga.ANIMATED_EXPLORE_FRAMES:
            self.animations.setdefault(tid, {})[frame] = autotile_variant(auto(source), tid%48, frame)
        for tid, frames in self.animations.items():
            if 0 not in frames and tid in self.images: frames[0] = self.images[tid]
            if tid not in self.runtime_keys: add(tid, frames[0], dict(autotile=True), len(frames))
            self.assets[self.runtime_keys[tid]]['frames'] = len(frames)
        water_sources = {s['source']:auto(s['source']) for s in WATER_TILES.values()}
        for tid, spec in WATER_TILES.items():
            add(tid, water_frame(tid, water_sources, 0), spec, 8)
            self.animations[tid] = {f:water_frame(tid, water_sources, f) for f in range(8)}
        for i, spec in enumerate(VALLEY):
            add(9000+i, tile_image(i), spec, spec['frames'])
            if spec['frames'] > 1: self.animations[9000+i] = {f:tile_image(i,f) for f in range(spec['frames'])}
        for i, spec in enumerate(MIST): add(10000+i, asset_image(10000+i), spec)
        for _, tid, name, tile, depth in EDITOR_TILES+EXTRA_TILES:
            key = editor_asset_key(name,tile,depth)
            if name == MARKET_SHEET:
                source_key = 'slateport'
                frames = 4 if tile == BALLOON else 1
                add(tid,market_tile(tile),dict(sheet=name,tile=tile),frames,source_key=key)
                self.assets[key].update(source=source_key,tile=tile,label=f'绿宝石凯那市 · {tile}')
                if frames > 1: self.animations[tid] = {f:market_tile(tile,f) for f in range(frames)}
            else:
                source_key = next(k for k,v in SHEETS.items() if v == name)
                original = regular_tile(sheet(source_key),tile)
                frames = len(RED_FLOWER_SWAY) if (name,tile) == RED_FLOWER_SOURCE else 1
                add(tid, original, dict(sheet=name,tile=tile), frames, source_key=key)
                if frames > 1:
                    self.animations[tid] = {f:animate_tile(name,tile,original,f) for f in range(frames)}
                self.assets[key].update(source=source_key,tile=tile,label=f'{name} · {tile}')
            self.assets[key]['requiredLayer'] = 1
        for i, mask in enumerate(lake.MASKS):
            tid, key = lake.BASE+i, f'lake:{mask}'
            frames = {f:lake.tile_image(auto('Sea'),mask,f) for f in range(8)}
            add(tid,frames[0],dict(autotile='Sea',neighbours=mask),8,source_key=key)
            self.assets[key].update(label=f'自动湖泊 · {mask}',requiredLayer=0)
            self.animations[tid] = frames
        for spec in rotations.TILES:
            add(spec['id'],rotations.tile_image(spec),
                dict(sheet=spec['sheet'],tile=spec['source'],rotation=spec['angle'],
                     **({k:spec[k] for k in ('crop','cell','matrix','stackLayers') if k in spec} if 'crop' in spec else {})),source_key=spec['key'])
            self.assets[spec['key']].update(label=f'集市旋转图块 · {spec["id"]}',requiredLayer=2 if spec.get('stackLayers') and spec['cell'][1]==0 else 1)
        proc = subprocess.run([str(native_binary()), 'catalog'], input='\n'.join(map(str, self.runtime_keys)),
                              text=True, capture_output=True, check=True)
        for line in proc.stdout.splitlines():
            v = list(map(int,line.split()))
            self.assets[self.runtime_keys[v[0]]].update(road=bool(v[1]),water=bool(v[2]),forest=bool(v[3]),bridge=bool(v[4]),
                        depth=[v[5],v[8],v[11]],ladder=[bool(v[6]),bool(v[9]),bool(v[12])],behind=[bool(v[7]),bool(v[10]),bool(v[13])])
        self.prefabs = prefabs()
        self.atlas = Image.new('RGBA',(512, math.ceil(len(self.runtime_keys)/16)*32*8))
        for i,(tid,key) in enumerate(self.runtime_keys.items()):
            x,y = i%16*32, i//16*256
            self.assets[key]['atlas'] = [x,y]
            for f in range(8): self.atlas.alpha_composite(self.tile(key,f),(x,y+f*32))
        self.sources['runtime-art'] = dict(path='runtime/original-source-composites',
            sha256=hashlib.sha256(self.atlas.tobytes()).hexdigest())
        pack = ROOT/'data/packs/dev/game/maps.smonfx'
        data = pack.read_bytes()
        h = struct.unpack_from('<IHHIIIIII',data)
        self.pack_stats = dict(frames=h[2],dataWords=h[3],paletteWords=h[4],payloadBytes=h[6],packBytes=len(data),sha256=hashlib.sha256(data).hexdigest(),
                              limits=dict(frames=ga.MAX_PACK_FRAMES,dataWords=ga.MAX_PACK_DATA_WORDS,paletteWords=ga.MAX_PACK_PALETTE_WORDS,payloadBytes=ga.MAX_PACK_PAYLOAD_BYTES))
        self.version = hashlib.sha256(json.dumps([self.sources,self.assets],sort_keys=True).encode()).hexdigest()
        self.atlas_png = png(self.atlas)

    def tile(self, key, frame=0):
        asset = self.assets[key]
        tid = asset.get('runtimeId')
        if tid in self.animations:
            frames = self.animations[tid]
            return frames[frame % len(frames)]
        if tid in self.images: return self.images[tid]
        return regular_tile(sheet(asset['source']),asset['tile'])

    def payload(self):
        return dict(assets=self.assets,sources=self.sources,prefabs=self.prefabs,lake=lake.descriptor(),rotations=rotations.families(),stacks=rotations.STACK_FAMILIES,version=self.version,pack=self.pack_stats,
                    algorithmVersion=runtime.ALGORITHM_VERSION)
