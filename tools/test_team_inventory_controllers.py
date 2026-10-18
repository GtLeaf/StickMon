#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class TeamInventoryControllerTests(unittest.TestCase):
    def test_page_state_and_shared_service_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "team_inventory_controller_host"
            subprocess.run(
                [
                    "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Isrc", "-Ifirmware/amoled_1_8_v1/main",
                    "tools/team_inventory_controller_host.cpp",
                    "firmware/amoled_1_8_v1/main/app/TeamController.cpp",
                    "firmware/amoled_1_8_v1/main/app/InventoryController.cpp",
                    "src/game/ItemInventory.cpp", "src/game/ShopService.cpp",
                    "src/game/MoveManagementService.cpp",
                    "src/game/ExperienceService.cpp", "src/game/GameRandom.cpp",
                    "src/game/MonsterFactory.cpp", "src/game/Species.cpp",
                    "-o", str(binary),
                ], cwd=ROOT, check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
