#!/usr/bin/env python3
"""Verify exploration lighting rules and the shared RGB565 transform."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ExploreDaylightTests(unittest.TestCase):
    def test_timeline_and_color_transform(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "explore-daylight")
            subprocess.run([
                "c++", "-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror",
                "-Isrc", "tools/explore_daylight_host.cpp", "src/presentation/Canvas565.cpp",
                "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
