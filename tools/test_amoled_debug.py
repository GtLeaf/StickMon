#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
APP_HEADER = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.h"
HOME = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
FLOW = ROOT / "src" / "core" / "AppSceneFlow.h"
BUILD = ROOT / "tools" / "build_amoled_variant.sh"


class AmoledDebugMigrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = APP.read_text(encoding="utf-8")
        cls.app_header = APP_HEADER.read_text(encoding="utf-8")
        cls.home = HOME.read_text(encoding="utf-8")
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


if __name__ == "__main__":
    unittest.main()
