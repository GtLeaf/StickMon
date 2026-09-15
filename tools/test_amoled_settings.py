#!/usr/bin/env python3
"""Remaining source integration checks; pixel/hit contracts live in test_amoled_native_render."""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"


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


if __name__ == "__main__":
    unittest.main()
