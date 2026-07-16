#!/usr/bin/env python3

import unittest

from cave_tile_semantics import (
    BROKEN_ICE_HOLE_TILE,
    CAVE_HOLE_TILE,
    CAVE_ENTRANCE_TILES,
    CLIFF_LEFT_TO_DOWN_CORNER_TILE,
    CLIFF_RIGHT_TO_DOWN_CORNER_TILE,
    CRACKED_ICE_TILE,
    DOWN_LADDER_BASE_TILES,
    DOWN_LADDER_CENTER_TILE,
    DOWN_LADDER_OPEN_TOP_FLIP_Y,
    DOWN_LADDER_OPEN_TOP_SOURCE_TILES,
    DOWN_LADDER_ROCK_EDGE_TOP_TILES,
    DOWN_LADDER_TILE,
    DOWNWARD_STAIRS_TILE,
    EDGE_TRACE_TILES,
    EDGE_TRACE_TURN_TILES,
    FROST_CAVE_EXIT_TILES,
    FROST_BLOCKED_SNOW_MASS_TILES,
    FROST_INNER_WALL_CORNER_TILES,
    ICE_CAVE_EDGE_TRACE_TILES,
    ICE_CAVE_EDGE_TRACE_TURN_TILES,
    ICE_ROCK_ISLAND_BACKGROUND_TILES,
    ICE_ROCK_ISLAND_TRANSPARENT_TILES,
    ROCK_STEP_TILE,
    ROUND_WATER_BOTTOM_TILES,
    SMALL_ISLAND_CENTER_TILE,
    SMALL_ISLAND_EDGE_TILES,
    SMALL_ISLAND_TILES,
    UP_LADDER_TILES,
)
from generate_explore_map import build_semantic_map
from generate_named_explore_map import (
    DEFAULT_SEED,
    SCENE_SPECS,
    build_tileset_data,
    edge_name,
    integer_route_spec,
)
from map_generation_rules import (
    CUSTOM_TILE_VERTICAL_FLIPS,
    DEEP_SEA_EDGE_TILE_ID,
    DEEP_SEA_TILE_ID,
    HIGH_GRASS_TILE_ID,
    SEA_SHORE_TILE_ID,
    STREAM_BOTTOM_INNER_LEFT_TILE,
    STREAM_BOTTOM_INNER_RIGHT_TILE,
    STREAM_BOTTOM_OUTER_LEFT_TILE,
    STREAM_BOTTOM_OUTER_RIGHT_TILE,
    STREAM_TOP_INNER_LEFT_TILE,
    STREAM_TOP_INNER_RIGHT_TILE,
    STREAM_TOP_OUTER_LEFT_TILE,
    STREAM_TOP_OUTER_RIGHT_TILE,
)
from generate_typical_map_run import (
    BRIDGE_STREAM_STRAIGHT_MARGIN,
    CAVE_CLIFF_FACE_TILES,
    CAVE_DOWN_LADDER_RENDER_TILES,
    CAVE_DOWN_LADDER_TOP_TILES,
    CAVE_ISLAND_FLOOR_TILE,
    CAVE_ISLAND_FLOOR_METADATA_TILE,
    CAVE_ISLAND_RIM_UNDERLAY_TILE,
    MAP_H,
    MAP_W,
    POND_TILES,
    SEA_LEFT_SHORE_TILE,
    SEA_CORNER_UNDERLAY_TILE,
    SEA_RIGHT_SHORE_TILE,
    SEA_TOP_LEFT_CORNER_TILE,
    SEA_TOP_RIGHT_CORNER_TILE,
    SEA_TOP_SHORE_TILE,
    SEA_WATER_TILE,
    add_water_rect,
    build_map,
    render_clean_frames,
    stamp_vertical_stream,
    stamp_vertical_stream_transitions,
)


class NamedExploreMapTests(unittest.TestCase):
    def test_half_grid_exit_edges_match_map_boundaries(self):
        self.assertEqual(edge_name((-0.5, 4.5)), "LEFT")
        self.assertEqual(edge_name((15.5, 3.5)), "RIGHT")
        self.assertEqual(edge_name((8.5, -0.5)), "TOP")
        self.assertEqual(edge_name((11.5, 11.5)), "BOTTOM")

    def test_grass_path_expedition_routes_and_water_semantics(self):
        scene = SCENE_SPECS["grass_path"]
        expected_water = (60, 0, 60, 0)
        self.assertEqual(len(scene["maps"]), 4)
        for map_index, (map_spec, water_count) in enumerate(
            zip(scene["maps"], expected_water)
        ):
            clean, _routes, generation = build_map(
                map_spec,
                DEFAULT_SEED,
                return_generation_data=True,
            )
            data = build_tileset_data(generation["layers"], map_spec["label"])
            semantic = build_semantic_map(
                data,
                integer_route_spec(
                    map_spec,
                    DEFAULT_SEED,
                    generation["road_cells"],
                    map_index,
                    len(scene["maps"]),
                ),
            )

            self.assertEqual(clean.size, (416, 312))
            self.assertEqual(len(semantic["exits"]), 2)
            self.assertEqual(semantic["stats"]["water"], water_count)
            self.assertEqual(semantic["stats"]["floatOnlyReachable"], water_count)
            self.assertEqual(semantic["stats"]["invalidRouteEdges"], 0)
            self.assertTrue(
                set(generation["road_cells"]).isdisjoint(generation["water_cells"])
            )
            self.assertIn(390, generation["layers"][0])
            self.assertNotIn(HIGH_GRASS_TILE_ID, generation["layers"][1])
            if water_count:
                water_ids = {
                    generation["layers"][0][y * 16 + x]
                    for x, y in generation["water_cells"]
                }
                self.assertEqual(
                    water_ids,
                    {DEEP_SEA_TILE_ID, DEEP_SEA_EDGE_TILE_ID, SEA_SHORE_TILE_ID},
                )

    def test_pond_helper_uses_authoritative_still_water_tiles(self):
        layers = [[0] * (16 * 12) for _ in range(3)]
        positions = add_water_rect(layers, 2, 3)
        data = build_tileset_data(layers, "pond metadata test")
        tile_ids = {tile_id for row in POND_TILES for tile_id in row}

        self.assertEqual(len(positions), 9)
        self.assertEqual(
            {layers[0][y * 16 + x] for x, y in positions},
            tile_ids,
        )
        self.assertTrue(all(data["passages"][tile_id] == 0x0F for tile_id in tile_ids))
        self.assertTrue(all(data["terrainTags"][tile_id] == 6 for tile_id in tile_ids))

    def test_creek_bridge_slope_routes_use_bridges_and_stairs(self):
        scene = SCENE_SPECS["creek_bridge_slope"]
        expected = (
            {
                "water": 28, "bridge": 12, "cliff": 0, "stairs": 0,
                "boulders": 2, "water_rocks": 2,
            },
            {
                "water": 0, "bridge": 0, "cliff": 32, "stairs": 4,
                "boulders": 1, "water_rocks": 0,
            },
            {
                "water": 37, "bridge": 21, "cliff": 16, "stairs": 4,
                "boulders": 2, "water_rocks": 5,
            },
            {
                "water": 40, "bridge": 14, "cliff": 0, "stairs": 0,
                "boulders": 2, "water_rocks": 2,
            },
        )
        self.assertEqual(len(scene["maps"]), 4)

        for map_index, (map_spec, counts) in enumerate(zip(scene["maps"], expected)):
            clean, _routes, generation = build_map(
                map_spec,
                DEFAULT_SEED,
                return_generation_data=True,
            )
            road = set(generation["road_cells"])
            water = set(generation["water_cells"])
            bridges = set(generation["bridge_cells"])
            cliffs = set(generation["cliff_cells"])
            stairs = set(generation["stair_cells"])
            boulders = set(generation["boulder_cells"])
            water_rocks = set(generation["water_rock_cells"])
            transitions = set(generation["stream_transition_cells"])
            solid = set(generation["solid_scenery"])

            self.assertEqual(clean.size, (416, 312))
            self.assertEqual(len(bridges), counts["bridge"])
            self.assertEqual(len(cliffs), counts["cliff"])
            self.assertEqual(len(stairs), counts["stairs"])
            self.assertEqual(len(boulders), counts["boulders"])
            self.assertEqual(len(water_rocks), counts["water_rocks"])
            self.assertTrue(road & water <= bridges)
            self.assertEqual(road & cliffs, stairs)
            self.assertTrue(water_rocks <= water)
            self.assertTrue(water_rocks.isdisjoint(road | bridges))
            self.assertTrue(
                all(
                    max(abs(rock[0] - corner[0]), abs(rock[1] - corner[1])) > 1
                    for rock in water_rocks
                    for corner in transitions
                )
            )
            self.assertTrue(water_rocks <= solid)
            if bridges:
                self.assertTrue(bridges & road)
            self.assertTrue(stairs <= road)
            self.assertTrue(road.isdisjoint(solid))

            for cliff in map_spec.get("horizontal_cliffs", []):
                stair_left = cliff.get("stair_left")
                if stair_left is not None:
                    self.assertGreater(stair_left, cliff["left"])
                    self.assertLess(stair_left + 2, cliff["right"])

            data = build_tileset_data(generation["layers"], map_spec["label"])
            semantic = build_semantic_map(
                data,
                integer_route_spec(
                    map_spec,
                    DEFAULT_SEED,
                    generation["road_cells"],
                    map_index,
                    len(scene["maps"]),
                ),
            )
            self.assertEqual(semantic["stats"]["water"], counts["water"])
            self.assertEqual(semantic["stats"]["invalidRouteEdges"], 0)

            streams = map_spec.get("vertical_streams", [])
            for upstream in streams:
                for downstream in streams:
                    if downstream.get("top", 0) != upstream.get("bottom", 12):
                        continue
                    if downstream["left"] != upstream["left"]:
                        self.assertNotEqual(downstream["width"], upstream["width"])

            for bridge in map_spec.get("horizontal_bridges", []):
                check_top = max(0, bridge["top"] - BRIDGE_STREAM_STRAIGHT_MARGIN)
                check_bottom = min(
                    12,
                    bridge["top"]
                    + bridge.get("height", 2)
                    + BRIDGE_STREAM_STRAIGHT_MARGIN,
                )
                stream_rows = []
                for y in range(check_top, check_bottom):
                    stream_rows.append(
                        {
                            x
                            for stream in map_spec.get("vertical_streams", [])
                            if stream.get("top", 0) <= y < stream.get("bottom", 12)
                            for x in range(stream["left"], stream["left"] + stream["width"])
                        }
                    )
                self.assertTrue(all(row == stream_rows[0] for row in stream_rows))

        self.assertTrue(
            any(len(map_spec.get("vertical_streams", [])) == 1 for map_spec in scene["maps"])
        )
        self.assertTrue(
            any(
                len({stream["width"] for stream in map_spec.get("vertical_streams", [])})
                > 1
                for map_spec in scene["maps"]
            )
        )

        opposite = {"LEFT": "right", "RIGHT": "left", "TOP": "bottom", "BOTTOM": "top"}
        for map_spec, next_spec in zip(scene["maps"], scene["maps"][1:]):
            selected_edge = edge_name(map_spec["exits"][map_spec["run_exit"]])
            self.assertEqual(next_spec["entry_edge"], opposite[selected_edge])

    def test_ancient_waterfall_valley_uses_blocked_falls_and_ancient_trees(self):
        scene = SCENE_SPECS["ancient_waterfall_valley"]
        expected = (
            {"water": 55, "floatable": 35, "falls": 20, "trees": 18},
            {"water": 53, "floatable": 33, "falls": 20, "trees": 18},
            {"water": 55, "floatable": 35, "falls": 20, "trees": 24},
            {"water": 83, "floatable": 53, "falls": 30, "trees": 18},
        )

        self.assertEqual(scene["tileset_reference_map_id"], 69)
        self.assertEqual(len(scene["maps"]), 4)
        for map_index, (map_spec, counts) in enumerate(zip(scene["maps"], expected)):
            clean, _routes, generation = build_map(
                map_spec,
                DEFAULT_SEED,
                return_generation_data=True,
            )
            road = set(generation["road_cells"])
            water = set(generation["water_cells"])
            falls = set(generation["waterfall_cells"])
            crest = set(generation["waterfall_crest_cells"])
            body = set(generation["waterfall_body_cells"])
            bottom = set(generation["waterfall_bottom_cells"])
            cliffs = set(generation["cliff_cells"])
            stairs = set(generation["stair_cells"])
            trees = set(generation["ancient_tree_cells"])
            rocks = set(generation["water_rock_cells"])

            self.assertEqual(clean.size, (416, 312))
            self.assertEqual(len(falls), counts["falls"])
            self.assertEqual(len(trees), counts["trees"])
            self.assertTrue(all(y < 3 for _x, y in trees))
            self.assertEqual(falls, crest | body | bottom)
            self.assertTrue(falls <= water)
            self.assertTrue(falls.isdisjoint(road | cliffs | rocks))
            self.assertTrue(trees.isdisjoint(road | water | cliffs))
            self.assertTrue(trees <= set(generation["solid_scenery"]))
            self.assertEqual(road & cliffs, stairs)
            surface_ids = {
                (
                    generation["layers"][1][y * 16 + x]
                    or generation["layers"][0][y * 16 + x]
                )
                for x, y in water - falls
            }
            self.assertTrue(all(48 <= tile_id < 96 for tile_id in surface_ids))

            for channel in map_spec["sea_channels"]:
                left = channel["left"]
                right = left + channel["width"] - 1
                top = channel["top"]
                top_open_spans = channel.get("top_open_spans", ())
                def water_tile(x, y):
                    index = y * 16 + x
                    return (
                        generation["layers"][1][index]
                        or generation["layers"][0][index]
                    )

                if top_open_spans:
                    self.assertEqual(
                        water_tile(left, top),
                        SEA_TOP_LEFT_CORNER_TILE,
                    )
                    self.assertEqual(
                        water_tile(right, top),
                        SEA_TOP_RIGHT_CORNER_TILE,
                    )
                    self.assertEqual(
                        generation["layers"][0][top * 16 + left],
                        SEA_CORNER_UNDERLAY_TILE,
                    )
                    self.assertEqual(
                        generation["layers"][0][top * 16 + right],
                        SEA_CORNER_UNDERLAY_TILE,
                    )
                    open_cells = {
                        x
                        for span_left, span_width in top_open_spans
                        for x in range(span_left, span_left + span_width)
                    }
                    for x in range(left + 1, right):
                        expected_tile = (
                            SEA_WATER_TILE
                            if x in open_cells
                            else SEA_TOP_SHORE_TILE
                        )
                        self.assertEqual(
                            water_tile(x, top),
                            expected_tile,
                        )
                else:
                    self.assertEqual(
                        water_tile(left, top),
                        SEA_LEFT_SHORE_TILE,
                    )
                    self.assertEqual(
                        water_tile(right, top),
                        SEA_RIGHT_SHORE_TILE,
                    )

            if map_index == 0:
                animation = render_clean_frames(
                    generation["layers"],
                    map_spec["autotile_names"],
                    frame_count=2,
                )
                self.assertNotEqual(animation[0].tobytes(), animation[1].tobytes())

            data = build_tileset_data(
                generation["layers"],
                map_spec["label"],
                scene["tileset_reference_map_id"],
            )
            self.assertEqual(
                data["autotileNames"][4:7],
                ["Waterfall crest", "Waterfall", "Waterfall bottom"],
            )
            self.assertTrue(
                all(data["terrainTags"][generation["layers"][0][y * 16 + x]] == 9
                    for x, y in crest)
            )
            self.assertTrue(
                all(data["terrainTags"][generation["layers"][0][y * 16 + x]] == 8
                    for x, y in body | bottom)
            )
            semantic = build_semantic_map(
                data,
                integer_route_spec(
                    map_spec,
                    DEFAULT_SEED,
                    generation["road_cells"],
                    map_index,
                    len(scene["maps"]),
                ),
            )
            self.assertEqual(semantic["stats"]["water"], counts["water"])
            self.assertEqual(
                semantic["stats"]["floatableWater"],
                counts["floatable"],
            )
            self.assertEqual(semantic["stats"]["invalidRouteEdges"], 0)

        opposite = {"LEFT": "right", "RIGHT": "left", "TOP": "bottom", "BOTTOM": "top"}
        for map_spec, next_spec in zip(scene["maps"], scene["maps"][1:]):
            selected_edge = edge_name(map_spec["exits"][map_spec["run_exit"]])
            self.assertEqual(next_spec["entry_edge"], opposite[selected_edge])

    def test_waterfall_plateau_uses_water_dominant_bridged_routes(self):
        scene = SCENE_SPECS["waterfall_plateau"]
        expected = (
            {"water": 68, "semantic_water": 52, "falls": 25, "bridges": 16, "stairs": 10, "corners": 2},
            {"water": 112, "semantic_water": 94, "falls": 32, "bridges": 24, "stairs": 8, "corners": 4},
            {"water": 108, "semantic_water": 82, "falls": 40, "bridges": 28, "stairs": 10, "corners": 4},
            {"water": 76, "semantic_water": 56, "falls": 25, "bridges": 20, "stairs": 10, "corners": 2},
        )

        self.assertEqual(scene["tileset_reference_map_id"], 69)
        self.assertEqual(scene["animation_frames"], 8)
        self.assertEqual(len(scene["maps"]), 4)
        for map_index, (map_spec, counts) in enumerate(zip(scene["maps"], expected)):
            _clean, _routes, generation = build_map(
                map_spec,
                DEFAULT_SEED,
                return_generation_data=True,
            )
            road = set(generation["road_cells"])
            water = set(generation["water_cells"])
            falls = set(generation["waterfall_cells"])
            bridges = set(generation["bridge_cells"])
            cliffs = set(generation["cliff_cells"])
            stairs = set(generation["stair_cells"])

            self.assertEqual(len(water), counts["water"])
            self.assertEqual(len(falls), counts["falls"])
            self.assertEqual(len(bridges), counts["bridges"])
            self.assertEqual(len(stairs), counts["stairs"])
            self.assertTrue(road & water <= bridges)
            self.assertEqual(road & cliffs, stairs)
            self.assertTrue(falls.isdisjoint(road | cliffs))
            self.assertTrue(bridges & road & water)

            corner_indexes = {
                index
                for index, tile_id in enumerate(generation["layers"][1])
                if tile_id in {
                    SEA_TOP_LEFT_CORNER_TILE,
                    SEA_TOP_RIGHT_CORNER_TILE,
                }
            }
            self.assertEqual(len(corner_indexes), counts["corners"])
            self.assertTrue(all(
                generation["layers"][0][index] == SEA_CORNER_UNDERLAY_TILE
                for index in corner_indexes
            ))

            data = build_tileset_data(
                generation["layers"],
                map_spec["label"],
                scene["tileset_reference_map_id"],
            )
            semantic = build_semantic_map(
                data,
                integer_route_spec(
                    map_spec,
                    DEFAULT_SEED,
                    generation["road_cells"],
                    map_index,
                    len(scene["maps"]),
                ),
            )
            self.assertEqual(semantic["stats"]["water"], counts["semantic_water"])
            self.assertEqual(semantic["stats"]["invalidRouteEdges"], 0)

        opposite = {"LEFT": "right", "RIGHT": "left", "TOP": "bottom", "BOTTOM": "top"}
        for map_spec, next_spec in zip(scene["maps"], scene["maps"][1:]):
            selected_edge = edge_name(map_spec["exits"][map_spec["run_exit"]])
            self.assertEqual(next_spec["entry_edge"], opposite[selected_edge])

    def test_waterfall_archipelago_uses_one_water_field_with_bridged_rock_islands(self):
        old_scene = SCENE_SPECS["waterfall_plateau"]
        scene = SCENE_SPECS["waterfall_archipelago"]
        expected = (
            {
                "water": 103, "falls": 32, "bridges": 20, "islands": 57,
                "side_cliff": 32, "height": 3, "mode": "left",
                "ladders": 2, "down_module": 0,
            },
            {
                "water": 94, "falls": 40, "bridges": 22, "islands": 58,
                "side_cliff": 40, "height": 4, "mode": "right",
                "ladders": 0, "down_module": 0,
            },
            {
                "water": 129, "falls": 48, "bridges": 20, "islands": 63,
                "side_cliff": 0, "height": 2, "mode": "full",
                "ladders": 0, "down_module": 0,
            },
            {
                "water": 108, "falls": 50, "bridges": 14, "islands": 54,
                "side_cliff": 30, "height": 4, "mode": "left",
                "ladders": 1, "down_module": 9,
            },
        )

        self.assertEqual(len(old_scene["maps"]), 4)
        self.assertEqual(len(scene["maps"]), 4)
        self.assertTrue(all(spec["key"].startswith("waterfall_plateau_")
                            for spec in old_scene["maps"]))
        self.assertEqual(scene["tileset_reference_map_id"], 49)
        self.assertTrue(all(spec["tileset_name"] == "Caves" for spec in scene["maps"]))
        self.assertTrue(all(spec["tileset_reference_map_id"] == 49
                            for spec in scene["maps"]))
        self.assertTrue(all(spec["sea_base"] for spec in scene["maps"]))
        self.assertTrue(all(spec["hide_land_route"] for spec in scene["maps"]))
        self.assertTrue(all(len(spec["cave_islands"]) >= 2 for spec in scene["maps"]))
        self.assertTrue(all(len(spec["top_waterfalls"]) == 1 for spec in scene["maps"]))

        for map_index, (map_spec, counts) in enumerate(
            zip(scene["maps"], expected)
        ):
            _clean, _routes, generation = build_map(
                map_spec,
                DEFAULT_SEED,
                return_generation_data=True,
            )
            land = set(generation["land_cells"])
            road = set(generation["road_cells"])
            water = set(generation["water_cells"])
            falls = set(generation["waterfall_cells"])
            bridges = set(generation["bridge_cells"])
            cliffs = set(generation["cliff_cells"])
            stairs = set(generation["stair_cells"])
            ladders = set(generation["cave_ladder_cells"])
            down_module = set(generation["cave_down_ladder_module_cells"])
            islands = set(generation["island_footprint_cells"])
            side_cliff = set(generation["top_waterfall_side_cliff_cells"])
            island_floor = islands & land
            island_rim = islands - land

            self.assertGreater(len(land), 20)
            self.assertEqual(len(water), counts["water"])
            self.assertEqual(len(falls), counts["falls"])
            self.assertEqual(len(bridges), counts["bridges"])
            self.assertEqual(len(islands), counts["islands"])
            self.assertEqual(len(side_cliff), counts["side_cliff"])
            self.assertEqual(len(ladders), counts["ladders"])
            self.assertEqual(len(down_module), counts["down_module"])
            self.assertFalse(stairs)
            self.assertTrue(land.isdisjoint(water))
            self.assertTrue(islands.isdisjoint(water | falls))
            self.assertTrue(road & water <= bridges)
            self.assertTrue(road & cliffs <= bridges)
            self.assertTrue(bridges & road & water)
            self.assertTrue(falls.isdisjoint(road | cliffs))
            self.assertTrue(side_cliff <= cliffs)
            self.assertTrue(side_cliff.isdisjoint(land | water | road | bridges))
            self.assertTrue(ladders <= land & road)
            self.assertTrue(ladders.isdisjoint(water | cliffs | bridges))
            self.assertTrue(down_module <= land)
            self.assertTrue(down_module.isdisjoint(water | cliffs | bridges))
            self.assertFalse(any(
                4604 <= tile_id <= 4607
                for layer in generation["layers"]
                for tile_id in layer
            ))
            self.assertTrue(island_floor)
            self.assertTrue(island_rim)
            self.assertTrue(all(
                generation["layers"][0][y * MAP_W + x] == CAVE_ISLAND_FLOOR_TILE
                for x, y in island_floor - down_module
            ))
            self.assertTrue(all(
                generation["layers"][0][y * MAP_W + x]
                == CAVE_ISLAND_RIM_UNDERLAY_TILE
                for x, y in island_rim
            ))
            self.assertFalse(any(
                tile_id in (SEA_TOP_LEFT_CORNER_TILE, SEA_TOP_RIGHT_CORNER_TILE)
                for layer in generation["layers"]
                for tile_id in layer
            ))
            if map_index == 0:
                self.assertEqual(ladders, {(7, 10), (7, 11)})
                self.assertEqual(
                    tuple(generation["layers"][1][y * MAP_W + 7] for y in (10, 11)),
                    UP_LADDER_TILES,
                )
                self.assertEqual(map_spec["cave_ladders"][0]["endpoint"], "entry")
                self.assertNotIn("cave_down_ladder_modules", map_spec)
            elif map_index == 3:
                expected_module = {
                    (x, y)
                    for y in range(9, 12)
                    for x in range(11, 14)
                }
                self.assertEqual(down_module, expected_module)
                self.assertEqual(ladders, {(12, 10)})
                for offset_y, tile_row in enumerate(CAVE_DOWN_LADDER_RENDER_TILES):
                    for offset_x, tile_id in enumerate(tile_row):
                        x = 11 + offset_x
                        y = 9 + offset_y
                        self.assertEqual(
                            generation["layers"][0][y * MAP_W + x],
                            tile_id,
                        )
                self.assertEqual(
                    generation["layers"][0][10 * MAP_W + 12],
                    DOWN_LADDER_CENTER_TILE,
                )
                self.assertEqual(
                    generation["layers"][1][10 * MAP_W + 12],
                    DOWN_LADDER_TILE,
                )
                self.assertEqual(
                    map_spec["cave_down_ladder_modules"][0]["endpoint"],
                    "exit:0",
                )
                for custom_id, source_id in zip(
                    CAVE_DOWN_LADDER_TOP_TILES,
                    DOWN_LADDER_BASE_TILES[0],
                ):
                    self.assertEqual(
                        map_spec["tile_source_overrides"][custom_id],
                        ("Caves", source_id),
                    )
                self.assertNotIn("cave_ladders", map_spec)
            else:
                self.assertNotIn("cave_ladders", map_spec)
                self.assertNotIn("cave_down_ladder_modules", map_spec)

            boundary_route = {
                (x, y)
                for x, y in road
                if x in (0, MAP_W - 1) or y in (0, MAP_H - 1)
            }
            self.assertTrue(boundary_route)
            self.assertTrue(boundary_route <= land)
            self.assertTrue(boundary_route.isdisjoint(cliffs | water))
            self.assertTrue(all(
                generation["layers"][0][y * MAP_W + x] == CAVE_ISLAND_FLOOR_TILE
                for x, y in boundary_route - down_module
            ))

            for island in map_spec["cave_islands"]:
                left = island["left"]
                top = island["top"]
                right = left + island["width"] - 1
                bottom = top + island["height"] - 1
                if island.get("extend_left_to_edge", False):
                    expected_land = {(left, y) for y in range(top, bottom + 1)}
                    if not island.get("extend_top_to_edge", False):
                        expected_land.discard((left, top))
                    if not island.get("extend_bottom_to_edge", False):
                        expected_land.discard((left, bottom))
                    self.assertTrue(expected_land <= land)
                if island.get("extend_right_to_edge", False):
                    expected_land = {(right, y) for y in range(top, bottom + 1)}
                    if not island.get("extend_top_to_edge", False):
                        expected_land.discard((right, top))
                    if not island.get("extend_bottom_to_edge", False):
                        expected_land.discard((right, bottom))
                    self.assertTrue(expected_land <= land)
                if island.get("extend_bottom_to_edge", False):
                    expected_land = {(x, bottom) for x in range(left, right + 1)}
                    if not island.get("extend_left_to_edge", False):
                        expected_land.discard((left, bottom))
                    if not island.get("extend_right_to_edge", False):
                        expected_land.discard((right, bottom))
                    self.assertTrue(expected_land <= land)

            self.assertFalse(generation["upstream_water_cells"])
            self.assertFalse(generation["downstream_water_cells"])
            self.assertFalse(generation["waterfall_crest_cells"])
            waterfall = map_spec["top_waterfalls"][0]
            self.assertEqual(waterfall["body_height"], counts["height"])
            self.assertFalse(waterfall["left_wall"])
            self.assertFalse(waterfall["right_wall"])
            self.assertEqual(
                sum(y == 0 for _x, y in generation["waterfall_body_cells"]),
                waterfall["width"],
            )
            left_edge = waterfall["left"]
            right_edge = left_edge + waterfall["width"]
            if counts["mode"] == "left":
                self.assertEqual(left_edge, 0)
                self.assertFalse(waterfall.get("left_cliff_to_edge", False))
                self.assertTrue(waterfall["right_cliff_to_edge"])
                expected_side_cliff = {
                    (x, y)
                    for y in range(waterfall["body_height"] + 1)
                    for x in range(right_edge, MAP_W)
                }
                visible_side = {(right_edge, y) for y in range(counts["height"] + 1)}
            elif counts["mode"] == "right":
                self.assertEqual(right_edge, MAP_W)
                self.assertTrue(waterfall["left_cliff_to_edge"])
                self.assertFalse(waterfall.get("right_cliff_to_edge", False))
                expected_side_cliff = {
                    (x, y)
                    for y in range(waterfall["body_height"] + 1)
                    for x in range(left_edge)
                }
                visible_side = {(left_edge - 1, y) for y in range(counts["height"] + 1)}
            else:
                self.assertEqual((left_edge, right_edge), (0, MAP_W))
                self.assertFalse(waterfall.get("left_cliff_to_edge", False))
                self.assertFalse(waterfall.get("right_cliff_to_edge", False))
                expected_side_cliff = set()
                visible_side = set()
            self.assertEqual(side_cliff, expected_side_cliff)
            self.assertTrue(visible_side <= cliffs)
            if side_cliff:
                self.assertTrue(all(
                    (x, waterfall["body_height"]) in cliffs
                    for x, _y in side_cliff
                ))
                self.assertTrue(any(x in (0, MAP_W - 1) for x, _y in side_cliff))
                side_left = min(x for x, _y in side_cliff)
                side_right = max(x for x, _y in side_cliff)
                for x, y in side_cliff:
                    expected_tile = CAVE_CLIFF_FACE_TILES[
                        0 if x == side_left else 2 if x == side_right else 1
                    ]
                    self.assertEqual(
                        generation["layers"][1][y * MAP_W + x],
                        expected_tile,
                    )
            bottom_y = waterfall["body_height"]
            for offset in range(1, map_spec["waterfall_water_clearance"] + 1):
                for x in range(waterfall["left"], waterfall["left"] + waterfall["width"]):
                    self.assertIn((x, bottom_y + offset), water)
                    self.assertNotIn((x, bottom_y + offset), islands)

            reachable_water = set()
            pending = [next(iter(water))]
            while pending:
                cell = pending.pop()
                if cell in reachable_water or cell not in water:
                    continue
                reachable_water.add(cell)
                x, y = cell
                pending.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
            self.assertEqual(reachable_water, water)

            data = build_tileset_data(
                generation["layers"],
                map_spec["label"],
                map_spec["tileset_reference_map_id"],
            )
            for table_name in ("passages", "priorities", "terrainTags"):
                self.assertEqual(
                    data[table_name][CAVE_ISLAND_FLOOR_TILE],
                    data[table_name][CAVE_ISLAND_FLOOR_METADATA_TILE],
                )
                if down_module:
                    for custom_id in CAVE_DOWN_LADDER_TOP_TILES:
                        self.assertEqual(
                            data[table_name][custom_id],
                            data[table_name][DOWN_LADDER_CENTER_TILE],
                        )
            semantic = build_semantic_map(
                data,
                integer_route_spec(
                    map_spec,
                    DEFAULT_SEED,
                    generation["road_cells"],
                    map_index,
                    len(scene["maps"]),
                ),
            )
            self.assertEqual(semantic["stats"]["invalidRouteEdges"], 0)

        opposite = {"LEFT": "right", "RIGHT": "left", "TOP": "bottom", "BOTTOM": "top"}
        for map_spec, next_spec in zip(scene["maps"], scene["maps"][1:]):
            selected_edge = edge_name(map_spec["exits"][map_spec["run_exit"]])
            self.assertEqual(next_spec["entry_edge"], opposite[selected_edge])

    def test_cave_tile_semantics_match_confirmed_catalog(self):
        self.assertEqual(
            SMALL_ISLAND_TILES,
            ((552, 553, 554), (560, 561, 562), (568, 569, 570)),
        )
        self.assertEqual(SMALL_ISLAND_CENTER_TILE, 561)
        self.assertEqual(
            SMALL_ISLAND_EDGE_TILES,
            frozenset({552, 553, 554, 560, 562, 568, 569, 570}),
        )
        self.assertEqual(CLIFF_RIGHT_TO_DOWN_CORNER_TILE, 571)
        self.assertEqual(CLIFF_LEFT_TO_DOWN_CORNER_TILE, 572)
        self.assertEqual(CAVE_ENTRANCE_TILES["left"], (515,))
        self.assertEqual(CAVE_ENTRANCE_TILES["right"], (517,))
        self.assertEqual(CAVE_ENTRANCE_TILES["front"], (524, 532))
        self.assertEqual(CAVE_ENTRANCE_TILES["back"], (526,))
        self.assertEqual(ROCK_STEP_TILE, 539)
        self.assertEqual(UP_LADDER_TILES, (540, 548))
        self.assertEqual(DOWN_LADDER_TILE, 541)
        self.assertEqual(
            DOWN_LADDER_BASE_TILES,
            ((601, 602, 603), (609, 610, 611), (617, 618, 619)),
        )
        self.assertEqual(DOWN_LADDER_CENTER_TILE, 610)
        self.assertEqual(DOWN_LADDER_ROCK_EDGE_TOP_TILES, (601, 602, 603))
        self.assertEqual(DOWN_LADDER_OPEN_TOP_SOURCE_TILES, (617, 618, 619))
        self.assertTrue(DOWN_LADDER_OPEN_TOP_FLIP_Y)
        self.assertEqual(
            EDGE_TRACE_TILES,
            (579, 580, 588, 596, 597, 598, 590, 582, 583),
        )
        self.assertEqual(
            EDGE_TRACE_TURN_TILES,
            {
                "left_to_down": 580,
                "down_to_right": 596,
                "right_to_up": 598,
                "up_to_right": 582,
            },
        )
        self.assertEqual(FROST_CAVE_EXIT_TILES, (1299, 1300, 1301))
        self.assertEqual(
            FROST_BLOCKED_SNOW_MASS_TILES,
            ((1296, 1297, 1298), (1304, 1305, 1306), (1312, 1313, 1314)),
        )
        self.assertEqual(
            FROST_INNER_WALL_CORNER_TILES,
            {
                "outside_nw": 1314,
                "outside_ne": 1312,
                "outside_sw": 1298,
                "outside_se": 1296,
            },
        )
        self.assertEqual(CRACKED_ICE_TILE, 1321)
        self.assertEqual(BROKEN_ICE_HOLE_TILE, 1322)
        self.assertEqual(ROUND_WATER_BOTTOM_TILES, (1326, 1327))
        self.assertEqual(DOWNWARD_STAIRS_TILE, 1333)
        self.assertEqual(CAVE_HOLE_TILE, 1341)
        self.assertEqual(
            ICE_CAVE_EDGE_TRACE_TILES,
            (1344, 1345, 1353, 1361, 1362, 1363, 1355, 1347, 1348),
        )
        self.assertEqual(
            ICE_CAVE_EDGE_TRACE_TURN_TILES,
            {
                "left_to_down": 1345,
                "down_to_right": 1361,
                "right_to_up": 1363,
                "up_to_right": 1347,
            },
        )
        self.assertEqual(
            ICE_ROCK_ISLAND_TRANSPARENT_TILES,
            ((1120, 1121, 1122), (1128, 1129, 1130), (1136, 1137, 1138)),
        )
        self.assertEqual(
            ICE_ROCK_ISLAND_BACKGROUND_TILES,
            ((1123, 1124, 1125), (1131, 1132, 1133), (1139, 1140, 1141)),
        )

    def test_cave_ladder_must_touch_its_declared_endpoint(self):
        bad_spec = dict(SCENE_SPECS["waterfall_archipelago"]["maps"][0])
        bad_spec["key"] = "ladder_at_wrong_endpoint"
        bad_spec["cave_ladders"] = [
            {"left": 7, "top": 10, "kind": "up", "endpoint": "exit:0"},
        ]

        with self.assertRaisesRegex(RuntimeError, "misses its declared endpoint"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_down_ladder_cannot_be_used_without_its_base_module(self):
        bad_spec = dict(SCENE_SPECS["waterfall_archipelago"]["maps"][3])
        bad_spec["key"] = "standalone_pending_down_ladder"
        bad_spec["cave_ladders"] = [
            {"left": 11, "top": 11, "kind": "down", "endpoint": "exit:0"},
        ]

        with self.assertRaisesRegex(ValueError, "unsupported cave ladder kind: down"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_truncated_forest_module_below_top_edge_is_rejected(self):
        bad_spec = dict(SCENE_SPECS["ancient_waterfall_valley"]["maps"][0])
        bad_spec["key"] = "interior_truncated_forest"
        bad_spec["ancient_tree_clusters"] = [
            {"left": 0, "top": 9, "width": 2},
        ]

        with self.assertRaisesRegex(RuntimeError, "only allowed at the top edge"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_stream_bend_inside_bridge_margin_is_rejected(self):
        bad_spec = dict(SCENE_SPECS["creek_bridge_slope"]["maps"][0])
        bad_spec["key"] = "bridge_bend_regression"
        bad_spec["vertical_streams"] = [
            {"left": 8, "width": 3, "top": 0, "bottom": 8},
            {"left": 9, "width": 3, "top": 8, "bottom": 12},
        ]

        with self.assertRaisesRegex(RuntimeError, "stream bends too close to bridge"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_constant_width_stream_shift_is_rejected(self):
        bad_spec = dict(SCENE_SPECS["creek_bridge_slope"]["maps"][3])
        bad_spec["key"] = "constant_width_stream_shift"
        bad_spec["vertical_streams"] = [
            {"left": 8, "width": 4, "top": 0, "bottom": 10},
            {"left": 7, "width": 4, "top": 10, "bottom": 12},
        ]

        with self.assertRaisesRegex(RuntimeError, "cannot shift sideways"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_stream_width_transitions_use_dirt_bank_corners(self):
        layers = [[385] * (16 * 12) for _ in range(3)]
        narrowing = [
            {"left": 2, "width": 6, "top": 0, "bottom": 6},
            {"left": 3, "width": 4, "top": 6, "bottom": 12},
        ]
        for stream in narrowing:
            stamp_vertical_stream(layers, **stream)
        stamp_vertical_stream_transitions(layers, narrowing)

        self.assertEqual(layers[0][5 * 16 + 2], STREAM_BOTTOM_OUTER_LEFT_TILE)
        self.assertEqual(layers[0][5 * 16 + 3], STREAM_BOTTOM_INNER_LEFT_TILE)
        self.assertEqual(layers[0][5 * 16 + 6], STREAM_BOTTOM_INNER_RIGHT_TILE)
        self.assertEqual(layers[0][5 * 16 + 7], STREAM_BOTTOM_OUTER_RIGHT_TILE)

        layers = [[385] * (16 * 12) for _ in range(3)]
        widening = [
            {"left": 3, "width": 4, "top": 0, "bottom": 6},
            {"left": 2, "width": 6, "top": 6, "bottom": 12},
        ]
        for stream in widening:
            stamp_vertical_stream(layers, **stream)
        stamp_vertical_stream_transitions(layers, widening)

        self.assertEqual(layers[0][6 * 16 + 2], STREAM_TOP_OUTER_LEFT_TILE)
        self.assertEqual(layers[0][6 * 16 + 3], STREAM_TOP_INNER_LEFT_TILE)
        self.assertEqual(layers[0][6 * 16 + 6], STREAM_TOP_INNER_RIGHT_TILE)
        self.assertEqual(layers[0][6 * 16 + 7], STREAM_TOP_OUTER_RIGHT_TILE)

    def test_custom_stream_corners_copy_water_metadata(self):
        data = build_tileset_data([[385] * (16 * 12) for _ in range(3)], "corners")
        for custom_id, source_id in CUSTOM_TILE_VERTICAL_FLIPS.items():
            self.assertEqual(data["passages"][custom_id], data["passages"][source_id])
            self.assertEqual(data["priorities"][custom_id], data["priorities"][source_id])
            self.assertEqual(data["terrainTags"][custom_id], data["terrainTags"][source_id])

    def test_water_rock_next_to_stream_transition_is_rejected(self):
        bad_spec = dict(SCENE_SPECS["creek_bridge_slope"]["maps"][3])
        bad_spec["key"] = "water_rock_on_stream_corner"
        bad_spec["water_rocks"] = [{"left": 8, "top": 10, "kind": "small"}]

        with self.assertRaisesRegex(RuntimeError, "too close to stream transition"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_cliff_stairs_without_level_right_shoulder_are_rejected(self):
        bad_spec = dict(SCENE_SPECS["creek_bridge_slope"]["maps"][2])
        bad_spec["key"] = "stairs_at_cliff_step"
        bad_spec["horizontal_cliffs"] = [
            {"top": 7, "left": 8, "right": 13, "stair_left": 11},
            {"top": 6, "left": 13, "right": 16},
        ]

        with self.assertRaisesRegex(RuntimeError, "level shoulders on both sides"):
            build_map(bad_spec, DEFAULT_SEED)

    def test_stair_cliff_cannot_touch_a_different_elevation(self):
        bad_spec = dict(SCENE_SPECS["creek_bridge_slope"]["maps"][2])
        bad_spec["key"] = "stair_cliff_height_step"
        bad_spec["horizontal_cliffs"] = [
            {"top": 7, "left": 8, "right": 14, "stair_left": 11},
            {"top": 6, "left": 14, "right": 16},
        ]

        with self.assertRaisesRegex(RuntimeError, "cannot adjoin a different elevation"):
            build_map(bad_spec, DEFAULT_SEED)


if __name__ == "__main__":
    unittest.main()
