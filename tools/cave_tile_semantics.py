#!/usr/bin/env python3

SMALL_ISLAND_TILES = (
    (552, 553, 554),
    (560, 561, 562),
    (568, 569, 570),
)
SMALL_ISLAND_CENTER_TILE = 561
SMALL_ISLAND_EDGE_TILES = frozenset(
    tile_id
    for row in SMALL_ISLAND_TILES
    for tile_id in row
    if tile_id != SMALL_ISLAND_CENTER_TILE
)

CLIFF_RIGHT_TO_DOWN_CORNER_TILE = 571
CLIFF_LEFT_TO_DOWN_CORNER_TILE = 572

CAVE_ENTRANCE_TILES = {
    "left": (515,),
    "right": (517,),
    "front": (524, 532),
    "back": (526,),
}

ROCK_STEP_TILE = 539
UP_LADDER_TILES = (540, 548)
DOWN_LADDER_TILE = 541
DOWN_LADDER_BASE_TILES = (
    (601, 602, 603),
    (609, 610, 611),
    (617, 618, 619),
)
DOWN_LADDER_CENTER_TILE = 610
DOWN_LADDER_ROCK_EDGE_TOP_TILES = (601, 602, 603)
DOWN_LADDER_OPEN_TOP_SOURCE_TILES = (617, 618, 619)
DOWN_LADDER_OPEN_TOP_FLIP_Y = True

EDGE_TRACE_TILES = (579, 580, 588, 596, 597, 598, 590, 582, 583)
EDGE_TRACE_TURN_TILES = {
    "left_to_down": 580,
    "down_to_right": 596,
    "right_to_up": 598,
    "up_to_right": 582,
}

FROST_CAVE_EXIT_TILES = (1299, 1300, 1301)
FROST_BLOCKED_SNOW_MASS_TILES = (
    (1296, 1297, 1298),
    (1304, 1305, 1306),
    (1312, 1313, 1314),
)
FROST_INNER_WALL_CORNER_TILES = {
    "outside_nw": 1314,
    "outside_ne": 1312,
    "outside_sw": 1298,
    "outside_se": 1296,
}
CRACKED_ICE_TILE = 1321
BROKEN_ICE_HOLE_TILE = 1322
ROUND_WATER_BOTTOM_TILES = (1326, 1327)
DOWNWARD_STAIRS_TILE = 1333
CAVE_HOLE_TILE = 1341

ICE_CAVE_EDGE_TRACE_TILES = (
    1344, 1345, 1353, 1361, 1362, 1363, 1355, 1347, 1348,
)
ICE_CAVE_EDGE_TRACE_TURN_TILES = {
    "left_to_down": 1345,
    "down_to_right": 1361,
    "right_to_up": 1363,
    "up_to_right": 1347,
}

ICE_ROCK_ISLAND_TRANSPARENT_TILES = (
    (1120, 1121, 1122),
    (1128, 1129, 1130),
    (1136, 1137, 1138),
)
ICE_ROCK_ISLAND_BACKGROUND_TILES = (
    (1123, 1124, 1125),
    (1131, 1132, 1133),
    (1139, 1140, 1141),
)

# Runtime IDs are deliberately outside the regular Outside.png range. The
# firmware renders generated maps from one shared atlas, so raw Caves.png IDs
# such as 515 and 552 would otherwise resolve to unrelated Outside.png tiles.
CAVE_RUNTIME_TILE_SOURCES = (
    (4700, 515),   # entrance facing left
    (4701, 517),   # entrance facing right
    (4702, 524),   # entrance facing screen, upper half
    (4703, 532),   # entrance facing screen, lower half
    (4704, 526),   # entrance facing away from screen
    (4705, 539),   # rock steps
    (4706, 540),   # upward ladder, upper half
    (4707, 548),   # upward ladder, lower half
    (4708, 541),   # downward ladder overlay
    (4709, 552), (4710, 553), (4711, 554),
    (4712, 560), (4713, 561), (4714, 562),
    (4715, 568), (4716, 569), (4717, 570),
    (4718, 571), (4719, 572),
    (4720, 579), (4721, 580), (4722, 588),
    (4723, 596), (4724, 597), (4725, 598),
    (4726, 590), (4727, 582), (4728, 583),
    (4729, 601), (4730, 602), (4731, 603),
    (4732, 609), (4733, 610), (4734, 611),
    (4735, 617), (4736, 618), (4737, 619),
    (4738, 617), (4739, 618), (4740, 619),  # Y-flipped open top
    (4741, 1299), (4742, 1300), (4743, 1301),
    (4744, 1322),
    (4745, 1326), (4746, 1327),
    (4747, 1333),
    (4748, 1341),
    (4749, 1344), (4750, 1345), (4751, 1353),
    (4752, 1361), (4753, 1362), (4754, 1363),
    (4755, 1355), (4756, 1347), (4757, 1348),
    (4758, 513),  # Cave floor / rock plateau center
    (4759, 576),  # Cave cliff left edge
    (4760, 577),  # Cave cliff middle
    (4761, 578),  # Cave cliff right edge
)
CAVE_RUNTIME_FLIP_Y_IDS = frozenset((4738, 4739, 4740))

CAVE_ENTRANCE_RUNTIME_TILES = {
    "left": (4700,),
    "right": (4701,),
    "front": (4702, 4703),
    "back": (4704,),
}
CAVE_ROCK_STEP_RUNTIME_TILE = 4705
CAVE_UP_LADDER_RUNTIME_TILES = (4706, 4707)
CAVE_DOWN_LADDER_RUNTIME_TILE = 4708
CAVE_ROCK_ISLAND_RUNTIME_TILES = (
    (4709, 4710, 4711),
    (4712, 4713, 4714),
    (4715, 4716, 4717),
)
CAVE_CLIFF_TURN_RUNTIME_TILES = (4718, 4719)
CAVE_EDGE_TRACE_RUNTIME_TILES = (
    4720, 4721, 4722, 4723, 4724, 4725, 4726, 4727, 4728,
)
CAVE_DOWN_LADDER_ROCK_BASE_RUNTIME_TILES = (
    (4729, 4730, 4731),
    (4732, 4733, 4734),
    (4735, 4736, 4737),
)
CAVE_DOWN_LADDER_OPEN_BASE_RUNTIME_TILES = (
    (4738, 4739, 4740),
    (4732, 4733, 4734),
    (4735, 4736, 4737),
)

FROST_CAVE_EXIT_RUNTIME_TILES = (4741, 4742, 4743)
FROST_BROKEN_ICE_HOLE_RUNTIME_TILE = 4744
FROST_ROUND_WATER_BOTTOM_RUNTIME_TILES = (4745, 4746)
FROST_DOWNWARD_STAIRS_RUNTIME_TILE = 4747
FROST_CAVE_HOLE_RUNTIME_TILE = 4748
FROST_EDGE_TRACE_RUNTIME_TILES = (
    4749, 4750, 4751, 4752, 4753, 4754, 4755, 4756, 4757,
)

CAVE_FLOOR_RUNTIME_TILE = 4758
CAVE_CLIFF_RUNTIME_TILES = (4759, 4760, 4761)

assert tuple(runtime_id for runtime_id, _source_id in CAVE_RUNTIME_TILE_SOURCES) == tuple(
    range(4700, 4762)
)
