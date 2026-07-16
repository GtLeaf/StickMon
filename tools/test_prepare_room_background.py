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
        self.assertEqual(values[12], 1)
        self.assertEqual(values[25:29], (41, 127, 79, 155))
        self.assertEqual(values[29:31], (68, 132))
        self.assertEqual(values[31:33], (53, 150))
        anchor = struct.unpack_from(
            room_assets.ROOM_BEHAVIOR_ANCHOR_FORMAT,
            data,
            len(data) - struct.calcsize(room_assets.ROOM_BEHAVIOR_ANCHOR_FORMAT),
        )
        self.assertEqual(anchor, (
            room_assets.ROOM_ANCHOR_WINDOW_GAZE,
            room_assets.ROOM_FACING_BACK,
            159,
            93,
        ))


if __name__ == "__main__":
    unittest.main()
