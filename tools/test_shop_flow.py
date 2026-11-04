#!/usr/bin/env python3
"""Validate the UI-independent shop feature with real shared rules and saves."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ShopFlowTests(unittest.TestCase):
    def test_catalog_navigation_and_atomic_transactions(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "shop-flow"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Isrc",
                "tools/shop_flow_host.cpp", "src/core/features/ShopFlow.cpp",
                "src/core/SaveCodec.cpp", "src/game/ShopService.cpp",
                "src/game/ItemInventory.cpp", "src/game/ExperienceService.cpp",
                "src/game/Species.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
