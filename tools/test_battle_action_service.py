#!/usr/bin/env python3
"""Shared execution contracts; actual page wiring is checked separately."""
import shutil
import subprocess
import tempfile
import unittest
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class BattleActionServiceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("c++"):
            raise unittest.SkipTest("host C++ compiler is unavailable")
        temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temporary.cleanup)
        cls.binary = Path(temporary.name) / "battle-action-service"
        subprocess.run([
            "c++", "-std=c++17", "-Isrc",
            "tools/battle_action_service_host.cpp",
            "src/game/BattleActionService.cpp", "src/game/BattleSystem.cpp",
            "src/game/GameRandom.cpp", "src/game/Species.cpp",
            "-o", str(cls.binary),
        ], cwd=ROOT, check=True)

    def cases(self, *names):
        for name in names:
            with self.subTest(name=name):
                subprocess.run([str(self.binary), name], cwd=ROOT, check=True)

    def test_sleep_wake_freeze_thaw_and_paralysis(self):
        self.cases("sleep", "wake", "freeze", "thaw", "paralysis")

    def test_confusion_self_hit_faint_and_end(self):
        self.cases("confusion", "confusion-ko", "confusion-end")

    def test_recharge_flinch_and_charge_interruption(self):
        self.cases("recharge", "flinch", "charge-blocked")

    def test_charge_defense_and_release(self):
        self.cases("charge", "release", "reset-release")

    def test_miss_type_immunity_levitate_and_absorb(self):
        self.cases("miss", "immune", "levitate", "absorb", "flash-fire")

    def test_contact_ability_rampage_and_flinch_timing(self):
        self.cases("contact", "rampage", "flinch-after-action", "effect-order")

    def test_stockpile_healing_and_damage_before_consumption(self):
        self.cases("swallow", "spit-up")

    def test_drain_recoil_self_faint_and_sturdy(self):
        self.cases("drain", "recoil", "self-faint", "sturdy")

    def test_forced_end_and_no_status_on_fainted_defender(self):
        self.cases("forced-end", "wild-forced-end", "defender-ko",
                   "attacker-fainted", "defender-fainted")

    def test_full_random_trace_proficiency_and_repeat_safety(self):
        self.cases("random", "wild-proficiency")

    def test_real_stick_and_amoled_adapters_use_shared_execution(self):
        stick = (ROOT / "src/scenes/ExploreScene.cpp").read_text()
        amoled = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.cpp").read_text()
        def method(source, name):
            start = source.index(name)
            following = re.search(r"\n(?:void|bool|int8_t) \w+::", source[start + len(name):])
            return source[start:start + len(name) + following.start()] if following else source[start:]
        stick_prepare = method(stick, "void ExploreScene::beginBattleAction()")
        stick_commit = method(stick, "void ExploreScene::applyBattleDamage()")
        amoled_action = method(amoled, "void AmoledApp::performBattleAction(")
        self.assertIn("battlePendingAction.reset()", stick_prepare)
        self.assertIn("BattleActionService::prepare(", stick_prepare)
        self.assertIn("if (!BattleActionService::commit(", stick_commit)
        self.assertIn("BattleActionService::prepare(", amoled_action)
        self.assertIn("BattleActionService::commit(", amoled_action)
        for adapter in (stick_prepare, stick_commit, amoled_action):
            for operation in ("checkAction", "calcBasicDamage", "applyMoveEffects",
                              "applyPostDamageAbilities", "applyAbsorbAbility", "recordMoveResult"):
                self.assertNotIn(f"BattleSystem::{operation}(", adapter)
        for name in ("performBattlePlayerAction", "performBattleWildAction"):
            self.assertIn("performBattleAction(action, nowMs);",
                          method(amoled, f"void AmoledApp::{name}("))


if __name__ == "__main__":
    unittest.main()
