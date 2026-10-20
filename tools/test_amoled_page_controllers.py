#!/usr/bin/env python3
"""Exercise real AMOLED page behavior without reading controller internals."""

import unittest
from amoled_page_cases import run_page_case


class AmoledPageControllerTests(unittest.TestCase):
    def run_case(self, case):
        run_page_case(case)

    def test_slider_preview_cancel_save_and_startup_restore(self):
        self.run_case("settings-sliders")

    def test_speed_power_cycles_and_page_exit_cancel_gesture(self):
        self.run_case("settings-actions")

    def test_reset_rejects_orphan_cancelled_and_cross_button_releases(self):
        self.run_case("settings-reset")

    def test_team_status_moves_back_and_cancelled_swipe(self):
        self.run_case("team-navigation")

    def test_team_selected_status_and_front_action_persist(self):
        self.run_case("team-leader")

    def test_bag_target_back_and_cancelled_press_preserve_inventory(self):
        self.run_case("bag-navigation")

    def test_bag_target_heals_only_selected_member_and_returns(self):
        self.run_case("bag-use")

    def test_shop_sale_saves_once_and_zero_stock_stays_disabled(self):
        self.run_case("shop-transactions")

    def test_failed_saves_retry_without_waiting_for_periodic_save(self):
        self.run_case("save-retry")

    def test_history_only_save_failure_retries_after_successful_snapshot(self):
        run_page_case("history-retry")

    def test_page_exit_consumes_gestures_from_all_extracted_pages(self):
        self.run_case("page-exit-gestures")


if __name__ == "__main__":
    unittest.main()
