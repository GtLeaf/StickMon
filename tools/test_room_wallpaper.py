#!/usr/bin/env python3
"""Verify the shipped wall patches with the native decoder and production room packs."""
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image
import generate_room_wallpaper as wallpaper
import prepare_room_background as room
from wallpaper_catalog import exported_wallpapers
from compose_room import composite, render_base

ROOT = room.ROOT
FOREST_PACK = ROOT / 'data/packs/dev/rooms/forest.smonwall'


class WallpaperTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.tmp = Path(cls.temp.name)
        cls.binary = cls.tmp / 'wallpaper-host'
        obj = cls.tmp / 'tinflate.o'
        flags = ['-O1', '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections']
        subprocess.run(['cc', *flags, '-c', 'src/third_party/uzlib/tinflate.c', '-o', obj], cwd=ROOT, check=True)
        strip_unused = '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections'
        subprocess.run(['c++', '-std=c++17', *flags, strip_unused, '-Isrc',
                        'tools/room_wallpaper_host.cpp', 'src/core/RoomResource.cpp', 'src/core/RoomFurniture.cpp',
                        'src/core/DeflateDecoder.cpp', 'src/core/ResourcePack.cpp', 'src/core/ResourceFS.cpp',
                        'src/platform/api/PlatformServices.cpp', 'src/platform/desktop/DesktopPlatform.cpp',
                        obj, '-o', cls.binary], cwd=ROOT, check=True)

    def setUp(self):
        temp = tempfile.TemporaryDirectory(dir=self.tmp)
        self.addCleanup(temp.cleanup)
        self.data = Path(temp.name)
        self.pack = self.data / 'packs/dev'
        (self.pack / 'rooms').mkdir(parents=True)
        for name in ('manifest.json', 'rooms/standard.smonroom'):
            shutil.copyfile(ROOT / 'data/packs/dev' / name, self.pack / name)

    def render(self, mode, valid, wallpaper_id=1):
        out = self.data / 'pixels.rgb565'
        subprocess.run([self.binary, self.data, mode, 'valid' if valid else 'invalid', out, str(wallpaper_id)], check=True)
        return out.read_bytes()

    def test_generated_assets_current(self):
        wallpaper.generate(wallpaper.ART / 'catalog.json', self.data)
        generated_files = [self.data / 'src/assets' / name
                           for name in ('WallpaperIcons.inc', 'WallpaperCatalog.inc')]
        generated_files.extend(exported_wallpapers(self.data / 'data'))
        for generated in generated_files:
            self.assertEqual((ROOT / generated.relative_to(self.data)).read_bytes(), generated.read_bytes())

    def test_baked_and_layered_day_night_match_foreground_compositor(self):
        shutil.copyfile(FOREST_PACK, self.pack / 'rooms/forest.smonwall')
        layout = room.layout_without_preview_sprites(room.read_layout(room.DEFAULT_LAYOUT))
        art = room.scale_room_layout_for_art(layout, room.ROOM_ART_SCALE)
        forest = Image.open(wallpaper.ART / 'forest.png').convert('RGBA')
        mask = Image.open(wallpaper.ART / 'walls-mask.png').getchannel('A')
        self.assertEqual(forest.getchannel('A').tobytes(), mask.tobytes())
        for layered in (False, True):
            if layered:
                shutil.copytree(ROOT / 'origin_asset/generated/room/layered', self.pack, dirs_exist_ok=True)
            for mode in ('day', 'night'):
                with self.subTest(layered=layered, mode=mode):
                    base = room.background_path(room.DEFAULT_LAYOUT, layout, mode)
                    directory = room.DEFAULT_LAYOUT.parent
                    if layered:
                        expected = Image.alpha_composite(render_base(art, base, mode, directory), forest)
                    else:
                        expected = composite(art, base, room.DEFAULT_FURNITURE_DIR, mode,
                                             include_lighting=False, layout_dir=directory, wall_overlay=forest)
                    self.assertEqual(self.render(mode, True), room.rgb565_bytes(expected))

    def test_missing_corrupt_and_mismatched_keep_entire_original(self):
        self.render('day', False)
        original = self.render('day', True, wallpaper_id=0)
        self.assertEqual(self.render('day', False, wallpaper_id=254), original)
        raw = FOREST_PACK.read_bytes()
        entry = wallpaper.HEADER.size
        cases = [raw[:10], raw[:entry + 3]]
        for offset, value in ((entry, 0), (entry+4, len(raw)), (entry+8, 0xffffffff),
                              (entry+12, 0), (entry+16, 0xffffffff)):
            changed = bytearray(raw)
            struct.pack_into('<I', changed, offset, value)
            cases.append(changed)
        for i, data in enumerate(cases):
            with self.subTest(case=i):
                (self.pack / 'rooms/forest.smonwall').write_bytes(data)
                self.render('day', False)


if __name__ == '__main__':
    unittest.main()
