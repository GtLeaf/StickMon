#!/usr/bin/env python3
"""Check the shared new-game and starter-selection rules."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class HatchServiceTests(unittest.TestCase):
    def test_shake_filter_cooldown_resume_and_clock_wrap(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "hatch-interaction")
            subprocess.run([
                "c++", "-std=c++17", "-Isrc", "tools/hatch_interaction_host.cpp",
                "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], check=True)

    def test_new_game_starter_and_progress_contracts(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "hatch-service")
            subprocess.run([
                "c++", "-std=c++17", "-Isrc", "tools/hatch_service_host.cpp",
                "src/game/HatchService.cpp", "src/game/MonsterFactory.cpp",
                "src/game/GameRandom.cpp", "src/game/Species.cpp", "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], check=True)


if __name__ == "__main__":
    unittest.main()
