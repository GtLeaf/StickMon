#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class HomeCareTests(unittest.TestCase):
    def test_food_bowl_and_pet_rules(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "home_care_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Isrc",
                    "tools/home_care_host.cpp",
                    "src/game/HomeCare.cpp",
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
