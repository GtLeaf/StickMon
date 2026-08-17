#!/usr/bin/env python3

import argparse
import random
from pathlib import Path

from PIL import Image, ImageDraw

from asset_paths import essentials_dir
from generate_explore_map import autotile_variant
from map_generation_rules import (
    CUSTOM_TILE_VERTICAL_FLIPS,
    FOREST_BODY_IDS,
    FOREST_CROWN_IDS,
    ROAD_TILE_IDS,
    build_two_tile_road,
    stamp_forest_fence,
)


ROOT = Path(__file__).resolve().parents[1]
ESSENTIALS = essentials_dir()
TILESET = ESSENTIALS / "Graphics" / "Tilesets" / "Outside.png"
AUTOTILE_NAMES = (
    "Sea",
    "Sea without shore",
    "Sea deep",
    "Sand shore",
    "Flowers1",
    "Water rock",
    "Fountain1",
)
WATERFALL_AUTOTILE_NAMES = (
    "Sea",
    "Sea without shore",
    "Sea deep",
    "Sand shore",
    "Waterfall crest",
    "Waterfall",
    "Waterfall bottom",
)
MAP_W = 16
MAP_H = 12
SOURCE_TILE = 32
GAME_TILE = 26


def regular_tile(tileset, tile_id):
    source_id = CUSTOM_TILE_VERTICAL_FLIPS.get(tile_id, tile_id)
    index = source_id - 384
    x = (index % 8) * SOURCE_TILE
    y = (index // 8) * SOURCE_TILE
    tile = tileset.crop((x, y, x + SOURCE_TILE, y + SOURCE_TILE))
    if source_id != tile_id:
        tile = tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    return tile


def paste_tile(canvas, tileset, tile_id, x, y):
    canvas.alpha_composite(regular_tile(tileset, tile_id), (x * SOURCE_TILE, y * SOURCE_TILE))


def load_autotiles(names=AUTOTILE_NAMES):
    if names is None:
        names = AUTOTILE_NAMES
    return [
        (
            Image.open(ESSENTIALS / "Graphics" / "Autotiles" / f"{name}.png").convert("RGBA")
            if name
            else None
        )
        for name in names
    ]


def render_layer(
    tile_ids,
    tileset,
    autotiles=(),
    frame_index=0,
    tile_source_overrides=None,
    external_tilesets=None,
):
    tile_source_overrides = tile_source_overrides or {}
    external_tilesets = external_tilesets or {}
    canvas = Image.new(
        "RGBA",
        (MAP_W * SOURCE_TILE, MAP_H * SOURCE_TILE),
        (0, 0, 0, 0),
    )
    for index, tile_id in enumerate(tile_ids):
        if tile_id < 48:
            continue
        x = index % MAP_W
        y = index // MAP_W
        if tile_id < 384:
            autotile_index = tile_id // 48 - 1
            if not 0 <= autotile_index < len(autotiles):
                continue
            if autotiles[autotile_index] is None:
                continue
            tile = autotile_variant(
                autotiles[autotile_index],
                tile_id % 48,
                frame_index,
            )
            canvas.alpha_composite(tile, (x * SOURCE_TILE, y * SOURCE_TILE))
        else:
            source = tile_source_overrides.get(tile_id)
            if source:
                tileset_name, source_id = source
                paste_tile(canvas, external_tilesets[tileset_name], source_id, x, y)
            else:
                paste_tile(canvas, tileset, tile_id, x, y)
    return canvas


def checker(width, height):
    canvas = Image.new("RGBA", (width, height), (58, 67, 69, 255))
    draw = ImageDraw.Draw(canvas)
    size = 16
    for y in range(0, height, size):
        for x in range(0, width, size):
            if (x // size + y // size) % 2:
                draw.rectangle((x, y, x + size - 1, y + size - 1), fill=(76, 85, 87, 255))
    return canvas


def center(point):
    return (
        int(round(point[0] * GAME_TILE + GAME_TILE // 2)),
        int(round(point[1] * GAME_TILE + GAME_TILE // 2)),
    )


def sample_route(route):
    samples = []
    for first, second in zip(route, route[1:]):
        distance = int(round(abs(first[0] - second[0]) + abs(first[1] - second[1])))
        for step in range(distance):
            progress = step / max(1, distance)
            point = (
                first[0] + (second[0] - first[0]) * progress,
                first[1] + (second[1] - first[1]) * progress,
            )
            if not samples or point != samples[-1]:
                samples.append(point)
    samples.append(route[-1])
    return samples


def main():
    parser = argparse.ArgumentParser(description="Generate a preview using StickMon map composition rules")
    parser.add_argument("--output-dir", type=Path, default=Path("/tmp/stickmon-map-rule-preview"))
    parser.add_argument("--seed", type=int, default=20260712)
    args = parser.parse_args()

    tileset = Image.open(TILESET).convert("RGBA")
    autotiles = load_autotiles()
    layers = [[0] * (MAP_W * MAP_H) for _ in range(3)]
    rng = random.Random(args.seed)
    grass_ids = [385, 385, 385, 386, 387, 388, 389]
    for index in range(MAP_W * MAP_H):
        layers[0][index] = rng.choice(grass_ids)

    entry = (7.5, -0.5)
    junction = (7.5, 3.5)
    routes = [
        [entry, junction, (-0.5, 3.5)],
        [entry, junction, (MAP_W - 0.5, 3.5)],
    ]
    road_tiles = build_two_tile_road(routes, MAP_W, MAP_H)
    path_cells = set(road_tiles)

    for left, top, width, height in ((1, 0, 5, 3), (10, 0, 5, 3)):
        for y in range(top, top + height):
            for x in range(left, left + width):
                if (x, y) not in path_cells:
                    layers[0][y * MAP_W + x] = 390

    for (x, y), tile_id in road_tiles.items():
        layers[0][y * MAP_W + x] = tile_id

    forest = stamp_forest_fence(
        layers,
        MAP_W,
        MAP_H,
        forest_x=2,
        top_y=5,
        forest_width=14,
        bottom_y=11,
    )

    for x, y in ((1, 1), (5, 1), (10, 1), (14, 1)):
        layers[0][y * MAP_W + x] = 415

    forest_positions = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[0])
        if tile_id in FOREST_BODY_IDS
    }
    crown_positions = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[2])
        if tile_id in FOREST_CROWN_IDS
    }
    expected_forest = 14 * 6
    if len(forest_positions) != expected_forest or len(crown_positions) != 14:
        raise RuntimeError("forest fence rule produced an incomplete stamp")
    actual_road_cells = {
        (index % MAP_W, index // MAP_W)
        for index, tile_id in enumerate(layers[0])
        if tile_id in ROAD_TILE_IDS
    }
    if actual_road_cells != path_cells:
        raise RuntimeError("rendered road differs from the active route envelope")

    rendered_layers = [render_layer(layer, tileset, autotiles) for layer in layers]
    combined = Image.new("RGBA", rendered_layers[0].size, (0, 0, 0, 255))
    for layer in rendered_layers:
        combined.alpha_composite(layer)
    clean = combined.resize((MAP_W * GAME_TILE, MAP_H * GAME_TILE), Image.Resampling.NEAREST)

    review = clean.copy()
    draw = ImageDraw.Draw(review, "RGBA")
    for route, route_color in zip(
        routes,
        ((62, 139, 255, 235), (244, 82, 82, 220)),
    ):
        draw.line([center(point) for point in route], fill=route_color, width=4)
        for point in sample_route(route):
            x, y = center(point)
            draw.ellipse(
                (x - 3, y - 3, x + 3, y + 3),
                fill=(255, 255, 255, 230),
                outline=route_color,
            )
    entry_x, _ = center(entry)
    draw.polygon(((entry_x, 2), (entry_x - 8, 12), (entry_x + 8, 12)), fill=(78, 220, 117, 255))
    for route in routes:
        exit_x, exit_y = center(route[-1])
        draw.ellipse(
            (exit_x - 8, exit_y - 8, exit_x + 8, exit_y + 8),
            outline=(255, 196, 76, 255),
            width=3,
        )

    layer_previews = []
    for layer in rendered_layers:
        preview = checker(*layer.size)
        preview.alpha_composite(layer)
        layer_previews.append(preview.resize(clean.size, Image.Resampling.NEAREST))
    layer_previews.append(clean)
    gap = 8
    breakdown = Image.new(
        "RGBA",
        (clean.width * 2 + gap * 3, clean.height * 2 + gap * 3),
        (25, 33, 36, 255),
    )
    for index, preview in enumerate(layer_previews):
        x = gap + (index % 2) * (clean.width + gap)
        y = gap + (index // 2) * (clean.height + gap)
        breakdown.alpha_composite(preview, (x, y))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    clean_path = args.output_dir / "forest_fence_rule_clean.png"
    review_path = args.output_dir / "forest_fence_rule_routes.png"
    layers_path = args.output_dir / "forest_fence_rule_layers.png"
    clean.save(clean_path)
    review.save(review_path)
    breakdown.save(layers_path)
    print(f"forest={forest}")
    print(clean_path)
    print(review_path)
    print(layers_path)


if __name__ == "__main__":
    main()
