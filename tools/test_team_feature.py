#!/usr/bin/env python3
"""Verify roster capabilities without linking a scene, renderer, or device SDK."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class TeamFeatureTests(unittest.TestCase):
    def test_formal_visitor_and_exploration_capabilities(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "team-feature-host"
            subprocess.run([
                "c++", "-std=c++17", "-Isrc", "tools/team_feature_host.cpp",
                "src/core/features/TeamFeature.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
