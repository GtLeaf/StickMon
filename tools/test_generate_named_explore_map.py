#!/usr/bin/env python3

import unittest

from generate_explore_map import build_semantic_map
from generate_named_explore_map import (
    DEFAULT_SEED,
    SCENE_SPECS,
    build_tileset_data,
    edge_name,
    integer_route_spec,
)
from map_generation_rules import (
    DEEP_SEA_EDGE_TILE_ID,
    DEEP_SEA_TILE_ID,
    HIGH_GRASS_TILE_ID,
    SEA_SHORE_TILE_ID,
)
from generate_typical_map_run import POND_TILES, add_water_rect, build_map


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


if __name__ == "__main__":
    unittest.main()
