"""Curated original-art map editor objects; append-only runtime identifiers."""
from pathlib import Path
from slateport_market import SHEET, BALLOON

# (source sheet, source tile, shared object ground-row offset).
SOURCES = []
# Keep released source keys/IDs; these pixels belong below actors, not to a base.
GROUND_DECORATIONS = {('Outside.png', 1425)}
def rows(sheet, matrix):
    height=len(matrix)
    for y,row in enumerate(matrix):
        for tile in row:
            if tile: SOURCES.append((sheet,tile,height-y))

rows('Outside.png',((1384,1385,1386),(1392,1393,1394),(1400,0,1402)))
rows('Outside.png',((1387,1388,1389),(1395,1396,1397),(1403,0,1405)))
rows('Outside.png',((1368,1369,1370),(1376,1377,1378)))
rows('Outside.png',((1363,),))
rows('Outside.png',((1408,1409),(1416,0),(1424,1425)))
rows('Interior general.png',((1904,1905,1906),(1912,1913,1914)))
rows('Interior general.png',((1908,1909,1910),(1916,1917,1918)))
# Frozen release table: never derive existing IDs from editable prefab layouts.
LEGACY_MARKET_SOURCES = [('Slateport Emerald', 524, 1),
 ('Slateport Emerald', 539, 1),
 ('Slateport Emerald', 555, 1),
 ('Slateport Emerald', 547, 1),
 ('Slateport Emerald', 531, 1),
 ('Slateport Emerald', 523, 1),
 ('Slateport Emerald', 814, 1),
 ('Slateport Emerald', 815, 1),
 ('Slateport Emerald', 870, 1),
 ('Slateport Emerald', 836, 1),
 ('Slateport Emerald', 640, 3),
 ('Slateport Emerald', 643, 3),
 ('Slateport Emerald', 526, 3),
 ('Slateport Emerald', 644, 3),
 ('Slateport Emerald', 642, 3),
 ('Slateport Emerald', 648, 2),
 ('Slateport Emerald', 651, 2),
 ('Slateport Emerald', 558, 2),
 ('Slateport Emerald', 652, 2),
 ('Slateport Emerald', 650, 2),
 ('Slateport Emerald', 611, 1),
 ('Slateport Emerald', 612, 1),
 ('Slateport Emerald', 738, 2),
 ('Slateport Emerald', 739, 2),
 ('Slateport Emerald', 754, 1),
 ('Slateport Emerald', 740, 2),
 ('Slateport Emerald', 741, 2),
 ('Slateport Emerald', 756, 1),
 ('Slateport Emerald', 742, 2),
 ('Slateport Emerald', 743, 2),
 ('Slateport Emerald', 758, 1)]
SOURCES.extend(LEGACY_MARKET_SOURCES)

# Append-only release table, independent of editable prefab matrices.
# Includes corrected umbrella depths; legacy canopies keep their old IDs.
SOURCES.extend([('Slateport Emerald', 738, 3),
 ('Slateport Emerald', 739, 3),
 ('Slateport Emerald', 746, 2),
 ('Slateport Emerald', 747, 2),
 ('Slateport Emerald', 740, 3),
 ('Slateport Emerald', 741, 3),
 ('Slateport Emerald', 748, 2),
 ('Slateport Emerald', 749, 2),
 ('Slateport Emerald', 742, 3),
 ('Slateport Emerald', 743, 3),
 ('Slateport Emerald', 750, 2),
 ('Slateport Emerald', 751, 2),
 ('Slateport Emerald', 736, 3),
 ('Slateport Emerald', 744, 2),
 ('Slateport Emerald', 752, 1),
 ('Slateport Emerald', 737, 3),
 ('Slateport Emerald', 745, 2),
 ('Slateport Emerald', 753, 1),
 ('Outside.png', 868, 4),
 ('Outside.png', 872, 3),
 ('Outside.png', 873, 3),
 ('Outside.png', 874, 3),
 ('Outside.png', 880, 2),
 ('Outside.png', 881, 2),
 ('Outside.png', 882, 2),
 ('Outside.png', 888, 1),
 ('Outside.png', 889, 1),
 ('Outside.png', 890, 1),
 ('Outside.png', 854, 3),
 ('Outside.png', 862, 2),
 ('Outside.png', 870, 1),
 ('Outside.png', 878, 3),
 ('Outside.png', 886, 2),
 ('Outside.png', 894, 1),
 ('Outside.png', 863, 4),
 ('Outside.png', 871, 3),
 ('Outside.png', 887, 2),
 ('Outside.png', 895, 1),
 ('Outside.png', 916, 4),
 ('Outside.png', 920, 3),
 ('Outside.png', 921, 3),
 ('Outside.png', 922, 3),
 ('Outside.png', 928, 2),
 ('Outside.png', 929, 2),
 ('Outside.png', 930, 2),
 ('Outside.png', 936, 1),
 ('Outside.png', 937, 1),
 ('Outside.png', 938, 1),
 ('Outside.png', 947, 4),
 ('Outside.png', 948, 4),
 ('Outside.png', 949, 4),
 ('Outside.png', 955, 3),
 ('Outside.png', 956, 3),
 ('Outside.png', 957, 3),
 ('Outside.png', 979, 2),
 ('Outside.png', 980, 2),
 ('Outside.png', 981, 2),
 ('Outside.png', 995, 1),
 ('Outside.png', 996, 1),
 ('Outside.png', 997, 1),
 ('Outside.png', 1464, 2),
 ('Outside.png', 1465, 2),
 ('Outside.png', 1466, 2),
 ('Outside.png', 1472, 1),
 ('Outside.png', 1473, 1),
 ('Outside.png', 1474, 1),
 ('Outside.png', 1467, 2),
 ('Outside.png', 1468, 2),
 ('Outside.png', 1469, 2),
 ('Outside.png', 1475, 1),
 ('Outside.png', 1476, 1),
 ('Outside.png', 1477, 1),
 ('Outside.png', 907, 1),
 ('Outside.png', 990, 1),
 ('Outside.png', 1691, 1),
 ('Outside.png', 1681, 1),
 ('Outside.png', 1665, 1),
 ('Outside.png', 1662, 1),
 ('Outside.png', 1663, 1),
 ('Outside.png', 1678, 1),
 ('Outside.png', 1679, 1),
 ('Outside.png', 1680, 1),
 ('Outside.png', 1682, 1),
 ('Outside.png', 1657, 1),
 ('Outside.png', 1673, 1)])

EXTRA_SOURCES = [('Outside.png',1340,1),('Outside.png',1381,1),
                 ('Outside.png',1410,3),('Outside.png',1411,3),
                 ('Outside.png',1419,2),('Outside.png',1427,1)]

def asset_key(sheet, tile, depth):
    prefix = {"Outside.png":"outside", "Interior general.png":"interior", SHEET:"slateport"}[sheet]
    first = next(entry for entry in SOURCES+EXTRA_SOURCES if entry[:2] == (sheet,tile))
    # Keep source identity for the four fences already used by generated maps.
    native_fence = sheet == "Outside.png" and tile in (1662,1665,1681,1682)
    suffix = f":d{depth}" if first[2] != depth or native_fence else ""
    return f"{prefix}:{tile}{suffix}"

TILES = []
FRAMES = []
for i, (sheet, tile, depth) in enumerate(SOURCES):
    count = 4 if sheet == SHEET and tile == BALLOON else 1
    kind = f'EXPLORE_EDITOR_{len(FRAMES)}'
    TILES.append((kind,11000+i,sheet,tile,depth))
    for frame in range(count):
        FRAMES.append((f'EXPLORE_EDITOR_{len(FRAMES)}',11000+i,sheet,tile,depth,frame))


EXTRA_TILES = [(f'EXPLORE_MARKET_EXTRA_{i}',14000+i,sheet,tile,depth) for i,(sheet,tile,depth) in enumerate(EXTRA_SOURCES)]
EXTRA_FRAMES = [(*v,0) for v in EXTRA_TILES]

# Keep the six released frames and runtime IDs intact. The red flowers reuse
# the original pixels in three poses, with two holds per side in an 8-tick loop.
RED_FLOWER_SOURCE = ('Outside.png', 1340)
RED_FLOWER_SWAY = (0, 1, 1, 0, 0, -1, -1, 0)
RED_FLOWER_FRAME_BASE = len(EXTRA_FRAMES)
EXTRA_FRAMES.extend((f'EXPLORE_MARKET_EXTRA_{RED_FLOWER_FRAME_BASE+f}',
                     14000, *RED_FLOWER_SOURCE, 1, f)
                    for f in range(len(RED_FLOWER_SWAY)))


def animate_tile(sheet, tile, image, frame):
    """Bend the original flower pixels above their fixed roots, within one cell."""
    if (sheet, tile) != RED_FLOWER_SOURCE:
        return image
    direction = RED_FLOWER_SWAY[frame % len(RED_FLOWER_SWAY)]
    if not direction:
        return image.copy()
    from PIL import Image
    out = Image.new('RGBA', image.size)
    for y in range(image.height):
        dx = direction * (2 if y < 14 else 1 if y < 22 else 0)
        out.paste(image.crop((0, y, image.width, y+1)), (dx, y))
    return out

def cpp_table():
    rows=['#pragma once','#include <cstdint>','', '// Generated by tools/map_editor_assets.py; edit its source table.',
          'namespace ExploreEditorTiles {','struct Tile { uint16_t source; uint8_t depth; uint16_t frame; uint8_t frames; };',
          'inline constexpr uint16_t BASE = 11000;','inline constexpr Tile TILES[] = {']
    rows += [f'    {{{tile}, {0 if (sheet,tile) in GROUND_DECORATIONS else depth}, {int(kind.rsplit("_",1)[1])}, {4 if sheet == SHEET and tile == BALLOON else 1}}},'
             for kind,_,sheet,tile,depth in TILES]
    rows += ['};','inline constexpr uint16_t EXTRA_BASE = 14000;', 'inline constexpr Tile EXTRA_TILES[] = {']
    rows += [f'    {{{tile}, {depth}, {RED_FLOWER_FRAME_BASE if (sheet,tile)==RED_FLOWER_SOURCE else i}, {len(RED_FLOWER_SWAY) if (sheet,tile)==RED_FLOWER_SOURCE else 1}}},'
             for i,(_,_,sheet,tile,depth) in enumerate(EXTRA_TILES)]
    rows += ['};','inline constexpr uint16_t EXTRA_COUNT = sizeof(EXTRA_TILES) / sizeof(EXTRA_TILES[0]);']
    rows += ['inline constexpr uint16_t COUNT = sizeof(TILES) / sizeof(TILES[0]);',
             f'inline constexpr uint16_t FRAME_COUNT = {len(FRAMES)};',
             'inline constexpr const Tile* tile(uint16_t id) {',
             '    if (id >= EXTRA_BASE && id < EXTRA_BASE + EXTRA_COUNT) return &EXTRA_TILES[id-EXTRA_BASE];',
             '    return id >= BASE && id < BASE + COUNT ? &TILES[id - BASE] : nullptr;',
             '}','}  // namespace ExploreEditorTiles','']
    return '\n'.join(rows)

if __name__=='__main__':
    target=Path(__file__).resolve().parents[1]/'src/game/ExploreEditorTiles.h'
    target.write_text(cpp_table())
    print(f'{len(TILES)} editor tiles: {target}')
