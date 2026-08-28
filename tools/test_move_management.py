#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MoveManagementTests(unittest.TestCase):
    def test_host_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "move_management_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Isrc",
                    "tools/move_management_host.cpp",
                    "src/game/ItemInventory.cpp",
                    "src/game/MoveManagementService.cpp",
                    "src/game/ExperienceService.cpp",
                    "src/game/GameRandom.cpp",
                    "src/game/MonsterFactory.cpp",
                    "src/game/Species.cpp",
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
