#!/usr/bin/env python3

import re
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock
from pathlib import Path

from PIL import Image

import generate_pokemon_sprites as generator
from generate_pokemon_sprites import (
    ENEMY_FRONT_MAX_HEIGHT,
    ENEMY_FRONT_MAX_WIDTH,
    PMD_SPECS,
    prepare_enemy_battle_front,
    pmd_walking_frame_path,
    trim_alpha_padding,
)


ROOT = Path(__file__).resolve().parents[1]


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

    def test_enemy_front_is_generated_at_final_battle_size(self):
        image = Image.new("RGBA", (160, 160), (0, 0, 0, 0))
        for y in range(40, 120):
            for x in range(40, 120):
                image.putpixel((x, y), (x, y, 64, 255))

        generated = prepare_enemy_battle_front(image)

        self.assertEqual((48, 48), generated.size)
        self.assertLessEqual(generated.width, ENEMY_FRONT_MAX_WIDTH)
        self.assertLessEqual(generated.height, ENEMY_FRONT_MAX_HEIGHT)

    def test_only_enemy_front_uses_final_size_generation(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            graphics = Path(temp_dir)
            for folder in ("Icons", "Front", "Back"):
                (graphics / folder).mkdir()

            icon = Image.new("RGBA", (64, 64), (255, 0, 0, 255))
            front = Image.new("RGBA", (160, 160), (0, 0, 0, 0))
            front.paste((0, 255, 0, 255), (40, 40, 120, 120))
            back = Image.new("RGBA", (160, 160), (0, 0, 255, 255))
            icon.save(graphics / "Icons" / "TEST.png")
            front.save(graphics / "Front" / "TEST.png")
            back.save(graphics / "Back" / "TEST.png")

            writer = generator.AssetWriter()
            missing = []
            with mock.patch.object(generator, "GRAPHICS", graphics):
                generator.add_base_frames(writer, 1, "TEST", missing)

            dimensions = {
                frame["kind"]: (frame["width"], frame["height"])
                for frame in writer.frames
            }
            self.assertEqual([], missing)
            self.assertEqual((48, 48), dimensions["FRONT"])
            self.assertEqual((72, 72), dimensions["BACK"])
            self.assertEqual((64, 64), dimensions["ICON_0"])

    def test_all_configured_walking_frames_exist_and_are_visible(self):
        checked = 0
        static_sequences = set()
        for spec in PMD_SPECS:
            for direction in spec.directions:
                signatures = []
                for index in range(spec.walking_frames):
                    with self.subTest(
                            species=spec.species_id,
                            direction=direction,
                            frame=index):
                        image = Image.open(
                            pmd_walking_frame_path(spec, direction, index)
                        ).convert("RGBA")
                        self.assertIsNotNone(image.getchannel("A").getbbox())
                        signatures.append((image.size, image.tobytes()))
                        checked += 1
                if spec.walking_frames > 1 and len(set(signatures)) == 1:
                    static_sequences.add((spec.species_id, direction))

        expected = sum(
            len(spec.directions) * spec.walking_frames for spec in PMD_SPECS
        )
        self.assertEqual(expected, checked)
        self.assertEqual({(149, "back")}, static_sequences)

    def test_generated_walking_config_matches_source_specs(self):
        source = (ROOT / "src" / "assets" / "PokemonSprites.cpp").read_text()
        rows = re.findall(
            r"^\s*\{(\d+),\s*SpriteKind::.*?,\s*(\d+),\s*(?:true|false)\},$",
            source,
            flags=re.MULTILINE,
        )
        generated = {int(species_id): int(frame_count)
                     for species_id, frame_count in rows}
        expected = {spec.species_id: spec.walking_frames for spec in PMD_SPECS}

        self.assertEqual(expected, generated)

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_route_motion_timing(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "pokemon_motion_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "pokemon_motion_host.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
