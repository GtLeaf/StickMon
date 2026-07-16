#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from generate_runtime_tile_map import (
    ANCIENT_WATERFALL_VALLEY_AREA,
    BOULDER_TILE,
    BRIDGE_BOTTOM_TILE,
    BRIDGE_TOP_TILE,
    CLIFF_FACE_TILE,
    CLIFF_STAIR_LEFT_TILE,
    CLIFF_STAIR_RIGHT_TILE,
    CLIFF_TOP_TILE,
    CREEK_BRIDGE_SLOPE_AREA,
    DEEP_SEA_EDGE_TILE,
    DEEP_SEA_TILE,
    DENSE_GRASS_TILE,
    Edge,
    FLOWER_TILE,
    FROST_CRYSTAL_CAVE_AREA,
    FROST_CRYSTAL_TOP_TILE,
    FROST_FLOOR_TILE,
    FROST_ICE_TILES,
    FROST_INNER_CORNER_TILES,
    FROST_OUTSIDE_TILE,
    FROST_ROCK_HILL_TILES,
    FROST_WALL_TILES,
    MIST_FOREST_PATH_AREA,
    SEA_SHORE_TILE,
    SEA_CORNER_UNDERLAY_TILE,
    SEA_TOP_LEFT_CORNER_TILE,
    SEA_TOP_RIGHT_CORNER_TILE,
    SHRUB_TILE,
    STREAM_BOTTOM_INNER_LEFT_TILE,
    STREAM_BOTTOM_INNER_RIGHT_TILE,
    STREAM_BOTTOM_OUTER_LEFT_TILE,
    STREAM_BOTTOM_OUTER_RIGHT_TILE,
    STREAM_CENTER_TILE,
    STREAM_LEFT_TILE,
    STREAM_RIGHT_TILE,
    STREAM_TOP_INNER_LEFT_TILE,
    STREAM_TOP_INNER_RIGHT_TILE,
    STREAM_TOP_OUTER_LEFT_TILE,
    STREAM_TOP_OUTER_RIGHT_TILE,
    WATER_ROCK_LARGE_TILES,
    WATER_ROCK_SMALL_TILE,
    WATERFALL_BOTTOM_TILE,
    WATERFALL_CREST_TILES,
    connected_components,
    derive_seed,
    fingerprint,
    generate_map,
)


ROOT = Path(__file__).resolve().parents[1]
ROAD_TILES = {537, 538, 539, 540, 542, 545, 546, 547, 553, 554, 555, 556, 558}
STREAM_TILES = {
    STREAM_LEFT_TILE,
    STREAM_CENTER_TILE,
    STREAM_RIGHT_TILE,
    STREAM_TOP_OUTER_LEFT_TILE,
    STREAM_TOP_OUTER_RIGHT_TILE,
    STREAM_TOP_INNER_LEFT_TILE,
    STREAM_TOP_INNER_RIGHT_TILE,
    STREAM_BOTTOM_OUTER_LEFT_TILE,
    STREAM_BOTTOM_OUTER_RIGHT_TILE,
    STREAM_BOTTOM_INNER_LEFT_TILE,
    STREAM_BOTTOM_INNER_RIGHT_TILE,
}
STREAM_TRANSITION_TILES = {
    STREAM_TOP_OUTER_LEFT_TILE,
    STREAM_TOP_OUTER_RIGHT_TILE,
    STREAM_TOP_INNER_LEFT_TILE,
    STREAM_TOP_INNER_RIGHT_TILE,
    STREAM_BOTTOM_OUTER_LEFT_TILE,
    STREAM_BOTTOM_OUTER_RIGHT_TILE,
    STREAM_BOTTOM_INNER_LEFT_TILE,
    STREAM_BOTTOM_INNER_RIGHT_TILE,
}
WATER_ROCK_TILES = {
    WATER_ROCK_SMALL_TILE,
    *(tile_id for row in WATER_ROCK_LARGE_TILES for tile_id in row),
}


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
                allowed_boundary = set()
                for endpoint in (runtime_map.entry, *(path.exit for path in runtime_map.paths)):
                    x, y = endpoint.point
                    if endpoint.edge in (Edge.TOP, Edge.BOTTOM):
                        allowed_boundary.update(((x, y), (x + 1, y)))
                    else:
                        allowed_boundary.update(((x, y), (x, y + 1)))
                boundary_road = {
                    (x, y)
                    for x, y in road
                    if x in (0, 15) or y in (0, 11)
                }
                self.assertEqual(boundary_road, allowed_boundary)
                shared = []
                for first, second in zip(runtime_map.paths[0].points, runtime_map.paths[1].points):
                    if first != second:
                        break
                    shared.append(first)
                self.assertTrue(shared)
                self.assertEqual(shared[-1], runtime_map.junction)
                for path in runtime_map.paths:
                    self.assertGreaterEqual(len(path.points), 2)
                    self.assertEqual(len(path.points), len(set(path.points)))
                    self.assertTrue(set(path.points).issubset(road))
                    self.assertTrue(set(path.points).isdisjoint(water))

    def test_open_maps_use_one_grass_region_and_anchored_decorations(self):
        checked = 0
        for seed in range(1, 256):
            runtime_map = generate_map(seed, Edge.LEFT)
            if runtime_map.has_coast or runtime_map.has_forest:
                continue
            checked += 1
            dense_grass = {
                (index % 16, index // 16)
                for index, tile_id in enumerate(runtime_map.layers[0])
                if tile_id == DENSE_GRASS_TILE
            }
            flowers = {
                (index % 16, index // 16)
                for index, tile_id in enumerate(runtime_map.layers[0])
                if tile_id == FLOWER_TILE
            }
            shrubs = {
                (index % 16, index // 16)
                for index, tile_id in enumerate(runtime_map.layers[1])
                if tile_id == SHRUB_TILE
            }
            self.assertGreaterEqual(len(dense_grass), 24)
            self.assertEqual(len(connected_components(dense_grass)), 1)
            self.assertTrue(flowers or shrubs)

            for component in connected_components(flowers):
                self.assertIn(len(component), (3, 4, 5))
                self.assertTrue(any(
                    abs(x - gx) + abs(y - gy) == 1
                    for x, y in component
                    for gx, gy in dense_grass
                ))
            for component in connected_components(shrubs):
                self.assertGreaterEqual(len(component), 2)
                self.assertLessEqual(len(component), 4)
                self.assertTrue(
                    len({x for x, _y in component}) == 1
                    or len({y for _x, y in component}) == 1
                )
                self.assertTrue(all(
                    any(abs(x - gx) + abs(y - gy) == 1 for gx, gy in dense_grass)
                    for x, y in component
                ))
            road_indexes = {
                y * 16 + x
                for path in runtime_map.paths
                for x, y in path.points
            }
            self.assertTrue(all(
                runtime_map.layers[1][index] != SHRUB_TILE
                for index in road_indexes
            ))
            if checked == 16:
                break
        self.assertEqual(checked, 16)

    def test_active_area_profiles_keep_landscapes_visually_distinct(self):
        for seed in range(1, 33):
            grass = generate_map(seed, Edge.TOP)
            self.assertFalse(grass.has_coast)

            mist = generate_map(seed, Edge.TOP, MIST_FOREST_PATH_AREA)
            self.assertTrue(mist.has_forest)
            self.assertFalse(mist.has_coast)

            frost = generate_map(seed, Edge.TOP, FROST_CRYSTAL_CAVE_AREA)
            self.assertFalse(frost.has_forest)
            self.assertFalse(frost.has_coast)

    def test_frost_cave_uses_caves_tiles_and_keeps_routes_clear(self):
        ice_ids = set(FROST_ICE_TILES.values())
        ground_ids = {FROST_OUTSIDE_TILE, FROST_FLOOR_TILE, *ice_ids}
        wall_ids = set(FROST_WALL_TILES.values()) | set(
            FROST_INNER_CORNER_TILES.values()
        )
        rock_hill_ids = {
            tile_id
            for row in FROST_ROCK_HILL_TILES
            for tile_id in row
        }
        scenery_ids = {4508, 4509, 4510, 4542, 4543, 4544}
        observed_profiles = set()
        for edge in Edge:
            runtime_map = generate_map(0x20260713, edge, FROST_CRYSTAL_CAVE_AREA)
            self.assertEqual(runtime_map.entry.edge, edge)
            self.assertFalse(runtime_map.has_coast)
            self.assertFalse(runtime_map.has_forest)
            self.assertFalse(runtime_map.has_creek)
            self.assertFalse(runtime_map.has_waterfall)
            self.assertTrue(set(runtime_map.layers[0]).issubset(ground_ids))
            self.assertTrue(set(runtime_map.layers[1]).issubset({
                0, *wall_ids, *rock_hill_ids, *scenery_ids,
            }))
            self.assertEqual(runtime_map.layers[2].count(FROST_CRYSTAL_TOP_TILE), 2)
            self.assertTrue(set(runtime_map.layers[2]).issubset({
                0, FROST_CRYSTAL_TOP_TILE,
            }))
            for path in runtime_map.paths:
                self.assertEqual(path.points[0], runtime_map.entry.point)
                self.assertEqual(path.points[-1], path.exit.point)
                for x, y in path.points:
                    index = y * 16 + x
                    self.assertIn(runtime_map.layers[0][index], {
                        FROST_FLOOR_TILE, *ice_ids,
                    })
                    self.assertEqual(runtime_map.layers[1][index], 0)

        for seed in range(1, 65):
            for edge in Edge:
                runtime_map = generate_map(seed, edge, FROST_CRYSTAL_CAVE_AREA)
                has_ice = any(tile_id in ice_ids for tile_id in runtime_map.layers[0])
                hill_count = sum(
                    tile_id in rock_hill_ids for tile_id in runtime_map.layers[1]
                )
                self.assertIn(hill_count, (0, 9))
                self.assertFalse(has_ice and hill_count)
                observed_profiles.add((has_ice, hill_count == 9))
        self.assertEqual(
            observed_profiles,
            {(False, False), (False, True), (True, False)},
        )

    def test_creek_maps_keep_routes_on_bridges_and_transitions_away_from_bridges(self):
        transition_count = 0
        cliff_count = 0
        for seed in range(1, 129):
            for edge in Edge:
                runtime_map = generate_map(seed, edge, CREEK_BRIDGE_SLOPE_AREA)
                self.assertTrue(runtime_map.has_creek)
                self.assertFalse(runtime_map.has_coast)
                self.assertFalse(runtime_map.has_forest)

                stream_rows = []
                for y in range(12):
                    xs = {
                        x
                        for x in range(16)
                        if runtime_map.layers[0][y * 16 + x] in STREAM_TILES
                    }
                    self.assertGreaterEqual(len(xs), 3)
                    self.assertEqual(xs, set(range(min(xs), max(xs) + 1)))
                    stream_rows.append(xs)

                bridge_top = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[2])
                    if tile_id == BRIDGE_TOP_TILE
                }
                bridge_bottom = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[2])
                    if tile_id == BRIDGE_BOTTOM_TILE
                }
                self.assertTrue(bridge_top)
                self.assertEqual(len({y for _x, y in bridge_top}), 1)
                top = next(iter(bridge_top))[1]
                self.assertEqual({x for x, _y in bridge_top}, {x for x, _y in bridge_bottom})
                self.assertEqual({y for _x, y in bridge_bottom}, {top + 1})
                bridge_cells = bridge_top | bridge_bottom
                for y in range(max(0, top - 2), min(12, top + 4)):
                    self.assertEqual(stream_rows[y], stream_rows[top])

                transition_cells = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[0])
                    if tile_id in STREAM_TRANSITION_TILES
                }
                if transition_cells:
                    transition_count += 1
                for boundary in range(1, 12):
                    upstream = stream_rows[boundary - 1]
                    downstream = stream_rows[boundary]
                    if upstream == downstream:
                        continue
                    self.assertNotEqual(len(upstream), len(downstream))
                    self.assertLessEqual(abs(min(upstream) - min(downstream)), 1)
                    self.assertLessEqual(abs(max(upstream) - max(downstream)), 1)
                    self.assertTrue(boundary <= top - 2 or boundary >= top + 4)

                rock_cells = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[2])
                    if tile_id in WATER_ROCK_TILES
                }
                self.assertTrue(all(
                    max(abs(x - tx), abs(y - ty)) > 1
                    for x, y in rock_cells
                    for tx, ty in transition_cells
                ))

                stair_cells = {
                    (index % 16, index // 16)
                    for index, tile_id in enumerate(runtime_map.layers[0])
                    if tile_id in {CLIFF_STAIR_LEFT_TILE, CLIFF_STAIR_RIGHT_TILE}
                }
                if runtime_map.has_cliff:
                    cliff_count += 1
                    self.assertEqual(len(stair_cells), 4)
                    stair_xs = {x for x, _y in stair_cells}
                    stair_ys = {y for _x, y in stair_cells}
                    self.assertEqual(len(stair_xs), 2)
                    self.assertEqual(len(stair_ys), 2)
                    stair_left = min(stair_xs)
                    cliff_top = min(stair_ys)
                    self.assertEqual(
                        runtime_map.layers[0][cliff_top * 16 + stair_left - 1],
                        CLIFF_TOP_TILE,
                    )
                    self.assertEqual(
                        runtime_map.layers[0][cliff_top * 16 + stair_left + 2],
                        CLIFF_TOP_TILE,
                    )
                    self.assertEqual(
                        runtime_map.layers[1][(cliff_top + 1) * 16 + stair_left - 1],
                        CLIFF_FACE_TILE,
                    )
                    self.assertEqual(
                        runtime_map.layers[1][(cliff_top + 1) * 16 + stair_left + 2],
                        CLIFF_FACE_TILE,
                    )
                else:
                    self.assertFalse(stair_cells)

                for path in runtime_map.paths:
                    for point in path.points:
                        index = point[1] * 16 + point[0]
                        if runtime_map.layers[0][index] in STREAM_TILES:
                            self.assertIn(point, bridge_cells)
        self.assertGreater(transition_count, 0)
        self.assertGreater(cliff_count, 0)

    def test_ancient_waterfall_maps_keep_layered_corners_and_top_edge_forest(self):
        fingerprints = set()
        for edge in Edge:
            runtime_map = generate_map(
                0x20260713, edge, ANCIENT_WATERFALL_VALLEY_AREA
            )
            fingerprints.add(fingerprint(runtime_map))
            self.assertTrue(runtime_map.has_waterfall)
            self.assertTrue(runtime_map.has_cliff)
            self.assertTrue(runtime_map.has_forest)
            self.assertFalse(runtime_map.has_coast)
            self.assertFalse(runtime_map.has_creek)

            corner_indexes = {
                index
                for index, tile_id in enumerate(runtime_map.layers[1])
                if tile_id in {
                    SEA_TOP_LEFT_CORNER_TILE,
                    SEA_TOP_RIGHT_CORNER_TILE,
                }
            }
            self.assertEqual(len(corner_indexes), 2)
            self.assertTrue(all(
                runtime_map.layers[0][index] == SEA_CORNER_UNDERLAY_TILE
                for index in corner_indexes
            ))

            waterfall_tiles = set(WATERFALL_CREST_TILES) | {WATERFALL_BOTTOM_TILE}
            self.assertTrue(any(
                tile_id in waterfall_tiles for tile_id in runtime_map.layers[0]
            ))
            self.assertIn(BOULDER_TILE, runtime_map.layers[2])

            top_forest_indexes = {
                index
                for index, tile_id in enumerate(runtime_map.layers[0])
                if tile_id in {800, 801, 808, 809, 818, 819}
            }
            self.assertTrue(top_forest_indexes)
            self.assertTrue(all(index // 16 <= 2 for index in top_forest_indexes))

            stair_cells = {
                (index % 16, index // 16)
                for index, tile_id in enumerate(runtime_map.layers[0])
                if tile_id in {CLIFF_STAIR_LEFT_TILE, CLIFF_STAIR_RIGHT_TILE}
            }
            self.assertEqual(len(stair_cells), 10)
            for path in runtime_map.paths:
                for point in path.points:
                    index = point[1] * 16 + point[0]
                    self.assertNotIn(
                        runtime_map.layers[0][index],
                        set(WATERFALL_CREST_TILES) | {WATERFALL_BOTTOM_TILE},
                    )
        self.assertEqual(len(fingerprints), len(Edge))

    def test_seed_derivation_has_stable_snapshots(self):
        self.assertEqual(derive_seed(0x20260713, 0, 0), 0x99ADB62E)
        self.assertEqual(derive_seed(0x20260713, 1, 1), 0x73C0A14A)
        self.assertEqual(derive_seed(0x20260713, 2, 2), 0xAD5997E6)

    def test_runtime_safe_seed_generates_every_area_and_entry(self):
        for area in range(6):
            for edge in Edge:
                runtime_map = generate_map(1, edge, area)
                self.assertEqual(runtime_map.area_index, area)
                self.assertEqual(runtime_map.entry.edge, edge)
                self.assertEqual(len(runtime_map.paths), 2)

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
            for area in range(6):
                for seed in (1, 0x20260713, 0xDEADBEEF, 0xFFFFFFFF):
                    for edge in Edge:
                        expected = fingerprint(generate_map(seed, edge, area))
                        actual = int(
                            subprocess.check_output(
                                [str(binary), str(seed), str(int(edge)), str(area)],
                                text=True,
                            ).strip(),
                            16,
                        )
                        self.assertEqual(
                            actual,
                            expected,
                            f"area={area} seed={seed:#x} edge={edge.name}",
                        )


if __name__ == "__main__":
    unittest.main()
