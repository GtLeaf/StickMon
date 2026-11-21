#!/usr/bin/env python3
"""Exercise catalog browsing on both workshop layouts under address/undefined sanitizers."""
import subprocess
import tempfile
import unittest
from pathlib import Path

from wallpaper_catalog_fixture import wallpaper_catalog_fixture

ROOT = Path(__file__).resolve().parents[1]


class WorkshopStripTests(unittest.TestCase):
    def test_expanded_strip(self):
        with tempfile.TemporaryDirectory() as directory, wallpaper_catalog_fixture(ROOT / 'data') as fixture:
            _, flags = fixture
            host = Path(directory) / 'workshop-strip'
            subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Isrc', *flags,
                            'tools/workshop_swatch_strip_host.cpp', '-o', str(host)], cwd=ROOT, check=True)
            subprocess.run([str(host)], check=True)


if __name__ == '__main__':
    unittest.main()
