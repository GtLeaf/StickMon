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

    def test_pair_departure_has_serial_companion_phases(self):
        self.assertIn("WALK_COMPANION_TO_DOOR", self.header)
        self.assertIn("CROSS_COMPANION_DOOR", self.header)
        begin = self.app.index("bool AmoledApp::updateExploreDeparture")
        end = self.app.index("void AmoledApp::beginExploreReturn", begin)
        body = self.app[begin:end]
        self.assertIn("expeditionCompanionDeparting", body)
        self.assertIn("ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR", body)
        self.assertIn("ExpeditionDeparturePhase::CROSS_COMPANION_DOOR", body)
        self.assertIn(
            "if (expeditionCompanionDeparting) {",
            body,
        )
        self.assertIn(
            "expeditionDeparturePhase = ExpeditionDeparturePhase::WALK_COMPANION_TO_DOOR",
            body,
        )
        self.assertIn(
            "expeditionDeparturePhase = ExpeditionDeparturePhase::CROSS_COMPANION_DOOR",
            body,
        )
        self.assertIn(
            "expeditionDeparturePhase = ExpeditionDeparturePhase::FADE_OUT",
            body,
        )

    def test_actors_hide_at_outside_door_before_fade(self):
        begin = self.app.index("bool AmoledApp::updateExploreDeparture")
        end = self.app.index("void AmoledApp::beginExploreReturn", begin)
        body = self.app[begin:end]
        self.assertIn("expeditionMainHidden = true;", body)
        self.assertIn("expeditionCompanionHidden = true;", body)
        main_hide = body.index("expeditionMainHidden = true;")
        companion_hide = body.index("expeditionCompanionHidden = true;")
        self.assertIn(
            "petMotion = PetMotion::IDLE;",
            body[main_hide:main_hide + 240],
        )
        self.assertIn(
            "expeditionCompanionDeparting = false;",
            body[companion_hide:companion_hide + 240],
        )
        self.assertIn(
            "!homeSnapshot.actors[0].hidden && !expeditionMainHidden",
            self.app,
        )
        self.assertIn(
            "!homeSnapshot.actors[1].hidden &&\n            !expeditionCompanionHidden",
            self.app,
        )

    def test_companion_render_visibility_is_independent(self):
        begin = self.home.index("void drawHomeCompanion(")
        end = self.home.index("void drawHomeHpBar(", begin)
        body = self.home[begin:end]
        self.assertIn(
            "companion.petVisible = true;",
            body,
            "the companion must remain drawable after the leader crosses the door",
        )

    def test_final_route_uses_exit_motion_before_completion(self):
        self.assertIn("EXPLORE_ROUTE_EXIT_MARGIN", self.app)
        self.assertIn("void AmoledApp::beginExploreRouteExit", self.app)
        begin = self.app.index("void AmoledApp::beginExploreRouteExit")
        end = self.app.index("void AmoledApp::updateExploreRoute", begin)
        body = self.app[begin:end]
        self.assertIn("exploreRouteTargetX = EXPLORE_ROUTE_WORLD_WIDTH + EXPLORE_ROUTE_EXIT_MARGIN", body)
        self.assertIn("exploreRouteTargetY = EXPLORE_ROUTE_WORLD_HEIGHT + EXPLORE_ROUTE_EXIT_MARGIN", body)
        update = self.app.index("void AmoledApp::updateExploreRoute")
        finish = self.app.index("finishExploreRouteAtEnd(nowMs);", update)
        self.assertIn("if (exploreRouteExiting)", self.app[update:finish])
        self.assertIn("advanceExploreRouteWalkFrames(nowMs);",
                      self.app[update:finish])

    def test_departure_keeps_render_actor_in_sync_with_pet_coordinates(self):
        begin = self.app.index("bool AmoledApp::updateExploreDeparture")
        end = self.app.index("void AmoledApp::beginExploreReturn", begin)
        body = self.app[begin:end]
        self.assertIn("homeMainActor.x = petX;", body)
        self.assertIn("homeMainActor.y = petY;", body)
        self.assertIn("homeRuntime.transition(0, Home::Task::DOOR_ACTION", body)

    def test_return_walks_both_actors_into_the_room(self):
        self.assertIn("RETURN_CLEAR_MAIN", self.header)
        self.assertIn("RETURN_COMPANION_WALK_IN", self.header)
        begin = self.app.index("bool AmoledApp::updateExploreDeparture")
        end = self.app.index("void AmoledApp::beginExploreReturn", begin)
        body = self.app[begin:end]
        self.assertIn("homeCompanionActor.x = outsideX;", body)
        self.assertIn(
            "homeCompanionActor.targetX = expeditionCompanionDoorInsideX;",
            body,
        )
        self.assertIn("chooseInsidePose(homeMainActor,", body)
        self.assertIn("chooseInsidePose(homeCompanionActor,", body)
        self.assertIn("expeditionCompanionHidden = hasCompanion;", body)
        leader_cross = body.index("ExpeditionDeparturePhase::RETURN_WALK_IN")
        leader_clear = body.index("ExpeditionDeparturePhase::RETURN_CLEAR_MAIN")
        companion_cross = body.index(
            "ExpeditionDeparturePhase::RETURN_COMPANION_WALK_IN"
        )
        self.assertLess(leader_cross, leader_clear)
        self.assertLess(leader_clear, companion_cross)
        self.assertIn("expeditionCompanionHidden = false;", body)
        self.assertIn("stopCompanion(nowMs, 700);", body)
        self.assertIn("expeditionCompanionDeparting = false;", body)


if __name__ == "__main__":
    unittest.main()
