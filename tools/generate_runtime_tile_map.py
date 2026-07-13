#!/usr/bin/env python3

import argparse
import json
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from generate_map_rule_preview import (
    GAME_TILE,
    MAP_H,
    MAP_W,
    TILESET,
    load_autotiles,
    render_layer,
)


ALGORITHM_VERSION = 1
MASK32 = 0xFFFFFFFF
MAX_PATH_POINTS = 48
PATH_COUNT = 2

GRASS_TILES = (385, 385, 385, 386, 387, 388, 389)
DENSE_GRASS_TILE = 390
FLOWER_TILE = 415
DEEP_SEA_TILE = 144
DEEP_SEA_EDGE_TILE = 168
SEA_SHORE_TILE = 72

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
    entry: Endpoint
    paths: list[RoutePath]
    layers: list[list[int]]
    has_coast: bool
    has_forest: bool


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
    if endpoint.edge in (Edge.TOP, Edge.BOTTOM):
        append_line(points, endpoint.point[0], junction_y)
        append_line(points, endpoint.point[0], endpoint.point[1])
    else:
        append_line(points, junction_x, endpoint.point[1])
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


def generate_map(seed, entry_edge):
    seed &= MASK32
    topology = XorShift32(seed ^ 0xA511E9B3)
    terrain = XorShift32(seed ^ 0x63D83595)
    has_coast = entry_edge != Edge.LEFT and topology.bounded(100) < 45

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

    minimum_x = 7 if has_coast else 3
    junction_x = minimum_x + topology.bounded(12 - minimum_x)
    junction_y = 2 + topology.bounded(7)
    entry_coordinate = junction_x if entry_edge in (Edge.TOP, Edge.BOTTOM) else junction_y
    entry = endpoint_for_edge(entry_edge, junction_x, junction_y, entry_coordinate)
    trunk = [entry.point]
    append_line(trunk, junction_x, junction_y)

    paths = []
    for edge in candidates[:PATH_COUNT]:
        if edge in (Edge.TOP, Edge.BOTTOM):
            minimum_exit_x = 7 if has_coast else 2
            coordinate = minimum_exit_x + topology.bounded(14 - minimum_exit_x)
        else:
            coordinate = 1 + topology.bounded(10)
        endpoint = endpoint_for_edge(edge, junction_x, junction_y, coordinate)
        paths.append(build_path(trunk, junction_x, junction_y, endpoint))

    runtime_map = RuntimeMap(
        seed=seed,
        entry=entry,
        paths=paths,
        layers=[[0] * (MAP_W * MAP_H) for _ in range(3)],
        has_coast=has_coast,
        has_forest=False,
    )
    road = build_road(paths)
    water = set()
    forest = set()

    for index in range(MAP_W * MAP_H):
        runtime_map.layers[0][index] = GRASS_TILES[terrain.bounded(len(GRASS_TILES))]
    if has_coast:
        stamp_coast(runtime_map, water)

    patch_count = 3 + terrain.bounded(3)
    for _ in range(patch_count):
        width = 2 + terrain.bounded(4)
        height = 1 + terrain.bounded(3)
        left = terrain.bounded(MAP_W - width + 1)
        top = terrain.bounded(MAP_H - height + 1)
        for y in range(top, top + height):
            for x in range(left, left + width):
                if (x, y) not in road and (x, y) not in water:
                    runtime_map.layers[0][y * MAP_W + x] = DENSE_GRASS_TILE

    for x, y in road:
        runtime_map.layers[0][y * MAP_W + x] = road_tile(runtime_map, road, x, y)

    if terrain.bounded(100) < 70:
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

    flower_target = 6 + terrain.bounded(5)
    flowers = 0
    for _ in range(64):
        if flowers >= flower_target:
            break
        x = terrain.bounded(MAP_W)
        y = terrain.bounded(MAP_H)
        if (x, y) in road or (x, y) in water or (x, y) in forest:
            continue
        index = y * MAP_W + x
        if runtime_map.layers[0][index] == FLOWER_TILE:
            continue
        runtime_map.layers[0][index] = FLOWER_TILE
        flowers += 1

    for path in paths:
        if len(path.points) < 2:
            raise ValueError("route is too short")
        for point in path.points:
            if point not in road or point in water or point in forest:
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
    value = fnv_byte(value, int(runtime_map.entry.edge))
    value = fnv_byte(value, runtime_map.entry.point[0])
    value = fnv_byte(value, runtime_map.entry.point[1])
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
        "fingerprint": f"{fingerprint(runtime_map):08x}",
        "size": [MAP_W, MAP_H],
        "entry": endpoint_json(runtime_map.entry),
        "paths": [
            {
                "points": [list(point) for point in path.points],
                "exit": endpoint_json(path.exit),
            }
            for path in runtime_map.paths
        ],
        "hasCoast": runtime_map.has_coast,
        "hasForest": runtime_map.has_forest,
        "layers": runtime_map.layers,
    }


def render(runtime_map):
    tileset = Image.open(TILESET).convert("RGBA")
    autotiles = load_autotiles()
    rendered_layers = [render_layer(layer, tileset, autotiles) for layer in runtime_map.layers]
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
    label = (
        f"v{ALGORITHM_VERSION} seed={runtime_map.seed} "
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
    parser.add_argument("--count", type=int, choices=range(1, 5))
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
        runtime_map = generate_map(map_seed, entry)
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
            **{key: payload[key] for key in ("seed", "fingerprint", "entry", "hasCoast", "hasForest")},
        })
        clean_images.append(clean)
        debug_images.append(debug)
        entry = opposite(runtime_map.paths[selected_exit].exit.edge)
        print(
            f"map={index + 1} seed=0x{map_seed:08x} fingerprint={fingerprint(runtime_map):08x} "
            f"entry={EDGE_NAMES[runtime_map.entry.edge]} coast={int(runtime_map.has_coast)} "
            f"forest={int(runtime_map.has_forest)}"
        )

    manifest_path = args.output_dir / "runtime_tile_map_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    make_overview(clean_images, args.output_dir / "runtime_tile_maps.png")
    make_overview(debug_images, args.output_dir / "runtime_tile_maps_debug.png")
    print(f"manifest={manifest_path}")


if __name__ == "__main__":
    main()
