#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
APP_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h"
HOME = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
RUNTIME = ROOT / "src" / "brain" / "StickmonClawRuntime.cpp"
FLOW = ROOT / "src" / "core" / "AppSceneFlow.h"
BUILD = ROOT / "tools" / "build_amoled_variant.sh"


class AmoledDebugMigrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = APP.read_text(encoding="utf-8")
        cls.app_header = APP_HEADER.read_text(encoding="utf-8")
        cls.home = HOME.read_text(encoding="utf-8")
        cls.runtime = RUNTIME.read_text(encoding="utf-8")
        cls.flow = FLOW.read_text(encoding="utf-8")
        cls.build = BUILD.read_text(encoding="utf-8")

    def test_debug_is_build_gated(self):
        self.assertIn("#if STICKMON_ENABLE_DEBUG_FEATURES", self.app)
        self.assertIn("STICKMON_ENABLE_DEBUG_FEATURES", self.home)
        self.assertIn('"-DSTICKMON_ENABLE_DEBUG_FEATURES=$DEBUG_FEATURES"', self.build)

    def test_main_menu_exposes_debug_entry(self):
        self.assertIn("MainMenuItem::DEBUG", self.flow)
        self.assertIn("Scene::DEBUG", self.flow)
        self.assertIn("entry.target == AppSceneFlow::Scene::DEBUG", self.app)

    def test_all_debug_categories_and_actions_are_present(self):
        for category in (
            "MONSTER",
            "RESOURCE",
            "ENV",
            "MOTION",
            "BATTLE",
            "CONTACT_EVENT",
        ):
            self.assertIn(f"DebugViewModel::Category::{category}", self.app)
        for action in (
            "monster.fainted = false",
            "ExperienceService::add",
            "openDebugSwitchPopup",
            "openDebugTimePopup",
            "debugLightSource",
            "debugBattleRequested",
            "acceptDebugContact",
            "completeDebugContact",
        ):
            self.assertIn(action, self.app)
        self.assertNotIn("AMOLED has no local-contact scene yet", self.app)

    def test_debug_contact_uses_visitor_lifecycle(self):
        self.assertIn("Origin::VISITOR", self.app)
        self.assertIn("ContactRoster::sameMonster", self.app)
        self.assertIn("Ui::ContactVisit::KNOCK", self.app)
        self.assertIn("debugContactChoiceAt", self.app)
        self.assertIn("Game::GameState persistentState = gameState", self.app)
        self.assertIn("persistentState.team[slot].origin", self.app)

    def test_environment_and_boundary_debug_are_rendered(self):
        self.assertIn("drawDebugLight", self.home)
        self.assertIn("drawDebugWalkBoundary", self.home)
        self.assertIn("model.debugLightSource", self.app)
        self.assertIn("model.debugBoundaryVisible", self.app)

    def test_motion_debug_is_connected_to_runtime_behavior(self):
        self.assertIn("Platform::imu().readAcceleration", self.app)
        self.assertIn("startDebugPairChase", self.app)
        self.assertIn("updateDebugPairChase", self.app)
        self.assertIn("drawDebugPairChaser", self.home)

    def test_battle_bounds_toggle_is_consumed_by_renderer(self):
        self.assertIn("model.debugDrawBounds = debugBattleDrawBoundsVisible", self.app)
        self.assertIn("if (model.debugDrawBounds)", self.home)

    def test_amoled_debug_text_uses_native_font_and_row_centering(self):
        start = self.home.index("void renderDebugScreen(")
        end = self.home.index("#endif", start)
        render = self.home[start:end]
        self.assertIn("DEBUG_TEXT_Y_OFFSET = 8", self.home)
        self.assertIn("y + DEBUG_TEXT_Y_OFFSET", render)
        self.assertIn("canvas.coordinateScale() >= 2", self.home)
        self.assertIn("PixelRenderer::text(canvas, x, y, value, color, 1);", self.home)

    def test_computer_menu_has_compact_geometry(self):
        start = self.home.index("int computerItemAt(")
        end = self.home.index("}  // namespace AmoledV1", start)
        computer = self.home[start:end]
        self.assertIn("COMPUTER_MENU_ROW_HEIGHT = 43", self.home)
        self.assertIn("COMPUTER_MENU_CELL_HEIGHT = 39", self.home)
        self.assertIn("/ COMPUTER_MENU_ROW_HEIGHT", computer)
        self.assertIn("index * COMPUTER_MENU_ROW_HEIGHT", computer)
        self.assertNotIn("index * MENU_ROW_HEIGHT", computer)

    def test_ai_hosting_menu_owns_claw_and_wifi_controls(self):
        self.assertIn('Ui::Amoled::AI_HOSTING, Ui::BACK', self.home)
        self.assertIn('Ui::Amoled::WIFI, Ui::Amoled::ESP_CLAW', self.home)
        self.assertIn('Ui::Amoled::BACKEND, Ui::BACK', self.home)
        self.assertIn('Page::AI_HOSTING', self.app)
        self.assertIn('if (item == 0)', self.app)
        self.assertIn('claw.setWifiEnabled(!claw.wifiEnabled())', self.app)
        self.assertIn('claw.setEnabled(!claw.enabled())', self.app)
        self.assertIn('claw.wifiEnabled()', self.app)

    def test_backend_requires_wifi_and_uses_backend_title(self):
        self.assertIn('Ui::Amoled::BACKEND : Ui::COMPUTER', self.home)
        self.assertIn('Ui::Amoled::CLAW_WIFI_REQUIRED', self.app)
        self.assertIn('后台需要启用 ESP-Claw 和 Wi-Fi', self.runtime)


if __name__ == "__main__":
    unittest.main()
