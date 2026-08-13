#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RoomMovementAreaTests(unittest.TestCase):
    def test_polygon_footprint_and_segment_rules(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "room_movement_area_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Werror",
                    "-Isrc",
                    "tools/room_movement_area_host.cpp",
                    "src/core/RoomMovementArea.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
