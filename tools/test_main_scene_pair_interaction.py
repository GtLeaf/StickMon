#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MainScenePairInteractionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src" / "scenes" / "MainScene.cpp").read_text(
            encoding="utf-8"
        )
        cls.header = (ROOT / "src" / "scenes" / "MainScene.h").read_text(
            encoding="utf-8"
        )

    def test_interaction_pool_only_contains_talk_and_chase(self):
        begin = self.header.index("enum class PairInteraction")
        end = self.header.index("enum class PairInteractionPhase", begin)
        enum_body = self.header[begin:end]
        self.assertIn("TALK", enum_body)
        self.assertIn("CHASE", enum_body)
        self.assertNotIn("GREET", enum_body)
        self.assertNotIn("FOLLOW", enum_body)

    def test_talk_hops_are_sequential_and_hearts_start_afterward(self):
        self.assertIn("PAIR_TALK_HOP_MS * 2 + PAIR_TALK_GAP_MS", self.source)
        self.assertIn("pairConversationHopOffset(true, nowMs)", self.source)
        self.assertIn(
            "pairConversationHopOffset(false, Hal::ins().millis())",
            self.source,
        )
        begin = self.source.index("case PairInteractionPhase::CONVERSATION")
        end = self.source.index("case PairInteractionPhase::ACTIVE", begin)
        body = self.source[begin:end]
        self.assertIn("facePairActors()", body)
        self.assertIn("beginPairCelebrate(nowMs)", body)

    def test_chase_follower_route_uses_dynamic_spacing(self):
        begin = self.source.index("case PairInteractionPhase::ACTIVE")
        end = self.source.index("case PairInteractionPhase::SETTLE", begin)
        body = self.source[begin:end]
        self.assertIn(
            "followerMain, pairTrailX, pairTrailY, false", body
        )
        self.assertIn("beginPairChaseLeg(nowMs)", body)
        self.assertIn("pairNextRouteAttemptMs = nowMs + 250", body)

    def test_pair_approach_pose_is_reachable_before_selection(self):
        begin = self.source.index("bool MainScene::choosePairApproachPose")
        end = self.source.index("bool MainScene::choosePairLeaderGoal", begin)
        body = self.source[begin:end]
        self.assertIn("actorPathSegmentInsideWalkArea", body)
        self.assertIn("routeSegmentKeepsSpacing", body)

    def test_chase_has_a_longer_timeout_and_trace_events(self):
        self.assertIn("PAIR_TALK_TIMEOUT_MS = 9000UL", self.source)
        self.assertIn("PAIR_CHASE_TIMEOUT_MS = 24000UL", self.source)
        self.assertIn('"[PairAI] event=selected', self.source)
        self.assertIn('"[PairAI] event=chase_leg', self.source)
        self.assertIn('"[PairAI] event=chase_route_failed', self.source)
        self.assertIn('"[PairAI] event=finish', self.source)

    def test_debug_pair_request_forces_chase(self):
        self.assertIn("pairForcedChase =", self.source)
        self.assertIn("consumeDebugPairInteractionRequest()", self.source)
        self.assertIn("else if (forcedChase)", self.source)
        self.assertIn("pairInteraction = PairInteraction::CHASE", self.source)


if __name__ == "__main__":
    unittest.main()
