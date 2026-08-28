#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class SaveMigrationTests(unittest.TestCase):
    def test_v1_v2_records_migrate_and_are_rewritten_as_v3(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / "save_migration_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Isrc",
                    "tools/save_migration_host.cpp",
                    "src/core/SaveCodec.cpp",
                    "src/core/SaveManager.cpp",
                    "src/game/Species.cpp",
                    "src/platform/api/PlatformServices.cpp",
                    "src/platform/desktop/DesktopPlatform.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
