#!/usr/bin/env python3

import argparse
import random
from pathlib import Path

from generate_typical_map_run import build_map, make_overview


LANDMARK_MAP_SPECS = [
    {
        "key": "01_lighthouse_cove",
        "seed_offset": 511,
        "entry": (11.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(11.5, -0.5), (15.5, 6.5)],
        "routes": [
            [(11.5, 11.5), (11.5, 6.5), (11.5, -0.5)],
            [(11.5, 11.5), (11.5, 6.5), (15.5, 6.5)],
        ],
        "left_coasts": [6],
        "lighthouses": [(2, 3)],
        "high_grass": [(5, 3, 4, 6)],
        "dense": [(0, 9, 8, 3)],
        "flowers": [(7, 2), (9, 4), (4, 10)],
        "forest_stamps": [],
    },
    {
        "key": "02_high_grass_fork",
        "seed_offset": 622,
        "entry": (-0.5, 5.5),
        "entry_edge": "left",
        "exits": [(6.5, -0.5), (6.5, 11.5)],
        "routes": [
            [(-0.5, 5.5), (6.5, 5.5), (6.5, -0.5)],
            [(-0.5, 5.5), (6.5, 5.5), (6.5, 11.5)],
        ],
        "water_rects": [(11, 0, 5, 4)],
        "high_grass": [(0, 0, 5, 5), (10, 7, 6, 5)],
        "dense": [(0, 8, 5, 4)],
        "flowers": [(9, 1), (10, 5), (14, 6), (9, 10)],
        "forest_stamps": [],
    },
    {
        "key": "03_forest_meadow",
        "seed_offset": 733,
        "entry": (3.5, -0.5),
        "entry_edge": "top",
        "exits": [(-0.5, 6.5), (3.5, 11.5)],
        "routes": [
            [(3.5, -0.5), (3.5, 6.5), (-0.5, 6.5)],
            [(3.5, -0.5), (3.5, 6.5), (3.5, 11.5)],
        ],
        "water_rects": [(6, 8, 3, 4)],
        "high_grass": [(6, 0, 4, 4)],
        "dense": [(0, 8, 3, 4)],
        "flowers": [(1, 2), (7, 2), (8, 5), (6, 7)],
        "forest_stamps": [(10, 5, 6, 11)],
    },
    {
        "key": "04_lakeside_split",
        "seed_offset": 844,
        "entry": (15.5, 8.5),
        "entry_edge": "right",
        "exits": [(9.5, -0.5), (-0.5, 8.5)],
        "routes": [
            [(15.5, 8.5), (9.5, 8.5), (9.5, -0.5)],
            [(15.5, 8.5), (9.5, 8.5), (-0.5, 8.5)],
        ],
        "water_rects": [(0, 0, 6, 5)],
        "high_grass": [(0, 5, 6, 4)],
        "dense": [(12, 0, 4, 5)],
        "flowers": [(7, 1), (7, 5), (13, 6), (4, 7)],
        "forest_stamps": [],
    },
]


def main():
    parser = argparse.ArgumentParser(
        description="Generate a 2-4 map StickMon landmark run preview"
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/tmp/stickmon-landmark-map-run"),
    )
    parser.add_argument("--seed", type=int, default=20260713)
    parser.add_argument("--count", type=int, choices=range(2, 5))
    args = parser.parse_args()

    run_count = args.count if args.count else random.Random(args.seed).randint(2, 4)
    specs = LANDMARK_MAP_SPECS[:run_count]
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
        "landmark_run_clean_overview.png",
        False,
        args.output_dir,
    )
    make_overview(
        route_maps,
        specs,
        args.seed,
        "landmark_run_routes_overview.png",
        True,
        args.output_dir,
    )
    print(f"seed={args.seed} maps={run_count}")
    print(args.output_dir / "landmark_run_clean_overview.png")
    print(args.output_dir / "landmark_run_routes_overview.png")


if __name__ == "__main__":
    main()
