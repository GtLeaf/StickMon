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

    def test_first_release_save_format_has_no_migrations(self):
        state = (ROOT / "src" / "game" / "GameState.h").read_text()
        manager = (ROOT / "src" / "core" / "SaveManager.cpp").read_text()
        self.assertRegex(state, r"SAVE_VERSION\s*=\s*1\s*;")
        self.assertNotIn("LEGACY_SAVE_RECORD_VERSION", manager)
        self.assertNotRegex(manager, r"\bmigrat(?:e|ed|ion)")


if __name__ == "__main__":
    unittest.main()
