#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class MainSceneSleepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (
            ROOT / "src" / "scenes" / "MainScene.cpp"
        ).read_text(encoding="utf-8")
        cls.controller = (
            ROOT / "src" / "game" / "HomeActorController.cpp"
        ).read_text(encoding="utf-8")

    def test_second_monster_uses_its_schedule_not_room_night(self):
        begin = self.source.index("void MainScene::updateVisitor(")
        end = self.source.index(
            "const PokemonSprites::SpriteFrame* MainScene::visitorCurrentFrame",
            begin,
        )
        update_visitor = self.source[begin:end]
        self.assertRegex(update_visitor, r"Home::ActorController::survivalIntent\(\s*"
                         r"homeActorObservation\(1, nowMs\)")
        self.assertNotIn("mainSceneIsNight()", update_visitor)
        begin = self.source.index("Home::ActorObservation MainScene::homeActorObservation")
        end = self.source.index("Home::HouseholdObservation MainScene::homeHouseholdObservation", begin)
        observation = self.source[begin:end]
        self.assertIn("mon = state.team[teamSlot]", observation)
        self.assertRegex(observation, r"observation\.sleepTime = observation\.statusSleeping \|\|\s*"
                         r"\(care\.usesBed && \(teamSlot == 0\s*"
                         r"\? mainSceneIsSleepTime\(\) : monsterIsSleepTime\(mon\)\)\)")

    def test_saved_second_monster_rechecks_its_own_schedule(self):
        begin = self.source.index("void MainScene::onEnter()")
        end = self.source.index("void MainScene::onExit()", begin)
        on_enter = self.source[begin:end]
        self.assertIn("!monsterIsSleepTime(guest)", on_enter)

    def test_sleep_diagnostics_cover_every_wake_reason(self):
        for event in (
            "enter_schedule",
            "wake_schedule",
            "wake_spot_invalid",
            "wake_food_check",
            "wake_food_seek",
            "yield_bed",
            "resume_wake_schedule",
        ):
            self.assertIn(f'"{event}"', self.source)
        self.assertIn("[VisitorSleep] event=%s", self.source)
        self.assertNotIn('logVisitorSleepEvent("heartbeat"', self.source)

    def test_normal_hunger_does_not_interrupt_scheduled_sleep(self):
        self.assertRegex(self.source, r"observation\.wakeForFood = care\.needsFood &&\s*"
                         r"monsterShouldWakeForFood\(mon\.satiety\)")
        self.assertIn("actor.sleeping && (!actor.wakeForFood || !actor.foodRetryReady)",
                      self.controller)
        self.assertIn("visitorSurvivalIntent == Home::ActorIntent::WAKE_FOR_FOOD", self.source)
        self.assertIn(
            "MONSTER_SLEEP_FOOD_WAKE_SATIETY = 35",
            (ROOT / "src" / "game" / "MonsterMind.h").read_text(
                encoding="utf-8"),
        )
        self.assertIn(
            "VISITOR_NIGHT_FOOD_RETRY_MS = 60000UL", self.source)

    def test_pair_interactions_respect_both_sleep_schedules(self):
        begin = self.source.index("bool MainScene::pairInteractionAllowed()")
        end = self.source.index("bool MainScene::choosePairApproachPose", begin)
        body = self.source[begin:end]
        self.assertIn("monsterIsSleepTime(mainMon)", body)
        self.assertIn("monsterIsSleepTime(guestMon)", body)
        self.assertNotIn("mainSceneIsSleepTime()", body)

    def test_awake_teammate_can_yield_the_bed_corridor(self):
        begin = self.source.index("bool MainScene::startVisitorBedYield")
        end = self.source.index("void MainScene::finishVisitorBedYield", begin)
        body = self.source[begin:end]
        self.assertIn("visitor.task != VisitorState::IDLE", body)
        self.assertIn("visitor.task != VisitorState::SLEEPING", body)
        self.assertIn("const VisitorState resumeTask = visitor.task", body)
        self.assertRegex(body, r"homeCoordinator\.transitionPreparedRoute\(\s*"
                         r"1, VisitorState::YIELDING_BED, nowMs, 0, true\)")
        self.assertIn("visitor.resumeTask = resumeTask", body)
        self.assertLess(body.index("const VisitorState resumeTask = visitor.task"),
                        body.index("homeCoordinator.transitionPreparedRoute("))
        self.assertLess(body.index("homeCoordinator.transitionPreparedRoute("),
                        body.index("visitor.resumeTask = resumeTask"))
        self.assertIn("pickVisitorSleepSpot(candidateX, candidateY, true)", body)

        begin = self.source.index("void MainScene::setBedTarget")
        end = self.source.index("void MainScene::snapMonsterToBed", begin)
        body = self.source[begin:end]
        self.assertIn("startVisitorBedYield(nowMs)", body)
        self.assertRegex(body, r"startVisitorBedYield\(nowMs\)\) \{\s*"
                         r"homeCoordinator\.stop\(0, nowMs, 350\);")

    def test_bed_yield_restores_the_teammates_previous_state(self):
        begin = self.source.index("void MainScene::finishVisitorBedYield")
        end = self.source.index("void MainScene::logVisitorSleepEvent", begin)
        body = self.source[begin:end]
        self.assertIn(
            "visitor.resumeTask == VisitorState::SLEEPING", body)
        self.assertRegex(body, r"if \(resumeSleeping\) \{[\s\S]*"
                         r"homeCoordinator\.transition\(\s*1, VisitorState::SLEEPING, nowMs,")
        self.assertRegex(body, r"\} else \{\s*visitor\.sleepSpotValid = false;\s*"
                         r"homeCoordinator\.stop\(\s*1, nowMs,")
        self.assertIn("mainActor.nextDecisionMs =", body)
        self.assertIn("Home::WorldEvent::ACTOR_MOVED", body)

        begin = self.source.index("void MainScene::handleVisitorMoveBlocked")
        end = self.source.index("bool MainScene::buildFoodApproachRoute", begin)
        body = self.source[begin:end]
        self.assertIn("finishVisitorBedYield(nowMs, false)", body)


if __name__ == "__main__":
    unittest.main()
