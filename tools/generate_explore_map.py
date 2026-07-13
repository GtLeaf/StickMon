#!/usr/bin/env python3
import argparse
import json
import os
import struct
import subprocess
from collections import deque
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ESSENTIALS = Path(os.environ.get(
    "ESSENTIALS_DIR",
    "${ESSENTIALS_DIR}",
))
EXPORTER = ROOT / "tools" / "export_rmxp_map.rb"
OUTPUT_DIR = ROOT / "origin_asset" / "generated" / "game"
MAP_DATA_DIR = OUTPUT_DIR / "maps"
DEBUG_DIR = OUTPUT_DIR / "debug"
FULL_PREVIEW = OUTPUT_DIR / "explore_route1_full.png"
ROUTE_PREVIEW = OUTPUT_DIR / "explore_maps_routes.png"
SEMANTICS_PREVIEW = DEBUG_DIR / "explore_maps_semantics.png"

TILE_SIZE = 32
GAME_TILE_SIZE = 26
MAP_TILES_W = 16
MAP_TILES_H = 12

SMONMAP_MAGIC = b"SMAP"
SMONMAP_VERSION = 1
SMONMAP_CELL_STRIDE = 4
SMONMAP_HEADER = struct.Struct("<4sBBBBHBBBBBB")

FLAG_ENTRY = 1 << 0
FLAG_EXIT = 1 << 1
FLAG_WATER = 1 << 2
FLAG_FLOATABLE = 1 << 3
FLAG_ROUTE = 1 << 4
FLAG_EVENT = 1 << 5

TERRAIN_TAG_NAMES = {
    0: "None",
    1: "Ledge",
    2: "Grass",
    3: "Sand",
    4: "Rock",
    5: "DeepWater",
    6: "StillWater",
    7: "Water",
    8: "Waterfall",
    9: "WaterfallCrest",
    10: "TallGrass",
    11: "UnderwaterGrass",
    12: "Ice",
    13: "Neutral",
    14: "SootGrass",
    15: "Bridge",
    16: "Puddle",
    17: "NoEffect",
}
WATER_TERRAIN_TAGS = frozenset((5, 6, 7, 8, 9))
FLOATABLE_TERRAIN_TAGS = frozenset((5, 6, 7))
IGNORE_PASSABILITY_TAG = 13

# RPG Maker XP direction passage bits: down, left, right, up.
DIRECTIONS = (
    (0, 1, 0x01, 0x08),
    (-1, 0, 0x02, 0x04),
    (1, 0, 0x04, 0x02),
    (0, -1, 0x08, 0x01),
)

DEBUG_FONT_PATHS = (
    Path("/Library/Fonts/Arial Unicode.ttf"),
    Path("/System/Library/Fonts/STHeiti Medium.ttc"),
)

MAP_SPECS = [
    {
        "key": "route1",
        "label": "Route 1",
        "map_id": 5,
        "crop": (12, 6),
        "output": "explore_route1_416.png",
        "entry": (8, 0),
        "routes": [
            {
                "exit": (7, 11),
                "route": [
                    (8, 1), (8, 2), (9, 2), (10, 2), (11, 2), (12, 2),
                    (12, 3), (12, 4), (12, 5), (12, 6), (12, 7), (12, 8),
                    (11, 8), (10, 8), (9, 8), (8, 8), (7, 8), (7, 9),
                    (7, 10), (7, 11),
                ],
            },
        ],
    },
    {
        "key": "route2",
        "label": "Route 2",
        "map_id": 21,
        "crop": (14, 64),
        "output": "explore_route2_416.png",
        "entry": (6, 0),
        "routes": [
            {
                "exit": (6, 11),
                "route": [
                    (6, 1), (6, 2), (6, 3), (6, 4), (6, 5), (6, 6),
                    (5, 6), (4, 6), (3, 6), (3, 7), (4, 7), (5, 7),
                    (6, 7), (6, 8), (6, 9), (6, 10), (6, 11),
                ],
            },
            {
                "exit": (0, 6),
                "route": [
                    (6, 1), (6, 2), (6, 3), (6, 4), (6, 5), (6, 6),
                    (5, 6), (4, 6), (3, 6), (2, 6), (1, 6), (0, 6),
                ],
            },
        ],
    },
    {
        "key": "natural_park",
        "label": "Natural Park",
        "map_id": 28,
        "crop": (14, 17),
        "output": "explore_natural_park_416.png",
        "entry": (0, 6),
        "routes": [
            {
                "exit": (6, 11),
                "route": [
                    (1, 6), (2, 6), (3, 6), (4, 6), (5, 6), (6, 6),
                    (6, 7), (6, 8), (6, 9), (6, 10), (6, 11),
                ],
            },
            {
                "exit": (6, 0),
                "route": [
                    (1, 6), (2, 6), (3, 6), (4, 6), (5, 6), (6, 6),
                    (6, 5), (6, 4), (6, 3), (6, 2), (6, 1), (6, 0),
                ],
            },
        ],
    },
    {
        "key": "route6",
        "label": "Route 6",
        "map_id": 44,
        "crop": (7, 6),
        "output": "explore_route6_416.png",
        "entry": (3, 0),
        "routes": [
            {
                "exit": (15, 10),
                "route": [
                    (3, 1), (3, 2), (3, 3), (3, 4), (3, 5), (3, 6),
                    (3, 7), (3, 8), (3, 9), (3, 10), (4, 10), (5, 10),
                    (6, 10), (7, 10), (8, 10), (9, 10), (10, 10),
                    (11, 10), (12, 10), (13, 10), (14, 10), (15, 10),
                ],
            },
            {
                "exit": (3, 11),
                "route": [
                    (3, 1), (3, 2), (3, 3), (3, 4), (3, 5), (3, 6),
                    (3, 7), (3, 8), (3, 9), (3, 10), (3, 11),
                ],
            },
        ],
    },
]

# RPG Maker XP builds each autotile variant from four 16x16 source quarters.
AUTOTILE_PARTS = [
    [26, 27, 32, 33], [4, 27, 32, 33], [26, 5, 32, 33], [4, 5, 32, 33],
    [26, 27, 32, 11], [4, 27, 32, 11], [26, 5, 32, 11], [4, 5, 32, 11],
    [26, 27, 10, 33], [4, 27, 10, 33], [26, 5, 10, 33], [4, 5, 10, 33],
    [26, 27, 10, 11], [4, 27, 10, 11], [26, 5, 10, 11], [4, 5, 10, 11],
    [24, 25, 30, 31], [24, 5, 30, 31], [24, 25, 30, 11], [24, 5, 30, 11],
    [14, 15, 20, 21], [14, 15, 20, 11], [14, 15, 10, 21], [14, 15, 10, 11],
    [28, 29, 34, 35], [28, 29, 10, 35], [4, 29, 34, 35], [4, 29, 10, 35],
    [22, 23, 28, 29], [22, 23, 28, 11], [22, 23, 10, 29], [22, 23, 10, 11],
    [14, 17, 20, 23], [14, 17, 20, 11], [14, 17, 10, 23], [14, 17, 10, 11],
    [12, 13, 18, 19], [12, 13, 18, 11], [12, 17, 18, 19], [12, 17, 18, 11],
    [16, 17, 22, 23], [16, 17, 22, 11], [16, 13, 22, 23], [16, 13, 22, 11],
    [0, 1, 6, 7], [0, 1, 6, 11], [0, 5, 6, 7], [0, 5, 6, 11],
]


def export_map(map_id):
    payload = subprocess.check_output([
        "ruby", str(EXPORTER), str(ESSENTIALS), str(map_id),
    ])
    return json.loads(payload)


def load_image(path):
    if not path.exists():
        raise FileNotFoundError(path)
    return Image.open(path).convert("RGBA")


def regular_tile(tileset, tile_id):
    index = tile_id - 384
    if index < 0:
        return None
    x = (index % 8) * TILE_SIZE
    y = (index // 8) * TILE_SIZE
    if x + TILE_SIZE > tileset.width or y + TILE_SIZE > tileset.height:
        return None
    return tileset.crop((x, y, x + TILE_SIZE, y + TILE_SIZE))


def autotile_frame(source):
    if source.height == TILE_SIZE:
        return source.crop((0, 0, TILE_SIZE, TILE_SIZE))
    if source.width < 96 or source.height < 128:
        return source.crop((0, 0, min(TILE_SIZE, source.width), min(TILE_SIZE, source.height)))
    return source.crop((0, 0, 96, 128))


def autotile_variant(source, variant):
    frame = autotile_frame(source)
    if frame.size == (TILE_SIZE, TILE_SIZE):
        return frame
    tile = Image.new("RGBA", (TILE_SIZE, TILE_SIZE), (0, 0, 0, 0))
    for quadrant, part in enumerate(AUTOTILE_PARTS[variant % 48]):
        sx = (part % 6) * 16
        sy = (part // 6) * 16
        quarter = frame.crop((sx, sy, sx + 16, sy + 16))
        tile.alpha_composite(quarter, ((quadrant % 2) * 16, (quadrant // 2) * 16))
    return tile


def render_map(data):
    graphics = ESSENTIALS / "Graphics"
    tileset = load_image(graphics / "Tilesets" / f"{data['tilesetName']}.png")
    autotiles = []
    for name in data["autotileNames"]:
        autotiles.append(load_image(graphics / "Autotiles" / f"{name}.png") if name else None)

    width = data["width"]
    height = data["height"]
    canvas = Image.new("RGBA", (width * TILE_SIZE, height * TILE_SIZE), (0, 0, 0, 0))
    tile_cache = {}
    for layer in data["layers"]:
        for index, tile_id in enumerate(layer):
            if tile_id < 48:
                continue
            tile = tile_cache.get(tile_id)
            if tile_id not in tile_cache:
                if tile_id < 384:
                    autotile_index = tile_id // 48 - 1
                    source = autotiles[autotile_index] if 0 <= autotile_index < len(autotiles) else None
                    tile = autotile_variant(source, tile_id % 48) if source else None
                else:
                    tile = regular_tile(tileset, tile_id)
                tile_cache[tile_id] = tile
            if tile:
                x = (index % width) * TILE_SIZE
                y = (index // width) * TILE_SIZE
                canvas.alpha_composite(tile, (x, y))
    return canvas


def choose_crop(image, tile_x, tile_y, tiles_w, tiles_h):
    crop_w = tiles_w * TILE_SIZE
    crop_h = tiles_h * TILE_SIZE
    x = tile_x * TILE_SIZE
    y = tile_y * TILE_SIZE
    if x < 0 or y < 0 or x + crop_w > image.width or y + crop_h > image.height:
        raise ValueError(
            f"map crop outside source: crop={x},{y},{crop_w},{crop_h} source={image.size}"
        )
    crop = image.crop((x, y, x + crop_w, y + crop_h))
    game_size = (tiles_w * GAME_TILE_SIZE, tiles_h * GAME_TILE_SIZE)
    return crop.resize(game_size, Image.Resampling.NEAREST), (x, y, crop_w, crop_h)


def table_value(table, index, default=0):
    return table[index] if 0 <= index < len(table) else default


def tile_ids_at(data, x, y):
    index = y * data["width"] + x
    return [layer[index] for layer in reversed(data["layers"])]


def effective_terrain_tag(data, x, y):
    terrain_tags = data["terrainTags"]
    for tile_id in tile_ids_at(data, x, y):
        if tile_id == 0:
            continue
        terrain_tag = table_value(terrain_tags, tile_id)
        if terrain_tag not in (0, IGNORE_PASSABILITY_TAG):
            return terrain_tag
    return 0


def blocked_mask_for_cell(data, x, y, allow_float=False):
    passages = data["passages"]
    priorities = data["priorities"]
    terrain_tags = data["terrainTags"]
    blocked_mask = 0
    for _dx, _dy, direction_bit, _opposite_bit in DIRECTIONS:
        blocked = False
        for tile_id in tile_ids_at(data, x, y):
            if tile_id == 0:
                continue
            terrain_tag = table_value(terrain_tags, tile_id)
            if terrain_tag == IGNORE_PASSABILITY_TAG:
                continue
            passage = table_value(passages, tile_id)
            priority = table_value(priorities, tile_id)

            # Water tiles carry 0x0f passage in Essentials. Floating movement
            # opens that surface while preserving blocking structures above it.
            if allow_float and terrain_tag in FLOATABLE_TERRAIN_TAGS:
                if priority == 0:
                    blocked = False
                    break
                continue
            if (passage & direction_bit) or (passage & 0x0F) == 0x0F:
                blocked = True
                break
            if priority == 0:
                blocked = False
                break
        if blocked:
            blocked_mask |= direction_bit
    return blocked_mask


def add_grid_boundaries(mask, x, y, width, height):
    if y == height - 1:
        mask |= 0x01
    if x == 0:
        mask |= 0x02
    if x == width - 1:
        mask |= 0x04
    if y == 0:
        mask |= 0x08
    return mask


def can_step(cells, width, height, x, y, direction, mask_key):
    dx, dy, direction_bit, opposite_bit = direction
    nx = x + dx
    ny = y + dy
    if nx < 0 or ny < 0 or nx >= width or ny >= height:
        return False
    source = cells[y * width + x]
    destination = cells[ny * width + nx]
    return not (source[mask_key] & direction_bit or destination[mask_key] & opposite_bit)


def reachable_cells(cells, width, height, entry, mask_key):
    if not (0 <= entry[0] < width and 0 <= entry[1] < height):
        return set()
    reached = {entry}
    queue = deque([entry])
    while queue:
        x, y = queue.popleft()
        for direction in DIRECTIONS:
            if not can_step(cells, width, height, x, y, direction, mask_key):
                continue
            neighbor = (x + direction[0], y + direction[1])
            if neighbor not in reached:
                reached.add(neighbor)
                queue.append(neighbor)
    return reached


def build_semantic_map(data, spec, tiles_w=MAP_TILES_W, tiles_h=MAP_TILES_H):
    crop_x, crop_y = spec["crop"]
    route_cells = {tuple(spec["entry"])}
    exits = []
    for route_spec in spec["routes"]:
        exits.append(tuple(route_spec["exit"]))
        route_cells.update(tuple(point) for point in route_spec["route"])
    event_cells = {
        (event["x"] - crop_x, event["y"] - crop_y)
        for event in data.get("events", [])
        if crop_x <= event["x"] < crop_x + tiles_w
        and crop_y <= event["y"] < crop_y + tiles_h
    }

    cells = []
    for y in range(tiles_h):
        for x in range(tiles_w):
            source_x = crop_x + x
            source_y = crop_y + y
            terrain_tag = effective_terrain_tag(data, source_x, source_y)
            land_blocked = add_grid_boundaries(
                blocked_mask_for_cell(data, source_x, source_y),
                x, y, tiles_w, tiles_h,
            )
            float_blocked = add_grid_boundaries(
                blocked_mask_for_cell(data, source_x, source_y, allow_float=True),
                x, y, tiles_w, tiles_h,
            )
            flags = 0
            if (x, y) == tuple(spec["entry"]):
                flags |= FLAG_ENTRY
            if (x, y) in exits:
                flags |= FLAG_EXIT
            if terrain_tag in WATER_TERRAIN_TAGS:
                flags |= FLAG_WATER
            if terrain_tag in FLOATABLE_TERRAIN_TAGS:
                flags |= FLAG_FLOATABLE
            if (x, y) in route_cells:
                flags |= FLAG_ROUTE
            if (x, y) in event_cells:
                flags |= FLAG_EVENT
            cells.append({
                "x": x,
                "y": y,
                "landBlocked": land_blocked,
                "floatBlocked": float_blocked,
                "terrainTag": terrain_tag,
                "flags": flags,
            })

    entry = tuple(spec["entry"])
    land_reachable = reachable_cells(cells, tiles_w, tiles_h, entry, "landBlocked")
    float_reachable = reachable_cells(cells, tiles_w, tiles_h, entry, "floatBlocked")
    for cell in cells:
        point = (cell["x"], cell["y"])
        cell["landReachable"] = point in land_reachable
        cell["floatReachable"] = point in float_reachable

    invalid_route_edges = []
    for route_index, route_spec in enumerate(spec["routes"]):
        points = [entry] + [tuple(point) for point in route_spec["route"]]
        for start, end in zip(points, points[1:]):
            dx = end[0] - start[0]
            dy = end[1] - start[1]
            direction = next(
                (item for item in DIRECTIONS if item[0] == dx and item[1] == dy),
                None,
            )
            valid = direction is not None and can_step(
                cells, tiles_w, tiles_h, start[0], start[1], direction, "landBlocked"
            )
            if not valid:
                invalid_route_edges.append({
                    "route": route_index,
                    "from": list(start),
                    "to": list(end),
                })

    water_cells = [cell for cell in cells if cell["flags"] & FLAG_WATER]
    float_only = [
        cell for cell in cells
        if cell["floatReachable"] and not cell["landReachable"]
    ]
    return {
        "format": "StickMonExploreMap",
        "version": SMONMAP_VERSION,
        "source": {
            "mapId": data["mapId"],
            "name": data["name"],
            "crop": [crop_x, crop_y],
        },
        "grid": {
            "width": tiles_w,
            "height": tiles_h,
            "tileSize": GAME_TILE_SIZE,
        },
        "entry": list(entry),
        "exits": [list(point) for point in exits],
        "terrainTags": {str(key): value for key, value in TERRAIN_TAG_NAMES.items()},
        "flags": {
            "entry": FLAG_ENTRY,
            "exit": FLAG_EXIT,
            "water": FLAG_WATER,
            "floatable": FLAG_FLOATABLE,
            "route": FLAG_ROUTE,
            "event": FLAG_EVENT,
        },
        "stats": {
            "cells": len(cells),
            "landReachable": len(land_reachable),
            "floatReachable": len(float_reachable),
            "floatOnlyReachable": len(float_only),
            "water": len(water_cells),
            "floatableWater": sum(bool(cell["flags"] & FLAG_FLOATABLE) for cell in cells),
            "invalidRouteEdges": len(invalid_route_edges),
        },
        "invalidRouteEdges": invalid_route_edges,
        "cells": cells,
    }


def encode_smonmap(semantic):
    grid = semantic["grid"]
    source = semantic["source"]
    exits = semantic["exits"]
    header = SMONMAP_HEADER.pack(
        SMONMAP_MAGIC,
        semantic["version"],
        grid["width"],
        grid["height"],
        grid["tileSize"],
        source["mapId"],
        source["crop"][0],
        source["crop"][1],
        semantic["entry"][0],
        semantic["entry"][1],
        len(exits),
        SMONMAP_CELL_STRIDE,
    )
    payload = bytearray(header)
    for x, y in exits:
        payload.extend((x, y))
    for cell in semantic["cells"]:
        payload.extend((
            cell["landBlocked"] & 0x0F,
            cell["floatBlocked"] & 0x0F,
            cell["terrainTag"] & 0xFF,
            cell["flags"] & 0xFF,
        ))
    return bytes(payload)


def write_semantic_map(semantic, key):
    MAP_DATA_DIR.mkdir(parents=True, exist_ok=True)
    binary_path = MAP_DATA_DIR / f"explore_{key}.smonmap"
    json_path = MAP_DATA_DIR / f"explore_{key}.json"
    binary_path.write_bytes(encode_smonmap(semantic))
    json_path.write_text(
        json.dumps(semantic, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return binary_path, json_path


def render_route_preview(items):
    route_colors = [(255, 36, 36, 255), (42, 126, 255, 255)]
    sheet = Image.new(
        "RGBA",
        (MAP_TILES_W * GAME_TILE_SIZE * 2, MAP_TILES_H * GAME_TILE_SIZE * 2),
        (0, 0, 0, 255),
    )
    for index, (spec, image) in enumerate(items):
        preview = image.copy()
        draw = ImageDraw.Draw(preview)
        for route_index, route_spec in enumerate(spec["routes"]):
            color = route_colors[route_index % len(route_colors)]
            points = [
                (x * GAME_TILE_SIZE + GAME_TILE_SIZE // 2,
                 y * GAME_TILE_SIZE + GAME_TILE_SIZE // 2)
                for x, y in route_spec["route"]
            ]
            draw.line(points, fill=color, width=4)
            for x, y in points:
                draw.ellipse((x - 4, y - 4, x + 4, y + 4),
                             fill=(255, 255, 255, 255), outline=color, width=2)
        draw.text((4, 4), f'{spec["label"]} / Map{spec["map_id"]:03d}',
                  fill=(255, 255, 0, 255), stroke_width=1, stroke_fill=(0, 0, 0, 255))
        sheet.alpha_composite(
            preview,
            ((index % 2) * preview.width, (index // 2) * preview.height),
        )
    sheet.save(ROUTE_PREVIEW)


def render_semantic_debug(spec, image, semantic):
    legend_height = 48
    canvas = Image.new(
        "RGBA", (image.width, image.height + legend_height), (20, 24, 27, 255)
    )
    canvas.alpha_composite(image, (0, legend_height))
    overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for cell in semantic["cells"]:
        x0 = cell["x"] * GAME_TILE_SIZE
        y0 = legend_height + cell["y"] * GAME_TILE_SIZE
        x1 = x0 + GAME_TILE_SIZE - 1
        y1 = y0 + GAME_TILE_SIZE - 1
        is_water = bool(cell["flags"] & FLAG_WATER)
        is_floatable = bool(cell["flags"] & FLAG_FLOATABLE)
        if is_water and not is_floatable:
            fill = (0, 220, 220, 125)
        elif is_water and cell["floatReachable"] and not cell["landReachable"]:
            fill = (40, 130, 255, 115)
        elif is_water:
            fill = (25, 70, 150, 125)
        elif cell["landReachable"]:
            fill = (48, 190, 92, 88)
        else:
            fill = (225, 58, 68, 110)
        draw.rectangle((x0, y0, x1, y1), fill=fill, outline=(255, 255, 255, 45))

        edge = (245, 60, 64, 220)
        inset = 3
        if cell["landBlocked"] & 0x01:
            draw.line((x0 + inset, y1 - 1, x1 - inset, y1 - 1), fill=edge, width=2)
        if cell["landBlocked"] & 0x02:
            draw.line((x0 + 1, y0 + inset, x0 + 1, y1 - inset), fill=edge, width=2)
        if cell["landBlocked"] & 0x04:
            draw.line((x1 - 1, y0 + inset, x1 - 1, y1 - inset), fill=edge, width=2)
        if cell["landBlocked"] & 0x08:
            draw.line((x0 + inset, y0 + 1, x1 - inset, y0 + 1), fill=edge, width=2)

    canvas = Image.alpha_composite(canvas, overlay)
    draw = ImageDraw.Draw(canvas)
    for route_spec in spec["routes"]:
        points = [tuple(spec["entry"])] + [tuple(point) for point in route_spec["route"]]
        pixels = [
            (x * GAME_TILE_SIZE + GAME_TILE_SIZE // 2,
             legend_height + y * GAME_TILE_SIZE + GAME_TILE_SIZE // 2)
            for x, y in points
        ]
        draw.line(pixels, fill=(0, 0, 0, 220), width=5)
        draw.line(pixels, fill=(255, 255, 255, 235), width=2)

    entry_x, entry_y = semantic["entry"]
    entry_box = (
        entry_x * GAME_TILE_SIZE + 2,
        legend_height + entry_y * GAME_TILE_SIZE + 2,
        (entry_x + 1) * GAME_TILE_SIZE - 3,
        legend_height + (entry_y + 1) * GAME_TILE_SIZE - 3,
    )
    draw.rectangle(entry_box, outline=(255, 255, 255, 255), width=3)
    draw.text((entry_box[0] + 4, entry_box[1] + 7), "IN", fill=(0, 0, 0, 255),
              stroke_width=1, stroke_fill=(255, 255, 255, 255))
    for index, (exit_x, exit_y) in enumerate(semantic["exits"], start=1):
        box = (
            exit_x * GAME_TILE_SIZE + 2,
            legend_height + exit_y * GAME_TILE_SIZE + 2,
            (exit_x + 1) * GAME_TILE_SIZE - 3,
            legend_height + (exit_y + 1) * GAME_TILE_SIZE - 3,
        )
        draw.rectangle(box, outline=(255, 220, 30, 255), width=3)
        draw.text((box[0] + 4, box[1] + 7), f"E{index}", fill=(0, 0, 0, 255),
                  stroke_width=1, stroke_fill=(255, 230, 40, 255))

    legend = (
        (4, 5, (48, 190, 92, 255), "LAND"),
        (72, 5, (25, 70, 150, 255), "WATER"),
        (150, 5, (40, 130, 255, 255), "FLOAT+"),
        (234, 5, (225, 58, 68, 255), "BLOCKED"),
    )
    for x, y, color, label in legend:
        draw.rectangle((x, y, x + 11, y + 11), fill=color)
        draw.text((x + 15, y + 1), label, fill=(245, 245, 245, 255))
    draw.rectangle((4, 26, 15, 37), fill=(0, 220, 220, 255))
    draw.text((19, 25), "SPECIAL", fill=(235, 235, 235, 255))
    draw.line((91, 31, 111, 31), fill=(245, 60, 64, 255), width=2)
    draw.text((116, 25), "blocked edge", fill=(235, 235, 235, 255))
    draw.rectangle((229, 24, 250, 43), outline=(255, 220, 30, 255), width=2)
    draw.text((256, 25), "entry / exit", fill=(235, 235, 235, 255))
    title = spec.get("debug_title", f'{spec["label"]} / Map{spec["map_id"]:03d}')
    title_font = next(
        (ImageFont.truetype(str(path), 11) for path in DEBUG_FONT_PATHS if path.exists()),
        ImageFont.load_default(),
    )
    draw.text((4, legend_height + 4), title, fill=(255, 255, 0, 255),
              stroke_width=1, stroke_fill=(0, 0, 0, 255), font=title_font)
    return canvas


def render_semantics_overview(items):
    if not items:
        return
    width = max(image.width for _spec, image in items)
    height = max(image.height for _spec, image in items)
    sheet = Image.new("RGBA", (width * 2, height * 2), (10, 12, 14, 255))
    for index, (_spec, image) in enumerate(items):
        sheet.alpha_composite(image, ((index % 2) * width, (index // 2) * height))
    DEBUG_DIR.mkdir(parents=True, exist_ok=True)
    sheet.save(SEMANTICS_PREVIEW)


def main():
    parser = argparse.ArgumentParser(description="Render StickMon exploration maps from RPG Maker XP")
    parser.add_argument(
        "--map-key",
        choices=["all"] + [spec["key"] for spec in MAP_SPECS],
        default="all",
    )
    args = parser.parse_args()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    DEBUG_DIR.mkdir(parents=True, exist_ok=True)
    selected = MAP_SPECS if args.map_key == "all" else [
        spec for spec in MAP_SPECS if spec["key"] == args.map_key
    ]
    previews = []
    semantic_previews = []
    for spec in selected:
        data = export_map(spec["map_id"])
        rendered = render_map(data)
        explore, crop = choose_crop(
            rendered,
            spec["crop"][0],
            spec["crop"][1],
            MAP_TILES_W,
            MAP_TILES_H,
        )
        output = OUTPUT_DIR / spec["output"]
        explore.save(output)
        if spec["key"] == "route1":
            rendered.save(FULL_PREVIEW)
        semantic = build_semantic_map(data, spec)
        binary_path, json_path = write_semantic_map(semantic, spec["key"])
        semantic_debug = render_semantic_debug(spec, explore, semantic)
        debug_path = DEBUG_DIR / f'explore_{spec["key"]}_semantics.png'
        semantic_debug.save(debug_path)
        previews.append((spec, explore))
        semantic_previews.append((spec, semantic_debug))
        print(
            f"map={data['mapId']} name={data['name']} size={data['width']}x{data['height']} "
            f"crop={crop[0]},{crop[1]},{crop[2]},{crop[3]} tiles={MAP_TILES_W}x{MAP_TILES_H} "
            f"game_tile={GAME_TILE_SIZE} output_size={explore.width}x{explore.height} "
            f"output={output} semantic={binary_path} json={json_path} debug={debug_path} "
            f"land={semantic['stats']['landReachable']} "
            f"float={semantic['stats']['floatReachable']} "
            f"water={semantic['stats']['water']} "
            f"invalid_route_edges={semantic['stats']['invalidRouteEdges']}"
        )
    if args.map_key == "all":
        render_route_preview(previews)
        render_semantics_overview(semantic_previews)
        print(f"route_preview={ROUTE_PREVIEW}")
        print(f"semantics_preview={SEMANTICS_PREVIEW}")


if __name__ == "__main__":
    main()
