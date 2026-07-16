#!/usr/bin/env python3

"""Generate a provisional Frost Crystal Cave tileset guide and map run."""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from cave_tile_semantics import (
    FROST_INNER_WALL_CORNER_TILES,
    ICE_ROCK_ISLAND_BACKGROUND_TILES,
    ICE_ROCK_ISLAND_TRANSPARENT_TILES,
)


ROOT = Path(__file__).resolve().parents[1]
ESSENTIALS = Path(
    "${ESSENTIALS_DIR}"
)
CAVES_TILESET = ESSENTIALS / "Graphics" / "Tilesets" / "Caves.png"
DEFAULT_OUTPUT_DIR = (
    ROOT
    / "origin_asset"
    / "generated"
    / "game"
    / "frost_crystal_cave_prototype"
)
COMPLETE_TILE_CATALOG_DIR = (
    ROOT
    / "origin_asset"
    / "generated"
    / "game"
    / "tileset_reference"
    / "frost_crystal_cave"
)
COMPLETE_TILE_CATALOG = COMPLETE_TILE_CATALOG_DIR / "frost_candidates_contact_sheet.png"

SOURCE_TILE = 32
GAME_TILE = 26
MAP_W = 16
MAP_H = 12
FIRST_REGULAR_TILE_ID = 384

OUTSIDE_SNOW_TILE = 1305
CAVE_FLOOR_TILE = 1386

WALL_TILES = {
    "top_left": 1302,
    "top": 1313,
    "top_right": 1303,
    "left": 1306,
    "right": 1304,
    "bottom_left": 1310,
    "bottom": 1297,
    "bottom_right": 1311,
}

ICE_TILES = {
    "top_left": 1492,
    "top": 1506,
    "top_right": 1494,
    "left": 1499,
    "center": 1328,
    "right": 1497,
    "bottom_left": 1508,
    "bottom": 1490,
    "bottom_right": 1510,
}

SCENERY_TILES = {
    "crystal_top": 1350,
    "crystal_blue": 1358,
    "crystal_white": 1359,
    "snow_boulder": 1349,
    "snow_boulder_alt": 1351,
    "small_rock": 1366,
    "small_rock_alt": 1367,
}

ROCK_ISLAND_TILES = {
    "transparent": ICE_ROCK_ISLAND_TRANSPARENT_TILES,
    "background": ICE_ROCK_ISLAND_BACKGROUND_TILES,
}
ENABLED_ROCK_ISLAND_VARIANTS = frozenset({"transparent"})

PROVISIONAL_TILE_GROUPS = (
    (
        "Ground and ice",
        (
            (1305, "snow (blocked)"),
            (1386, "floor (walk)"),
            (1328, "ice (slide)"),
            (1320, "ice gleam"),
            (1321, "cracked ice"),
        ),
    ),
    (
        "Continuous ice wall",
        (
            (1302, "wall NW"),
            (1313, "wall north"),
            (1303, "wall NE"),
            (1306, "wall west"),
            (1304, "wall east"),
            (1310, "wall SW"),
            (1297, "wall south"),
            (1311, "wall SE"),
            (1296, "transition W?"),
            (1298, "transition E?"),
        ),
    ),
    (
        "Ice field edge",
        (
            (1492, "ice edge NW"),
            (1506, "ice edge north"),
            (1494, "ice edge NE"),
            (1499, "ice edge west"),
            (1497, "ice edge east"),
            (1508, "ice edge SW"),
            (1490, "ice edge south"),
            (1510, "ice edge SE"),
        ),
    ),
    (
        "Confirmed exits and state tiles",
        (
            (1299, "cave exit left"),
            (1300, "cave exit center"),
            (1301, "cave exit right"),
            (1322, "broken-ice hole"),
            (1326, "round water SW"),
            (1327, "round water SE"),
            (1333, "stairs down"),
            (1341, "hole"),
        ),
    ),
    (
        "Obstacles",
        (
            (1350, "crystal top"),
            (1358, "blue crystal"),
            (1359, "white crystal"),
            (1349, "snow boulder"),
            (1351, "boulder alt"),
            (1366, "small rock"),
            (1367, "small rock alt"),
            (1332, "ladder top"),
            (1340, "ladder bottom"),
        ),
    ),
    (
        "Ice-cave edge trace (same order as the 579 series)",
        (
            (1344, "trace start"),
            (1345, "right to down"),
            (1353, "down segment"),
            (1361, "down to right"),
            (1362, "right segment"),
            (1363, "right to up"),
            (1355, "up segment"),
            (1347, "up to right"),
            (1348, "trace end"),
        ),
    ),
)


MAP_SPECS = (
    {
        "key": "frost_crystal_cave_01",
        "label": "Winding Rock Entrance",
        "rects": ((0, 5, 9, 11), (5, 1, 12, 8), (9, 0, 12, 3), (11, 3, 15, 7)),
        "entry": {"edge": "left", "point": (0, 9), "span": ((0, 8), (0, 9))},
        "exits": (
            {"edge": "top", "point": (10, 0), "span": ((10, 0), (11, 0))},
            {"edge": "right", "point": (15, 6), "span": ((15, 5), (15, 6))},
        ),
        "selected_exit": 0,
        "ice_rects": (),
        "rock_islands": ((2, 6, "transparent"),),
        "crystals": ((6, 5, "blue"), (13, 5, "white")),
        "boulders": ((8, 10, "large"), (11, 7, "small")),
        "routes": (
            ((0, 9), (7, 9), (7, 6), (10, 6), (10, 0)),
            ((0, 9), (7, 9), (7, 6), (15, 6)),
        ),
    },
    {
        "key": "frost_crystal_cave_02",
        "label": "Crosswind Ice Gallery",
        "rects": ((0, 3, 15, 8), (5, 0, 11, 11)),
        "entry": {"edge": "bottom", "point": (8, 11), "span": ((8, 11), (9, 11))},
        "exits": (
            {"edge": "left", "point": (0, 5), "span": ((0, 4), (0, 5))},
            {"edge": "right", "point": (15, 4), "span": ((15, 4), (15, 5))},
        ),
        "selected_exit": 0,
        "ice_rects": ((6, 1, 10, 7),),
        "rock_islands": (),
        "crystals": ((2, 7, "white"), (10, 2, "blue")),
        "boulders": ((3, 6, "large"), (10, 9, "small")),
        "routes": (
            ((8, 11), (8, 4), (1, 4), (1, 5), (0, 5)),
            ((8, 11), (8, 4), (15, 4)),
        ),
    },
    {
        "key": "frost_crystal_cave_03",
        "label": "Twin Crystal Halls",
        "rects": ((0, 1, 6, 8), (5, 4, 10, 7), (9, 2, 15, 10), (11, 8, 14, 11), (2, 0, 5, 3)),
        "entry": {"edge": "left", "point": (0, 6), "span": ((0, 5), (0, 6))},
        "exits": (
            {"edge": "bottom", "point": (12, 11), "span": ((12, 11), (13, 11))},
            {"edge": "top", "point": (3, 0), "span": ((3, 0), (4, 0))},
        ),
        "selected_exit": 0,
        "ice_rects": (),
        "rock_islands": (),
        "crystals": ((7, 5, "blue"), (4, 7, "white")),
        "boulders": ((2, 5, "large"), (14, 8, "small")),
        "routes": (
            ((0, 6), (7, 6), (13, 6), (13, 11), (12, 11)),
            ((0, 6), (3, 6), (3, 0)),
        ),
    },
    {
        "key": "frost_crystal_cave_04",
        "label": "Deep Crystal Ring",
        "rects": ((2, 0, 13, 4), (0, 2, 5, 10), (10, 2, 15, 10), (4, 7, 11, 11)),
        "entry": {"edge": "bottom", "point": (7, 11), "span": ((7, 11), (8, 11))},
        "exits": (
            {"edge": "right", "point": (15, 4), "span": ((15, 3), (15, 4))},
            {"edge": "left", "point": (0, 8), "span": ((0, 7), (0, 8))},
        ),
        "selected_exit": 0,
        "ice_rects": ((5, 1, 10, 3),),
        "rock_islands": (),
        "crystals": ((3, 2, "white"), (13, 5, "blue")),
        "boulders": ((4, 9, "small"), (11, 9, "large")),
        "routes": (
            ((7, 11), (7, 8), (11, 8), (11, 4), (15, 4)),
            ((7, 11), (7, 8), (4, 8), (0, 8)),
        ),
    },
)


def load_font(size, bold=False):
    candidates = (
        Path("/System/Library/Fonts/SFNSMono.ttf"),
        Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf")
        if bold
        else Path("/System/Library/Fonts/Supplemental/Arial.ttf"),
    )
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def source_tile(tileset, tile_id):
    index = tile_id - FIRST_REGULAR_TILE_ID
    if index < 0:
        raise ValueError(f"tile {tile_id} is not a regular RPG Maker XP tile")
    x = (index % 8) * SOURCE_TILE
    y = (index // 8) * SOURCE_TILE
    if x + SOURCE_TILE > tileset.width or y + SOURCE_TILE > tileset.height:
        raise ValueError(f"tile {tile_id} is outside Caves.png")
    return tileset.crop((x, y, x + SOURCE_TILE, y + SOURCE_TILE))


def rectangle_cells(rect):
    left, top, right, bottom = rect
    return {
        (x, y)
        for y in range(top, bottom + 1)
        for x in range(left, right + 1)
        if 0 <= x < MAP_W and 0 <= y < MAP_H
    }


def expand_route(control_points):
    cells = []
    for first, second in zip(control_points, control_points[1:]):
        x1, y1 = first
        x2, y2 = second
        if x1 != x2 and y1 != y2:
            raise ValueError(f"route segment must be axis aligned: {first}->{second}")
        dx = 0 if x1 == x2 else (1 if x2 > x1 else -1)
        dy = 0 if y1 == y2 else (1 if y2 > y1 else -1)
        x, y = x1, y1
        if not cells:
            cells.append((x, y))
        while (x, y) != (x2, y2):
            x += dx
            y += dy
            cells.append((x, y))
    return cells


def portal_openings(spec):
    openings = set()
    for portal in (spec["entry"], *spec["exits"]):
        for cell in portal["span"]:
            openings.add((cell, portal["edge"]))
    return openings


def boundary_tile(floor, openings, x, y):
    tests = {
        "top": (x, y - 1),
        "right": (x + 1, y),
        "bottom": (x, y + 1),
        "left": (x - 1, y),
    }
    outside = {
        direction
        for direction, neighbor in tests.items()
        if neighbor not in floor and ((x, y), direction) not in openings
    }
    if not outside:
        return 0
    if outside == {"top", "left"}:
        return WALL_TILES["top_left"]
    if outside == {"top", "right"}:
        return WALL_TILES["top_right"]
    if outside == {"bottom", "left"}:
        return WALL_TILES["bottom_left"]
    if outside == {"bottom", "right"}:
        return WALL_TILES["bottom_right"]
    if len(outside) == 1:
        return WALL_TILES[next(iter(outside))]
    raise ValueError(f"unsupported narrow cave boundary at {(x, y)}: {sorted(outside)}")


def stamp_ice(layers, rect, floor, walls, ice):
    left, top, right, bottom = rect
    if right - left < 2 or bottom - top < 2:
        raise ValueError("ice fields must be at least 3x3")
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            cell = (x, y)
            if cell not in floor or cell in walls:
                raise ValueError(f"ice field crosses cave wall at {cell}")
            if x == left and y == top:
                tile_id = ICE_TILES["top_left"]
            elif x == right and y == top:
                tile_id = ICE_TILES["top_right"]
            elif x == left and y == bottom:
                tile_id = ICE_TILES["bottom_left"]
            elif x == right and y == bottom:
                tile_id = ICE_TILES["bottom_right"]
            elif y == top:
                tile_id = ICE_TILES["top"]
            elif y == bottom:
                tile_id = ICE_TILES["bottom"]
            elif x == left:
                tile_id = ICE_TILES["left"]
            elif x == right:
                tile_id = ICE_TILES["right"]
            else:
                tile_id = ICE_TILES["center"]
            layers[0][y * MAP_W + x] = tile_id
            ice.add(cell)


def stamp_inner_wall_corners(layers, floor, walls):
    corner_checks = (
        ("outside_nw", (-1, -1), (-1, 0), (0, -1)),
        ("outside_ne", (1, -1), (1, 0), (0, -1)),
        ("outside_sw", (-1, 1), (-1, 0), (0, 1)),
        ("outside_se", (1, 1), (1, 0), (0, 1)),
    )
    for x, y in sorted(floor):
        index = y * MAP_W + x
        if layers[1][index]:
            continue
        matches = []
        for name, diagonal, first_side, second_side in corner_checks:
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
            raise ValueError(f"ambiguous inner wall corner at {(x, y)}: {matches}")
        if matches:
            layers[1][index] = FROST_INNER_WALL_CORNER_TILES[matches[0]]
            walls.add((x, y))


def stamp_rock_island(
    layers,
    left,
    top,
    variant,
    floor,
    walls,
    route_cells,
    blocked,
    rock_island_cells,
):
    if variant not in ENABLED_ROCK_ISLAND_VARIANTS:
        raise ValueError(
            f"rock-island variant is disabled for Frost Crystal Cave: {variant}"
        )
    try:
        module = ROCK_ISLAND_TILES[variant]
    except KeyError as error:
        raise ValueError(f"unknown rock-island variant: {variant}") from error

    cells = {
        (left + column, top + row)
        for row in range(3)
        for column in range(3)
    }
    if any(not (0 <= x < MAP_W and 0 <= y < MAP_H) for x, y in cells):
        raise ValueError(f"rock island at {(left, top)} leaves the map")
    if not cells <= floor:
        raise ValueError(f"rock island at {(left, top)} leaves the cave floor")
    if cells & walls:
        raise ValueError(f"rock island at {(left, top)} overlaps a cave wall")
    if cells & route_cells:
        raise ValueError(f"rock island at {(left, top)} blocks a route")
    if cells & blocked:
        raise ValueError(f"rock island at {(left, top)} overlaps blocked scenery")

    for row, tile_row in enumerate(module):
        for column, tile_id in enumerate(tile_row):
            x = left + column
            y = top + row
            layers[1][y * MAP_W + x] = tile_id
    blocked.update(cells)
    rock_island_cells.update(cells)


def build_map(spec):
    layers = [[0] * (MAP_W * MAP_H) for _ in range(3)]
    layers[0] = [OUTSIDE_SNOW_TILE] * (MAP_W * MAP_H)
    floor = set().union(*(rectangle_cells(rect) for rect in spec["rects"]))
    openings = portal_openings(spec)
    for x, y in floor:
        layers[0][y * MAP_W + x] = CAVE_FLOOR_TILE

    walls = set()
    for x, y in floor:
        tile_id = boundary_tile(floor, openings, x, y)
        if tile_id:
            layers[1][y * MAP_W + x] = tile_id
            walls.add((x, y))

    stamp_inner_wall_corners(layers, floor, walls)

    ice = set()
    for rect in spec["ice_rects"]:
        stamp_ice(layers, rect, floor, walls, ice)

    route_cells = {cell for route in spec["routes"] for cell in expand_route(route)}
    blocked = set(walls)
    rock_island_cells = set()
    for left, top, variant in spec["rock_islands"]:
        stamp_rock_island(
            layers,
            left,
            top,
            variant,
            floor,
            walls,
            route_cells,
            blocked,
            rock_island_cells,
        )

    for x, y, variant in spec["crystals"]:
        if (x, y) in route_cells or (x, y) in blocked:
            raise ValueError(f"crystal blocks a route at {(x, y)}")
        layers[1][y * MAP_W + x] = SCENERY_TILES[
            "crystal_blue" if variant == "blue" else "crystal_white"
        ]
        if y > 0:
            layers[2][(y - 1) * MAP_W + x] = SCENERY_TILES["crystal_top"]
        blocked.add((x, y))

    for index, (x, y, size) in enumerate(spec["boulders"]):
        if (x, y) in route_cells or (x, y) in blocked:
            raise ValueError(f"boulder blocks a route at {(x, y)}")
        if size == "large":
            tile_id = SCENERY_TILES[
                "snow_boulder" if index % 2 == 0 else "snow_boulder_alt"
            ]
        else:
            tile_id = SCENERY_TILES[
                "small_rock" if index % 2 == 0 else "small_rock_alt"
            ]
        layers[1][y * MAP_W + x] = tile_id
        blocked.add((x, y))

    walkable = floor - blocked
    for cell in route_cells:
        if cell not in walkable:
            raise ValueError(f"route crosses blocked terrain at {cell}")
    return {
        "layers": layers,
        "floor": floor,
        "walkable": walkable,
        "blocked": blocked | ({(x, y) for y in range(MAP_H) for x in range(MAP_W)} - floor),
        "ice": ice,
        "rock_island_cells": rock_island_cells,
        "route_cells": route_cells,
    }


def render_map(layers, tileset):
    canvas = Image.new("RGBA", (MAP_W * SOURCE_TILE, MAP_H * SOURCE_TILE), (0, 0, 0, 255))
    cache = {}
    for layer in layers:
        for index, tile_id in enumerate(layer):
            if tile_id == 0:
                continue
            tile = cache.setdefault(tile_id, source_tile(tileset, tile_id))
            canvas.alpha_composite(tile, ((index % MAP_W) * SOURCE_TILE, (index // MAP_W) * SOURCE_TILE))
    return canvas.resize((MAP_W * GAME_TILE, MAP_H * GAME_TILE), Image.Resampling.NEAREST)


def cell_center(cell):
    return (cell[0] * GAME_TILE + GAME_TILE // 2, cell[1] * GAME_TILE + GAME_TILE // 2)


def render_debug(spec, data, clean):
    debug = clean.copy()
    draw = ImageDraw.Draw(debug, "RGBA")
    for y in range(MAP_H):
        for x in range(MAP_W):
            bounds = (x * GAME_TILE, y * GAME_TILE, (x + 1) * GAME_TILE - 1, (y + 1) * GAME_TILE - 1)
            cell = (x, y)
            if cell in data["blocked"]:
                fill = (215, 55, 55, 48)
            elif cell in data["ice"]:
                fill = (40, 205, 255, 72)
            else:
                fill = (72, 220, 125, 26)
            draw.rectangle(bounds, fill=fill, outline=(24, 38, 44, 70))

    colors = ((48, 122, 255, 235), (244, 82, 82, 225))
    for route, color in zip(spec["routes"], colors):
        points = [cell_center(cell) for cell in expand_route(route)]
        draw.line(points, fill=color, width=4)

    entry = cell_center(spec["entry"]["point"])
    draw.rectangle((entry[0] - 7, entry[1] - 7, entry[0] + 7, entry[1] + 7), outline=(64, 242, 118, 255), width=3)
    for index, portal in enumerate(spec["exits"]):
        center = cell_center(portal["point"])
        color = (255, 219, 78, 255) if index == spec["selected_exit"] else (255, 255, 255, 235)
        draw.ellipse((center[0] - 7, center[1] - 7, center[0] + 7, center[1] + 7), outline=color, width=3)

    for left, top, variant in spec["rock_islands"]:
        bounds = (
            left * GAME_TILE,
            top * GAME_TILE,
            (left + 3) * GAME_TILE - 1,
            (top + 3) * GAME_TILE - 1,
        )
        color = (245, 214, 91, 255) if variant == "transparent" else (222, 148, 255, 255)
        draw.rectangle(bounds, outline=color, width=2)
        draw.text(
            (bounds[0] + 3, bounds[1] + 2),
            "alpha" if variant == "transparent" else "bg",
            fill=color,
            font=ImageFont.load_default(),
        )

    title = f"{spec['key']}  green=walkable cyan=ice red=blocked"
    draw.rectangle((0, 0, min(debug.width, len(title) * 7 + 8), 18), fill=(12, 18, 22, 220))
    draw.text((4, 3), title, fill=(255, 244, 154, 255), font=ImageFont.load_default())
    return debug


def checkerboard(size, square=8):
    image = Image.new("RGBA", (size, size), (205, 210, 213, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size, square):
        for x in range(0, size, square):
            if (x // square + y // square) % 2:
                draw.rectangle((x, y, x + square - 1, y + square - 1), fill=(234, 237, 239, 255))
    return image


def render_tile_guide(tileset, output_path):
    columns = 5
    cell_w = 230
    cell_h = 128
    margin = 24
    title_h = 64
    group_h = 38
    rows = sum((len(entries) + columns - 1) // columns for _name, entries in PROVISIONAL_TILE_GROUPS)
    height = title_h + len(PROVISIONAL_TILE_GROUPS) * group_h + rows * cell_h + margin
    canvas = Image.new("RGBA", (margin * 2 + columns * cell_w, height), (18, 23, 26, 255))
    draw = ImageDraw.Draw(canvas)
    draw.text((margin, 16), "Frost Crystal Cave - provisional tile semantics", font=load_font(24, True), fill=(255, 231, 125, 255))
    draw.text((margin, 44), "Source: Pokemon Essentials Caves.png (32x32 regular tiles)", font=load_font(13), fill=(178, 198, 205, 255))

    y = title_h
    preview_size = SOURCE_TILE * 3
    checker = checkerboard(preview_size)
    for group_name, entries in PROVISIONAL_TILE_GROUPS:
        draw.rectangle((margin, y, canvas.width - margin, y + group_h - 6), fill=(35, 47, 52, 255))
        draw.text((margin + 10, y + 7), group_name, font=load_font(18, True), fill=(220, 238, 242, 255))
        y += group_h
        group_rows = (len(entries) + columns - 1) // columns
        for index, (tile_id, label) in enumerate(entries):
            column = index % columns
            row = index // columns
            x = margin + column * cell_w
            cell_y = y + row * cell_h
            draw.rectangle((x, cell_y, x + cell_w - 6, cell_y + cell_h - 6), fill=(29, 36, 40, 255), outline=(67, 82, 88, 255))
            canvas.alpha_composite(checker, (x + 8, cell_y + 8))
            tile = source_tile(tileset, tile_id).resize((preview_size, preview_size), Image.Resampling.NEAREST)
            canvas.alpha_composite(tile, (x + 8, cell_y + 8))
            draw.text((x + 116, cell_y + 18), str(tile_id), font=load_font(21, True), fill=(255, 224, 105, 255))
            draw.text((x + 116, cell_y + 50), label, font=load_font(13), fill=(222, 230, 232, 255))
        y += group_rows * cell_h
    canvas.save(output_path)


def make_overview(records, output_path, key):
    gap = 10
    label_h = 26
    canvas = Image.new(
        "RGBA",
        (gap * 3 + MAP_W * GAME_TILE * 2, gap * 3 + (MAP_H * GAME_TILE + label_h) * 2),
        (16, 22, 25, 255),
    )
    draw = ImageDraw.Draw(canvas)
    for index, record in enumerate(records):
        x = gap + (index % 2) * (MAP_W * GAME_TILE + gap)
        y = gap + (index // 2) * (MAP_H * GAME_TILE + label_h + gap)
        draw.text((x, y), f"{index + 1}. {record['label']}", font=load_font(15, True), fill=(242, 231, 168, 255))
        canvas.alpha_composite(record[key], (x, y + label_h))
    canvas.save(output_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    tileset = Image.open(CAVES_TILESET).convert("RGBA")
    guide_path = COMPLETE_TILE_CATALOG
    if not guide_path.exists():
        raise FileNotFoundError(
            "generate the complete frost tile catalog before rendering the prototype"
        )

    records = []
    manifest_maps = []
    for spec in MAP_SPECS:
        data = build_map(spec)
        clean = render_map(data["layers"], tileset)
        debug = render_debug(spec, data, clean)
        clean_path = args.output_dir / f"{spec['key']}_416.png"
        debug_path = args.output_dir / f"{spec['key']}_debug.png"
        clean.save(clean_path)
        debug.save(debug_path)
        records.append({"label": spec["label"], "clean": clean, "debug": debug})
        manifest_maps.append(
            {
                "key": spec["key"],
                "label": spec["label"],
                "image": clean_path.name,
                "debug": debug_path.name,
                "entry": spec["entry"],
                "exits": spec["exits"],
                "selectedExit": spec["selected_exit"],
                "rockIslands": [
                    {"x": x, "y": y, "variant": variant}
                    for x, y, variant in spec["rock_islands"]
                ],
                "stats": {
                    "walkable": len(data["walkable"]),
                    "blocked": len(data["blocked"]),
                    "ice": len(data["ice"]),
                    "rockIslandTiles": len(data["rock_island_cells"]),
                },
            }
        )

    overview_path = args.output_dir / "frost_crystal_cave_expedition.png"
    debug_overview_path = args.output_dir / "frost_crystal_cave_expedition_debug.png"
    make_overview(records, overview_path, "clean")
    make_overview(records, debug_overview_path, "debug")

    manifest = {
        "status": "prototype-only",
        "sourceTileset": str(CAVES_TILESET),
        "referenceMap": "Map034 Ice Cave",
        "tileCatalog": str(guide_path.relative_to(ROOT)),
        "tileCatalogPages": [
            str(path.relative_to(ROOT))
            for path in sorted(COMPLETE_TILE_CATALOG_DIR.glob("frost_candidates_[0-9]*.png"))
        ],
        "assumptions": {
            "outsideSnowBlocked": OUTSIDE_SNOW_TILE,
            "walkableFloor": CAVE_FLOOR_TILE,
            "wallTiles": WALL_TILES,
            "iceTiles": ICE_TILES,
            "sceneryTiles": SCENERY_TILES,
            "enabledRockIslandVariants": sorted(ENABLED_ROCK_ISLAND_VARIANTS),
        },
        "maps": manifest_maps,
    }
    manifest_path = args.output_dir / "frost_crystal_cave_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"tile-catalog={guide_path}")
    print(f"overview={overview_path}")
    print(f"debug={debug_overview_path}")
    print(f"manifest={manifest_path}")


if __name__ == "__main__":
    main()
