#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class AudioManagerHomeSilenceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_home_pauses_two_to_four_minutes_between_complete_plays(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / "audio_manager_home_silence_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Isrc",
                    "tools/audio_manager_home_silence_host.cpp",
                    "src/core/AudioManager.cpp",
                    "src/core/ResourcePack.cpp",
                    "src/core/ResourceFS.cpp",
                    "src/platform/api/PlatformServices.cpp",
                    "src/platform/desktop/DesktopPlatform.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run(
                [str(executable), str(ROOT / "data")],
                cwd=ROOT,
                check=True,
            )


if __name__ == "__main__":
    unittest.main()
