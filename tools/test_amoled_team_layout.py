#!/usr/bin/env python3
"""Remaining source integration checks; pixel/hit contracts live in test_amoled_native_render."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from amoled_page_cases import run_page_case
from amoled_source import read_home_source


ROOT = Path(__file__).resolve().parents[1]
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
        cls.controller = (ROOT / "firmware/amoled_1_8_v1/main/app/TeamController.cpp").read_text()
        cls.team_source = TEAM_SCREENS.read_text(encoding="utf-8")
        cls.models_source = SCREEN_MODELS.read_text(encoding="utf-8")
        cls.strings_source = UI_STRINGS.read_text(encoding="utf-8")
        cls.common_source = UI_COMMON.read_text(encoding="utf-8")
        cls.metrics_source = UI_METRICS.read_text(encoding="utf-8")
        cls.room_source = ROOM_SCREENS.read_text(encoding="utf-8")
        cls.settings_source = SETTINGS_SCREENS.read_text(encoding="utf-8")

    def test_team_action_popup_uses_shared_hit_test(self):
        run_page_case("team-navigation")

    def test_item_target_tap_bypasses_regular_team_popup(self):
        run_page_case("bag-use")

    def test_item_target_team_preserves_bag_return_chain(self):
        run_page_case("bag-navigation")
        run_page_case("bag-use")

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_item_target_return_chain_for_route_and_explore_menu(self):
        source = r"""
#include "core/AppSceneFlow.h"
#include <cassert>
#include <initializer_list>

int main() {
    using namespace AppSceneFlow;
    for (Scene origin : {Scene::EXPLORE_ROUTE, Scene::EXPLORE_MENU,
                         Scene::MAIN_MENU}) {
        Controller flow(origin);
        flow.openSubScene(Scene::BAG);
        assert(flow.subSceneReturn() == origin);
        flow.enter(Scene::TEAM);
        assert(flow.subSceneReturn() == origin);
        flow.enter(Scene::BAG);
        assert(flow.closeSubScene() == origin);
    }
    Controller normalTeam(Scene::EXPLORE_MENU);
    normalTeam.openSubScene(Scene::TEAM);
    assert(normalTeam.closeSubScene() == Scene::EXPLORE_MENU);
}
"""
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "item_target_return"
            subprocess.run(
                ["c++", "-std=c++17", f"-I{ROOT / 'src'}", "-x", "c++",
                 "-", "-o", str(binary)],
                input=source, text=True, capture_output=True, check=True,
            )
            subprocess.run([str(binary)], capture_output=True, check=True)

    def test_front_action_sets_popup_target_before_switching(self):
        run_page_case("team-leader")

    def test_status_action_opens_selected_monster_status_page(self):
        run_page_case("team-leader")
        run_page_case("team-navigation")

    def test_status_page_has_stick_five_page_data_and_shared_v2_renderer(self):
        self.assertIn("constexpr uint8_t TEAM_STATUS_PAGE_COUNT = 5;", self.source)
        for label in ("CURRENT_STATS", "MOVE_INFO", "EFFORT_STATS", "INDIVIDUAL_STATS"):
            self.assertIn(f"Ui::Status::{label}", self.source)
        self.assertIn("PokemonSprites::SpriteKind::STATUS", self.source)
        v2_cmake = (ROOT / "firmware" / "amoled_1_8_v2" / "main" / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("${STICKMON_AMOLED_UI_SOURCES}", v2_cmake)
        self.assertIn("firmware/common/amoled_sources.cmake", v2_cmake)

    def test_status_overview_sprite_identity_and_flavor_layout(self):
        overview = self.team_source[
            self.team_source.index("if (page == 0) {"):
            self.team_source.index("} else if (page == 1) {")]
        self.assertIn("128.0f / std::max(1, static_cast<int>(width))", overview)
        self.assertIn("constexpr int identityX = 160;", overview)
        self.assertIn("identityX + textWidth(line) + 8", overview)
        self.assertIn("gender == Game::Gender::MALE", overview)
        self.assertIn("gender == Game::Gender::FEMALE", overview)
        self.assertIn('text(canvas, baseX + 24, 310, Ui::Status::SOURCE_INFO', overview)
        self.assertIn('const int genderY = gender == Game::Gender::MALE', overview)
        self.assertIn("Ui::Status::FLAVOR_DISLIKE_EMPTY", overview)

    def test_status_pages_switch_by_swipe_with_snap_animation(self):
        run_page_case("team-navigation")
        self.assertIn("int16_t slideOffsetX = 0;", self.models_source)

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

        run_page_case("team-navigation")

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

        self.assertIn("constexpr int MOVES_ROW_STEP = 66;", self.controller)
        clamp = self.controller.split("void TeamController::clampMovesScroll()", 1)[1].split(
            "void TeamController::startDetailAnimation", 1)[0]
        self.assertIn("* MOVES_ROW_STEP", clamp)

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
        self.assertIn("drawPageHeader(canvas, Ui::SHOP", self.source)
        self.assertIn("drawPageHeaderCenteredText(canvas, coins",
                      self.source)
        self.assertIn("drawHeader(canvas, HEADER_HEIGHT, true)", self.source)
        self.assertIn("drawClawTabs(canvas, model.clawLogView)", self.source)
        self.assertIn("UiCommon::drawHeader(canvas, HEADER_HEIGHT);",
                      self.source)

    def test_moves_expanded_detail_only_repeats_power_accuracy_and_description(self):
        start = self.team_source.index("void drawMoveDetailContent(")
        end = self.team_source.index("\n}\n", start)
        detail = self.team_source[start:end]
        self.assertIn('"威力:%u  命中:%u"', detail)
        self.assertIn("move.description", detail)
        self.assertNotIn("move.name", detail)
        self.assertNotIn("typeName(move.type)", detail)
        self.assertNotIn("moveProficiency", detail)


if __name__ == "__main__":
    unittest.main()
