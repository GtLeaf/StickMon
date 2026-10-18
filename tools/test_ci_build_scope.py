#!/usr/bin/env python3
"""Ensure scope filtering cannot omit consumers of a shared source change."""

import unittest

from ci_build_scope import ALL, AMOLED, WEB, build_targets


class BuildScopeTests(unittest.TestCase):
    def test_shared_rules_and_resources_build_every_consumer(self):
        for path in ("src/game/HomeRuntime.cpp", "src/core/SaveCodec.h", "src/assets/GameAssets.cpp",
                     "src/platform/api/PlatformServices.h", "data/packs/dev/game/ui.smonfx"):
            with self.subTest(path=path):
                self.assertEqual(build_targets([path]), ALL)

    def test_shared_amoled_pages_and_manifests_build_both_boards_and_web(self):
        for path in ("firmware/amoled_1_8_v1/main/app/SettingsController.cpp",
                     "firmware/amoled_1_8_v1/main/app/sources.txt",
                     "firmware/amoled_1_8_v1/main/ui/SettingsScreens.cpp",
                     "firmware/amoled_1_8_v1/main/TouchEvent.h",
                     "firmware/common/amoled_shared_sources.txt", "tools/amoled_source.py"):
            with self.subTest(path=path):
                self.assertEqual(build_targets([path]), AMOLED)

    def test_board_only_changes_do_not_build_unrelated_targets(self):
        self.assertEqual(build_targets(["firmware/amoled_1_8_v1/main/TouchInput.cpp"]), {"amoled-v1"})
        self.assertEqual(build_targets(["firmware/amoled_1_8_v2/main/AmoledPlatform.cpp"]), {"amoled-v2"})
        self.assertEqual(build_targets(["src/platform/m5stick_s3/M5StickS3Platform.cpp"]), {"sticks3"})
        self.assertEqual(build_targets(["src/brain/BrainBridge.cpp"]), {"amoled-v1", "amoled-v2"})

    def test_web_shells_and_platform_follow_their_targets(self):
        self.assertEqual(build_targets(["web/index.html"]), {"web-sticks3"})
        self.assertEqual(build_targets(["web/amoled-v2/index.html"]), {"web-amoled-v2"})
        self.assertEqual(build_targets(["src/platform/web/main_amoled.cpp"]), {"web-amoled-v2"})
        self.assertEqual(build_targets(["src/platform/web/WebPlatform.h"]), WEB)

    def test_combined_changes_union_targets_and_documentation_skips_builds(self):
        self.assertEqual(build_targets(["doc/方案.md", "AGENTS.md", "tools/test_home_runtime.py"]), set())
        self.assertEqual(build_targets(["src/scenes/MainScene.cpp", "firmware/amoled_1_8_v2/main/main.cpp"]),
                         {"sticks3", "web-sticks3", "amoled-v2"})
        self.assertEqual(build_targets([".github/workflows/checks.yml"]), ALL)


if __name__ == "__main__":
    unittest.main()
