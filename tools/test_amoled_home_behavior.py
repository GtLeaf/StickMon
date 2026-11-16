#!/usr/bin/env python3

import unittest
from pathlib import Path
from amoled_source import read_app_source


ROOT = Path(__file__).resolve().parents[1]
APP = read_app_source(ROOT)
HEADER = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.h").read_text(
    encoding="utf-8"
)
HOME_SCREEN = (
    ROOT / "firmware/amoled_1_8_v1/main/HomeScreen.cpp"
).read_text(encoding="utf-8")


class AmoledHomeBehaviorTests(unittest.TestCase):
    def test_room_food_renders_below_all_pets(self):
        start = HOME_SCREEN.index("void renderHomeScreen(")
        end = HOME_SCREEN.index("void renderDebugScreen(", start)
        render = HOME_SCREEN[start:end]
        food = render.index("drawFoodContent(canvas")
        fallback_bowl = render.index("drawBowl(canvas")
        companion = render.index("drawHomeCompanion(canvas")
        main_pet = render.index("drawPet(canvas, model,")
        self.assertLess(food, companion)
        self.assertLess(food, main_pet)
        self.assertLess(fallback_bowl, companion)
        self.assertLess(fallback_bowl, main_pet)

    def test_successful_pet_does_not_show_a_toast(self):
        pet = APP[APP.index("case PetOutcome::REWARDED:"):APP.index(
            "case PetOutcome::DAILY_LIMIT:", APP.index("case PetOutcome::REWARDED:"))]
        self.assertNotIn("setToast(", pet)

    def test_auto_feed_does_not_show_yum_toast(self):
        start = APP.index('"[AmoledApp] auto-feed satiety=')
        end = APP.index("saveState();", start)
        self.assertNotIn("setToast(", APP[start:end])

    def test_both_actors_have_survival_states(self):
        for token in (
            "PetMotion::SEEKING_SLEEP",
            "PetMotion::SLEEPING",
            "homeCompanionActor.mind.update(",
            "Home::Task::SEEK_FOOD",
            "Home::Task::SEEK_SLEEP",
            "Home::Task::SLEEPING",
            "monsterShouldWakeForFood(monster.satiety)",
            "BehaviorAnchorType::VISITOR_SLEEP",
        ):
            self.assertIn(token, APP)

    # Direct entry, lease failures, anchor/grid/route geometry and wake deadlines
    # are covered by test_home_sleep_flow and real-App frozen sleep traces.
    def test_main_survival_policy_uses_the_mind_that_is_updated(self):
        observation = APP[
            APP.index("Home::ActorObservation AmoledApp::homeActorObservation("):
            APP.index("Home::HouseholdObservation AmoledApp::homeHouseholdObservation(")
        ]
        self.assertIn("actor->mind.topDesire()", observation)
        self.assertIn("homeMainActor.mind.update(", APP)
        self.assertIn('"[HomeSleep] event=decision intent=%u desire=%u "', APP)

    def test_awake_pet_leaves_persisted_bed_pose(self):
        self.assertIn("const bool atBedPose =", APP)
        self.assertIn("chooseWakeTarget(wakeX, wakeY)", APP)
        self.assertIn("[HomeSleep] event=leave_bed", APP)

    def test_empty_bowl_severe_hunger_keeps_wander_available(self):
        MIND = (ROOT / "src/game/MonsterMind.cpp").read_text(encoding="utf-8")
        self.assertIn("if (bowlHasFood)", MIND)
        self.assertIn("wander += 35", MIND)

    # Food ownership/visitor/retry/clearance/serving behavior is exercised by
    # test_home_food_flow.py and the real-App food trace in test_amoled_native_render.py.
    def test_food_adapter_keeps_shared_avoidance_and_diagnostics(self):
        for token in (
            "world.actorCollisionMode = Home::ActorCollisionMode::SOFT",
            "homeRuntime.beginTick(nowMs);",
            "observation.visitor = monster.origin == Game::Origin::VISITOR",
            '"[HomeFood] event=place result=%u',
            '"[HomeFood] event=%s actor=%u stage=%s owner=%d task=%u/%u',
            '"[HomeFood] event=blocked actor=0 stage=route',
            '"[HomeFood] event=blocked actor=1 stage=%s',
        ):
            self.assertIn(token, APP)

    def test_pair_timing_and_real_actor_routes_match_stick_contract(self):
        for token in (
            "PAIR_INTERACTION_MIN_INTERVAL_MS = 90000UL",
            "PAIR_INTERACTION_MAX_INTERVAL_MS = 180000UL",
            "Home::ActorController::pairActivity(",
            "homeRuntime.beginPair(state_.activity, nowMs)",
            "homeRuntime.advanceRoute(\n                0,",
            "homeRuntime.advanceRoute(\n                1,",
            "PokemonSprites::frameVisibleWidth(frame)",
            "gameState.pairMoodRewardsToday < 3",
        ):
            self.assertIn(token, APP)

    def test_pair_talk_budget_uses_route_length_and_accepts_near_goal(self):
        for token in (
            "float pairRouteDistance(const Home::Actor& actor)",
            "uint32_t pairApproachBudgetMs(const Home::Actor& actor)",
            "index < actor.route.count",
            "routeDistance += std::hypot",
            "state_.phaseUntilMs = nowMs + pairApproachBudgetMs(movingActor);",
            "goalDistance <= PAIR_APPROACH_GOAL_TOLERANCE",
            'Trace::NEAR_GOAL',
        ):
            self.assertIn(token, APP)

    def test_pair_talk_moves_both_actors_to_fixed_points_together(self):
        for token in (
            "state_.parallelApproach = true",
            "state_.mainApproachSpeed",
            "state_.companionApproachSpeed",
            "sharedDurationSeconds",
            "0, nowMs, std::max(0.1f, state_.mainApproachSpeed)",
            "std::max(0.1f, state_.companionApproachSpeed)",
            '"[FriendDiag] talk positioned kind=%u parallel=%u',
            '"[FriendDiag] talk approach-parallel kind=%u',
            '"[FriendDiag] talk active kind=%u',
        ):
            self.assertIn(token, APP)
        self.assertNotIn("pairTalkSecondPending", APP)
        self.assertIn("talk configured-gap", APP)

    def test_visitors_enter_and_leave_through_room_door(self):
        for token in (
            "void AmoledApp::beginVisitorEntry(uint32_t nowMs)",
            "void AmoledApp::beginVisitorExit(uint32_t nowMs",
            "room.doorwayInsideX()",
            "room.doorwayOutsideX()",
            "void AmoledApp::finishVisitorExit(uint32_t nowMs, bool debugVisitor)",
        ):
            self.assertIn(token, APP)
        self.assertIn("VisitorFlow visitorFlow_", HEADER)
        self.assertIn("VisitAwayFlow visitAwayFlow_", HEADER)

    def test_visitor_exit_route_advances_walk_presentation(self):
        flow = (ROOT / "firmware/amoled_1_8_v1/main/app/VisitorFlow.cpp").read_text()
        start = flow.index("if (state_.motion == Motion::EXITING && !state_.crossingDoor)")
        end = flow.index("const float targetX", start)
        route = flow[start:end]
        for token in (
            "const float previousX = homeCompanionActor.x;",
            "const float movedX = homeCompanionActor.x - previousX;",
            "face(1, movedX, movedY);",
            "effects_.frameMask |= 2;",
            "outsideX - homeCompanionActor.x",
            "damageActor(1, previousY);",
        ):
            self.assertIn(token, route)
        self.assertIn("effects.frameMask & 2", APP)
        self.assertIn("++companionFrame; nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;", APP)

    def test_temporary_visitors_are_always_removed_from_saves(self):
        self.assertIn("Game::GameState persistentState = gameState;", APP)
        self.assertIn("persistentState.activeSlot = 0;", APP)
        save = APP[APP.index("bool AmoledApp::saveState()"):]
        self.assertNotIn("for (uint8_t slot = 0; slot < persistentState.teamCount;)", save)
        self.assertNotIn(
            "#if STICKMON_ENABLE_DEBUG_FEATURES\n    Game::GameState persistentState",
            APP,
        )

    def test_stick_environment_actions_are_migrated(self):
        for token in (
            "ATTENTION_INITIAL_MIN_MS = 25000UL",
            "ATTENTION_MIN_MS = 90000UL",
            "SPECIAL_ACTION_MIN_MS = 20000UL",
            "Action::LOOK_AROUND",
            "Action::QUIET_GAZE",
            "Action::DASH",
            "Action::STEP_BACK",
            "Action::CIRCLE",
            "Action::WINDOW_APPROACH",
            "Home::Task::ROOM_ACTION",
            "RoomResource::BehaviorAnchorType::WINDOW_GAZE",
        ):
            self.assertIn(token, APP)
        self.assertIn("HomeFlow homeFlow_", HEADER)

    def test_environment_actions_yield_to_survival_and_pair_rules(self):
        self.assertIn(
            "if (homeFlow_.state().action != RoomAction::NONE && urgentNeed)", APP
        )
        self.assertIn("cancelRoomAction(nowMs);", APP)
        self.assertIn("homeFlow_.state().action != RoomAction::NONE ||", APP)

    def test_window_gaze_and_exploration_timestamps_are_persisted(self):
        self.assertIn("gameState.team[0].lastWindowGazeAt =", APP)
        self.assertIn("gameState.team[slot].lastExploredAt = exploredAt;", APP)
        self.assertIn("effects_.save = true;\n        return;\n    case Action::CIRCLE", APP)

    def test_transient_home_actions_are_normalized_before_save(self):
        for token in (
            "const bool transientMain = homeFlow_.state().action != RoomAction::NONE",
            "homeMainActor.task == Home::Task::PAIR_ACTION",
            "homeMainActor.task == Home::Task::DOOR_ACTION",
            "storedMotion == MainMotion::EATING",
            "storedMotion == MainMotion::STOPPING",
            "const bool transientCompanion = homeRuntime.pairActive()",
            "homeCompanionActor.task == Home::Task::SEEK_FOOD",
        ):
            self.assertIn(token, APP)

    def test_saved_sleep_and_companion_food_retry_are_restored_safely(self):
        self.assertIn("saved.aiMode == 8 && sleepTime", APP)
        self.assertIn("saved.state == 6 && sleepTime", APP)
        self.assertIn("nowMs + saved.foodRetryRemainingMs", APP)
        self.assertIn("actor.foodWakeRetryAfterMs, nowMs", APP)



if __name__ == "__main__":
    unittest.main()
