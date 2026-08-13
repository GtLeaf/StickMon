#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MainSceneDualActorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (
            ROOT / "src" / "scenes" / "MainScene.cpp"
        ).read_text(encoding="utf-8")
        cls.header = (
            ROOT / "src" / "scenes" / "MainScene.h"
        ).read_text(encoding="utf-8")

    def test_visitor_uses_its_own_geometry(self):
        self.assertIn("ActorGeometry MainScene::visitorGeometry()", self.source)
        self.assertIn(
            "actorFootprintInsideWalkArea(\n            nextX, nextY, visitorGeometry())",
            self.source,
        )
        begin = self.source.index("bool MainScene::buildVisitorMoveRoute")
        end = self.source.index("bool MainScene::buildMoveRouteFrom", begin)
        self.assertIn("visitorGeometry()", self.source[begin:end])

    def test_routes_treat_the_other_actor_as_an_obstacle(self):
        self.assertIn("routeSegmentKeepsSpacing", self.source)
        self.assertIn("otherGroundOffsetY", self.source)
        self.assertIn("visitorNextReplanMs = nowMs + 350", self.source)
        self.assertIn("handleVisitorMoveBlocked", self.source)
        begin = self.source.index("bool MainScene::doorStepKeepsSpacing")
        end = self.source.index("float MainScene::doorActorMinSeparation", begin)
        self.assertIn("routeSegmentKeepsSpacing", self.source[begin:end])

    def test_final_waypoint_snap_also_preserves_actor_spacing(self):
        main_arrival = self.source.index("if (dist < 1.2f || step >= dist)")
        main_snap = self.source.index("monsterX = waypointX", main_arrival)
        self.assertIn(
            "doorStepKeepsSpacing",
            self.source[main_arrival:main_snap],
        )
        visitor_arrival = self.source.index(
            "if (distance <= step || distance < arriveDist)"
        )
        visitor_snap = self.source.index(
            "visitor.x = movementTargetX", visitor_arrival
        )
        self.assertIn(
            "doorStepKeepsSpacing",
            self.source[visitor_arrival:visitor_snap],
        )

    def test_aborted_food_route_releases_bowl_claim(self):
        begin = self.source.index("void MainScene::abortMovement")
        end = self.source.index("void MainScene::handleVisitorMoveBlocked", begin)
        self.assertIn("releaseBowl(0)", self.source[begin:end])
        begin = end
        end = self.source.index("bool MainScene::buildFoodApproachRoute", begin)
        self.assertIn("releaseBowl(1)", self.source[begin:end])

    def test_restored_sleep_and_walk_states_rebuild_routes(self):
        begin = self.source.index("bool MainScene::restoreVisitorViewState")
        end = self.source.index("void MainScene::persistVisitorViewState", begin)
        body = self.source[begin:end]
        self.assertIn("VisitorState::GO_TO_SLEEP", body)
        self.assertIn("VisitorState::YIELDING_BED", body)
        self.assertIn("buildVisitorMoveRoute", body)

    def test_transient_food_states_restore_as_idle(self):
        begin = self.source.index("bool MainScene::restoreVisitorViewState")
        end = self.source.index("void MainScene::persistVisitorViewState", begin)
        body = self.source[begin:end]
        self.assertIn("visitor.state == VisitorState::SEEK_FOOD", body)
        self.assertIn("visitor.state == VisitorState::FEEDING", body)
        self.assertIn("visitor.state = VisitorState::IDLE", body)

    def test_secondary_actor_is_part_of_persistent_view_state(self):
        view_header = (
            ROOT / "src" / "core" / "MainSceneViewState.h"
        ).read_text(encoding="utf-8")
        save_source = (
            ROOT / "src" / "core" / "SaveManager.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("SecondarySceneViewState secondary", view_header)
        self.assertIn("MAIN_SCENE_VIEW_VERSION = 2", save_source)
        self.assertIn("MainSceneViewRecordV1", save_source)
        self.assertIn("loadMainSceneViewRecordV1", save_source)
        self.assertIn("secondaryStateRemainingMs", save_source)
        for field in (
            "secondaryIvPacked",
            "secondaryMetAt",
            "secondaryNature",
            "secondaryMetArea",
            "secondaryOrigin",
        ):
            self.assertGreaterEqual(save_source.count(field), 3)
        self.assertIn("monsterIdentityFieldsValid", save_source)
        self.assertIn("SECONDARY_SCENE_STATE_MAX", save_source)
        self.assertIn("SECONDARY_SCENE_DIRECTION_MAX", save_source)
        self.assertNotIn("static VisitorActor savedVisitor", self.header)


if __name__ == "__main__":
    unittest.main()
