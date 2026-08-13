#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ExploreAreaCatalogTests(unittest.TestCase):
    def test_catalog_and_unlock_progression(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "explore_area_catalog_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Isrc",
                    "tools/explore_area_catalog_host.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
