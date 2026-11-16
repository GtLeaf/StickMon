#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class AmoledExpeditionDepartureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.cpp").read_text(
            encoding="utf-8"
        )
        cls.home = (ROOT / "firmware/amoled_1_8_v1/main/HomeScreen.cpp").read_text(
            encoding="utf-8"
        )
        cls.header = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.h").read_text(
            encoding="utf-8"
        )
        cls.v1_main = (
            ROOT / "firmware/amoled_1_8_v1/main/main.cpp"
        ).read_text(encoding="utf-8")
        cls.v2_main = (
            ROOT / "firmware/amoled_1_8_v2/main/main.cpp"
        ).read_text(encoding="utf-8")

    def test_scene_transition_blocks_display_lock_on_both_boards(self):
        self.assertIn("bool displayLockAllowed() const;", self.header)
        for main in (self.v1_main, self.v2_main):
            lock_start = main.index(
                "if (lockPhase == LockPhase::OPEN && lockRequest"
            )
            lock_body = main[lock_start : lock_start + 240]
            self.assertIn("app.displayLockAllowed()", lock_body)

    def test_final_route_uses_exit_motion_before_completion(self):
        presenter = (ROOT / "firmware/amoled_1_8_v1/main/app/ExplorePresenter.cpp").read_text()
        self.assertIn("EXPLORE_ROUTE_EXIT_MARGIN", presenter)
        self.assertIn("explorePresenter_.beginExit(", self.app)
        begin = presenter.index("void ExplorePresenter::beginExit")
        end = presenter.index("bool ExplorePresenter::advanceWalkFrames", begin)
        body = presenter[begin:end]
        self.assertIn("exploreRouteTargetX = EXPLORE_ROUTE_WORLD_WIDTH + EXPLORE_ROUTE_EXIT_MARGIN", body)
        self.assertIn("exploreRouteTargetY = EXPLORE_ROUTE_WORLD_HEIGHT + EXPLORE_ROUTE_EXIT_MARGIN", body)
        update = presenter.index("ExplorePresenter::Motion ExplorePresenter::updateMotion")
        finish = presenter.index("return Motion::EXIT_FINISHED;", update)
        self.assertIn("if (route.exploreRouteExiting)", presenter[update:finish])
        self.assertIn("advanceWalkFrames(route, nowMs);", presenter[update:finish])
        app_update = self.app.index("void AmoledApp::updateExploreRoute")
        finish = self.app.index("exploreSession_.finishExit(gameState);", app_update)
        self.assertIn("motion == ExplorePresenter::Motion::EXIT_FINISHED", self.app[app_update:finish])


if __name__ == "__main__":
    unittest.main()
