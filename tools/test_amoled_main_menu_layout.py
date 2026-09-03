#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"


class AmoledMainMenuLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = HOME_SCREEN.read_text(encoding="utf-8")

    def test_menu_uses_two_column_cells(self):
        self.assertIn("constexpr int MENU_CELL_WIDTH = 82;", self.source)
        self.assertIn("constexpr int MENU_CELL_HEIGHT = 76;", self.source)
        self.assertIn("int rowCount = (MAIN_MENU_ITEM_COUNT + 1) / 2;", self.source)
        self.assertIn("int index = row * 2 + column;", self.source)

    def test_menu_hit_testing_matches_grid_geometry(self):
        start = self.source.index("int mainMenuItemAt(")
        end = self.source.index("void renderMainMenu(", start)
        hit_test = self.source[start:end]
        self.assertIn("int column = localX / (MENU_CELL_WIDTH + MENU_GRID_GAP);", hit_test)
        self.assertIn("if (column >= 2 || cellX >= MENU_CELL_WIDTH)", hit_test)
        self.assertIn("return index < MAIN_MENU_ITEM_COUNT ? index : -1;", hit_test)

    def test_menu_renders_icon_above_centered_label(self):
        start = self.source.index("void renderMainMenu(")
        end = self.source.index("bool exploreBackAt(", start)
        render = self.source[start:end]
        self.assertIn("canvas.drawRgb565Rle(", render)
        self.assertIn("y + 7, MenuAssets::FRAME_W, MenuAssets::FRAME_H", render)
        self.assertIn("y + 54, entry.shortLabel", render)
        self.assertNotIn("canvas.drawLine(163", render)

    def test_menu_has_no_header_or_scrollbar(self):
        start = self.source.index("void renderMainMenu(")
        end = self.source.index("#if STICKMON_ENABLE_DEBUG_FEATURES", start)
        render = self.source[start:end]
        self.assertIn("MAIN_MENU_CONTENT_TOP", render)
        self.assertNotIn("drawBackIcon(canvas)", render)
        self.assertNotIn("canvas.fillRoundRect(180", render)


if __name__ == "__main__":
    unittest.main()
