#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
MAIN_FILES = (
    ROOT / "firmware" / "amoled_1_8_v1" / "main" / "main.cpp",
    ROOT / "firmware" / "amoled_1_8_v2" / "main" / "main.cpp",
)


class AmoledLockTests(unittest.TestCase):
    def test_only_home_pet_provides_a_lock_focus(self):
        source = APP.read_text()
        start = source.index("bool AmoledApp::lockFocusPoint(")
        end = source.index("void AmoledApp::setSettingsSliderValue(", start)
        focus = source[start:end]

        self.assertIn("Scene::HOME", focus)
        self.assertIn("gameState.teamCount > 0", focus)
        self.assertIn("return true;", focus)
        self.assertIn("return false;", focus)

    def test_no_focus_locks_to_black_and_stops_redrawing(self):
        for path in MAIN_FILES:
            with self.subTest(version=path.parents[1].name):
                source = path.read_text()
                self.assertIn(
                    "int finalRadius = preserveFocus ? LOCK_FINAL_RADIUS : 0;",
                    source,
                )
                self.assertIn(
                    "lockPhase == LockPhase::LOCKED && !lockHasFocus;", source
                )
                self.assertIn(
                    "(!lockedWithoutFocus && app.needsRender())", source
                )

    def test_sleeping_lock_breathes_only_when_pet_has_focus(self):
        for path in MAIN_FILES:
            with self.subTest(version=path.parents[1].name):
                source = path.read_text()
                self.assertIn("LOCK_SLEEP_BREATH_AMPLITUDE", source)
                self.assertIn("LOCK_SLEEP_BREATH_PERIOD_MS", source)
                self.assertIn("!preserveFocus || !sleeping", source)
                self.assertIn("std::cos(cycle * 6.2831853f)", source)
                self.assertIn("app.petIsSleeping()", source)
                self.assertIn("lockSleeping != lastLockSleeping", source)

    def test_app_sleep_state_is_home_and_schedule_aware(self):
        source = APP.read_text()
        start = source.index("bool AmoledApp::petIsSleeping() const")
        end = source.index("void AmoledApp::setSettingsSliderValue(", start)
        sleep = source[start:end]
        self.assertIn("Scene::HOME", sleep)
        self.assertIn("gameState.teamCount == 0", sleep)
        self.assertIn("petResting", sleep)
        self.assertIn("Game::isSleepCareTime", sleep)


if __name__ == "__main__":
    unittest.main()
