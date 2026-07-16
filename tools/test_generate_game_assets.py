#!/usr/bin/env python3

import unittest
from types import SimpleNamespace

from generate_game_assets import MAX_PACK_FRAMES, validate_runtime_pack_limit, write_pack


class GenerateGameAssetsTests(unittest.TestCase):
    def test_generator_frame_limit_matches_runtime(self):
        validate_runtime_pack_limit()

    def test_oversized_pack_is_rejected_before_writing(self):
        writer = SimpleNamespace(
            frames=[{}] * (MAX_PACK_FRAMES + 1),
            data=[],
            palettes=[],
        )
        with self.assertRaisesRegex(ValueError, "frames; limit"):
            write_pack("map", writer)


if __name__ == "__main__":
    unittest.main()
