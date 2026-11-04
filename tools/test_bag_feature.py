#!/usr/bin/env python3
"""Verify the shared bag catalog and item outcomes without a scene or device UI."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BagFeatureTests(unittest.TestCase):
    def test_shared_catalog_and_actions(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "bag-feature-host"
            subprocess.run([
                "c++", "-std=c++17", "-Isrc", "tools/bag_feature_host.cpp",
                "src/core/features/BagFeature.cpp", "src/game/ItemInventory.cpp",
                "src/game/ExperienceService.cpp", "src/game/Species.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
