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

    def test_second_monster_uses_its_schedule_not_room_night(self):
        begin = self.source.index("void MainScene::updateVisitor(")
        end = self.source.index(
            "const PokemonSprites::SpriteFrame* MainScene::visitorCurrentFrame",
            begin,
        )
        update_visitor = self.source[begin:end]
        self.assertIn(
            "bool sleepTime = monsterIsSleepTime(visitorMonster);",
            update_visitor,
        )
        self.assertNotIn("mainSceneIsNight()", update_visitor)

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
        self.assertIn(
            "monsterShouldWakeForFood(visitorMonster.satiety)", self.source)
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
        self.assertIn("visitor.resumeTask = visitor.task", body)
        self.assertIn("pickVisitorSleepSpot(candidateX, candidateY, true)", body)

        begin = self.source.index("void MainScene::setBedTarget")
        end = self.source.index("void MainScene::snapMonsterToBed", begin)
        body = self.source[begin:end]
        self.assertIn("startVisitorBedYield(nowMs)", body)
        self.assertIn("startVisitorBedYield(nowMs)", body)
        self.assertIn("mainActor.nextDecisionMs = nowMs + 350", body)

    def test_bed_yield_restores_the_teammates_previous_state(self):
        begin = self.source.index("void MainScene::finishVisitorBedYield")
        end = self.source.index("void MainScene::logVisitorSleepEvent", begin)
        body = self.source[begin:end]
        self.assertIn(
            "visitor.resumeTask == VisitorState::SLEEPING", body)
        self.assertIn("visitor.task = VisitorState::SLEEPING", body)
        self.assertIn("visitor.task = VisitorState::IDLE", body)
        self.assertIn("mainActor.nextDecisionMs =", body)
        self.assertIn("Home::WorldEvent::ACTOR_MOVED", body)

        begin = self.source.index("void MainScene::handleVisitorMoveBlocked")
        end = self.source.index("bool MainScene::buildFoodApproachRoute", begin)
        body = self.source[begin:end]
        self.assertIn("finishVisitorBedYield(nowMs, false)", body)


if __name__ == "__main__":
    unittest.main()
