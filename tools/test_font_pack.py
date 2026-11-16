import struct
import tempfile
import unittest
import zlib
from pathlib import Path
import generate_font16cn as generator

class FontPackTests(unittest.TestCase):
    def test_generator_writes_lossless_compressed_records(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'test.smonfont'
            generator.write_pack_font(p,['中'],lambda ch:bytes(range(32)))
            b=p.read_bytes()
            self.assertEqual(0x80000000,struct.unpack_from('<I',b,12)[0])
            self.assertEqual(36,struct.unpack_from('<I',b,16)[0])
            self.assertEqual(struct.pack('<I',ord('中'))+bytes(range(32)),zlib.decompress(b[28:],-15))

    def test_transport_roundtrip_is_idempotent(self):
        from font_pack import compress_font, unpack_font
        raw=struct.pack('<IHHBBBBI',0x4e464d53,1,1,16,16,32,0,0)+struct.pack('<I',0x4e2d)+bytes(range(32))
        compressed=compress_font(raw)
        self.assertEqual(raw,unpack_font(compressed))
        self.assertEqual(raw,unpack_font(raw))
        self.assertEqual(compressed,compress_font(compressed))
        for b in [compressed[:-1],compressed+b'\0',compressed[:24]+bytes(4)+compressed[28:]]:
            with self.subTest(data=b[:28]),self.assertRaises(ValueError):unpack_font(b)

if __name__=='__main__':unittest.main()
