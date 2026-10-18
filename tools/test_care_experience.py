#!/usr/bin/env python3
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class CareExperienceTests(unittest.TestCase):
    def test_daily_care_rewards_and_persistence(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = str(Path(temporary) / "care")
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Isrc",
                "tools/care_experience_host.cpp", "src/game/HomeCare.cpp",
                "src/game/BathService.cpp", "src/game/ItemInventory.cpp",
                "src/game/Species.cpp", "src/game/MonsterFactory.cpp",
                "src/game/GameRandom.cpp", "src/game/ExperienceService.cpp",
                "src/core/SaveCodec.cpp", "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], check=True)

if __name__ == "__main__":
    unittest.main()
