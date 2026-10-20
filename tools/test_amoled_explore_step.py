#!/usr/bin/env python3
"""Replay the real exploration adapter and exercise session lifecycle contracts."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]


class AmoledExploreStepTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(temporary.cleanup)
        cls.binary = build_amoled_host(
            Path(temporary.name) / 'explore-session',
            ['tools/amoled_explore_session_host.cpp'], debug=True)
        if path := os.environ.get('AMOLED_EXPLORE_KEEP_BINARY'):
            import shutil
            shutil.copy2(cls.binary, path)

    def run_case(self, name):
        return subprocess.run([str(self.binary), str(ROOT / 'data'), name],
                              cwd=ROOT, check=True, text=True, stdout=subprocess.PIPE).stdout

    def test_fixed_routes_maps_events_and_rng_match_before_extraction(self):
        golden = (ROOT / 'tools/fixtures/amoled_explore_routes.txt').read_text()
        self.assertEqual(golden, self.run_case('routes'))

    def test_steps_recovery_cooldown_and_duplicate_completion(self):
        self.run_case('steps')

    def test_pickup_once_manual_auto_boss_stop_and_unhealthy_team(self):
        self.run_case('pickup-boss')

    def test_pickup_toast_owns_text_renders_and_expires_without_residue(self):
        self.run_case('pickup-toast')

    def test_repel_honey_guarantee_cooldown_and_encounter_cap(self):
        self.run_case('encounters')

    def test_ice_fall_and_pause_resume_motion_exit_clock_wrap(self):
        self.run_case('pause-ice')

    def test_return_once_item_reset_visitors_and_session_isolation(self):
        self.run_case('settlement')

    def test_main_menu_home_uses_return_fade_and_saves_settlement(self):
        self.run_case('menu-return')

    def test_battle_and_growth_return_rebase_route_animation_clock(self):
        self.run_case('battle-clock')

    def test_presenter_aliases_models_consumes_damage_and_isolates_instances(self):
        self.run_case('presenter')


if __name__ == '__main__':
    unittest.main()
