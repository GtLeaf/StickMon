#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check static/external wire equivalence and the real ContentStore decoder."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
from generate_sdgoods_catalog import ROOT, generate, exporter_source

OUT=ROOT/'.stick-local/artifacts/sdgoods-resource-storage-20261105/content-tests'

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    static=OUT/'static';generate(static)
    root=OUT/'resources';catalog=root/'packs/dev/catalog';catalog.mkdir(parents=True,exist_ok=True)
    for path in ('active.json','packs/dev/manifest.json'):shutil.copyfile(ROOT/'data'/path,root/path)
    for p in static.glob('*.smondata'):shutil.copyfile(p,catalog/p.name)
    source=exporter_source()
    source='''#include "core/ContentStore.h"
#include "platform/desktop/DesktopPlatform.h"
struct TestRecord { uint32_t number; const char* text; bool enabled; };
template<class V> void contentFields(V& v,TestRecord& x) {v(x.number,x.text,x.enabled);}
'''+source
    where=source.index('int main(')
    source=source[:where]+'''template<class T,size_t N,bool Exact> void emit(uint16_t id,const ContentData::Table<T,N,Exact>& values) {
    if(!values.ready()) std::exit(6);
    emitSpan(id,values.data(),values.size());
}
'''+source[where:]
    source=source.replace('if(argc!=2) return 2;','''if(argc!=3 && argc!=4) return 2;
    DesktopPlatform desktop(argv[2]);Platform::bind(desktop.serviceBundle());
    if(!ResourceFS::ins().begin()) return 4;
    ContentStore store;ContentData::bind(&store);
    if(argc==4 && std::string(argv[3])=="extension") {
        const auto* s=findSpecies(999);PokemonSprites::WalkingAnimation walking{};PokemonSprites::PetAnimationProfile pet{};
        if(!s || speciesCount()!=65 || !findLearnset(999) || !PokemonSprites::walkingAnimation(999,PokemonSprites::WalkDirection::DOWN,walking) ||
           !PokemonSprites::petAnimationProfile(999,pet) || !PokemonMotion::profile(999) || !ExploreEcology::profile(999) || !PokemonBattleSizes::front(999).width) return 10;
        const auto* picture=PokemonSprites::findSpeciesSprite(999,PokemonSprites::SpriteKind::FRONT);
        return picture && ExploreGrassPathData::TILES[1]==386 ? 0:11;
    }
    if(argc==4) {
        ContentData::Table<TestRecord,4,false> table{100};
        const bool valid=table.ready();
        if(std::string(argv[3])=="valid") return valid && store.healthy() && table[0].number==0x12345678 && std::string(table[0].text)=="hello" && table[0].enabled ? 0:8;
        return !valid && !store.healthy() ? 0:9;
    }''')
    cpp=OUT/'external.cpp';cpp.write_text(source);binary=OUT/'external'
    inflate=OUT/'tinflate.o'
    subprocess.run(['cc','-O1','-fsanitize=address,undefined','-c','src/third_party/uzlib/tinflate.c','-o',str(inflate)],cwd=ROOT,check=True)
    extra=[]
    if sys.platform!='darwin':
        # ResourcePack uses raw deflate. GNU sanitizer linking also retains
        # uzlib's unused gzip/zlib wrappers, whose checksums are not bundled.
        checksum=OUT/'uzlib_stub.c'
        checksum.write_text('extern "C" {\nunsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n'
                            'unsigned uzlib_crc32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n}\n')
        extra.append(str(checksum))
    subprocess.run(['c++','-std=c++17','-O1','-g','-ffunction-sections','-fdata-sections',
                    '-fsanitize=address,undefined','-DSTICKMON_EXTERNAL_CONTENT=1','-Isrc',str(cpp),
                    'src/game/ExploreEcologyFlight.cpp','src/core/ResourcePack.cpp','src/core/DeflateDecoder.cpp',str(inflate),*extra,'src/platform/desktop/DesktopPlatform.cpp','src/platform/api/PlatformServices.cpp','src/core/ResourceFS.cpp',
                    '-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections','-o',str(binary)],cwd=ROOT,check=True)
    external=OUT/'external-output';external.mkdir(exist_ok=True)
    def run(mode=None):
        subprocess.run([str(binary),str(external),str(root)]+([mode] if mode else []),check=True)
    run()
    for p in static.glob('*.smondata'):assert p.read_bytes()==(external/p.name).read_bytes(),p.name
    # Decoding equivalence alone misses dangling references present in both
    # sources. Run the actual startup validator on valid and broken catalogs.
    from validate_sdgoods_catalog import validate
    validate(root)
    tutor=catalog/'54.smondata';original_tutor=tutor.read_bytes()
    try:
        broken=bytearray(original_tutor);struct.pack_into('<H',broken,16,168)
        tutor.write_bytes(broken)
        try:validate(root)
        except subprocess.CalledProcessError:pass
        else:raise AssertionError('accepted removed held-item tutor move 168')
    finally:tutor.write_bytes(original_tutor)
    payload=struct.pack('<IH',0x12345678,6)+b'hello\0'+b'\x01'
    good=struct.pack('<IHHII',0x44434d53,1,100,1,len(payload))+payload
    testfile=catalog/'100.smondata'
    testfile.write_bytes(good);run('valid')
    bad=[b'',good[:15],good[:-1],good+b'\0']
    for at,value in ((0,0),(4,2),(6,101),(8,0),(8,5),(12,255),(20,0),(20,255),(27,1),(28,2)):
        item=bytearray(good);item[at]=value;bad.append(item)
    for n in range(16,len(good)):
        item=bytearray(good[:n]);struct.pack_into('<I',item,12,n-16);bad.append(item)
    for data in bad:testfile.write_bytes(data);run('invalid')
    testfile.unlink();run('invalid')
    # Append a playable alias using existing supported rules. Reuse the same binary.
    def append_alias(ident,record_size):
        path=catalog/f'{ident}.smondata';raw=bytearray(path.read_bytes())
        n,size=struct.unpack_from('<II',raw,8)
        row=bytearray(raw[16:16+record_size]);struct.pack_into('<H',row,0,999)
        raw+=row;struct.pack_into('<II',raw,8,n+1,size+len(row));path.write_bytes(raw)
    species=(catalog/'1.smondata').read_bytes();first_size=22+struct.unpack_from('<H',species,18)[0]
    append_alias(1,first_size)
    for ident in (5,6,7,8,9,25,26,28):
        raw=(catalog/f'{ident}.smondata').read_bytes();n,size=struct.unpack_from('<II',raw,8)
        assert size%n==0
        append_alias(ident,size//n)
    path=catalog/'10.smondata';raw=bytearray(path.read_bytes());struct.pack_into('<H',raw,18,386);path.write_bytes(raw)
    sprite=bytearray((ROOT/'data/packs/dev/sprites/001.smonsp').read_bytes());struct.pack_into('<H',sprite,6,999)
    sprite_dir=root/'packs/dev/sprites';sprite_dir.mkdir(exist_ok=True);(sprite_dir/'999.smonsp').write_bytes(sprite)
    run('extension')
    print(f'PASS {len(list(static.glob("*.smondata")))} tables identical after external decode/re-encode; {len(bad)+1} malformed/short catalog cases rejected; resource-only new species and tile replacement passed')

if __name__=='__main__':main()
