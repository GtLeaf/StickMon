#!/usr/bin/env python3

import unittest

from map_generation_rules import (
    FENCE_LEFT,
    FENCE_TOP,
    FENCE_TOP_LEFT,
    FENCE_TOP_RIGHT,
    FOREST_BODY_IDS,
    FOREST_CROWN_IDS,
    DEEP_SEA_EDGE_TILE_ID,
    DEEP_SEA_TILE_ID,
    HIGH_GRASS_TILE_ID,
    LIGHTHOUSE_TILE_ROWS,
    SEA_SHORE_TILE_ID,
    build_two_tile_road,
    stamp_forest_fence,
    stamp_high_grass,
    stamp_left_coast,
    stamp_lighthouse,
    two_tile_road_cells,
)


class ForestFenceRuleTests(unittest.TestCase):
    def test_stamp_matches_map039_layer_contract(self):
        width = 16
        height = 12
        layers = [[0] * (width * height) for _ in range(3)]

        bounds = stamp_forest_fence(
            layers,
            width,
            height,
            forest_x=2,
            top_y=5,
            forest_width=14,
            bottom_y=11,
        )

        self.assertEqual(bounds["forest_bounds"], (2, 6, 15, 11))
        self.assertEqual(layers[1][5 * width + 1], FENCE_TOP_LEFT)
        self.assertTrue(all(layers[1][5 * width + x] == FENCE_TOP for x in range(2, 14)))
        self.assertEqual(layers[1][5 * width + 14], FENCE_TOP_RIGHT)
        self.assertTrue(all(layers[1][y * width + 1] == FENCE_LEFT for y in range(6, 12)))
        self.assertEqual(
            [layers[2][5 * width + x] for x in range(2, 16)],
            [FOREST_CROWN_IDS[index % 2] for index in range(14)],
        )
        self.assertEqual(
            [layers[0][y * width + 2] for y in range(6, 12)],
            [810, 802, 810, 802, 810, 818],
        )
        self.assertEqual(
            [layers[0][y * width + 3] for y in range(6, 12)],
            [811, 801, 809, 801, 809, 819],
        )

        body = [tile_id for tile_id in layers[0] if tile_id in FOREST_BODY_IDS]
        self.assertEqual(len(body), 14 * 6)

    def test_stamp_rejects_incomplete_forest(self):
        layers = [[0] * (16 * 12) for _ in range(3)]
        with self.assertRaises(ValueError):
            stamp_forest_fence(layers, 16, 12, 2, 8, 14, 11)
        with self.assertRaises(ValueError):
            stamp_forest_fence(layers, 16, 12, 2, 4, 14, 11)
        with self.assertRaises(ValueError):
            stamp_forest_fence(layers, 16, 12, 2, 5, 13, 11)


class TwoTileRoadRuleTests(unittest.TestCase):
    def test_road_cells_are_exactly_the_two_tile_route_envelope(self):
        routes = [
            [(1.5, -0.5), (1.5, 2.5), (-0.5, 2.5)],
            [(1.5, -0.5), (1.5, 2.5), (4.5, 2.5)],
        ]

        road = build_two_tile_road(routes, 5, 4)

        self.assertEqual(set(road), two_tile_road_cells(routes, 5, 4))
        self.assertEqual(len(road), 14)
        self.assertEqual({x for x, y in road if y == 1}, {1, 2})
        self.assertEqual({y for x, y in road if x == 4}, {2, 3})

    def test_road_rejects_diagonal_or_tile_center_segments(self):
        with self.assertRaises(ValueError):
            build_two_tile_road([[(0.5, 0.5), (2.5, 2.5)]], 5, 4)
        with self.assertRaises(ValueError):
            build_two_tile_road([[(1, -0.5), (1, 2.5)]], 5, 4)

    def test_multi_exit_network_contains_every_route_branch(self):
        routes = [
            [(1.5, -0.5), (1.5, 2.5), (-0.5, 2.5)],
            [(1.5, -0.5), (1.5, 2.5), (4.5, 2.5)],
        ]

        left_cells = two_tile_road_cells([routes[0]], 5, 4)
        right_cells = two_tile_road_cells([routes[1]], 5, 4)
        road = build_two_tile_road(routes, 5, 4)

        self.assertEqual(set(road), left_cells | right_cells)
        self.assertTrue(left_cells - right_cells)
        self.assertTrue(right_cells - left_cells)


class LandscapeTemplateTests(unittest.TestCase):
    def test_lighthouse_uses_complete_outside_tileset_template(self):
        layers = [[0] * (8 * 8) for _ in range(3)]

        result = stamp_lighthouse(layers, 8, 8, left=2, top=1)

        self.assertEqual(result["bounds"], (2, 1, 4, 6))
        self.assertEqual(len(result["positions"]), 18)
        for row, expected in enumerate(LIGHTHOUSE_TILE_ROWS, start=1):
            self.assertEqual(
                tuple(layers[1][row * 8 + x] for x in range(2, 5)),
                expected,
            )

    def test_high_grass_stays_on_layer_one_and_skips_routes(self):
        layers = [[0] * (6 * 5) for _ in range(3)]

        positions = stamp_high_grass(
            layers,
            6,
            5,
            left=1,
            top=1,
            width=4,
            height=3,
            blocked_cells={(2, 2), (3, 2)},
        )

        self.assertNotIn((2, 2), positions)
        self.assertNotIn((3, 2), positions)
        self.assertEqual(len(positions), 10)
        self.assertTrue(
            all(layers[1][y * 6 + x] == HIGH_GRASS_TILE_ID for x, y in positions)
        )
        self.assertTrue(all(tile_id == 0 for tile_id in layers[0]))

    def test_left_coast_matches_map039_deep_sea_transition(self):
        layers = [[0] * (8 * 5) for _ in range(3)]

        positions = stamp_left_coast(layers, 8, 5, coast_x=6)

        self.assertEqual(len(positions), 6 * 5)
        self.assertEqual(
            tuple(layers[0][2 * 8 + x] for x in range(6)),
            (
                DEEP_SEA_TILE_ID,
                DEEP_SEA_TILE_ID,
                DEEP_SEA_TILE_ID,
                DEEP_SEA_TILE_ID,
                DEEP_SEA_EDGE_TILE_ID,
                SEA_SHORE_TILE_ID,
            ),
        )


if __name__ == "__main__":
    unittest.main()
