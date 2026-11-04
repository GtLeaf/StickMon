#!/usr/bin/env python3
"""Verify native scene targets select one UI family and share feature flows."""

import configparser
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from build_web import ROOT, sources_for, ui_flags_for

OSTB_CMAKE = ROOT / "firmware/ostb_3st/main/CMakeLists.txt"


def relative_sources(target):
    return {p.relative_to(ROOT).as_posix() for p in sources_for(target)}


class UiFamilySourceTests(unittest.TestCase):
    def test_scene_targets_compile_only_the_selected_family(self):
        for target, selected, excluded in (
                ("sticks3", "stick", "compact_touch"),
                ("ostb", "compact_touch", "stick")):
            with self.subTest(target=target):
                sources = sources_for(target)
                self.assertEqual(len(sources), len(set(sources)))
                self.assertTrue(all(p.is_file() for p in sources))
                paths = relative_sources(target)
                for page in ("ShopUi.cpp", "BagUi.cpp", "TeamUi.cpp", "StatusUi.cpp", "MovesUi.cpp",
                             "RoomUi.cpp", "FoodUi.cpp", "ComputerUi.cpp", "ContactsUi.cpp", "SettingsUi.cpp",
                             "MainMenuUi.cpp", "HomeUi.cpp", "ExploreUi.cpp", "ShowerUi.cpp", "SocialUi.cpp", "DebugUi.cpp", "ProgressionUi.cpp",
                             "HatchUi.cpp", "MenuOverlayUi.cpp", "ApplicationUi.cpp"):
                    self.assertIn(f"src/ui/{selected}/{page}", paths)
                    self.assertNotIn(f"src/ui/{excluded}/{page}", paths)
                self.assertFalse(any(p.startswith(f"src/ui/{excluded}/") for p in paths))
                self.assertNotIn("src/presentation/ItemTouchUi.cpp", paths)
                self.assertEqual(target == "ostb", "src/ui/compact_touch/ItemTouchUi.cpp" in paths)
                self.assertEqual(target == "ostb", "src/ui/compact_touch/ExploreEcologyUi.cpp" in paths)
                self.assertEqual(target == "ostb", "src/ui/compact_touch/ExploreHudUi.cpp" in paths)
                self.assertIn("src/core/features/ShopFlow.cpp", paths)
                self.assertIn("src/core/features/BagFeature.cpp", paths)
                self.assertIn("src/core/features/TeamFeature.cpp", paths)
                self.assertIn("src/core/features/MovesFeature.cpp", paths)
                self.assertIn("src/core/features/RoomFeature.cpp", paths)
                self.assertIn("src/core/features/ContactsFeature.cpp", paths)
                self.assertIn("src/ui/common/ShopUiBase.cpp", paths)
                self.assertIn("src/ui/common/StatusUiContent.cpp", paths)
                self.assertIn("src/ui/common/MovesUiBase.cpp", paths)
                self.assertIn("src/ui/common/RoomUiBase.cpp", paths)
                self.assertIn("src/ui/common/ComputerUiBase.cpp", paths)
                self.assertIn("src/ui/common/ContactsUiBase.cpp", paths)
                self.assertIn("src/ui/common/ContactsUiContent.cpp", paths)
                self.assertIn("src/ui/common/SettingsUiBase.cpp", paths)
                self.assertIn("src/ui/common/MainMenuUiBase.cpp", paths)
                self.assertIn("src/ui/common/HomeUiBase.cpp", paths)
                self.assertIn("src/ui/common/HomeUiContent.cpp", paths)
                self.assertIn("src/ui/common/ExploreUiBase.cpp", paths)
                self.assertIn("src/ui/common/ExploreRouteUiContent.cpp", paths)
                self.assertIn("src/ui/common/ExploreBattleUiContent.cpp", paths)
                self.assertIn("src/ui/common/ShowerUiBase.cpp", paths)
                self.assertIn("src/ui/common/ShowerUiContent.cpp", paths)
                self.assertIn("src/ui/common/SocialUiBase.cpp", paths)
                self.assertIn("src/ui/common/SocialUiContent.cpp", paths)
                self.assertIn("src/ui/common/DebugUiBase.cpp", paths)
                self.assertIn("src/ui/common/ProgressionUiBase.cpp", paths)
                self.assertIn("src/ui/common/HatchUiBase.cpp", paths)
                self.assertIn("src/ui/common/HatchUiContent.cpp", paths)
                self.assertIn("src/ui/common/MenuOverlayUiBase.cpp", paths)
                self.assertIn("src/ui/common/ApplicationUiContent.cpp", paths)

    def test_targets_share_all_sources_outside_ui_families(self):
        def shared(target):
            return {p for p in relative_sources(target)
                    if not p.startswith(("src/ui/stick/", "src/ui/compact_touch/"))}
        self.assertEqual(shared("sticks3"), shared("ostb"))
        amoled = relative_sources("amoled-v2")
        self.assertFalse(any(p.startswith(("src/ui/", "src/core/features/")) for p in amoled))

    def test_firmware_builds_use_matching_selection(self):
        config = configparser.ConfigParser(interpolation=None)
        config.read(ROOT / "platformio.ini")
        source_filter = config["env:m5stick-s3"]["build_src_filter"].split()
        self.assertIn("-<ui/>", source_filter)
        self.assertIn("+<ui/common/>", source_filter)
        self.assertIn("+<ui/stick/>", source_filter)
        self.assertNotIn("+<ui/compact_touch/>", source_filter)
        for env in ("env:m5stick-s3", "env:m5stick-s3-debug"):
            self.assertIn("-DSTICKMON_UI_COMPACT_TOUCH=0", config[env]["build_flags"].split())

    @unittest.skipUnless(OSTB_CMAKE.is_file(), "OSTB firmware is local-only")
    def test_local_ostb_build_uses_matching_selection(self):
        cmake = OSTB_CMAKE.read_text()
        directories = re.search(r"foreach\(DIR\s+([^)]*)\)", cmake)[1].split()
        self.assertIn("core/features", directories)
        self.assertIn("ui/common", directories)
        self.assertIn("ui/compact_touch", directories)
        self.assertNotIn("ui/stick", directories)
        for flag in ui_flags_for("ostb"):
            self.assertIn(flag.removeprefix("-D"), cmake)

    def test_unknown_target_is_rejected(self):
        with self.assertRaises(ValueError):
            sources_for("typo")

    @unittest.skipUnless(shutil.which("cmake"), "CMake is unavailable")
    @unittest.skipUnless(OSTB_CMAKE.is_file(), "OSTB firmware is local-only")
    def test_ostb_cmake_compiles_the_same_source_set_as_the_host(self):
        with tempfile.TemporaryDirectory() as folder:
            base = Path(folder)
            script = base / "sources.cmake"
            script.write_text(
                'macro(idf_component_register)\n'
                '  cmake_parse_arguments(component "" "" "SRCS;INCLUDE_DIRS;REQUIRES" ${ARGN})\n'
                f'  file(WRITE "{base}/sources.txt" "${{component_SRCS}}")\n'
                '  return()\n'
                'endmacro()\n'
                f'include("{ROOT}/firmware/ostb_3st/main/CMakeLists.txt")\n')
            subprocess.run(["cmake", "-P", str(script)],
                           check=True, capture_output=True, text=True)
            paths = (base / "sources.txt").read_text().split(";")
            resolved = [Path(p).resolve() for p in paths if Path(p).is_absolute()]
            native = [p for p in resolved if p.is_relative_to(ROOT / "src")]
            host = [p for p in sources_for("ostb") if "/platform/web/" not in str(p)]
            self.assertEqual(len(native), len(set(native)))
            self.assertEqual(set(host), set(native))

    def test_features_do_not_depend_on_ui_or_hardware(self):
        forbidden = re.compile(
            r'#\s*include\s*[<"](?:ui/|presentation/|scenes/|hardware/|platform/|'
            r'Arduino\.h|M5|esp_|driver/|freertos/)')
        for source in (ROOT / "src/core/features").glob("*"):
            if source.suffix in (".h", ".cpp"):
                self.assertIsNone(forbidden.search(source.read_text()), str(source.relative_to(ROOT)))
        for family, opposite in (("stick", "compact_touch"), ("compact_touch", "stick")):
            for source in (ROOT / "src/ui" / family).glob("*"):
                if source.suffix in (".h", ".cpp"):
                    self.assertNotIn(f'"ui/{opposite}/', source.read_text())

    def test_migrated_pages_are_delegated_and_do_not_own_application_state(self):
        scene = (ROOT / "src/scenes/MenuScene.cpp").read_text()
        header = (ROOT / "src/scenes/MenuScene.h").read_text()
        for page in ("Team", "Status", "Moves", "Room", "Food", "Computer", "Contacts"):
            self.assertIn(f"Ui::{page}Ui {page.lower()}Ui;", header)
            self.assertNotIn(f"MenuScene::render{page}Page", scene)
        for helper in ("touchTeamLayout", "touchTeamPopup", "touchStatusBody",
                       "touchMoveLayout", "touchForgetPopup", "statusSlideFrames",
                       "touchRoomLayout", "foodCursor", "foodScroll", "roomCursor", "descScroll",
                       "computerCursor", "storageCursor", "storageActionCursor", "storageScroll",
                       "storageActionOpen", "storageInviteConfirm", "storageReleaseConfirm", "pageScroll",
                       "renderStorage", "renderTouchComputer"):
            self.assertNotIn(helper, scene + header)
        self.assertIn("case ViewMode::COMPUTER: return &computerUi;", scene)
        self.assertIn("case ViewMode::STORAGE: return &contactsUi;", scene)
        self.assertIn("Ui::MainMenuUi mainMenuUi;", header)
        self.assertIn("case ViewMode::MENU: return &mainMenuUi;", scene)
        self.assertNotIn("MenuScene::renderMenu", scene)
        for helper in ("animCursor", "menuGridScroll", "gridTouch", "lastCursor", "lastBatteryLevel"):
            self.assertNotIn(helper, scene + header)
        settings = (ROOT / "src/scenes/SettingsScene.cpp").read_text()
        settings_header = (ROOT / "src/scenes/SettingsScene.h").read_text()
        self.assertIn("Ui::SettingsUi ui;", settings_header)
        self.assertIn("return ui.onTouch(event);", settings)
        for helper in ("TouchScroll", "menuScroll", "touchScroll", "sliderCandidate", "sliderDragging",
                       "renderMenu", "renderTouch", "renderHelp", "renderResetConfirm", "PixelRenderer", "TouchUi"):
            self.assertNotIn(helper, settings + settings_header)
        home = (ROOT / "src/scenes/MainScene.cpp").read_text()
        home_header = (ROOT / "src/scenes/MainScene.h").read_text()
        self.assertIn("Ui::HomeUi homeUi;", home_header)
        self.assertIn("Ui::HomeFrame MainScene::homeFrame() const", home)
        for call in ("homeUi.render()", "homeUi.onTouch(event)", "homeUi.onButton(event)",
                     "homeUi.touchTarget(x, y)", "homeUi.onTap(target)"):
            self.assertIn(call, home)
        for helper in ("PixelRenderer", "TouchUi", "cameraX", "cameraY", "RenderItem",
                       "MainScene::drawHud", "MainScene::drawMonster", "MainScene::drawShadow",
                       "MainScene::drawContactDialog", "MainScene::sortAndDraw"):
            self.assertNotIn(helper, home + home_header)
        explore = (ROOT / "src/scenes/ExploreScene.cpp").read_text()
        explore_header = (ROOT / "src/scenes/ExploreScene.h").read_text()
        self.assertIn("Ui::ExploreUi ui;", explore_header)
        for frame in ("Input", "Route", "Battle", "Tutorial"):
            self.assertIn(f"Ui::Explore{frame}Frame ExploreScene::explore{frame}Frame() const", explore)
        for call in ("ui.render()", "ui.onButton(event)", "ui.updateAreaCursor("):
            self.assertIn(call, explore)
        for helper in ("PixelRenderer", "TouchUi", "Hal::DISPLAY_W", "Hal::DISPLAY_H",
                       "ExploreScene::renderAreaMenu", "ExploreScene::renderWalking",
                       "ExploreScene::renderEncounter", "ExploreScene::renderFoodThrow",
                       "ExploreScene::renderBattleHud", "ExploreScene::renderCommandBox",
                       "ExploreScene::drawRouteMonster", "ExploreScene::drawMonsterSprite",
                       "ExploreScene::battleHpForRender", "ExploreScene::battleHitShakeX",
                       "ExploreScene::battleSwitchOffsetX", "ExploreScene::battleExpForRender"):
            self.assertNotIn(helper, explore + explore_header)
        for state in ("areaCursor", "areaAnimCursor", "areaPreviewVisualCycle", "battleCursor",
                      "exploreMenuCursor", "friendshipConfirmYes", "specialChallengeYes",
                      "routeVisualWalkDirection", "routeFollowerVisualWalkDirection"):
            self.assertNotRegex(explore_header, rf"\b(?:bool|uint8_t|uint32_t|float)\s+{state}\b")
        # The scene routes raw gestures; only the selected UI maps coordinates.
        for touch_hook in ("exploreSubView.onTouch(event)", "exploreSubView.cancelTouch()",
                           "finishSubViewInput()"):
            self.assertIn(touch_hook, explore)
        self.assertIn("exploreSubView.touchNavigation()", explore_header)
        for touch_hook in ("ui.touchTarget(x, y)", "ui.onTap(target)"):
            self.assertIn(touch_hook, explore_header)
        self.assertIn("exploreSubViewOpen ? -1", explore_header)
        for shared in ("ExploreUiBase", "ExploreRouteUiContent", "ExploreBattleUiContent"):
            source = (ROOT / f"src/ui/common/{shared}.cpp").read_text()
            self.assertNotIn("scenes/", source)
        shower = (ROOT / "src/scenes/ShowerScene.cpp").read_text()
        shower_header = (ROOT / "src/scenes/ShowerScene.h").read_text()
        self.assertIn("Ui::ShowerUi ui;", shower_header)
        for call in ("ui.render()", "ui.onButton(event)"):
            self.assertIn(call, shower)
        for helper in ("PixelRenderer", "Hal::DISPLAY_W", "Hal::DISPLAY_H", "readAccel(",
                       "ShowerScene::drawAtmosphere", "ShowerScene::drawMonster",
                       "ShowerScene::drawFoam", "ShowerScene::drawWater", "ShowerScene::drawTool",
                       "ShowerScene::drawMenu", "ShowerScene::drawSoapPicker",
                       "ShowerScene::drawHearts", "ShowerScene::drawExitConfirm",
                       "ShowerScene::drawIncomplete", "ShowerScene::drawExpFloats",
                       "ShowerScene::drawToast"):
            self.assertNotIn(helper, shower + shower_header)
        for state in ("menuCursor", "menuAnimCursor", "soapCursor", "exitConfirmYes",
                      "gravityX", "gravityY", "gravityZ", "accelSeeded",
                      "soapX", "soapY", "brushX", "brushY", "waterSpawnCarry",
                      "hopStartMs", "hopHeightPx", "wiggleStartMs", "turnUntilMs",
                      "turnFlip", "atmosphereAlpha", "toastUntilMs"):
            self.assertNotRegex(shower_header, rf"\b(?:bool|uint8_t|uint32_t|float)\s+{state}\b")
        # Touch layout stays in the selected UI; the scene forwards semantic taps.
        for touch_hook in ("ui.touchTarget(x, y)", "ui.onTap(target)"):
            self.assertIn(touch_hook, shower_header)
        self.assertIn("ui.onTouch(event)", shower_header)
        self.assertIn("ui.cancelTouch()", shower_header)
        for shared in ("ShowerUiBase", "ShowerUiContent"):
            source = (ROOT / f"src/ui/common/{shared}.cpp").read_text()
            self.assertNotIn("scenes/", source)
        social = (ROOT / "src/scenes/SocialScene.cpp").read_text()
        social_header = (ROOT / "src/scenes/SocialScene.h").read_text()
        self.assertIn("Ui::SocialUi ui;", social_header)
        for call in ("ui.render()", "ui.onButton(event)", "ui.update(model, nowMs, demand)"):
            self.assertIn(call, social)
        for helper in ("PixelRenderer", "Hal::DISPLAY_W", "Hal::DISPLAY_H", "renderMenu",
                       "renderRoomList", "renderStatus", "lastVisualTick", "stateStartedMs"):
            self.assertNotIn(helper, social + social_header)
        for state in ("cursor", "roomCursor", "lastRoomCount"):
            self.assertNotRegex(social_header, rf"\buint8_t\s+{state}\b")
        for touch_hook in ("ui.onTouch(event)", "ui.touchNavigation()", "ui.cancelTouch()"):
            self.assertIn(touch_hook, social_header)
        for helper in ("TouchTap", "TouchUi", "MenuGrid", "PixelRenderer"):
            self.assertNotIn(helper, social_header)
        for shared in ("SocialUiBase", "SocialUiContent"):
            content = (ROOT / f"src/ui/common/{shared}.cpp").read_text()
            self.assertNotIn("scenes/", content)
            for effect in ("EspNowLink::ins()", "service.start", "service.stop",
                           "service.acceptIncoming", "service.selectRoom", "service.endVisit"):
                self.assertNotIn(effect, content)
        self.assertIn("Ui::DebugUi debugUi;", header)
        self.assertIn("case ViewMode::DEBUG: return &debugUi;", scene)
        for helper in ("renderDebugPage", "renderDebugSwitchPopup", "debugCursor",
                       "debugCategory", "debugSwitchDigits", "debugScroll"):
            self.assertNotIn(helper, scene + header)
        adapter = (ROOT / "src/core/ProgressionUi.cpp").read_text()
        for helper in ("PixelRenderer", "Hal::DISPLAY_", "drawEvolution", "confirmOpen"):
            self.assertNotIn(helper, adapter)
        self.assertIn("Ui::ProgressionUi ui(effects);", adapter)
        hatch = (ROOT / "src/scenes/HatchScene.cpp").read_text()
        hatch_header = (ROOT / "src/scenes/HatchScene.h").read_text()
        self.assertIn("Ui::HatchUi ui;", hatch_header)
        for call in ("ui.render(hatchFrame(", "ui.onButton(event,", "ui.onTap(target,",
                     "ui.touchTarget(x, y)", "ui.updateVisuals(hatchFrame("):
            self.assertIn(call, hatch)
        for helper in ("PixelRenderer", "TouchUi", "Hal::DISPLAY_", "RoomRenderer",
                       "drawRoom", "drawEgg", "drawHud", "lastEggOffset", "toastUntil"):
            self.assertNotIn(helper, hatch + hatch_header)
        self.assertIn("Ui::MenuOverlayUi overlayUi;", header)
        for call in ("overlayUi.renderBackground()", "overlayUi.renderToast(menuOverlayFrame())",
                     "overlayUi.renderTutorial(menuOverlayFrame())"):
            self.assertIn(call, scene)
        for helper in ("PixelRenderer", "TouchUi::", "TutorialOverlay", "MenuGrid::", "Hal::DISPLAY_"):
            for source in (ROOT / "src/scenes").glob("*.cpp"):
                self.assertNotIn(helper, source.read_text(), str(source.relative_to(ROOT)))
        for source in (ROOT / "src/ui").rglob("DebugUi*.cpp"):
            self.assertIn("#if STICKMON_ENABLE_DEBUG_FEATURES", source.read_text())
        for source in (ROOT / "src/ui").rglob("*"):
            if source.suffix in (".cpp", ".h"):
                self.assertNotIn("GameEngine", source.read_text(), str(source.relative_to(ROOT)))

    def test_application_overlays_and_input_use_the_selected_family(self):
        engine = (ROOT / "src/core/GameEngine.cpp").read_text()
        scene = (ROOT / "src/core/Scene.h").read_text()
        self.assertIn('#include "ui/ApplicationUi.h"', engine)
        self.assertIn('#include "ui/SceneInput.h"', scene)
        self.assertIn('Ui::SceneInput::target(', scene)
        self.assertIn('Ui::SceneInput::button(', scene)
        for forbidden in ('TouchUi', 'PixelRenderer::text', 'PixelRenderer::darken',
                          'fillRect(', 'drawRect(', 'POP_X', 'POP_Y'):
            self.assertNotIn(forbidden, engine)
        self.assertEqual(['bind'], re.findall(r'PixelRenderer::(\w+)', engine))
        for forbidden in ('LOGICAL_DISPLAY', '0x7000', 'Height - 32', 'event.x * 3'):
            self.assertNotIn(forbidden, scene)
        for family in ('stick', 'compact_touch'):
            for header in ('ApplicationUi.h', 'SceneInput.h'):
                self.assertTrue((ROOT / 'src/ui' / family / header).is_file())
        tools = (ROOT / 'src/presentation/TouchUi.cpp').read_text()
        self.assertNotIn('void drawNavigation(', tools)

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_profile_and_conflicting_flags(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "profile.cpp"
            for target, compact, width, height, inset, top in (
                    ("sticks3", "false", 240, 135, 0, 0),
                    ("ostb", "true", 296, 240, 28, 4)):
                source.write_text(
                    '#include "ui/UiProfile.h"\n'
                    f'static_assert(Ui::compactTouch == {compact});\n'
                    f'static_assert(Ui::device.touchInput == {compact});\n'
                    f'static_assert(Ui::device.width == {width});\n'
                    f'static_assert(Ui::device.height == {height});\n'
                    f'static_assert(Ui::device.headerInsetX == {inset});\n'
                    f'static_assert(Ui::device.headerTop == {top});\n')
                subprocess.run(["c++", "-std=c++17", "-fsyntax-only", "-Isrc",
                                *ui_flags_for(target), str(source)], cwd=ROOT,
                               check=True, capture_output=True, text=True)
            source.write_text('#include "ui/UiProfile.h"\n')
            for flags, message in (
                    (["-DSTICKMON_UI_COMPACT_TOUCH=1"], "requires touch input"),
                    (["-DSTICKMON_UI_COMPACT_TOUCH=2"], "must be 0 or 1")):
                result = subprocess.run(["c++", "-std=c++17", "-fsyntax-only", "-Isrc",
                                         *flags, str(source)], cwd=ROOT,
                                        capture_output=True, text=True)
                self.assertNotEqual(0, result.returncode)
                self.assertIn(message, result.stderr)


if __name__ == "__main__":
    unittest.main()
