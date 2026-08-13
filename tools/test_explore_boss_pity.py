#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ExploreBossPityTests(unittest.TestCase):
    def test_probability_curve_and_resets(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "explore_boss_pity_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Isrc",
                    "tools/explore_boss_pity_host.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(executable)], check=True)

    def test_explore_scene_uses_pity_lifecycle(self):
        source = (ROOT / "src" / "scenes" / "ExploreScene.cpp").read_text()
        self.assertIn("ExploreBossPity::chanceForMisses", source)
        self.assertIn("ExploreBossPity::requiresGuaranteedEligibleRun", source)
        self.assertIn("ExploreBossPity::increment", source)
        self.assertIn("ExploreBossPity::resetArea", source)
        self.assertIn("naturalRunCompletionPending", source)
        self.assertIn("currentSlot == expeditionPitySlotIndex", source)

    def test_friendship_confirmation_preserves_visible_battle_logs(self):
        source = (ROOT / "src" / "scenes" / "ExploreScene.cpp").read_text()
        keep_comment = source.index(
            "Keep the two-line context visible while the player answers"
        )
        clear_visible = source.index("battleLogVisibleCount = 0", keep_comment)
        contact_confirm = source.index(
            "case FriendshipStep::CONTACT_CONFIRM:", keep_comment
        )
        preserve_return = source.index("return playbackEnded;", contact_confirm)
        self.assertLess(preserve_return, clear_visible)


if __name__ == "__main__":
    unittest.main()
