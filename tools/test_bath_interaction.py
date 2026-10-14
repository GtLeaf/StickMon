#!/usr/bin/env python3
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BathInteractionTests(unittest.TestCase):
    def test_shared_foam_rules_geometry_and_time(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "bath-interaction")
            subprocess.run(["c++", "-std=c++17", "-Isrc",
                            "tools/bath_interaction_host.cpp", "src/game/BathInteraction.cpp",
                            "src/game/GameRandom.cpp", "-o", binary], cwd=ROOT, check=True)
            subprocess.run([binary], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
