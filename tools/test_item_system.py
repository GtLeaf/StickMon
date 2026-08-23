#!/usr/bin/env python3

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def array_items(source, name):
    match = re.search(
        rf"static constexpr Item {name}\[\]\s*=\s*\{{(.*?)\}};",
        source,
        re.S,
    )
    if not match:
        raise AssertionError(f"missing {name}")
    return re.findall(r"\b[A-Z][A-Z0-9_]*\b", match.group(1))


def pickup_entries(source, name):
    match = re.search(
        rf"static constexpr PickupEntry {name}\[\]\s*=\s*\{{(.*?)\}};",
        source,
        re.S,
    )
    if not match:
        raise AssertionError(f"missing {name}")
    return {
        pickup: int(weight)
        for pickup, weight in re.findall(
            r"\{\s*(PICKUP_[A-Z0-9_]+)\s*,\s*(\d+)\s*\}",
            match.group(1),
        )
    }


class ItemSystemTests(unittest.TestCase):
    def compile_and_run_host(self, source_name):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / Path(source_name).stem
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / source_name),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run(
                [str(binary)],
                check=True,
                capture_output=True,
                text=True,
            )

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_session_item_effects(self):
        self.compile_and_run_host("item_system_host.cpp")

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_shop_unlocks_follow_exploration_progress(self):
        self.compile_and_run_host("explore_item_progression_host.cpp")

    def test_shop_explore_catalog_keeps_all_products_and_back(self):
        source = (ROOT / "src" / "scenes" / "ShopScene.h").read_text()
        items = array_items(source, "EXPLORE_ITEMS")
        expected = [
            "POTION",
            "SUPER_POTION",
            "ANTIDOTE",
            "PARALYZE_HEAL",
            "AWAKENING",
            "BURN_HEAL",
            "ICE_HEAL",
            "MAX_POTION",
            "FULL_RESTORE",
            "FIRE_STONE",
            "WATER_STONE",
            "THUNDER_STONE",
            "REVIVE",
            "MAX_REPEL",
            "HONEY",
            "BACK",
        ]
        self.assertEqual(expected, items)

    def test_explore_pickup_distribution_matches_item_design(self):
        source = (ROOT / "src" / "scenes" / "ExploreScene.cpp").read_text()
        expected = {
            "GRASS_PATH_PICKUPS": {
                "PICKUP_COIN": 40,
                "PICKUP_POTION": 25,
                "PICKUP_ANTIDOTE": 10,
                "PICKUP_HONEY": 10,
                "PICKUP_NUGGET": 3,
                "PICKUP_RARE_CANDY": 2,
            },
            "CREEK_SLOPE_PICKUPS": {
                "PICKUP_COIN": 40,
                "PICKUP_POTION": 20,
                "PICKUP_ANTIDOTE": 10,
                "PICKUP_MAX_REPEL": 8,
                "PICKUP_HONEY": 8,
                "PICKUP_NUGGET": 5,
                "PICKUP_RARE_CANDY": 2,
            },
            "TALL_GRASS_PARK_PICKUPS": {
                "PICKUP_COIN": 38,
                "PICKUP_SUPER_POTION": 20,
                "PICKUP_ANTIDOTE": 8,
                "PICKUP_REVIVE": 5,
                "PICKUP_MAX_REPEL": 8,
                "PICKUP_NUGGET": 6,
                "PICKUP_BIG_PEARL": 3,
                "PICKUP_RARE_CANDY": 2,
            },
            "FROST_CRYSTAL_CAVE_PICKUPS": {
                "PICKUP_COIN": 36,
                "PICKUP_SUPER_POTION": 18,
                "PICKUP_FULL_HEAL": 8,
                "PICKUP_REVIVE": 6,
                "PICKUP_MAX_REPEL": 6,
                "PICKUP_NUGGET": 4,
                "PICKUP_BIG_PEARL": 6,
                "PICKUP_HEART_SCALE": 2,
                "PICKUP_RARE_CANDY": 2,
            },
            "MIST_FOREST_PATH_PICKUPS": {
                "PICKUP_COIN": 34,
                "PICKUP_MAX_POTION": 15,
                "PICKUP_FULL_HEAL": 8,
                "PICKUP_REVIVE": 7,
                "PICKUP_BIG_PEARL": 7,
                "PICKUP_STAR_PIECE": 4,
                "PICKUP_HEART_SCALE": 2,
                "PICKUP_RARE_CANDY": 2,
            },
            "ANCIENT_WATERFALL_VALLEY_PICKUPS": {
                "PICKUP_COIN": 32,
                "PICKUP_MAX_POTION": 15,
                "PICKUP_FULL_RESTORE": 6,
                "PICKUP_REVIVE": 8,
                "PICKUP_BIG_PEARL": 5,
                "PICKUP_STAR_PIECE": 7,
                "PICKUP_HEART_SCALE": 2,
                "PICKUP_RARE_CANDY": 2,
            },
        }
        coin_ranges = {
            "GRASS_PATH_PICKUPS": (10, 30),
            "CREEK_SLOPE_PICKUPS": (15, 40),
            "TALL_GRASS_PARK_PICKUPS": (20, 60),
            "FROST_CRYSTAL_CAVE_PICKUPS": (30, 80),
            "MIST_FOREST_PATH_PICKUPS": (40, 110),
            "ANCIENT_WATERFALL_VALLEY_PICKUPS": (50, 150),
        }

        for table_name, entries in expected.items():
            self.assertEqual(entries, pickup_entries(source, table_name))
            range_match = re.search(
                rf"{table_name},\s*ENTRY_COUNT\({table_name}\),\s*"
                rf"(\d+),\s*(\d+)",
                source,
            )
            self.assertIsNotNone(range_match, table_name)
            self.assertEqual(
                coin_ranges[table_name],
                tuple(int(value) for value in range_match.groups()),
            )

        self.assertIn("stepsToday >= 5000", source)

    def test_new_battle_medicines_have_results(self):
        source = (ROOT / "src" / "scenes" / "MenuScene.h").read_text()
        match = re.search(
            r"enum class BattleBagResult.*?\{(.*?)\};", source, re.S
        )
        self.assertIsNotNone(match)
        values = set(re.findall(r"\b[A-Z][A-Z0-9_]*\b", match.group(1)))
        self.assertTrue(
            {"MAX_POTION", "FULL_RESTORE", "FULL_HEAL", "REVIVE"} <= values
        )

    def test_battle_bag_can_open_extra_medicine_confirmation(self):
        source = (ROOT / "src" / "scenes" / "MenuScene.cpp").read_text()
        self.assertIn(
            "source >= BAG_SOURCE_EXTRA_BASE &&\n"
            "                       source < BAG_SOURCE_EXTRA_BASE + BAG_SOURCE_EXTRA_COUNT &&\n"
            "                       (!battleBagMode ||",
            source,
        )
        self.assertIn(
            "source == BAG_SOURCE_EXTRA_BASE + 0 ||",
            source,
        )

    def test_new_items_use_dedicated_assets(self):
        source = (ROOT / "src" / "assets" / "GameAssets.cpp").read_text()
        item_names = [
            "MAX_POTION",
            "FULL_RESTORE",
            "FULL_HEAL",
            "FIRE_STONE",
            "WATER_STONE",
            "THUNDER_STONE",
            "REVIVE",
            "MAX_REPEL",
            "HONEY",
            "NUGGET",
            "BIG_PEARL",
            "STAR_PIECE",
            "HEART_SCALE",
        ]
        for name in item_names:
            self.assertRegex(
                source,
                rf"case Game::ItemId::{name}:\s*return Kind::ITEM_{name};",
            )

    def test_stone_is_consumed_only_after_evolution_animation(self):
        header = (ROOT / "src" / "core" / "GameEngine.h").read_text()
        source = (ROOT / "src" / "core" / "GameEngine.cpp").read_text()
        self.assertIn(
            "Game::ItemId consumedItem = Game::ItemId::COUNT;", header
        )
        use_stone = re.search(
            r"bool GameEngine::useEvolutionStone\(.*?"
            r"uint8_t GameEngine::grantBathReward",
            source,
            re.S,
        )
        acknowledge = re.search(
            r"bool GameEngine::acknowledgePendingEvolution\(\).*?"
            r"bool GameEngine::cancelPendingEvolution",
            source,
            re.S,
        )
        self.assertIsNotNone(use_stone)
        self.assertIsNotNone(acknowledge)
        self.assertNotIn("removeItem(", use_stone.group(0))
        self.assertIn(
            "removeItem(event.consumedItem, 1, SaveUrgency::SOON)",
            acknowledge.group(0),
        )

    def test_save_format_keeps_v1_v2_to_v3_migration(self):
        state = (ROOT / "src" / "game" / "GameState.h").read_text()
        manager = (ROOT / "src" / "core" / "SaveManager.cpp").read_text()
        self.assertRegex(state, r"SAVE_VERSION\s*=\s*3\s*;")
        self.assertRegex(state, r"MIN_SUPPORTED_SAVE_VERSION\s*=\s*1\s*;")
        self.assertIn("LEGACY_SAVE_RECORD_VERSION_V1", manager)
        self.assertIn("struct SaveRecordV1", manager)
        self.assertIn("struct SaveRecordV1WithViewV2", manager)
        self.assertIn("migrateLegacySaveRecord", manager)
        self.assertIn("migration committed", manager)
        self.assertIn("legacy blob retained for retry", manager)
        self.assertIn("sizeof(SaveRecordV1) == 1612", manager)
        engine = (ROOT / "src" / "core" / "GameEngine.cpp").read_text()
        self.assertIn("SaveManager::LoadStatus::NEWER_VERSION", engine)
        self.assertIn("SaveManager::LoadStatus::INVALID", engine)
        self.assertIn("saveWritesBlocked", engine)
        self.assertIn("!loadedState && !saveWritesBlocked", engine)


if __name__ == "__main__":
    unittest.main()
