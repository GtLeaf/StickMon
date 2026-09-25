#!/usr/bin/env python3
"""Render real AMOLED pages and compare native dirty bands with full frames."""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from amoled_source import amoled_ui_sources

ROOT = Path(__file__).resolve().parents[1]


class AmoledNativeRenderTests(unittest.TestCase):
    def test_page_headers_are_self_contained(self):
        for header in ("RoomScreens.h", "SettingsScreens.h", "UiCommon.h",
                       "ExploreMapRenderer.h", "RenderCaches.h", "SceneFade.h"):
            with self.subTest(header=header):
                subprocess.run([
                    "c++", "-std=c++17", "-fsyntax-only", "-x", "c++",
                    "-Isrc", "-Ifirmware/amoled_1_8_v1/main", "-",
                ], input=f'#include "ui/{header}"\n',
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

    @classmethod
    def setUpClass(cls):
        sources = [
            "tools/amoled_native_render_host.cpp",
            "tools/amoled_ui_behavior_host.cpp",
            "tools/amoled_expedition_host.cpp",
            "firmware/amoled_1_8_v1/main/AmoledApp.cpp",
            "src/hardware/EspNowLink.cpp",
            *[str(path.relative_to(ROOT)) for path in amoled_ui_sources(ROOT)],
            "src/platform/api/PlatformServices.cpp",
            "src/platform/desktop/DesktopPlatform.cpp",
        ]
        for directory, names in {
            "presentation": ["Canvas565", "PixelRenderer", "HudRenderer"],
            "assets": ["FontFallbackCN", "GameAssets", "HudAssets", "MenuAssets", "PokemonSprites"],
            "core": ["FontResource", "RoomResource", "RoomRenderer", "ResourceFS", "ResourcePack", "DeflateDecoder",
                     "AudioManager", "CryPlayer", "GameClockService", "RoomMovementArea",
                     "RoomNavigator",
                     "SaveCodec", "SaveManager", "VisitSessionService"],
        }.items():
            sources += [f"src/{directory}/{name}.cpp" for name in names]
        sources += [str(p.relative_to(ROOT)) for p in sorted((ROOT / "src/game").glob("*.cpp"))]
        tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(tmp.cleanup)
        # The host exercises app logic, not ESP touch-device initialization.
        (Path(tmp.name) / "esp_err.h").write_text("#pragma once\nusing esp_err_t = int;\n")
        (Path(tmp.name) / "esp_lcd_touch.h").write_text(
            "#pragma once\nusing esp_lcd_touch_handle_t = void*;\n")
        cls.binary = str(Path(tmp.name) / "amoled-native-render")
        inflater = str(Path(tmp.name) / "tinflate.o")
        subprocess.run(["cc", "-O1", "-c", "src/third_party/uzlib/tinflate.c",
                        "-o", inflater], cwd=ROOT, check=True)
        subprocess.run([
            "c++", "-std=c++17", "-O1", "-ffunction-sections", "-fdata-sections",
            "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
            "-Isrc", "-Ifirmware/amoled_1_8_v1/main",
            "-DSTICKMON_HAS_CLAW=0",
            "-DSTICKMON_ENABLE_DEBUG_FEATURES=" + os.environ.get("AMOLED_RENDER_DEBUG", "0"),
            "-I" + tmp.name, *sources, inflater, "-o", cls.binary,
        ], cwd=ROOT, check=True)

    def run_case(self, name):
        subprocess.run([self.binary, str(ROOT / "data"), "--case", name],
                       cwd=ROOT, check=True)

    def test_explore_button_pixels_and_hit_boundaries(self):
        self.run_case("explore-buttons")

    def test_shared_page_header_pixels_and_hit_boundaries(self):
        self.run_case("page-header")

    def test_battle_sprite_fit_and_no_floating_damage_numbers(self):
        self.run_case("battle-sprite-presentation")

    def test_home_faint_rest_progress_reuses_hp_bar(self):
        self.run_case("home-faint-hp-hud")

    def test_home_visit_recall_icon_and_confirmation(self):
        self.run_case("home-visit-recall")

    def test_explore_toast_and_completion_overlays_are_not_clipped(self):
        self.run_case("explore-overlay-geometry")

    def test_shop_detail_pressed_buttons_and_static_frame(self):
        self.run_case("shop-detail")

    def test_settings_slider_endpoints_thickness_and_pressed_state(self):
        self.run_case("settings-sliders")

    def test_team_popup_rows_and_separators(self):
        self.run_case("team-popup")

    def test_team_status_navigation_and_page_indicators(self):
        self.run_case("team-navigation")

    def test_team_status_formats_values_and_labels(self):
        self.run_case("team-status-values")

    def test_team_status_hp_and_experience_segmented_bars(self):
        self.run_case("team-status-segmented-bars")

    def test_team_moves_show_type_name_and_proficiency_grades(self):
        self.run_case("team-moves-list")

    def test_computer_contacts_popup_confirm_and_status_geometry(self):
        self.run_case("computer-contacts")

    def test_main_menu_grid_scrolling_and_pressed_cell(self):
        self.run_case("main-menu-grid")

    def test_cache_ownership_invalidation_and_scene_lifecycle(self):
        self.run_case("cache-lifecycle")

    def test_native_alpha_blend_matches_old_pixels_and_clip(self):
        self.run_case("alpha-blend-native")

    def test_prewarmed_battle_background_matches_cold_render(self):
        self.run_case("battle-prewarm")

    def test_page_clip_intersection_and_early_return(self):
        self.run_case("page-clip")

    def test_progression_replacement_requires_selection_and_confirmation(self):
        self.run_case("progression-replace")

    def test_expedition_black_frames_and_direct_home_return(self):
        self.run_case("expedition-transition")

    def test_debug_visitor_prompt_talk_and_exit(self):
        if os.environ.get("AMOLED_RENDER_DEBUG") == "1":
            self.run_case("visitor-diagnostics")

    def test_debug_visitor_recovers_invalid_host_door_pose(self):
        if os.environ.get("AMOLED_RENDER_DEBUG") == "1":
            self.run_case("visitor-invalid-entry")

    def test_unknown_behavior_case_is_rejected(self):
        result = subprocess.run(
            [self.binary, str(ROOT / "data"), "--case", "unknown-case"],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown behavior case", result.stderr)

    def test_native_geometry_and_partial_frames(self):
        args = [self.binary, str(ROOT / "data")]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
