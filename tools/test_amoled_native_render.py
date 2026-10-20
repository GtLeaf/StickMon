#!/usr/bin/env python3
"""Render real AMOLED pages and compare native dirty bands with full frames."""
import os
import json
import re
import struct
import zlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]


class AmoledNativeRenderTests(unittest.TestCase):
    def test_page_headers_are_self_contained(self):
        headers = [f"ui/{header}" for header in
                   ("RoomScreens.h", "SettingsScreens.h", "HatchScreens.h", "UiCommon.h",
                    "ExploreMapRenderer.h", "RenderCaches.h", "SceneFade.h")]
        headers += ["app/HatchController.h", "app/ShowerController.h",
                    "app/BattleSession.h", "app/BattleTimeline.h",
                    "app/ExploreSession.h", "app/ExplorePresenter.h", "app/HomeFlow.h", "app/PairFlow.h",
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

    @classmethod
    def setUpClass(cls):
        sources = [
            "tools/amoled_native_render_host.cpp",
            "tools/amoled_battery_host.cpp",
            "tools/amoled_ecology_host.cpp",
            "tools/amoled_ui_behavior_host.cpp",
            "tools/amoled_expedition_host.cpp",
            "tools/amoled_battle_flow_host.cpp",
            "tools/amoled_shower_host.cpp",
            "tools/amoled_bath_hatch_controller_host.cpp",
            "tools/amoled_care_host.cpp",
            "tools/amoled_lock_power_host.cpp",
        ]
        tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(tmp.cleanup)
        cls.binary = str(Path(tmp.name) / "amoled-native-render")
        build_amoled_host(Path(cls.binary), sources,
                          debug=os.environ.get("AMOLED_RENDER_DEBUG", "0") == "1")
        keep_binary = os.environ.get("AMOLED_RENDER_KEEP_BINARY")
        if keep_binary:
            target = Path(keep_binary).resolve()
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(cls.binary, target)
            target.chmod(0o755)

    def run_case(self, name):
        subprocess.run([self.binary, str(ROOT / "data"), "--case", name],
                       cwd=ROOT, check=True)

    def test_shadow_contact_mirroring_hops_and_partial_redraw(self):
        args = [self.binary, str(ROOT / "data"), "--case", "pet-shadow"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

    def test_legacy_v2_pack_still_loads_and_draws_without_shadow_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory) / "data"
            shutil.copytree(ROOT / "data", data)
            path = data / "packs/dev/sprites/001.smonsp"
            packed = path.read_bytes()
            header = list(struct.unpack_from("<IHHHHHHIII", packed))
            raw = zlib.decompress(packed[28:], -15)
            old = b"".join(raw[i * 24:i * 24 + 20] for i in range(header[3]))
            old += raw[header[3] * 24:]
            encoder = zlib.compressobj(level=6, wbits=-15)
            compressed = encoder.compress(old) + encoder.flush()
            header[1], header[7], header[8], header[9] = 2, len(old), len(compressed), zlib.crc32(old)
            path.write_bytes(struct.pack("<IHHHHHHIII", *header) + compressed)
            subprocess.run([self.binary, str(data), "--case", "shadow-legacy"],
                           cwd=ROOT, check=True)

    def test_battery_sampling_header_redraw_unknown_and_recovery(self):
        self.run_case("battery-display")

    def test_explore_ecology_behavior_partial_frames_and_pause(self):
        self.run_case("explore-ecology")

    def test_explore_fish_bridge_occlusion_and_wildlife_grass(self):
        self.run_case("explore-water")

    def test_frost_wall_crystal_depth_and_route_partial_frames(self):
        args = [self.binary, str(ROOT / "data"), "--case", "explore-frost"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

    def test_shower_drag_damage_feedback_idle_and_completion(self):
        self.run_case("shower-flow")

    def test_bath_hatch_controller_effects_cancel_retry_and_clock_wrap(self):
        self.run_case("bath-hatch-controller-effects")

    def test_shower_level_up_returns_keep_stage_rewards_recovery_and_saved_state(self):
        self.run_case("shower-growth-roundtrip")

    def test_locked_room_care_time_and_damage(self):
        self.run_case("locked-room-power")

    def test_home_care_rewards_progression_and_reboot(self):
        self.run_case("home-care-rewards")

    def test_home_hud_messages_wrap_restore_and_ignore_hidden_buttons(self):
        args = [self.binary, str(ROOT / "data"), "--case", "home-hud-messages"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

    def test_home_actor_motion_and_lock_wake_preserve_authoritative_pose(self):
        self.run_case("home-room-authority")

    def test_home_two_servings_choose_hungrier_actor_and_clear_bowl(self):
        self.run_case("home-food-turns")

    def check_home_sequence(self, case):
        result = subprocess.run([self.binary, str(ROOT / "data"), "--case", case],
                                cwd=ROOT, check=True, text=True, capture_output=True)
        actual = re.findall(r"HomeSequence [^\n]+", result.stdout)
        expected = json.loads((ROOT / "tools/fixtures/amoled_home_flow_baseline.json").read_text())
        self.assertEqual(expected["sequence"], actual)

    def check_visitor_sequence(self, case, key):
        result = subprocess.run([self.binary, str(ROOT / "data"), "--case", case],
                                cwd=ROOT, check=True, capture_output=True, text=True)
        actual = re.findall(r"VisitorSequence [^\n]+", result.stdout)
        fixture = json.loads((ROOT / "tools/fixtures/amoled_visitor_flow_baseline.json").read_text())
        self.assertEqual(fixture[key], actual)

    def test_visitor_matches_pre_refactor_behavior(self):
        self.check_visitor_sequence("visitor-sequence", "normal")

    def test_visitor_matches_pre_refactor_clock_wrap(self):
        self.check_visitor_sequence("visitor-wrap", "wrap")

    def test_visitor_early_exit_cancellation_and_landing_effects(self):
        self.run_case("visitor-effects")

    def test_visit_away_departure_cancel_timeout_and_wrap(self):
        self.run_case("visit-away-effects")

    def test_pair_flow_effects_and_termination(self):
        self.run_case("pair-effects")

    def check_pair_sequence(self, case, key):
        result = subprocess.run([self.binary, str(ROOT / "data"), "--case", case],
                                cwd=ROOT, check=True, capture_output=True, text=True)
        actual = re.findall(r"PairSequence [^\n]+", result.stdout)
        fixture = json.loads((ROOT / "tools/fixtures/amoled_pair_flow_baseline.json").read_text())
        self.assertEqual(fixture[key], actual)

    def test_pair_flow_matches_pre_refactor_behavior(self):
        self.check_pair_sequence("pair-sequence", "normal")

    def test_pair_flow_matches_pre_refactor_clock_wrap(self):
        self.check_pair_sequence("pair-wrap", "wrap")

    def test_home_ambient_routes_frames_damage_and_rng_match_baseline(self):
        self.check_home_sequence("home-ambient-sequence")

    def test_home_ambient_clock_wrap_matches_baseline(self):
        self.check_home_sequence("home-ambient-wrap")

    def test_home_ambient_effects_cancel_clearance_and_stationary_waits(self):
        self.run_case("home-ambient-effects")

    def test_home_both_faint_recover_sleep_and_wake(self):
        self.run_case("home-faint-sleep-wake")

    def test_home_fed_companion_clears_bowl_for_hungry_leader_after_restore(self):
        self.run_case("home-food-exit-stall")

    def test_home_save_during_turn_restores_walk_and_food_routes(self):
        self.run_case("home-saved-turns")

    def test_home_yield_turn_and_room_action_arrival_presentation(self):
        self.run_case("home-yield-actions")

    def test_shower_foam_rinse_feedback_and_cache_partial_frames(self):
        args = [self.binary, str(ROOT / "data"), "--case", "shower-render"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

    def test_explore_position_cadence_and_damage_rectangle_pixels(self):
        self.run_case("explore-motion-damage")

    def test_park_third_route_selection_boss_placement_and_map_link(self):
        self.run_case("explore-park-routes")

    def test_explore_button_pixels_and_hit_boundaries(self):
        self.run_case("explore-buttons")

    def test_first_boot_hatch_resume_and_save_commit(self):
        self.run_case("hatch-flow")

    def test_hatch_pixels_and_dirty_bands(self):
        args = [self.binary, str(ROOT / "data"), "--case", "hatch-render"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

    def test_shared_page_header_pixels_and_hit_boundaries(self):
        self.run_case("page-header")

    def test_battle_sprite_fit_and_no_floating_damage_numbers(self):
        self.run_case("battle-sprite-presentation")

    def test_snorlax_front_and_attack_animation_match_partial_redraws(self):
        self.run_case("battle-snorlax")

    def test_body_size_proportions_and_flying_species_match_partial_redraws(self):
        self.run_case("battle-body-sizes")

    def test_pet_hearts_follow_visible_head_and_hop(self):
        args = [self.binary, str(ROOT / "data"), "--case", "pet-heart-layout"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

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

    def test_settings_reset_requires_confirmation_and_preserves_non_game_storage(self):
        self.run_case("settings-reset")

    def test_settings_reset_confirmation_pixels_and_partial_frames(self):
        args = [self.binary, str(ROOT / "data"), "--case", "settings-reset-render"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

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

    def test_battle_fx_frames_and_animation_dirty_rows(self):
        args = [self.binary, str(ROOT / "data"), "--case", "battle-fx"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)

    def test_battle_commit_delay_fresh_clock_and_one_shot_audio(self):
        self.run_case("battle-flow-commit")

    def test_battle_log_storage_capacity_failure_and_clock_wrap(self):
        self.run_case("battle-flow-logs")

    def test_battle_switch_preload_ack_and_permanent_roster_order(self):
        self.run_case("battle-flow-switch")

    def test_battle_settlement_once_and_two_member_progression(self):
        self.run_case("battle-flow-settlement")

    def test_battle_friendship_offer_decline_full_box_and_team(self):
        self.run_case("battle-flow-friendship")

    def test_battle_asset_actions_follow_real_results(self):
        self.run_case("battle-asset-actions")

    def test_battle_shared_rules_and_effect_logs_apply_once_on_both_sides(self):
        self.run_case("battle-shared-execution")

    def test_debug_battle_fixtures_preserve_player_save_on_close_and_reboot(self):
        if os.environ.get("AMOLED_RENDER_DEBUG") == "1":
            self.run_case("battle-diagnostic-save")

    def test_medicine_target_selection_hp_animation_and_return(self):
        self.run_case("medicine-flow")

    def test_page_clip_intersection_and_early_return(self):
        self.run_case("page-clip")

    def test_progression_replacement_requires_selection_and_confirmation(self):
        self.run_case("progression-replace")

    def test_explore_pickup_is_visible_and_matches_partial_redraws(self):
        self.run_case("explore-pickup")
        # Firmware replaces the shared UI pack with the native AMOLED pack.
        with tempfile.TemporaryDirectory() as directory:
            data = Path(directory)
            for item in (ROOT / "data").iterdir():
                if item.name != "packs":
                    (data / item.name).symlink_to(item, target_is_directory=item.is_dir())
            pack = data / "packs/dev"
            pack.mkdir(parents=True)
            for item in (ROOT / "data/packs/dev").iterdir():
                if item.name != "game":
                    (pack / item.name).symlink_to(item, target_is_directory=item.is_dir())
            game = pack / "game"
            game.mkdir()
            for item in (ROOT / "data/packs/dev/game").iterdir():
                source = item.with_name("ui_amoled.smonfx") if item.name == "ui.smonfx" else item
                (game / item.name).symlink_to(source)
            subprocess.run([self.binary, str(data), "--case", "explore-pickup"],
                           cwd=ROOT, check=True)

    def test_explore_vegetation_respects_actor_depth_and_partial_redraws(self):
        args = [self.binary, str(ROOT / "data"), "--case", "explore-vegetation"]
        output = os.environ.get("AMOLED_RENDER_OUTPUT")
        if output:
            Path(output).mkdir(parents=True, exist_ok=True)
            args.append(output)
        subprocess.run(args, cwd=ROOT, check=True)


    def test_lock_focus_matches_visible_sprite_pixels(self):
        self.run_case("lock-focus-pixels")

    def test_expedition_black_frames_and_direct_home_return(self):
        self.run_case("expedition-transition")

    def test_healthy_reserve_continues_normal_and_boss_encounters(self):
        self.run_case("explore-reserve-encounters")

    def test_explore_saves_at_rewards_and_exit_boundaries(self):
        self.run_case("explore-save-boundaries")

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
