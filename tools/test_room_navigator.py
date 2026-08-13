#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RoomNavigatorTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_route_can_enter_from_outside_and_turn_inside(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "room_navigator_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Werror",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "room_navigator_host.cpp"),
                    str(ROOT / "src" / "core" / "RoomNavigator.cpp"),
                    str(ROOT / "src" / "core" / "RoomMovementArea.cpp"),
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
