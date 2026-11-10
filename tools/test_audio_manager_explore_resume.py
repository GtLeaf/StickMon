#!/usr/bin/env python3
"""Check actual streamed PCM across exploration/battle transitions."""

import subprocess
import shutil
import tempfile
import unittest
from pathlib import Path

import generate_audio_assets as audio

ROOT = Path(__file__).resolve().parents[1]


class ExploreMusicResumeTests(unittest.TestCase):
    def test_battle_returns_to_interrupted_exploration_pcm(self):
        with tempfile.TemporaryDirectory() as directory:
            fixture = Path(directory) / "loop-fixture"
            tracks = fixture / "packs/dev/audio"
            tracks.mkdir(parents=True)
            shutil.copy2(ROOT / "data/active.json", fixture / "active.json")
            shutil.copy2(ROOT / "data/packs/dev/manifest.json", tracks.parent / "manifest.json")
            for track in (ROOT / "data/packs/dev/audio").glob("bgm_*.smonaudio"):
                shutil.copy2(track, tracks / track.name)
            # An 8 kHz track exercises interpolation state, a short final
            # block, and a loop that starts in the middle of an ADPCM block.
            samples = [((i * 271) % 40000) - 20000 for i in range(3 * audio.IMA_BLOCK_SAMPLES + 83)]
            (tracks / "bgm_explore.smonaudio").write_bytes(
                audio.make_audio_pack(samples, 8000, True, 317))
            for queue_target in (2, 3):
                with self.subTest(queue_target=queue_target):
                    binary = Path(directory) / f"resume-{queue_target}"
                    subprocess.run([
                        "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        f"-DSTICKMON_MUSIC_QUEUE_TARGET_BLOCKS={queue_target}", "-Isrc",
                        "tools/audio_manager_explore_resume_host.cpp",
                        "src/core/AudioManager.cpp", "src/core/ResourcePack.cpp",
                        "src/core/ResourceFS.cpp", "src/platform/api/PlatformServices.cpp",
                        "src/platform/desktop/DesktopPlatform.cpp", "-o", str(binary),
                    ], cwd=ROOT, check=True, capture_output=True, text=True)
                    for resources in (ROOT / "data", fixture):
                        result = subprocess.run([str(binary), str(resources)],
                                                cwd=ROOT, capture_output=True, text=True)
                        self.assertEqual(0, result.returncode, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
