#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class BathServiceTests(unittest.TestCase):
    def test_shared_bath_inventory_and_rewards(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "bath_service_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Isrc",
                    "tools/bath_service_host.cpp",
                    "src/game/BathService.cpp",
                    "src/game/ItemInventory.cpp",
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
