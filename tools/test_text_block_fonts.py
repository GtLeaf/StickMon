#!/usr/bin/env python3
"""Large-font punctuation sits at the CJK ink bottom, without changing shape."""
import struct
import unittest
from PIL import ImageFont
import generate_font16cn as gen
from font_pack import unpack_font

def points(bits):
    return {(x,y) for y in range(32) for x in range(32)
            if bits[y*4+x//8] & (128>>(x%8))}

class PunctuationTests(unittest.TestCase):
    def test_low_punctuation_uses_text_bottom(self):
        font=ImageFont.truetype(str(gen.SARASA_FONT_PATH),32)
        reference=max(y for x,y in points(gen.glyph_bytes_large(font,'球')))
        for char in '，。、':
            with self.subTest(char=char):
                ink=points(gen.glyph_bytes_large(font,char))
                self.assertEqual(max(y for x,y in ink),reference)
                self.assertLessEqual(max(y for x,y in ink),30)

    def test_pack_matches_generator(self):
        font=ImageFont.truetype(str(gen.SARASA_FONT_PATH),32)
        data=unpack_font(gen.PACK_FONT_LARGE_REGULAR_OUT.read_bytes())
        _,_,count,w,h,stride,_,_=struct.unpack_from('<IHHBBBBI',data)
        self.assertEqual((w,h,stride),(32,32,128))
        self.assertEqual(len(data),16+count*(4+stride))
        for i in range(count):
            start=16+i*(4+stride);char=chr(struct.unpack_from('<I',data,start)[0])
            with self.subTest(char=char):
                self.assertEqual(data[start+4:start+4+stride],gen.glyph_bytes_large(font,char))

if __name__=='__main__':unittest.main()
