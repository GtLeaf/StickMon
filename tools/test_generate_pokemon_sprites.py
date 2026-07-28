#!/usr/bin/env python3

import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib
from unittest import mock
from pathlib import Path

from PIL import Image

import generate_pokemon_sprites as generator
from generate_pokemon_sprites import (
    ENEMY_FRONT_MAX_HEIGHT,
    ENEMY_FRONT_MAX_WIDTH,
    PMD_SPECS,
    prepare_enemy_battle_front,
    prepare_player_battle_back,
    prepare_status_portrait,
    pmd_walking_frame_path,
    trim_alpha_padding,
)


ROOT = Path(__file__).resolve().parents[1]


class GeneratePokemonSpritesTests(unittest.TestCase):
    def test_asset_writer_records_visible_bottom_padding(self):
        image = Image.new("RGBA", (12, 16), (0, 0, 0, 0))
        image.paste((255, 255, 255, 255), (2, 3, 10, 11))

        writer = generator.AssetWriter()
        writer.add_frame(1, "TEST", "FRONT", image)

        self.assertEqual(5, writer.frames[0]["ground_padding"])

    def test_species_pack_encodes_ground_padding_marker(self):
        image = Image.new("RGBA", (12, 16), (0, 0, 0, 0))
        image.paste((255, 255, 255, 255), (2, 3, 10, 11))
        writer = generator.AssetWriter()
        writer.add_frame(1, "TEST", "FRONT", image)

        with tempfile.TemporaryDirectory() as temp_dir:
            pack_path = Path(temp_dir) / "001.smonsp"
            generator.write_species_pack(pack_path, 1, writer, {"FRONT": 7})
            pack = pack_path.read_bytes()

        header_size = struct.calcsize("<IHHHHHHIII")
        payload = zlib.decompress(pack[header_size:], wbits=-15)
        packed_frame = struct.unpack_from("<HBBBBHIII", payload)

        self.assertEqual(
            generator.SPRITE_FRAME_GROUND_MARKER | 5,
            packed_frame[5],
        )

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

    def test_status_portrait_only_scales_down_to_fit_panel(self):
        small = Image.new("RGBA", (160, 160), (0, 0, 0, 0))
        small.paste((255, 255, 255, 255), (55, 45, 103, 121))
        large = Image.new("RGBA", (160, 160), (0, 0, 0, 0))
        large.paste((255, 255, 255, 255), (20, 20, 140, 140))

        self.assertEqual((48, 76), prepare_status_portrait(small).size)
        self.assertEqual((70, 70), prepare_status_portrait(large).size)

    def test_player_back_is_generated_at_final_battle_size(self):
        square = Image.new("RGBA", (160, 160), (255, 255, 255, 255))
        wide = Image.new("RGBA", (160, 160), (0, 0, 0, 0))
        wide.paste((255, 255, 255, 255), (20, 50, 140, 110))

        self.assertEqual((65, 65), prepare_player_battle_back(square).size)
        self.assertEqual((105, 53), prepare_player_battle_back(wide).size)

    def test_base_frames_use_purpose_specific_sizes(self):
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
                generator.add_base_frames(writer, 3, "TEST", missing)

            dimensions = {
                frame["kind"]: (frame["width"], frame["height"])
                for frame in writer.frames
            }
            self.assertEqual([], missing)
            self.assertEqual((48, 48), dimensions["FRONT"])
            self.assertEqual((65, 65), dimensions["BACK"])
            self.assertEqual((64, 64), dimensions["ICON_0"])
            self.assertEqual((70, 70), dimensions["STATUS"])

    def test_no_upscale_species_keeps_grid_aligned_front_size(self):
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
                generator.add_base_frames(writer, 194, "TEST", missing)

            dimensions = {
                frame["kind"]: (frame["width"], frame["height"])
                for frame in writer.frames
            }
            self.assertEqual([], missing)
            # 乌波等易糊精灵不强制放大:80x80 内容保持 72/160 基准比例
            self.assertEqual((36, 36), dimensions["FRONT"])

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

    def test_dynamic_lru_cache_holds_current_and_two_prefetched_areas(self):
        source = (ROOT / "src" / "assets" / "PokemonSprites.cpp").read_text()

        self.assertIn("static constexpr uint8_t DYNAMIC_CACHE_CAP = 18;", source)
        self.assertIn(
            "bool preloadDynamicSpecies(const uint16_t* speciesIds, uint8_t count,",
            source,
        )
        self.assertIn("findCachedSpeciesSprite", source)
        self.assertIn("loadAttempts >= loadBudget", source)
        self.assertIn("uint32_t lastUsed = 0;", source)
        self.assertIn("dynamicCacheSlot(requested, requestedCount)", source)
        self.assertIn("setDynamicSceneSpecies", source)

        explore_source = (
            ROOT / "src" / "scenes" / "ExploreScene.cpp"
        ).read_text()
        self.assertRegex(
            explore_source,
            r"areaPreloadSpeciesIds,\s*areaPreloadSpeciesCount,\s*0\)",
        )
        self.assertRegex(
            explore_source,
            r"areaPreloadSpeciesIds,\s*areaPreloadSpeciesCount,\s*1\)",
        )
        self.assertIn("ahead <= AREA_PRELOAD_AHEAD", explore_source)
        self.assertIn("(areaCursor + ahead) % ROUTE_MAP_COUNT", explore_source)

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
