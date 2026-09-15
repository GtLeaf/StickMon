#!/usr/bin/env python3
"""Remaining source integration checks; pixel/hit contracts live in test_amoled_native_render."""

import unittest
from pathlib import Path

from amoled_source import read_home_source


ROOT = Path(__file__).resolve().parents[1]
V1_MAIN = ROOT / "firmware" / "amoled_1_8_v1" / "main"
V2_MAIN = ROOT / "firmware" / "amoled_1_8_v2" / "main"


class AmoledNativeLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.home = read_home_source(ROOT)
        cls.home_header = (V1_MAIN / "HomeScreen.h").read_text(encoding="utf-8")
        cls.app = (V1_MAIN / "AmoledApp.cpp").read_text(encoding="utf-8")
        cls.geometry = (V1_MAIN / "AmoledGeometry.h").read_text(encoding="utf-8")
        cls.v1_main = (V1_MAIN / "main.cpp").read_text(encoding="utf-8")
        cls.v2_main = (V2_MAIN / "main.cpp").read_text(encoding="utf-8")

    def test_legacy_business_coordinate_helpers_are_removed(self):
        for source in (self.home, self.app, self.geometry,
                       cls_text(V1_MAIN / "main.cpp"),
                       cls_text(V2_MAIN / "main.cpp")):
            self.assertNotRegex(
                source,
                r"nativeCoordinate\(|nativeExtent\(|nativeRow\(|"
                r"touchToPage\(|CoordinateMode",
            )
        self.assertNotIn("physicalEvent.x / 2", self.app)
        self.assertNotIn("physicalEvent.y / 2", self.app)

    def test_touch_drivers_preserve_panel_pixels(self):
        for directory in (V1_MAIN, V2_MAIN):
            source = cls_text(directory / "TouchInput.cpp")
            self.assertNotIn("toPageCoordinate", source)
            self.assertNotIn("PAGE_WIDTH", source)
            self.assertIn("toDisplayCoordinate(point.x, DISPLAY_WIDTH, CAL_X_SCALE, CAL_X_OFFSET)", source)
            self.assertIn("toDisplayCoordinate(point.y, DISPLAY_HEIGHT, CAL_Y_SCALE, CAL_Y_OFFSET)", source)
        self.assertNotIn("pageRowBegin", self.app)
        self.assertNotIn("pageRowEnd", self.app)
        self.assertNotIn("rowEnd = 224", self.home_header)

    def test_dirty_regions_and_lock_animation_submit_native_rows(self):
        for source in (self.v1_main, self.v2_main):
            self.assertIn("submitFrameRegion(", source)
            self.assertIn("forceRenderRows(", source)
            self.assertNotIn("nativeCoordinate(", source)
            self.assertNotIn("nativeExtent(", source)
        self.assertIn("EXPLORE_ROUTE_MAP_FRAME_MS", self.app)
        self.assertIn("LOCK_ANIMATION_MS", self.v1_main)

    def test_lock_dirty_region_is_not_scaled_twice(self):
        for source in (self.v1_main, self.v2_main):
            self.assertIn("nativeRenderBegin = static_cast<uint16_t>(", source)
            self.assertIn("app.forceRenderRows(nativeRenderBegin, nativeRenderEnd);", source)
            self.assertIn(
                "nativeRenderXBegin, nativeRenderXEnd,\n"
                "                             nativeRenderBegin, nativeRenderEnd);",
                source,
            )
            self.assertNotIn(
                "((renderBegin) * AmoledUi::RESOURCE_SCALE),\n"
                "                             ((renderEnd) * AmoledUi::RESOURCE_SCALE)",
                source,
            )

    def test_dirty_region_bounds_are_native(self):
        for source in (self.v1_main, self.v2_main):
            self.assertIn("xBegin = std::min<uint16_t>(xBegin, LOGICAL_WIDTH);", source)
            self.assertIn("sourceEnd = std::min<uint16_t>(sourceEnd, LOGICAL_HEIGHT);", source)
            self.assertIn("constexpr uint16_t LOGICAL_WIDTH = AmoledUi::WIDTH;", source)
            self.assertIn("constexpr uint16_t LOGICAL_HEIGHT = AmoledUi::HEIGHT;", source)


def cls_text(path):
    return path.read_text(encoding="utf-8")


if __name__ == "__main__":
    unittest.main()
