#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from generate_runtime_tile_map import (
    DEEP_SEA_EDGE_TILE,
    DEEP_SEA_TILE,
    DENSE_GRASS_TILE,
    Edge,
    SEA_SHORE_TILE,
    derive_seed,
    fingerprint,
    generate_map,
)


ROOT = Path(__file__).resolve().parents[1]
ROAD_TILES = {537, 538, 539, 540, 542, 545, 546, 547, 553, 554, 555, 556, 558}


class RuntimeTileMapTests(unittest.TestCase):
    def test_generation_is_deterministic_and_uses_only_normal_grass(self):
        first = generate_map(0x20260713, Edge.TOP)
        second = generate_map(0x20260713, Edge.TOP)

        self.assertEqual(fingerprint(first), fingerprint(second))
        self.assertEqual(first.layers, second.layers)
        self.assertIn(DENSE_GRASS_TILE, first.layers[0])
        self.assertTrue(all(391 not in layer for layer in first.layers))

    def test_routes_remain_on_two_exit_road_networks(self):
        for seed in range(1, 65):
            for edge in Edge:
                runtime_map = generate_map(seed, edge)
                road = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[0])
                    if tile_id in ROAD_TILES
                }
                water = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[0])
                    if tile_id in {DEEP_SEA_TILE, DEEP_SEA_EDGE_TILE, SEA_SHORE_TILE}
                }

                self.assertEqual(len(runtime_map.paths), 2)
                self.assertEqual(len({path.exit.edge for path in runtime_map.paths}), 2)
                self.assertNotIn(runtime_map.entry.edge, {path.exit.edge for path in runtime_map.paths})
                for path in runtime_map.paths:
                    self.assertGreaterEqual(len(path.points), 2)
                    self.assertTrue(set(path.points).issubset(road))
                    self.assertTrue(set(path.points).isdisjoint(water))

    def test_left_coast_uses_map039_transition_and_reserves_left_edge(self):
        coast_count = 0
        for seed in range(1, 128):
            runtime_map = generate_map(seed, Edge.TOP)
            if not runtime_map.has_coast:
                continue
            coast_count += 1
            self.assertNotEqual(runtime_map.entry.edge, Edge.LEFT)
            self.assertTrue(all(path.exit.edge != Edge.LEFT for path in runtime_map.paths))
            for y in range(12):
                row = runtime_map.layers[0][y * 16:(y + 1) * 16]
                self.assertEqual(row[:5], [DEEP_SEA_TILE] * 3 + [DEEP_SEA_EDGE_TILE, SEA_SHORE_TILE])
        self.assertGreater(coast_count, 0)

    def test_seed_derivation_has_stable_snapshots(self):
        self.assertEqual(derive_seed(0x20260713, 0, 0), 0x99ADB62E)
        self.assertEqual(derive_seed(0x20260713, 1, 1), 0x73C0A14A)
        self.assertEqual(derive_seed(0x20260713, 2, 2), 0xAD5997E6)

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_python_matches_firmware_cpp_fingerprint(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "runtime_tile_map_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "runtime_tile_map_host.cpp"),
                    str(ROOT / "src" / "game" / "ExploreMapGenerator.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            for seed in (1, 0x20260713, 0xDEADBEEF, 0xFFFFFFFF):
                for edge in Edge:
                    expected = fingerprint(generate_map(seed, edge))
                    actual = int(
                        subprocess.check_output(
                            [str(binary), str(seed), str(int(edge))],
                            text=True,
                        ).strip(),
                        16,
                    )
                    self.assertEqual(actual, expected, f"seed={seed:#x} edge={edge.name}")


if __name__ == "__main__":
    unittest.main()
