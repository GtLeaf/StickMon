#!/usr/bin/env python3
"""Compile public headers independently without linking the native application."""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AmoledHeaderTests(unittest.TestCase):
    def test_page_headers_are_self_contained(self):
        headers = [f"ui/{header}" for header in
                   ("RoomScreens.h", "SettingsScreens.h", "SettingsLayout.h", "UiTheme.h", "ThemedUi.h", "HatchScreens.h", "UiCommon.h",
                    "ItemShopScreens.h", "TeamScreens.h", "ComputerScreens.h",
                    "ExploreScreens.h", "ProgressionScreens.h", "ShowerScreens.h",
                    "ExploreMapRenderer.h", "RenderCaches.h", "SceneFade.h")]
        headers += ["app/HatchController.h", "app/ShowerController.h",
                    "app/BattleSession.h", "app/BattleTimeline.h",
                    "app/ExploreSession.h", "app/ExploreTransitionFlow.h", "app/ExplorePresenter.h", "app/HomeViewState.h", "app/HomeFoodFlow.h", "app/HomeSleepFlow.h", "app/HomeRestFlow.h", "app/HomePresenter.h", "app/HomeFlow.h", "app/PairFlow.h",
                    "app/VisitorFlow.h", "app/VisitAwayFlow.h"]
        for header in headers:
            with self.subTest(header=header):
                subprocess.run([
                    "c++", "-std=c++17", "-fsyntax-only", "-x", "c++",
                    "-Isrc", "-Ifirmware/amoled_1_8_v1/main", "-",
                ], input=f'#include "{header}"\n',
                   text=True, cwd=ROOT, check=True)

    def test_models_header_is_self_contained(self):
        for debug in (0, 1):
            with self.subTest(debug=debug):
                subprocess.run([
                    "c++", "-std=c++17", "-fsyntax-only", "-x", "c++",
                    "-Isrc", "-Ifirmware/amoled_1_8_v1/main",
                    f"-DSTICKMON_ENABLE_DEBUG_FEATURES={debug}", "-",
                ], input='#include "ui/models/ScreenModels.h"\n'
                         'AmoledV1::HomeViewModel model;\n',
                   text=True, cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
