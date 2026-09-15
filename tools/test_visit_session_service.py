#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class VisitSessionServiceTests(unittest.TestCase):
    def test_host_and_visitor_protocol(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "visit_session_service_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Isrc",
                    "tools/visit_session_service_host.cpp",
                    "src/core/VisitSessionService.cpp",
                    "src/game/MonsterFactory.cpp",
                    "src/game/GameRandom.cpp",
                    "src/game/Species.cpp",
                    "src/hardware/EspNowLink.cpp",
                    "src/platform/api/PlatformServices.cpp",
                    "src/platform/desktop/DesktopPlatform.cpp",
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(binary)], check=True)

    def test_amoled_communication_contract(self):
        app = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.cpp").read_text()
        screen = (ROOT / "firmware/amoled_1_8_v1/main/HomeScreen.cpp").read_text()
        self.assertIn("model.rooms[index].roomId", screen)
        self.assertIn("model.remote.satiety", screen)
        self.assertIn("model.remote.mood", screen)
        self.assertIn("model.remote.affection", screen)
        self.assertIn('"HP --"', screen)
        self.assertIn("visitSession.endVisit();", app)
        self.assertIn("communicationAfter.localIsHost", app)
        self.assertIn("sceneFlow.goHome();", app)


if __name__ == "__main__":
    unittest.main()
