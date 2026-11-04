"""Compile approved forest/cave/bridge designs into portable runtime tables.

Four bridge skeletons each retain four validated detail recipes. The expedition
seed selects a recipe once for the whole journey; revisits never reroll a room.
All source art is original material, including the imported user reference.
"""
import copy
import json
from pathlib import Path

from ancient_valley_layouts import DATA as CAVES, TILE_SOURCES
from valley_bridge_layouts import generate as bridge_design

ROOT = Path(__file__).resolve().parents[1]
FOREST = json.loads((Path(__file__).with_name('valley_forest_design.json')).read_text())
ORDER = {'A': 'SNE', 'B': 'SNE', 'C': 'WNS', 'D': 'WN', 'E': 'SE', 'F': 'SW'}
SIDE = {'N': 0, 'E': 1, 'S': 2, 'W': 3}
ASSETS = []
_asset_ids = {}


def asset(spec, depth=0, flags=0):
    key = json.dumps([spec, depth, flags], sort_keys=True)
    if key not in _asset_ids:
        _asset_ids[key] = 9000 + len(ASSETS)
        frames = 8 if 'quarters' in spec else 1
        ASSETS.append(dict(spec=spec, depth=depth, flags=flags, frames=frames))
    return _asset_ids[key]


def native(sheet, source):
    depth, flags = 0, 0
    if sheet == 'Caves.png':
        if source == 610 or 657 <= source <= 675:
            flags = 2
        if 504 <= source <= 532 or source in (588, 590):
            depth = 2 if source == 524 else 1
        if source in (557, 558, 559, 566):
            depth, flags = 1, 1
        if source in (542, 543, 550, 551):
            depth = 2 if source in (542, 543) else 1
    return asset(dict(sheet=sheet, tile=source), depth, flags)


def mask(cells):
    cells = set(map(tuple, cells))
    return [sum(1 << x for x in range(16) if (x, y) in cells) for y in range(12)]


def portal(point, edge, target, entrance, warp=True):
    return dict(point=list(point), edge=edge, target=target, entrance=entrance, warp=warp)


def forest_room(seed, key):
    src = FOREST['seeds'][str(seed)][key]
    layers = [[0] * 192 for _ in range(3)]
    for tile, x, y, kind in src['placements']:
        if not (0 <= x < 16 and 0 <= y < 12) or kind == 'tree':
            continue
        z = 1 if kind in ('cave-mouth', 'foothill-rock', 'decoration') else 0
        tid = tile if (385 <= tile <= 390 or tile == 415 or
                       tile in (537, 538, 539, 540, 542, 545, 546, 547, 553, 554, 555, 556, 558)) else native('Outside.png', tile)
        if z == 1 and kind != 'cave-mouth':
            tid = asset(dict(sheet='Outside.png', tile=tile), 1, 1)
        layers[z][y * 16 + x] = tid
    parts = {}
    for obj in sorted(src['tree_objects'], key=lambda o: o['top'] + o['height']):
        for tile, dx, dy in obj['parts']:
            x, y = obj['left'] + dx, obj['top'] + dy
            if not (0 <= x < 16 and 0 <= y < 12):
                continue
            cell = parts.setdefault((x, y), dict(tiles=[], ground=0, root=False))
            cell['tiles'].append(tile)
            cell['ground'] = max(cell['ground'], obj['top'] + obj['height'])
            cell['root'] |= dy == obj['height'] - 1
    for (x, y), p in sorted(parts.items()):
        assert layers[1][y * 16 + x] == 0, (key, 'tree overlaps decoration')
        layers[1][y * 16 + x] = asset(dict(sheet='Outside.png', parts=p['tiles']),
                                     p['ground'] - y, 32 | (0 if p['root'] else 1))
    ports = []
    for side in ORDER[key]:
        destination, lane = src['ports'][side]
        point = (lane, 0) if side == 'N' else (15, lane) if side == 'E' else (lane, 11) if side == 'S' else (0, lane)
        if destination == '入口':
            target, entrance = 255, 0
        else:
            target = ord(destination) - ord('A')
            opposite = 'SWNE'['NESW'.index(side)]
            entrance = ORDER[destination].index(opposite)
        ports.append(portal(point, SIDE[side], target, entrance, False))
    if src['cave']:
        ports.append(portal(src['cave'], 0, 6 if key == 'E' else 10, 0))
    return dict(layers=layers, roads=[mask(src['road'])] * 2, stairs=[0] * 12,
                portals=ports, forest=True)


def cave_room(node):
    index = {6: 2, 7: 3, 8: 1, 9: 0}[node]
    src = CAVES['layouts'][index]
    links = {6: [(4, 2), (9, 2), (7, 0)], 7: [(6, 2), (8, 2), (9, 0)],
             8: [(9, 1), (255, 0), (7, 1)], 9: [(7, 2), (8, 0), (6, 1)]}[node]
    points = [(src['entry']['point'], src['entry']['edge'])]
    points += [(r['points'][-1], r['edge']) for r in src['routes']]
    roads = {tuple(p) for r in src['routes'] for p in r['points']}
    return dict(layers=src['layers'], roads=[mask(roads)] * 2,
                stairs=mask(src['stairs']), forest=False,
                portals=[portal(p, edge, *link) for (p, edge), link in zip(points, links)])


def bridge_room(data, key):
    src = data['rooms'][key]
    layers = copy.deepcopy(src['layers'])
    for layer in layers:
        for i, tile in enumerate(layer):
            if tile == 96:
                layer[i] = 5013  # Original Sea without shore variant 0, already packed.
            elif 6000 <= tile < 8000:
                source = tile - 6000
                # Existing runtime aliases retain their established semantics.
                existing = next((t for t, s in TILE_SOURCES.items() if s == ('Caves.png', source)), None)
                layer[i] = existing if existing is not None else native('Caves.png', source)
            elif 8000 <= tile < 8256:
                quarters = src['water_quarters'][tile]
                open_water = quarters == [(26, False), (27, False), (32, False), (33, False)]
                layer[i] = asset(dict(quarters=quarters), flags=4 | (8 if open_water else 0))
            elif tile >= 10000:
                layer[i] = native('Outside.png', tile - 10000)
    for x, y in src['bridge']:
        layers[2][y * 16 + x] = asset(dict(bridge=True), flags=16)
    if key == 'A':
        links = [(5, 2), (11, 0), (11, 1), (12, 0)]
        roads = [mask(p['cells']) for p in src['paths']]
    elif key == 'B':
        links = [(10, 1), (13, 2)]
        roads = [mask(p for path in src['paths'] for p in path['cells'])] * 2
    else:
        links = [(13, 3), (255, 0)]
        roads = [mask(p for path in src['paths'] for p in path['cells'])] * 2
    return dict(layers=layers, roads=roads, stairs=mask(src['stairs']), forest=False,
                portals=[portal(p['visual_anchor'], {'TOP': 0, 'RIGHT': 1, 'BOTTOM': 2, 'LEFT': 3}[p['edge']], *link)
                         for p, link in zip(src['portals'], links)])


FORESTS = [[forest_room(seed, key) for key in 'ABCDEF'] for seed in (38, 91)]
OLD_CAVES = [cave_room(node) for node in range(6, 10)]
BRIDGES = [[bridge_room(bridge_design(seed), key) for key in 'ABC']
           for group in ((6144, 6148, 6152, 6156), (105, 109, 113, 117),
                         (38, 42, 46, 50), (91, 95, 99, 103)) for seed in group]
offset = 0
FRAMES = []
for i, item in enumerate(ASSETS):
    item['offset'] = offset
    for frame in range(item['frames']):
        FRAMES.append((f'EXPLORE_REGION_{offset + frame}', i, frame))
    offset += item['frames']


def room_for(seed, node):
    if node < 6:
        return FORESTS[(seed >> 8) & 1][node]
    if node < 10:
        return OLD_CAVES[node - 6]
    family = (seed ^ (seed >> 16)) % 4
    detail = (seed >> 2) % 4
    return BRIDGES[family * 4 + detail][0 if node in (10, 13) else node - 10]


def tile_image(index, frame=0):
    from PIL import Image
    from asset_paths import essentials_dir
    from generate_explore_map import regular_tile
    return _tile_image(index, frame, Image, essentials_dir, regular_tile)


def _tile_image(index, frame, Image, essentials_dir, regular_tile):
    spec = ASSETS[index]['spec']
    graphics = essentials_dir() / 'Graphics'
    if 'quarters' in spec:
        source = Image.open(graphics / 'Autotiles/Sea.png').convert('RGBA')
        image = Image.new('RGBA', (32, 32))
        for k, (q, flip) in enumerate(spec['quarters']):
            x, y = frame % 8 * 96 + q % 6 * 16, q // 6 * 16
            part = source.crop((x, y, x + 16, y + 16))
            if flip:
                part = part.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            image.alpha_composite(part, (k % 2 * 16, k // 2 * 16))
        return image
    if spec.get('bridge'):
        return Image.open(ROOT / 'origin_asset/source_maps/meteor_falls_emerald.png').convert('RGBA').crop((304, 288, 336, 320))
    sheet = Image.open(graphics / 'Tilesets' / spec['sheet']).convert('RGBA')
    image = Image.new('RGBA', (32, 32))
    for tile in spec.get('parts', [spec.get('tile')]):
        image.alpha_composite(regular_tile(sheet, tile))
    return image


def cpp_table():
    lines = ['// Generated by tools/valley_region.py; edit the source designs.',
             '#pragma once', '#include <cstdint>', 'namespace ExploreValleyData {',
             'struct Point { uint8_t x, y; };',
             'struct Portal { Point point; uint8_t edge, target, entrance; bool warp; };',
             'struct Room { uint16_t layers[3][192]; uint16_t roads[2][12]; uint16_t stairs[12];',
             '    uint8_t count; Portal portals[4]; bool forest; };',
             'struct Tile { uint16_t offset; uint8_t frames, depth, flags; };',
             f'constexpr Tile TILES[{len(ASSETS)}] = {{']
    for a in ASSETS:
        lines.append('    {' + ','.join(str(a[k]) for k in ('offset', 'frames', 'depth', 'flags')) + '},')
    lines += ['};', 'inline constexpr const Tile* tile(uint16_t id) {',
              f'    return id >= 9000 && id < {9000 + len(ASSETS)} ? &TILES[id-9000] : nullptr;', '}',
              'inline constexpr bool flag(uint16_t id, uint8_t flag) { return tile(id) && (tile(id)->flags & flag); }']
    def emit_room(room):
        lines.append('    {{')
        for layer in room['layers']:
            lines.append('      {' + ','.join(map(str, layer)) + '},')
        lines.append('    }, {')
        for road in room['roads']:
            lines.append('      {' + ','.join(map(str, road)) + '},')
        lines.append('    }, {' + ','.join(map(str, room['stairs'])) + '}, ' + str(len(room['portals'])) + ', {')
        for p in room['portals']:
            lines.append('      {{' + ','.join(map(str, p['point'])) + '},' + ','.join(str(p[k]) for k in ('edge', 'target', 'entrance')) + ',' + str(p['warp']).lower() + '},')
        lines.append('    }, ' + str(room['forest']).lower() + '},')
    for name, groups in (('FORESTS', FORESTS), ('CAVES', [OLD_CAVES]), ('BRIDGES', BRIDGES)):
        cols = len(groups[0])
        lines.append(f'constexpr Room {name}[{len(groups)}][{cols}] = {{')
        for group in groups:
            lines.append('  {')
            for room in group:
                emit_room(room)
            lines.append('  },')
        lines.append('};')
    lines += ['inline const Room& room(uint32_t seed, uint8_t node) {',
              '    if (node < 6) return FORESTS[(seed >> 8) & 1][node];',
              '    if (node < 10) return CAVES[0][node-6];',
              '    const uint8_t choice = ((seed ^ (seed >> 16)) % 4) * 4 + ((seed >> 2) % 4);',
              '    return BRIDGES[choice][node == 10 || node == 13 ? 0 : node-10];',
              '}', '} // namespace ExploreValleyData', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    path = ROOT / 'src/game/ExploreValleyData.h'
    path.write_text(cpp_table())
    print(f'{path}: {len(ASSETS)} original tile recipes, {len(FRAMES)} frames')
