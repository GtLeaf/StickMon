#!/usr/bin/env python3

import unittest

from PIL import Image

from generate_pokemon_sprites import trim_alpha_padding


class GeneratePokemonSpritesTests(unittest.TestCase):
    def test_trim_alpha_padding_keeps_only_visible_bounds(self):
        image = Image.new("RGBA", (8, 7), (0, 0, 0, 0))
        image.putpixel((2, 1), (255, 0, 0, 17))
        image.putpixel((5, 4), (0, 255, 0, 255))

        trimmed = trim_alpha_padding(image)

        self.assertEqual(trimmed.size, (4, 4))
        self.assertEqual(trimmed.getpixel((0, 0))[3], 17)
        self.assertEqual(trimmed.getpixel((3, 3))[3], 255)

    def test_trim_alpha_padding_leaves_empty_frame_unchanged(self):
        image = Image.new("RGBA", (6, 5), (0, 0, 0, 0))

        self.assertEqual(trim_alpha_padding(image).size, image.size)


if __name__ == "__main__":
    unittest.main()
