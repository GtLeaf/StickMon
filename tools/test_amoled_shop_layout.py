#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOME_SCREEN = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "HomeScreen.cpp"
AMOLED_APP = ROOT / "firmware" / "amoled_1_8_v1" / "main" / "AmoledApp.cpp"


class AmoledShopLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.screen = HOME_SCREEN.read_text(encoding="utf-8")
        cls.app = AMOLED_APP.read_text(encoding="utf-8")

    def test_left_rail_has_buy_sell_and_leave(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        self.assertIn(
            "Ui::Amoled::BUY, Ui::Amoled::SELL, Ui::Amoled::LEAVE",
            render,
        )
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
        self.assertIn("SHOP_GRID_ICON_SCALE = 1.0f", self.screen)
        self.assertIn("SHOP_DETAIL_ICON_END_SCALE = 1.4f", self.screen)
        self.assertIn("float iconScale = SHOP_DETAIL_ICON_END_SCALE", render)
        self.assertIn("shopDetailProgress = 1.0f", self.app)
        self.assertIn("shopDetailProgress = 0.0f", self.app)

    def test_grid_cells_render_native_icons_without_labels(self):
        render = self._function(self.screen, "void renderShopScreen(",
                                "int roomMenuItemAt(")
        grid = render[render.index("for (uint8_t index = 0;"):
                      render.index("if (model.itemCount == 0)")]
        self.assertIn("SHOP_GRID_ICON_SCALE", grid)
        self.assertNotIn("Game::ShopService::shortName", grid)

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
