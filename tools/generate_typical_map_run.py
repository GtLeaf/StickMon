#!/usr/bin/env python3

import argparse
import random
from collections import deque
from math import ceil, floor
from pathlib import Path

from PIL import Image, ImageDraw

from cave_tile_semantics import (
    DOWN_LADDER_BASE_TILES,
    DOWN_LADDER_CENTER_TILE,
    DOWN_LADDER_TILE,
    UP_LADDER_TILES,
)
from generate_map_rule_preview import (
    GAME_TILE,
    MAP_H,
    MAP_W,
    TILESET,
    WATERFALL_AUTOTILE_NAMES,
    center,
    load_autotiles,
    render_layer,
    sample_route,
)
from map_generation_rules import (
    STREAM_BOTTOM_INNER_LEFT_TILE,
    STREAM_BOTTOM_INNER_RIGHT_TILE,
    STREAM_BOTTOM_OUTER_LEFT_TILE,
    STREAM_BOTTOM_OUTER_RIGHT_TILE,
    STREAM_TOP_INNER_LEFT_TILE,
    STREAM_TOP_INNER_RIGHT_TILE,
    STREAM_TOP_OUTER_LEFT_TILE,
    STREAM_TOP_OUTER_RIGHT_TILE,
    FOREST_BODY_IDS,
    FOREST_CROWN_IDS,
    HIGH_GRASS_TILE_ID,
    LIGHTHOUSE_IDS,
    ROAD_TILE_IDS,
    build_two_tile_road,
    stamp_forest_fence,
    stamp_high_grass,
    stamp_left_coast,
    stamp_lighthouse,
    two_tile_road_cells,
)


# Map028's rock-edged still-water module. Every tile keeps the Outside
# tileset's passage=0x0f and terrainTag=6 metadata.
POND_TILES = (
    (1088, 1089, 1090),
    (1096, 1097, 1098),
    (1104, 1092, 1106),
)

VERTICAL_STREAM_TILES = (1096, 1097, 1098)
SEA_WATER_TILE = 48
SEA_LEFT_SHORE_TILE = 64
SEA_TOP_SHORE_TILE = 68
SEA_RIGHT_SHORE_TILE = 72
SEA_TOP_RIGHT_CORNER_TILE = 80
SEA_TOP_LEFT_CORNER_TILE = 84
SEA_BOTTOM_SHORE_TILE = 88
SEA_CORNER_UNDERLAY_TILE = 385
HORIZONTAL_BRIDGE_TILES = (1627, 1643)
CLIFF_TOP_TILE = 1188
CLIFF_FACE_TILE = 1185
CLIFF_STAIR_TILES = (1161, 1162)
WATER_ROCK_LARGE_TILES = (
    (1506, 1507),
    (1514, 1515),
)
WATER_ROCK_SMALL_TILE = 1532

WATERFALL_CREST_TILES = (283, 273, 285)
WATERFALL_BODY_TOP_TILES = (322, 308, 324)
WATERFALL_BODY_MIDDLE_TILES = (304, 288, 312)
WATERFALL_BODY_LOWER_TILES = (328, 316, 326)
WATERFALL_BOTTOM_TILE = 336
TOP_EDGE_FOREST_TILE_ROWS = (
    (800, 801),
    (808, 809),
    (818, 819),
)

CAVE_GROUND_TILES = (609, 610, 610, 610, 610, 611, 618)
CAVE_PATH_TILE = 666
CAVE_BASIN_TOP_TILES = (504, 505, 506)
CAVE_BASIN_SIDE_TILES = (512, 514)
CAVE_BASIN_BOTTOM_TILES = (520, 521, 522)
CAVE_ISLAND_FLOOR_TILE = 4603
CAVE_ISLAND_FLOOR_SOURCE_TILE = 513
CAVE_ISLAND_FLOOR_METADATA_TILE = 610
CAVE_ISLAND_RIM_UNDERLAY_TILE = SEA_WATER_TILE
CAVE_BRIDGE_TILES = (4600, 4601, 4602)
CAVE_CLIFF_FACE_TILES = (576, 577, 578)
CAVE_DOWN_LADDER_TOP_TILES = (4610, 4611, 4612)
CAVE_DOWN_LADDER_RENDER_TILES = (
    CAVE_DOWN_LADDER_TOP_TILES,
    DOWN_LADDER_BASE_TILES[1],
    DOWN_LADDER_BASE_TILES[2],
)
CAVE_BRIDGE_TILE_SOURCES = {
    CAVE_BRIDGE_TILES[0]: ("Outside", 1627),
    CAVE_BRIDGE_TILES[1]: ("Outside", 1635),
    CAVE_BRIDGE_TILES[2]: ("Outside", 1643),
}
CAVE_ARCHIPELAGO_TILE_SOURCES = {
    **CAVE_BRIDGE_TILE_SOURCES,
    CAVE_ISLAND_FLOOR_TILE: ("Caves", CAVE_ISLAND_FLOOR_SOURCE_TILE),
    **{
        custom_id: ("Caves", source_id)
        for custom_id, source_id in zip(
            CAVE_DOWN_LADDER_TOP_TILES,
            DOWN_LADDER_BASE_TILES[0],
        )
    },
}
CAVE_ARCHIPELAGO_TILE_METADATA_SOURCES = {
    **{
        custom_id: ("Outside", source_id)
        for custom_id, (_tileset_name, source_id) in CAVE_BRIDGE_TILE_SOURCES.items()
    },
    CAVE_ISLAND_FLOOR_TILE: ("Caves", CAVE_ISLAND_FLOOR_METADATA_TILE),
    **{
        custom_id: ("Caves", DOWN_LADDER_CENTER_TILE)
        for custom_id in CAVE_DOWN_LADDER_TOP_TILES
    },
}

ROUTE_COLORS = (
    (62, 139, 255, 235),
    (244, 82, 82, 220),
    (255, 184, 64, 225),
)


MAP_SPECS = [
    {
        "key": "01_forest_fork",
        "seed_offset": 101,
        "entry": (7.5, -0.5),
        "entry_edge": "top",
        "exits": [(-0.5, 3.5), (15.5, 3.5)],
        "routes": [
            [(7.5, -0.5), (7.5, 3.5), (-0.5, 3.5)],
            [(7.5, -0.5), (7.5, 3.5), (15.5, 3.5)],
        ],
        "dense": [(1, 0, 5, 3), (10, 0, 5, 3)],
        "ponds": [],
        "flowers": [(1, 1), (5, 1), (10, 1), (14, 1)],
        "forest_stamps": [(2, 5, 14, 11)],
    },
    {
        "key": "02_pond_crossing",
        "seed_offset": 202,
        "entry": (-0.5, 3.5),
        "entry_edge": "left",
        "exits": [(6.5, -0.5), (6.5, 11.5)],
        "routes": [
            [(-0.5, 3.5), (6.5, 3.5), (6.5, -0.5)],
            [(-0.5, 3.5), (6.5, 3.5), (6.5, 11.5)],
        ],
        "dense": [(1, 0, 4, 3), (0, 8, 6, 4)],
        "ponds": [(11, 0)],
        "flowers": [(5, 1), (10, 2), (14, 3), (1, 7), (5, 8)],
        "forest_stamps": [(10, 5, 6, 11)],
    },
    {
        "key": "03_flower_bend",
        "seed_offset": 303,
        "entry": (10.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(10.5, -0.5), (15.5, 3.5)],
        "routes": [
            [(10.5, 11.5), (10.5, 3.5), (10.5, -0.5)],
            [(10.5, 11.5), (10.5, 3.5), (15.5, 3.5)],
        ],
        "dense": [(0, 0, 9, 3), (0, 7, 10, 5), (13, 7, 3, 5)],
        "ponds": [(2, 3)],
        "flowers": [(1, 2), (5, 2), (1, 6), (5, 6), (8, 6), (14, 2), (14, 8)],
        "forest_stamps": [],
    },
    {
        "key": "04_forest_gate",
        "seed_offset": 404,
        "entry": (-0.5, 6.5),
        "entry_edge": "left",
        "exits": [(6.5, -0.5), (6.5, 11.5)],
        "routes": [
            [(-0.5, 6.5), (6.5, 6.5), (6.5, -0.5)],
            [(-0.5, 6.5), (6.5, 6.5), (6.5, 11.5)],
        ],
        "dense": [(1, 1, 4, 4), (0, 9, 6, 3)],
        "ponds": [(11, 1)],
        "flowers": [(5, 2), (10, 3), (14, 4), (1, 8), (5, 9)],
        "forest_stamps": [(10, 5, 6, 11)],
    },
]


def add_water_rect(layers, left, top, width=3, height=3):
    if width < 3 or height < 3:
        raise ValueError("water rectangle must be at least 3x3")
    if left < 0 or top < 0 or left + width > MAP_W or top + height > MAP_H:
        raise ValueError("water rectangle does not fit inside map")

    positions = set()
    for row in range(height):
        tile_row = POND_TILES[0 if row == 0 else 2 if row == height - 1 else 1]
        for column in range(width):
            tile_id = tile_row[0 if column == 0 else 2 if column == width - 1 else 1]
            x = left + column
            y = top + row
            layers[0][y * MAP_W + x] = tile_id
            positions.add((x, y))
    return positions


def add_pond(layers, left, top):
    return add_water_rect(layers, left, top)


def stamp_sea_channel(
    layers,
    left,
    top,
    width,
    bottom=MAP_H,
    top_open_spans=(),
):
    if width < 3:
        raise ValueError("sea channel must be at least 3 tiles wide")
    if left < 0 or left + width > MAP_W or top < 0 or bottom > MAP_H or top >= bottom:
        raise ValueError("sea channel does not fit inside map")

    right = left + width
    top_open_cells = {
        x
        for span_left, span_width in top_open_spans
        for x in range(span_left, span_left + span_width)
    }
    if top_open_cells - set(range(left, right)):
        raise ValueError("sea channel top opening must stay inside the channel")

    positions = set()
    for y in range(top, bottom):
        for column in range(width):
            x = left + column
            if y == top and top_open_spans:
                if x in top_open_cells:
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
                layers[0][index] = SEA_CORNER_UNDERLAY_TILE
                layers[1][index] = tile_id
            else:
                layers[0][index] = tile_id
            positions.add((x, y))
    return positions


def land_cells_from_rows(rows):
    positions = set()
    for y, left, right in rows:
        if y < 0 or y >= MAP_H or left < 0 or right > MAP_W or left >= right:
            raise ValueError(f"land row outside map: {(y, left, right)}")
        positions.update((x, y) for x in range(left, right))
    return positions


def stamp_water_backdrop(layers, land_cells, rng):
    """Build a Sea-first map and cut irregular walkable islands into it."""

    land = set(land_cells)
    if not land:
        raise ValueError("water backdrop needs at least one land cell")
    if any(not (0 <= x < MAP_W and 0 <= y < MAP_H) for x, y in land):
        raise ValueError("water backdrop land cell outside map")

    grass_ids = (385, 385, 385, 386, 387, 388, 389)
    for y in range(MAP_H):
        for x in range(MAP_W):
            index = y * MAP_W + x
            layers[0][index] = rng.choice(grass_ids) if (x, y) in land else SEA_WATER_TILE
            layers[1][index] = 0

    water = {
        (x, y)
        for y in range(MAP_H)
        for x in range(MAP_W)
        if (x, y) not in land
    }
    for x, y in water:
        north_land = (x, y - 1) in land
        east_land = (x + 1, y) in land
        south_land = (x, y + 1) in land
        west_land = (x - 1, y) in land
        tile_id = SEA_WATER_TILE
        if north_land and east_land:
            tile_id = SEA_TOP_RIGHT_CORNER_TILE
        elif north_land and west_land:
            tile_id = SEA_TOP_LEFT_CORNER_TILE
        elif north_land:
            tile_id = SEA_TOP_SHORE_TILE
        elif south_land:
            tile_id = SEA_BOTTOM_SHORE_TILE
        elif west_land:
            tile_id = SEA_LEFT_SHORE_TILE
        elif east_land:
            tile_id = SEA_RIGHT_SHORE_TILE

        index = y * MAP_W + x
        if tile_id in (SEA_TOP_LEFT_CORNER_TILE, SEA_TOP_RIGHT_CORNER_TILE):
            layers[0][index] = SEA_CORNER_UNDERLAY_TILE
            layers[1][index] = tile_id
        else:
            layers[0][index] = tile_id
    return water


def stamp_cave_basin(
    layers,
    left,
    top,
    width,
    height,
    open_top_spans=(),
    open_bottom_spans=(),
):
    if width < 5 or height < 4:
        raise ValueError("cave basin must be at least 5x4 tiles")
    if left < 0 or top < 0 or left + width > MAP_W or top + height > MAP_H:
        raise ValueError("cave basin does not fit inside map")

    right = left + width
    bottom = top + height
    top_open = {
        x
        for span_left, span_width in open_top_spans
        for x in range(span_left, span_left + span_width)
    }
    bottom_open = {
        x
        for span_left, span_width in open_bottom_spans
        for x in range(span_left, span_left + span_width)
    }
    valid_x = set(range(left + 1, right - 1))
    if top_open - valid_x or bottom_open - valid_x:
        raise ValueError("cave basin opening must stay inside its side walls")

    water = set()
    rim = set()
    for y in range(top, bottom):
        for x in range(left, right):
            index = y * MAP_W + x
            on_top = y == top
            on_bottom = y == bottom - 1
            on_left = x == left
            on_right = x == right - 1
            opening = (on_top and x in top_open) or (on_bottom and x in bottom_open)
            if opening or not (on_top or on_bottom or on_left or on_right):
                layers[0][index] = SEA_WATER_TILE
                layers[1][index] = 0
                water.add((x, y))
                continue

            if on_top:
                tile_id = CAVE_BASIN_TOP_TILES[
                    0 if on_left else 2 if on_right else 1
                ]
            elif on_bottom:
                tile_id = CAVE_BASIN_BOTTOM_TILES[
                    0 if on_left else 2 if on_right else 1
                ]
            else:
                tile_id = CAVE_BASIN_SIDE_TILES[0 if on_left else 1]
            layers[1][index] = tile_id
            rim.add((x, y))
    return {"water": water, "rim": rim}


def stamp_cave_island(
    layers,
    left,
    top,
    width,
    height,
    open_top_spans=(),
    open_bottom_spans=(),
    open_left_spans=(),
    open_right_spans=(),
    extend_top_to_edge=False,
    extend_bottom_to_edge=False,
    extend_left_to_edge=False,
    extend_right_to_edge=False,
):
    if width < 3 or height < 3:
        raise ValueError("cave island must be at least 3x3 tiles")
    if left < 0 or top < 0 or left + width > MAP_W or top + height > MAP_H:
        raise ValueError("cave island does not fit inside map")
    edge_extensions = (
        (extend_top_to_edge, top == 0, open_top_spans, "top"),
        (extend_bottom_to_edge, top + height == MAP_H, open_bottom_spans, "bottom"),
        (extend_left_to_edge, left == 0, open_left_spans, "left"),
        (extend_right_to_edge, left + width == MAP_W, open_right_spans, "right"),
    )
    for enabled, touches_edge, spans, edge_name in edge_extensions:
        if enabled and not touches_edge:
            raise ValueError(f"cave island {edge_name} extension does not touch map edge")
        if enabled and spans:
            raise ValueError(f"cave island {edge_name} extension cannot also use openings")

    right = left + width
    bottom = top + height
    top_open = {
        x
        for span_left, span_width in open_top_spans
        for x in range(span_left, span_left + span_width)
    }
    bottom_open = {
        x
        for span_left, span_width in open_bottom_spans
        for x in range(span_left, span_left + span_width)
    }
    left_open = {
        y
        for span_top, span_height in open_left_spans
        for y in range(span_top, span_top + span_height)
    }
    right_open = {
        y
        for span_top, span_height in open_right_spans
        for y in range(span_top, span_top + span_height)
    }
    if top_open - set(range(left, right)) or bottom_open - set(range(left, right)):
        raise ValueError("cave island horizontal opening leaves island edge")
    if left_open - set(range(top, bottom)) or right_open - set(range(top, bottom)):
        raise ValueError("cave island vertical opening leaves island edge")

    footprint = set()
    walkable = set()
    rim = set()
    for y in range(top, bottom):
        for x in range(left, right):
            index = y * MAP_W + x
            footprint.add((x, y))
            on_top = y == top and not extend_top_to_edge
            on_bottom = y == bottom - 1 and not extend_bottom_to_edge
            on_left = x == left and not extend_left_to_edge
            on_right = x == right - 1 and not extend_right_to_edge
            opening = (
                (on_top and x in top_open)
                or (on_bottom and x in bottom_open)
                or (on_left and y in left_open)
                or (on_right and y in right_open)
            )
            if opening or not (on_top or on_bottom or on_left or on_right):
                layers[0][index] = CAVE_ISLAND_FLOOR_TILE
                layers[1][index] = 0
                walkable.add((x, y))
                continue

            if on_top:
                tile_id = CAVE_BASIN_TOP_TILES[
                    0 if on_left else 2 if on_right else 1
                ]
            elif on_bottom:
                tile_id = CAVE_BASIN_BOTTOM_TILES[
                    0 if on_left else 2 if on_right else 1
                ]
            else:
                tile_id = CAVE_BASIN_SIDE_TILES[0 if on_left else 1]
            layers[0][index] = CAVE_ISLAND_RIM_UNDERLAY_TILE
            layers[1][index] = tile_id
            rim.add((x, y))
    return {"footprint": footprint, "walkable": walkable, "rim": rim}


def stamp_cave_ladder(layers, left, top, kind):
    if kind == "up":
        tile_ids = UP_LADDER_TILES
    else:
        raise ValueError(f"unsupported cave ladder kind: {kind}")
    if left < 0 or left >= MAP_W or top < 0 or top + len(tile_ids) > MAP_H:
        raise ValueError("cave ladder does not fit inside map")

    cells = set()
    for offset, tile_id in enumerate(tile_ids):
        point = (left, top + offset)
        layers[1][point[1] * MAP_W + point[0]] = tile_id
        cells.add(point)
    return cells


def stamp_cave_down_ladder_module(layers, left, top):
    width = len(DOWN_LADDER_BASE_TILES[0])
    height = len(DOWN_LADDER_BASE_TILES)
    if left < 0 or top < 0 or left + width > MAP_W or top + height > MAP_H:
        raise ValueError("cave down-ladder module does not fit inside map")

    footprint = set()
    for offset_y, tile_row in enumerate(CAVE_DOWN_LADDER_RENDER_TILES):
        for offset_x, tile_id in enumerate(tile_row):
            point = (left + offset_x, top + offset_y)
            layers[0][point[1] * MAP_W + point[0]] = tile_id
            footprint.add(point)

    center = (left + 1, top + 1)
    center_index = center[1] * MAP_W + center[0]
    if layers[0][center_index] != DOWN_LADDER_CENTER_TILE:
        raise RuntimeError("cave down-ladder base has the wrong center tile")
    layers[1][center_index] = DOWN_LADDER_TILE
    return {"footprint": footprint, "center": center}


def visible_endpoint_cells(point):
    x, y = point
    if x < 0:
        return {(0, floor(y)), (0, ceil(y))}
    if x > MAP_W - 1:
        return {(MAP_W - 1, floor(y)), (MAP_W - 1, ceil(y))}
    if y < 0:
        return {(floor(x), 0), (ceil(x), 0)}
    if y > MAP_H - 1:
        return {(floor(x), MAP_H - 1), (ceil(x), MAP_H - 1)}
    raise ValueError(f"route endpoint is not outside the map: {point}")


def declared_endpoint_point(spec, endpoint):
    if endpoint == "entry":
        return spec["entry"]
    if endpoint and endpoint.startswith("exit:"):
        exit_index = int(endpoint.split(":", 1)[1])
        return spec["exits"][exit_index]
    raise ValueError(f"invalid cave ladder endpoint: {endpoint}")


def stamp_top_waterfall(
    layers,
    left,
    width,
    body_height,
    left_wall=True,
    right_wall=True,
    left_cliff_to_edge=False,
    right_cliff_to_edge=False,
):
    if width < 3 or body_height < 2:
        raise ValueError("top waterfall must be at least 3x2 tiles")
    bottom_y = body_height
    if left < 0 or left + width > MAP_W or bottom_y >= MAP_H:
        raise ValueError("top waterfall does not fit inside map")
    if (left_wall or left_cliff_to_edge) and left == 0:
        raise ValueError("top waterfall left wall does not fit inside map")
    if (right_wall or right_cliff_to_edge) and left + width == MAP_W:
        raise ValueError("top waterfall right wall does not fit inside map")
    if left_wall and left_cliff_to_edge:
        raise ValueError("top waterfall cannot use a left wall and cliff fill together")
    if right_wall and right_cliff_to_edge:
        raise ValueError("top waterfall cannot use a right wall and cliff fill together")

    def stamp_row(y, tile_row):
        cells = set()
        for column in range(width):
            x = left + column
            tile_id = tile_row[0 if column == 0 else 2 if column == width - 1 else 1]
            layers[0][y * MAP_W + x] = tile_id
            layers[1][y * MAP_W + x] = 0
            cells.add((x, y))
        return cells

    body = set()
    for y in range(body_height):
        tile_row = (
            WATERFALL_BODY_LOWER_TILES
            if y == body_height - 1
            else WATERFALL_BODY_MIDDLE_TILES
        )
        body.update(stamp_row(y, tile_row))
    bottom = stamp_row(bottom_y, (WATERFALL_BOTTOM_TILE,) * 3)

    rim = set()
    side_cliff_footprint = set()
    if left_cliff_to_edge:
        for y in range(bottom_y + 1):
            for x in range(left):
                index = y * MAP_W + x
                side_cliff_footprint.add((x, y))
                tile_id = CAVE_CLIFF_FACE_TILES[
                    0 if x == 0 else 2 if x == left - 1 else 1
                ]
                layers[0][index] = CAVE_ISLAND_RIM_UNDERLAY_TILE
                layers[1][index] = tile_id
                rim.add((x, y))
    if right_cliff_to_edge:
        cliff_left = left + width
        for y in range(bottom_y + 1):
            for x in range(cliff_left, MAP_W):
                index = y * MAP_W + x
                side_cliff_footprint.add((x, y))
                tile_id = CAVE_CLIFF_FACE_TILES[
                    0 if x == cliff_left else 2 if x == MAP_W - 1 else 1
                ]
                layers[0][index] = CAVE_ISLAND_RIM_UNDERLAY_TILE
                layers[1][index] = tile_id
                rim.add((x, y))
    for y in range(bottom_y + 1):
        walls = []
        if left_wall:
            walls.append((left - 1, CAVE_BASIN_SIDE_TILES[0]))
        if right_wall:
            walls.append((left + width, CAVE_BASIN_SIDE_TILES[1]))
        for x, tile_id in walls:
            layers[1][y * MAP_W + x] = tile_id
            rim.add((x, y))
    return {
        "positions": body | bottom,
        "crest": set(),
        "body": body,
        "bottom": bottom,
        "bottom_y": bottom_y,
        "rim": rim,
        "side_cliff_footprint": side_cliff_footprint,
    }


def stamp_vertical_stream(layers, left, width=4, top=0, bottom=MAP_H):
    if width < 3:
        raise ValueError("vertical stream must be at least 3 tiles wide")
    if left < 0 or left + width > MAP_W or top < 0 or bottom > MAP_H or top >= bottom:
        raise ValueError("vertical stream does not fit inside map")

    positions = set()
    for y in range(top, bottom):
        for column in range(width):
            x = left + column
            tile_id = VERTICAL_STREAM_TILES[
                0 if column == 0 else 2 if column == width - 1 else 1
            ]
            layers[0][y * MAP_W + x] = tile_id
            positions.add((x, y))
    return positions


BRIDGE_STREAM_STRAIGHT_MARGIN = 2


def validate_cliff_stair_shoulders(spec):
    cliffs = spec.get("horizontal_cliffs", [])
    for cliff in cliffs:
        stair_left = cliff.get("stair_left")
        if stair_left is None:
            continue
        if stair_left <= cliff["left"] or stair_left + 2 >= cliff["right"]:
            raise RuntimeError(
                f"cliff stairs need level shoulders on both sides: {spec['key']}"
            )
        for adjacent in cliffs:
            if adjacent is cliff or adjacent["top"] == cliff["top"]:
                continue
            if (
                adjacent["right"] == cliff["left"]
                or adjacent["left"] == cliff["right"]
            ):
                raise RuntimeError(
                    "stair cliff cannot adjoin a different elevation: "
                    f"{spec['key']}"
                )


def validate_vertical_stream_transitions(spec):
    streams = spec.get("vertical_streams", [])
    for upstream in streams:
        upstream_bottom = upstream.get("bottom", MAP_H)
        upstream_left = upstream["left"]
        upstream_width = upstream.get("width", 4)
        upstream_right = upstream_left + upstream_width

        for downstream in streams:
            if downstream is upstream or downstream.get("top", 0) != upstream_bottom:
                continue

            downstream_left = downstream["left"]
            downstream_width = downstream.get("width", 4)
            downstream_right = downstream_left + downstream_width
            connected = max(upstream_left, downstream_left) < min(
                upstream_right, downstream_right
            )
            if connected and (
                abs(downstream_left - upstream_left) > 1
                or abs(downstream_right - upstream_right) > 1
            ):
                raise RuntimeError(
                    "stream bank may move at most one tile per transition "
                    f"at row {upstream_bottom}: {spec['key']}"
                )
            if (
                connected
                and upstream_width == downstream_width
                and upstream_left != downstream_left
            ):
                raise RuntimeError(
                    "constant-width stream cannot shift sideways "
                    f"at row {upstream_bottom}: {spec['key']}"
                )


def stamp_vertical_stream_transitions(layers, streams):
    transition_cells = set()
    for upstream in streams:
        boundary = upstream.get("bottom", MAP_H)
        if boundary <= 0 or boundary >= MAP_H:
            continue

        upstream_left = upstream["left"]
        upstream_right = upstream_left + upstream.get("width", 4)
        for downstream in streams:
            if downstream is upstream or downstream.get("top", 0) != boundary:
                continue

            downstream_left = downstream["left"]
            downstream_right = downstream_left + downstream.get("width", 4)
            if max(upstream_left, downstream_left) >= min(
                upstream_right, downstream_right
            ):
                continue

            if downstream_left > upstream_left:
                outer = (upstream_left, boundary - 1)
                inner = (downstream_left, boundary - 1)
                layers[0][outer[1] * MAP_W + outer[0]] = STREAM_BOTTOM_OUTER_LEFT_TILE
                layers[0][inner[1] * MAP_W + inner[0]] = STREAM_BOTTOM_INNER_LEFT_TILE
                transition_cells.update((outer, inner))
            elif downstream_left < upstream_left:
                outer = (downstream_left, boundary)
                inner = (upstream_left, boundary)
                layers[0][outer[1] * MAP_W + outer[0]] = STREAM_TOP_OUTER_LEFT_TILE
                layers[0][inner[1] * MAP_W + inner[0]] = STREAM_TOP_INNER_LEFT_TILE
                transition_cells.update((outer, inner))

            if downstream_right < upstream_right:
                outer = (upstream_right - 1, boundary - 1)
                inner = (downstream_right - 1, boundary - 1)
                layers[0][outer[1] * MAP_W + outer[0]] = STREAM_BOTTOM_OUTER_RIGHT_TILE
                layers[0][inner[1] * MAP_W + inner[0]] = STREAM_BOTTOM_INNER_RIGHT_TILE
                transition_cells.update((outer, inner))
            elif downstream_right > upstream_right:
                outer = (downstream_right - 1, boundary)
                inner = (upstream_right - 1, boundary)
                layers[0][outer[1] * MAP_W + outer[0]] = STREAM_TOP_OUTER_RIGHT_TILE
                layers[0][inner[1] * MAP_W + inner[0]] = STREAM_TOP_INNER_RIGHT_TILE
                transition_cells.update((outer, inner))
    return transition_cells


def validate_bridge_stream_straights(spec, margin=BRIDGE_STREAM_STRAIGHT_MARGIN):
    streams = spec.get("vertical_streams", [])
    bridges = spec.get("horizontal_bridges", [])
    if not streams or not bridges:
        return

    def stream_xs_at(y, expected=None):
        spans = []
        for stream in streams:
            if stream.get("top", 0) <= y < stream.get("bottom", MAP_H):
                span = set(range(stream["left"], stream["left"] + stream["width"]))
                if expected is None or span & expected:
                    spans.append(span)
        return set().union(*spans) if spans else set()

    for bridge in bridges:
        bridge_left = bridge["left"]
        bridge_right = bridge_left + bridge["length"]
        bridge_top = bridge["top"]
        bridge_bottom = bridge_top + bridge.get("height", 2)
        bridge_span = set(range(bridge_left, bridge_right))

        expected = stream_xs_at(bridge_top) & bridge_span
        if not expected:
            continue
        expected = stream_xs_at(bridge_top, expected)

        check_top = max(0, bridge_top - margin)
        check_bottom = min(MAP_H, bridge_bottom + margin)
        for y in range(check_top, check_bottom):
            if stream_xs_at(y, expected) != expected:
                raise RuntimeError(
                    f"stream bends too close to bridge at row {y}: {spec['key']}"
                )


def stamp_horizontal_bridge(layers, left, top, length, height=2, tile_rows=None):
    if length < 3:
        raise ValueError("horizontal bridge must be at least 3 tiles long")
    if height < 2:
        raise ValueError("horizontal bridge must be at least 2 tiles wide")
    if left < 0 or left + length > MAP_W or top < 0 or top + height > MAP_H:
        raise ValueError("horizontal bridge does not fit inside map")

    positions = set()
    if tile_rows is None:
        top_tile, middle_tile, bottom_tile = (
            HORIZONTAL_BRIDGE_TILES[0],
            1635,
            HORIZONTAL_BRIDGE_TILES[1],
        )
    else:
        if len(tile_rows) != 3:
            raise ValueError("horizontal bridge tile_rows must contain top/middle/bottom")
        top_tile, middle_tile, bottom_tile = tile_rows
    bridge_rows = (top_tile, *((middle_tile,) * (height - 2)), bottom_tile)
    for row, tile_id in enumerate(bridge_rows):
        y = top + row
        for x in range(left, left + length):
            layers[2][y * MAP_W + x] = tile_id
            positions.add((x, y))
    return positions


def stamp_horizontal_cliff(layers, top, left, right, stair_left=None):
    if right - left < 2:
        raise ValueError("horizontal cliff must be at least 2 tiles long")
    if left < 0 or right > MAP_W or top < 0 or top + 2 > MAP_H:
        raise ValueError("horizontal cliff does not fit inside map")
    if stair_left is not None and (stair_left < left or stair_left + 2 > right):
        raise ValueError("cliff stairs must fit inside the cliff")

    cliff = set()
    stairs = set()
    for x in range(left, right):
        for y in (top, top + 1):
            point = (x, y)
            cliff.add(point)
            if stair_left is not None and stair_left <= x < stair_left + 2:
                tile_id = CLIFF_STAIR_TILES[x - stair_left]
                layers[0][y * MAP_W + x] = tile_id
                layers[1][y * MAP_W + x] = 0
                stairs.add(point)
            elif y == top:
                layers[0][y * MAP_W + x] = CLIFF_TOP_TILE
                layers[1][y * MAP_W + x] = 0
            else:
                layers[1][y * MAP_W + x] = CLIFF_FACE_TILE
    return cliff, stairs


def stamp_water_rock(layers, left, top, kind="small"):
    if kind == "small":
        tile_rows = ((WATER_ROCK_SMALL_TILE,),)
    elif kind == "large":
        tile_rows = WATER_ROCK_LARGE_TILES
    else:
        raise ValueError(f"unknown water rock kind: {kind}")

    height = len(tile_rows)
    width = len(tile_rows[0])
    if left < 0 or left + width > MAP_W or top < 0 or top + height > MAP_H:
        raise ValueError("water rock does not fit inside map")

    positions = set()
    for row, tile_row in enumerate(tile_rows):
        for column, tile_id in enumerate(tile_row):
            x = left + column
            y = top + row
            layers[2][y * MAP_W + x] = tile_id
            positions.add((x, y))
    return positions


def stamp_top_edge_forest_cluster(layers, left, width=2):
    if width < 2 or width % 2:
        raise ValueError("top-edge forest width must be a positive pair count")
    if left < 0 or left + width > MAP_W:
        raise ValueError("top-edge forest does not fit inside map")

    positions = set()
    for row, tile_pair in enumerate(TOP_EDGE_FOREST_TILE_ROWS):
        for column in range(width):
            x = left + column
            y = row
            layers[0][y * MAP_W + x] = tile_pair[column % 2]
            positions.add((x, y))
    return positions


def stamp_waterfall(layers, left, width, crest_top, body_height=3):
    if width < 3:
        raise ValueError("waterfall must be at least 3 tiles wide")
    if body_height < 2:
        raise ValueError("waterfall body must be at least 2 tiles tall")
    bottom_y = crest_top + body_height + 1
    if (
        left < 0
        or left + width > MAP_W
        or crest_top < 0
        or bottom_y >= MAP_H
    ):
        raise ValueError("waterfall does not fit inside map")

    def stamp_row(y, tile_row):
        cells = set()
        for column in range(width):
            x = left + column
            tile_id = tile_row[0 if column == 0 else 2 if column == width - 1 else 1]
            layers[0][y * MAP_W + x] = tile_id
            cells.add((x, y))
        return cells

    crest = stamp_row(crest_top, WATERFALL_CREST_TILES)
    body = set()
    for row in range(body_height):
        if row == 0:
            tile_row = WATERFALL_BODY_TOP_TILES
        elif row == body_height - 1:
            tile_row = WATERFALL_BODY_LOWER_TILES
        else:
            tile_row = WATERFALL_BODY_MIDDLE_TILES
        body.update(stamp_row(crest_top + 1 + row, tile_row))
    bottom = stamp_row(bottom_y, (WATERFALL_BOTTOM_TILE,) * 3)
    return {
        "positions": crest | body | bottom,
        "crest": crest,
        "body": body,
        "bottom": bottom,
        "bottom_y": bottom_y,
    }


def stamp_waterfall_wall(
    layers,
    top,
    bottom,
    gaps,
    stair_left,
    left=0,
    right=MAP_W,
    terraced=False,
):
    if left < 0 or right > MAP_W or left >= right or top < 0 or bottom > MAP_H:
        raise ValueError("waterfall wall does not fit inside map")
    if bottom - top < 3:
        raise ValueError("waterfall wall must be at least 3 tiles tall")
    if stair_left <= left or stair_left + 2 >= right:
        raise ValueError("waterfall wall stairs need level shoulders")

    gap_cells = {
        (x, y)
        for gap_left, gap_width in gaps
        for y in range(top, bottom)
        for x in range(gap_left, gap_left + gap_width)
    }
    stairs = {
        (x, y)
        for y in range(top, bottom)
        for x in (stair_left, stair_left + 1)
    }
    if gap_cells & stairs:
        raise ValueError("waterfall wall stairs overlap a waterfall gap")

    cliff = set()
    row_segments = []
    if terraced:
        for y in range(top, bottom):
            segments = []
            segment_left = None
            for x in range(left, right + 1):
                point = (x, y)
                occupied = x < right and point not in gap_cells and point not in stairs
                if occupied and segment_left is None:
                    segment_left = x
                elif not occupied and segment_left is not None:
                    segments.append((segment_left, x))
                    segment_left = None
            row_segments.append(segments)

    for y in range(top, bottom):
        segments = row_segments[y - top] if terraced else ()
        for x in range(left, right):
            point = (x, y)
            if point in gap_cells:
                continue
            cliff.add(point)
            if point in stairs:
                layers[0][y * MAP_W + x] = CLIFF_STAIR_TILES[x - stair_left]
                layers[1][y * MAP_W + x] = 0
            elif terraced:
                segment_left, segment_right = next(
                    segment for segment in segments if segment[0] <= x < segment[1]
                )
                is_left = x == segment_left
                is_right = x == segment_right - 1
                if y == bottom - 1:
                    layers[0][y * MAP_W + x] = 1177
                    if is_left and not is_right:
                        layers[1][y * MAP_W + x] = 1184
                    elif is_right and not is_left:
                        layers[1][y * MAP_W + x] = 1186
                    else:
                        layers[1][y * MAP_W + x] = 1185
                elif y == top:
                    layers[0][y * MAP_W + x] = (
                        1187 if is_left else 1189 if is_right else 1188
                    )
                    layers[1][y * MAP_W + x] = 0
                else:
                    layers[0][y * MAP_W + x] = (
                        1192 if is_left else 1193 if is_right else 1188
                    )
                    layers[1][y * MAP_W + x] = 0
            else:
                layers[0][y * MAP_W + x] = CLIFF_TOP_TILE
                layers[1][y * MAP_W + x] = 0
    return cliff, stairs


def validate_route_network(spec):
    routes = spec["routes"]
    exits = spec["exits"]
    if not routes or len(routes) != len(exits):
        raise RuntimeError(f"route/exit count mismatch: {spec['key']}")
    for route, exit_point in zip(routes, exits):
        if route[0] != spec["entry"] or route[-1] != exit_point:
            raise RuntimeError(f"route endpoint mismatch: {spec['key']}")
    if len(routes) > 1:
        shared_prefix = 0
        for points in zip(*routes):
            if any(point != points[0] for point in points[1:]):
                break
            shared_prefix += 1
        if shared_prefix < 2:
            raise RuntimeError(f"multi-exit routes need a shared trunk: {spec['key']}")


def draw_entry(draw, point, edge):
    x, y = center(point)
    color = (78, 220, 117, 255)
    if edge == "top":
        shape = ((x, 2), (x - 8, 12), (x + 8, 12))
    elif edge == "bottom":
        shape = ((x, MAP_H * GAME_TILE - 3), (x - 8, MAP_H * GAME_TILE - 13),
                 (x + 8, MAP_H * GAME_TILE - 13))
    elif edge == "left":
        shape = ((2, y), (12, y - 8), (12, y + 8))
    else:
        shape = ((MAP_W * GAME_TILE - 3, y),
                 (MAP_W * GAME_TILE - 13, y - 8),
                 (MAP_W * GAME_TILE - 13, y + 8))
    draw.polygon(shape, fill=color)


def render_clean_frames(
    layers,
    autotile_names=None,
    frame_count=1,
    tileset_name="Outside",
    tile_source_overrides=None,
):
    if frame_count < 1:
        raise ValueError("frame count must be positive")
    tileset = Image.open(TILESET.with_name(f"{tileset_name}.png")).convert("RGBA")
    autotiles = load_autotiles(autotile_names)
    tile_source_overrides = tile_source_overrides or {}
    external_tilesets = {
        source[0]: Image.open(TILESET.with_name(f"{source[0]}.png")).convert("RGBA")
        for source in tile_source_overrides.values()
    }
    frames = []
    for frame_index in range(frame_count):
        rendered_layers = [
            render_layer(
                layer,
                tileset,
                autotiles,
                frame_index,
                tile_source_overrides,
                external_tilesets,
            )
            for layer in layers
        ]
        combined = Image.new("RGBA", rendered_layers[0].size, (0, 0, 0, 255))
        for layer in rendered_layers:
            combined.alpha_composite(layer)
        frames.append(
            combined.resize(
                (MAP_W * GAME_TILE, MAP_H * GAME_TILE),
                Image.Resampling.NEAREST,
            )
        )
    return frames


def build_map(spec, run_seed, return_generation_data=False):
    validate_route_network(spec)
    validate_cliff_stair_shoulders(spec)
    validate_bridge_stream_straights(spec)
    validate_vertical_stream_transitions(spec)
    layers = [[0] * (MAP_W * MAP_H) for _ in range(3)]
    rng = random.Random(run_seed + spec["seed_offset"])
    land_cells = set()
    water_cells = set()
    base_tile_ids = spec.get("base_tile_ids")
    if spec.get("sea_base"):
        for index in range(MAP_W * MAP_H):
            layers[0][index] = SEA_WATER_TILE
        water_cells.update(
            (x, y) for y in range(MAP_H) for x in range(MAP_W)
        )
    elif base_tile_ids:
        for index in range(MAP_W * MAP_H):
            layers[0][index] = rng.choice(base_tile_ids)
        land_cells.update(
            (x, y) for y in range(MAP_H) for x in range(MAP_W)
        )
    elif spec.get("water_base"):
        land_cells = land_cells_from_rows(spec.get("land_rows", ()))
        water_cells.update(stamp_water_backdrop(layers, land_cells, rng))
    else:
        grass_ids = [385, 385, 385, 386, 387, 388, 389]
        for index in range(MAP_W * MAP_H):
            layers[0][index] = rng.choice(grass_ids)
        land_cells.update(
            (x, y) for y in range(MAP_H) for x in range(MAP_W)
        )

    road_tiles = build_two_tile_road(spec["routes"], MAP_W, MAP_H)
    path_cells = set(road_tiles)

    island_footprint_cells = set()
    cave_island_footprints = []
    island_rim_cells = set()
    for island in spec.get("cave_islands", []):
        result = stamp_cave_island(layers, **island)
        cave_island_footprints.append(set(result["footprint"]))
        island_footprint_cells.update(result["footprint"])
        island_rim_cells.update(result["rim"])
        land_cells.update(result["walkable"])
        water_cells.difference_update(result["footprint"])

    for coast_x in spec.get("left_coasts", []):
        water_cells.update(stamp_left_coast(layers, MAP_W, MAP_H, coast_x))
    for left, top in spec.get("ponds", []):
        water_cells.update(add_pond(layers, left, top))
    for left, top, width, height in spec.get("water_rects", []):
        water_cells.update(add_water_rect(layers, left, top, width, height))
    for channel in spec.get("sea_channels", []):
        water_cells.update(stamp_sea_channel(layers, **channel))
    upstream_water_cells = set()
    downstream_water_cells = set()
    basin_rim_cells = set()
    for basin in spec.get("cave_basins", []):
        basin_args = {key: value for key, value in basin.items() if key != "role"}
        result = stamp_cave_basin(layers, **basin_args)
        water_cells.update(result["water"])
        basin_rim_cells.update(result["rim"])
        if basin.get("role") == "upstream":
            upstream_water_cells.update(result["water"])
        elif basin.get("role") == "downstream":
            downstream_water_cells.update(result["water"])
    vertical_streams = spec.get("vertical_streams", [])
    for stream in vertical_streams:
        water_cells.update(stamp_vertical_stream(layers, **stream))
    stream_transition_cells = stamp_vertical_stream_transitions(layers, vertical_streams)
    waterfall_cells = set()
    waterfall_crest_cells = set()
    waterfall_body_cells = set()
    waterfall_bottom_cells = set()
    waterfall_results = []
    for waterfall in spec.get("waterfalls", []):
        result = stamp_waterfall(layers, **waterfall)
        waterfall_results.append(result)
        waterfall_cells.update(result["positions"])
        waterfall_crest_cells.update(result["crest"])
        waterfall_body_cells.update(result["body"])
        waterfall_bottom_cells.update(result["bottom"])
    top_waterfall_rim_cells = set()
    top_waterfall_side_cliff_cells = set()
    for waterfall in spec.get("top_waterfalls", []):
        result = stamp_top_waterfall(layers, **waterfall)
        waterfall_cells.update(result["positions"])
        waterfall_crest_cells.update(result["crest"])
        waterfall_body_cells.update(result["body"])
        waterfall_bottom_cells.update(result["bottom"])
        top_waterfall_rim_cells.update(result["rim"])
        top_waterfall_side_cliff_cells.update(result["side_cliff_footprint"])
        water_cells.difference_update(result["side_cliff_footprint"])
    water_cells.update(waterfall_cells)
    stream_transition_scenery_exclusion = {
        (x + dx, y + dy)
        for x, y in stream_transition_cells
        for dx in (-1, 0, 1)
        for dy in (-1, 0, 1)
        if 0 <= x + dx < MAP_W and 0 <= y + dy < MAP_H
    }

    cliff_cells = set(basin_rim_cells | top_waterfall_rim_cells | island_rim_cells)
    stair_cells = set()
    for cliff in spec.get("horizontal_cliffs", []):
        cliff, stairs = stamp_horizontal_cliff(layers, **cliff)
        cliff_cells.update(cliff)
        stair_cells.update(stairs)
    for wall in spec.get("waterfall_walls", []):
        cliff, stairs = stamp_waterfall_wall(layers, **wall)
        cliff_cells.update(cliff)
        stair_cells.update(stairs)
    if spec.get("water_base") or spec.get("sea_base"):
        water_cells.difference_update(cliff_cells)
        water_cells.update(waterfall_cells)
    if base_tile_ids:
        land_cells.difference_update(water_cells | (cliff_cells - stair_cells))

    if spec.get("sea_base"):
        if island_footprint_cells & waterfall_cells:
            raise RuntimeError(f"rock island receives waterfall: {spec['key']}")
        if water_cells:
            connected_water = set()
            queue = deque([next(iter(water_cells))])
            while queue:
                point = queue.popleft()
                if point in connected_water or point not in water_cells:
                    continue
                connected_water.add(point)
                x, y = point
                queue.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
            if connected_water != water_cells:
                raise RuntimeError(f"sea base was split into disconnected pools: {spec['key']}")
        for waterfall in spec.get("top_waterfalls", []):
            clearance = spec.get("waterfall_water_clearance", 1)
            for offset in range(1, clearance + 1):
                below_y = waterfall["body_height"] + offset
                for x in range(waterfall["left"], waterfall["left"] + waterfall["width"]):
                    if (x, below_y) not in water_cells:
                        raise RuntimeError(
                            f"waterfall does not empty directly into sea: {spec['key']}"
                        )

    bridge_cells = set()
    bridge_groups = []
    for bridge in spec.get("horizontal_bridges", []):
        group = stamp_horizontal_bridge(layers, **bridge)
        bridge_groups.append(group)
        bridge_cells.update(group)
    if cave_island_footprints:
        for group in bridge_groups:
            touched_islands = sum(bool(group & footprint) for footprint in cave_island_footprints)
            if touched_islands < 2 or not group & water_cells:
                raise RuntimeError(f"bridge does not connect two rock islands: {spec['key']}")

    water_rock_cells = set()
    for water_rock in spec.get("water_rocks", []):
        water_rock_cells.update(stamp_water_rock(layers, **water_rock))

    for waterfall, result in zip(spec.get("waterfalls", []), waterfall_results):
        crest_y = waterfall["crest_top"]
        bottom_y = result["bottom_y"]
        for x in range(waterfall["left"], waterfall["left"] + waterfall["width"]):
            if crest_y > 0 and (x, crest_y - 1) not in water_cells:
                raise RuntimeError(f"waterfall has no upstream water: {spec['key']}")
            if bottom_y + 1 < MAP_H and (x, bottom_y + 1) not in water_cells:
                raise RuntimeError(f"waterfall has no downstream basin: {spec['key']}")

    if upstream_water_cells and downstream_water_cells:
        remaining = set(water_cells) - waterfall_cells
        unseen = set(remaining)
        while unseen:
            start = unseen.pop()
            component = {start}
            queue = deque([start])
            while queue:
                x, y = queue.popleft()
                for neighbor in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if neighbor in unseen:
                        unseen.remove(neighbor)
                        component.add(neighbor)
                        queue.append(neighbor)
            if component & upstream_water_cells and component & downstream_water_cells:
                raise RuntimeError(
                    f"upstream and downstream water connect around waterfall: {spec['key']}"
                )

    if bridge_cells and not bridge_cells & water_cells:
        raise RuntimeError(f"bridge does not cross water: {spec['key']}")
    if bridge_cells and not bridge_cells & path_cells:
        raise RuntimeError(f"bridge does not carry a route: {spec['key']}")
    if water_rock_cells - water_cells:
        raise RuntimeError(f"water rock outside water: {spec['key']}")
    if water_rock_cells & (bridge_cells | path_cells):
        raise RuntimeError(f"water rock overlaps bridge or route: {spec['key']}")
    if water_rock_cells & waterfall_cells:
        raise RuntimeError(f"water rock overlaps waterfall: {spec['key']}")
    if water_rock_cells & stream_transition_scenery_exclusion:
        raise RuntimeError(f"water rock too close to stream transition: {spec['key']}")
    if (water_cells & path_cells) - bridge_cells:
        raise RuntimeError(f"water overlaps route without a bridge: {spec['key']}")
    if stair_cells - path_cells:
        raise RuntimeError(f"cliff stairs outside route envelope: {spec['key']}")
    if (cliff_cells & path_cells) - (stair_cells | bridge_cells):
        raise RuntimeError(f"cliff overlaps route without stairs: {spec['key']}")
    if waterfall_cells & path_cells:
        raise RuntimeError(f"waterfall overlaps route: {spec['key']}")
    if waterfall_cells & cliff_cells:
        raise RuntimeError(f"waterfall gap was filled by cliff: {spec['key']}")
    if spec.get("water_base") or spec.get("sea_base"):
        covered_route = land_cells | bridge_cells | stair_cells
        if path_cells - covered_route:
            raise RuntimeError(f"water-base route leaves its islands: {spec['key']}")

    for left, top, width, height in spec.get("dense", []):
        for y in range(top, top + height):
            for x in range(left, left + width):
                if (
                    (x, y) not in path_cells
                    and (x, y) not in water_cells
                    and (x, y) not in cliff_cells
                ):
                    layers[0][y * MAP_W + x] = 390

    route_tile_id = spec.get("route_tile_id")
    hide_land_route = spec.get("hide_land_route", False)
    for (x, y), tile_id in road_tiles.items():
        if (
            not hide_land_route
            and (x, y) not in water_cells
            and (x, y) not in cliff_cells
        ):
            layers[0][y * MAP_W + x] = route_tile_id or tile_id

    cave_ladder_cells = set()
    for ladder in spec.get("cave_ladders", []):
        endpoint = ladder.get("endpoint")
        endpoint_point = declared_endpoint_point(spec, endpoint)
        ladder_args = {
            key: value
            for key, value in ladder.items()
            if key != "endpoint"
        }
        cells = stamp_cave_ladder(layers, **ladder_args)
        if cells - (land_cells & path_cells):
            raise RuntimeError(
                f"cave ladder must stay on a walkable route: {spec['key']}"
            )
        if cells & (water_cells | cliff_cells | bridge_cells):
            raise RuntimeError(
                f"cave ladder overlaps water, cliff, or bridge: {spec['key']}"
            )
        if not any(
            x in (0, MAP_W - 1) or y in (0, MAP_H - 1)
            for x, y in cells
        ):
            raise RuntimeError(
                f"cave route ladder must touch a map boundary: {spec['key']}"
            )
        if cells.isdisjoint(visible_endpoint_cells(endpoint_point)):
            raise RuntimeError(
                f"cave ladder misses its declared endpoint: {spec['key']}"
            )
        cave_ladder_cells.update(cells)

    cave_down_ladder_module_cells = set()
    for module in spec.get("cave_down_ladder_modules", []):
        endpoint = module.get("endpoint")
        endpoint_point = declared_endpoint_point(spec, endpoint)
        module_args = {
            key: value
            for key, value in module.items()
            if key != "endpoint"
        }
        result = stamp_cave_down_ladder_module(layers, **module_args)
        footprint = result["footprint"]
        ladder_center = result["center"]
        if footprint - land_cells:
            raise RuntimeError(
                f"cave down-ladder module must stay on walkable land: {spec['key']}"
            )
        if footprint & (water_cells | cliff_cells | bridge_cells):
            raise RuntimeError(
                f"cave down-ladder module overlaps water, cliff, or bridge: {spec['key']}"
            )
        if ladder_center not in path_cells:
            raise RuntimeError(
                f"cave down-ladder center must stay on a route: {spec['key']}"
            )
        if footprint.isdisjoint(visible_endpoint_cells(endpoint_point)):
            raise RuntimeError(
                f"cave down-ladder module misses its declared endpoint: {spec['key']}"
            )
        cave_down_ladder_module_cells.update(footprint)
        cave_ladder_cells.add(ladder_center)

    forest_footprints = set()
    for forest_x, top_y, forest_width, bottom_y in spec.get("forest_stamps", []):
        forest_footprints.update(
            (x, y)
            for y in range(top_y, bottom_y + 1)
            for x in range(forest_x - 1, forest_x + forest_width)
        )
    lighthouse_footprints = set()
    for left, top in spec.get("lighthouses", []):
        lighthouse_footprints.update(
            (x, y)
            for y in range(top, top + 6)
            for x in range(left, left + 3)
        )
    ancient_tree_cells = set()
    if spec.get("ancient_tree_clusters"):
        raise RuntimeError(
            f"truncated forest modules are only allowed at the top edge: {spec['key']}"
        )
    for cluster in spec.get("top_edge_forest_clusters", []):
        if cluster.get("top", 0) != 0:
            raise RuntimeError(
                f"truncated forest modules are only allowed at the top edge: {spec['key']}"
            )
        width = cluster.get("width", 2)
        cluster_cells = {
            (x, y)
            for y in range(3)
            for x in range(cluster["left"], cluster["left"] + width)
        }
        if cluster_cells & (path_cells | water_cells | cliff_cells):
            raise RuntimeError(
                f"top-edge forest overlaps route, water, or cliff: {spec['key']}"
            )
        ancient_tree_cells.update(
            stamp_top_edge_forest_cluster(
                layers,
                left=cluster["left"],
                width=width,
            )
        )
    boulder_cells = set()
    for x, y, tile_id in spec.get("boulders", []):
        if not (0 <= x < MAP_W and 0 <= y < MAP_H):
            raise ValueError(f"boulder outside map: {(x, y)}")
        if (x, y) in path_cells or (x, y) in water_cells or (x, y) in cliff_cells:
            raise RuntimeError(f"boulder overlaps route, water, or cliff: {spec['key']}")
        if (x, y) in ancient_tree_cells:
            raise RuntimeError(f"boulder overlaps ancient tree: {spec['key']}")
        if (x, y) in stream_transition_scenery_exclusion:
            raise RuntimeError(f"boulder too close to stream transition: {spec['key']}")
        layers[2][y * MAP_W + x] = tile_id
        boulder_cells.add((x, y))
    blocked_structures = (water_cells - bridge_cells) | (cliff_cells - stair_cells)
    solid_scenery = (
        blocked_structures
        | forest_footprints
        | lighthouse_footprints
        | ancient_tree_cells
        | boulder_cells
        | water_rock_cells
    )
    if (forest_footprints | lighthouse_footprints) & path_cells:
        raise RuntimeError(f"solid scenery overlaps route: {spec['key']}")

    for x, y in spec.get("flowers", []):
        if (x, y) not in path_cells and (x, y) not in solid_scenery:
            layers[0][y * MAP_W + x] = 415

    expected_high_grass_positions = set()
    high_grass_blocked = path_cells | solid_scenery
    for left, top, width, height in spec.get("high_grass", []):
        expected_high_grass_positions.update(
            stamp_high_grass(
                layers,
                MAP_W,
                MAP_H,
                left,
                top,
                width,
                height,
                blocked_cells=high_grass_blocked,
            )
        )

    expected_forest_positions = set()
    expected_crown_positions = set()
    for forest_x, top_y, forest_width, bottom_y in spec.get("forest_stamps", []):
        stamp_forest_fence(
            layers,
            MAP_W,
            MAP_H,
            forest_x=forest_x,
            top_y=top_y,
            forest_width=forest_width,
            bottom_y=bottom_y,
        )
        expected_forest_positions.update(
            (x, y)
            for y in range(top_y + 1, bottom_y + 1)
            for x in range(forest_x, forest_x + forest_width)
        )
        expected_crown_positions.update(
            (x, top_y) for x in range(forest_x, forest_x + forest_width)
        )

    expected_lighthouse_positions = set()
    for left, top in spec.get("lighthouses", []):
        expected_lighthouse_positions.update(
            stamp_lighthouse(layers, MAP_W, MAP_H, left=left, top=top)["positions"]
        )

    actual_forest_positions = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[0])
        if tile_id in FOREST_BODY_IDS
    }
    actual_crown_positions = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[2])
        if tile_id in FOREST_CROWN_IDS
    }
    if actual_forest_positions != expected_forest_positions | ancient_tree_cells:
        raise RuntimeError(f"forest body outside approved templates: {spec['key']}")
    if actual_crown_positions != expected_crown_positions:
        raise RuntimeError(f"forest crown outside template: {spec['key']}")
    actual_high_grass_positions = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[1])
        if tile_id == HIGH_GRASS_TILE_ID
    }
    if actual_high_grass_positions != expected_high_grass_positions:
        raise RuntimeError(f"high grass outside template: {spec['key']}")
    actual_lighthouse_positions = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[1])
        if tile_id in LIGHTHOUSE_IDS
    }
    if actual_lighthouse_positions != expected_lighthouse_positions:
        raise RuntimeError(f"lighthouse outside template: {spec['key']}")

    route_road_cells = two_tile_road_cells(spec["routes"], MAP_W, MAP_H)
    if set(road_tiles) != route_road_cells:
        raise RuntimeError(f"road outside multi-exit route envelope: {spec['key']}")
    expected_road_cells = route_road_cells - water_cells - cliff_cells
    if hide_land_route:
        rendered_road_cells = expected_road_cells
    elif route_tile_id:
        rendered_road_cells = {
            (index % MAP_W, index // MAP_W)
            for index, tile_id in enumerate(layers[0])
            if tile_id == route_tile_id
        }
    else:
        rendered_road_cells = {
            (index % MAP_W, index // MAP_W)
            for index, tile_id in enumerate(layers[0])
            if tile_id in ROAD_TILE_IDS
        }
    if rendered_road_cells != expected_road_cells:
        raise RuntimeError(f"multi-exit road was overwritten or expanded: {spec['key']}")
    rendered_route_cells = rendered_road_cells | (bridge_cells & path_cells) | stair_cells
    if rendered_route_cells != route_road_cells:
        raise RuntimeError(f"route is not fully covered by road, bridge, or stairs: {spec['key']}")

    clean = render_clean_frames(
        layers,
        spec.get("autotile_names"),
        tileset_name=spec.get("tileset_name", "Outside"),
        tile_source_overrides=spec.get("tile_source_overrides"),
    )[0]

    review = clean.copy()
    draw = ImageDraw.Draw(review, "RGBA")
    for route_index, route in enumerate(spec["routes"]):
        route_color = ROUTE_COLORS[route_index % len(ROUTE_COLORS)]
        draw.line([center(point) for point in route], fill=route_color, width=4)
        for point in sample_route(route):
            x, y = center(point)
            draw.ellipse(
                (x - 3, y - 3, x + 3, y + 3),
                fill=(255, 255, 255, 230),
                outline=route_color,
            )
    draw_entry(draw, spec["entry"], spec["entry_edge"])
    for exit_point in spec["exits"]:
        exit_x, exit_y = center(exit_point)
        draw.ellipse(
            (exit_x - 8, exit_y - 8, exit_x + 8, exit_y + 8),
            outline=(255, 196, 76, 255),
            width=3,
        )

    if return_generation_data:
        return clean, review, {
            "layers": layers,
            "land_cells": sorted(land_cells),
            "road_cells": sorted(path_cells),
            "water_cells": sorted(water_cells),
            "bridge_cells": sorted(bridge_cells),
            "cliff_cells": sorted(cliff_cells),
            "stair_cells": sorted(stair_cells),
            "cave_ladder_cells": sorted(cave_ladder_cells),
            "cave_down_ladder_module_cells": sorted(cave_down_ladder_module_cells),
            "boulder_cells": sorted(boulder_cells),
            "water_rock_cells": sorted(water_rock_cells),
            "waterfall_cells": sorted(waterfall_cells),
            "waterfall_crest_cells": sorted(waterfall_crest_cells),
            "waterfall_body_cells": sorted(waterfall_body_cells),
            "waterfall_bottom_cells": sorted(waterfall_bottom_cells),
            "upstream_water_cells": sorted(upstream_water_cells),
            "downstream_water_cells": sorted(downstream_water_cells),
            "island_footprint_cells": sorted(island_footprint_cells),
            "top_waterfall_side_cliff_cells": sorted(top_waterfall_side_cliff_cells),
            "ancient_tree_cells": sorted(ancient_tree_cells),
            "stream_transition_cells": sorted(stream_transition_cells),
            "solid_scenery": sorted(solid_scenery),
        }
    return clean, review


def make_overview(images, specs, run_seed, filename, route_view, output_dir):
    gap = 8
    footer = 76
    cell_w = MAP_W * GAME_TILE
    cell_h = MAP_H * GAME_TILE
    canvas = Image.new(
        "RGBA",
        (cell_w * 2 + gap * 3, cell_h * 2 + gap * 3 + footer),
        (24, 32, 35, 255),
    )
    draw = ImageDraw.Draw(canvas, "RGBA")
    positions = (
        (gap, gap),
        (cell_w + gap * 2, gap),
        (gap, cell_h + gap * 2),
        (cell_w + gap * 2, cell_h + gap * 2),
    )
    for index, position in enumerate(positions):
        x, y = position
        if index < len(images):
            canvas.alpha_composite(images[index], position)
            draw.rectangle((x + 4, y + 4, x + 62, y + 21), fill=(20, 28, 30, 215))
            draw.text((x + 10, y + 7), f"MAP {index + 1}", fill=(255, 255, 255, 255))
        else:
            draw.rectangle(
                (x, y, x + cell_w - 1, y + cell_h - 1),
                fill=(31, 43, 46, 255),
                outline=(74, 94, 96, 255),
            )

    footer_y = cell_h * 2 + gap * 3
    draw.text((gap + 4, footer_y + 12), f"SEED {run_seed}   LENGTH {len(specs)}", fill=(255, 216, 72, 255))
    draw.text(
        (gap + 4, footer_y + 34),
        "  ->  ".join(
            f"MAP {index + 1} {len(spec['exits'])} EXITS"
            for index, spec in enumerate(specs)
        ),
        fill=(110, 177, 255, 255),
    )
    if route_view:
        draw.text(
            (gap + 4, footer_y + 54),
            "ALL ROUTES REACH EXITS   ROAD COVERAGE 100%",
            fill=(190, 206, 204, 255),
        )
    canvas.save(output_dir / filename)


def main():
    parser = argparse.ArgumentParser(description="Generate a 2-4 map StickMon typical run preview")
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/stickmon-typical-map-run"))
    parser.add_argument("--seed", type=int, default=20260713)
    parser.add_argument("--count", type=int, choices=range(2, 5))
    args = parser.parse_args()

    run_count = args.count if args.count else random.Random(args.seed).randint(2, 4)
    specs = MAP_SPECS[:run_count]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    clean_maps = []
    route_maps = []
    for spec in specs:
        clean, routes = build_map(spec, args.seed)
        clean.save(args.output_dir / f"{spec['key']}_clean.png")
        routes.save(args.output_dir / f"{spec['key']}_routes.png")
        clean_maps.append(clean)
        route_maps.append(routes)

    make_overview(
        clean_maps,
        specs,
        args.seed,
        "typical_run_clean_overview.png",
        False,
        args.output_dir,
    )
    make_overview(
        route_maps,
        specs,
        args.seed,
        "typical_run_routes_overview.png",
        True,
        args.output_dir,
    )
    print(f"seed={args.seed} maps={run_count}")
    print(args.output_dir / "typical_run_clean_overview.png")
    print(args.output_dir / "typical_run_routes_overview.png")


if __name__ == "__main__":
    main()
