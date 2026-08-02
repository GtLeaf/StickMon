#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RenderDemandTests(unittest.TestCase):
    def test_startup_sprite_load_wakes_render_demand(self):
        source = (ROOT / "src" / "core" / "GameEngine.cpp").read_text()

        self.assertIn("syncSpriteCache(1, &spriteCacheChanged)", source)
        wake = source.index("if (spriteCacheChanged) {")
        self.assertIn("invalidateScene();", source[wake:wake + 240])
        self.assertIn("scheduleSceneUpdate(nowMs);", source[wake:wake + 240])

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_render_demand_and_ui_motion_contracts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "render_demand_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "render_demand_host.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run(
                [str(binary)],
                check=True,
                capture_output=True,
                text=True,
            )


if __name__ == "__main__":
    unittest.main()
