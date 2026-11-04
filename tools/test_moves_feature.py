#!/usr/bin/env python3
"""Validate move-page semantic selection against the shared move rules."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MovesFeatureTests(unittest.TestCase):
    def test_semantic_flow_and_shared_game_outcomes(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "moves-feature"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Isrc",
                "tools/moves_feature_host.cpp", "src/core/features/MovesFeature.cpp",
                "src/game/MoveManagementService.cpp", "src/game/ItemInventory.cpp",
                "src/game/MonsterFactory.cpp", "src/game/Species.cpp",
                "src/game/GameRandom.cpp", "src/game/ExperienceService.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
