#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_STRINGS = ROOT / "src" / "core" / "UiStrings.h"
AREA_KEYS = (
    "GRASS_PATH",
    "CREEK_SLOPE",
    "TALL_GRASS_PARK",
    "FROST_CRYSTAL_CAVE",
    "MIST_FOREST_PATH",
    "ANCIENT_WATERFALL_VALLEY",
)


class ExploreAreaNameTests(unittest.TestCase):
    def test_all_explore_area_names_use_three_characters(self):
        source = UI_STRINGS.read_text(encoding="utf-8")
        for key in AREA_KEYS:
            match = re.search(
                rf'\b{key}\s*=\s*"([^"]+)"',
                source,
            )
            self.assertIsNotNone(match, f"missing explore area string: {key}")
            self.assertEqual(len(match.group(1)), 3, key)


if __name__ == "__main__":
    unittest.main()
