#!/usr/bin/env python3

import argparse
import json
from collections import deque
from math import floor

from PIL import Image, ImageDraw, ImageFont

from generate_explore_map import (
    DEBUG_FONT_PATHS,
    DEBUG_DIR,
    OUTPUT_DIR,
    build_semantic_map,
    export_map,
    render_semantic_debug,
    write_semantic_map,
)
from generate_typical_map_run import MAP_H, MAP_W, build_map


DEFAULT_SEED = 20260713

GRASS_PATH_MAPS = (
    {
        "key": "grass_path_01",
        "label": "草丛小路 01",
        "seed_offset": 110,
        "entry": (7.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(7.5, -0.5), (15.5, 3.5)],
        "routes": [
            [(7.5, 11.5), (7.5, 3.5), (7.5, -0.5)],
            [(7.5, 11.5), (7.5, 3.5), (15.5, 3.5)],
        ],
        "dense": [(0, 0, 6, 3), (0, 4, 5, 3), (4, 8, 3, 4)],
        "left_coasts": [5],
        "ponds": [],
        "flowers": [
            (5, 1), (5, 5), (0, 7), (4, 10),
            (9, 1), (12, 1), (14, 2), (10, 4),
        ],
        "forest_stamps": [(10, 5, 6, 11)],
        "run_exit": 1,
    },
    {
        "key": "grass_path_02",
        "label": "草丛小路 02",
        "seed_offset": 220,
        "entry": (-0.5, 6.5),
        "entry_edge": "left",
        "exits": [(5.5, -0.5), (11.5, 11.5)],
        "routes": [
            [(-0.5, 6.5), (5.5, 6.5), (5.5, -0.5)],
            [(-0.5, 6.5), (5.5, 6.5), (11.5, 6.5), (11.5, 11.5)],
        ],
        "dense": [(0, 0, 5, 5), (8, 0, 8, 5), (0, 8, 6, 4)],
        "ponds": [],
        "flowers": [
            (1, 5), (3, 5), (8, 5), (10, 5),
            (14, 5), (7, 9), (8, 10), (14, 9),
        ],
        "forest_stamps": [],
        "run_exit": 1,
    },
    {
        "key": "grass_path_03",
        "label": "草丛小路 03",
        "seed_offset": 330,
        "entry": (10.5, -0.5),
        "entry_edge": "top",
        "exits": [(10.5, 11.5), (15.5, 4.5)],
        "routes": [
            [(10.5, -0.5), (10.5, 4.5), (10.5, 11.5)],
            [(10.5, -0.5), (10.5, 4.5), (15.5, 4.5)],
        ],
        "dense": [(0, 0, 8, 4), (0, 7, 7, 5), (13, 6, 3, 6)],
        "left_coasts": [5],
        "ponds": [],
        "flowers": [
            (1, 3), (6, 3), (1, 6), (6, 6),
            (8, 8), (14, 2), (14, 5), (8, 10),
        ],
        "forest_stamps": [],
        "run_exit": 1,
    },
    {
        "key": "grass_path_04",
        "label": "草丛小路 04",
        "seed_offset": 440,
        "entry": (-0.5, 8.5),
        "entry_edge": "left",
        "exits": [(15.5, 8.5), (8.5, -0.5)],
        "routes": [
            [(-0.5, 8.5), (8.5, 8.5), (15.5, 8.5)],
            [(-0.5, 8.5), (8.5, 8.5), (8.5, -0.5)],
        ],
        "dense": [(0, 0, 7, 4), (0, 5, 7, 3), (0, 10, 8, 2)],
        "ponds": [],
        "flowers": [
            (0, 4), (3, 4), (6, 4), (10, 1),
            (10, 4), (7, 6), (4, 7), (12, 10),
        ],
        "forest_stamps": [(12, 1, 4, 7)],
        "run_exit": 0,
    },
)

SCENE_SPECS = {
    "grass_path": {
        "label": "草丛小路",
        "maps": GRASS_PATH_MAPS,
    },
}


def boundary_cell(point):
    x, y = point
    if x < 0:
        return 0, min(MAP_H - 1, max(0, floor(y)))
    if x >= MAP_W:
        return MAP_W - 1, min(MAP_H - 1, max(0, floor(y)))
    if y < 0:
        return min(MAP_W - 1, max(0, floor(x))), 0
    if y >= MAP_H:
        return min(MAP_W - 1, max(0, floor(x))), MAP_H - 1
    return min(MAP_W - 1, max(0, floor(x))), min(MAP_H - 1, max(0, floor(y)))


def shortest_road_route(road_cells, start, target):
    road = set(road_cells)
    if start not in road or target not in road:
        raise RuntimeError(f"route endpoint outside road: {start}->{target}")
    queue = deque([start])
    previous = {start: None}
    while queue:
        point = queue.popleft()
        if point == target:
            break
        x, y = point
        for neighbor in ((x, y - 1), (x + 1, y), (x, y + 1), (x - 1, y)):
            if neighbor in road and neighbor not in previous:
                previous[neighbor] = point
                queue.append(neighbor)
    if target not in previous:
        raise RuntimeError(f"no road route: {start}->{target}")
    route = []
    point = target
    while point is not None:
        route.append(point)
        point = previous[point]
    return list(reversed(route))


def integer_route_spec(map_spec, seed, road_cells, map_index, map_count):
    entry = boundary_cell(map_spec["entry"])
    routes = []
    for exit_point in map_spec["exits"]:
        exit_cell = boundary_cell(exit_point)
        route = shortest_road_route(road_cells, entry, exit_cell)
        routes.append({"exit": exit_cell, "route": route[1:]})
    return {
        "key": map_spec["key"],
        "label": map_spec["label"],
        "map_id": 0,
        "crop": (0, 0),
        "entry": entry,
        "routes": routes,
        "debug_title": f'{map_spec["label"]} / {map_index + 1}/{map_count} / SEED {seed}',
    }


def build_tileset_data(layers, scene_name):
    # Map005 uses the same Outside tileset and supplies its authoritative
    # passage, priority and terrain-tag tables for the generated tile IDs.
    data = export_map(5)
    data.update({
        "mapId": 0,
        "name": scene_name,
        "width": MAP_W,
        "height": MAP_H,
        "layers": layers,
        "events": [],
    })
    return data


def load_overview_font(size):
    for path in DEBUG_FONT_PATHS:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def edge_name(point):
    x, y = point
    if x < 0:
        return "LEFT"
    if x >= MAP_W - 0.5:
        return "RIGHT"
    if y < 0:
        return "TOP"
    if y >= MAP_H - 0.5:
        return "BOTTOM"
    raise ValueError(f"point is not on a map exit: {point}")


def make_overview(images, map_specs, title, output_path, show_map_labels=True):
    columns = 2
    rows = (len(images) + columns - 1) // columns
    gap = 8
    header = 34
    footer = 28
    cell_w = max(image.width for image in images)
    cell_h = max(image.height for image in images)
    canvas = Image.new(
        "RGBA",
        (columns * cell_w + (columns + 1) * gap,
         header + rows * cell_h + (rows + 1) * gap + footer),
        (20, 27, 29, 255),
    )
    draw = ImageDraw.Draw(canvas, "RGBA")
    title_font = load_overview_font(17)
    label_font = load_overview_font(12)
    draw.text((gap, 7), title, fill=(255, 230, 90, 255), font=title_font)
    for index, (image, spec) in enumerate(zip(images, map_specs)):
        x = gap + (index % columns) * (cell_w + gap)
        y = header + gap + (index // columns) * (cell_h + gap)
        canvas.alpha_composite(image, (x, y))
        if show_map_labels:
            draw.rectangle((x + 4, y + 4, x + 116, y + 25), fill=(15, 22, 24, 215))
            draw.text((x + 9, y + 7), spec["label"], fill=(255, 255, 255, 255), font=label_font)
    footer_y = header + rows * cell_h + (rows + 1) * gap
    transitions = "   ".join(
        f"{index + 1:02d} {edge_name(spec['exits'][spec['run_exit']])} -> "
        f"{index + 2:02d} {map_specs[index + 1]['entry_edge'].upper()}"
        for index, spec in enumerate(map_specs[:-1])
    )
    draw.text(
        (gap, footer_y + 5),
        transitions,
        fill=(128, 190, 255, 255),
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)


def relative_output(path):
    return str(path.relative_to(OUTPUT_DIR.parents[2]))


def generate_scene(scene_key, seed, count=4):
    scene = SCENE_SPECS[scene_key]
    if count < 2 or count > len(scene["maps"]):
        raise ValueError(f"map count must be between 2 and {len(scene['maps'])}")
    map_specs = list(scene["maps"][:count])
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    DEBUG_DIR.mkdir(parents=True, exist_ok=True)
    clean_images = []
    route_images = []
    debug_images = []
    outputs = []

    for map_index, map_spec in enumerate(map_specs):
        clean, routes, generation = build_map(
            map_spec,
            seed,
            return_generation_data=True,
        )
        map_path = OUTPUT_DIR / f"explore_{map_spec['key']}_416.png"
        route_path = DEBUG_DIR / f"explore_{map_spec['key']}_routes.png"
        clean.save(map_path)
        routes.save(route_path)

        semantic_spec = integer_route_spec(
            map_spec,
            seed,
            generation["road_cells"],
            map_index,
            len(map_specs),
        )
        data = build_tileset_data(generation["layers"], map_spec["label"])
        semantic = build_semantic_map(data, semantic_spec)
        semantic["source"].update({
            "generated": True,
            "seed": seed,
            "variant": map_spec["key"],
            "tilesetReferenceMapId": 5,
        })
        semantic["generation"] = {
            "roadCells": [list(point) for point in generation["road_cells"]],
            "waterCells": [list(point) for point in generation["water_cells"]],
            "solidScenery": [list(point) for point in generation["solid_scenery"]],
        }
        binary_path, json_path = write_semantic_map(semantic, map_spec["key"])
        debug = render_semantic_debug(semantic_spec, clean, semantic)
        debug_path = DEBUG_DIR / f"explore_{map_spec['key']}_semantics.png"
        debug.save(debug_path)

        if map_index == 0:
            clean.save(OUTPUT_DIR / f"explore_{scene_key}_416.png")
            routes.save(DEBUG_DIR / f"explore_{scene_key}_routes.png")
            debug.save(DEBUG_DIR / f"explore_{scene_key}_semantics.png")
            write_semantic_map(semantic, scene_key)

        clean_images.append(clean)
        route_images.append(routes)
        debug_images.append(debug)
        outputs.append({
            "key": map_spec["key"],
            "map": map_path,
            "routes": route_path,
            "debug": debug_path,
            "binary": binary_path,
            "json": json_path,
            "stats": semantic["stats"],
            "entry": semantic["entry"],
            "exits": semantic["exits"],
            "runExit": map_spec["run_exit"],
        })

    overview_path = OUTPUT_DIR / f"explore_{scene_key}_expedition.png"
    route_overview_path = DEBUG_DIR / f"explore_{scene_key}_expedition_routes.png"
    debug_overview_path = DEBUG_DIR / f"explore_{scene_key}_expedition_semantics.png"
    make_overview(clean_images, map_specs, f'{scene["label"]} / {count} MAP EXPEDITION', overview_path)
    make_overview(route_images, map_specs, f'{scene["label"]} / ROUTES', route_overview_path)
    make_overview(
        debug_images,
        map_specs,
        f'{scene["label"]} / SEMANTICS',
        debug_overview_path,
        show_map_labels=False,
    )

    manifest_path = OUTPUT_DIR / "maps" / f"explore_{scene_key}_expedition.json"
    manifest = {
        "format": "StickMonExploreExpedition",
        "version": 1,
        "scene": scene_key,
        "label": scene["label"],
        "seed": seed,
        "mapCount": len(outputs),
        "maps": [
            {
                "key": item["key"],
                "map": relative_output(item["map"]),
                "semantic": relative_output(item["binary"]),
                "entry": item["entry"],
                "exits": item["exits"],
                "selectedExit": item["runExit"],
            }
            for item in outputs
        ],
        "transitions": [
            {
                "from": outputs[index]["key"],
                "exit": outputs[index]["runExit"],
                "to": outputs[index + 1]["key"],
            }
            for index in range(len(outputs) - 1)
        ],
    }
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {
        "maps": outputs,
        "overview": overview_path,
        "routeOverview": route_overview_path,
        "debugOverview": debug_overview_path,
        "manifest": manifest_path,
    }


def main():
    parser = argparse.ArgumentParser(description="Generate a named StickMon exploration map")
    parser.add_argument("--scene", choices=sorted(SCENE_SPECS), default="grass_path")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--count", type=int, choices=range(2, 5), default=4)
    args = parser.parse_args()

    result = generate_scene(args.scene, args.seed, args.count)
    print(f"scene={args.scene} seed={args.seed} maps={len(result['maps'])}")
    for item in result["maps"]:
        print(f"map={item['key']} stats={item['stats']} image={item['map']} debug={item['debug']}")
    for kind in ("overview", "routeOverview", "debugOverview", "manifest"):
        print(f"{kind}={result[kind]}")


if __name__ == "__main__":
    main()
