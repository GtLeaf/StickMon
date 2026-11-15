#!/usr/bin/env python3
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]

def pack(payload, count=3, version=2, flags=4, crc=None, rate=16000, reserved=0):
    return struct.pack('<IHHIIIIIHH', 0x52434d53, version, 25, rate,
                       count, count, len(payload),
                       zlib.crc32(payload) if crc is None else crc, flags, reserved) + payload

class CryPlayerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.root = Path(cls.temp.name)
        cls.binary = cls.root/'cry-player'
        obj=cls.root/'tinflate.o'
        subprocess.run(['cc','-O1','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-c',
                        'src/third_party/uzlib/tinflate.c','-o',str(obj)],cwd=ROOT,check=True)
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-O1',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Isrc',
                        'tools/cry_player_host.cpp','src/core/CryPlayer.cpp',
                        'src/core/DeflateDecoder.cpp','src/core/ResourcePack.cpp',
                        'src/core/ResourceFS.cpp','src/platform/api/PlatformServices.cpp',
                        'src/platform/desktop/DesktopPlatform.cpp',str(obj),'-o',str(cls.binary)],
                       cwd=ROOT,check=True)

    def run_pack(self, data, wanted, species=25):
        with tempfile.TemporaryDirectory(dir=self.root) as tmp:
            root=Path(tmp); folder=root/'packs/dev/cries';folder.mkdir(parents=True)
            (folder.parent/'manifest.json').write_text(json.dumps({'schema':1,'format':'smon-resource-pack-v1','cries':'cries'}))
            (folder/f'{species:03d}.smoncry').write_bytes(data)
            subprocess.run([str(self.binary),str(root),str(int(wanted)),str(species)],check=True)

    def test_adpcm_vector_and_tail(self):
        block=bytes(4)+bytes([0x77])*6+bytes(1014)
        self.run_pack(pack(block,count=13),True)
        data=bytearray(pack(block*2,count=2042));struct.pack_into('<H',data,6,26)
        self.run_pack(data,True,26)

    def test_legacy_deflate_pcm(self):
        pcm=bytes([0,128,255]);c=zlib.compressobj(wbits=-15);payload=c.compress(pcm)+c.flush()
        data=bytearray(pack(payload,version=1,flags=3,crc=zlib.crc32(pcm)))
        struct.pack_into('<H',data,6,27);self.run_pack(data,True,27)

    def test_rejects_corrupt_or_inconsistent_v2(self):
        block=bytes(1024)
        bad=[pack(block,crc=123),pack(block,count=0),pack(block,count=64001),
             pack(block,count=2042),pack(block,rate=8000),pack(block,flags=3),
             pack(block,reserved=1),pack(block,version=3),pack(block)[:-1],
             pack(block)+b'\0',pack(block[:2]+bytes([89])+block[3:]),
             pack(block[:3]+b'\1'+block[4:]),
             pack(block*2,count=2042)[:-1]+b'\1']
        # Corrupt a later block's header but update the payload CRC, ensuring
        # every block is validated independently of the checksum.
        bad.append(pack(block+block[:2]+bytes([89])+block[3:],count=2042))
        for offset, fmt, value in [(0,'I',0),(6,'H',24),(12,'I',0xffffffff),
                                   (16,'I',2)]:
            data=bytearray(pack(block));struct.pack_into('<'+fmt,data,offset,value)
            bad.append(data)
        for data in bad:
            with self.subTest(header=data[:32]):self.run_pack(data,False)

    def test_all_shipped_cries_play_exact_duration(self):
        for p in sorted((ROOT/'data/packs/dev/cries').glob('*.smoncry')):
            h=struct.unpack('<IHHIIIIIHH',p.read_bytes()[:32])
            with self.subTest(species=p.stem):
                subprocess.run([str(self.binary),str(ROOT/'data'),'1',
                                str(int(p.stem)),str(h[4])],check=True,
                               capture_output=True)

if __name__=='__main__':unittest.main()
