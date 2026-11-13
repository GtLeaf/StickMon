"""Market item source preparation, append-only pack install, and glyph retention."""
import importlib.util
import hashlib
import json
import os
import re
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

from PIL import Image
from generate_game_assets import ROOT, KIND_ORDER, OUTPUTS, Writer, write_pack


class MarketItemAssetsTests(unittest.TestCase):
    def module(self):
        path = ROOT / 'tools/prepare_market_item_icons.py'
        self.assertTrue(path.exists(), 'maintained market item preparation script is missing')
        spec = importlib.util.spec_from_file_location('market_items', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_preparation_preserves_transparency_and_exact_twofold_pixels(self):
        module = self.module()
        source = ROOT / 'origin_asset/icon/market/items/source/hp_up.png'
        image = module.prepare_icon(source)
        with Image.open(source) as original:
            expected = original.convert('RGBA').resize((48, 48), Image.Resampling.NEAREST)
        self.assertEqual(image.tobytes(), expected.tobytes())
        self.assertEqual(image.getchannel('A').getextrema(), (0, 255))

    def test_shared_vitamin_uses_original_item_palette(self):
        module = self.module()
        root = ROOT / 'origin_asset/icon/market/items/source'
        images = [module.prepare_icon(root / 'vitamin.png', root / f'{name}.pal')
                  for name in ('protein', 'iron', 'calcium', 'zinc', 'carbos')]
        self.assertEqual(len({image.tobytes() for image in images}), 5)
        self.assertEqual(len({image.getchannel('A').tobytes() for image in images}), 1)
        with Image.open(root / 'vitamin.png') as source:
            index = next(i for i, value in enumerate(source.tobytes()) if value == 4)
        x, y = (index % 24) * 2, (index // 24) * 2
        self.assertEqual(images[0].getpixel((x, y)), (197, 98, 98, 255))

    def test_install_appends_without_changing_legacy_records_and_is_idempotent(self):
        module = self.module()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'ui.smonfx'
            writer = Writer()
            writer.add('ITEM_POTION', Image.new('RGBA', (36, 36), (80, 160, 220, 255)))
            write_pack('ui', writer, path)
            before = module.read_writer(path)
            image = module.prepare_icon(ROOT / 'origin_asset/icon/market/items/source/hp_up.png')
            module.install_icons({'MARKET_ITEM_HP_UP': image}, path)
            after = module.read_writer(path)
            self.assertEqual(after.frames[0], before.frames[0])
            self.assertEqual(after.data[:len(before.data)], before.data)
            self.assertEqual(after.palettes[:len(before.palettes)], before.palettes)
            self.assertEqual(after.frames[-1]['kind'], 'MARKET_ITEM_HP_UP')
            first = path.read_bytes()
            module.install_icons({'MARKET_ITEM_HP_UP': image}, path)
            self.assertEqual(first, path.read_bytes())
            data = bytearray(first)
            struct.pack_into('<I', data, 28, 0)
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'CRC'):
                module.read_writer(path)

    def test_glyph_extension_preserves_existing_bytes_and_excludes_unscii(self):
        module = self.module()
        from generate_font16cn import write_pack_font
        with tempfile.TemporaryDirectory() as directory:
            for name, size, stride, version in [('zh16', 16, 32, 1), ('zh32', 32, 128, 1), ('round', 16, 128, 2)]:
                path = Path(directory) / name
                old = bytes([42] * stride)
                if version == 1:
                    write_pack_font(path, ['旧'], lambda ch: old, size, size, stride)
                else:
                    path.write_bytes(struct.pack('<IHHBBBBI', 0x4e464d53, 2, 1, size, size, stride, 0, 4) + struct.pack('<I', ord('旧')) + old)
                module.extend_font(path, {'旧', '锌', 'A', '♂'})
                data = path.read_bytes()
                count = struct.unpack_from('<H', data, 6)[0]
                glyphs = {chr(struct.unpack_from('<I', data, 16 + i * (4 + stride))[0]): data[20 + i * (4 + stride):20 + i * (4 + stride) + stride] for i in range(count)}
                self.assertEqual(set(glyphs), {'旧', '锌'})
                self.assertEqual(glyphs['旧'], old)
                self.assertTrue(any(glyphs['锌']))
                first = path.read_bytes()
                module.extend_font(path, {'旧', '锌'})
                self.assertEqual(first, path.read_bytes())

    def test_replacing_new_icon_preserves_frame_order_and_deduplicates_shared_sprite(self):
        module = self.module()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'ui.smonfx'
            writer = Writer()
            writer.add('ITEM_POTION', Image.new('RGBA', (36, 36), (80, 160, 220, 255)))
            write_pack('ui', writer, path)
            image = module.prepare_icon(ROOT / 'origin_asset/icon/market/items/source/hp_up.png')
            module.install_icons({'MARKET_ITEM_HP_UP': image, 'MARKET_ITEM_PROTEIN': image}, path)
            before = module.read_writer(path)
            self.assertEqual(before.frames[1]['offset'], before.frames[2]['offset'])
            changed = image.copy()
            changed.putpixel((20, 20), (255, 0, 0, 255))
            module.install_icons({'MARKET_ITEM_HP_UP': changed}, path)
            after = module.read_writer(path)
            self.assertEqual([f['kind'] for f in before.frames], [f['kind'] for f in after.frames])

    def test_all_market_icons_registered_at_end_and_installed_in_ui_pack(self):
        module = self.module()
        from generate_game_assets import MARKET_ITEMS, validate_kind_order, validate_committed_pack_kind_ids, indexed4_rle
        self.assertEqual(len(MARKET_ITEMS), 17)
        self.assertEqual(KIND_ORDER[1542:], [kind for kind, _ in MARKET_ITEMS])
        self.assertEqual(hashlib.sha256('\n'.join(KIND_ORDER[:1542]).encode()).hexdigest(),
                         '9a76ee93b62c40ce2179dffcfafad3cf0aaa61750514cad3d2ea8d1e0be12ecc')
        writer = module.read_writer(OUTPUTS['ui'])
        self.assertEqual([frame['kind'] for frame in writer.frames[-17:]], [kind for kind, _ in MARKET_ITEMS])
        for kind, filename in MARKET_ITEMS:
            with Image.open(ROOT / 'origin_asset/icon/market/items' / filename) as image:
                image = image.convert('RGBA')
            self.assertEqual(image.size, (48, 48))
            self.assertEqual(image.getchannel('A').getextrema(), (0, 255))
            name = Path(filename).stem
            shared = name in module.VITAMINS
            source_root = ROOT / 'origin_asset/icon/market/items/source'
            original = module.prepare_icon(source_root / ('vitamin.png' if shared else filename),
                source_root / f'{name}.pal' if shared else None)
            self.assertEqual(image.tobytes(), original.tobytes())
            encoded, palette = indexed4_rle(image)
            frame = next(frame for frame in writer.frames if frame['kind'] == kind)
            self.assertEqual(writer.data[frame['offset']:frame['offset'] + frame['length']], encoded)
            self.assertEqual(writer.palettes[frame['palette_offset']:frame['palette_offset'] + frame['palette_size']], palette)
        validate_kind_order()
        validate_committed_pack_kind_ids()

    def test_original_source_blobs_match_recorded_git_sha1(self):
        source_root = ROOT / 'origin_asset/icon/market/items/source'
        sources = json.loads((source_root / 'sources.json').read_text())
        self.assertEqual(len(sources), 18)
        for source in sources:
            content = (source_root / source['file']).read_bytes()
            self.assertEqual(hashlib.sha1(f'blob {len(content)}\0'.encode() + content).hexdigest(), source['gitBlobSha'])

    def test_catalog_text_glyphs_available_in_all_regular_fonts(self):
        from generate_font16cn import collect_chars_from_text, collect_ui_chars, UNSCII_CHARS
        chars = collect_chars_from_text((ROOT / 'src/game/MarketItemCatalog.h').read_text()) - UNSCII_CHARS
        self.assertTrue(chars <= collect_ui_chars())
        for name in ('zh16', 'zh32', 'round-zh16-a4'):
            data = (ROOT / f'data/packs/dev/fonts/{name}.smonfont').read_bytes()
            count = struct.unpack_from('<H', data, 6)[0]
            stride = data[10]
            glyphs = {chr(struct.unpack_from('<I', data, 16 + i * (4 + stride))[0]) for i in range(count)}
            self.assertTrue(chars <= glyphs, f'{name} missing: {chars - glyphs}')

    def test_native_amoled_pack_has_market_icons_at_48_pixels(self):
        module = self.module()
        from generate_game_assets import MARKET_ITEMS
        native = module.read_writer(ROOT / 'data/packs/dev/game/ui_amoled.smonfx')
        frames = {frame['kind']: frame for frame in native.frames}
        for kind in ['MARKET_MYSTERY_BOX', *(kind for kind, _ in MARKET_ITEMS)]:
            self.assertIn(kind, frames)
            self.assertEqual((frames[kind]['width'], frames[kind]['height']), (48, 48))

    def test_runtime_maps_new_items_and_draws_native_48_pixels(self):
        from generate_game_assets import MARKET_ITEMS
        source = (ROOT / 'src/assets/GameAssets.cpp').read_text()
        mapping = re.search(r'Kind itemKind\(.*?\n\}', source, re.S).group(0)
        draw = re.search(r'bool drawItemIcon48\(.*?\n\}', source, re.S).group(0)
        checks = []
        for kind, _ in MARKET_ITEMS:
            item = kind.removeprefix('MARKET_ITEM_')
            checks.append(f'if (itemKind(Game::ItemId::{item}) != Kind::{kind}) return 1;')
            checks.append(f'calls=0; if (!drawItemIcon48(Game::ItemId::{item}, 20, 30) || calls != 1 || seenKind != Kind::{kind} || seenScale != 1.0f || seenX != 20 || seenY != 30) return 2;')
        code = '\n'.join(['#include "assets/GameAssets.h"', 'namespace GameAssets {',
            'int calls=0, seenX=0, seenY=0; Kind seenKind; float seenScale;',
            'bool drawCentered(Kind kind,int x,int y,float scale,bool) { ++calls; seenKind=kind; seenScale=scale; seenX=x; seenY=y; return true; }',
            mapping, draw, '}', 'int main() { using namespace GameAssets;', *checks, 'return 0; }'])
        with tempfile.TemporaryDirectory() as directory:
            cpp, exe = Path(directory) / 'mapping.cpp', Path(directory) / 'mapping'
            cpp.write_text(code)
            subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-I', str(ROOT / 'src'), str(cpp), '-o', str(exe)], check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
