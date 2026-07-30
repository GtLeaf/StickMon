#!/usr/bin/env python3

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace

from PIL import Image

from generate_game_assets import (
    EXPLORE_PICKUP_CONTENT_SIZE,
    EXPLORE_PICKUP_SIZE,
    ITEMS,
    MAX_PACK_FRAMES,
    SHOWER_BRUSH_SIZE,
    SHOWER_BACKGROUND_SIZE,
    SHOWER_BUBBLE_SIZES,
    SHOWER_SOAP_SIZE,
    SHOWER_SPRINKLER_SIZE,
    SHOWER_MENU_SOAP_SIZE,
    SHOWER_MENU_BRUSH_SIZE,
    SHOWER_MENU_SPRINKLER_SIZE,
    SHOWER_SOAP_COUNT,
    prepare_shower_assets,
    prepare_explore_pickup_marker,
    validate_explore_pickup_pack_mapping,
    validate_runtime_pack_limit,
    validate_shower_pack_mapping,
    write_pack,
)


class GenerateGameAssetsTests(unittest.TestCase):
    def test_food_icons_match_their_ui_names(self):
        item_sources = dict(ITEMS)
        self.assertEqual(item_sources["ITEM_NORMAL_FOOD"], "LAVACOOKIE.png")
        self.assertEqual(item_sources["ITEM_TASTY_FOOD"], "RAGECANDYBAR.png")

    def test_runtime_uses_direct_kind_index(self):
        source = (
            Path(__file__).resolve().parents[1]
            / "src"
            / "assets"
            / "GameAssets.cpp"
        ).read_text()

        self.assertIn("uint16_t frameIndices[", source)
        find_frame = source.split("FrameRef findFrame(Kind kind)", 1)[1].split(
            "bool decodeBackgroundViewport", 1
        )[0]
        self.assertNotIn("pack.frameCount; ++i", find_frame)

    def test_generator_frame_limit_matches_runtime(self):
        validate_runtime_pack_limit()

    def test_explore_pickup_marker_is_routed_to_ui_pack(self):
        validate_explore_pickup_pack_mapping()

    def test_shower_assets_are_routed_to_ui_pack(self):
        validate_shower_pack_mapping()

    def test_shower_reference_sheets_are_extracted_at_runtime_sizes(self):
        assets = dict(prepare_shower_assets())
        for index, size in enumerate(SHOWER_BUBBLE_SIZES):
            self.assertEqual(assets[f"SHOWER_BUBBLE_{index}"].size, size)
        self.assertEqual(assets["SHOWER_BRUSH"].size, SHOWER_BRUSH_SIZE)
        for index in range(SHOWER_SOAP_COUNT):
            self.assertEqual(assets[f"SHOWER_SOAP_{index}"].size, SHOWER_SOAP_SIZE)
            self.assertIsNotNone(
                assets[f"SHOWER_SOAP_{index}"].getchannel("A").getbbox()
            )
        self.assertEqual(
            assets["SHOWER_SPRINKLER"].size, SHOWER_SPRINKLER_SIZE
        )
        self.assertEqual(assets["SHOWER_MENU_SOAP"].size, SHOWER_MENU_SOAP_SIZE)
        self.assertEqual(assets["SHOWER_MENU_BRUSH"].size, SHOWER_MENU_BRUSH_SIZE)
        self.assertEqual(
            assets["SHOWER_MENU_SPRINKLER"].size,
            SHOWER_MENU_SPRINKLER_SIZE,
        )
        self.assertEqual(
            assets["SHOWER_BACKGROUND"].size, SHOWER_BACKGROUND_SIZE
        )

    def test_oversized_pack_is_rejected_before_writing(self):
        writer = SimpleNamespace(
            frames=[{}] * (MAX_PACK_FRAMES + 1),
            data=[],
            palettes=[],
        )
        with self.assertRaisesRegex(ValueError, "frames; limit"):
            write_pack("map", writer)

    def test_explore_pickup_marker_is_generated_at_native_render_size(self):
        with TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / "marker.png"
            source = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
            source.paste((255, 0, 0, 255), (6, 12, 42, 36))
            source.save(source_path)

            marker = prepare_explore_pickup_marker(source_path)

        self.assertEqual(marker.size, (EXPLORE_PICKUP_SIZE, EXPLORE_PICKUP_SIZE))
        bbox = marker.getchannel("A").getbbox()
        self.assertIsNotNone(bbox)
        self.assertLessEqual(bbox[2] - bbox[0], EXPLORE_PICKUP_CONTENT_SIZE)
        self.assertLessEqual(bbox[3] - bbox[1], EXPLORE_PICKUP_CONTENT_SIZE)


if __name__ == "__main__":
    unittest.main()
