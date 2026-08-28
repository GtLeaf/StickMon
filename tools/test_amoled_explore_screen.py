#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"


class AmoledExploreScreenTests(unittest.TestCase):
    def test_background_continues_behind_scene_selector(self):
        source = HOME_SCREEN.read_text()
        start = source.index("void renderExploreScreen(")
        end = source.index("bool exploreRouteBackAt(", start)
        render = source[start:end]

        self.assertIn("if (rowBegin < HEADER_HEIGHT)", render)
        self.assertIn("canvas.width(), HEADER_HEIGHT", render)
        self.assertIn("PixelRenderer::fillRectAlpha(", render)
        self.assertNotIn(
            "canvas.fillRect(0, HEADER_HEIGHT, leftWidth - 1", render
        )


if __name__ == "__main__":
    unittest.main()
