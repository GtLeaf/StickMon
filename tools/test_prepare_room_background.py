#!/usr/bin/env python3
import struct
import unittest
from pathlib import Path

import prepare_room_background as room_assets


class RoomPackTests(unittest.TestCase):
    def test_room_pack_v3_header_size(self):
        self.assertEqual(room_assets.ROOM_PACK_VERSION, 3)
        self.assertEqual(struct.calcsize(room_assets.ROOM_PACK_HEADER_FORMAT), 76)
        self.assertEqual(struct.calcsize(room_assets.ROOM_BEHAVIOR_ANCHOR_FORMAT), 6)

    def test_doorway_anchors_follow_walk_area_direction(self):
        layout = {
            "roomGeometry": {
                "faces": [{
                    "id": "door",
                    "type": "doorway",
                    "points": [[41, 144], [65, 155], [79, 136], [56, 127]],
                }],
            },
        }
        walk = [
            (121, 73), (107, 75), (94, 67), (71, 64),
            (50, 67), (37, 78), (39, 91), (60, 100),
            (58, 118), (45, 124), (133, 155), (234, 110),
        ]

        polygon, inside, outside = room_assets.doorway_region(layout, 240, 161, walk)

        self.assertEqual(polygon, [(41, 144), (65, 155), (79, 136), (56, 127)])
        self.assertEqual(inside, (68, 132))
        self.assertEqual(outside, (53, 150))

    def test_generated_standard_room_contains_doorway_and_window_anchor(self):
        pack = Path(__file__).resolve().parents[1] / "data/packs/dev/rooms/standard.smonroom"
        data = pack.read_bytes()
        values = struct.unpack_from(room_assets.ROOM_PACK_HEADER_FORMAT, data)

        self.assertEqual(values[0], room_assets.ROOM_PACK_MAGIC)
        self.assertEqual(values[1], room_assets.ROOM_PACK_VERSION)
        self.assertEqual(values[11], 4)
        self.assertEqual(values[12], 2)
        self.assertEqual(values[25:29], (41, 127, 79, 155))
        self.assertEqual(values[29:31], (68, 132))
        self.assertEqual(values[31:33], (53, 150))
        anchor_size = struct.calcsize(room_assets.ROOM_BEHAVIOR_ANCHOR_FORMAT)
        window_anchor = struct.unpack_from(
            room_assets.ROOM_BEHAVIOR_ANCHOR_FORMAT,
            data,
            len(data) - anchor_size * 2,
        )
        sleep_anchor = struct.unpack_from(
            room_assets.ROOM_BEHAVIOR_ANCHOR_FORMAT,
            data,
            len(data) - anchor_size,
        )
        self.assertEqual(window_anchor, (
            room_assets.ROOM_ANCHOR_WINDOW_GAZE,
            room_assets.ROOM_FACING_BACK,
            159,
            93,
        ))
        self.assertEqual(sleep_anchor, (
            room_assets.ROOM_ANCHOR_VISITOR_SLEEP,
            0,
            132,
            117,
        ))

    def test_visitor_sleep_anchor_prefers_carpet_center(self):
        layout = {
            "furniture": [{
                "name": "carpet",
                "source": "furniture",
                "visible": True,
                "kind": "floor_flat",
                "x": 30,
                "y": 30,
                "targetWidth": 40,
                "targetHeight": 40,
            }],
        }
        walk = [(0, 0), (99, 0), (99, 99), (0, 99)]

        self.assertEqual(
            room_assets.visitor_sleep_anchor_for_layout(
                layout, 100, 100, walk),
            (50, 50),
        )

    def test_visitor_sleep_anchor_avoids_furniture_without_carpet(self):
        layout = {
            "furniture": [{
                "name": "table",
                "source": "furniture",
                "visible": True,
                "kind": "floor_furniture",
                "footprint": "polygon",
                "x": 45,
                "y": 40,
                "targetWidth": 30,
                "targetHeight": 30,
                "footprintPolygon": [
                    [0, 0], [1, 0], [1, 1], [0, 1],
                ],
            }],
        }
        walk = [(0, 0), (119, 0), (119, 99), (0, 99)]
        point = room_assets.visitor_sleep_anchor_for_layout(
            layout, 120, 100, walk)
        obstacle = room_assets.furniture_obstacle_polygons(
            layout, 120, 100)[0]

        self.assertIsNotNone(point)
        self.assertGreaterEqual(
            room_assets.point_polygon_distance_squared(
                point[0], point[1], obstacle),
            18.0 ** 2,
        )


if __name__ == "__main__":
    unittest.main()
