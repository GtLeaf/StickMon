#!/usr/bin/env python3

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class RadioExclusivityTests(unittest.TestCase):
    def test_all_boards_use_shared_esp_now_channel(self):
        config = (ROOT / "src/hardware/EspNowRadioConfig.h").read_text()
        self.assertIn("constexpr uint8_t CHANNEL = 6", config)

        sources = [
            ROOT / "firmware/amoled_1_8_v1/main/AmoledPlatform.cpp",
            ROOT / "firmware/amoled_1_8_v2/main/AmoledPlatform.cpp",
            ROOT / "src/platform/m5stick_s3/M5StickS3Platform.cpp",
        ]
        for source in sources:
            text = source.read_text()
            self.assertIn('"hardware/EspNowRadioConfig.h"', text)
            self.assertIn("esp_wifi_set_channel(EspNowRadioConfig::CHANNEL", text)
            self.assertIn("WIFI_PS_NONE", text)

    def test_amoled_visit_owns_and_releases_claw_network(self):
        app = (ROOT / "firmware/amoled_1_8_v1/main/AmoledApp.cpp").read_text()
        self.assertIn("enterPeerSession()", app)
        self.assertIn("leavePeerSession()", app)
        self.assertIn("EspNowLink::ins().end()", app)
        self.assertIn("communicationAfter.state == VisitState::FAILED", app)
        self.assertIn("communicationAfter.state == VisitState::ENDED", app)

        claw = (ROOT / "src/brain/StickmonClawRuntime.cpp").read_text()
        self.assertIn("s_wifiReconnectSuppressed", claw)
        self.assertIn("if (peerSession)", claw)
        self.assertIn("if (suppressed)", claw)
        self.assertIn("玩家联机，Wi-Fi 网络已暂停", claw)


if __name__ == "__main__":
    unittest.main()
