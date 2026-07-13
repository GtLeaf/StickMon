#!/usr/bin/env python3

import argparse
import random
from pathlib import Path

from PIL import Image, ImageDraw

from generate_map_rule_preview import (
    GAME_TILE,
    MAP_H,
    MAP_W,
    TILESET,
    center,
    load_autotiles,
    render_layer,
    sample_route,
)
from map_generation_rules import (
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


def build_map(spec, run_seed, return_generation_data=False):
    validate_route_network(spec)
    layers = [[0] * (MAP_W * MAP_H) for _ in range(3)]
    rng = random.Random(run_seed + spec["seed_offset"])
    grass_ids = [385, 385, 385, 386, 387, 388, 389]
    for index in range(MAP_W * MAP_H):
        layers[0][index] = rng.choice(grass_ids)

    road_tiles = build_two_tile_road(spec["routes"], MAP_W, MAP_H)
    path_cells = set(road_tiles)

    water_cells = set()
    for coast_x in spec.get("left_coasts", []):
        water_cells.update(stamp_left_coast(layers, MAP_W, MAP_H, coast_x))
    for left, top in spec.get("ponds", []):
        water_cells.update(add_pond(layers, left, top))
    for left, top, width, height in spec.get("water_rects", []):
        water_cells.update(add_water_rect(layers, left, top, width, height))
    if water_cells & path_cells:
        raise RuntimeError(f"water overlaps route without a bridge: {spec['key']}")

    for left, top, width, height in spec.get("dense", []):
        for y in range(top, top + height):
            for x in range(left, left + width):
                if (x, y) not in path_cells and (x, y) not in water_cells:
                    layers[0][y * MAP_W + x] = 390

    for (x, y), tile_id in road_tiles.items():
        layers[0][y * MAP_W + x] = tile_id

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
    solid_scenery = water_cells | forest_footprints | lighthouse_footprints
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
    if actual_forest_positions != expected_forest_positions:
        raise RuntimeError(f"forest body outside template: {spec['key']}")
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
    rendered_road_cells = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[0])
        if tile_id in ROAD_TILE_IDS
    }
    if rendered_road_cells != route_road_cells:
        raise RuntimeError(f"multi-exit road was overwritten or expanded: {spec['key']}")

    tileset = Image.open(TILESET).convert("RGBA")
    autotiles = load_autotiles()
    rendered_layers = [render_layer(layer, tileset, autotiles) for layer in layers]
    combined = Image.new("RGBA", rendered_layers[0].size, (0, 0, 0, 255))
    for layer in rendered_layers:
        combined.alpha_composite(layer)
    clean = combined.resize((MAP_W * GAME_TILE, MAP_H * GAME_TILE), Image.Resampling.NEAREST)

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
            "road_cells": sorted(path_cells),
            "water_cells": sorted(water_cells),
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
