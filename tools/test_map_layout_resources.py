import struct
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def table(payload,count=49,key=10,crc=None):
    c=zlib.compressobj(9,wbits=-15);encoded=c.compress(payload)+c.flush()
    return struct.pack('<IHHIIII',0x44434d53,2,key,count,len(payload),len(encoded),zlib.crc32(payload) if crc is None else crc)+encoded

class MapLayoutResourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.tmp.cleanup)
        cls.root=Path(cls.tmp.name);cls.binary=cls.root/'store'
        obj=cls.root/'tinflate.o'
        subprocess.run(['cc','-O1','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-c','src/third_party/uzlib/tinflate.c','-o',str(obj)],cwd=ROOT,check=True)
        subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer',
            '-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DSTICKMON_EXTERNAL_MAP_LAYOUTS=1','-Isrc',
            'tools/content_store_map_host.cpp','src/core/DeflateDecoder.cpp','src/core/ResourceFS.cpp',
            'src/platform/api/PlatformServices.cpp','src/platform/desktop/DesktopPlatform.cpp',str(obj),'-o',str(cls.binary)],cwd=ROOT,check=True)
    def run_table(self,data,wanted):
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            p=Path(directory)/'packs/dev/catalog';p.mkdir(parents=True);(p/'10.smondata').write_bytes(data)
            subprocess.run([str(self.binary),directory,str(int(wanted))],check=True,capture_output=True)
    def test_compressed_map_table_decodes_exact_fields(self):
        self.run_table(table(struct.pack('<49H',*range(49))),True)
    def test_bad_table_not_cached(self):
        raw=struct.pack('<49H',*range(49));b=table(raw)
        for bad in [table(raw,crc=0),table(raw,count=0),table(raw,count=50),table(raw,key=11),b[:-1],b+b'\0']:
            self.run_table(bad,False)

    @classmethod
    def parity_binary(cls,external):
        binary=cls.root/f'parity-{external}'
        if not binary.exists():
            subprocess.run(['c++','-std=c++17','-O1','-ffunction-sections','-fdata-sections',
                '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                '-Wl,--gc-sections','-Isrc',f'-DSTICKMON_EXTERNAL_MAP_LAYOUTS={external}',
                'tools/map_layout_parity_host.cpp','src/game/ExploreMapGenerator.cpp',
                'src/core/MapLayoutResources.cpp','src/core/DeflateDecoder.cpp','src/core/ResourceFS.cpp',
                'src/platform/api/PlatformServices.cpp','src/platform/desktop/DesktopPlatform.cpp',
                str(cls.root/'tinflate.o'),'-o',str(binary)],cwd=ROOT,check=True)
        return binary

    def test_bad_layouts_block_preload(self):
        import shutil
        binary=self.parity_binary(1)
        for key,offset,value in [(11,576,255),(13,1156,255),(19,62,255),
                (20,2,255),(15,2,0),(15,2,255),(12,1,255),(13,1156,0),
                (16,1224,0),(16,1224,1),(10,None,0)]:
            with self.subTest(key=key),tempfile.TemporaryDirectory(dir=self.root) as directory:
                root=Path(directory);catalog=root/'packs/dev/catalog';catalog.mkdir(parents=True)
                for p in (ROOT/'data/packs/dev/catalog').glob('*.smondata'):shutil.copyfile(p,catalog/p.name)
                path=catalog/f'{key}.smondata'
                if offset is None:path.unlink()
                else:
                    data=path.read_bytes();count=struct.unpack_from('<I',data,8)[0]
                    payload=bytearray(zlib.decompress(data[24:],-15));payload[offset]=value
                    path.write_bytes(table(payload,count=count,key=key))
                subprocess.run([str(binary),directory,str(root/'unused.bin'),'0'],check=True,capture_output=True)

    def test_embedded_and_external_maps_match_without_runtime_io(self):
        import shutil
        binaries=[self.parity_binary(i) for i in [0,1]]
        with tempfile.TemporaryDirectory(dir=self.root) as directory:
            root=Path(directory);catalog=root/'packs/dev/catalog';catalog.mkdir(parents=True)
            for p in (ROOT/'data/packs/dev/catalog').glob('*.smondata'):shutil.copyfile(p,catalog/p.name)
            outputs=[]
            for i,binary in enumerate(binaries):
                output=root/f'maps-{i}.bin';outputs.append(output)
                subprocess.run([str(binary),str(root),str(output),'1'],check=True)
            self.assertGreater(outputs[0].stat().st_size,1000000)
            self.assertEqual(outputs[0].read_bytes(),outputs[1].read_bytes())
if __name__=='__main__':unittest.main()
