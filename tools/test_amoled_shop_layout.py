#!/usr/bin/env python3

import unittest
from pathlib import Path

from amoled_source import read_home_source


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"


class AmoledShopLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.screen = read_home_source(ROOT)
        cls.app = AMOLED_APP.read_text(encoding="utf-8")

    def test_left_rail_only_has_buy_and_sell(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        self.assertIn(
            "Ui::Amoled::BUY, Ui::Amoled::SELL",
            render,
        )
        rail = render[render.index("MENU_LABELS"):
                      render.index("if (!detailOpen)")]
        self.assertNotIn("Ui::Amoled::LEAVE", rail)
        self.assertIn("index < 2", rail)
        self.assertIn("SHOP_RAIL_DIVIDER_X", render)

    def test_buy_grid_has_daily_then_explore_sections(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        daily = render.index("drawSectionHeader(0, Ui::Shop::CATEGORY_DAILY)")
        explore = render.index("Ui::Shop::CATEGORY_EXPLORE", daily)
        self.assertLess(daily, explore)
        self.assertIn("sectionIndex % 2", self.screen)
        self.assertNotIn("scrollbar", render.lower())

    def test_buy_mode_combines_both_catalogs(self):
        count = self._function(self.app, "uint8_t AmoledApp::currentItemCount()",
                               "Game::ItemId AmoledApp::currentItemAt(")
        self.assertIn("shopDailyItemCount() +", count)
        self.assertIn("shopExploreItemCount()", count)

    def test_detail_is_static_and_uses_native_icon_scale(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        self.assertIn("showRail", render)
        self.assertIn("const bool detailOpen", render)
        self.assertNotIn("shopEase", render)
        self.assertIn("SHOP_GRID_ICON_SCALE = 1.5f", self.screen)
        self.assertIn("SHOP_DETAIL_ICON_END_SCALE = 1.5f", self.screen)
        self.assertIn("float iconScale = SHOP_DETAIL_ICON_END_SCALE", render)
        self.assertIn("shopDetailProgress = 1.0f", self.app)
        self.assertIn("shopDetailProgress = 0.0f", self.app)

    def test_shop_header_has_back_title_right_and_centered_coins(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        header = render[:render.index("if (rowEnd <= MENU_CONTENT_TOP)")]
        self.assertIn("drawPageHeader(canvas, Ui::SHOP", header)
        self.assertIn("drawPageHeaderCenteredText(canvas, coins", header)
        tap = self._function(self.app, "void AmoledApp::handleTap(",
                             "void AmoledApp::update(")
        shop = tap[tap.index("if (sceneFlow.current() == AppSceneFlow::Scene::SHOP)"):]
        shop = shop[:shop.index("if (sceneFlow.current() == AppSceneFlow::Scene::EXPLORE_AREAS)")]
        self.assertIn("itemListBackAt(x, y, itemConfirmOpen)", shop)
        self.assertIn("closeItemScene();", shop)

    def test_grid_cells_render_native_icons_without_labels(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        grid = render[render.index("for (uint8_t index = 0;"):
                      render.index("if (model.itemCount == 0)")]
        self.assertIn("SHOP_GRID_ICON_SCALE", grid)
        self.assertNotIn("Game::ShopService::shortName", grid)

    def test_static_detail_does_not_wait_for_animation_to_accept_touch(self):
        touch = self._function(self.app, "void AmoledApp::handleTouch(",
                               "void AmoledApp::handleTap(")
        tap = self._function(self.app, "void AmoledApp::handleTap(",
                             "void AmoledApp::update(")
        self.assertNotIn("shopDetailProgress >= 1.0f", touch)
        self.assertNotIn("if (shopDetailProgress < 1.0f) return;", tap)

    def test_detail_release_requires_same_button_before_generic_tap_slop(self):
        touch = self._function(self.app, "void AmoledApp::handleTouch(",
                               "void AmoledApp::handleTap(")
        release = touch[touch.index("case TouchEventType::UP:"):]
        self.assertIn("itemConfirmChoiceAt(touchStartX, touchStartY)", release)
        self.assertIn("startChoice >= 0", release)
        self.assertIn("startChoice == itemConfirmChoiceAt(event.x, event.y)", release)
        self.assertLess(release.index("const int startChoice"),
                        release.index("} else if (!dragging && distance <= TAP_SLOP)"))

    def test_successful_shop_transaction_stays_in_detail(self):
        action = self._function(
            self.app,
            "void AmoledApp::performPendingItemAction(",
            "bool AmoledApp::saveState()",
        )
        transaction = action[action.index("bool shopTransaction") :]
        self.assertIn("requestFullRender();", transaction)
        self.assertIn("return;", transaction)
        self.assertLess(transaction.index("return;"),
                        transaction.index("itemConfirmOpen = false;"))

    @staticmethod
    def _function(source, start_marker, end_marker):
        start = source.index(start_marker)
        end = source.index(end_marker, start)
        return source[start:end]


if __name__ == "__main__":
    unittest.main()
