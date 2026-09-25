#!/usr/bin/env python3

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class EvolutionSequenceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_shared_timeline_boundaries(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "evolution_sequence_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "evolution_sequence_host.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_amoled_stone_selection_does_not_commit_early(self):
        source = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
        ).read_text(encoding="utf-8")
        target_flow = re.search(
            r"if \(selectingItemTarget\).*?"
            r"if \(isEvolutionStone\(pendingItem\)\) \{(.*?)"
            r"const uint8_t oldLevel",
            source,
            re.S,
        )
        self.assertIsNotNone(target_flow)
        body = target_flow.group(1)
        self.assertIn("openEvolutionProgression(", body)
        self.assertNotIn("useOnTeam(", body)
        self.assertNotIn("ItemInventory::remove(", body)
        self.assertNotIn("speciesId =", body)

    def test_amoled_commits_only_after_animation_completion(self):
        source = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
        ).read_text(encoding="utf-8")
        advance = re.search(
            r"void AmoledApp::advanceProgression\(uint32_t nowMs\) \{(.*?)"
            r"void AmoledApp::completeProgression",
            source,
            re.S,
        )
        self.assertIsNotNone(advance)
        body = advance.group(1)
        completion_check = body.index(
            "if (!progressionEvolution.animationComplete(nowMs)) return;"
        )
        remove_item = body.index("Game::ItemInventory::remove(")
        commit_species = body.index("monster.speciesId = target->id;")
        self.assertLess(completion_check, remove_item)
        self.assertLess(remove_item, commit_species)

    def test_cancel_path_keeps_species_and_item(self):
        source = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
        ).read_text(encoding="utf-8")
        cancel = re.search(
            r"if \(progressionEvolution\.cancelling\(\)\) \{(.*?)"
            r"\} else if \(progressionEvolution\.animationComplete",
            source,
            re.S,
        )
        self.assertIsNotNone(cancel)
        body = cancel.group(1)
        self.assertIn("cancellationComplete(nowMs)", body)
        self.assertNotIn("ItemInventory::remove(", body)
        self.assertNotIn("speciesId =", body)

    def test_amoled_evolution_uses_a_600ms_hold_without_move_cancellation(self):
        source = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("constexpr uint32_t EVOLUTION_CANCEL_HOLD_MS = 600;", source)
        self.assertIn("constexpr uint32_t EVOLUTION_TOUCH_GAP_MS = 50;", source)
        hold_update = re.search(
            r"if \(progressionEvolutionTouchHeld\) \{(.*?)"
            r"if \(!progressionEvolution\.cancelling\(\)",
            source,
            re.S,
        )
        self.assertIsNotNone(hold_update)
        body = hold_update.group(1)
        self.assertLess(
            body.index("progressionEvolution.animationComplete(nowMs)"),
            body.index("EVOLUTION_CANCEL_HOLD_MS"),
        )
        self.assertIn("progressionEvolution.beginCancellation(nowMs)", body)
        self.assertNotRegex(
            body,
            r"touchStart[XY]|TAP_SLOP|DRAG_START_SLOP|std::abs",
        )

    def test_amoled_evolution_consumes_the_touch_until_release(self):
        source = (
            ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("progressionEvolutionTouchConsumed", source)
        self.assertIn("resumeGesture || cancellable || cancellationAnimating", source)
        self.assertIn("evolutionTouchWasConsumed", source)
        self.assertIn("else if (evolutionTouchWasConsumed)", source)
        self.assertLess(
            source.index("else if (evolutionTouchWasConsumed)"),
            source.index(
                "else if (!dragging && distance <= TAP_SLOP)",
                source.index("else if (evolutionTouchWasConsumed)"),
            ),
        )


if __name__ == "__main__":
    unittest.main()
