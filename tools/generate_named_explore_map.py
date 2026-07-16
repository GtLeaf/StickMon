#!/usr/bin/env python3

import argparse
import json
from collections import deque
from math import floor

from PIL import Image, ImageDraw, ImageFont

from generate_map_rule_preview import WATERFALL_AUTOTILE_NAMES
from map_generation_rules import CUSTOM_TILE_VERTICAL_FLIPS

from generate_explore_map import (
    DEBUG_FONT_PATHS,
    DEBUG_DIR,
    OUTPUT_DIR,
    build_semantic_map,
    export_map,
    render_semantic_debug,
    write_semantic_map,
)
from generate_typical_map_run import (
    CAVE_ARCHIPELAGO_TILE_METADATA_SOURCES,
    CAVE_ARCHIPELAGO_TILE_SOURCES,
    CAVE_BRIDGE_TILES,
    CAVE_GROUND_TILES,
    CAVE_PATH_TILE,
    MAP_H,
    MAP_W,
    build_map,
    render_clean_frames,
)


DEFAULT_SEED = 20260713

CAVE_WATERFALL_AUTOTILE_NAMES = (
    "Sea",
    "Sea without shore",
    "Sea deep",
    "",
    "Waterfall crest",
    "Waterfall",
    "Waterfall bottom",
)

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

CREEK_BRIDGE_SLOPE_MAPS = (
    {
        "key": "creek_bridge_slope_01",
        "label": "溪谷入口 01",
        "seed_offset": 510,
        "entry": (3.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(15.5, 7.5), (3.5, -0.5)],
        "routes": [
            [(3.5, 11.5), (3.5, 7.5), (15.5, 7.5)],
            [(3.5, 11.5), (3.5, 7.5), (3.5, -0.5)],
        ],
        "vertical_streams": [
            {"left": 8, "width": 3, "top": 0, "bottom": 12},
        ],
        "horizontal_bridges": [{"left": 7, "top": 7, "length": 6}],
        "water_rocks": [
            {"left": 9, "top": 3, "kind": "small"},
            {"left": 10, "top": 10, "kind": "small"},
        ],
        "boulders": [(7, 3, 1231), (12, 10, 1231)],
        "dense": [
            (0, 0, 3, 6), (5, 0, 3, 6), (12, 0, 4, 6),
            (5, 9, 3, 3), (12, 9, 4, 3),
        ],
        "flowers": [
            (5, 6), (6, 6), (7, 6),
            (13, 6), (14, 6), (15, 6),
            (10, 10), (11, 10),
        ],
        "run_exit": 0,
    },
    {
        "key": "creek_bridge_slope_02",
        "label": "岩阶分水 02",
        "seed_offset": 620,
        "entry": (-0.5, 8.5),
        "entry_edge": "left",
        "exits": [(8.5, -0.5), (15.5, 3.5)],
        "routes": [
            [(-0.5, 8.5), (8.5, 8.5), (8.5, 3.5), (8.5, -0.5)],
            [(-0.5, 8.5), (8.5, 8.5), (8.5, 3.5), (15.5, 3.5)],
        ],
        "horizontal_cliffs": [
            {"top": 5, "left": 0, "right": 16, "stair_left": 8},
        ],
        "boulders": [(14, 9, 1231)],
        "dense": [
            (0, 0, 6, 5), (11, 0, 5, 3),
            (0, 10, 7, 2), (11, 7, 5, 5),
        ],
        "flowers": [
            (5, 3), (5, 4), (6, 4),
            (11, 3), (12, 3), (12, 4),
            (10, 9), (10, 10),
        ],
        "run_exit": 0,
    },
    {
        "key": "creek_bridge_slope_03",
        "label": "双层溪桥 03",
        "seed_offset": 730,
        "entry": (11.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(-0.5, 3.5), (11.5, -0.5)],
        "routes": [
            [(11.5, 11.5), (11.5, 3.5), (-0.5, 3.5)],
            [(11.5, 11.5), (11.5, 3.5), (11.5, -0.5)],
        ],
        "vertical_streams": [
            {"left": 3, "width": 5, "top": 0, "bottom": 9},
            {"left": 4, "width": 4, "top": 9, "bottom": 12},
        ],
        "horizontal_bridges": [
            {"left": 2, "top": 3, "length": 7, "height": 3},
        ],
        "water_rocks": [
            {"left": 5, "top": 1, "kind": "large"},
            {"left": 6, "top": 9, "kind": "small"},
        ],
        "horizontal_cliffs": [
            {"top": 7, "left": 8, "right": 16, "stair_left": 11},
        ],
        "boulders": [(2, 1, 1231), (15, 5, 1231)],
        "dense": [
            (0, 0, 4, 3), (8, 0, 3, 3),
            (0, 6, 4, 6), (13, 9, 3, 3),
        ],
        "flowers": [
            (1, 3), (1, 4), (2, 4),
            (8, 5), (9, 5), (9, 6),
            (13, 9), (14, 9),
        ],
        "run_exit": 0,
    },
    {
        "key": "creek_bridge_slope_04",
        "label": "河湾岔路 04",
        "seed_offset": 840,
        "entry": (15.5, 6.5),
        "entry_edge": "right",
        "exits": [(4.5, -0.5), (4.5, 11.5)],
        "routes": [
            [(15.5, 6.5), (4.5, 6.5), (4.5, -0.5)],
            [(15.5, 6.5), (4.5, 6.5), (4.5, 11.5)],
        ],
        "vertical_streams": [
            {"left": 8, "width": 4, "top": 0, "bottom": 10},
            {"left": 7, "width": 5, "top": 10, "bottom": 12},
        ],
        "horizontal_bridges": [{"left": 6, "top": 6, "length": 7}],
        "water_rocks": [
            {"left": 9, "top": 3, "kind": "small"},
            {"left": 10, "top": 10, "kind": "small"},
        ],
        "boulders": [(7, 2, 1231), (12, 10, 1231)],
        "dense": [
            (0, 0, 5, 5), (11, 0, 5, 5),
            (0, 8, 5, 4), (11, 9, 5, 3),
        ],
        "flowers": [
            (2, 4), (3, 4), (3, 5),
            (11, 4), (12, 4), (12, 5),
            (2, 8), (3, 8), (3, 9),
        ],
        "run_exit": 1,
    },
)

ANCIENT_WATERFALL_VALLEY_MAPS = (
    {
        "key": "ancient_waterfall_valley_01",
        "label": "谷口侧瀑 01",
        "seed_offset": 910,
        "entry": (11.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(15.5, 9.5), (11.5, -0.5)],
        "routes": [
            [(11.5, 11.5), (11.5, 9.5), (15.5, 9.5)],
            [(11.5, 11.5), (11.5, 9.5), (11.5, -0.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 4, "top": 0, "width": 4, "bottom": 3},
            {
                "left": 3,
                "top": 8,
                "width": 6,
                "top_open_spans": [(4, 4)],
            },
        ],
        "waterfalls": [
            {"left": 4, "width": 4, "crest_top": 3, "body_height": 3},
        ],
        "waterfall_walls": [
            {"top": 3, "bottom": 8, "gaps": [(4, 4)], "stair_left": 11},
        ],
        "water_rocks": [{"left": 6, "top": 9, "kind": "small"}],
        "top_edge_forest_clusters": [
            {"left": 0, "width": 4},
            {"left": 13, "width": 2},
        ],
        "boulders": [(9, 1, 1231), (14, 11, 1231)],
        "dense": [(8, 0, 3, 3), (0, 8, 3, 4), (9, 8, 2, 4)],
        "flowers": [(8, 1), (10, 1), (1, 8), (2, 8), (9, 10), (10, 11)],
        "run_exit": 0,
    },
    {
        "key": "ancient_waterfall_valley_02",
        "label": "石阶高台 02",
        "seed_offset": 1020,
        "entry": (-0.5, 2.5),
        "entry_edge": "left",
        "exits": [(4.5, 11.5), (4.5, -0.5)],
        "routes": [
            [(-0.5, 2.5), (4.5, 2.5), (4.5, 11.5)],
            [(-0.5, 2.5), (4.5, 2.5), (4.5, -0.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 10, "top": 0, "width": 4, "bottom": 4},
            {
                "left": 9,
                "top": 9,
                "width": 6,
                "top_open_spans": [(10, 4)],
            },
        ],
        "waterfalls": [
            {"left": 10, "width": 4, "crest_top": 4, "body_height": 3},
        ],
        "waterfall_walls": [
            {"top": 4, "bottom": 9, "gaps": [(10, 4)], "stair_left": 4},
        ],
        "water_rocks": [{"left": 11, "top": 10, "kind": "small"}],
        "top_edge_forest_clusters": [
            {"left": 6, "width": 4},
            {"left": 14, "width": 2},
        ],
        "boulders": [(7, 3, 1231), (8, 10, 1231)],
        "dense": [(0, 0, 4, 2), (6, 0, 4, 4), (0, 9, 4, 3), (6, 9, 3, 3)],
        "flowers": [(0, 1), (1, 1), (7, 3), (8, 3), (6, 8), (7, 8)],
        "run_exit": 0,
    },
    {
        "key": "ancient_waterfall_valley_03",
        "label": "古潭分流 03",
        "seed_offset": 1130,
        "entry": (5.5, -0.5),
        "entry_edge": "top",
        "exits": [(-0.5, 9.5), (5.5, 11.5)],
        "routes": [
            [(5.5, -0.5), (5.5, 9.5), (-0.5, 9.5)],
            [(5.5, -0.5), (5.5, 9.5), (5.5, 11.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 10, "top": 0, "width": 4, "bottom": 3},
            {
                "left": 9,
                "top": 8,
                "width": 6,
                "top_open_spans": [(10, 4)],
            },
        ],
        "waterfalls": [
            {"left": 10, "width": 4, "crest_top": 3, "body_height": 3},
        ],
        "waterfall_walls": [
            {"top": 3, "bottom": 8, "gaps": [(10, 4)], "stair_left": 5},
        ],
        "water_rocks": [{"left": 11, "top": 9, "kind": "small"}],
        "top_edge_forest_clusters": [
            {"left": 0, "width": 4},
            {"left": 7, "width": 2},
            {"left": 14, "width": 2},
        ],
        "boulders": [(2, 8, 1231), (15, 9, 1231)],
        "dense": [(0, 0, 4, 3), (7, 0, 2, 3), (0, 8, 5, 1), (7, 8, 2, 4)],
        "flowers": [(3, 8), (4, 8), (7, 8), (8, 8), (14, 8), (15, 8)],
        "run_exit": 0,
    },
    {
        "key": "ancient_waterfall_valley_04",
        "label": "双瀑深谷 04",
        "seed_offset": 1240,
        "entry": (15.5, 9.5),
        "entry_edge": "right",
        "exits": [(12.5, -0.5), (12.5, 11.5)],
        "routes": [
            [(15.5, 9.5), (12.5, 9.5), (12.5, -0.5)],
            [(15.5, 9.5), (12.5, 9.5), (12.5, 11.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 2, "top": 0, "width": 3, "bottom": 3},
            {"left": 7, "top": 0, "width": 3, "bottom": 3},
            {
                "left": 1,
                "top": 8,
                "width": 10,
                "top_open_spans": [(2, 3), (7, 3)],
            },
        ],
        "waterfalls": [
            {"left": 2, "width": 3, "crest_top": 3, "body_height": 3},
            {"left": 7, "width": 3, "crest_top": 3, "body_height": 3},
        ],
        "waterfall_walls": [
            {
                "top": 3,
                "bottom": 8,
                "gaps": [(2, 3), (7, 3)],
                "stair_left": 12,
            },
        ],
        "water_rocks": [
            {"left": 5, "top": 9, "kind": "large"},
            {"left": 3, "top": 10, "kind": "small"},
        ],
        "top_edge_forest_clusters": [
            {"left": 0, "width": 2},
            {"left": 10, "width": 2},
            {"left": 14, "width": 2},
        ],
        "boulders": [(6, 1, 1231), (11, 9, 1231)],
        "dense": [(0, 0, 2, 3), (10, 0, 2, 3), (14, 0, 2, 3), (11, 8, 1, 4)],
        "flowers": [(0, 7), (11, 8), (14, 8), (15, 8), (11, 11)],
        "run_exit": 0,
    },
)

WATERFALL_PLATEAU_MAPS = (
    {
        "key": "waterfall_plateau_01",
        "label": "环瀑台阶 01",
        "seed_offset": 1310,
        "entry": (6.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(15.5, 9.5), (6.5, -0.5)],
        "routes": [
            [(6.5, 11.5), (6.5, 9.5), (15.5, 9.5)],
            [(6.5, 11.5), (6.5, 9.5), (6.5, -0.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 10, "top": 0, "width": 5, "bottom": 3},
            {
                "left": 9,
                "top": 8,
                "width": 7,
                "top_open_spans": [(10, 5)],
            },
        ],
        "waterfalls": [
            {"left": 10, "width": 5, "crest_top": 3, "body_height": 3},
        ],
        "waterfall_walls": [
            {"top": 3, "bottom": 8, "gaps": [(10, 5)], "stair_left": 6},
        ],
        "horizontal_bridges": [
            {"left": 8, "top": 9, "length": 8},
        ],
        "water_rocks": [
            {"left": 12, "top": 1, "kind": "small"},
            {"left": 12, "top": 11, "kind": "small"},
        ],
        "top_edge_forest_clusters": [
            {"left": 0, "width": 4},
        ],
        "boulders": [(8, 1, 1231), (3, 9, 1231)],
        "dense": [(4, 0, 2, 3), (0, 8, 3, 4), (4, 8, 2, 4)],
        "flowers": [(4, 1), (5, 1), (1, 8), (2, 8), (4, 10), (5, 11)],
        "run_exit": 0,
    },
    {
        "key": "waterfall_plateau_02",
        "label": "双瀑中洲 02",
        "seed_offset": 1420,
        "entry": (-0.5, 2.5),
        "entry_edge": "left",
        "exits": [(15.5, 2.5), (7.5, 11.5)],
        "routes": [
            [(-0.5, 2.5), (7.5, 2.5), (15.5, 2.5)],
            [(-0.5, 2.5), (7.5, 2.5), (7.5, 11.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 2, "top": 0, "width": 4, "bottom": 4},
            {"left": 10, "top": 0, "width": 4, "bottom": 4},
            {
                "left": 1,
                "top": 8,
                "width": 6,
                "top_open_spans": [(2, 4)],
            },
            {
                "left": 9,
                "top": 8,
                "width": 6,
                "top_open_spans": [(10, 4)],
            },
        ],
        "waterfalls": [
            {"left": 2, "width": 4, "crest_top": 4, "body_height": 2},
            {"left": 10, "width": 4, "crest_top": 4, "body_height": 2},
        ],
        "waterfall_walls": [
            {
                "top": 4,
                "bottom": 8,
                "gaps": [(2, 4), (10, 4)],
                "stair_left": 7,
            },
        ],
        "horizontal_bridges": [
            {"left": 1, "top": 2, "length": 6},
            {"left": 9, "top": 2, "length": 6},
        ],
        "water_rocks": [
            {"left": 3, "top": 10, "kind": "small"},
            {"left": 11, "top": 10, "kind": "small"},
        ],
        "boulders": [(0, 9, 1231), (15, 9, 1231)],
        "dense": [(0, 4, 2, 4), (6, 8, 1, 4), (15, 4, 1, 4)],
        "flowers": [(0, 5), (1, 6), (6, 9), (15, 5), (15, 7)],
        "run_exit": 1,
    },
    {
        "key": "waterfall_plateau_03",
        "label": "双桥分瀑 03",
        "seed_offset": 1530,
        "entry": (7.5, -0.5),
        "entry_edge": "top",
        "exits": [(-0.5, 9.5), (15.5, 9.5)],
        "routes": [
            [(7.5, -0.5), (7.5, 9.5), (-0.5, 9.5)],
            [(7.5, -0.5), (7.5, 9.5), (15.5, 9.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 1, "top": 0, "width": 4, "bottom": 4},
            {"left": 11, "top": 0, "width": 4, "bottom": 4},
            {
                "left": 0,
                "top": 9,
                "width": 6,
                "top_open_spans": [(1, 4)],
            },
            {
                "left": 10,
                "top": 9,
                "width": 6,
                "top_open_spans": [(11, 4)],
            },
        ],
        "waterfalls": [
            {"left": 1, "width": 4, "crest_top": 4, "body_height": 3},
            {"left": 11, "width": 4, "crest_top": 4, "body_height": 3},
        ],
        "waterfall_walls": [
            {
                "top": 4,
                "bottom": 9,
                "gaps": [(1, 4), (11, 4)],
                "stair_left": 7,
            },
        ],
        "horizontal_bridges": [
            {"left": 0, "top": 9, "length": 7},
            {"left": 9, "top": 9, "length": 7},
        ],
        "water_rocks": [
            {"left": 2, "top": 11, "kind": "small"},
            {"left": 13, "top": 11, "kind": "small"},
        ],
        "boulders": [(5, 1, 1231), (9, 2, 1231)],
        "dense": [(5, 0, 2, 4), (9, 0, 2, 4), (6, 9, 1, 3)],
        "flowers": [(5, 2), (6, 3), (9, 1), (10, 3), (6, 11)],
        "run_exit": 1,
    },
    {
        "key": "waterfall_plateau_04",
        "label": "宽潭终台 04",
        "seed_offset": 1640,
        "entry": (-0.5, 9.5),
        "entry_edge": "left",
        "exits": [(12.5, -0.5), (12.5, 11.5)],
        "routes": [
            [(-0.5, 9.5), (12.5, 9.5), (12.5, -0.5)],
            [(-0.5, 9.5), (12.5, 9.5), (12.5, 11.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "sea_channels": [
            {"left": 5, "top": 0, "width": 5, "bottom": 3},
            {
                "left": 3,
                "top": 8,
                "width": 9,
                "top_open_spans": [(5, 5)],
            },
        ],
        "waterfalls": [
            {"left": 5, "width": 5, "crest_top": 3, "body_height": 3},
        ],
        "waterfall_walls": [
            {"top": 3, "bottom": 8, "gaps": [(5, 5)], "stair_left": 12},
        ],
        "horizontal_bridges": [
            {"left": 2, "top": 9, "length": 10},
        ],
        "water_rocks": [
            {"left": 7, "top": 1, "kind": "small"},
            {"left": 6, "top": 11, "kind": "small"},
        ],
        "top_edge_forest_clusters": [
            {"left": 0, "width": 4},
            {"left": 14, "width": 2},
        ],
        "boulders": [(10, 1, 1231), (1, 8, 1231)],
        "dense": [(0, 4, 3, 4), (0, 8, 1, 4), (14, 8, 2, 4)],
        "flowers": [(0, 6), (1, 6), (14, 8), (15, 8), (14, 11)],
        "run_exit": 0,
    },
)

WATERFALL_ARCHIPELAGO_GRASS_MAPS = (
    {
        "key": "waterfall_archipelago_01",
        "label": "环水阶台 01",
        "seed_offset": 1750,
        "entry": (7.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(15.5, 8.5), (7.5, -0.5)],
        "routes": [
            [(7.5, 11.5), (7.5, 8.5), (15.5, 8.5)],
            [(7.5, 11.5), (7.5, 8.5), (7.5, -0.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "water_base": True,
        "land_rows": [
            (0, 5, 11), (1, 5, 11), (2, 5, 11),
            (3, 5, 11), (4, 5, 11), (5, 5, 11), (6, 5, 11),
            (7, 5, 11),
            (8, 4, 11), (9, 4, 11), (10, 4, 11), (11, 5, 10),
            (6, 14, 16), (7, 14, 16), (8, 14, 16),
            (9, 14, 16), (10, 14, 16), (11, 14, 16),
            (9, 0, 2), (10, 0, 2), (11, 0, 2),
        ],
        "waterfalls": [
            {"left": 2, "width": 3, "crest_top": 3, "body_height": 2},
        ],
        "waterfall_walls": [
            {
                "top": 3,
                "bottom": 7,
                "left": 1,
                "right": 12,
                "gaps": [(2, 3)],
                "stair_left": 7,
                "terraced": True,
            },
        ],
        "horizontal_bridges": [
            {"left": 9, "top": 8, "length": 7},
        ],
        "water_rocks": [
            {"left": 1, "top": 1, "kind": "small"},
            {"left": 12, "top": 5, "kind": "small"},
        ],
        "boulders": [(5, 1, 1231), (15, 6, 1231), (1, 10, 1231)],
        "dense": [(5, 0, 2, 3), (4, 9, 2, 3)],
        "flowers": [(9, 1), (5, 8), (6, 10), (15, 10)],
        "run_exit": 0,
    },
    {
        "key": "waterfall_archipelago_02",
        "label": "横桥双岛 02",
        "seed_offset": 1860,
        "entry": (-0.5, 8.5),
        "entry_edge": "left",
        "exits": [(9.5, 11.5), (9.5, -0.5)],
        "routes": [
            [(-0.5, 8.5), (9.5, 8.5), (9.5, 11.5)],
            [(-0.5, 8.5), (9.5, 8.5), (9.5, -0.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "water_base": True,
        "land_rows": [
            (7, 0, 5), (8, 0, 5), (9, 0, 5),
            (10, 0, 5), (11, 0, 5),
            (0, 8, 13), (1, 8, 13), (2, 8, 13),
            (3, 7, 13), (4, 7, 13), (5, 7, 13),
            (6, 7, 13), (7, 7, 13),
            (8, 8, 13), (9, 8, 13), (10, 8, 13), (11, 8, 12),
        ],
        "waterfalls": [
            {"left": 13, "width": 3, "crest_top": 4, "body_height": 2},
        ],
        "waterfall_walls": [
            {
                "top": 4,
                "bottom": 8,
                "left": 6,
                "right": 16,
                "gaps": [(13, 3)],
                "stair_left": 9,
                "terraced": True,
            },
        ],
        "horizontal_bridges": [
            {"left": 3, "top": 8, "length": 6},
        ],
        "water_rocks": [
            {"left": 3, "top": 2, "kind": "large"},
            {"left": 14, "top": 10, "kind": "small"},
        ],
        "boulders": [(11, 2, 1231), (2, 10, 1231)],
        "dense": [(8, 0, 2, 3), (0, 10, 2, 2)],
        "flowers": [(12, 1), (7, 3), (11, 10), (3, 7)],
        "run_exit": 0,
    },
    {
        "key": "waterfall_archipelago_03",
        "label": "双桥分台 03",
        "seed_offset": 1970,
        "entry": (9.5, -0.5),
        "entry_edge": "top",
        "exits": [(-0.5, 8.5), (15.5, 8.5)],
        "routes": [
            [(9.5, -0.5), (9.5, 8.5), (-0.5, 8.5)],
            [(9.5, -0.5), (9.5, 8.5), (15.5, 8.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "water_base": True,
        "land_rows": [
            (0, 8, 13), (1, 8, 13), (2, 8, 13),
            (3, 7, 13), (4, 7, 13), (5, 7, 13),
            (6, 7, 13), (7, 7, 13), (8, 7, 13),
            (9, 7, 13), (10, 7, 13), (11, 8, 12),
            (7, 0, 4), (8, 0, 4), (9, 0, 4),
            (10, 0, 4), (11, 0, 4),
            (7, 14, 16), (8, 14, 16), (9, 14, 16),
            (10, 14, 16), (11, 14, 16),
        ],
        "waterfalls": [
            {"left": 4, "width": 3, "crest_top": 3, "body_height": 2},
        ],
        "waterfall_walls": [
            {
                "top": 3,
                "bottom": 7,
                "left": 3,
                "right": 14,
                "gaps": [(4, 3)],
                "stair_left": 9,
                "terraced": True,
            },
        ],
        "horizontal_bridges": [
            {"left": 3, "top": 8, "length": 6},
            {"left": 11, "top": 8, "length": 5},
        ],
        "water_rocks": [
            {"left": 1, "top": 3, "kind": "small"},
            {"left": 14, "top": 2, "kind": "small"},
            {"left": 5, "top": 10, "kind": "small"},
        ],
        "boulders": [(11, 1, 1231), (2, 10, 1231), (15, 10, 1231)],
        "dense": [(11, 0, 2, 3), (0, 10, 2, 2)],
        "flowers": [(8, 1), (12, 7), (1, 7), (15, 7)],
        "run_exit": 1,
    },
    {
        "key": "waterfall_archipelago_04",
        "label": "群瀑终台 04",
        "seed_offset": 2080,
        "entry": (-0.5, 8.5),
        "entry_edge": "left",
        "exits": [(11.5, -0.5), (11.5, 11.5)],
        "routes": [
            [(-0.5, 8.5), (11.5, 8.5), (11.5, -0.5)],
            [(-0.5, 8.5), (11.5, 8.5), (11.5, 11.5)],
        ],
        "autotile_names": WATERFALL_AUTOTILE_NAMES,
        "water_base": True,
        "land_rows": [
            (0, 0, 4), (1, 0, 4), (2, 0, 4),
            (7, 0, 5), (8, 0, 5), (9, 0, 5),
            (10, 0, 5), (11, 0, 5),
            (0, 10, 16), (1, 10, 16), (2, 10, 16), (3, 10, 16),
            (4, 10, 16), (5, 10, 16), (6, 10, 16), (7, 10, 16),
            (8, 10, 16), (9, 10, 16), (10, 10, 16), (11, 10, 16),
        ],
        "waterfalls": [
            {"left": 7, "width": 3, "crest_top": 4, "body_height": 2},
        ],
        "waterfall_walls": [
            {
                "top": 4,
                "bottom": 8,
                "left": 6,
                "right": 16,
                "gaps": [(7, 3)],
                "stair_left": 11,
                "terraced": True,
            },
        ],
        "horizontal_bridges": [
            {"left": 3, "top": 8, "length": 8},
        ],
        "water_rocks": [
            {"left": 4, "top": 2, "kind": "large"},
            {"left": 5, "top": 10, "kind": "small"},
        ],
        "boulders": [(14, 2, 1231), (2, 10, 1231)],
        "dense": [(0, 0, 2, 3), (14, 0, 2, 3), (14, 9, 2, 3)],
        "flowers": [(3, 1), (10, 2), (15, 7), (10, 10)],
        "run_exit": 0,
    },
)

WATERFALL_ARCHIPELAGO_MAPS = (
    {
        "key": "waterfall_archipelago_01",
        "label": "左瀑三岛 01",
        "seed_offset": 2190,
        "entry": (7.5, 11.5),
        "entry_edge": "bottom",
        "exits": [(15.5, 8.5), (-0.5, 8.5)],
        "routes": [
            [(7.5, 11.5), (7.5, 8.5), (15.5, 8.5)],
            [(7.5, 11.5), (7.5, 8.5), (-0.5, 8.5)],
        ],
        "tileset_name": "Caves",
        "tileset_reference_map_id": 49,
        "autotile_names": CAVE_WATERFALL_AUTOTILE_NAMES,
        "tile_source_overrides": CAVE_ARCHIPELAGO_TILE_SOURCES,
        "sea_base": True,
        "hide_land_route": True,
        "cave_islands": [
            {
                "left": 0, "top": 7, "width": 4, "height": 4,
                "extend_left_to_edge": True,
            },
            {
                "left": 6, "top": 7, "width": 5, "height": 5,
                "extend_bottom_to_edge": True,
            },
            {
                "left": 12, "top": 7, "width": 4, "height": 4,
                "extend_right_to_edge": True,
            },
        ],
        "cave_ladders": [
            {"left": 7, "top": 10, "kind": "up", "endpoint": "entry"},
        ],
        "top_waterfalls": [
            {
                "left": 0, "width": 8, "body_height": 3,
                "left_wall": False, "right_wall": False,
                "right_cliff_to_edge": True,
            },
        ],
        "waterfall_water_clearance": 2,
        "horizontal_bridges": [
            {"left": 2, "top": 8, "length": 5, "tile_rows": CAVE_BRIDGE_TILES},
            {"left": 9, "top": 8, "length": 5, "tile_rows": CAVE_BRIDGE_TILES},
        ],
        "run_exit": 0,
    },
    {
        "key": "waterfall_archipelago_02",
        "label": "右瀑横桥 02",
        "seed_offset": 2300,
        "entry": (-0.5, 8.5),
        "entry_edge": "left",
        "exits": [(15.5, 8.5), (9.5, 11.5)],
        "routes": [
            [(-0.5, 8.5), (9.5, 8.5), (15.5, 8.5)],
            [(-0.5, 8.5), (9.5, 8.5), (9.5, 11.5)],
        ],
        "tileset_name": "Caves",
        "tileset_reference_map_id": 49,
        "autotile_names": CAVE_WATERFALL_AUTOTILE_NAMES,
        "tile_source_overrides": CAVE_ARCHIPELAGO_TILE_SOURCES,
        "sea_base": True,
        "hide_land_route": True,
        "cave_islands": [
            {
                "left": 0, "top": 7, "width": 4, "height": 4,
                "extend_left_to_edge": True,
            },
            {
                "left": 6, "top": 7, "width": 6, "height": 5,
                "extend_bottom_to_edge": True,
            },
            {
                "left": 13, "top": 7, "width": 3, "height": 4,
                "extend_right_to_edge": True,
            },
        ],
        "top_waterfalls": [
            {
                "left": 8, "width": 8, "body_height": 4,
                "left_wall": False, "right_wall": False,
                "left_cliff_to_edge": True,
            },
        ],
        "waterfall_water_clearance": 2,
        "horizontal_bridges": [
            {"left": 2, "top": 8, "length": 6, "tile_rows": CAVE_BRIDGE_TILES},
            {"left": 10, "top": 8, "length": 5, "tile_rows": CAVE_BRIDGE_TILES},
        ],
        "run_exit": 0,
    },
    {
        "key": "waterfall_archipelago_03",
        "label": "横贯大瀑 03",
        "seed_offset": 2410,
        "entry": (-0.5, 9.5),
        "entry_edge": "left",
        "exits": [(15.5, 8.5), (4.5, 11.5)],
        "routes": [
            [(-0.5, 9.5), (4.5, 9.5), (4.5, 8.5), (15.5, 8.5)],
            [(-0.5, 9.5), (4.5, 9.5), (4.5, 11.5)],
        ],
        "tileset_name": "Caves",
        "tileset_reference_map_id": 49,
        "autotile_names": CAVE_WATERFALL_AUTOTILE_NAMES,
        "tile_source_overrides": CAVE_ARCHIPELAGO_TILE_SOURCES,
        "sea_base": True,
        "hide_land_route": True,
        "cave_islands": [
            {
                "left": 0, "top": 7, "width": 7, "height": 5,
                "extend_left_to_edge": True,
                "extend_bottom_to_edge": True,
            },
            {"left": 8, "top": 7, "width": 4, "height": 4},
            {
                "left": 13, "top": 7, "width": 3, "height": 4,
                "extend_right_to_edge": True,
            },
        ],
        "top_waterfalls": [
            {
                "left": 0, "width": 16, "body_height": 2,
                "left_wall": False, "right_wall": False,
            },
        ],
        "waterfall_water_clearance": 2,
        "horizontal_bridges": [
            {"left": 5, "top": 8, "length": 5, "tile_rows": CAVE_BRIDGE_TILES},
            {"left": 10, "top": 8, "length": 5, "tile_rows": CAVE_BRIDGE_TILES},
        ],
        "run_exit": 0,
    },
    {
        "key": "waterfall_archipelago_04",
        "label": "深瀑终岛 04",
        "seed_offset": 2520,
        "entry": (-0.5, 8.5),
        "entry_edge": "left",
        "exits": [(11.5, 11.5), (15.5, 8.5)],
        "routes": [
            [(-0.5, 8.5), (11.5, 8.5), (11.5, 11.5)],
            [(-0.5, 8.5), (11.5, 8.5), (15.5, 8.5)],
        ],
        "tileset_name": "Caves",
        "tileset_reference_map_id": 49,
        "autotile_names": CAVE_WATERFALL_AUTOTILE_NAMES,
        "tile_source_overrides": CAVE_ARCHIPELAGO_TILE_SOURCES,
        "sea_base": True,
        "hide_land_route": True,
        "cave_islands": [
            {
                "left": 0, "top": 7, "width": 6, "height": 4,
                "extend_left_to_edge": True,
            },
            {
                "left": 10, "top": 7, "width": 6, "height": 5,
                "extend_right_to_edge": True,
                "extend_bottom_to_edge": True,
            },
        ],
        "cave_down_ladder_modules": [
            {"left": 11, "top": 9, "endpoint": "exit:0"},
        ],
        "top_waterfalls": [
            {
                "left": 0, "width": 10, "body_height": 4,
                "left_wall": False, "right_wall": False,
                "right_cliff_to_edge": True,
            },
        ],
        "waterfall_water_clearance": 2,
        "horizontal_bridges": [
            {"left": 4, "top": 8, "length": 7, "tile_rows": CAVE_BRIDGE_TILES},
        ],
        "run_exit": 0,
    },
)

SCENE_SPECS = {
    "grass_path": {
        "label": "草丛小路",
        "maps": GRASS_PATH_MAPS,
    },
    "creek_bridge_slope": {
        "label": "溪桥坡地",
        "maps": CREEK_BRIDGE_SLOPE_MAPS,
    },
    "ancient_waterfall_valley": {
        "label": "古木瀑谷",
        "maps": ANCIENT_WATERFALL_VALLEY_MAPS,
        "tileset_reference_map_id": 69,
        "animation_frames": 8,
        "animation_frame_duration_ms": 140,
    },
    "waterfall_plateau": {
        "label": "瀑岛高台",
        "maps": WATERFALL_PLATEAU_MAPS,
        "tileset_reference_map_id": 69,
        "animation_frames": 8,
        "animation_frame_duration_ms": 140,
    },
    "waterfall_archipelago": {
        "label": "瀑海群岛",
        "maps": WATERFALL_ARCHIPELAGO_MAPS,
        "tileset_reference_map_id": 49,
        "animation_frames": 8,
        "animation_frame_duration_ms": 140,
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


def build_tileset_data(layers, scene_name, reference_map_id=5):
    # The reference map supplies authoritative Outside autotile slots together
    # with passage, priority, and terrain-tag tables for generated tile IDs.
    data = export_map(reference_map_id)
    data.update({
        "mapId": 0,
        "name": scene_name,
        "width": MAP_W,
        "height": MAP_H,
        "layers": layers,
        "events": [],
    })
    present_tile_ids = {tile_id for layer in layers for tile_id in layer}
    max_custom_id = max(
        max(CUSTOM_TILE_VERTICAL_FLIPS),
        max(CAVE_ARCHIPELAGO_TILE_SOURCES),
        max(present_tile_ids, default=0),
    )
    present_custom_ids = present_tile_ids & set(CAVE_ARCHIPELAGO_TILE_METADATA_SOURCES)
    reference_map_ids = {"Outside": 5, "Caves": 49}
    metadata_tilesets = {
        CAVE_ARCHIPELAGO_TILE_METADATA_SOURCES[custom_id][0]
        for custom_id in present_custom_ids
    }
    metadata_sources = {
        tileset_name: export_map(reference_map_ids[tileset_name])
        for tileset_name in metadata_tilesets
    }
    for table_name in ("passages", "priorities", "terrainTags"):
        table = list(data[table_name])
        table.extend([0] * (max_custom_id + 1 - len(table)))
        for custom_id, source_id in CUSTOM_TILE_VERTICAL_FLIPS.items():
            table[custom_id] = table[source_id]
        for custom_id in present_custom_ids:
            tileset_name, source_id = CAVE_ARCHIPELAGO_TILE_METADATA_SOURCES[custom_id]
            table[custom_id] = metadata_sources[tileset_name][table_name][source_id]
        data[table_name] = table
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


def make_overview(images, map_specs, title, output_path=None, show_map_labels=True):
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
    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(output_path)
    return canvas


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
    animation_frame_sets = []
    outputs = []
    animation_frame_count = scene.get("animation_frames", 1)
    animation_frame_duration = scene.get("animation_frame_duration_ms", 140)

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
        animation_path = None
        animation_frames = [clean]
        if animation_frame_count > 1:
            animation_frames = render_clean_frames(
                generation["layers"],
                map_spec.get("autotile_names"),
                animation_frame_count,
                tileset_name=map_spec.get("tileset_name", "Outside"),
                tile_source_overrides=map_spec.get("tile_source_overrides"),
            )
            animation_path = OUTPUT_DIR / f"explore_{map_spec['key']}_416.gif"
            animation_frames[0].save(
                animation_path,
                save_all=True,
                append_images=animation_frames[1:],
                duration=animation_frame_duration,
                loop=0,
                disposal=2,
            )

        semantic_spec = integer_route_spec(
            map_spec,
            seed,
            generation["road_cells"],
            map_index,
            len(map_specs),
        )
        reference_map_id = map_spec.get(
            "tileset_reference_map_id",
            scene.get("tileset_reference_map_id", 5),
        )
        data = build_tileset_data(
            generation["layers"],
            map_spec["label"],
            reference_map_id,
        )
        semantic = build_semantic_map(data, semantic_spec)
        semantic["source"].update({
            "generated": True,
            "seed": seed,
            "variant": map_spec["key"],
            "tilesetReferenceMapId": reference_map_id,
        })
        semantic["generation"] = {
            "landCells": [list(point) for point in generation["land_cells"]],
            "roadCells": [list(point) for point in generation["road_cells"]],
            "waterCells": [list(point) for point in generation["water_cells"]],
            "bridgeCells": [list(point) for point in generation["bridge_cells"]],
            "cliffCells": [list(point) for point in generation["cliff_cells"]],
            "stairCells": [list(point) for point in generation["stair_cells"]],
            "caveLadderCells": [
                list(point) for point in generation["cave_ladder_cells"]
            ],
            "caveDownLadderModuleCells": [
                list(point)
                for point in generation["cave_down_ladder_module_cells"]
            ],
            "boulderCells": [list(point) for point in generation["boulder_cells"]],
            "waterRockCells": [list(point) for point in generation["water_rock_cells"]],
            "waterfallCells": [list(point) for point in generation["waterfall_cells"]],
            "waterfallCrestCells": [
                list(point) for point in generation["waterfall_crest_cells"]
            ],
            "waterfallBodyCells": [
                list(point) for point in generation["waterfall_body_cells"]
            ],
            "waterfallBottomCells": [
                list(point) for point in generation["waterfall_bottom_cells"]
            ],
            "upstreamWaterCells": [
                list(point) for point in generation["upstream_water_cells"]
            ],
            "downstreamWaterCells": [
                list(point) for point in generation["downstream_water_cells"]
            ],
            "islandFootprintCells": [
                list(point) for point in generation["island_footprint_cells"]
            ],
            "ancientTreeCells": [
                list(point) for point in generation["ancient_tree_cells"]
            ],
            "streamTransitionCells": [
                list(point) for point in generation["stream_transition_cells"]
            ],
            "solidScenery": [list(point) for point in generation["solid_scenery"]],
        }
        binary_path, json_path = write_semantic_map(semantic, map_spec["key"])
        debug = render_semantic_debug(semantic_spec, clean, semantic)
        debug_path = DEBUG_DIR / f"explore_{map_spec['key']}_semantics.png"
        debug.save(debug_path)

        if map_index == 0:
            clean.save(OUTPUT_DIR / f"explore_{scene_key}_416.png")
            if animation_path is not None:
                animation_frames[0].save(
                    OUTPUT_DIR / f"explore_{scene_key}_416.gif",
                    save_all=True,
                    append_images=animation_frames[1:],
                    duration=animation_frame_duration,
                    loop=0,
                    disposal=2,
                )
            routes.save(DEBUG_DIR / f"explore_{scene_key}_routes.png")
            debug.save(DEBUG_DIR / f"explore_{scene_key}_semantics.png")
            write_semantic_map(semantic, scene_key)

        clean_images.append(clean)
        route_images.append(routes)
        debug_images.append(debug)
        animation_frame_sets.append(animation_frames)
        outputs.append({
            "key": map_spec["key"],
            "map": map_path,
            "routes": route_path,
            "debug": debug_path,
            "binary": binary_path,
            "json": json_path,
            "animation": animation_path,
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
    animation_overview_path = None
    if animation_frame_count > 1:
        animation_overview_path = (
            OUTPUT_DIR / f"explore_{scene_key}_expedition_animated.gif"
        )
        animation_overview_frames = [
            make_overview(
                [frames[frame_index] for frames in animation_frame_sets],
                map_specs,
                f'{scene["label"]} / {count} MAP EXPEDITION / ANIMATED',
            )
            for frame_index in range(animation_frame_count)
        ]
        animation_overview_frames[0].save(
            animation_overview_path,
            save_all=True,
            append_images=animation_overview_frames[1:],
            duration=animation_frame_duration,
            loop=0,
            disposal=2,
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
                "animation": (
                    relative_output(item["animation"])
                    if item["animation"] is not None
                    else None
                ),
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
        "animationOverview": animation_overview_path,
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
    for kind in (
        "overview",
        "routeOverview",
        "debugOverview",
        "animationOverview",
        "manifest",
    ):
        if result[kind] is not None:
            print(f"{kind}={result[kind]}")


if __name__ == "__main__":
    main()
