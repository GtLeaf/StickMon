#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ExploreEndBondTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.explore = (ROOT / "src/scenes/ExploreScene.cpp").read_text(
            encoding="utf-8"
        )
        cls.strings = (ROOT / "src/core/UiStrings.h").read_text(
            encoding="utf-8"
        )

    def test_every_visible_end_prompt_settles_bond_first(self):
        begin = self.explore.index("void ExploreScene::beginExploreEnding()")
        settle = self.explore.index("settleAdventureBond();", begin)
        ending = self.explore.index("phase = Phase::ENDING;", begin)
        self.assertLess(settle, ending)
        self.assertEqual(self.explore.count("phase = Phase::ENDING;"), 1)
        self.assertGreaterEqual(self.explore.count("beginExploreEnding();"), 2)

    def test_end_prompt_names_the_bond_value(self):
        self.assertIn('BOND_GAIN_FMT = "羁绊值 +%u"', self.strings)


class MoveLearnComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src/core/ProgressionUi.cpp").read_text(
            encoding="utf-8"
        )

    def test_selecting_pending_move_can_decline_it_after_confirmation(self):
        self.assertIn(
            "MOVE_LEARN_NEW_SLOT = Game::MOVE_SLOT_COUNT", self.source
        )
        self.assertIn("moveSlot <= MOVE_LEARN_NEW_SLOT", self.source)
        self.assertIn("state.confirmOpen = true", self.source)
        self.assertIn(
            "state.replacementSlot == MOVE_LEARN_NEW_SLOT", self.source
        )
        self.assertIn("engine.resolvePendingMoveLearn(false)", self.source)
        self.assertIn(
            "bool showingNewMove = uiState.replacementSlot == "
            "MOVE_LEARN_NEW_SLOT",
            self.source,
        )
        self.assertIn("? newMove", self.source)
        self.assertIn("Ui::Team::MOVE_UNLEARNED", self.source)
        self.assertIn("Ui::Explore::LEARN_GIVE_UP_FMT", self.source)


if __name__ == "__main__":
    unittest.main()
