import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class EvolutionCommitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / "src" / "core" / "GameEngine.cpp").read_text()

    def function_body(self, name, next_name):
        match = re.search(
            rf"bool GameEngine::{name}\(.*?"
            rf"bool GameEngine::{next_name}",
            self.source,
            re.S,
        )
        self.assertIsNotNone(match)
        return match.group(0)

    def test_level_up_only_queues_evolution(self):
        match = re.search(
            r"bool GameEngine::applyLevelUpEvolutions\(.*?"
            r"uint32_t GameEngine::applyFaintPenaltyToTeamMember",
            self.source,
            re.S,
        )
        self.assertIsNotNone(match)
        body = match.group(0)
        self.assertIn("queueEvolutionEvent(", body)
        self.assertNotIn("mon.speciesId =", body)
        self.assertNotIn("sanitizeMonsterMovesForSpecies", body)

    def test_animation_acknowledgement_commits_evolution(self):
        body = self.function_body(
            "acknowledgePendingEvolution", "cancelPendingEvolution"
        )
        self.assertIn("mon.speciesId = target->id;", body)
        self.assertIn("sanitizeMonsterMovesForSpecies(mon, *target);", body)
        self.assertIn("syncSpriteCache();", body)

    def test_scene_animation_time_is_not_scaled_by_game_speed(self):
        self.assertIn("updatingScene->update(nowMs, dt);", self.source)
        self.assertNotIn(
            "updatingScene->update(nowMs, dt * gameSpeed())", self.source
        )


if __name__ == "__main__":
    unittest.main()
