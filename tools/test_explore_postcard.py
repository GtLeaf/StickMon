"""Render and verify both postcard trial and the compiled legacy fallback."""
from pathlib import Path
import argparse,subprocess,os,sys
from amoled_host import ROOT,build_amoled_host
def main():
    p=argparse.ArgumentParser();p.add_argument('--legacy',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    original=subprocess.run
    if not a.legacy:
        env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'}
        layout=out/'layout'
        subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer','-Isrc','-Ifirmware/amoled_1_8_v1/main',
                        'tools/explore_postcard_layout_host.cpp','-o',str(layout)],cwd=ROOT,check=True)
        subprocess.run([str(layout)],check=True,env=env)
        sys.path.insert(0,str(ROOT/'tools/room_editor'))
        from generate_sprite_previews import parse_kind_names,load_sprite_pack
        kinds=parse_kind_names((ROOT/'src/assets/PokemonSprites.h').read_text())
        checked=0
        for pack in (ROOT/'data/packs/dev/sprites').glob('*.smonsp'):
            _,frames,_,_=load_sprite_pack(pack,kinds)
            for frame in frames:
                if frame['kindName']=='FRONT':
                    assert frame['width']<=312 and frame['height']<=132,(pack,frame)
                    checked+=1
        assert checked>=64
        print(f'postcard-assets: {checked} native FRONT frames fit the foreground',flush=True)
    if a.legacy:
        def run(cmd,*args,**kwargs):
            if isinstance(cmd,list) and cmd and cmd[0]=='c++':cmd=[*cmd,'-DSTICKMON_EXPLORE_POSTCARD=0']
            return original(cmd,*args,**kwargs)
        subprocess.run=run
    binary=build_amoled_host(out/'journey',[ROOT/'tools/explore_postcard_amoled_host.cpp'],debug=True)
    subprocess.run([str(binary),str(ROOT/'data'),str(out)],check=True)
    from PIL import Image
    for image in out.glob('*.ppm'):Image.open(image).save(image.with_suffix('.png'))
if __name__=='__main__':main()
