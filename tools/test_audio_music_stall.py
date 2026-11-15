#!/usr/bin/env python3
"""A long UI frame must not drain the streamed market music on AMOLED."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MusicStallTests(unittest.TestCase):
    def test_amoled_queue_survives_long_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            # (queue target, refill per update, stall ms, underrun expected)
            cases = ((2, 1, 240, 1),  # shared default: one 128 ms block of slack
                     (3, 2, 240, 0),  # AMOLED build
                     (3, 2, 100, 0))
            for target, refill, stall, underrun in cases:
                with self.subTest(target=target, refill=refill, stall=stall):
                    binary = Path(directory) / f"stall-{target}-{refill}"
                    subprocess.run([
                        "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        f"-DSTICKMON_MUSIC_QUEUE_TARGET_BLOCKS={target}",
                        f"-DSTICKMON_MUSIC_REFILL_BLOCKS={refill}",
                        "-Isrc", "-Ifirmware/common",
                        "tools/audio_music_stall_host.cpp",
                        "src/core/AudioManager.cpp", "src/core/ResourcePack.cpp",
                        "src/core/ResourceFS.cpp", "src/platform/api/PlatformServices.cpp",
                        "src/platform/desktop/DesktopPlatform.cpp", "-o", str(binary),
                    ], cwd=ROOT, check=True, capture_output=True, text=True)
                    result = subprocess.run(
                        [str(binary), str(ROOT / "data"), str(stall), str(underrun)],
                        cwd=ROOT, capture_output=True, text=True)
                    self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_amoled_builds_use_the_deeper_queue(self):
        for target in ("amoled_1_8_v1", "amoled_1_8_v2"):
            text = (ROOT / "firmware" / target / "main/CMakeLists.txt").read_text()
            self.assertIn("STICKMON_MUSIC_QUEUE_TARGET_BLOCKS=3", text)
            self.assertIn("STICKMON_MUSIC_REFILL_BLOCKS=2", text)


if __name__ == "__main__":
    unittest.main()
