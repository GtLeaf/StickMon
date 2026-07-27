#!/usr/bin/env python3

import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from types import SimpleNamespace

from PIL import Image

from generate_game_assets import (
    EXPLORE_PICKUP_CONTENT_SIZE,
    EXPLORE_PICKUP_SIZE,
    MAX_PACK_FRAMES,
    prepare_explore_pickup_marker,
    validate_explore_pickup_pack_mapping,
    validate_runtime_pack_limit,
    write_pack,
)


class GenerateGameAssetsTests(unittest.TestCase):
    def test_generator_frame_limit_matches_runtime(self):
        validate_runtime_pack_limit()

    def test_explore_pickup_marker_is_routed_to_ui_pack(self):
        validate_explore_pickup_pack_mapping()

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
