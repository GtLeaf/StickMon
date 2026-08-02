#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DoorDepartureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (
            ROOT / "src" / "scenes" / "MainScene.cpp"
        ).read_text(encoding="utf-8")

    def test_route_failures_fall_back_to_visible_door_crossing(self):
        begin = self.source.index("void MainScene::beginDoorTransition")
        update = self.source.index("void MainScene::updateDoorTransition")
        initial_route = self.source[begin:update]
        fallback = initial_route.index("[DoorDeparture] route fallback")
        self.assertNotIn(
            "finishDoorDeparture();",
            initial_route[fallback - 700:fallback + 300],
        )

        second = self.source.index("void MainScene::beginSecondDoorExit")
        finish = self.source.index("void MainScene::finishDoorDeparture", second)
        second_route = self.source[second:finish]
        self.assertIn("fallback actor=%u mode=cross", second_route)
        self.assertIn("DoorTransitionMode::EXIT_SECOND_CROSS", second_route)

    def test_stalled_routes_do_not_wait_for_full_route_timeout(self):
        self.assertIn("DOOR_STALL_TIMEOUT_MS = 700UL", self.source)
        self.assertIn("waiting route stalled fallback=wait_pose", self.source)
        self.assertIn("first route stalled fallback=cross", self.source)
        self.assertIn("second route stalled fallback=cross", self.source)


if __name__ == "__main__":
    unittest.main()
