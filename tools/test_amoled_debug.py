#!/usr/bin/env python3

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest

from amoled_source import read_home_source


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
        cls.home = read_home_source(ROOT)
        cls.runtime = RUNTIME.read_text(encoding="utf-8")
        cls.flow = FLOW.read_text(encoding="utf-8")
        cls.build = BUILD.read_text(encoding="utf-8")

    def test_debug_is_build_gated(self):
        self.assertIn("#if STICKMON_ENABLE_DEBUG_FEATURES", self.app)
        self.assertIn("STICKMON_ENABLE_DEBUG_FEATURES", self.home)
        self.assertIn('"-DSTICKMON_ENABLE_DEBUG_FEATURES=$DEBUG_FEATURES"', self.build)

    def test_prebuilt_amoled_assets_do_not_require_pillow(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ("tools/build_amoled_variant.sh", "tools/validate_amoled_ui_assets.py",
                             "src/assets/GameAssets.h", "data/packs/dev/game/ui.smonfx",
                             "data/packs/dev/game/ui_amoled.smonfx"):
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, destination)
            shared = root / "data/packs/dev/game/ui.smonfx"
            native = shared.with_name("ui_amoled.smonfx")
            os.utime(native, (shared.stat().st_mtime + 1, shared.stat().st_mtime + 1))
            items = root / "essentials/Graphics/Items"
            items.mkdir(parents=True)
            sdk = root / "idf"
            sdk.mkdir()
            (sdk / "export.sh").write_text("# Test SDK: compilation is replaced with a probe.\n")
            binary = root / "bin"
            binary.mkdir()
            python_args = root / "python-args.txt"
            idf_args = root / "idf-args.txt"
            scripts = {
                "python3": '#!/bin/sh\nprintf "%s\\n" "$*" >> "$STICKMON_TEST_PYTHON_ARGS"\n'
                           f'exec {shlex.quote(sys.executable)} -S "$@"\n',
                "idf.py": '#!/bin/sh\nprintf "%s\\n" "$*" > "$STICKMON_TEST_IDF_ARGS"\n',
            }
            for name, source in scripts.items():
                path = binary / name
                path.write_text(source)
                path.chmod(0o755)
            env = dict(os.environ, IDF_PATH=str(sdk), IDF_PYTHON_ENV_PATH=str(sdk / "python"),
                       STICKMON_ESSENTIALS_DIR=str(root / "essentials"),
                       STICKMON_TEST_PYTHON_ARGS=str(python_args), STICKMON_TEST_IDF_ARGS=str(idf_args),
                       PATH=str(binary) + os.pathsep + os.environ["PATH"])
            env.pop("PYTHONPATH", None)
            no_pillow = subprocess.run([str(binary / "python3"), "-c", "import PIL"],
                                       env=env, capture_output=True)
            self.assertNotEqual(no_pillow.returncode, 0)
            python_args.unlink()
            for board in ("v1", "v2"):
                with self.subTest(board=board):
                    (root / f"firmware/amoled_1_8_{board}").mkdir(parents=True)
                    result = subprocess.run(["bash", str(root / "tools/build_amoled_variant.sh"),
                                             board, "lite", "debug"], env=env,
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("-DSTICKMON_ENABLE_CLAW=OFF", idf_args.read_text())
                    self.assertIn("build-lite-debug", idf_args.read_text())
            calls = python_args.read_text()
            self.assertIn("validate_amoled_ui_assets.py", calls)
            self.assertNotIn("import PIL", calls)
            self.assertNotIn("generate_amoled_ui_assets.py", calls)

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
        self.assertIn("Ui::ContactVisit::PLAY_FMT", self.app)
        self.assertIn("debugPromptBuffer", self.app)
        self.assertIn("debugContactPromptFade", self.app)
        self.assertIn("debugContactChoiceAt", self.app)
        self.assertIn("Game::GameState persistentState = gameState", self.app)
        self.assertIn("persistentState.team[slot].origin", self.app)

    def test_contact_copy_follows_stick_dialog_order(self):
        begin = self.app[self.app.index("bool AmoledApp::beginDebugContactEvent"):
                         self.app.index("void AmoledApp::renderDebugTouchOverlay")]
        self.assertIn("Ui::ContactVisit::KNOCK", begin)
        accept = self.app[self.app.index("void AmoledApp::acceptDebugContact"):
                          self.app.index("void AmoledApp::completeDebugContact")]
        self.assertIn("debugContactEventWaitingForEntry = !autoResolve", accept)
        self.assertIn("Ui::ContactVisit::EXPLORE_FMT", accept)
        home_tap = self.app[self.app.index("void AmoledApp::handleTap"):
                            self.app.index("void AmoledApp::handleDebugTap")]
        self.assertNotIn("setToast(Ui::ContactVisit::BYE_VISIT", home_tap)
        self.assertIn("if (choice == 0 && debugContactKind == 3)", home_tap)

    def test_contact_choice_bubbles_only_render_while_choosing(self):
        self.assertIn("debugContactChoiceVisible", self.app)
        self.assertIn("debugContactSelectedChoice", self.app)
        self.assertIn("debugContactChoiceConfirmUntilMs = nowMs + 100", self.app)
        self.assertIn("PROMPT_FADE_STEP = 40", self.app)
        prompt = self.home[
            self.home.index("void drawDebugContactPrompt"):
            self.home.index("void drawDebugContactGuest")
        ]
        self.assertIn("if (!model.debugContactChoiceVisible) return;", prompt)
        self.assertIn("model.debugContactSelectedChoice != index", prompt)

    def test_serial_contact_diagnostics_follow_debug_events(self):
        main = (ROOT / "firmware/amoled_1_8_v2/main/main.cpp").read_text(
            encoding="utf-8")
        visitor_capture = (ROOT / "tools/capture_amoled_v2_visitor.py").read_text(
            encoding="utf-8")
        self.assertIn("beginDebugContactEvent(static_cast<uint8_t>(debugCursor + 1)",
                      self.app)
        self.assertIn("if (!beginDebugContactEvent(kind, nowMs)) return false;",
                      self.app)
        self.assertIn("sleepSafeScene = sleepSafeScene && !debugContactPending &&",
                      self.app)
        self.assertIn("onWake(nowMs);\n    acceptDebugContact(nowMs, true);", self.app)
        self.assertIn("[FriendDiag] route kind=3 started=", self.app)
        self.assertIn("[FriendDiag] complete kind=", self.app)
        self.assertIn('"diag contact return"', main)
        self.assertIn("app.debugTriggerContact(kind)", main)
        self.assertIn("app.debugPromptContact(kind)", main)
        self.assertIn("app.debugAcceptContact()", main)
        self.assertIn("app.debugStartPairTalk()", main)
        self.assertNotIn('"talk": "diag pair talk\\n"', visitor_capture)
        self.assertIn('phase = "waiting-talk"', visitor_capture)
        self.assertIn("[FriendDiag] visitor-exit route-fallback", self.app)
        self.assertIn("[FriendDiag] visitor-exit route-end", self.app)
        self.assertIn("[FriendDiag] talk positioned", self.app)

    def test_environment_and_boundary_debug_are_rendered(self):
        self.assertIn("drawDebugLight", self.home)
        self.assertIn("drawDebugWalkBoundary", self.home)
        self.assertIn("model.debugLightSource", self.app)
        self.assertIn("model.debugBoundaryVisible", self.app)

    def test_talk_debug_marks_bottom_center_without_labels(self):
        start = self.home.index("void drawDebugTalkPoints(")
        end = self.home.index("#endif", start)
        markers = self.home[start:end]
        self.assertIn("model.debugWelcomeCenterX", markers)
        self.assertIn("model.debugWelcomeGroundY", markers)
        self.assertNotIn("text(canvas", markers)
        self.assertIn("anchor=bottom-center", self.app)
        self.assertIn("errorPx=%.2f", self.app)

    def test_motion_debug_is_connected_to_runtime_behavior(self):
        self.assertIn("Platform::imu().readAcceleration", self.app)
        self.assertIn("startDebugPairChase", self.app)
        self.assertIn("updateDebugPairChase", self.app)
        self.assertIn("drawDebugPairChaser", self.home)

    def test_battle_bounds_toggle_is_consumed_by_renderer(self):
        self.assertIn("model.debugDrawBounds = debugBattleDrawBoundsVisible", self.app)
        self.assertIn("if (model.debugDrawBounds)", self.home)

    def test_touch_display_keeps_only_latest_native_touch(self):
        strings = (ROOT / "src" / "core" / "UiStrings.h").read_text(
            encoding="utf-8"
        )
        v1_main = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "main.cpp").read_text(
            encoding="utf-8"
        )
        v2_main = (ROOT / "firmware" / "amoled_1_8_v2" / "main" / "main.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn('TOUCH_DISPLAY = "点击显示"', strings)
        self.assertIn("debugTouchDisplayEnabled = !debugTouchDisplayEnabled", self.app)
        self.assertIn("debugTouchX = std::clamp<int16_t>(event.x", self.app)
        self.assertIn("debugTouchY = std::clamp<int16_t>(event.y", self.app)
        self.assertIn("[TouchDisplay] x=%d y=%d", self.app)
        self.assertIn("void AmoledApp::renderDebugTouchOverlay", self.app)
        self.assertNotIn("debugTouchPoints[", self.app_header)
        for main in (v1_main, v2_main):
            self.assertIn("app.renderDebugTouchOverlay(canvas);", main)

    def test_touch_diagnostic_page_records_down_up_without_calibrating(self):
        touch_test = (ROOT / "firmware" / "amoled_1_8_v1" / "main" / "TouchTest.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("TARGETS[]", touch_test)
        self.assertIn("Point down", touch_test)
        self.assertIn("Point up", touch_test)
        self.assertIn("distance", self.app)
        self.assertIn("diagnostic_only=1", self.app)
        self.assertIn("handleDebugTouchTest", self.app)
        self.assertNotIn("correctedX", touch_test)
        self.assertNotIn("NVS", touch_test)

    def test_amoled_debug_text_uses_native_font_and_row_centering(self):
        start = self.home.index("void renderDebugScreen(")
        end = self.home.index("#endif", start)
        render = self.home[start:end]
        self.assertIn("DEBUG_TEXT_Y_OFFSET = 16", self.home)
        self.assertIn("y + DEBUG_TEXT_Y_OFFSET", render)
        common = (HOME.parent / "ui" / "UiCommon.cpp").read_text(encoding="utf-8")
        self.assertIn("PixelRenderer::text(canvas, x, y, value, color, scale);", common)
        self.assertNotIn("((x) * AmoledUi::RESOURCE_SCALE)", self.home)

    def test_native_text_uses_large_glyph_advances(self):
        renderer = (ROOT / "src" / "presentation" / "PixelRenderer.cpp").read_text(
            encoding="utf-8"
        )
        start = renderer.index("int nativeGlyphAdvance(")
        end = renderer.index("void drawTextPass(", start)
        advance = renderer[start:end]
        self.assertIn("ASCII_CELL_WIDTH * NATIVE_FONT_SCALE", advance)
        self.assertIn("FontResource::LARGE_GLYPH_W", advance)
        native_pass = renderer[renderer.index("void drawNativeTextPass(") :]
        self.assertIn("FontResource::LARGE_GLYPH_H", native_pass)
        self.assertIn("nativeGlyphAdvance(target, codepoint)", native_pass)

    def test_computer_menu_has_compact_geometry(self):
        start = self.home.index("int computerItemAt(")
        end = self.home.index("}  // namespace AmoledV1", start)
        computer = self.home[start:end]
        self.assertIn("COMPUTER_MENU_ROW_HEIGHT = 86", self.home)
        self.assertIn("COMPUTER_MENU_CELL_HEIGHT = 78", self.home)
        self.assertIn("/ COMPUTER_MENU_ROW_HEIGHT", computer)
        self.assertIn("index * COMPUTER_MENU_ROW_HEIGHT", computer)
        self.assertNotIn("index * MENU_ROW_HEIGHT", computer)

    def test_ai_hosting_menu_owns_claw_and_wifi_controls(self):
        self.assertIn('Ui::SOCIAL, Ui::Amoled::STORAGE_PAGE', self.home)
        self.assertIn('Ui::Amoled::AI_HOSTING,', self.home)
        self.assertIn('Ui::Amoled::WIFI, Ui::Amoled::ESP_CLAW', self.home)
        self.assertIn('Ui::Amoled::BACKEND, Ui::BACK', self.home)
        self.assertIn('computerPage = ComputerViewModel::Page::STORAGE', self.app)
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
