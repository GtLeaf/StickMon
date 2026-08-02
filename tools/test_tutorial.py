#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class TutorialTests(unittest.TestCase):
    def read(self, relative_path):
        return (ROOT / relative_path).read_text(encoding="utf-8")

    def test_all_tutorial_steps_have_unique_persistent_bits(self):
        source = self.read("src/game/Tutorial.h")
        values = [
            int(bit)
            for bit in re.findall(r"^\s*[A-Z_]+\s*=\s*1U\s*<<\s*(\d+),", source, re.M)
        ]
        self.assertEqual(values, list(range(8)))
        self.assertIn("TUTORIAL_ALL_FLAGS = 0xFF", source)

    def test_progress_only_completes_at_semantic_scene_actions(self):
        expected_hooks = {
            "src/scenes/MainScene.cpp": ("ROOM_FEED", "ROOM_PET", "OPEN_MENU"),
            "src/scenes/MenuScene.cpp": ("MENU_NAV", "MENU_BACK"),
            "src/scenes/ExploreScene.cpp": (
                "EXPLORE_WALK",
                "EXPLORE_MENU",
                "BATTLE_ACTION",
            ),
        }
        for path, steps in expected_hooks.items():
            source = self.read(path)
            for step in steps:
                self.assertIn(f"Game::TutorialStep::{step}", source, path)

    def test_replay_resets_progress_and_returns_home(self):
        source = self.read("src/scenes/SettingsScene.cpp")
        replay = re.compile(
            r"resetTutorial\(\);\s*"
            r"GameEngine::ins\(\)\.requestScene\(SceneID::MAIN\);"
        )
        self.assertRegex(source, replay)

    def test_battle_help_stays_in_command_footer(self):
        source = self.read("src/scenes/ExploreScene.cpp")
        command_box = source[source.index("void ExploreScene::renderCommandBox()") :]
        command_box = command_box[: command_box.index("\n}") + 2]
        self.assertIn("Ui::Tutorial::BATTLE_ACTION", command_box)
        self.assertNotIn("TutorialOverlay::draw", command_box)


if __name__ == "__main__":
    unittest.main()
