#!/usr/bin/env python3

import hashlib
import os
import re
import subprocess
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace

from PIL import Image

from generate_game_assets import (
    EXPLORE_PICKUP_CONTENT_SIZE,
    EXPLORE_PICKUP_SIZE,
    EXPLORE_TILES,
    GAME_ASSETS_HEADER,
    GAME_ASSETS_SOURCE,
    ITEMS,
    KIND_IDS,
    KIND_ORDER,
    MAX_PACK_FRAMES,
    OUTPUTS,
    SHOWER_BRUSH_SIZE,
    SHOWER_BACKGROUND_SIZE,
    SHOWER_BUBBLE_SIZES,
    SHOWER_SOAP_SIZE,
    SHOWER_SPRINKLER_SIZE,
    SHOWER_MENU_SOAP_SIZE,
    SHOWER_MENU_BRUSH_SIZE,
    SHOWER_MENU_SPRINKLER_SIZE,
    SHOWER_SOAP_COUNT,
    TALL_GRASS_PARK_TILES,
    CREEK_TILES,
    Writer,
    expand_stale_pack_selection,
    expected_pack_kind_names,
    expected_writer_kind_ids,
    indexed4_rle,
    prepare_battle_fx_frames,
    prepare_explore_tile,
    prepare_shower_assets,
    prepare_explore_pickup_marker,
    quantize_rgba,
    read_pack_kind_ids,
    validate_explore_pickup_pack_mapping,
    validate_committed_pack_kind_ids,
    validate_explore_tile_mapping,
    validate_kind_order,
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

    def test_tall_grass_park_preserves_existing_kind_ids(self):
        # The 355 IDs already shipped before the park additions remain stable.
        legacy_order = "\n".join(KIND_ORDER[:355]).encode()
        self.assertEqual(
            hashlib.sha256(legacy_order).hexdigest(),
            "dc9eac4133174b97e57bb3162f8045f0b173b7e64eef21a34764ae723cc22152",
        )
        validate_kind_order()
        validate_explore_tile_mapping()

    def test_tall_grass_and_both_tree_variants_are_exported(self):
        exported = {tile_id for _, tile_id in
                    EXPLORE_TILES + TALL_GRASS_PARK_TILES}
        requested = {
            391, 399,
            800, 801, 808, 809, 816, 817,
            804, 805, 812, 813, 820, 821,
            824, 825, 826, 832, 833, 834, 840, 841, 842,
        }
        self.assertTrue(requested <= exported)
        self.assertEqual(len(exported),
                         len(EXPLORE_TILES + TALL_GRASS_PARK_TILES))

    def test_runtime_pack_routing_matches_all_generated_assets(self):
        source = GAME_ASSETS_SOURCE.read_text()
        header = GAME_ASSETS_HEADER.read_text()
        kind_enum = re.search(
            r"enum class Kind\s*:\s*uint16_t\s*\{.*?\};", header, re.S
        ).group(0)
        slot_enum = re.search(
            r"enum class PackSlot\s*:\s*uint8_t\s*\{.*?\};", source, re.S
        ).group(0)
        routing = re.search(
            r"PackSlot packSlotFor\(Kind kind\)\s*\{.*?\n\}", source, re.S
        ).group(0)
        checks = []
        for pack_name, kinds in expected_pack_kind_names().items():
            for kind in kinds:
                checks.append(
                    f"if (packSlotFor(Kind::{kind}) != "
                    f"PackSlot::{pack_name.upper()}) return 1;"
                )
        checks.append("if (packSlotFor(Kind::COUNT) != PackSlot::COUNT) return 2;")
        code = "\n".join((
            "#include <cstdint>", kind_enum, slot_enum, routing,
            "int main() {", *checks, "return 0;", "}",
        ))
        with TemporaryDirectory() as temp_dir:
            cpp_path = Path(temp_dir) / "pack_routing.cpp"
            exe_path = Path(temp_dir) / "pack_routing"
            cpp_path.write_text(code)
            subprocess.run(
                [os.environ.get("CXX", "c++"), "-std=c++17", str(cpp_path),
                 "-o", str(exe_path)], check=True, capture_output=True, text=True,
            )
            subprocess.run([str(exe_path)], check=True)

    def test_expanded_map_pack_fits_the_synchronized_frame_limit(self):
        writer = Writer()
        kinds = expected_pack_kind_names()["map"]
        self.assertEqual(len(kinds), 279)
        for kind in kinds:
            writer.add(kind, Image.new("RGBA", (26, 26), (40, 120, 80, 255)))
        with TemporaryDirectory() as temp_dir:
            pack_path = Path(temp_dir) / "maps.smonfx"
            stats = write_pack("map", writer, pack_path)
            self.assertEqual(read_pack_kind_ids(pack_path),
                             [KIND_IDS[kind] for kind in kinds])
        self.assertEqual(stats["frames"], 279)
        self.assertLessEqual(stats["frames"], MAX_PACK_FRAMES)

    def test_creek_caps_use_unmodified_outside_tiles(self):
        from asset_paths import essentials_dir
        sheet = Image.open(essentials_dir() / "Graphics/Tilesets/Outside.png").convert("RGBA")
        self.assertEqual(CREEK_TILES,
                         [(f"EXPLORE_TILE_{tile}", tile) for tile in (1184, 1186, 1176, 1178, 1187, 1189)])
        for kind, tile_id in CREEK_TILES:
            index = tile_id - 384
            x, y = index % 8 * 32, index // 8 * 32
            original = sheet.crop((x, y, x + 32, y + 32)).resize(
                (26, 26), Image.Resampling.NEAREST)
            self.assertEqual(prepare_explore_tile(tile_id, sheet, ()).tobytes(), original.tobytes())
            self.assertEqual(KIND_IDS[kind], len(KIND_ORDER) - 15 - len(CREEK_TILES) + CREEK_TILES.index((kind, tile_id)))
            if tile_id in (1184, 1186, 1176, 1178):
                self.assertIn(0, original.getchannel("A").tobytes())

    def test_frost_trail_uses_whole_unmodified_native_tiles(self):
        from asset_paths import essentials_dir
        from map_generation_rules import FROST_ROUTE_TILE_SOURCES
        sheet = Image.open(essentials_dir() / "Graphics/Tilesets/Caves.png").convert("RGBA")
        self.assertEqual(len(FROST_ROUTE_TILE_SOURCES), 15)
        for tile_id, source in FROST_ROUTE_TILE_SOURCES.items():
            index = source - 384
            x, y = index % 8 * 32, index // 8 * 32
            original = sheet.crop((x, y, x + 32, y + 32)).resize(
                (26, 26), Image.Resampling.NEAREST)
            self.assertEqual(prepare_explore_tile(source, sheet, ()).tobytes(), original.tobytes())
            self.assertEqual(KIND_IDS[f"EXPLORE_TILE_{tile_id}"],
                             len(KIND_ORDER) - 15 + tile_id - 4780)

    def test_new_grass_and_tree_tiles_preserve_transparent_coverage(self):
        sheet = Image.new("RGBA", (256, 32 * 58), (0, 0, 0, 0))
        for _, tile_id in TALL_GRASS_PARK_TILES:
            offset = tile_id - 384
            left, top = (offset % 8) * 32, (offset // 8) * 32
            sheet.paste((40, 160, 80, 255),
                        (left + 4, top + 6, left + 28, top + 30))
            tile = quantize_rgba(prepare_explore_tile(tile_id, sheet, ()), 16)
            encoded, _ = indexed4_rle(tile)
            decoded_mask = []
            index = 0
            while index < len(encoded):
                run = encoded[index]
                index += 1
                count = run & 0x7FFF
                transparent = bool(run & 0x8000)
                decoded_mask.extend([0 if transparent else 255] * count)
                if not transparent:
                    index += (count + 3) // 4
            with self.subTest(tile=tile_id):
                self.assertEqual(tile.size, (26, 26))
                self.assertEqual(decoded_mask,
                                 list(tile.getchannel("A").tobytes()))
                self.assertIn(0, decoded_mask)
                self.assertIn(255, decoded_mask)

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

    def test_writer_kind_ids_follow_current_enum(self):
        writer = SimpleNamespace(frames=[{"kind": "ITEM_NORMAL_FOOD"}])
        self.assertEqual(expected_writer_kind_ids(writer), [4])

    def test_committed_packs_match_current_kind_ids(self):
        validate_committed_pack_kind_ids()
        expected = expected_pack_kind_names()
        for name, kinds in expected.items():
            with self.subTest(pack=name):
                self.assertEqual(
                    read_pack_kind_ids(OUTPUTS[name]),
                    [expected_writer_kind_ids(
                        SimpleNamespace(frames=[{"kind": kind}])
                     )[0] for kind in kinds],
                )

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

    def test_battle_fx_cross_row_cells_preserve_relative_size_and_anchor(self):
        selected = (
            (2, (255, 0, 0, 255), (20, 40, 44, 64)),
            (3, (0, 255, 0, 255), (80, 40, 128, 88)),
            (4, (0, 0, 255, 255), (80, 100, 104, 124)),
            (5, (255, 255, 0, 255), (20, 100, 68, 148)),
        )
        with TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / "cross_row_fx.png"
            sheet = Image.new("RGBA", (192 * 5, 192 * 2), (0, 0, 0, 0))
            for index, color, box in selected:
                left, top = (index % 5) * 192, (index // 5) * 192
                sheet.paste(color, (left + box[0], top + box[1],
                                    left + box[2], top + box[3]))
            sheet.save(source_path)

            frames = prepare_battle_fx_frames(
                source_path, (2, 3, 4, 5), (1.0, 1.0, 1.0, 1.0))

        boxes = []
        for frame, (_, color, _) in zip(frames, selected):
            self.assertEqual(frame.size, (64, 64))
            self.assertEqual(
                {pixel for _, pixel in frame.getcolors() if pixel[3]}, {color})
            box = frame.getchannel("A").getbbox()
            self.assertIsNotNone(box)
            boxes.append(box)

        widths = [box[2] - box[0] for box in boxes]
        self.assertGreater(widths[1], widths[0] * 1.7)
        self.assertLess(widths[1], widths[0] * 2.3)
        self.assertGreater(widths[3], widths[2] * 1.7)
        centers = [((box[0] + box[2]) / 2,
                    (box[1] + box[3]) / 2) for box in boxes]
        self.assertLess(centers[0][0], centers[1][0])
        self.assertLess(centers[3][0], centers[2][0])
        self.assertLess(centers[0][1], centers[3][1])
        self.assertLess(centers[1][1], centers[2][1])

    def test_battle_fx_soft_alpha_and_fade_reduce_binary_coverage(self):
        with TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / "soft_fx.png"
            sheet = Image.new("RGBA", (192 * 5, 192 * 2), (0, 0, 0, 0))
            sheet.paste((100, 180, 255, 255), (32, 32, 96, 96))
            sheet.paste((100, 180, 255, 128),
                        (192 + 32, 32, 192 + 96, 96))
            sheet.save(source_path)

            frames = prepare_battle_fx_frames(
                source_path, (0, 1, 1, 1), (1.0, 1.0, 0.35, 0.35))

        alphas = [frame.getchannel("A") for frame in frames]
        for alpha in alphas:
            self.assertEqual({value for _, value in alpha.getcolors()},
                             {0, 255})
            self.assertIsNotNone(alpha.getbbox())
        counts = [alpha.histogram()[255] for alpha in alphas]
        self.assertGreater(counts[1], counts[0] * 0.35)
        self.assertLess(counts[1], counts[0] * 0.65)
        self.assertGreater(counts[2], 0)
        self.assertLess(counts[2], counts[1] * 0.50)
        self.assertEqual(alphas[2].tobytes(), alphas[3].tobytes())


if __name__ == "__main__":
    unittest.main()
