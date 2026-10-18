#!/usr/bin/env python3
"""Verify map-anchored grass contact and rebound without renderer state."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ExploreGrassTests(unittest.TestCase):
    def test_contacts_and_lifecycle(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "explore-grass")
            subprocess.run([
                "c++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
                "-Isrc", "tools/explore_grass_host.cpp", "src/game/ExploreGrass.cpp",
                "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
