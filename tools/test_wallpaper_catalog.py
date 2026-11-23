#!/usr/bin/env python3
"""Exercise authoring and staging through a sparse-ID catalog with an extra finish."""
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

from PIL import Image
from font_pack import unpack_font

import generate_room_wallpaper as author
from pack_sdgoods_resources import ENTRY, pack


class WallpaperCatalogTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.art = self.root / 'art'
        shutil.copytree(author.ART, self.art)
        self.catalog = self.art / 'catalog.json'

    def test_exports_extension_and_stages_only_declared_wallpapers(self):
        self.assertTrue(self.catalog.is_file(), 'authoritative room wallpaper catalog missing')
        from wallpaper_catalog import exported_wallpapers
        from generate_font16cn import wallpaper_chars
        production = {entry['id']: entry for entry in json.loads(self.catalog.read_text())}
        entries = [production[0], production[1]]
        entries.insert(1, {'id': 37, 'name': '海浜', 'price': 90, 'asset': 'sea',
                           'swatch': 'swatch-sea.png'})
        self.catalog.write_text(json.dumps(entries, ensure_ascii=False))
        swatch = Image.new('RGBA', (32, 32), (255, 0, 0, 255))
        swatch.save(self.art / 'swatch-sea.png')
        with Image.open(self.art / 'forest.png') as source:
            overlay = Image.new('RGBA', source.size, (255, 0, 0))
            overlay.putalpha(source.getchannel('A'))
            overlay.save(self.art / 'sea.png')
        output = self.root / 'generated'
        fonts = output / 'data/packs/dev/fonts'
        shutil.copytree(author.room.ROOT / 'data/packs/dev/fonts', fonts)
        def glyphs(path):
            data = unpack_font(path.read_bytes())
            stride = data[10]
            return {chr(struct.unpack_from('<I', data, i)[0]): data[i+4:i+4+stride]
                    for i in range(16, len(data), 4+stride)}
        old_fonts = {p: glyphs(p) for p in fonts.iterdir() if 'zh' in p.name}
        author.generate(self.catalog, output)
        for path, old in old_fonts.items():
            installed = glyphs(path)
            self.assertTrue(set('海浜') <= installed.keys(), path.name)
            self.assertEqual(old, {c: installed[c] for c in old}, path.name)
        data = output / 'data'
        rooms = data / 'packs/dev/rooms'
        self.assertNotEqual((rooms / 'sea.smonwall').read_bytes(), (rooms / 'forest.smonwall').read_bytes())
        self.assertEqual((rooms / 'forest.smonwall').read_bytes(),
                         (author.room.PACK_ROOM_OUT / 'forest.smonwall').read_bytes())
        (rooms / 'unused.smonwall').write_bytes(b'old export')
        self.assertEqual(set(exported_wallpapers(data, self.catalog)),
                         {rooms / 'forest.smonwall', rooms / 'sea.smonwall'})
        archive = self.root / 'resources.bin'
        pack(data, archive, catalog=self.catalog)
        packed = archive.read_bytes()
        count, = struct.unpack_from('<I', packed, 4)
        index = {}
        for i in range(count):
            name, offset, size = ENTRY.unpack_from(packed, 8 + i * ENTRY.size)
            index[name.rstrip(b'\0').decode('ascii')] = packed[offset:offset+size]
        self.assertEqual({name for name in index if name.endswith('.smonwall')},
                         {'/packs/dev/rooms/forest.smonwall', '/packs/dev/rooms/sea.smonwall'})
        self.assertEqual(index['/packs/dev/rooms/sea.smonwall'], (rooms / 'sea.smonwall').read_bytes())
        self.assertTrue(set('海浜') <= wallpaper_chars(self.catalog))
        includes = output / 'src/assets'
        test_program = self.root / 'main.cpp'
        test_program.write_text("""
#include "game/WallpaperCatalog.h"
#include "presentation/WallpaperArt.h"
using namespace Game::MarketWorkshop;
static_assert(WALLPAPER_COUNT == 3);
static_assert(wallpaperIndex(static_cast<Wallpaper>(37)) == 1);
static_assert(wallpaperIndex(Wallpaper::FOREST) == 2);
static_assert(wallpaperEntry(static_cast<Wallpaper>(37))->price == 90);
static_assert(wallpaperEntry(static_cast<Wallpaper>(38)) == nullptr);
static_assert(WallpaperArt::Detail::Swatches[1].palette[1] == 0xf800);
int main() {}
""")
        subprocess.run(['c++', '-std=c++17', '-fsyntax-only', '-I', author.room.ROOT / 'src',
                        f'-DSTICKMON_WALLPAPER_CATALOG_INCLUDE="{includes / "WallpaperCatalog.inc"}"',
                        f'-DSTICKMON_WALLPAPER_ICONS_INCLUDE="{includes / "WallpaperIcons.inc"}"',
                        test_program], check=True)
        self.catalog.write_text(json.dumps([entries[0], {**entries[1], 'price': 19}, entries[2]]))
        with self.assertRaisesRegex(ValueError, 'WallpaperCatalog.inc'):
            exported_wallpapers(data, self.catalog)
        self.catalog.write_text(json.dumps(entries))
        (rooms / 'sea.smonwall').unlink()
        with self.assertRaisesRegex(ValueError, 'sea.smonwall'):
            exported_wallpapers(data, self.catalog)

    def test_name_length(self):
        from wallpaper_catalog import load_catalog
        entries = json.loads(self.catalog.read_text())
        for name in ('林', '海盐小屋', '青A12'):
            entries[0]['name'] = name
            self.catalog.write_text(json.dumps(entries))
            self.assertEqual(load_catalog(self.catalog)[0]['name'], name)
        entries[0]['name'] = '海盐蓝小屋'
        self.catalog.write_text(json.dumps(entries))
        with self.assertRaisesRegex(ValueError, 'exceeds 4 characters'):
            load_catalog(self.catalog)

    def test_invalid_catalog_rejected(self):
        self.assertTrue(self.catalog.is_file(), 'authoritative room wallpaper catalog missing')
        from wallpaper_catalog import load_catalog
        entries = {entry['id']: entry for entry in json.loads(self.catalog.read_text())}
        for change in ({'id': 0}, {'id': -1}, {'id': 256}, {'id': True}, {'price': -1},
                       {'price': 65536}, {'price': False}, {'name': '  '}, {'name': 'foo\nbar'},
                       {'asset': '../escape'}, {'asset': None}, {'swatch': '../source.png'}):
            with self.subTest(change=change):
                self.catalog.write_text(json.dumps([entries[0], {**entries[1], **change}]))
                with self.assertRaises(ValueError):
                    load_catalog(self.catalog)


if __name__ == '__main__':
    unittest.main()
