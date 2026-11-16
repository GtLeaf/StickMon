#!/usr/bin/env python3
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[1]
NAMES=['zh16','ascii16-unscii','zh32','ascii32','round-zh16-a4','round-zh20-a4']

def compressed(raw):
    h=bytearray(raw[:16]);struct.pack_into('<I',h,12,struct.unpack_from('<I',h,12)[0]|0x80000000)
    c=zlib.compressobj(9,wbits=-15);body=c.compress(raw[16:])+c.flush()
    return h+struct.pack('<III',len(raw)-16,len(body),zlib.crc32(raw[16:]))+body

class FontResourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.temp.cleanup)
        cls.root=Path(cls.temp.name);cls.expected=cls.root/'expected';cls.expected.mkdir()
        cls.raw={}
        # Decode fixture input independently of the production Python codec.
        # No Git history or local-only baseline is required to run the suite.
        for name in NAMES:
            raw=(ROOT/f'data/packs/dev/fonts/{name}.smonfont').read_bytes()
            flags=struct.unpack_from('<I',raw,12)[0]
            if flags & 0x80000000:
                size,encoded,crc=struct.unpack_from('<III',raw,16)
                payload=zlib.decompress(raw[28:],-15)
                assert len(payload)==size and encoded==len(raw)-28 and zlib.crc32(payload)==crc
                h=bytearray(raw[:16]);struct.pack_into('<I',h,12,flags&0x7fffffff)
                raw=bytes(h)+payload
            cls.raw[name]=raw;(cls.expected/(name+'.smonfont')).write_bytes(raw)
        cls.binary=cls.root/'font-host';obj=cls.root/'tinflate.o'
        flags=['-O1','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections']
        subprocess.run(['cc',*flags,'-c','src/third_party/uzlib/tinflate.c','-o',str(obj)],cwd=ROOT,check=True)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-Wl,--gc-sections','-Isrc',
            'tools/font_resource_host.cpp','src/core/FontResource.cpp','src/core/DeflateDecoder.cpp',
            'src/core/ResourcePack.cpp','src/core/ResourceFS.cpp','src/platform/api/PlatformServices.cpp',
            'src/platform/desktop/DesktopPlatform.cpp',str(obj),'-o',str(cls.binary)],cwd=ROOT,check=True)

    def run_fonts(self, data, selected='all', wanted=True):
        with tempfile.TemporaryDirectory(dir=self.root) as tmp:
            root=Path(tmp);fonts=root/'packs/dev/fonts';fonts.mkdir(parents=True)
            (fonts.parent/'manifest.json').write_text(json.dumps({'schema':1,'format':'smon-resource-pack-v1','fonts':'fonts','font':'fonts/zh16.smonfont'}))
            for name,blob in data.items():(fonts/(name+'.smonfont')).write_bytes(blob)
            subprocess.run([str(self.binary),str(root),str(self.expected),selected,str(int(wanted))],check=True,capture_output=True)

    def test_legacy_fonts_match_every_glyph(self):self.run_fonts(self.raw)
    def test_compressed_fonts_match_every_glyph(self):self.run_fonts({n:compressed(b) for n,b in self.raw.items()})
    def test_bad_compressed_payload_is_not_exposed(self):
        for name in ['zh16','zh32','round-zh20-a4']:
            b=compressed(self.raw[name]);bad=[b[:-1],b+b'\0']
            # Keep the advertised file length consistent, so rejection must
            # come from strict DEFLATE completion rather than size checks.
            for encoded in [b[28:-1], b[28:]+b'CORRUPT']:
                header=bytearray(b[:28]);struct.pack_into('<I',header,20,len(encoded))
                bad.append(bytes(header)+encoded)
            c=zlib.compressobj(9,wbits=-15)
            encoded=c.compress(self.raw[name][16:]+b'\0')+c.flush()
            header=bytearray(b[:28]);struct.pack_into('<I',header,20,len(encoded))
            bad.append(bytes(header)+encoded)
            for off,value in [(16,0xffffffff),(20,0xffffffff),(24,123),(12,0x80000080)]:
                v=bytearray(b);struct.pack_into('<I',v,off,value);bad.append(v)
            for off,fmt,value in [(4,'H',3),(6,'H',0),(6,'H',2049),(8,'B',1)]:
                v=bytearray(b);struct.pack_into('<'+fmt,v,off,value);bad.append(v)
            raw=bytearray(self.raw[name]);struct.pack_into('<I',raw,16,0)
            bad.append(compressed(raw))
            for blob in bad:
                with self.subTest(name=name,header=blob[:28]):
                    data=dict(self.raw);data[name]=blob;self.run_fonts(data,name,False)

if __name__=='__main__':unittest.main()
