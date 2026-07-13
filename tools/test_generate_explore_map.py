#!/usr/bin/env python3

import unittest

from generate_explore_map import (
    FLAG_FLOATABLE,
    FLAG_WATER,
    SMONMAP_CELL_STRIDE,
    SMONMAP_HEADER,
    SMONMAP_MAGIC,
    blocked_mask_for_cell,
    build_semantic_map,
    encode_smonmap,
)


GROUND_TILE = 384
WATER_TILE = 385
STRUCTURE_TILE = 386
NEUTRAL_TILE = 387
WATERFALL_TILE = 388


def map_data(width, height, layers):
    table_size = WATERFALL_TILE + 1
    passages = [0] * table_size
    priorities = [0] * table_size
    terrain_tags = [0] * table_size
    passages[WATER_TILE] = 0x0F
    passages[STRUCTURE_TILE] = 0x0F
    passages[NEUTRAL_TILE] = 0x0F
    passages[WATERFALL_TILE] = 0x0F
    terrain_tags[WATER_TILE] = 7
    terrain_tags[NEUTRAL_TILE] = 13
    terrain_tags[WATERFALL_TILE] = 8
    return {
        "mapId": 99,
        "name": "Semantic Test",
        "width": width,
        "height": height,
        "layers": layers,
        "passages": passages,
        "priorities": priorities,
        "terrainTags": terrain_tags,
        "events": [],
    }


class ExploreMapSemanticsTests(unittest.TestCase):
    def test_float_opens_water_without_making_it_land_walkable(self):
        data = map_data(1, 1, [[WATER_TILE], [0], [0]])

        self.assertEqual(blocked_mask_for_cell(data, 0, 0), 0x0F)
        self.assertEqual(blocked_mask_for_cell(data, 0, 0, allow_float=True), 0)

    def test_float_does_not_open_structure_above_water(self):
        data = map_data(1, 1, [[WATER_TILE], [STRUCTURE_TILE], [0]])

        self.assertEqual(blocked_mask_for_cell(data, 0, 0, allow_float=True), 0x0F)

    def test_neutral_overlay_defers_to_underlying_tile(self):
        data = map_data(1, 1, [[GROUND_TILE], [NEUTRAL_TILE], [0]])

        self.assertEqual(blocked_mask_for_cell(data, 0, 0), 0)

    def test_waterfall_requires_a_separate_future_ability(self):
        data = map_data(1, 1, [[WATERFALL_TILE], [0], [0]])

        self.assertEqual(blocked_mask_for_cell(data, 0, 0, allow_float=True), 0x0F)

    def test_semantic_map_preserves_water_and_float_reachability(self):
        data = map_data(2, 1, [[GROUND_TILE, WATER_TILE], [0, 0], [0, 0]])
        spec = {
            "crop": (0, 0),
            "entry": (0, 0),
            "routes": [{"exit": (1, 0), "route": [(1, 0)]}],
        }

        semantic = build_semantic_map(data, spec, tiles_w=2, tiles_h=1)
        water = semantic["cells"][1]

        self.assertEqual(semantic["stats"]["landReachable"], 1)
        self.assertEqual(semantic["stats"]["floatReachable"], 2)
        self.assertEqual(semantic["stats"]["floatOnlyReachable"], 1)
        self.assertTrue(water["flags"] & FLAG_WATER)
        self.assertTrue(water["flags"] & FLAG_FLOATABLE)
        self.assertFalse(water["landReachable"])
        self.assertTrue(water["floatReachable"])

        payload = encode_smonmap(semantic)
        header = SMONMAP_HEADER.unpack_from(payload)
        self.assertEqual(header[0], SMONMAP_MAGIC)
        self.assertEqual(header[-1], SMONMAP_CELL_STRIDE)
        self.assertEqual(len(payload), SMONMAP_HEADER.size + 2 + 2 * SMONMAP_CELL_STRIDE)


if __name__ == "__main__":
    unittest.main()
