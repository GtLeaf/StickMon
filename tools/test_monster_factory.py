#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MonsterFactoryTests(unittest.TestCase):
    def test_individual_value_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "monster_factory_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Isrc",
                    "tools/monster_factory_host.cpp",
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
