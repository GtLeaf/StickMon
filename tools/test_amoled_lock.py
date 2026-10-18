#!/usr/bin/env python3

import unittest
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
MAIN_FILES = (
    ROOT / "firmware" / "amoled_1_8_v1" / "main" / "main.cpp",
    ROOT / "firmware" / "amoled_1_8_v2" / "main" / "main.cpp",
)


class AmoledLockTests(unittest.TestCase):
    def test_double_tap_gesture_behavior(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / "amoled-lock")
            subprocess.run([
                "c++", "-std=c++17", "-Ifirmware/amoled_1_8_v1/main",
                "tools/amoled_lock_host.cpp", "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], check=True)

    def test_both_boards_only_unlock_on_completed_pet_double_tap(self):
        for path in MAIN_FILES:
            with self.subTest(version=path.parents[1].name):
                source = path.read_text()
                start = source.index("            if (lockPhase != LockPhase::OPEN) {")
                end = source.index("\n#if STICKMON_ENABLE_DEBUG_FEATURES", start)
                touch = source[start:end]
                self.assertIn("lockPhase == LockPhase::LOCKED", touch)
                self.assertIn("app.lockTapHitsPet(event.x, event.y)", touch)
                self.assertIn("dx * dx + dy * dy <= radius * radius", touch)
                self.assertIn("lockDoubleTap.release(", touch)
                self.assertIn("if (unlock)", touch)
                wake = touch.index("app.onWake(nowMs)")
                self.assertGreater(wake, touch.index("if (unlock)"))
                self.assertNotIn("Display unlocked by touch", source)
                self.assertIn("app.prepareDisplayLock(nowMs)", source)
                start = source.index("// The snapshot must contain the room")
                snapshot = source.index("std::memcpy(lockSnapshot", start)
                self.assertIn("app.render(canvas)", source[start:snapshot])
                self.assertIn("lockDoubleTap.reset();", source)

    def test_only_home_pet_provides_a_lock_focus(self):
        source = APP.read_text()
        start = source.index("bool AmoledApp::lockFocusPoint(")
        end = source.index("\nvoid AmoledApp::", start + 1)
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
                self.assertIn("app.petSleepExpansion(nowMs)", source)
                self.assertIn("!preserveFocus || !sleeping", source)
                self.assertIn("expansion * LOCK_SLEEP_BREATH_AMPLITUDE", source)
                self.assertIn("app.petIsSleeping()", source)
                self.assertIn("lockSleeping != lastLockSleeping", source)

    def test_app_sleep_state_is_home_and_schedule_aware(self):
        source = APP.read_text()
        start = source.index("bool AmoledApp::petIsSleeping() const")
        end = source.index("\nvoid AmoledApp::", start + 1)
        sleep = source[start:end]
        self.assertIn("Scene::HOME", sleep)
        self.assertIn("gameState.teamCount == 0", sleep)
        self.assertIn("petIsResting()", sleep)
        self.assertNotIn("petScheduledSleeping", sleep)


if __name__ == "__main__":
    unittest.main()
