#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"


class AmoledSettingsTests(unittest.TestCase):
    def test_saved_brightness_is_applied_during_startup(self):
        source = AMOLED_APP.read_text()
        begin_start = source.index("void AmoledApp::begin(")
        begin_end = source.index("void AmoledApp::handleTouch(", begin_start)
        begin = source[begin_start:begin_end]

        load = begin.index("saveManager.load(")
        brightness = begin.index(
            "Platform::display().setBrightness(gameState.settings.brightness);"
        )
        first_render = begin.index("requestFullRender();")
        self.assertLess(load, brightness)
        self.assertLess(brightness, first_render)

    def test_brightness_and_volume_use_thick_sliders_without_values(self):
        source = HOME_SCREEN.read_text()
        start = source.index("void drawSettingsSlider(")
        end = source.index("int progressionItemAt(", start)
        settings = source[start:end]

        self.assertIn("trackWidth, 8, 4, track", settings)
        self.assertIn("pressed ? 7 : 6", settings)
        self.assertNotIn('"%u", model.brightness', settings)
        self.assertNotIn('"%u%%", model.volume', settings)


if __name__ == "__main__":
    unittest.main()
