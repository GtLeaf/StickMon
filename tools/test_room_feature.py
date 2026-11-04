#!/usr/bin/env python3
"""Validate room/food queries and dispatch without a game engine or device SDK."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RoomFeatureTests(unittest.TestCase):
    def test_food_boundaries_and_common_action_dispatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "room-feature"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Isrc",
                "tools/room_feature_host.cpp", "src/core/features/RoomFeature.cpp",
                "src/ui/common/RoomUiBase.cpp", "src/game/ItemInventory.cpp",
                "src/game/Species.cpp", "src/game/ExperienceService.cpp",
                "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
