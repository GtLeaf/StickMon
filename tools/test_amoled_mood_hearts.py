#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
APP_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h"
SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
SCREEN_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.h"


class AmoledMoodHeartTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = APP.read_text(encoding="utf-8")
        cls.app_header = APP_HEADER.read_text(encoding="utf-8")
        cls.screen = SCREEN.read_text(encoding="utf-8")
        cls.screen_header = SCREEN_HEADER.read_text(encoding="utf-8")

    def test_mood_maps_to_five_twenty_point_hearts(self):
        start = self.app.index("uint8_t moodHeartCountFor(")
        end = self.app.index("void AmoledApp::begin(", start)
        mapping = self.app[start:end]
        self.assertIn("mood / 20", mapping)
        self.assertIn("std::min<uint8_t>(5", mapping)

    def test_home_replaces_title_with_mood_hearts(self):
        start = self.screen.index("void renderHomeScreen(")
        end = self.screen.index("int mainMenuItemAt(", start)
        home = self.screen[start:end]
        self.assertIn("MOOD_HEART_START_X", home)
        self.assertIn("model.moodHearts", home)
        self.assertNotIn('"STICKMON"', home)

    def test_heart_loss_starts_localized_burst_and_requests_header_frames(self):
        start = self.app.index("void AmoledApp::updateMoodHearts(")
        end = self.app.index("void AmoledApp::updatePet(", start)
        update = self.app[start:end]
        self.assertIn("nextHeartCount < moodHeartCount", update)
        self.assertIn("moodBurstStartedMs = nowMs", update)
        self.assertIn("requestRenderRows(0, HOME_HEADER_HEIGHT)", update)

        burst_start = self.screen.index("void drawHeartBurst(")
        burst_end = self.screen.index("void drawPlant(", burst_start)
        burst = self.screen[burst_start:burst_end]
        self.assertGreaterEqual(burst.count("canvas.drawLine"), 4)

    def test_burst_state_is_part_of_home_view_model(self):
        self.assertIn("uint8_t moodHearts = 0;", self.screen_header)
        self.assertIn("uint8_t moodBurstHeart = 0xFF;", self.screen_header)
        self.assertIn("void updateMoodHearts(uint32_t nowMs);", self.app_header)


if __name__ == "__main__":
    unittest.main()
