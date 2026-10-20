import unittest
from pathlib import Path

from amoled_page_cases import run_page_case


ROOT = Path(__file__).resolve().parents[1]
SCREEN = ROOT / "firmware/amoled_1_8_v1/main/HomeScreen.cpp"


class AmoledProgressionTests(unittest.TestCase):
    def test_skip_keeps_both_moves_and_proficiency(self):
        run_page_case("progression-skip")

    def test_replace_second_move_resets_only_its_proficiency(self):
        run_page_case("progression-replace-2")

    def test_replace_third_move_resets_only_its_proficiency(self):
        run_page_case("progression-replace-3")

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


if __name__ == "__main__":
    unittest.main()
