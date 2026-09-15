#!/usr/bin/env python3
"""Remaining source integration checks; pixel/hit contracts live in test_amoled_native_render."""

import unittest
from pathlib import Path

from amoled_source import read_home_source


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"
TEAM_SCREENS = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
                "ui" / "TeamScreens.inc")
SCREEN_MODELS = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
                 "ui" / "models" / "ScreenModels.h")
UI_STRINGS = ROOT / "src" / "core" / "UiStrings.h"
UI_COMMON = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
             "ui" / "UiCommon.cpp")
UI_METRICS = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
              "ui" / "UiMetrics.h")
ROOM_SCREENS = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
                "ui" / "RoomScreens.cpp")
SETTINGS_SCREENS = (ROOT / "firmware" / "amoled_1_8_v1" / "main" /
                    "ui" / "SettingsScreens.cpp")


class AmoledTeamLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = read_home_source(ROOT)
        cls.app_source = AMOLED_APP.read_text(encoding="utf-8")
        cls.team_source = TEAM_SCREENS.read_text(encoding="utf-8")
        cls.models_source = SCREEN_MODELS.read_text(encoding="utf-8")
        cls.strings_source = UI_STRINGS.read_text(encoding="utf-8")
        cls.common_source = UI_COMMON.read_text(encoding="utf-8")
        cls.metrics_source = UI_METRICS.read_text(encoding="utf-8")
        cls.room_source = ROOM_SCREENS.read_text(encoding="utf-8")
        cls.settings_source = SETTINGS_SCREENS.read_text(encoding="utf-8")

    def test_team_action_popup_uses_shared_hit_test(self):
        start = self.app_source.index("if (teamActionPopupOpen)")
        end = self.app_source.index("if (teamMovesOpen)", start)
        handler = self.app_source[start:end]
        self.assertIn("teamActionPopupItemAt(x, y, teamActionPopupSlot)", handler)

    def test_front_action_sets_popup_target_before_switching(self):
        start = self.app_source.index("if (teamActionPopupOpen)")
        end = self.app_source.index("if (teamMovesOpen)", start)
        handler = self.app_source[start:end]
        action_start = handler.index("if (!leader && choice == 0) {")
        action_end = handler.index("}", action_start)
        action = handler[action_start:action_end]
        self.assertLess(action.index("pendingTeamSlot = teamActionPopupSlot;"),
                        action.index("switchTeamLeader(nowMs);"))

        start = self.app_source.index("void AmoledApp::switchTeamLeader(")
        end = self.app_source.index("void AmoledApp::leaveTeamMember(", start)
        switch = self.app_source[start:end]
        self.assertIn("Game::TeamRoster::moveToFront(gameState, pendingTeamSlot)",
                      switch)

    def test_status_action_opens_selected_monster_status_page(self):
        start = self.app_source.index("if (teamStatusOpen)")
        end = self.app_source.index("if (teamMovesOpen)", start)
        handler = self.app_source[start:end]
        self.assertIn("teamStatusOpen = false", handler)
        self.assertNotIn("teamStatusNextAt", handler)

        popup = self.app_source[
            self.app_source.index("if (teamActionPopupOpen)"):
            self.app_source.index("if (teamMovesOpen)",
                                  self.app_source.index("if (teamActionPopupOpen)"))
        ]
        self.assertIn("teamStatusOpen = true", popup)
        self.assertIn("teamStatusSlot = teamActionPopupSlot", popup)

    def test_status_page_has_stick_five_page_data_and_shared_v2_renderer(self):
        self.assertIn("constexpr uint8_t TEAM_STATUS_PAGE_COUNT = 5;", self.source)
        for label in ("CURRENT_STATS", "MOVE_INFO", "EFFORT_STATS", "INDIVIDUAL_STATS"):
            self.assertIn(f"Ui::Status::{label}", self.source)
        self.assertIn("PokemonSprites::SpriteKind::STATUS", self.source)
        v2_cmake = (ROOT / "firmware" / "amoled_1_8_v2" / "main" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("${STICKMON_AMOLED_UI_SOURCES}", v2_cmake)
        self.assertIn("firmware/common/amoled_sources.cmake", v2_cmake)

    def test_status_pages_switch_by_swipe_with_snap_animation(self):
        self.assertIn("teamStatusDragging = true;", self.app_source)
        self.assertIn("beginTeamStatusSlide(static_cast<uint8_t>(target)", self.app_source)
        self.assertIn("teamStatusPage = teamStatusAnimTargetPage;", self.app_source)
        self.assertIn("model.slideOffsetX = teamStatusSlideX;", self.app_source)
        self.assertNotIn("teamStatusNextAt", self.app_source)
        self.assertIn("int16_t slideOffsetX = 0;",
                      self.models_source)

    def test_moves_uses_shared_page_header_with_title_only(self):
        start = self.team_source.index("bool teamMovesBackAt")
        end = self.team_source.index("void renderTeamMovesScreen", start)
        header = self.team_source[start:end]
        self.assertIn("UiCommon::pageHeaderBackAt(x, y)", header)
        renderer = self.team_source[
            self.team_source.index("void renderTeamMovesScreen"):
            self.team_source.index("drawToast(canvas, model.toast);",
                                   self.team_source.index("void renderTeamMovesScreen"))
        ]
        self.assertIn("UiCommon::drawPageHeader(canvas, Ui::Amoled::MOVES)",
                      renderer)
        self.assertNotIn("drawTeamMovesHeader", self.team_source)
        self.assertNotIn("clockText", header)
        self.assertNotIn("gameMinutesOfDay", header)
        self.assertNotIn("gameMinutesOfDay", self.models_source[
            self.models_source.index("struct TeamMovesViewModel"):
            self.models_source.index("struct RoomMenuViewModel")])

    def test_moves_manage_list_only_contains_move_slots(self):
        start = self.team_source.index("int teamMovesItemAt(",
                                       self.team_source.index("int teamMovesItemAt(") + 1)
        end = self.team_source.index("void renderTeamMovesScreen", start)
        hit_test = self.team_source[start:end]
        self.assertIn("? Game::MOVE_SLOT_COUNT", hit_test)

        start = self.team_source.index("void renderTeamMovesScreen")
        end = self.team_source.index("drawToast(canvas, model.toast);", start)
        renderer = self.team_source[start:end]
        self.assertIn("? Game::MOVE_SLOT_COUNT", renderer)
        self.assertNotIn("Ui::Amoled::RECALL_MOVE", renderer)

        start = self.app_source.index("if (teamMovesOpen)")
        end = self.app_source.index("if (teamConfirmOpen)", start)
        handler = self.app_source[start:end]
        manage_start = handler.index(
            "if (teamMovesMode == TeamMovesViewModel::Mode::MANAGE)")
        manage_end = handler.index(
            "else if (teamMovesMode == TeamMovesViewModel::Mode::RECALL_SELECT)",
            manage_start)
        manage_handler = handler[manage_start:manage_end]
        self.assertNotIn("refreshTeamMoveRecallable", manage_handler)
        self.assertNotIn("teamMovesOpen = false", manage_handler)

    def test_moves_cells_are_taller_centered_and_use_black_page_background(self):
        start = self.team_source.index("int teamMovesItemAt(",
                                       self.team_source.index("int teamMovesItemAt(") + 1)
        end = self.team_source.index("drawToast(canvas, model.toast);", start)
        moves_page = self.team_source[start:end]
        self.assertEqual(moves_page.count("constexpr int ROW_HEIGHT = 60;"), 2)
        self.assertEqual(moves_page.count("constexpr int ROW_STEP = 66;"), 2)
        self.assertIn("const int textY = y +", moves_page)
        self.assertIn(
            "(ROW_HEIGHT - FontResource::LARGE_GLYPH_H) / 2",
            moves_page)
        self.assertIn("UiCommon::drawPageHeader(canvas, Ui::Amoled::MOVES)",
                      moves_page)
        self.assertIn("(AmoledUi::HEIGHT),\n                    rgb(0, 0, 0));",
                      moves_page)

        clamp_start = self.app_source.index("void AmoledApp::clampTeamMovesScroll")
        clamp_end = self.app_source.index(
            "void AmoledApp::startTeamMovesDetailAnimation", clamp_start)
        self.assertIn("constexpr int ROW_STEP = 66;",
                      self.app_source[clamp_start:clamp_end])

    def test_moves_cells_show_emerald_type_badge_and_four_proficiency_grades(self):
        start = self.team_source.index("const char* teamMovesProficiencyName")
        end = self.team_source.index("void drawTeamStatusPageContent", start)
        helpers = self.team_source[start:end]
        for threshold in ("value >= 25", "value >= 60", "value >= 90"):
            self.assertIn(threshold, helpers)
        for label in ("入门", "熟悉", "熟练", "精通"):
            self.assertIn(f'= "{label}";', self.strings_source)
        self.assertIn("void drawTeamMoveTypeBadge", helpers)
        self.assertIn("rgb(168, 168, 120)", helpers)
        self.assertIn("rgb(232, 236, 218)", helpers)

        start = self.team_source.index("void renderTeamMovesScreen")
        end = self.team_source.index("drawToast(canvas, model.toast);", start)
        renderer = self.team_source[start:end]
        self.assertIn("drawTeamMoveTypeBadge(canvas, 20, y + 8, move->type)",
                      renderer)
        self.assertIn("text(canvas, 108, textY, move->name", renderer)
        self.assertIn("344 - textWidth(proficiency)", renderer)

    def test_shared_page_header_owns_geometry_and_black_style(self):
        self.assertIn("PAGE_HEADER_HEIGHT = 76", self.metrics_source)
        self.assertIn("PAGE_HEADER_BACK_HIT_WIDTH = 80", self.metrics_source)
        start = self.common_source.index("bool pageHeaderBackAt")
        header = self.common_source[start:self.common_source.index(
            "void drawToast", start)]
        self.assertIn("UiMetrics::PAGE_HEADER_BACK_HIT_WIDTH", header)
        self.assertIn("fillCircle(backCenterX, centerY, 26", header)
        self.assertIn("constexpr int backCenterX = 40;", header)
        self.assertIn("constexpr int rightPadding = 28;", header)
        self.assertIn("rgb(0, 0, 0)", header)

    def test_regular_pages_use_shared_header_while_scene_huds_stay_independent(self):
        for source, title in (
                (self.room_source, "Ui::ROOM"),
                (self.room_source, "Ui::FOOD"),
                (self.settings_source, "Ui::SETTINGS"),
                (self.source, "Ui::TEAM"),
                (self.source, "Ui::SOCIAL"),
                (self.source, "Ui::Amoled::WASH_PET")):
            with self.subTest(title=title):
                self.assertIn(f"drawPageHeader(canvas, {title}", source)
        self.assertIn("drawPageHeader(canvas, title)", self.source)
        self.assertIn("drawPageHeader(canvas, Ui::SHOP);", self.source)
        self.assertIn("drawPageHeaderCenteredText(canvas, coins",
                      self.source)
        self.assertIn("drawHeader(canvas, HEADER_HEIGHT, true)", self.source)
        self.assertIn("drawClawTabs(canvas, model.clawLogView)", self.source)
        self.assertIn("UiCommon::drawHeader(canvas, HEADER_HEIGHT);",
                      self.source)

    def test_moves_expanded_detail_only_repeats_power_accuracy_and_description(self):
        start = self.team_source.index("if (detailVisible) {")
        end = self.team_source.index("detailClip.reset();", start)
        detail = self.team_source[start:end]
        self.assertIn('"威力:%u  命中:%u"', detail)
        self.assertIn("selectedMove->description", detail)
        self.assertNotIn("selectedMove->name", detail)
        self.assertNotIn("typeName(selectedMove->type)", detail)
        self.assertNotIn("moveProficiency", detail)


if __name__ == "__main__":
    unittest.main()
