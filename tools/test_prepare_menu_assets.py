#!/usr/bin/env python3
import unittest

from PIL import Image

import prepare_menu_assets as menu_assets


class MainMenuAssetTests(unittest.TestCase):
    def test_standalone_icons_are_ordered_one_through_nine(self):
        paths = menu_assets.standalone_icon_paths(menu_assets.DEFAULT_SOURCE_DIR)

        self.assertEqual(len(paths), menu_assets.ICON_COUNT)
        for index, path in enumerate(paths, start=1):
            self.assertTrue(
                path.name == f"{index}.png" or path.name.endswith(f"({index}).png")
            )

    def test_baked_checkerboard_becomes_transparent(self):
        source = menu_assets.standalone_icon_paths(menu_assets.DEFAULT_SOURCE_DIR)[0]
        image = menu_assets.remove_baked_checkerboard(Image.open(source))

        self.assertEqual(image.getpixel((0, 0))[3], 0)
        self.assertIsNotNone(image.getchannel("A").getbbox())

    def test_generated_sheet_contains_nine_transparent_frames(self):
        sheet = menu_assets.build_sheet_from_directory(menu_assets.DEFAULT_SOURCE_DIR)

        self.assertEqual(
            sheet.size,
            (menu_assets.ICON_COUNT * menu_assets.ICON_SIZE, menu_assets.ICON_SIZE),
        )
        for index in range(menu_assets.ICON_COUNT):
            frame = sheet.crop((
                index * menu_assets.ICON_SIZE,
                0,
                (index + 1) * menu_assets.ICON_SIZE,
                menu_assets.ICON_SIZE,
            ))
            self.assertIsNotNone(frame.getchannel("A").getbbox())
            self.assertEqual(frame.getpixel((0, 0))[3], 0)


if __name__ == "__main__":
    unittest.main()
