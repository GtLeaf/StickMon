import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.cpp"
SCREEN = ROOT / "firmware/amoled_1_8_v1/main/HomeScreen.cpp"


class AmoledProgressionTests(unittest.TestCase):
    def test_completion_does_not_show_growth_toast(self):
        body = APP.read_text(encoding="utf-8").split(
            "void AmoledApp::completeProgression(uint32_t nowMs) {", 1
        )[1].split("void AmoledApp::openShowerScene", 1)[0]
        self.assertNotIn("GROWTH_COMPLETE", body)

    def test_replacement_list_reuses_move_detail_and_requires_confirmation(self):
        source = SCREEN.read_text(encoding="utf-8")
        body = source.split(
            "if (model.mode == ProgressionViewModel::Mode::MOVE_REPLACE) {", 1
        )[1].split("UiCommon::drawHeader(canvas", 1)[0]
        self.assertIn("Ui::Amoled::MOVE_LIST", body)
        self.assertIn("drawMoveDetailContent", body)
        self.assertIn("model.selectedItem < 3", body)
        self.assertNotIn("GIVE_UP_MOVE", body)
        hit = source.split("int progressionReplaceItemAt(", 1)[1].split(
            "void drawLevelUpDialog", 1
        )[0]
        self.assertIn("index > selectedItem ? push : 0", hit)
        self.assertIn("return index", hit)

    def test_confirming_new_move_skips_and_replacement_resets_proficiency(self):
        source = APP.read_text(encoding="utf-8")
        body = re.search(
            r"if \(progressionMode == ProgressionViewModel::Mode::MOVE_REPLACE\) \{"
            r"(.*?)\n        if \(progressionMode == ProgressionViewModel::Mode::EVOLUTION\)",
            source, re.S,
        )
        self.assertIsNotNone(body)
        action = body.group(1)
        self.assertIn("if (progressionSelectedItem >= 3) return", action)
        self.assertIn("progressionSelectedItem == 0", action)
        self.assertIn("Mode::MOVE_LEARN", action)
        self.assertIn("monster.moveProficiency[1] = 0", action)
        self.assertIn("monster.moveProficiency[2] = 0", action)
        self.assertLess(action.index("advanceProgression(nowMs)"),
                        action.index("progressionReplaceItemAt("))

    def test_level_up_entrance_and_gesture_gate(self):
        app = APP.read_text(encoding="utf-8")
        screen = SCREEN.read_text(encoding="utf-8")
        dialog = screen.split("void drawLevelUpDialog", 1)[1].split(
            "void drawLevelUpScene", 1)[0]
        self.assertIn("LEVEL_UP_DIALOG_FMT", dialog)
        self.assertNotIn("Ui::Amoled::CONTINUE", dialog)
        self.assertIn("model.oldLevel", screen)
        self.assertIn("model.levelUpElapsedMs", screen)
        self.assertIn("progressionLevelUpTouchAllowed", app)
        self.assertIn("event.timestampMs - progressionLevelUpStartedMs >=", app)
        self.assertIn("!progressionLevelUpTouchAllowed", app)
        self.assertIn("nowMs - progressionLevelUpStartedMs < LEVEL_UP_ANIMATION_MS", app)


if __name__ == "__main__":
    unittest.main()
