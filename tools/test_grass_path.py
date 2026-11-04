"""Check the finite grass recipe pool and its runtime selection contract."""
from pathlib import Path
import unittest

from grass_path import all_rooms, cpp_table, generate_map, selection
from grass_path_layouts import GRID, wants_fence
import generate_runtime_tile_map as g


class GrassPathTests(unittest.TestCase):
    def test_tables_are_current_and_every_recipe_is_passable(self):
        root = Path(__file__).resolve().parents[1]
        self.assertEqual((root/'src/game/ExploreGrassPathData.h').read_text(), cpp_table())
        self.assertEqual(len(all_rooms()), 96)
        for m, meta in all_rooms():
            road = g.build_road(m.paths)
            g.validate_topology(m.paths, m.junction)
            g.validate_border_road(m.entry, m.paths, road)
            g.validate_park_wide_road(m.paths, road)
            self.assertEqual(len(meta['grassPatches']), 2)
            self.assertEqual(len(g.connected_components(set(meta['flowerCells']))), 2)
            clear = g.expanded({m.entry.point, m.junction, *(p.exit.point for p in m.paths)})
            for tree in meta['trees']:
                for dy, row in enumerate(tree['tiles']):
                    for dx, tile in enumerate(row):
                        cell = (tree['x'] + dx, tree['y'] + dy)
                        self.assertIn(cell, GRID)
                        self.assertNotIn(cell, road | clear)
                        self.assertEqual(m.layers[1][cell[1]*16+cell[0]], tile)
            if meta['fencedWoodland']:
                left, top, width, bottom = meta['fenceSpec']
                self.assertEqual((width, bottom-top), (4, 4))
                self.assertFalse(g.forest_footprint(meta['fenceSpec']) & (road | clear))
                self.assertEqual(len(meta['trees']), 2)
            else:
                self.assertEqual(len(meta['trees']), (3, 4, 4)[meta['style']])

    def test_all_variants_reachable_and_fences_are_rare(self):
        choices = {selection(seed) for seed in range(10000)}
        self.assertEqual(choices, {(s, f, i) for s in range(3) for f in range(2) for i in range(4)})
        rate = sum(wants_fence(seed) for seed in range(10000)) / 10000
        self.assertGreater(rate, .14)
        self.assertLess(rate, .16)
        for edge in g.Edge:
            for seed in (0, 1, 38, 91, 0xffffffff):
                a, b = generate_map(seed, edge), generate_map(seed, edge)
                self.assertEqual(a, b)
                a.layers[0][0] = 0
                self.assertNotEqual(a.layers, b.layers)


if __name__ == '__main__':
    unittest.main()
