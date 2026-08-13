#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class BattleStatusTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_status_effects_and_turn_rules(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "battle_status_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'tools' / 'host_stubs'}",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "battle_status_host.cpp"),
                    str(ROOT / "src" / "game" / "BattleSystem.cpp"),
                    str(ROOT / "src" / "game" / "GameRandom.cpp"),
                    str(ROOT / "src" / "game" / "Species.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_player_and_wild_both_use_tactical_move_selection(self):
        source = (ROOT / "src" / "scenes" / "ExploreScene.cpp").read_text()
        attack_start = source.index("void ExploreScene::attackWild()")
        attack_end = source.index("void ExploreScene::wildCounterattack()")
        attack = source[attack_start:attack_end]
        self.assertIn("playerBattleState.lockedMoveId", attack)
        self.assertIn("BattleSystem::chooseAiMoveSlot(", attack)
        self.assertIn("battleTurnSpecialSlots[1] =", attack)
        self.assertIn("BattleSystem::isChargingMove(wildBattleState)", attack)
        self.assertIn(
            ": BattleSystem::chooseAiMoveSlot(",
            attack,
        )
        self.assertNotIn("rollSpecialMoveSlot", attack)

    def test_solar_beam_charges_then_releases_automatically(self):
        source = (ROOT / "src" / "scenes" / "ExploreScene.cpp").read_text()
        action_start = source.index("void ExploreScene::beginBattleAction()")
        action_end = source.index("void ExploreScene::applyBattleDamage()")
        action = source[action_start:action_end]
        charge = action.index("BattleSystem::beginChargingMove")
        damage = action.index("BattleSystem::calcBasicDamage")
        self.assertLess(charge, damage)
        self.assertIn("battleTurnStage = BattleTurnStage::WAIT_ACTION_LOGS", action)
        self.assertIn("battleActionReleasingCharge", action)
        self.assertIn("void ExploreScene::beginChargedBattleTurn()", source)
        self.assertIn("BattleSystem::isChargingMove(playerBattleState)", source)
        finish_turn = source.index("void ExploreScene::finishBattleEndTurn()")
        charged_turn = source.index("void ExploreScene::beginChargedBattleTurn()")
        finish = source[finish_turn:charged_turn]
        self.assertIn("BattleSystem::isChargingMove(playerBattleState)", finish)
        self.assertNotIn("BattleSystem::isChargingMove(wildBattleState)", finish)
        self.assertIn("BattleSystem::isChargingMove(wildBattleState)", source)


if __name__ == "__main__":
    unittest.main()
