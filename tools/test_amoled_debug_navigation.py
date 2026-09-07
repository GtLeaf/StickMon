#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"


class AmoledDebugNavigationTests(unittest.TestCase):
    def test_root_debug_back_returns_to_main_menu(self):
        source = APP.read_text(encoding="utf-8")
        start = source.index("void AmoledApp::handleDebugTap(")
        end = source.index("void AmoledApp::clampMenuScroll(", start)
        handler = source[start:end]
        root_back = handler[handler.index("if (debugBackAt") :]
        self.assertIn(
            "sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);", root_back
        )
        self.assertNotIn("sceneFlow.closeMenu();", root_back)

    def test_debug_subcategory_back_still_returns_to_root(self):
        source = APP.read_text(encoding="utf-8")
        start = source.index("if (debugBackAt")
        end = source.index("int item = debugItemAt", start)
        handler = source[start:end]
        self.assertIn(
            "debugCategory = DebugViewModel::Category::ROOT;", handler
        )

    def test_root_bottom_back_returns_to_main_menu(self):
        source = APP.read_text(encoding="utf-8")
        start = source.index("void AmoledApp::executeDebugAction(")
        end = source.index("#endif", start)
        action = source[start:end]
        self.assertIn(
            "sceneFlow.enter(AppSceneFlow::Scene::MAIN_MENU);", action
        )
        self.assertNotIn("sceneFlow.closeMenu();", action[action.index("default:") :])


if __name__ == "__main__":
    unittest.main()
