"""Mystery box artwork and packed-frame/glyph integration."""
import struct
import unittest
import zlib
from PIL import Image
from generate_game_assets import ROOT, KIND_IDS, KIND_ORDER, OUTPUTS, validate_kind_order, validate_committed_pack_kind_ids


class MysteryBoxAssetsTests(unittest.TestCase):
    def test_native_icon_and_pack_registration(self):
        self.assertIn('MARKET_MYSTERY_BOX', KIND_ORDER)
        image = Image.open(ROOT / 'origin_asset/icon/market/mystery-box.png').convert('RGBA')
        self.assertEqual(image.size, (48, 48))
        self.assertEqual(image.getchannel('A').getextrema(), (0, 255))
        bounds = image.getchannel('A').getbbox()
        self.assertGreaterEqual(min(bounds[2] - bounds[0], bounds[3] - bounds[1]), 38)
        content = OUTPUTS['ui'].read_bytes(); count = struct.unpack_from('<H', content, 6)[0]
        raw = zlib.decompress(content[32:], -15)
        frames = [struct.unpack_from('<HHH', raw, i * 22) for i in range(count)]
        self.assertEqual([f for f in frames if f[0] == KIND_IDS['MARKET_MYSTERY_BOX']], [(KIND_IDS['MARKET_MYSTERY_BOX'], 48, 48)])
        validate_kind_order(); validate_committed_pack_kind_ids()

    def test_new_name_glyphs_exist(self):
        for name, size in [('zh16', 32), ('zh32', 128)]:
            data = (ROOT / f'data/packs/dev/fonts/{name}.smonfont').read_bytes()
            count = struct.unpack_from('<H', data, 6)[0]
            glyphs = {chr(struct.unpack_from('<I', data, 16 + i * (4 + size))[0]) for i in range(count)}
            self.assertTrue(set('盲盒') <= glyphs)


if __name__ == '__main__':
    unittest.main()
