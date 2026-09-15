#!/usr/bin/env python3
"""Remaining source integration checks; pixel/hit contracts live in test_amoled_native_render."""

import unittest
from pathlib import Path

from amoled_source import read_home_source


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"


class AmoledMainMenuLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = read_home_source(ROOT)

    def test_menu_renders_icon_above_centered_label(self):
        start = self.source.index("void renderMainMenu(")
        end = self.source.index("bool exploreRouteBackAt(", start)
        render = self.source[start:end]
        self.assertIn("PixelRenderer::drawRgb565RleScaled(", render)
        self.assertIn("y + 14, MenuAssets::FRAME_W, MenuAssets::FRAME_H", render)
        self.assertIn("y + 108, entry.shortLabel", render)
        self.assertNotIn("canvas.drawLine(163", render)


if __name__ == "__main__":
    unittest.main()
