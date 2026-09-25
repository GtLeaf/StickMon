#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.cpp").read_text(
    encoding="utf-8"
)
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
        main_pet = render.index("drawPet(canvas, model)")
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

    def test_main_sleep_commits_directly_when_already_at_bed_pose(self):
        begin = APP.index("MonsterDesire desire = monsterMind.topDesire();")
        end = APP.index(
            '"[AmoledPet] decision desire=%u motion=%u', begin
        )
        body = APP[begin:end]
        self.assertIn("const bool atSleepPose =", body)
        self.assertIn("homeMainActor.route.clear();", body)
        self.assertIn("petScheduledSleeping = true;", body)
        self.assertIn(
            "homeRuntime.transition(\n"
            "                0, Home::Task::SLEEPING",
            body,
        )
        self.assertIn("sleep blocked bedOwner=%d", body)

    def test_main_survival_policy_uses_the_mind_that_is_updated(self):
        observation = APP[
            APP.index("Home::ActorObservation AmoledApp::homeActorObservation("):
            APP.index("Home::HouseholdObservation AmoledApp::homeHouseholdObservation(")
        ]
        self.assertIn(
            "observation.desire = teamSlot == 0\n"
            "            ? monsterMind.topDesire() : actor->mind.topDesire();",
            observation,
        )
        self.assertIn('"[HomeSleep] event=decision intent=%u desire=%u "', APP)

    def test_awake_pet_leaves_persisted_bed_pose(self):
        self.assertIn("const bool atBedPose =", APP)
        self.assertIn("chooseWakeTarget(wakeX, wakeY)", APP)
        self.assertIn("[HomeSleep] event=leave_bed", APP)

    def test_empty_bowl_severe_hunger_keeps_wander_available(self):
        MIND = (ROOT / "src/game/MonsterMind.cpp").read_text(encoding="utf-8")
        self.assertIn("if (bowlHasFood)", MIND)
        self.assertIn("wander += 35", MIND)

    def test_bowl_is_arbitrated_and_visitors_cannot_eat(self):
        self.assertIn("bool AmoledApp::claimBowl(uint8_t teamSlot", APP)
        self.assertIn("startMainFoodSeek(nowMs)", APP)
        self.assertIn("startCompanionFoodSeek(nowMs)", APP)
        self.assertIn(
            "observation.visitor = monster.origin == Game::Origin::VISITOR",
            APP,
        )

    def test_fresh_food_wakes_both_pets_and_bypasses_mind_inertia(self):
        for token in (
            "wakeHomeFoodArbitration(nowMs);",
            "homeRuntime.notify(Home::WorldEvent::BOWL_CHANGED)",
            "homeCompanionActor.nextMindUpdateMs = nowMs",
            "homeCompanionActor.nextDecisionMs = nowMs",
            "serviceHomeFoodArbitration(nowMs);",
        ):
            self.assertIn(token, APP)

    def test_more_hungry_pet_gets_next_serving_without_blocker_yield(self):
        for token in (
            "int8_t AmoledApp::preferredBowlEater(uint32_t nowMs) const",
            "Home::ActorController::selectBowlActor(",
            "homeHouseholdObservation(nowMs)).actorId",
            "world.actorCollisionMode = Home::ActorCollisionMode::SOFT",
            "homeRuntime.beginTick(nowMs);",
        ):
            self.assertIn(token, APP)
        self.assertNotIn("startMainFoodYield", APP)
        self.assertNotIn("startCompanionFoodYield", APP)

    def test_unowned_food_reenters_arbitration_after_route_failure_or_restore(self):
        self.assertIn(
            "!homeFoodArbitrationPending &&\n"
            "        gameState.room.bowlCount > 0 &&\n"
            "        homeRuntime.owner(Home::Resource::BOWL) < 0 &&\n"
            "        preferredBowlEater(nowMs) >= 0",
            APP,
        )
        self.assertIn("nextHomeFoodArbitrationMs = nowMs;", APP)

    def test_pet_already_beside_bowl_enters_feeding_without_a_route(self):
        for token in (
            "bool AmoledApp::homeActorNearFood(uint8_t actorId) const",
            "if (homeActorNearFood(0))",
            "if (homeActorNearFood(1))",
            "homeRuntime.transition(0, Home::Task::FEEDING",
            "homeRuntime.transition(1, Home::Task::FEEDING",
            '"[HomeFood] event=direct actor=0',
            '"[HomeFood] event=direct actor=1',
        ):
            self.assertIn(token, APP)

    def test_companion_food_uses_soft_avoidance_and_reachable_approach(self):
        for token in (
            "bool AmoledApp::companionFoodAvoidsMain() const",
            "const bool avoidMain = companionFoodAvoidsMain();",
            "for (int radius = 0; radius <= 36 && !routeFound; radius += 2)",
            "homeRuntime.planRoute(1, candidateX, candidateY,\n"
            "                                          false, avoidMain)",
            "companionFoodAvoidsMain()));",
        ):
            self.assertIn(token, APP)

    def test_companion_clears_bowl_before_leader_takes_next_turn(self):
        self.assertIn("bool AmoledApp::startCompanionFoodExit(uint32_t nowMs)", APP)
        self.assertIn("if (teamSlot == 0 && homeActorBlocksFoodApproach(1))", APP)
        self.assertIn("beginCompanionMove(Home::Task::YIELDING, x, y,\n"
                      "                                   nowMs, false, false)", APP)
        self.assertIn('"[HomeFood] event=exit actor=1', APP)
        self.assertIn('"[HomeFood] event=exit_failed actor=1', APP)

    def test_food_diagnostics_cover_state_and_route_failures(self):
        for token in (
            '"[HomeFood] event=place result=%u',
            '"satiety=%u/%u canEat=%u/%u task=%u/%u desire2=%u "',
            '"near=%u/%u block=%u/%u pending=%u\\n"',
            '"[HomeFood] event=reject actor=0 stage=claim',
            '"[HomeFood] event=reject actor=0 stage=approach',
            '"[HomeFood] event=reject actor=0 stage=route',
            '"[HomeFood] event=reject actor=1 stage=claim',
            '"[HomeFood] event=reject actor=1 stage=route',
            '"[HomeFood] event=blocked actor=0 stage=replan',
            '"[HomeFood] event=blocked actor=1 stage=%s',
        ):
            self.assertIn(token, APP)

    def test_new_serving_releases_lingering_old_feed_session(self):
        self.assertIn(
            "petMotion == PetMotion::EATING ||\n"
            "        (petMotion == PetMotion::STOPPING && petStoppingToEat)",
            APP,
        )
        self.assertIn(
            "homeCompanionActor.task == Home::Task::FEEDING", APP
        )

    def test_pair_timing_and_real_actor_routes_match_stick_contract(self):
        for token in (
            "PAIR_INTERACTION_MIN_INTERVAL_MS = 90000UL",
            "PAIR_INTERACTION_MAX_INTERVAL_MS = 180000UL",
            "GameRandom::random(100) >= 45",
            "homeRuntime.beginPair(pairActivity, nowMs)",
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
            "pairPhaseUntilMs = nowMs + pairApproachBudgetMs(movingActor);",
            "goalDistance <= PAIR_APPROACH_GOAL_TOLERANCE",
            '"[FriendDiag] talk approach-near-goal',
        ):
            self.assertIn(token, APP)

    def test_pair_talk_moves_both_actors_to_fixed_points_together(self):
        for token in (
            "pairTalkParallelApproach = true",
            "pairTalkMainApproachSpeed",
            "pairTalkCompanionApproachSpeed",
            "sharedDurationSeconds",
            "0, nowMs, std::max(0.1f, pairTalkMainApproachSpeed)",
            "std::max(0.1f, pairTalkCompanionApproachSpeed)",
            '"[FriendDiag] talk positioned kind=%u parallel=1',
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
            "void AmoledApp::finishVisitorExit(uint32_t nowMs)",
        ):
            self.assertIn(token, APP)
        self.assertIn("VisitorMotion visitorMotion", HEADER)

    def test_visitor_exit_route_advances_walk_presentation(self):
        start = APP.index(
            "if (visitorMotion == VisitorMotion::EXITING && !visitorCrossingDoor)"
        )
        end = APP.index("const float targetX", start)
        route = APP[start:end]
        for token in (
            "const float previousX = homeCompanionActor.x;",
            "const float movedX = homeCompanionActor.x - previousX;",
            "companionDirection = petDirectionForDelta(movedX, movedY);",
            "++companionFrame;",
            "nextCompanionFrameMs = nowMs + MOTION_FRAME_MS;",
            "outsideX - homeCompanionActor.x",
            '"frame=%u dir=%u moved=%u\\n"',
        ):
            self.assertIn(token, route)
        self.assertIn('"[FriendDiag] visitor-exit cross kind=%u', APP)

    def test_temporary_visitors_are_always_removed_from_saves(self):
        self.assertIn("Game::GameState persistentState = gameState;", APP)
        self.assertIn("origin != Game::Origin::VISITOR", APP)
        self.assertNotIn(
            "#if STICKMON_ENABLE_DEBUG_FEATURES\n    Game::GameState persistentState",
            APP,
        )

    def test_stick_environment_actions_are_migrated(self):
        for token in (
            "ATTENTION_INITIAL_MIN_MS = 25000UL",
            "ATTENTION_MIN_MS = 90000UL",
            "SPECIAL_ACTION_MIN_MS = 20000UL",
            "RoomAction::LOOK_AROUND",
            "RoomAction::QUIET_GAZE",
            "RoomAction::DASH",
            "RoomAction::STEP_BACK",
            "RoomAction::CIRCLE",
            "RoomAction::WINDOW_APPROACH",
            "Home::Task::ROOM_ACTION",
            "RoomResource::BehaviorAnchorType::WINDOW_GAZE",
        ):
            self.assertIn(token, APP)
        self.assertIn("RoomAction roomAction", HEADER)

    def test_environment_actions_yield_to_survival_and_pair_rules(self):
        self.assertIn(
            "if (roomAction != RoomAction::NONE && urgentNeed)", APP
        )
        self.assertIn("cancelRoomAction(nowMs);", APP)
        self.assertIn("roomAction != RoomAction::NONE ||", APP)

    def test_window_gaze_and_exploration_timestamps_are_persisted(self):
        self.assertIn("gameState.team[0].lastWindowGazeAt =", APP)
        self.assertIn("gameState.team[slot].lastExploredAt = exploredAt;", APP)
        self.assertIn("saveState();\n        return;\n    case RoomAction::CIRCLE", APP)

    def test_transient_home_actions_are_normalized_before_save(self):
        for token in (
            "const bool transientMain = roomAction != RoomAction::NONE",
            "homeMainActor.task == Home::Task::PAIR_ACTION",
            "homeMainActor.task == Home::Task::DOOR_ACTION",
            "storedMotion == PetMotion::EATING",
            "storedMotion == PetMotion::STOPPING",
            "const bool transientCompanion = homeRuntime.pairActive()",
            "homeCompanionActor.task == Home::Task::SEEK_FOOD",
        ):
            self.assertIn(token, APP)

    def test_saved_sleep_and_companion_food_retry_are_restored_safely(self):
        self.assertIn("mainViewState.aiMode == 8 && savedSleepTime", APP)
        self.assertIn("saved.state == 6 && sleepTime", APP)
        self.assertIn("nowMs + saved.foodRetryRemainingMs", APP)
        self.assertIn(
            "homeCompanionActor.foodWakeRetryAfterMs - nowMs", APP
        )

    def test_companion_sleep_matches_stick_anchor_and_locks_target(self):
        for token in (
            "float AmoledApp::companionSleepMinDistance() const",
            "bool AmoledApp::companionSleepSpotUsableWithDistance(",
            "bool AmoledApp::companionPointBlocksBedRoute(float x, float y) const",
            "BehaviorAnchorType::VISITOR_SLEEP",
            "homeMainActor.task == Home::Task::SEEK_SLEEP",
            "homeCompanionActor.sleepX = x;",
            "homeCompanionActor.sleepY = y;",
            "homeCompanionActor.sleepSpotValid = true;",
        ):
            self.assertIn(token, APP)


if __name__ == "__main__":
    unittest.main()
