#!/usr/bin/env python3

"""Render previews from the current C++ Frost Crystal Cave generator."""

import argparse
import json
import subprocess
import tempfile
from pathlib import Path
from types import SimpleNamespace

try:
    from PIL import Image, ImageDraw, ImageFont
except ModuleNotFoundError as exc:
    raise SystemExit(
        "Pillow is required. Run with the ESP-IDF Python, for example:\n"
        "/Users/gtleaf/.espressif/python_env/idf5.5_py3.11_env/bin/python "
        "tools/generate_frost_crystal_cave_preview.py"
    ) from exc


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
EDGE_NAMES = ("top", "right", "bottom", "left")
DEFAULT_SEEDS = (
    0x20260713,
    0x13579BDF,
    0xC0FFEE01,
    0xDEADBEEF,
)

CPP_DUMP_SOURCE = r'''
#include "game/ExploreMapGenerator.h"

#include <cstdio>
#include <cstdlib>

static const char* edgeName(ExploreMapGenerator::Edge edge) {
    switch (edge) {
        case ExploreMapGenerator::Edge::TOP: return "top";
        case ExploreMapGenerator::Edge::RIGHT: return "right";
        case ExploreMapGenerator::Edge::BOTTOM: return "bottom";
        case ExploreMapGenerator::Edge::LEFT: return "left";
    }
    return "unknown";
}

int main(int argc, char** argv) {
    if (argc != 6) return 2;
    const uint32_t seed = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 0));
    const unsigned edgeValue = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 0));
    if (edgeValue > 3) return 2;
    ExploreMapGenerator::FrostContext frost{
        static_cast<uint8_t>(std::strtoul(argv[3], nullptr, 0)),
        static_cast<uint8_t>(std::strtoul(argv[4], nullptr, 0)),
        std::strtoul(argv[5], nullptr, 0) != 0};

    ExploreMapGenerator::Map map;
    if (!ExploreMapGenerator::generate(
            seed, static_cast<ExploreMapGenerator::Edge>(edgeValue),
            ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA, map, frost)) {
        return 1;
    }

    std::printf(
        "{\"seed\":%u,\"fingerprint\":\"%08x\","
        "\"entry\":{\"x\":%u,\"y\":%u,\"edge\":\"%s\"},"
        "\"junction\":{\"x\":%u,\"y\":%u},\"layers\":[",
        map.seed, ExploreMapGenerator::fingerprint(map), map.entry.point.x,
        map.entry.point.y, edgeName(map.entry.edge), map.junction.x, map.junction.y);

    for (unsigned layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        if (layer) std::putchar(',');
        std::putchar('[');
        for (unsigned index = 0; index < ExploreMapGenerator::CELL_COUNT; ++index) {
            if (index) std::putchar(',');
            std::printf("%u", map.layers[layer][index]);
        }
        std::putchar(']');
    }

    std::printf("],\"paths\":[");
    for (unsigned pathIndex = 0; pathIndex < map.pathCount; ++pathIndex) {
        if (pathIndex) std::putchar(',');
        const auto& path = map.paths[pathIndex];
        std::printf("{\"points\":[");
        for (unsigned index = 0; index < path.pointCount; ++index) {
            if (index) std::putchar(',');
            std::printf("[%u,%u]", path.points[index].x, path.points[index].y);
        }
        std::printf(
            "],\"exit\":{\"x\":%u,\"y\":%u,\"edge\":\"%s\"},"
            "\"fallsToNextLevel\":%s}",
            path.exit.point.x, path.exit.point.y, edgeName(path.exit.edge),
            path.fallsToNextLevel ? "true" : "false");
    }
    std::puts("]}");
    return 0;
}
'''


def parse_uint(value):
    return int(value, 0) & 0xFFFFFFFF


def as_runtime_map(payload):
    runtime_map = SimpleNamespace(
        **{key: convert(value) for key, value in payload.items()}
    )
    runtime_map.entry.point = (runtime_map.entry.x, runtime_map.entry.y)
    runtime_map.entry.edge = EDGE_NAMES.index(runtime_map.entry.edge)
    runtime_map.junction = (runtime_map.junction.x, runtime_map.junction.y)
    runtime_map.area_index = 3
    runtime_map.has_coast = False
    runtime_map.has_forest = False
    runtime_map.has_creek = False
    runtime_map.has_cliff = False
    runtime_map.has_waterfall = False
    for path in runtime_map.paths:
        path.falls_to_next_level = path.fallsToNextLevel
        path.exit.point = (path.exit.x, path.exit.y)
        path.exit.edge = EDGE_NAMES.index(path.exit.edge)
    return runtime_map


def convert(value):
    if isinstance(value, dict):
        return SimpleNamespace(**{key: convert(item) for key, item in value.items()})
    if isinstance(value, list):
        return [convert(item) for item in value]
    return value


def build_dump_host(build_dir):
    source = build_dir / "frost_crystal_cave_dump.cpp"
    binary = build_dir / "frost_crystal_cave_dump"
    source.write_text(CPP_DUMP_SOURCE, encoding="utf-8")
    subprocess.run(
        [
            "c++",
            "-std=c++17",
            f"-I{ROOT / 'src'}",
            str(source),
            str(ROOT / "src/game/ExploreMapGenerator.cpp"),
            "-o",
            str(binary),
        ],
        check=True,
    )
    return binary


def load_map(binary, seed, edge, level=0, level_count=1, entered_by_ladder=False):
    raw = subprocess.check_output(
        [str(binary), hex(seed), str(edge), str(level), str(level_count),
         str(int(entered_by_ladder))], text=True
    )
    return as_runtime_map(json.loads(raw))


def annotate(image, runtime_map, index):
    label = (
        f"Frost Crystal Cave #{index}  seed={runtime_map.seed:08x}  "
        f"fp={runtime_map.fingerprint}  entry={EDGE_NAMES[runtime_map.entry.edge]}"
    )
    image = image.convert("RGBA")
    draw = ImageDraw.Draw(image, "RGBA")
    draw.rectangle(
        (0, 0, min(image.width, len(label) * 7 + 10), 20),
        fill=(10, 17, 21, 220),
    )
    draw.text((5, 4), label, fill=(255, 242, 112, 255), font=ImageFont.load_default())
    return image


def main():
    parser = argparse.ArgumentParser(
        description="Render Frost Crystal Cave previews from the current C++ generator"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "out/frost_crystal_cave_previews",
    )
    parser.add_argument("--seed", type=parse_uint, nargs="+")
    parser.add_argument("--levels", action="store_true",
                        help="Render seeds as consecutive cave levels")
    parser.add_argument(
        "--edges",
        nargs="+",
        choices=EDGE_NAMES,
        default=list(EDGE_NAMES),
        help="Entry edges, repeated cyclically for the supplied seeds",
    )
    args = parser.parse_args()

    seeds = args.seed or list(DEFAULT_SEEDS)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    from generate_runtime_tile_map import render, render_debug

    with tempfile.TemporaryDirectory(prefix="stickmon-frost-preview-") as temp_dir:
        binary = build_dump_host(Path(temp_dir))
        images = []
        manifest = []
        for index, seed in enumerate(seeds, 1):
            edge_name = args.edges[(index - 1) % len(args.edges)]
            edge = EDGE_NAMES.index(edge_name)
            runtime_map = load_map(
                binary, seed, edge, index - 1 if args.levels else 0,
                len(seeds) if args.levels else 1,
                args.levels and index > 1 and (index % 2 == 0),
            )
            clean = annotate(render(runtime_map), runtime_map, index)
            debug = annotate(render_debug(runtime_map, clean), runtime_map, index)
            stem = f"frost_crystal_cave_{index:02d}_{seed:08x}_{edge_name}"
            clean_path = args.output_dir / f"{stem}.png"
            debug_path = args.output_dir / f"{stem}_debug.png"
            clean.save(clean_path)
            debug.save(debug_path)
            images.append(debug)
            manifest.append({
                "image": str(clean_path),
                "debug": str(debug_path),
                "seed": f"0x{seed:08x}",
                "entry": edge_name,
                "fingerprint": runtime_map.fingerprint,
            })

    width, height = images[0].size
    gap = 12
    columns = 2
    rows = (len(images) + columns - 1) // columns
    overview = Image.new(
        "RGBA",
        (columns * width + (columns + 1) * gap,
         rows * height + (rows + 1) * gap),
        (20, 27, 31, 255),
    )
    for index, image in enumerate(images):
        overview.alpha_composite(
            image,
            (gap + (index % columns) * (width + gap),
             gap + (index // columns) * (height + gap)),
        )
    overview_path = args.output_dir / "frost_crystal_cave_overview_debug.png"
    overview.save(overview_path)
    (args.output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )

    print(overview_path)
    for item in manifest:
        print(item["image"])
        print(item["debug"])


if __name__ == "__main__":
    main()
