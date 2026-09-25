#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from cave_tile_semantics import (
    CAVE_CLIFF_RUNTIME_TILES,
    CAVE_ENTRANCE_RUNTIME_TILES,
    CAVE_FLOOR_RUNTIME_TILE,
    CAVE_ROCK_STEP_RUNTIME_TILE,
    CAVE_RUNTIME_FLIP_Y_IDS,
    CAVE_RUNTIME_ROTATIONS,
    CAVE_RUNTIME_TILE_SOURCES,
    FROST_BROKEN_ICE_HOLE_RUNTIME_TILE,
    FROST_CAVE_HOLE_RUNTIME_TILE,
    FROST_EXIT_DIRECTIONAL_RUNTIME_TILES,
    FROST_DEEP_ENTRANCE_RUNTIME_TILE,
    FROST_UP_LADDER_RUNTIME_TILES,
    FROST_DOWNWARD_STAIRS_RUNTIME_TILE,
)

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
    FROST_WALL_TILES,
    MIST_FOREST_PATH_AREA,
    SEA_SHORE_TILE,
    SEA_CORNER_UNDERLAY_TILE,
    SEA_LEFT_SHORE_TILE,
    SEA_RIGHT_SHORE_TILE,
    SEA_TOP_SHORE_TILE,
    SEA_WATER_TILE,
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
    WATERFALL_BODY_LOWER_TILES,
    WATERFALL_BODY_MIDDLE_TILES,
    WATERFALL_BODY_TOP_TILES,
    WATERFALL_CREST_TILES,
    connected_components,
    derive_seed,
    fingerprint,
    generate_map,
    is_smooth_ice_tile,
    route_crosses_ice_straight,
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
        ground_ids = {
            FROST_OUTSIDE_TILE, FROST_FLOOR_TILE, *ice_ids,
            4505, 4506,
        }
        wall_ids = set(FROST_WALL_TILES.values()) | set(
            FROST_INNER_CORNER_TILES.values()
        )
        scenery_ids = {4507, 4508, 4509, 4510, 4542, 4543, 4544}
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
                0, *wall_ids, *scenery_ids,
                *(tile_id for tiles in FROST_EXIT_DIRECTIONAL_RUNTIME_TILES.values()
                  for tile_id in tiles),
            }))
            self.assertFalse(any(4532 <= tile_id <= 4540 for layer in runtime_map.layers
                                 for tile_id in layer))
            self.assertFalse(any(4705 <= tile_id <= 4740 for layer in runtime_map.layers
                                 for tile_id in layer))
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
                    self.assertIn(runtime_map.layers[1][index], {
                        0,
                        *(tile_id for tiles in FROST_EXIT_DIRECTIONAL_RUNTIME_TILES.values()
                          for tile_id in tiles),
                    })

            self.assertGreaterEqual(
                len({
                    fingerprint(generate_map(seed, edge,
                                             FROST_CRYSTAL_CAVE_AREA))
                    for seed in range(1, 65)
                }),
                16,
            )

        for seed in range(1, 65):
            for edge in Edge:
                runtime_map = generate_map(seed, edge, FROST_CRYSTAL_CAVE_AREA)
                has_ice = any(tile_id in ice_ids for tile_id in runtime_map.layers[0])
                self.assertFalse(any(4532 <= tile_id <= 4540
                                     for layer in runtime_map.layers
                                     for tile_id in layer))
                observed_profiles.add(has_ice)
        self.assertEqual(
            observed_profiles,
            {False, True},
        )

    def test_frost_ice_routes_slide_straight_and_fork_on_dry_ground(self):
        observed_ice_profiles = set()
        for seed in range(1, 65):
            for edge in Edge:
                runtime_map = generate_map(seed, edge, FROST_CRYSTAL_CAVE_AREA)
                route_ice_counts = []
                for path in runtime_map.paths:
                    self.assertTrue(route_crosses_ice_straight(runtime_map, path))
                    route_ice_counts.append(sum(
                        is_smooth_ice_tile(
                            runtime_map.layers[0][y * 16 + x]
                        )
                        for x, y in path.points
                    ))
                junction_index = runtime_map.junction[1] * 16 + runtime_map.junction[0]
                self.assertFalse(is_smooth_ice_tile(
                    runtime_map.layers[0][junction_index]
                ))
                if any(route_ice_counts):
                    self.assertTrue(all(count > 0 for count in route_ice_counts))
                observed_ice_profiles.add(tuple(count > 0 for count in route_ice_counts))
        self.assertEqual(observed_ice_profiles, {(False, False), (True, True)})

    def test_frost_level_portals_cracks_and_landings_stay_connected(self):
        exterior = {
            tile_id for tiles in FROST_EXIT_DIRECTIONAL_RUNTIME_TILES.values()
            for tile_id in tiles
        }
        for seed in range(1, 257):
            for edge in Edge:
                first = generate_map(seed, edge, FROST_CRYSTAL_CAVE_AREA,
                                     frost_level=0, frost_level_count=3)
                middle = generate_map(seed, edge, FROST_CRYSTAL_CAVE_AREA,
                                      frost_level=1, frost_level_count=3,
                                      frost_entered_by_ladder=True)
                last = generate_map(seed, edge, FROST_CRYSTAL_CAVE_AREA,
                                    frost_level=2, frost_level_count=3)
                self.assertEqual(len(exterior & set(first.layers[1])), 3)
                self.assertFalse(exterior & set(middle.layers[1]))
                self.assertTrue(exterior & set(last.layers[1]))
                self.assertNotIn(4506, first.layers[0])
                self.assertFalse(first.paths[0].falls_to_next_level)
                self.assertFalse(first.paths[1].falls_to_next_level)
                self.assertNotIn(4506, middle.layers[0])
                self.assertFalse(middle.paths[0].falls_to_next_level)
                self.assertFalse(middle.paths[1].falls_to_next_level)
                for path in last.paths:
                    candidates = []
                    for path_index in range(1, len(path.points) - 2):
                        x, y = path.points[path_index]
                        cell = y * 16 + x
                        if (last.layers[0][cell] == 4511 and
                                last.layers[1][cell] == 0 and
                                last.layers[2][cell] == 0):
                            candidates.append(path_index)
                    self.assertTrue(candidates)
                for index, tile_id in enumerate(middle.layers[1]):
                    if tile_id != FROST_DEEP_ENTRANCE_RUNTIME_TILE:
                        continue
                    x, y = index % 16, index // 16
                    adjacent = (
                        middle.layers[1][neighbor_y * 16 + neighbor_x]
                        for neighbor_x, neighbor_y in (
                            (x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)
                        ) if 0 <= neighbor_x < 16 and 0 <= neighbor_y < 12
                    )
                    self.assertTrue(any(tile in set(FROST_WALL_TILES.values())
                                        for tile in adjacent))

        layouts = {
            (runtime_map.entry.point,
             tuple(tuple(path.points) for path in runtime_map.paths))
            for seed in range(256)
            for runtime_map in (
                generate_map(seed, Edge.TOP, FROST_CRYSTAL_CAVE_AREA,
                             frost_level=0, frost_level_count=3),
            )
        }
        self.assertEqual(len(layouts), 4)

    def test_cave_runtime_aliases_are_complete_and_portals_are_atomic(self):
        runtime_ids = [runtime_id for runtime_id, _source_id in CAVE_RUNTIME_TILE_SOURCES]
        self.assertEqual(runtime_ids, list(range(4700, 4774)))
        self.assertEqual(CAVE_RUNTIME_FLIP_Y_IDS, frozenset((4738, 4739, 4740)))
        source_by_runtime_id = dict(CAVE_RUNTIME_TILE_SOURCES)
        self.assertEqual(source_by_runtime_id[4741], 1301)
        for runtime_id in (4742, 4763, 4766, 4769):
            self.assertEqual(source_by_runtime_id[runtime_id], 1300)
        self.assertEqual(source_by_runtime_id[4743], 1299)
        self.assertEqual(CAVE_RUNTIME_ROTATIONS[4741], 180)
        self.assertEqual(CAVE_RUNTIME_ROTATIONS[4742], 180)
        self.assertEqual(CAVE_RUNTIME_ROTATIONS[4743], 180)
        self.assertEqual(FROST_EXIT_DIRECTIONAL_RUNTIME_TILES["top"],
                         (4743, 4742, 4741))
        self.assertEqual(CAVE_RUNTIME_ROTATIONS[4762], 270)
        self.assertEqual(CAVE_RUNTIME_ROTATIONS[4768], 90)
        self.assertNotIn(4765, CAVE_RUNTIME_ROTATIONS)

        observed_edges = set()
        for edge in Edge:
            runtime_map = generate_map(0x20260713, edge, FROST_CRYSTAL_CAVE_AREA)
            endpoints = (runtime_map.entry, *(path.exit for path in runtime_map.paths))
            route_anchors = (
                runtime_map.paths[0].points[0],
                *(path.points[-1] for path in runtime_map.paths),
            )
            for endpoint, route_anchor in zip(endpoints, route_anchors):
                observed_edges.add(endpoint.edge)
                x, y = route_anchor
                tiles = FROST_EXIT_DIRECTIONAL_RUNTIME_TILES[
                    endpoint.edge.name.lower()
                ]
                self.assertEqual(runtime_map.layers[1][y * 16 + x], tiles[1])
                if endpoint.edge in (Edge.TOP, Edge.BOTTOM):
                    portal_cells = [
                        (x - 1 + offset, y) for offset in range(3)
                    ]
                else:
                    portal_cells = [
                        (x, y - 1 + offset) for offset in range(3)
                    ]
                self.assertEqual(
                    [runtime_map.layers[1][cell_y * 16 + cell_x]
                     for cell_x, cell_y in portal_cells],
                    list(tiles),
                )
                self.assertNotIn(
                    4747,
                    runtime_map.layers[1],
                )
        self.assertEqual(observed_edges, set(Edge))

        for edge in Edge:
            first = generate_map(0x20260713, edge, FROST_CRYSTAL_CAVE_AREA,
                                 frost_level=0, frost_level_count=3)
            middle = generate_map(0x13579BDF, edge, FROST_CRYSTAL_CAVE_AREA,
                                  frost_level=1, frost_level_count=3,
                                  frost_entered_by_ladder=True)
            last = generate_map(0xC0FFEE01, edge, FROST_CRYSTAL_CAVE_AREA,
                                frost_level=2, frost_level_count=3)
            self.assertIn(FROST_UP_LADDER_RUNTIME_TILES[1], first.layers[1])
            self.assertIn(FROST_UP_LADDER_RUNTIME_TILES[0], first.layers[2])
            self.assertIn(FROST_DOWNWARD_STAIRS_RUNTIME_TILE, middle.layers[1])
            self.assertIn(FROST_UP_LADDER_RUNTIME_TILES[1], middle.layers[1])
            self.assertIn(FROST_UP_LADDER_RUNTIME_TILES[0], middle.layers[2])
            self.assertNotIn(4506, first.layers[0])
            self.assertNotIn(4506, middle.layers[0])
            self.assertNotIn(4506, last.layers[0])
            self.assertFalse(first.paths[0].falls_to_next_level)
            self.assertFalse(middle.paths[0].falls_to_next_level)
            self.assertFalse(middle.paths[1].falls_to_next_level)
            middle_path0_exit = middle.paths[0].points[-1]
            self.assertEqual(
                middle.layers[1][middle_path0_exit[1] * 16 +
                                  middle_path0_exit[0]],
                FROST_DOWNWARD_STAIRS_RUNTIME_TILE,
            )
            for path in middle.paths:
                self.assertTrue(0 < path.exit.point[0] < 15)
                self.assertTrue(0 < path.exit.point[1] < 11)

            fallen = generate_map(0x13579BDF, edge, FROST_CRYSTAL_CAVE_AREA,
                                  frost_level=1, frost_level_count=3,
                                  frost_entered_by_ladder=False)
            self.assertNotIn(FROST_DEEP_ENTRANCE_RUNTIME_TILE,
                             fallen.layers[1])

        for seed in range(1, 65):
            runtime_map = generate_map(seed, Edge.TOP, FROST_CRYSTAL_CAVE_AREA)
            all_tiles = set().union(*map(set, runtime_map.layers))
            self.assertNotIn(FROST_BROKEN_ICE_HOLE_RUNTIME_TILE, all_tiles)
            self.assertNotIn(FROST_CAVE_HOLE_RUNTIME_TILE, all_tiles)

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

    def test_ancient_waterfall_maps_use_cave_ground_cliffs_and_portals(self):
        fingerprints = set()
        for edge in Edge:
            runtime_map = generate_map(
                0x20260713, edge, ANCIENT_WATERFALL_VALLEY_AREA
            )
            fingerprints.add(fingerprint(runtime_map))
            self.assertTrue(runtime_map.has_waterfall)
            self.assertTrue(runtime_map.has_cliff)
            self.assertFalse(runtime_map.has_forest)
            self.assertFalse(runtime_map.has_coast)
            self.assertFalse(runtime_map.has_creek)

            waterfall_tiles = (
                set(WATERFALL_CREST_TILES)
                | set(WATERFALL_BODY_TOP_TILES)
                | set(WATERFALL_BODY_MIDDLE_TILES)
                | set(WATERFALL_BODY_LOWER_TILES)
                | {WATERFALL_BOTTOM_TILE}
            )
            cave_ground_tiles = {
                CAVE_FLOOR_RUNTIME_TILE,
                CAVE_ROCK_STEP_RUNTIME_TILE,
                *CAVE_CLIFF_RUNTIME_TILES,
            }
            self.assertTrue(set(runtime_map.layers[0]).issubset(
                cave_ground_tiles
                | waterfall_tiles
                | {SEA_WATER_TILE, SEA_LEFT_SHORE_TILE, SEA_TOP_SHORE_TILE,
                   SEA_RIGHT_SHORE_TILE, SEA_TOP_LEFT_CORNER_TILE,
                   SEA_TOP_RIGHT_CORNER_TILE, SEA_CORNER_UNDERLAY_TILE}
            ))
            self.assertNotIn(BOULDER_TILE, runtime_map.layers[2])
            self.assertTrue(any(tile_id in CAVE_CLIFF_RUNTIME_TILES
                                for tile_id in runtime_map.layers[0]))
            self.assertTrue(any(tile_id == CAVE_FLOOR_RUNTIME_TILE
                                for tile_id in runtime_map.layers[0]))
            entrance_tiles = {
                tile_id
                for tiles in CAVE_ENTRANCE_RUNTIME_TILES.values()
                for tile_id in tiles
            }
            self.assertTrue(entrance_tiles & set(runtime_map.layers[1]))

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

            self.assertTrue(any(
                tile_id in waterfall_tiles for tile_id in runtime_map.layers[0]
            ))

            stair_cells = {
                (index % 16, index // 16)
                for index, tile_id in enumerate(runtime_map.layers[0])
                if tile_id == CAVE_ROCK_STEP_RUNTIME_TILE
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
            for level in range(3):
                for ladder in (False, True):
                    for seed in (1, 0x20260713, 0xDEADBEEF):
                        for edge in Edge:
                            runtime_map = generate_map(
                                seed, edge, FROST_CRYSTAL_CAVE_AREA,
                                frost_level=level, frost_level_count=3,
                                frost_entered_by_ladder=ladder,
                            )
                            actual = int(subprocess.check_output([
                                str(binary), str(seed), str(int(edge)),
                                str(FROST_CRYSTAL_CAVE_AREA), str(level), "3",
                                str(int(ladder)),
                            ], text=True).strip(), 16)
                            self.assertEqual(actual, fingerprint(runtime_map))


if __name__ == "__main__":
    unittest.main()
