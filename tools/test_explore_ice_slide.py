#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ExploreIceSlideTests(unittest.TestCase):
    def test_slide_direction_and_event_placement_helpers(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            executable = Path(temp_dir) / "explore_ice_slide_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Isrc",
                    "tools/explore_ice_slide_host.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(executable)], check=True)

    def test_scene_locks_input_and_defers_events_while_sliding(self):
        source = (ROOT / "src" / "scenes" / "ExploreScene.cpp").read_text()
        self.assertIn("phase == Phase::WALKING && iceSliding", source)
        self.assertIn("ExploreIceSlide::continues", source)
        self.assertIn("ExploreIceSlide::nearestNonIceIndex", source)
        slide_resolution = source.index("if (iceSliding) {", source.index(
            "void ExploreScene::finishCompletedWalkStep()"
        ))
        boss_resolution = source.index("if (routeBossPending", slide_resolution)
        pickup_resolution = source.index("if (collectRoutePickup())", slide_resolution)
        encounter_resolution = source.index("rollRandomEncounter(", slide_resolution)
        self.assertLess(slide_resolution, boss_resolution)
        self.assertLess(slide_resolution, pickup_resolution)
        self.assertLess(slide_resolution, encounter_resolution)


if __name__ == "__main__":
    unittest.main()
