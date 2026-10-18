#!/usr/bin/env python3
"""Verify ecology against real generated maps and deterministic simulation."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ExploreEcologyTests(unittest.TestCase):
    def test_spawn_behavior_pause_habitats_and_pool(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "explore-ecology")
            subprocess.run([
                "c++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-parameter", "-Wno-unused-function", "-Isrc",
                "tools/explore_ecology_host.cpp", "src/game/ExploreEcology.cpp",
                "src/game/ExploreEcologyFlight.cpp",
                "src/game/ExploreEcologyGroup.cpp",
                "src/game/ExploreMapGenerator.cpp", "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
