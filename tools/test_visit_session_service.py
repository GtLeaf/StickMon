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
        self.assertIn("visitSession.visitorHealthKnown()", app)
        self.assertIn("communicationBefore.remote.hpCur != communicationAfter.remote.hpCur", app)
        self.assertIn("visitSession.endVisit();", app)
        self.assertIn("communicationAfter.localIsHost", app)
        self.assertIn("sceneFlow.goHome();", app)
        self.assertIn("beginVisitorExit(nowMs, false);", app)
        self.assertIn("homeCompanionActor.hidden", app)

    def test_stick_uses_shared_visit_session(self):
        scene = (ROOT / "src/scenes/SocialScene.cpp").read_text()
        engine = (ROOT / "src/core/GameEngine.cpp").read_text()
        self.assertIn("service.acceptIncoming(cursor == 0)", scene)
        self.assertIn("service.selectRoom(roomCursor)", scene)
        self.assertIn("visitSession.update(nowMs)", engine)
        self.assertIn("visitSession.takeHostRecall()", engine)
        self.assertIn("beginLinkedGuestExit(", engine)
        self.assertIn("switchScene(SceneID::MAIN)", engine)
        self.assertNotIn("sendJoinAck(joinMac, true", scene)

    def test_stick_visitor_enters_home_after_acceptance(self):
        scene = (ROOT / "src/scenes/SocialScene.cpp").read_text()
        active_transition = scene.split("if (model.state == State::ACTIVE) {", 1)[1].split("}", 1)[0]
        self.assertIn("requestScene(SceneID::MAIN)", active_transition)
        self.assertNotIn("localIsHost", active_transition)
        home = (ROOT / "src/scenes/MainScene.cpp").read_text()
        self.assertIn("!GameEngine::ins().visitAsHost()", home)
        self.assertIn("beginVisitDeparture(nowMs)", home)


if __name__ == "__main__":
    unittest.main()
