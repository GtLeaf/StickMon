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
                    str(ROOT / "src" / "game" / "Species.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
