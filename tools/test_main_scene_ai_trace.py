#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MainSceneAiTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src" / "scenes" / "MainScene.cpp").read_text(
            encoding="utf-8"
        )
        cls.header = (ROOT / "src" / "scenes" / "MainScene.h").read_text(
            encoding="utf-8"
        )

    def test_trace_is_debug_only_and_rate_limited(self):
        self.assertIn("#if STICKMON_ENABLE_TRACE_LOGS", self.source)
        self.assertIn("MAIN_AI_TRACE_INTERVAL_MS = 2000UL", self.source)
        self.assertIn("nextMainAiTraceMs", self.source)
        self.assertIn('"[MainAI] ', self.source)

    def test_trace_covers_the_main_blocking_gates(self):
        for gate in (
            "PROGRESSION",
            "CONTACT",
            "PAIR",
            "DOOR_TRANSITION",
            "STATIONARY_SPECIES",
            "FAINTED_OR_STATUS_SLEEP",
            "FEEDING",
            "WAKING",
            "TURNING",
            "RESTING",
            "ROOM_ACTION",
            "IDLE_WAIT",
            "ROUTE_FAILED",
            "BLOCKED_BY_SECONDARY",
            "OUTSIDE_WALK_AREA",
            "MOVING",
        ):
            self.assertIn(f"MainAiTraceGate::{gate}", self.source)
            self.assertIn(gate, self.header)

    def test_route_failures_are_forced_events(self):
        begin = self.source.index("void MainScene::beginMovement")
        end = self.source.index("void MainScene::beginTurn", begin)
        self.assertIn("MainAiTraceGate::ROUTE_FAILED, true", self.source[begin:end])

        begin = self.source.index("bool MainScene::startWander")
        end = self.source.index("bool MainScene::openPendingProgression", begin)
        self.assertIn("MainAiTraceGate::ROUTE_FAILED, true", self.source[begin:end])

    def test_wander_targets_apply_secondary_clearance_before_routing(self):
        begin = self.source.index("bool MainScene::startWander")
        end = self.source.index("bool MainScene::openPendingProgression", begin)
        body = self.source[begin:end]
        self.assertIn("mainTargetKeepsVisitorSpacing(candidateX, candidateY)", body)
        self.assertIn("bool MainScene::mainTargetKeepsVisitorSpacing", self.source)

    def test_food_approach_uses_reachable_pose_and_yields_when_blocked(self):
        begin = self.source.index("void MainScene::setFoodTarget")
        end = self.source.index("void MainScene::setBedTarget", begin)
        body = self.source[begin:end]
        self.assertIn("buildFoodApproachRoute(true", body)
        self.assertIn("startVisitorFoodYield(nowMs)", body)

        begin = self.source.index("bool MainScene::buildFoodApproachRoute")
        end = self.source.index("bool MainScene::monsterCanUseBedSleepPose", begin)
        body = self.source[begin:end]
        self.assertIn("buildMoveRoute(candidateX, candidateY)", body)
        self.assertIn("buildVisitorMoveRoute(candidateX, candidateY)", body)

        begin = self.source.index("bool MainScene::startVisitorFoodYield")
        end = self.source.index("void MainScene::enterVisitorFeeding", begin)
        body = self.source[begin:end]
        self.assertIn("visitor.state != VisitorState::IDLE", body)
        self.assertIn("visitor.state = VisitorState::WALK", body)
        self.assertIn("buildVisitorMoveRoute(candidateX, candidateY)", body)

    def test_failed_food_arbitration_does_not_freeze_main_actor(self):
        begin = self.source.index("void MainScene::setFoodTarget")
        end = self.source.index("void MainScene::setBedTarget", begin)
        self.assertIn("startMainFoodYield(nowMs)", self.source[begin:end])

        begin = self.source.index("void MainScene::enterFeeding")
        end = self.source.index("void MainScene::updateFeeding", begin)
        self.assertIn("startMainFoodYield(nowMs)", self.source[begin:end])

        begin = self.source.index("bool MainScene::startMainFoodYield")
        end = self.source.index("bool MainScene::startVisitorFoodSeek", begin)
        yield_body = self.source[begin:end]
        self.assertIn("foodDx * foodDx + foodDy * foodDy < clearanceSq", yield_body)
        self.assertIn("buildMoveRoute(candidateX, candidateY)", yield_body)
        self.assertIn("beginPreparedMovement(AiMode::WANDER", yield_body)

        begin = self.source.index("void MainScene::chooseAiGoal")
        end = self.source.index("bool MainScene::startWander", begin)
        body = self.source[begin:end]
        self.assertIn("aiMode != AiMode::IDLE ||", body)
        self.assertIn("visitor.state != VisitorState::IDLE", body)
        self.assertIn("if (startWander(nowMs)) return", body)

    def test_trace_separates_bowl_contents_and_owner(self):
        self.assertIn("bowl=%u eater=%d preferred=%d vhun=%d", self.source)

    def test_failed_visitor_food_routes_are_rate_limited(self):
        begin = self.source.index("bool MainScene::startVisitorFoodSeek")
        end = self.source.index("bool MainScene::startVisitorFoodYield", begin)
        body = self.source[begin:end]
        self.assertIn("visitorFoodRouteFailureStillValid", body)
        self.assertIn("rememberVisitorFoodRouteFailure", body)
        self.assertIn("mainYieldingForVisitorFood", body)
        self.assertIn("reason=route_failed", body)
        self.assertIn("retry=state_change", body)

    def test_food_route_search_is_bounded_to_navigation_scale(self):
        begin = self.source.index("bool MainScene::buildFoodApproachRoute")
        end = self.source.index("bool MainScene::monsterCanUseBedSleepPose", begin)
        body = self.source[begin:end]
        self.assertIn("uint16_t routeCandidates", body)
        self.assertIn("static constexpr int8_t CANDIDATES[][2]", body)
        self.assertIn("builds=%u", body)
        self.assertNotIn("radius <= 36", body)
        self.assertNotIn("radius += 2", body)

    def test_failed_route_cache_tracks_bowl_and_actor_grid_cells(self):
        self.assertIn("struct FoodRouteFailure", self.header)
        begin = self.source.index(
            "bool MainScene::visitorFoodRouteFailureStillValid"
        )
        end = self.source.index("bool MainScene::startVisitorFoodYield", begin)
        body = self.source[begin:end]
        for marker in (
            "bowlFoodIndex()",
            "bowlFoodCount()",
            "bowlFoodBitesRemaining()",
            "mainCellX",
            "mainCellY",
            "visitorCellX",
            "visitorCellY",
        ):
            self.assertIn(marker, body)


if __name__ == "__main__":
    unittest.main()
