#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class HomeCoordinatorTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_resource_and_pair_invariants(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "home_coordinator_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "home_coordinator_host.cpp"),
                    str(ROOT / "src" / "game" / "HomeActor.cpp"),
                    str(ROOT / "src" / "game" / "HomeCoordinator.cpp"),
                    str(ROOT / "src" / "game" / "MonsterMind.cpp"),
                    str(ROOT / "src" / "game" / "Species.cpp"),
                    str(ROOT / "src" / "game" / "GameRandom.cpp"),
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
