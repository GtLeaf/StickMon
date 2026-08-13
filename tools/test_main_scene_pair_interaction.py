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
        cls.engine_source = (ROOT / "src" / "core" / "GameEngine.cpp").read_text(
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

    def test_chase_consumes_preplanned_routes_without_active_pathfinding(self):
        begin = self.source.index("case PairInteractionPhase::ACTIVE")
        end = self.source.index("case PairInteractionPhase::SETTLE", begin)
        body = self.source[begin:end]
        self.assertNotIn("buildPairActorRoute", body)
        self.assertNotIn("buildMoveRouteFrom", body)
        self.assertNotIn("RoomNavigator", body)
        self.assertIn(
            "pairLeaderMain, leaderSpeed, dtSeconds, nowMs, false, true",
            body,
        )
        self.assertIn(
            "followerMain, followerSpeed, dtSeconds, nowMs, false, true",
            body,
        )
        self.assertIn("PairMovementProfile::CHASE", body)

        begin = self.source.index("bool MainScene::buildPairChasePlan")
        end = self.source.index("void MainScene::degradePairChaseToTalk", begin)
        planner = self.source[begin:end]
        self.assertIn("Home::Route leaderRoute", planner)
        self.assertIn("Home::Route followerRoute", planner)
        self.assertIn("follower.route = followerRoute", planner)
        self.assertIn("leader.route = leaderRoute", planner)
        self.assertIn("PAIR_CHASE_MIN_POINT_COUNT", planner)

    def test_chase_compact_geometry_is_used_for_planning_and_runtime(self):
        begin = self.source.index("bool MainScene::buildPairActorRoute")
        end = self.source.index("bool MainScene::buildPairChasePlan", begin)
        planner = self.source[begin:end]
        self.assertIn("pairActorGeometry(mainActor, profile)", planner)
        self.assertIn("allowOutsideStart, movementGeometry, false", planner)

        begin = self.source.index("bool MainScene::updatePairActorRoute")
        end = self.source.index("void MainScene::facePairActors", begin)
        runtime = self.source[begin:end]
        self.assertIn("pairActorGeometry(mainActor, profile)", runtime)
        self.assertIn("keepSpacing, &movementGeometry", runtime)

        begin = self.source.index("case PairInteractionPhase::ACTIVE")
        end = self.source.index("case PairInteractionPhase::SETTLE", begin)
        active = self.source[begin:end]
        self.assertNotIn("nowMs, true, true", active)
        self.assertGreaterEqual(active.count("PairMovementProfile::CHASE"), 2)

        begin = self.source.index("void MainScene::beginPairSettle")
        end = self.source.index("void MainScene::completePairSettle", begin)
        settle = self.source[begin:end]
        self.assertIn("restorePairActorToStandardArea(true);", settle)
        self.assertNotIn("restorePairActorToStandardArea(true, true);", settle)

        begin = self.source.index("void MainScene::finishPairInteraction")
        end = self.source.index("void MainScene::cancelPairInteraction", begin)
        finish = self.source[begin:end]
        self.assertIn("restorePairActorToStandardArea(true, true);", finish)

    def test_pair_route_replan_is_transactional(self):
        begin = self.source.index("bool MainScene::buildPairActorRoute")
        end = self.source.index("bool MainScene::buildPairChasePlan", begin)
        body = self.source[begin:end]
        self.assertIn("Home::Route proposed", body)
        self.assertIn("this->mainActor.route = proposed", body)
        self.assertIn("visitor.route = proposed", body)

    def test_pair_visitor_renders_walk_frames_while_routed(self):
        begin = self.source.index(
            "const PokemonSprites::SpriteFrame* MainScene::visitorCurrentFrame")
        end = self.source.index("void MainScene::drawVisitor", begin)
        body = self.source[begin:end]
        self.assertIn("visitor.task == VisitorState::PAIR_ACTION", body)
        self.assertIn("visitor.route.index < visitor.route.count", body)

    def test_pair_approach_pose_is_reachable_before_selection(self):
        begin = self.source.index("bool MainScene::choosePairApproachPose")
        end = self.source.index("bool MainScene::buildPairActorRoute", begin)
        body = self.source[begin:end]
        self.assertIn("actorPathSegmentInsideWalkArea", body)
        self.assertIn("routeSegmentKeepsSpacing", body)

    def test_chase_has_a_longer_timeout_and_trace_events(self):
        self.assertIn("PAIR_TALK_TIMEOUT_MS = 9000UL", self.source)
        self.assertIn("PAIR_CHASE_TIMEOUT_MS = 24000UL", self.source)
        self.assertIn('"[PairAI] event=selected', self.source)
        self.assertIn('"[PairAI] event=chase_plan', self.source)
        self.assertIn('"[PairAI] event=chase_plan_failed', self.source)
        self.assertIn('"[PairAI] event=finish', self.source)
        self.assertIn('"[PairAI] event=cancel', self.source)

    def test_conversations_use_a_separate_social_gap(self):
        self.assertIn("PAIR_CONVERSATION_EXTRA_GAP = 28.0f", self.source)
        self.assertIn("58.0f, 66.0f", self.source)

        begin = self.source.index("bool MainScene::choosePairApproachPose")
        end = self.source.index("bool MainScene::buildPairActorRoute", begin)
        approach = self.source[begin:end]
        self.assertIn("float desired = pairConversationSeparation()", approach)

        begin = self.source.index("bool MainScene::beginContactMeetingArrangement")
        end = self.source.index("bool MainScene::beginContactGuestApproach", begin)
        arrival = self.source[begin:end]
        self.assertIn("conversationGap = pairConversationSeparation()", arrival)
        self.assertIn("guestX, guestY, guestGeometry, conversationGap", arrival)

    def test_contact_stay_timer_starts_after_arrival_dialog(self):
        self.assertIn("contactVisit.stayTimerStarted = false", self.engine_source)
        self.assertIn("void GameEngine::beginContactVisitStay", self.engine_source)
        timed_out = self.engine_source[
            self.engine_source.index("bool GameEngine::contactVisitTimedOut") :
            self.engine_source.index("void GameEngine::requestContactVisitFarewell")
        ]
        self.assertIn("contactVisit.stayTimerStarted", timed_out)

        begin = self.source.index("bool MainScene::handleContactDialogButton")
        end = self.source.index("void MainScene::drawContactDialog", begin)
        dialogs = self.source[begin:end]
        self.assertGreaterEqual(dialogs.count("engine.beginContactVisitStay(nowMs)"), 2)

        begin = self.source.index("void MainScene::updateContactVisit")
        end = self.source.index("void MainScene::beginContactGuestEntry", begin)
        visit_update = self.source[begin:end]
        self.assertIn(
            "pairInteraction == PairInteraction::NONE &&",
            visit_update,
        )

    def test_debug_pair_request_forces_chase(self):
        self.assertIn("pairForcedChase =", self.source)
        self.assertIn("consumeDebugPairInteractionRequest()", self.source)
        self.assertIn("else if (forcedChase)", self.source)
        self.assertIn("pairInteraction = PairInteraction::CHASE", self.source)

    def test_contact_arrival_uses_host_door_guest_and_talk_phases(self):
        enum_begin = self.header.index("enum class ContactGuestMotion")
        enum_end = self.header.index("enum class PairInteraction", enum_begin)
        enum_body = self.header[enum_begin:enum_end]
        for phase in (
            "HOST_TO_DOOR",
            "HOST_OPEN_DOOR",
            "HOST_CLEAR_DOOR",
            "ENTER_CROSS",
            "HOST_TO_MEET",
            "GUEST_TO_MEET",
            "ARRIVAL_TALK",
        ):
            self.assertIn(phase, enum_body)

        begin = self.source.index("void MainScene::beginContactGuestEntry")
        end = self.source.index("void MainScene::beginTeamMemberEntry", begin)
        body = self.source[begin:end]
        self.assertIn("beginContactHostClearDoor", body)
        self.assertIn("beginContactGuestApproach", body)
        self.assertIn("beginContactArrivalConversation", body)
        self.assertIn("showContactArrivalDialog", body)
        self.assertIn('"[ContactArrival] event=host_to_door', body)

    def test_contact_arrival_reuses_sequential_speaker_hops(self):
        begin = self.source.index("float MainScene::pairConversationHopOffset")
        end = self.source.index("bool MainScene::startPairInteraction", begin)
        body = self.source[begin:end]
        self.assertIn("ContactGuestMotion::ARRIVAL_TALK", body)
        self.assertIn("contactGuestMotionStartedMs", body)
        self.assertIn("PAIR_TALK_HOP_MS + PAIR_TALK_GAP_MS", body)

    def test_contact_motion_keeps_host_walk_animation_velocity(self):
        begin = self.source.index("updateContactVisit(nowMs, dtSeconds)")
        end = self.source.index("if (updatePairInteraction", begin)
        body = self.source[begin:end]
        self.assertIn("if (contactDialog != ContactDialog::NONE)", body)
        self.assertIn("mainActor.velocityX = 0.0f", body)

    def test_contact_guest_and_hud_stay_hidden_until_door_entry(self):
        begin = self.source.index("void MainScene::beginContactGuestEntry")
        end = self.source.index("bool MainScene::beginContactHostClearDoor", begin)
        entry_body = self.source[begin:end]
        self.assertIn("doorVisitorHidden = true", entry_body)

        begin = self.source.index(
            "case ContactGuestMotion::HOST_TO_DOOR")
        end = self.source.index(
            "case ContactGuestMotion::HOST_OPEN_DOOR", begin)
        open_body = self.source[begin:end]
        self.assertIn("doorVisitorHidden = false", open_body)

        begin = self.source.index("void MainScene::drawHud()")
        end = self.source.index("void MainScene::drawToast", begin)
        hud_body = self.source[begin:end]
        self.assertIn("bool visitingGuestEntered", hud_body)
        self.assertIn("visitor.active && !doorVisitorHidden", hud_body)
        self.assertIn(
            "contactGuestMotion != ContactGuestMotion::ENTER_CROSS",
            hud_body,
        )

    def test_contact_dialog_adapts_height_and_uses_translucent_background(self):
        self.assertIn("CONTACT_DIALOG_SINGLE_H = 28", self.source)
        self.assertIn("CONTACT_DIALOG_CHOICE_H = 45", self.source)
        self.assertIn("CONTACT_DIALOG_BG_ALPHA = 153", self.source)
        begin = self.source.index("void MainScene::drawContactDialog()")
        end = self.source.index("bool MainScene::visitorHostActive", begin)
        body = self.source[begin:end]
        self.assertIn("int boxH = choiceDialog", body)
        self.assertIn("CONTACT_DIALOG_BOTTOM - boxH", body)
        self.assertIn("fillRoundRectAlpha(", body)

    def test_contact_guest_walks_inward_after_crossing_the_door(self):
        begin = self.source.index("bool MainScene::beginContactMeetingArrangement")
        end = self.source.index("bool MainScene::beginContactGuestApproach", begin)
        arrangement = self.source[begin:end]
        self.assertIn("contactGuestMeetX", arrangement)
        self.assertIn("contactGuestMeetY", arrangement)
        self.assertIn("ContactGuestMotion::HOST_TO_MEET", arrangement)
        self.assertIn("guestOutsideStart, guestGeometry, false", arrangement)

        begin = self.source.index("bool MainScene::beginContactGuestApproach")
        end = self.source.index(
            "void MainScene::beginContactArrivalConversation", begin)
        body = self.source[begin:end]
        self.assertIn("minTravel = 14.0f", body)
        self.assertIn("buildPairActorRoute(", body)
        self.assertIn("visitor.task = VisitorState::WALK", body)
        self.assertIn('event=guest_meet_failed', body)
        self.assertIn('route=%u candidates=%u', body)
        self.assertIn("!actorFootprintInsideWalkArea(", body)

        update_begin = self.source.index("void MainScene::updateContactVisit")
        update_end = self.source.index("void MainScene::beginContactGuestEntry", update_begin)
        update = self.source[update_begin:update_end]
        self.assertLess(
            update.index("case ContactGuestMotion::HOST_TO_MEET"),
            update.index("case ContactGuestMotion::GUEST_TO_MEET"),
        )

    def test_contact_arrival_does_not_talk_after_route_failure(self):
        begin = self.source.index("case ContactGuestMotion::ENTER_CROSS")
        end = self.source.index("case ContactGuestMotion::HOST_TO_MEET", begin)
        body = self.source[begin:end]
        self.assertIn("deferContactMeeting(nowMs)", body)
        self.assertNotIn("beginContactArrivalConversation(nowMs)", body)
        self.assertIn("case ContactGuestMotion::MEETING_RETRY", body)

    def test_contact_departure_walks_out_without_teleporting(self):
        enum_begin = self.header.index("enum class ContactGuestMotion")
        enum_end = self.header.index("enum class PairInteraction", enum_begin)
        enum_body = self.header[enum_begin:enum_end]
        self.assertIn("EXIT_ROUTE", enum_body)
        self.assertIn("EXIT_DIRECT", enum_body)
        self.assertIn("EXIT_CROSS", enum_body)

        begin = self.source.index("void MainScene::beginContactGuestExit")
        end = self.source.index("bool MainScene::handleContactDialogButton", begin)
        body = self.source[begin:end]
        self.assertIn("visitorDoorInsideX, visitorDoorInsideY", body)
        self.assertIn("ContactGuestMotion::EXIT_ROUTE", body)
        self.assertIn("ContactGuestMotion::EXIT_DIRECT", body)
        self.assertNotIn("visitor.x = visitorDoorInsideX", body)
        self.assertNotIn("visitor.y = visitorDoorInsideY", body)

        update_begin = self.source.index(
            "case ContactGuestMotion::EXIT_ROUTE")
        update_end = self.source.index(
            "case ContactGuestMotion::NONE", update_begin)
        update = self.source[update_begin:update_end]
        self.assertIn("case ContactGuestMotion::EXIT_DIRECT", update)
        self.assertIn("case ContactGuestMotion::EXIT_CROSS", update)
        self.assertLess(
            update.index("case ContactGuestMotion::EXIT_DIRECT"),
            update.index("case ContactGuestMotion::EXIT_CROSS"),
        )
        self.assertLess(
            update.index("case ContactGuestMotion::EXIT_CROSS"),
            update.index("deactivateVisitor()"),
        )

    def test_visitor_door_anchor_uses_visitor_geometry(self):
        begin = self.source.index("bool MainScene::prepareDoorAnchors")
        end = self.source.index("bool MainScene::chooseDoorWaitPose", begin)
        body = self.source[begin:end]
        self.assertIn("chooseDoorInsidePoseForGeometry(", body)
        self.assertIn("visitorGeometry(), visitorDoorInsideX", body)
        self.assertIn("actorFootprintInsideWalkArea(", body)


if __name__ == "__main__":
    unittest.main()
