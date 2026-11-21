#!/usr/bin/env python3
"""Actual360px round engine and Kecleon transactions; no OSTB/V1 target builds."""
import argparse,hashlib,json,os,re,subprocess,sys
from concurrent.futures import ThreadPoolExecutor
from contextlib import nullcontext
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def build(out,external=False,sanitize=False,extra_flags=(),debug=True,host_source=None):
    cmake=(ROOT/'firmware/sdgoods/main/CMakeLists.txt').read_text()
    dirs=re.search(r'foreach\(DIR ([^)]+)\)',cmake).group(1).split()
    excluded=set(re.search(r'/ui/compact_touch/\(([^)]+)\)',cmake).group(1).split('|'))
    sources=[p for d in dirs for p in sorted((ROOT/'src'/d).glob('*.cpp'))
             if not(p.parent.name=='compact_touch' and p.stem in excluded)]
    sources += [ROOT/'src/platform/api/PlatformServices.cpp',ROOT/'src/platform/desktop/DesktopPlatform.cpp',ROOT/'firmware/sdgoods/main/RoomSession.cpp',host_source or ROOT/'tools/sdgoods_market_host.cpp']
    obj=out/'tinflate.o'
    subprocess.run(['cc','-O1','-c',str(ROOT/'src/third_party/uzlib/tinflate.c'),'-o',str(obj)],check=True)
    flags=['STICKMON_DISPLAY_WIDTH=360','STICKMON_DISPLAY_HEIGHT=360','STICKMON_ROOM_PIXEL_SCALE=1','STICKMON_TOUCH_INPUT=1','STICKMON_UI_COMPACT_TOUCH=1','STICKMON_UI_ROUND_TOUCH=1','STICKMON_UI_HEADER_INSET_X=60','STICKMON_UI_HEADER_TOP=32','STICKMON_DEDICATED_POWER_BUTTON=1',f'STICKMON_ENABLE_DEBUG_FEATURES={int(debug)}','STICKMON_HAS_CLAW=0',f'STICKMON_EXTERNAL_CONTENT={int(external)}']
    binary=out/'sdgoods-market-host'
    flags += [f'STICKMON_EXTERNAL_MENU_ICONS={int(external)}']
    common=['c++','-std=c++17','-O1','-ffunction-sections','-fdata-sections','-Isrc','-Ifirmware/sdgoods/main','-Ifirmware/common',*['-D'+f for f in flags]]
    if sanitize: common += ['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    common += list(extra_flags)
    # Configuration identity prevents a reused output directory from silently
    # running embedded content when the requested variant is external.
    signature=hashlib.sha256(json.dumps(common).encode()).hexdigest()[:12]
    objects=out/('objects-'+signature);objects.mkdir(exist_ok=True)
    def compile_one(p):
        target=objects/('_'.join(p.relative_to(ROOT).parts)+'.o');dep=target.with_suffix('.d')
        dependencies=[]
        if dep.exists():
            dependencies=[ROOT/x for x in dep.read_text().replace('\\\n',' ').split(':',1)[1].split()]
        if not target.exists() or not dependencies or any(not x.exists() or x.stat().st_mtime>target.stat().st_mtime for x in dependencies):
            subprocess.run([*common,'-MMD','-MF',str(dep),'-c',str(p),'-o',str(target)],cwd=ROOT,check=True)
        return target
    with ThreadPoolExecutor(max_workers=3) as pool:
        compiled=list(pool.map(compile_one,sources))
    strip_unused = '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections'
    subprocess.run(['c++',strip_unused,*(['-fsanitize=address,undefined'] if sanitize else []),*[str(p) for p in compiled],str(obj),'-o',str(binary)],cwd=ROOT,check=True)
    return binary

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--save-only', action='store_true')
    p.add_argument('--external', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--release', action='store_true')
    p.add_argument('--journey', choices=('market-lock',))
    p.add_argument('--catalog-fixture', action='store_true',
                   help='Run the six-entry test catalog through the round market')
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    resource=ROOT/'data'
    if a.external:
        from build_sdgoods_resources import stage
        resource=out/'resource-stage';stage(resource)
    context = nullcontext((resource, []))
    if a.catalog_fixture:
        from wallpaper_catalog_fixture import wallpaper_catalog_fixture
        context = wallpaper_catalog_fixture(resource)
    with context as (resource, flags):
        binary=build(out,a.external,a.sanitize,extra_flags=flags,debug=not a.release)
        subprocess.run([str(binary),str(resource),str(out),a.journey or ('save' if a.save_only else 'all'),'external' if a.external else 'embedded'],cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    from PIL import Image
    for ppm in out.glob('*.ppm'):
        Image.open(ppm).save(ppm.with_suffix('.png'))
        ppm.unlink()
if __name__=='__main__':main()
