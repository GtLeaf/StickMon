#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from amoled_page_cases import run_page_case


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

    def test_amoled_cancel_hold_preserves_species_and_stone(self):
        run_page_case("evolution-cancel")

    def test_amoled_commits_once_after_animation_and_confirmation(self):
        run_page_case("evolution-commit")


if __name__ == "__main__":
    unittest.main()
