#!/usr/bin/env python3

import os
import unittest
from pathlib import Path
from unittest.mock import patch

from asset_paths import DEFAULT_ESSENTIALS_DIR, essentials_dir, portable_asset_path


class AssetPathTests(unittest.TestCase):
    def test_default_essentials_directory_is_inside_ignored_external_folder(self):
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(DEFAULT_ESSENTIALS_DIR, essentials_dir())
            self.assertEqual("external", essentials_dir().parent.name)

    def test_environment_override_expands_home(self):
        with patch.dict(os.environ, {"ESSENTIALS_DIR": "~/pokemon-assets"}):
            self.assertEqual(Path.home() / "pokemon-assets", essentials_dir())

    def test_manifest_path_is_relative_to_asset_root(self):
        source = DEFAULT_ESSENTIALS_DIR / "Graphics" / "Tilesets" / "Caves.png"
        self.assertEqual(
            "Graphics/Tilesets/Caves.png",
            portable_asset_path(source, DEFAULT_ESSENTIALS_DIR),
        )

    def test_external_source_falls_back_to_filename(self):
        self.assertEqual(
            "Caves.png",
            portable_asset_path(Path("/private/assets/Caves.png"), DEFAULT_ESSENTIALS_DIR),
        )


if __name__ == "__main__":
    unittest.main()
