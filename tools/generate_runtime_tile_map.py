#!/usr/bin/env python3

import argparse
import json
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from cave_tile_semantics import (
    FROST_CAVE_EXIT_RUNTIME_TILES,
    FROST_DOWNWARD_STAIRS_RUNTIME_TILE,
)

from generate_map_rule_preview import (
    GAME_TILE,
    MAP_H,
    MAP_W,
    TILESET,
    WATERFALL_AUTOTILE_NAMES,
    load_autotiles,
    render_layer,
)
from map_generation_rules import CUSTOM_TILE_SOURCES, CUSTOM_TILE_SOURCE_FLIP_Y


ALGORITHM_VERSION = 8
MASK32 = 0xFFFFFFFF
MAX_PATH_POINTS = 48
PATH_COUNT = 2
GRASS_PATH_AREA = 0
CREEK_BRIDGE_SLOPE_AREA = 1
TALL_GRASS_PARK_AREA = 2
FROST_CRYSTAL_CAVE_AREA = 3
MIST_FOREST_PATH_AREA = 4
ANCIENT_WATERFALL_VALLEY_AREA = 5


class Edge(IntEnum):
    TOP = 0
    RIGHT = 1
    BOTTOM = 2
    LEFT = 3


EDGE_NAMES = {
    Edge.TOP: "top",
    Edge.RIGHT: "right",
    Edge.BOTTOM: "bottom",
    Edge.LEFT: "left",
}
NAME_EDGES = {name: edge for edge, name in EDGE_NAMES.items()}

# coast chance, forest chance, grass minimum, grass variance, flower groups
AREA_PROFILES = (
    (0, 20, 24, 8, 2),
    (0, 0, 16, 8, 1),
    (0, 25, 44, 10, 2),
    (0, 0, 0, 0, 0),
    (0, 100, 30, 8, 1),
    (0, 0, 18, 7, 1),
)

GRASS_TILES = (385, 385, 385, 386, 387, 388, 389)
DENSE_GRASS_TILE = 390
FLOWER_TILE = 415
SHRUB_TILE = 859
DEEP_SEA_TILE = 144
DEEP_SEA_EDGE_TILE = 168
SEA_SHORE_TILE = 72

SEA_WATER_TILE = 48
SEA_LEFT_SHORE_TILE = 64
SEA_TOP_SHORE_TILE = 68
SEA_RIGHT_SHORE_TILE = 72
SEA_TOP_RIGHT_CORNER_TILE = 80
SEA_TOP_LEFT_CORNER_TILE = 84
SEA_CORNER_UNDERLAY_TILE = 385

WATERFALL_CREST_TILES = (283, 273, 285)
WATERFALL_BODY_TOP_TILES = (322, 308, 324)
WATERFALL_BODY_MIDDLE_TILES = (304, 288, 312)
WATERFALL_BODY_LOWER_TILES = (328, 316, 326)
WATERFALL_BOTTOM_TILE = 336

STREAM_LEFT_TILE = 1096
STREAM_CENTER_TILE = 1097
STREAM_RIGHT_TILE = 1098
STREAM_TOP_OUTER_LEFT_TILE = 1088
STREAM_TOP_OUTER_RIGHT_TILE = 1090
STREAM_TOP_INNER_LEFT_TILE = 1104
STREAM_TOP_INNER_RIGHT_TILE = 1106
STREAM_BOTTOM_OUTER_LEFT_TILE = 4400
STREAM_BOTTOM_OUTER_RIGHT_TILE = 4401
STREAM_BOTTOM_INNER_LEFT_TILE = 4402
STREAM_BOTTOM_INNER_RIGHT_TILE = 4403

CLIFF_TOP_TILE = 1188
CLIFF_FACE_TILE = 1185
CLIFF_STAIR_LEFT_TILE = 1161
CLIFF_STAIR_RIGHT_TILE = 1162

BRIDGE_TOP_TILE = 1627
BRIDGE_BOTTOM_TILE = 1643
WATER_ROCK_SMALL_TILE = 1532
WATER_ROCK_LARGE_TILES = ((1506, 1507), (1514, 1515))
BOULDER_TILE = 1231

SNOW_GROUND_TILES = (4500, 4500, 4500, 4500, 4502, 4503)
SNOW_PATH_TILE = 4501
SNOW_GROUND_DECOR_TILES = (4504, 4505, 4506)
SNOW_SCENERY_TILES = (4507, 4508, 4509, 4510)

FROST_OUTSIDE_TILE = 4500
FROST_FLOOR_TILE = 4511
FROST_WALL_TILES = {
    "top_left": 4512,
    "top": 4513,
    "top_right": 4514,
    "left": 4515,
    "right": 4516,
    "bottom_left": 4517,
    "bottom": 4518,
    "bottom_right": 4519,
}
FROST_INNER_CORNER_TILES = {
    "outside_nw": 4520,
    "outside_ne": 4521,
    "outside_sw": 4522,
    "outside_se": 4523,
}
FROST_ICE_TILES = {
    "top_left": 4524,
    "top": 4525,
    "top_right": 4526,
    "left": 4527,
    "center": 4504,
    "right": 4528,
    "bottom_left": 4529,
    "bottom": 4530,
    "bottom_right": 4531,
}
SMOOTH_ICE_TILE_IDS = frozenset(FROST_ICE_TILES.values())
FROST_ROCK_HILL_TILES = (
    (4532, 4533, 4534),
    (4535, 4536, 4537),
    (4538, 4539, 4540),
)
FROST_CRYSTAL_TOP_TILE = 4541

FROST_HORIZONTAL_TEMPLATES = (
    {
        "rects": ((0, 5, 9, 11), (5, 1, 12, 8), (9, 0, 12, 3), (11, 3, 15, 7)),
        "entry": ((0, 9), Edge.LEFT),
        "exits": (((10, 0), Edge.TOP), ((15, 6), Edge.RIGHT)),
        "junction": (7, 6),
        "routes": (
            ((0, 9), (7, 9), (7, 6), (10, 6), (10, 0)),
            ((0, 9), (7, 9), (7, 6), (15, 6)),
        ),
        "ice_rects": (),
        "rock_hill": (2, 6),
        "crystals": ((6, 5, 4508), (13, 5, 4509)),
        "boulders": ((8, 10, 4542), (11, 7, 4510)),
    },
    {
        "rects": (
            (0, 1, 6, 8),
            (5, 4, 10, 7),
            (9, 2, 15, 10),
            (11, 8, 14, 11),
            (2, 0, 5, 3),
        ),
        "entry": ((0, 6), Edge.LEFT),
        "exits": (((12, 11), Edge.BOTTOM), ((3, 0), Edge.TOP)),
        "junction": (3, 6),
        "routes": (
            ((0, 6), (7, 6), (13, 6), (13, 11), (12, 11)),
            ((0, 6), (3, 6), (3, 0)),
        ),
        "ice_rects": (),
        "rock_hill": None,
        "crystals": ((7, 5, 4508), (4, 7, 4509)),
        "boulders": ((2, 5, 4542), (14, 8, 4544)),
    },
)

FROST_VERTICAL_TEMPLATES = (
    {
        "rects": ((0, 3, 15, 8), (5, 0, 11, 11)),
        "entry": ((8, 11), Edge.BOTTOM),
        "exits": (((0, 5), Edge.LEFT), ((15, 4), Edge.RIGHT)),
        "junction": (8, 4),
        "routes": (
            ((8, 11), (8, 4), (1, 4), (1, 5), (0, 5)),
            ((8, 11), (8, 4), (15, 4)),
        ),
        "ice_rects": ((6, 5, 10, 9),),
        "rock_hill": None,
        "crystals": ((2, 7, 4509), (10, 2, 4508)),
        "boulders": ((3, 6, 4543), (10, 9, 4510)),
    },
    {
        "rects": ((2, 0, 13, 4), (0, 2, 5, 10), (10, 2, 15, 10), (4, 7, 11, 11)),
        "entry": ((7, 11), Edge.BOTTOM),
        "exits": (((15, 4), Edge.RIGHT), ((0, 8), Edge.LEFT)),
        "junction": (11, 4),
        "routes": (
            ((7, 11), (7, 8), (4, 8), (4, 2), (11, 2), (11, 4), (15, 4)),
            ((7, 11), (7, 8), (4, 8), (4, 2), (11, 2), (11, 8), (0, 8)),
        ),
        "ice_rects": ((5, 1, 10, 3),),
        "rock_hill": None,
        "crystals": ((3, 2, 4509), (13, 5, 4508)),
        "boulders": ((4, 9, 4544), (11, 9, 4542)),
    },
)

TOP_EDGE_FOREST_TILE_ROWS = (
    (800, 801),
    (808, 809),
    (818, 819),
)

FENCE_TOP_LEFT = 1662
FENCE_LEFT = 1665
FENCE_TOP = 1681
FENCE_TOP_RIGHT = 1682

FOREST_SPECS = (
    (10, 5, 6, 11),
    (12, 1, 4, 7),
    (1, 5, 6, 11),
    (1, 0, 6, 6),
)

CARDINAL_DIRECTIONS = ((1, 0), (-1, 0), (0, 1), (0, -1))
FLOWER_CLUSTER_SHAPES = (
    ((0, 0), (1, 0), (0, 1)),
    ((0, 0), (1, 0), (0, 1), (1, 1)),
    ((0, 0), (0, 1), (1, 1), (0, 2)),
    ((0, 0), (0, 1), (1, 1), (0, 2), (1, 2)),
)


@dataclass(frozen=True)
class Endpoint:
    point: tuple[int, int]
    edge: Edge


@dataclass
class RoutePath:
    points: list[tuple[int, int]]
    exit: Endpoint


@dataclass
class RuntimeMap:
    seed: int
    area_index: int
    entry: Endpoint
    junction: tuple[int, int]
    paths: list[RoutePath]
    layers: list[list[int]]
    has_coast: bool
    has_forest: bool
    has_creek: bool
    has_cliff: bool
    has_waterfall: bool


@dataclass(frozen=True)
class CreekSpec:
    left: int
    width: int
    bridge_top: int


@dataclass(frozen=True)
class StreamShape:
    upstream_left: int
    upstream_width: int
    downstream_left: int
    downstream_width: int
    boundary: int


@dataclass(frozen=True)
class CliffSpec:
    top: int
    left: int
    right: int
    stair_left: int


class XorShift32:
    def __init__(self, seed):
        self.state = seed & MASK32 or 0x6D2B79F5

    def next(self):
        value = self.state
        value ^= (value << 13) & MASK32
        value &= MASK32
        value ^= value >> 17
        value &= MASK32
        value ^= (value << 5) & MASK32
        value &= MASK32
        self.state = value
        return value

    def bounded(self, bound):
        return 0 if bound == 0 else self.next() % bound


def derive_seed(expedition_seed, block_index, area_index):
    value = expedition_seed & MASK32
    value ^= (0x9E3779B9 * (block_index + 1)) & MASK32
    value ^= (0x85EBCA6B * (area_index + 1)) & MASK32
    value ^= value >> 16
    value = (value * 0x7FEB352D) & MASK32
    value ^= value >> 15
    value = (value * 0x846CA68B) & MASK32
    value ^= value >> 16
    value &= MASK32
    return value or 0x6D2B79F5


def opposite(edge):
    return Edge((int(edge) + 2) % 4)


def endpoint_for_edge(edge, junction_x, junction_y, coordinate):
    if edge == Edge.TOP:
        return Endpoint((coordinate, 0), edge)
    if edge == Edge.RIGHT:
        return Endpoint((MAP_W - 1, coordinate), edge)
    if edge == Edge.BOTTOM:
        return Endpoint((coordinate, MAP_H - 1), edge)
    if edge == Edge.LEFT:
        return Endpoint((0, coordinate), edge)
    return Endpoint((junction_x, junction_y), Edge.TOP)


def append_line(points, target_x, target_y):
    if not points:
        points.append((target_x, target_y))
        return
    x, y = points[-1]
    while x != target_x or y != target_y:
        if x < target_x:
            x += 1
        elif x > target_x:
            x -= 1
        elif y < target_y:
            y += 1
        else:
            y -= 1
        if not points or points[-1] != (x, y):
            if len(points) >= MAX_PATH_POINTS:
                raise ValueError("route exceeds MAX_PATH_POINTS")
            points.append((x, y))


def build_path(trunk, junction_x, junction_y, endpoint):
    points = list(trunk)
    if endpoint.edge == Edge.TOP:
        append_line(points, junction_x, 1)
        append_line(points, endpoint.point[0], 1)
    elif endpoint.edge == Edge.RIGHT:
        append_line(points, MAP_W - 3, junction_y)
        append_line(points, MAP_W - 3, endpoint.point[1])
    elif endpoint.edge == Edge.BOTTOM:
        append_line(points, junction_x, MAP_H - 3)
        append_line(points, endpoint.point[0], MAP_H - 3)
    else:
        append_line(points, 1, junction_y)
        append_line(points, 1, endpoint.point[1])
    append_line(points, endpoint.point[0], endpoint.point[1])
    return RoutePath(points, endpoint)


def add_road_pair(road, point, vertical):
    x, y = point
    if 0 <= x < MAP_W and 0 <= y < MAP_H:
        road.add((x, y))
    companion = (x + 1, y) if vertical else (x, y + 1)
    if 0 <= companion[0] < MAP_W and 0 <= companion[1] < MAP_H:
        road.add(companion)


def build_road(paths):
    road = set()
    for path in paths:
        for first, second in zip(path.points, path.points[1:]):
            vertical = first[0] == second[0]
            add_road_pair(road, first, vertical)
            add_road_pair(road, second, vertical)
    return road


def endpoint_edge_cells(endpoint):
    x, y = endpoint.point
    if endpoint.edge in (Edge.TOP, Edge.BOTTOM):
        return {(x, y), (x + 1, y)}
    return {(x, y), (x, y + 1)}


def validate_border_road(entry, paths, road):
    boundary_road = {
        (x, y)
        for x, y in road
        if x in (0, MAP_W - 1) or y in (0, MAP_H - 1)
    }
    allowed = endpoint_edge_cells(entry)
    for path in paths:
        allowed.update(endpoint_edge_cells(path.exit))
    if boundary_road != allowed:
        raise ValueError("road touches the map edge outside an entrance or exit")


def endpoint_connects(endpoint, x, y):
    ex, ey = endpoint.point
    if endpoint.edge == Edge.TOP:
        return y == -1 and x in (ex, ex + 1)
    if endpoint.edge == Edge.RIGHT:
        return x == MAP_W and y in (ey, ey + 1)
    if endpoint.edge == Edge.BOTTOM:
        return y == MAP_H and x in (ex, ex + 1)
    return x == -1 and y in (ey, ey + 1)


def connected_road(runtime_map, road, x, y):
    if (x, y) in road or endpoint_connects(runtime_map.entry, x, y):
        return True
    return any(endpoint_connects(path.exit, x, y) for path in runtime_map.paths)


def road_tile(runtime_map, road, x, y):
    north = connected_road(runtime_map, road, x, y - 1)
    east = connected_road(runtime_map, road, x + 1, y)
    south = connected_road(runtime_map, road, x, y + 1)
    west = connected_road(runtime_map, road, x - 1, y)
    if north and east and south and west:
        if not connected_road(runtime_map, road, x - 1, y - 1):
            return 558
        if not connected_road(runtime_map, road, x + 1, y - 1):
            return 556
        if not connected_road(runtime_map, road, x - 1, y + 1):
            return 542
        if not connected_road(runtime_map, road, x + 1, y + 1):
            return 540
        return 546
    if not north and not west:
        return 537
    if not north and not east:
        return 539
    if not south and not west:
        return 553
    if not south and not east:
        return 555
    if not north:
        return 538
    if not south:
        return 554
    if not west:
        return 545
    if not east:
        return 547
    return 546


def stamp_coast(runtime_map, water):
    for y in range(MAP_H):
        for x in range(3):
            runtime_map.layers[0][y * MAP_W + x] = DEEP_SEA_TILE
            water.add((x, y))
        runtime_map.layers[0][y * MAP_W + 3] = DEEP_SEA_EDGE_TILE
        runtime_map.layers[0][y * MAP_W + 4] = SEA_SHORE_TILE
        water.update(((3, y), (4, y)))


def stamp_sea_channel(
    runtime_map,
    left,
    top,
    width,
    bottom,
    open_left=0,
    open_width=0,
):
    water = set()
    for y in range(top, bottom):
        for column in range(width):
            x = left + column
            open_cell = open_width > 0 and open_left <= x < open_left + open_width
            if y == top and open_width > 0:
                if open_cell:
                    tile_id = SEA_WATER_TILE
                elif column == 0:
                    tile_id = SEA_TOP_LEFT_CORNER_TILE
                elif column == width - 1:
                    tile_id = SEA_TOP_RIGHT_CORNER_TILE
                else:
                    tile_id = SEA_TOP_SHORE_TILE
            elif column == 0:
                tile_id = SEA_LEFT_SHORE_TILE
            elif column == width - 1:
                tile_id = SEA_RIGHT_SHORE_TILE
            else:
                tile_id = SEA_WATER_TILE

            index = y * MAP_W + x
            if tile_id in (SEA_TOP_LEFT_CORNER_TILE, SEA_TOP_RIGHT_CORNER_TILE):
                runtime_map.layers[0][index] = SEA_CORNER_UNDERLAY_TILE
                runtime_map.layers[1][index] = tile_id
            else:
                runtime_map.layers[0][index] = tile_id
            water.add((x, y))
    return water


def stamp_waterfall(runtime_map, left, width, crest_top, body_height=3):
    water = set()

    def stamp_row(y, tile_row):
        for column in range(width):
            x = left + column
            tile_index = 0 if column == 0 else 2 if column == width - 1 else 1
            runtime_map.layers[0][y * MAP_W + x] = tile_row[tile_index]
            water.add((x, y))

    stamp_row(crest_top, WATERFALL_CREST_TILES)
    for row in range(body_height):
        tile_row = (
            WATERFALL_BODY_TOP_TILES
            if row == 0
            else WATERFALL_BODY_LOWER_TILES
            if row == body_height - 1
            else WATERFALL_BODY_MIDDLE_TILES
        )
        stamp_row(crest_top + 1 + row, tile_row)
    stamp_row(
        crest_top + body_height + 1,
        (WATERFALL_BOTTOM_TILE,) * 3,
    )
    return water


def stamp_waterfall_wall(
    runtime_map,
    top,
    bottom,
    first_gap,
    stair_left,
    second_gap=None,
):
    gaps = (first_gap,) + ((second_gap,) if second_gap else ())
    gap_cells = {
        (x, y)
        for gap_left, gap_width in gaps
        for y in range(top, bottom)
        for x in range(gap_left, gap_left + gap_width)
    }
    cliff = set()
    stairs = set()
    for y in range(top, bottom):
        for x in range(MAP_W):
            point = (x, y)
            if point in gap_cells:
                continue
            cliff.add(point)
            if x in (stair_left, stair_left + 1):
                runtime_map.layers[0][y * MAP_W + x] = (
                    CLIFF_STAIR_LEFT_TILE if x == stair_left else CLIFF_STAIR_RIGHT_TILE
                )
                runtime_map.layers[1][y * MAP_W + x] = 0
                stairs.add(point)
            else:
                runtime_map.layers[0][y * MAP_W + x] = CLIFF_TOP_TILE
                runtime_map.layers[1][y * MAP_W + x] = 0
    return cliff, stairs


def stamp_top_edge_forest_cluster(runtime_map, left, width):
    forest = set()
    for row, tile_pair in enumerate(TOP_EDGE_FOREST_TILE_ROWS):
        for column in range(width):
            x = left + column
            runtime_map.layers[0][row * MAP_W + x] = tile_pair[column % 2]
            forest.add((x, row))
    return forest


def stamp_small_water_rock(runtime_map, x, y):
    runtime_map.layers[2][y * MAP_W + x] = WATER_ROCK_SMALL_TILE


def stamp_large_water_rock(runtime_map, left, top):
    for row, tile_row in enumerate(WATER_ROCK_LARGE_TILES):
        for column, tile_id in enumerate(tile_row):
            runtime_map.layers[2][(top + row) * MAP_W + left + column] = tile_id


def stamp_boulder(runtime_map, x, y, scenery):
    runtime_map.layers[2][y * MAP_W + x] = BOULDER_TILE
    scenery.add((x, y))


def stamp_ancient_waterfall_valley(runtime_map):
    water = set()
    forest = set()
    cliff = set()
    stairs = set()
    scenery = set()

    def sea(*args, **kwargs):
        water.update(stamp_sea_channel(runtime_map, *args, **kwargs))

    def waterfall(*args, **kwargs):
        water.update(stamp_waterfall(runtime_map, *args, **kwargs))

    def wall(*args, **kwargs):
        wall_cliff, wall_stairs = stamp_waterfall_wall(runtime_map, *args, **kwargs)
        cliff.update(wall_cliff)
        stairs.update(wall_stairs)

    def top_forest(left, width):
        forest.update(stamp_top_edge_forest_cluster(runtime_map, left, width))

    edge = runtime_map.entry.edge
    if edge == Edge.BOTTOM:
        sea(4, 0, 4, 3)
        sea(3, 8, 6, MAP_H, 4, 4)
        waterfall(4, 4, 3, 3)
        wall(3, 8, (4, 4), 11)
        stamp_small_water_rock(runtime_map, 6, 9)
        top_forest(0, 4)
        top_forest(13, 2)
        stamp_boulder(runtime_map, 9, 1, scenery)
        stamp_boulder(runtime_map, 14, 11, scenery)
    elif edge == Edge.LEFT:
        sea(10, 0, 4, 4)
        sea(9, 9, 6, MAP_H, 10, 4)
        waterfall(10, 4, 4, 3)
        wall(4, 9, (10, 4), 4)
        stamp_small_water_rock(runtime_map, 11, 10)
        top_forest(6, 4)
        top_forest(14, 2)
        stamp_boulder(runtime_map, 7, 3, scenery)
        stamp_boulder(runtime_map, 8, 10, scenery)
    elif edge == Edge.TOP:
        sea(10, 0, 4, 3)
        sea(9, 8, 6, MAP_H, 10, 4)
        waterfall(10, 4, 3, 3)
        wall(3, 8, (10, 4), 5)
        stamp_small_water_rock(runtime_map, 11, 9)
        top_forest(0, 4)
        top_forest(7, 2)
        top_forest(14, 2)
        stamp_boulder(runtime_map, 2, 8, scenery)
        stamp_boulder(runtime_map, 15, 9, scenery)
    else:
        sea(2, 0, 3, 3)
        sea(7, 0, 3, 3)
        sea(1, 8, 10, MAP_H, 2, 3)
        for x in range(7, 10):
            runtime_map.layers[0][8 * MAP_W + x] = SEA_WATER_TILE
        waterfall(2, 3, 3, 3)
        waterfall(7, 3, 3, 3)
        wall(3, 8, (2, 3), 12, second_gap=(7, 3))
        stamp_large_water_rock(runtime_map, 5, 9)
        stamp_small_water_rock(runtime_map, 3, 10)
        top_forest(0, 2)
        top_forest(10, 2)
        top_forest(14, 2)
        stamp_boulder(runtime_map, 6, 1, scenery)
        stamp_boulder(runtime_map, 11, 9, scenery)

    return water, forest, cliff, stairs, scenery


def forest_footprint(spec):
    forest_x, top_y, width, bottom_y = spec
    return {
        (x, y)
        for y in range(top_y, bottom_y + 1)
        for x in range(forest_x - 1, forest_x + width)
    }


def stamp_forest(runtime_map, spec):
    forest_x, top_y, width, bottom_y = spec
    forest_right = forest_x + width - 1
    fence_right = forest_right - 1
    runtime_map.layers[1][top_y * MAP_W + forest_x - 1] = FENCE_TOP_LEFT
    for x in range(forest_x, fence_right):
        runtime_map.layers[1][top_y * MAP_W + x] = FENCE_TOP
    runtime_map.layers[1][top_y * MAP_W + fence_right] = FENCE_TOP_RIGHT
    for y in range(top_y + 1, bottom_y + 1):
        runtime_map.layers[1][y * MAP_W + forest_x - 1] = FENCE_LEFT

    for offset in range(width):
        x = forest_x + offset
        runtime_map.layers[2][top_y * MAP_W + x] = 804 if offset % 2 == 0 else 805
        runtime_map.layers[0][(top_y + 1) * MAP_W + x] = 810 if offset % 2 == 0 else 811
        for y in range(top_y + 2, bottom_y):
            row_index = y - (top_y + 2)
            if row_index % 2 == 0:
                tile_id = 802 if offset == 0 else (800 if offset % 2 == 0 else 801)
            else:
                tile_id = 810 if offset == 0 else (808 if offset % 2 == 0 else 809)
            runtime_map.layers[0][y * MAP_W + x] = tile_id
        runtime_map.layers[0][bottom_y * MAP_W + x] = 818 if offset % 2 == 0 else 819


def expanded(cells, radius=1):
    return {
        (x + dx, y + dy)
        for x, y in cells
        for dy in range(-radius, radius + 1)
        for dx in range(-radius, radius + 1)
        if 0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H
    }


def orthogonally_expanded(cells):
    return {
        (x + dx, y + dy)
        for x, y in cells
        for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1))
        if 0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H
    }


def validate_topology(paths, junction):
    if len(paths) != PATH_COUNT:
        raise ValueError("unexpected path count")
    shared_count = 0
    for first, second in zip(paths[0].points, paths[1].points):
        if first != second:
            break
        shared_count += 1
    if shared_count == 0 or paths[0].points[shared_count - 1] != junction:
        raise ValueError("logical junction does not match the visible fork")

    branch_roads = []
    for path in paths:
        if len(path.points) != len(set(path.points)):
            raise ValueError("route backtracks over itself")
        branch = path.points[shared_count - 1:]
        if len(branch) < 2:
            raise ValueError("route does not leave the junction")
        branch_roads.append(build_road([RoutePath(branch, path.exit)]))

    junction_zone = {
        (x, y)
        for y in range(junction[1] - 1, junction[1] + 2)
        for x in range(junction[0] - 1, junction[0] + 2)
    }
    first_branch = branch_roads[0] - junction_zone
    second_branch = branch_roads[1] - junction_zone
    if first_branch & second_branch:
        raise ValueError("road branches overlap after the junction")
    if orthogonally_expanded(first_branch) & second_branch:
        raise ValueError("road branches merge into an oversized corridor")


def neighbors4(point):
    x, y = point
    for dx, dy in CARDINAL_DIRECTIONS:
        neighbor = (x + dx, y + dy)
        if 0 <= neighbor[0] < MAP_W and 0 <= neighbor[1] < MAP_H:
            yield neighbor


def connected_components(cells):
    remaining = set(cells)
    components = []
    while remaining:
        start = min(remaining)
        remaining.remove(start)
        stack = [start]
        component = {start}
        while stack:
            point = stack.pop()
            for neighbor in neighbors4(point):
                if neighbor not in remaining:
                    continue
                remaining.remove(neighbor)
                component.add(neighbor)
                stack.append(neighbor)
        components.append(component)
    return sorted(components, key=lambda component: (-len(component), min(component)))


def local_openness(point, available, radius=2):
    x, y = point
    return sum(
        (x + dx, y + dy) in available
        for dy in range(-radius, radius + 1)
        for dx in range(-radius, radius + 1)
        if abs(dx) + abs(dy) <= radius
    )


def grow_compact_patch(rng, available, requested_size):
    components = connected_components(available)
    if not components:
        return set()
    component = components[0]
    reserve = 12 if len(component) >= 32 else max(4, len(component) // 4)
    target = min(requested_size, len(component) - reserve)
    if target < 6:
        return set()

    scored_seeds = []
    for point in sorted(component):
        edge_margin = min(point[0], MAP_W - 1 - point[0], point[1], MAP_H - 1 - point[1])
        score = local_openness(point, component) * 4 + edge_margin
        scored_seeds.append((score, point))
    best_seed_score = max(score for score, _point in scored_seeds)
    seed_options = [point for score, point in scored_seeds if score >= best_seed_score - 2]
    seed = seed_options[rng.bounded(len(seed_options))]

    patch = {seed}
    frontier = set(neighbors4(seed)) & component
    while len(patch) < target and frontier:
        scored_frontier = []
        for point in sorted(frontier):
            adjacent = sum(neighbor in patch for neighbor in neighbors4(point))
            distance = abs(point[0] - seed[0]) + abs(point[1] - seed[1])
            score = adjacent * 100 + local_openness(point, component, radius=1) * 4 - distance
            scored_frontier.append((score, point))
        best_score = max(score for score, _point in scored_frontier)
        options = [point for score, point in scored_frontier if score == best_score]
        chosen = options[rng.bounded(len(options))]
        patch.add(chosen)
        frontier.remove(chosen)
        frontier.update(set(neighbors4(chosen)) & component - patch)
    return patch


def minimum_distance(first, second):
    if not first or not second:
        return MAP_W + MAP_H
    return min(
        abs(ax - bx) + abs(ay - by)
        for ax, ay in first
        for bx, by in second
    )


def shrub_boundary_candidates(dense_grass, forbidden, road):
    candidates = []
    for length in range(4, 1, -1):
        for horizontal in (True, False):
            width = length if horizontal else 1
            height = 1 if horizontal else length
            for top in range(1, MAP_H - height):
                for left in range(1, MAP_W - width):
                    group = {
                        (left + offset, top) if horizontal else (left, top + offset)
                        for offset in range(length)
                    }
                    if group & forbidden:
                        continue
                    sides = ((0, -1), (0, 1)) if horizontal else ((-1, 0), (1, 0))
                    if not any(
                        all((x + dx, y + dy) in dense_grass for x, y in group)
                        for dx, dy in sides
                    ):
                        continue
                    edge_margin = min(
                        min(x, MAP_W - 1 - x, y, MAP_H - 1 - y)
                        for x, y in group
                    )
                    road_distance = minimum_distance(group, road)
                    score = length * 100 - min(road_distance, 8) * 3 + edge_margin
                    candidates.append((score, tuple(sorted(group))))
    return candidates


def stamp_boundary_shrubs(runtime_map, rng, dense_grass, road, water, forest):
    forbidden = dense_grass | water | forest | road
    candidates = shrub_boundary_candidates(dense_grass, forbidden, road)
    if not candidates:
        return set()
    best_score = max(score for score, _group in candidates)
    options = [group for score, group in candidates if score >= best_score - 3]
    shrubs = set(options[rng.bounded(len(options))])
    for x, y in shrubs:
        runtime_map.layers[1][y * MAP_W + x] = SHRUB_TILE
    return shrubs


def flower_shape_variants():
    variants = set()
    for source in FLOWER_CLUSTER_SHAPES:
        for mirror in (False, True):
            points = [(-x if mirror else x, y) for x, y in source]
            for _rotation in range(4):
                minimum_x = min(x for x, _y in points)
                minimum_y = min(y for _x, y in points)
                variants.add(tuple(sorted((x - minimum_x, y - minimum_y) for x, y in points)))
                points = [(-y, x) for x, y in points]
    return tuple(sorted(variants, key=lambda shape: (len(shape), shape)))


FLOWER_SHAPE_VARIANTS = flower_shape_variants()


def flower_cluster_candidates(dense_grass, forbidden, road):
    halo = expanded(dense_grass, radius=2) - dense_grass
    candidates = []
    for shape in FLOWER_SHAPE_VARIANTS:
        shape_width = max(x for x, _y in shape) + 1
        shape_height = max(y for _x, y in shape) + 1
        for top in range(MAP_H - shape_height + 1):
            for left in range(MAP_W - shape_width + 1):
                group = {(left + x, top + y) for x, y in shape}
                if group & forbidden or not group <= halo:
                    continue
                adjacency = sum(
                    neighbor in dense_grass
                    for point in group
                    for neighbor in neighbors4(point)
                )
                if adjacency == 0:
                    continue
                road_distance = minimum_distance(group, road)
                edge_margin = min(
                    min(x, MAP_W - 1 - x, y, MAP_H - 1 - y)
                    for x, y in group
                )
                score = adjacency * 30 + min(road_distance, 6) * 2 + edge_margin
                candidates.append((score, tuple(sorted(group))))
    return candidates


def stamp_boundary_flowers(runtime_map, rng, dense_grass, shrubs, road, water, forest, count):
    flowers = set()
    strict_forbidden = dense_grass | water | forest | expanded(road) | expanded(shrubs)
    relaxed_forbidden = dense_grass | shrubs | water | forest | road
    for _ in range(count):
        flower_clearance = expanded(flowers, radius=2)
        candidates = flower_cluster_candidates(
            dense_grass, strict_forbidden | flower_clearance, road
        )
        if not candidates:
            candidates = flower_cluster_candidates(
                dense_grass, relaxed_forbidden | flower_clearance, road
            )
        if not candidates:
            break
        best_score = max(score for score, _group in candidates)
        options = [group for score, group in candidates if score >= best_score - 4]
        group = set(options[rng.bounded(len(options))])
        flowers.update(group)
        for x, y in group:
            runtime_map.layers[0][y * MAP_W + x] = FLOWER_TILE
    return flowers


def stamp_ground_decorations(runtime_map, rng, road, water, forest, profile):
    _coast_chance, _forest_chance, grass_minimum, grass_variance, flower_groups = profile
    requested_grass = grass_minimum + rng.bounded(grass_variance + 1)
    available = {
        (x, y)
        for y in range(MAP_H)
        for x in range(MAP_W)
        if (x, y) not in road and (x, y) not in water and (x, y) not in forest
    }
    dense_grass = grow_compact_patch(rng, available, requested_grass)
    for x, y in dense_grass:
        runtime_map.layers[0][y * MAP_W + x] = DENSE_GRASS_TILE

    shrubs = stamp_boundary_shrubs(runtime_map, rng, dense_grass, road, water, forest)
    flowers = stamp_boundary_flowers(
        runtime_map,
        rng,
        dense_grass,
        shrubs,
        road,
        water,
        forest,
        flower_groups,
    )
    return shrubs, dense_grass, flowers


def stamp_snow_decorations(runtime_map, rng, road, scenery):
    ground_target = 10 + rng.bounded(7)
    ground_placed = 0
    for _attempt in range(ground_target * 16):
        if ground_placed >= ground_target:
            break
        x = rng.bounded(MAP_W)
        y = rng.bounded(MAP_H)
        if (x, y) in road or (x, y) in scenery:
            continue
        runtime_map.layers[0][y * MAP_W + x] = (
            SNOW_GROUND_DECOR_TILES[rng.bounded(len(SNOW_GROUND_DECOR_TILES))]
        )
        scenery.add((x, y))
        ground_placed += 1

    road_halo = expanded(road, 1)
    scenery_target = 6 + rng.bounded(5)
    scenery_placed = 0
    for _attempt in range(scenery_target * 20):
        if scenery_placed >= scenery_target:
            break
        x = rng.bounded(MAP_W)
        y = rng.bounded(MAP_H)
        if (x, y) in road_halo or (x, y) in scenery:
            continue
        runtime_map.layers[1][y * MAP_W + x] = (
            SNOW_SCENERY_TILES[rng.bounded(len(SNOW_SCENERY_TILES))]
        )
        scenery.add((x, y))
        scenery_placed += 1
    return scenery


def build_topology(seed, entry_edge, coast_chance):
    topology = XorShift32(seed ^ 0xA511E9B3)
    coast_roll = topology.bounded(100)
    has_coast = coast_chance > 0 and entry_edge != Edge.LEFT and coast_roll < coast_chance

    candidates = [
        edge for edge in Edge
        if edge != entry_edge and not (has_coast and edge == Edge.LEFT)
    ]
    for count in range(len(candidates), 1, -1):
        swap_index = topology.bounded(count)
        candidates[count - 1], candidates[swap_index] = (
            candidates[swap_index], candidates[count - 1]
        )
    if len(candidates) < PATH_COUNT:
        raise ValueError("not enough exit edges")

    minimum_x = 7 if has_coast else 4
    junction_x = minimum_x + topology.bounded(11 - minimum_x)
    junction_y = 4 + topology.bounded(3)
    entry_coordinate = junction_x if entry_edge in (Edge.TOP, Edge.BOTTOM) else junction_y
    entry = endpoint_for_edge(entry_edge, junction_x, junction_y, entry_coordinate)
    trunk = [entry.point]
    append_line(trunk, junction_x, junction_y)

    paths = []
    for edge in candidates[:PATH_COUNT]:
        if edge in (Edge.TOP, Edge.BOTTOM):
            minimum_exit_x = 7 if has_coast else 4
            coordinate = minimum_exit_x + topology.bounded(11 - minimum_exit_x)
        else:
            coordinate = 4 + topology.bounded(3)
        endpoint = endpoint_for_edge(edge, junction_x, junction_y, coordinate)
        paths.append(build_path(trunk, junction_x, junction_y, endpoint))
    junction = (junction_x, junction_y)
    validate_topology(paths, junction)
    road = build_road(paths)
    validate_border_road(entry, paths, road)
    return entry, junction, paths, has_coast, road


def build_ancient_waterfall_topology(entry_edge):
    layouts = {
        Edge.BOTTOM: (
            (11, MAP_H - 1),
            (11, 9),
            ((Edge.RIGHT, (MAP_W - 1, 9)), (Edge.TOP, (11, 0))),
        ),
        Edge.LEFT: (
            (0, 2),
            (4, 2),
            ((Edge.BOTTOM, (4, MAP_H - 1)), (Edge.TOP, (4, 0))),
        ),
        Edge.TOP: (
            (5, 0),
            (5, 9),
            ((Edge.LEFT, (0, 9)), (Edge.BOTTOM, (5, MAP_H - 1))),
        ),
        Edge.RIGHT: (
            (MAP_W - 1, 9),
            (12, 9),
            ((Edge.TOP, (12, 0)), (Edge.BOTTOM, (12, MAP_H - 1))),
        ),
    }
    entry_point, junction, exit_specs = layouts[entry_edge]
    entry = Endpoint(entry_point, entry_edge)
    trunk = [entry.point]
    append_line(trunk, *junction)
    paths = []
    for exit_edge, exit_point in exit_specs:
        points = list(trunk)
        append_line(points, *exit_point)
        paths.append(RoutePath(points, Endpoint(exit_point, exit_edge)))
    validate_topology(paths, junction)
    road = build_road(paths)
    validate_border_road(entry, paths, road)
    return entry, junction, paths, False, road


def creek_candidates(road):
    candidates = []
    for width in (3, 4, 5):
        for left in range(2, MAP_W - width - 1):
            for bridge_top in range(1, MAP_H - 2):
                crossing = {
                    (x, y)
                    for y in range(MAP_H)
                    for x in range(left, left + width)
                    if (x, y) in road
                }
                bridge_water = {
                    (x, y)
                    for y in (bridge_top, bridge_top + 1)
                    for x in range(left, left + width)
                }
                approaches = {
                    (left - 1, bridge_top),
                    (left - 1, bridge_top + 1),
                    (left + width, bridge_top),
                    (left + width, bridge_top + 1),
                }
                if crossing and crossing <= bridge_water and approaches <= road:
                    candidates.append(CreekSpec(left, width, bridge_top))
    return candidates


def stream_cells(shape):
    cells = set()
    for y in range(MAP_H):
        if shape.boundary and y >= shape.boundary:
            left, width = shape.downstream_left, shape.downstream_width
        else:
            left, width = shape.upstream_left, shape.upstream_width
        cells.update((x, y) for x in range(left, left + width))
    return cells


def stream_transition_candidates(road, creek):
    base_left = creek.left
    base_width = creek.width
    bridge_water = {
        (x, y)
        for y in (creek.bridge_top, creek.bridge_top + 1)
        for x in range(base_left, base_left + base_width)
    }
    remote_spans = (
        (base_left + 1, base_width - 1),
        (base_left, base_width - 1),
        (base_left - 1, base_width + 1),
        (base_left, base_width + 1),
    )
    candidates = []
    for boundary in range(1, MAP_H):
        if not (
            boundary <= creek.bridge_top - 2
            or boundary >= creek.bridge_top + 4
        ):
            continue
        bridge_is_downstream = boundary <= creek.bridge_top - 2
        for remote_left, remote_width in remote_spans:
            if (
                remote_width < 3
                or remote_left < 1
                or remote_left + remote_width > MAP_W - 1
            ):
                continue
            if bridge_is_downstream:
                shape = StreamShape(
                    remote_left, remote_width, base_left, base_width, boundary
                )
            else:
                shape = StreamShape(
                    base_left, base_width, remote_left, remote_width, boundary
                )
            if (stream_cells(shape) & road) <= bridge_water:
                candidates.append(shape)
    return candidates


def choose_stream_shape(features, road, creek):
    straight = StreamShape(creek.left, creek.width, creek.left, creek.width, 0)
    transitions = stream_transition_candidates(road, creek)
    if features.bounded(100) < 55 and transitions:
        return transitions[features.bounded(len(transitions))]
    return straight


def stamp_stream(runtime_map, shape):
    water = set()
    transition_cells = set()
    for y in range(MAP_H):
        if shape.boundary and y >= shape.boundary:
            left, width = shape.downstream_left, shape.downstream_width
        else:
            left, width = shape.upstream_left, shape.upstream_width
        for column in range(width):
            x = left + column
            tile_id = (
                STREAM_LEFT_TILE
                if column == 0
                else STREAM_RIGHT_TILE
                if column == width - 1
                else STREAM_CENTER_TILE
            )
            runtime_map.layers[0][y * MAP_W + x] = tile_id
            water.add((x, y))

    if not shape.boundary:
        return water, transition_cells

    boundary = shape.boundary
    upstream_left = shape.upstream_left
    upstream_right = upstream_left + shape.upstream_width
    downstream_left = shape.downstream_left
    downstream_right = downstream_left + shape.downstream_width
    if downstream_left > upstream_left:
        corners = (
            (upstream_left, boundary - 1, STREAM_BOTTOM_OUTER_LEFT_TILE),
            (downstream_left, boundary - 1, STREAM_BOTTOM_INNER_LEFT_TILE),
        )
    elif downstream_left < upstream_left:
        corners = (
            (downstream_left, boundary, STREAM_TOP_OUTER_LEFT_TILE),
            (upstream_left, boundary, STREAM_TOP_INNER_LEFT_TILE),
        )
    else:
        corners = ()
    if downstream_right < upstream_right:
        corners += (
            (upstream_right - 1, boundary - 1, STREAM_BOTTOM_OUTER_RIGHT_TILE),
            (downstream_right - 1, boundary - 1, STREAM_BOTTOM_INNER_RIGHT_TILE),
        )
    elif downstream_right > upstream_right:
        corners += (
            (downstream_right - 1, boundary, STREAM_TOP_OUTER_RIGHT_TILE),
            (upstream_right - 1, boundary, STREAM_TOP_INNER_RIGHT_TILE),
        )
    for x, y, tile_id in corners:
        runtime_map.layers[0][y * MAP_W + x] = tile_id
        transition_cells.add((x, y))
    return water, transition_cells


def stamp_bridge(runtime_map, creek):
    bridge = set()
    left = creek.left - 1
    length = creek.width + 2
    for x in range(left, left + length):
        runtime_map.layers[2][creek.bridge_top * MAP_W + x] = BRIDGE_TOP_TILE
        runtime_map.layers[2][(creek.bridge_top + 1) * MAP_W + x] = BRIDGE_BOTTOM_TILE
        bridge.update(((x, creek.bridge_top), (x, creek.bridge_top + 1)))
    return bridge


def cliff_candidates(road, water, bridge, shape):
    minimum_left = min(shape.upstream_left, shape.downstream_left)
    maximum_right = max(
        shape.upstream_left + shape.upstream_width,
        shape.downstream_left + shape.downstream_width,
    )
    candidates = []
    for left, right in ((0, minimum_left), (maximum_right, MAP_W)):
        if right - left < 5:
            continue
        for top in range(1, MAP_H - 2):
            cliff = {(x, y) for x in range(left, right) for y in (top, top + 1)}
            if cliff & (water | bridge):
                continue
            for stair_left in range(left + 1, right - 2):
                stairs = {
                    (x, y)
                    for x in (stair_left, stair_left + 1)
                    for y in (top, top + 1)
                }
                vertical_approach = {
                    (x, y)
                    for x in (stair_left, stair_left + 1)
                    for y in (top - 1, top + 2)
                }
                if (
                    stairs <= road
                    and vertical_approach <= road
                    and (road & cliff) <= stairs
                ):
                    candidates.append(CliffSpec(top, left, right, stair_left))
    return candidates


def stamp_cliff(runtime_map, spec):
    cliff = set()
    stairs = set()
    for x in range(spec.left, spec.right):
        for y in (spec.top, spec.top + 1):
            point = (x, y)
            cliff.add(point)
            if spec.stair_left <= x < spec.stair_left + 2:
                runtime_map.layers[0][y * MAP_W + x] = (
                    CLIFF_STAIR_LEFT_TILE
                    if x == spec.stair_left
                    else CLIFF_STAIR_RIGHT_TILE
                )
                runtime_map.layers[1][y * MAP_W + x] = 0
                stairs.add(point)
            elif y == spec.top:
                runtime_map.layers[0][y * MAP_W + x] = CLIFF_TOP_TILE
                runtime_map.layers[1][y * MAP_W + x] = 0
            else:
                runtime_map.layers[1][y * MAP_W + x] = CLIFF_FACE_TILE
    return cliff, stairs


def stamp_water_rocks(runtime_map, features, water, road, bridge, transition_cells):
    rocks = set()
    transition_exclusion = expanded(transition_cells)
    target = 1 + features.bounded(2)
    for _index in range(target):
        large = features.bounded(100) < 35
        forbidden = road | bridge | transition_exclusion | expanded(rocks)

        def candidates_for(kind):
            width = 2 if kind == "large" else 1
            height = 2 if kind == "large" else 1
            result = []
            for y in range(MAP_H - height + 1):
                for x in range(MAP_W - width + 1):
                    cells = {
                        (x + dx, y + dy)
                        for dy in range(height)
                        for dx in range(width)
                    }
                    if cells <= water and not cells & forbidden and all(
                        runtime_map.layers[0][cy * MAP_W + cx] == STREAM_CENTER_TILE
                        for cx, cy in cells
                    ):
                        result.append((x, y))
            return result

        kind = "large" if large else "small"
        candidates = candidates_for(kind)
        if not candidates and large:
            kind = "small"
            candidates = candidates_for(kind)
        if not candidates:
            continue
        left, top = candidates[features.bounded(len(candidates))]
        if kind == "small":
            runtime_map.layers[2][top * MAP_W + left] = WATER_ROCK_SMALL_TILE
            rocks.add((left, top))
        else:
            for dy, row in enumerate(WATER_ROCK_LARGE_TILES):
                for dx, tile_id in enumerate(row):
                    runtime_map.layers[2][(top + dy) * MAP_W + left + dx] = tile_id
                    rocks.add((left + dx, top + dy))
    return rocks


def transform_frost_point(point, mirror_x, mirror_y):
    x, y = point
    if mirror_x:
        x = MAP_W - 1 - x
    if mirror_y:
        y = MAP_H - 1 - y
    return x, y


def transform_frost_edge(edge, mirror_x, mirror_y):
    if mirror_x:
        if edge == Edge.LEFT:
            edge = Edge.RIGHT
        elif edge == Edge.RIGHT:
            edge = Edge.LEFT
    if mirror_y:
        if edge == Edge.TOP:
            edge = Edge.BOTTOM
        elif edge == Edge.BOTTOM:
            edge = Edge.TOP
    return edge


def transform_frost_endpoint(value, mirror_x, mirror_y):
    point, edge = value
    transformed_point = transform_frost_point(point, mirror_x, mirror_y)
    if mirror_x and edge in (Edge.TOP, Edge.BOTTOM):
        transformed_point = (transformed_point[0] - 1, transformed_point[1])
    if mirror_y and edge in (Edge.LEFT, Edge.RIGHT):
        transformed_point = (transformed_point[0], transformed_point[1] - 1)
    return Endpoint(
        transformed_point,
        transform_frost_edge(edge, mirror_x, mirror_y),
    )


def expand_frost_route(control_points, mirror_x, mirror_y):
    transformed = [
        transform_frost_point(point, mirror_x, mirror_y)
        for point in control_points
    ]
    points = [transformed[0]]
    for x, y in transformed[1:]:
        append_line(points, x, y)
    return points


def frost_boundary_tile(floor, openings, x, y):
    neighbors = (
        (Edge.TOP, (x, y - 1), "top"),
        (Edge.RIGHT, (x + 1, y), "right"),
        (Edge.BOTTOM, (x, y + 1), "bottom"),
        (Edge.LEFT, (x - 1, y), "left"),
    )
    outside = frozenset(
        name
        for edge, neighbor, name in neighbors
        if neighbor not in floor and ((x, y), edge) not in openings
    )
    if not outside:
        return 0
    corners = {
        frozenset(("top", "left")): FROST_WALL_TILES["top_left"],
        frozenset(("top", "right")): FROST_WALL_TILES["top_right"],
        frozenset(("bottom", "left")): FROST_WALL_TILES["bottom_left"],
        frozenset(("bottom", "right")): FROST_WALL_TILES["bottom_right"],
    }
    if outside in corners:
        return corners[outside]
    if len(outside) == 1:
        return FROST_WALL_TILES[next(iter(outside))]
    raise ValueError(f"unsupported frost wall boundary at {(x, y)}: {sorted(outside)}")


def stamp_frost_portals(runtime_map):
    route_cells = {
        point
        for path in runtime_map.paths
        for point in path.points
    }
    portals = [(runtime_map.entry, runtime_map.paths[0].points[0])]
    portals.extend((path.exit, path.points[-1]) for path in runtime_map.paths)
    for endpoint, route_anchor in portals:
        x, y = route_anchor
        side_cells = {(x - 1, y), (x + 1, y)}
        if (
            endpoint.edge == Edge.TOP
            and 0 < x < MAP_W - 1
            and not side_cells & route_cells
        ):
            for offset, tile_id in enumerate(FROST_CAVE_EXIT_RUNTIME_TILES, -1):
                runtime_map.layers[1][y * MAP_W + x + offset] = tile_id
        elif endpoint.edge == Edge.BOTTOM:
            runtime_map.layers[1][y * MAP_W + x] = (
                FROST_DOWNWARD_STAIRS_RUNTIME_TILE
            )


def is_smooth_ice_tile(tile_id):
    return tile_id in SMOOTH_ICE_TILE_IDS


def route_crosses_ice_straight(runtime_map, path):
    for index, (x, y) in enumerate(path.points):
        if not is_smooth_ice_tile(runtime_map.layers[0][y * MAP_W + x]):
            continue
        if index == 0 or index + 1 >= len(path.points):
            return False
        previous = path.points[index - 1]
        following = path.points[index + 1]
        incoming = (x - previous[0], y - previous[1])
        outgoing = (following[0] - x, following[1] - y)
        if incoming not in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            return False
        if incoming != outgoing:
            return False
    return True


def generate_frost_cave_map(seed, entry_edge):
    horizontal = entry_edge in (Edge.LEFT, Edge.RIGHT)
    templates = FROST_HORIZONTAL_TEMPLATES if horizontal else FROST_VERTICAL_TEMPLATES
    spec = templates[(seed >> 5) & 1]
    mirror_x = entry_edge == Edge.RIGHT
    mirror_y = entry_edge == Edge.TOP

    entry = transform_frost_endpoint(spec["entry"], mirror_x, mirror_y)
    exits = [
        transform_frost_endpoint(value, mirror_x, mirror_y)
        for value in spec["exits"]
    ]
    paths = []
    for index, control_points in enumerate(spec["routes"]):
        points = expand_frost_route(control_points, mirror_x, mirror_y)
        append_line(points, *exits[index].point)
        paths.append(RoutePath(points, exits[index]))
    runtime_map = RuntimeMap(
        seed=seed,
        area_index=FROST_CRYSTAL_CAVE_AREA,
        entry=entry,
        junction=transform_frost_point(spec["junction"], mirror_x, mirror_y),
        paths=paths,
        layers=[
            [FROST_OUTSIDE_TILE] * (MAP_W * MAP_H),
            [0] * (MAP_W * MAP_H),
            [0] * (MAP_W * MAP_H),
        ],
        has_coast=False,
        has_forest=False,
        has_creek=False,
        has_cliff=False,
        has_waterfall=False,
    )

    floor = set()
    for left, top, right, bottom in spec["rects"]:
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                floor.add(transform_frost_point((x, y), mirror_x, mirror_y))
    for x, y in floor:
        runtime_map.layers[0][y * MAP_W + x] = FROST_FLOOR_TILE

    portals = (entry, *exits)
    openings = {
        (cell, endpoint.edge)
        for endpoint in portals
        for cell in endpoint_edge_cells(endpoint)
    }
    if any(not endpoint_edge_cells(endpoint) <= floor for endpoint in portals):
        raise ValueError("frost cave portal leaves its floor mask")

    walls = set()
    for x, y in sorted(floor):
        tile_id = frost_boundary_tile(floor, openings, x, y)
        if tile_id:
            runtime_map.layers[1][y * MAP_W + x] = tile_id
            walls.add((x, y))

    inner_checks = (
        ("outside_nw", (-1, -1), (-1, 0), (0, -1)),
        ("outside_ne", (1, -1), (1, 0), (0, -1)),
        ("outside_sw", (-1, 1), (-1, 0), (0, 1)),
        ("outside_se", (1, 1), (1, 0), (0, 1)),
    )
    for x, y in sorted(floor):
        index = y * MAP_W + x
        if runtime_map.layers[1][index]:
            continue
        matches = []
        for name, diagonal, first_side, second_side in inner_checks:
            diagonal_cell = (x + diagonal[0], y + diagonal[1])
            first_cell = (x + first_side[0], y + first_side[1])
            second_cell = (x + second_side[0], y + second_side[1])
            if (
                diagonal_cell not in floor
                and first_cell in floor
                and second_cell in floor
            ):
                matches.append(name)
        if len(matches) > 1:
            raise ValueError(f"ambiguous frost inner corner at {(x, y)}: {matches}")
        if matches:
            runtime_map.layers[1][index] = FROST_INNER_CORNER_TILES[matches[0]]
            walls.add((x, y))

    stamp_frost_portals(runtime_map)

    ice = set()
    for left, top, right, bottom in spec["ice_rects"]:
        for y in range(top, bottom + 1):
            for x in range(left, right + 1):
                ice.add(transform_frost_point((x, y), mirror_x, mirror_y))
    for x, y in sorted(ice):
        if (x, y) not in floor or (x, y) in walls:
            raise ValueError(f"frost ice crosses a wall at {(x, y)}")
        north = (x, y - 1) not in ice
        east = (x + 1, y) not in ice
        south = (x, y + 1) not in ice
        west = (x - 1, y) not in ice
        if north and west:
            name = "top_left"
        elif north and east:
            name = "top_right"
        elif south and west:
            name = "bottom_left"
        elif south and east:
            name = "bottom_right"
        elif north:
            name = "top"
        elif south:
            name = "bottom"
        elif west:
            name = "left"
        elif east:
            name = "right"
        else:
            name = "center"
        runtime_map.layers[0][y * MAP_W + x] = FROST_ICE_TILES[name]

    route_cells = {
        point
        for path in paths
        for point in path.points
    }
    blocked = set(walls)
    hill_origin = spec["rock_hill"]
    if hill_origin is not None:
        first = transform_frost_point(hill_origin, mirror_x, mirror_y)
        last = transform_frost_point(
            (hill_origin[0] + 2, hill_origin[1] + 2),
            mirror_x,
            mirror_y,
        )
        hill_left = min(first[0], last[0])
        hill_top = min(first[1], last[1])
        hill_cells = {
            (hill_left + column, hill_top + row)
            for row in range(3)
            for column in range(3)
        }
        if not hill_cells <= floor or hill_cells & blocked or hill_cells & route_cells:
            raise ValueError("transparent frost rock hill overlaps blocked terrain")
        for row, tile_row in enumerate(FROST_ROCK_HILL_TILES):
            for column, tile_id in enumerate(tile_row):
                x = hill_left + column
                y = hill_top + row
                runtime_map.layers[1][y * MAP_W + x] = tile_id
        blocked.update(hill_cells)

    for x, y, tile_id in spec["crystals"]:
        x, y = transform_frost_point((x, y), mirror_x, mirror_y)
        if (x, y) in blocked or (x, y) in route_cells or y == 0:
            raise ValueError(f"frost crystal blocks a route at {(x, y)}")
        runtime_map.layers[1][y * MAP_W + x] = tile_id
        runtime_map.layers[2][(y - 1) * MAP_W + x] = FROST_CRYSTAL_TOP_TILE
        blocked.add((x, y))

    for x, y, tile_id in spec["boulders"]:
        x, y = transform_frost_point((x, y), mirror_x, mirror_y)
        if (x, y) in blocked or (x, y) in route_cells:
            raise ValueError(f"frost boulder blocks a route at {(x, y)}")
        runtime_map.layers[1][y * MAP_W + x] = tile_id
        blocked.add((x, y))

    for path in paths:
        if len(path.points) < 2:
            raise ValueError("frost route is too short")
        if not route_crosses_ice_straight(runtime_map, path):
            raise ValueError("frost route turns or stops on smooth ice")
        for point in path.points:
            if point not in floor or point in blocked:
                raise ValueError(f"frost route crosses blocked terrain: {point}")
    return runtime_map


def generate_map(seed, entry_edge, area_index=0):
    seed &= MASK32
    area_index &= 0xFF
    terrain = XorShift32(seed ^ 0x63D83595)
    features = XorShift32(seed ^ 0xC2B2AE35)
    profile = AREA_PROFILES[area_index] if area_index < len(AREA_PROFILES) else AREA_PROFILES[0]
    creek = None
    has_waterfall = area_index == ANCIENT_WATERFALL_VALLEY_AREA
    has_snow = area_index == FROST_CRYSTAL_CAVE_AREA

    if has_snow:
        return generate_frost_cave_map(seed, entry_edge)

    if has_waterfall:
        entry, junction, paths, has_coast, road = build_ancient_waterfall_topology(
            entry_edge
        )
    elif area_index == CREEK_BRIDGE_SLOPE_AREA:
        for attempt in range(16):
            topology_seed = seed ^ ((0x9E3779B9 * (attempt + 1)) & MASK32)
            entry, junction, paths, has_coast, road = build_topology(
                topology_seed, entry_edge, coast_chance=0
            )
            candidates = creek_candidates(road)
            if candidates:
                creek = candidates[features.bounded(len(candidates))]
                break
        if creek is None:
            raise ValueError("unable to place creek bridge after 16 topology attempts")
    elif area_index == MIST_FOREST_PATH_AREA:
        for attempt in range(16):
            topology_seed = (
                seed
                if attempt == 0
                else seed ^ ((0x9E3779B9 * attempt) & MASK32)
            )
            entry, junction, paths, has_coast, road = build_topology(
                topology_seed, entry_edge, coast_chance=profile[0]
            )
            if any(not (forest_footprint(spec) & road) for spec in FOREST_SPECS):
                break
        else:
            raise ValueError("unable to place mist forest after 16 topology attempts")
    else:
        entry, junction, paths, has_coast, road = build_topology(
            seed, entry_edge, coast_chance=profile[0]
        )

    runtime_map = RuntimeMap(
        seed=seed,
        area_index=area_index,
        entry=entry,
        junction=junction,
        paths=paths,
        layers=[[0] * (MAP_W * MAP_H) for _ in range(3)],
        has_coast=has_coast,
        has_forest=False,
        has_creek=creek is not None,
        has_cliff=False,
        has_waterfall=has_waterfall,
    )
    water = set()
    forest = set()
    cliff = set()
    stairs = set()
    bridge = set()
    transition_cells = set()
    scenery = set()

    for index in range(MAP_W * MAP_H):
        tiles = SNOW_GROUND_TILES if has_snow else GRASS_TILES
        runtime_map.layers[0][index] = tiles[terrain.bounded(len(tiles))]
    if has_coast:
        stamp_coast(runtime_map, water)

    if has_waterfall:
        water, forest, cliff, stairs, scenery = stamp_ancient_waterfall_valley(
            runtime_map
        )
        runtime_map.has_forest = True
        runtime_map.has_cliff = True

    if creek is not None:
        shape = choose_stream_shape(features, road, creek)
        water, transition_cells = stamp_stream(runtime_map, shape)
        bridge = stamp_bridge(runtime_map, creek)
        if features.bounded(100) < 60:
            candidates = cliff_candidates(road, water, bridge, shape)
            if candidates:
                cliff_spec = candidates[features.bounded(len(candidates))]
                cliff, stairs = stamp_cliff(runtime_map, cliff_spec)
                runtime_map.has_cliff = True
        stamp_water_rocks(
            runtime_map, features, water, road, bridge, transition_cells
        )

    for x, y in road:
        if (x, y) not in water and (x, y) not in cliff and (x, y) not in forest:
            runtime_map.layers[0][y * MAP_W + x] = (
                SNOW_PATH_TILE if has_snow else road_tile(runtime_map, road, x, y)
            )

    if (
        creek is None
        and not has_waterfall
        and not has_snow
        and terrain.bounded(100) < profile[1]
    ):
        order = [0, 1, 2, 3]
        for count in range(4, 1, -1):
            swap_index = terrain.bounded(count)
            order[count - 1], order[swap_index] = order[swap_index], order[count - 1]
        for index in order:
            footprint = forest_footprint(FOREST_SPECS[index])
            if footprint & road or footprint & water:
                continue
            stamp_forest(runtime_map, FOREST_SPECS[index])
            forest = footprint
            runtime_map.has_forest = True
            break

    if has_snow:
        stamp_snow_decorations(runtime_map, terrain, road, scenery)
    else:
        stamp_ground_decorations(
            runtime_map, terrain, road, water, forest | cliff | scenery, profile
        )

    for path in paths:
        if len(path.points) < 2:
            raise ValueError("route is too short")
        for point in path.points:
            blocked_water = point in water and point not in bridge
            blocked_cliff = point in cliff and point not in stairs
            if point not in road or blocked_water or blocked_cliff or point in forest:
                raise ValueError(f"route crosses blocked scenery: {point}")
    return runtime_map


def fnv_byte(value, byte):
    return ((value ^ byte) * 16777619) & MASK32


def fnv_word(value, word):
    value = fnv_byte(value, word & 0xFF)
    return fnv_byte(value, (word >> 8) & 0xFF)


def fingerprint(runtime_map):
    value = 2166136261
    value = fnv_word(value, ALGORITHM_VERSION)
    value = fnv_byte(value, runtime_map.area_index)
    value = fnv_byte(value, int(runtime_map.entry.edge))
    value = fnv_byte(value, runtime_map.entry.point[0])
    value = fnv_byte(value, runtime_map.entry.point[1])
    value = fnv_byte(value, runtime_map.junction[0])
    value = fnv_byte(value, runtime_map.junction[1])
    value = fnv_byte(value, len(runtime_map.paths))
    for path in runtime_map.paths:
        value = fnv_byte(value, len(path.points))
        value = fnv_byte(value, int(path.exit.edge))
        value = fnv_byte(value, path.exit.point[0])
        value = fnv_byte(value, path.exit.point[1])
        for x, y in path.points:
            value = fnv_byte(value, x)
            value = fnv_byte(value, y)
    value = fnv_byte(value, 1 if runtime_map.has_coast else 0)
    value = fnv_byte(value, 1 if runtime_map.has_forest else 0)
    value = fnv_byte(value, 1 if runtime_map.has_creek else 0)
    value = fnv_byte(value, 1 if runtime_map.has_cliff else 0)
    value = fnv_byte(value, 1 if runtime_map.has_waterfall else 0)
    for layer in runtime_map.layers:
        for tile_id in layer:
            value = fnv_word(value, tile_id)
    return value


def endpoint_json(endpoint):
    return {"point": list(endpoint.point), "edge": EDGE_NAMES[endpoint.edge]}


def map_json(runtime_map):
    return {
        "algorithmVersion": ALGORITHM_VERSION,
        "seed": runtime_map.seed,
        "areaIndex": runtime_map.area_index,
        "fingerprint": f"{fingerprint(runtime_map):08x}",
        "size": [MAP_W, MAP_H],
        "entry": endpoint_json(runtime_map.entry),
        "junction": list(runtime_map.junction),
        "paths": [
            {
                "points": [list(point) for point in path.points],
                "exit": endpoint_json(path.exit),
            }
            for path in runtime_map.paths
        ],
        "hasCoast": runtime_map.has_coast,
        "hasForest": runtime_map.has_forest,
        "hasCreek": runtime_map.has_creek,
        "hasCliff": runtime_map.has_cliff,
        "hasWaterfall": runtime_map.has_waterfall,
        "layers": runtime_map.layers,
    }


def render(runtime_map, frame_index=0):
    tileset = Image.open(TILESET).convert("RGBA")
    autotiles = load_autotiles(WATERFALL_AUTOTILE_NAMES)
    external_tilesets = {
        tileset_name: Image.open(TILESET.parent / tileset_name).convert("RGBA")
        for tileset_name, _source_id in CUSTOM_TILE_SOURCES.values()
    }
    rendered_layers = []
    for layer in runtime_map.layers:
        outside_ids = [0 if tile_id in CUSTOM_TILE_SOURCES else tile_id for tile_id in layer]
        rendered = render_layer(outside_ids, tileset, autotiles, frame_index)
        for tileset_name, external_tileset in external_tilesets.items():
            external_ids = [
                CUSTOM_TILE_SOURCES[tile_id][1]
                if tile_id in CUSTOM_TILE_SOURCES
                and CUSTOM_TILE_SOURCES[tile_id][0] == tileset_name
                and tile_id not in CUSTOM_TILE_SOURCE_FLIP_Y
                else 0
                for tile_id in layer
            ]
            rendered.alpha_composite(render_layer(external_ids, external_tileset))
            for index, tile_id in enumerate(layer):
                if (
                    tile_id not in CUSTOM_TILE_SOURCE_FLIP_Y
                    or CUSTOM_TILE_SOURCES[tile_id][0] != tileset_name
                ):
                    continue
                source_id = CUSTOM_TILE_SOURCES[tile_id][1]
                source_index = source_id - 384
                source_x = (source_index % 8) * 32
                source_y = (source_index // 8) * 32
                tile = external_tileset.crop(
                    (source_x, source_y, source_x + 32, source_y + 32)
                ).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
                rendered.alpha_composite(
                    tile, ((index % MAP_W) * 32, (index // MAP_W) * 32)
                )
        rendered_layers.append(rendered)
    combined = Image.new("RGBA", rendered_layers[0].size, (0, 0, 0, 255))
    for layer in rendered_layers:
        combined.alpha_composite(layer)
    return combined.resize((MAP_W * GAME_TILE, MAP_H * GAME_TILE), Image.Resampling.NEAREST)


def point_center(point):
    return (
        point[0] * GAME_TILE + GAME_TILE // 2,
        point[1] * GAME_TILE + GAME_TILE // 2,
    )


def render_debug(runtime_map, clean):
    debug = clean.copy()
    draw = ImageDraw.Draw(debug, "RGBA")
    colors = ((62, 139, 255, 235), (244, 82, 82, 225))
    for path, color in zip(runtime_map.paths, colors):
        centers = [point_center(point) for point in path.points]
        draw.line(centers, fill=color, width=4)
        for x, y in centers:
            draw.ellipse((x - 2, y - 2, x + 2, y + 2), fill=(255, 255, 255, 230))
        ex, ey = point_center(path.exit.point)
        draw.ellipse((ex - 8, ey - 8, ex + 8, ey + 8), outline=(255, 196, 76, 255), width=3)
    ix, iy = point_center(runtime_map.entry.point)
    draw.rectangle((ix - 7, iy - 7, ix + 7, iy + 7), outline=(78, 220, 117, 255), width=3)
    jx, jy = point_center(runtime_map.junction)
    draw.polygon(
        ((jx, jy - 7), (jx + 7, jy), (jx, jy + 7), (jx - 7, jy)),
        fill=(255, 255, 255, 240),
        outline=(35, 43, 47, 255),
    )
    label = (
        f"v{ALGORITHM_VERSION} area={runtime_map.area_index} seed={runtime_map.seed} "
        f"fp={fingerprint(runtime_map):08x} entry={EDGE_NAMES[runtime_map.entry.edge]}"
    )
    draw.rectangle((0, 0, min(clean.width, len(label) * 7 + 8), 18), fill=(14, 21, 24, 220))
    draw.text((4, 3), label, fill=(255, 242, 112, 255), font=ImageFont.load_default())
    return debug


def make_overview(images, output_path):
    columns = 2
    rows = (len(images) + columns - 1) // columns
    gap = 8
    canvas = Image.new(
        "RGBA",
        (columns * MAP_W * GAME_TILE + (columns + 1) * gap,
         rows * MAP_H * GAME_TILE + (rows + 1) * gap),
        (20, 27, 29, 255),
    )
    for index, image in enumerate(images):
        x = gap + (index % columns) * (MAP_W * GAME_TILE + gap)
        y = gap + (index // columns) * (MAP_H * GAME_TILE + gap)
        canvas.alpha_composite(image, (x, y))
    canvas.save(output_path)


def parse_seed(value):
    return int(value, 0) & MASK32


def main():
    parser = argparse.ArgumentParser(description="Preview StickMon's runtime tile map algorithm")
    parser.add_argument("--seed", type=parse_seed, default=0x20260713)
    parser.add_argument("--map-seed", type=parse_seed)
    parser.add_argument("--entry", choices=("random", *NAME_EDGES), default="random")
    parser.add_argument("--count", type=int, choices=range(1, 9))
    parser.add_argument("--areas", default="0,1,2,3")
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/stickmon-runtime-tile-maps"))
    args = parser.parse_args()

    areas = [int(value) for value in args.areas.split(",") if value.strip()]
    if not areas or any(area < 0 or area > 255 for area in areas):
        raise ValueError("--areas must contain comma-separated values from 0 to 255")
    if args.map_seed is not None and args.count not in (None, 1):
        raise ValueError("--map-seed can only generate one map")
    count = args.count if args.count is not None else (1 if args.map_seed is not None else 4)
    entry_seed = args.map_seed if args.map_seed is not None else args.seed
    entry = Edge((entry_seed >> 8) & 3) if args.entry == "random" else NAME_EDGES[args.entry]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    clean_images = []
    debug_images = []
    manifest = {
        "algorithmVersion": ALGORITHM_VERSION,
        "expeditionSeed": args.seed,
        "maps": [],
    }
    for index in range(count):
        area = areas[index % len(areas)]
        map_seed = args.map_seed if args.map_seed is not None else derive_seed(args.seed, index, area)
        runtime_map = generate_map(map_seed, entry, area)
        clean = render(runtime_map)
        debug = render_debug(runtime_map, clean)
        stem = f"runtime_tile_map_{index + 1:02d}_{map_seed:08x}"
        clean_path = args.output_dir / f"{stem}.png"
        debug_path = args.output_dir / f"{stem}_debug.png"
        json_path = args.output_dir / f"{stem}.json"
        clean.save(clean_path)
        debug.save(debug_path)
        payload = map_json(runtime_map)
        json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        selected_exit = (map_seed >> 16) % len(runtime_map.paths)
        manifest["maps"].append({
            "index": index,
            "area": area,
            "selectedExit": selected_exit,
            "image": clean_path.name,
            "debug": debug_path.name,
            "data": json_path.name,
            **{
                key: payload[key]
                for key in (
                    "seed", "fingerprint", "entry", "hasCoast", "hasForest",
                    "hasCreek", "hasCliff", "hasWaterfall",
                )
            },
        })
        clean_images.append(clean)
        debug_images.append(debug)
        entry = opposite(runtime_map.paths[selected_exit].exit.edge)
        print(
            f"map={index + 1} seed=0x{map_seed:08x} fingerprint={fingerprint(runtime_map):08x} "
            f"entry={EDGE_NAMES[runtime_map.entry.edge]} coast={int(runtime_map.has_coast)} "
            f"forest={int(runtime_map.has_forest)} creek={int(runtime_map.has_creek)} "
            f"cliff={int(runtime_map.has_cliff)} waterfall={int(runtime_map.has_waterfall)}"
        )

    manifest_path = args.output_dir / "runtime_tile_map_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    make_overview(clean_images, args.output_dir / "runtime_tile_maps.png")
    make_overview(debug_images, args.output_dir / "runtime_tile_maps_debug.png")
    print(f"manifest={manifest_path}")


if __name__ == "__main__":
    main()
