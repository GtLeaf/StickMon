#!/usr/bin/env python3
"""Exercise owned home furniture through maintained touch applications."""
import argparse
import os
import subprocess
from pathlib import Path

from amoled_host import ROOT,build_amoled_host
from test_sdgoods_market import build


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--native-cache',type=Path)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--native-only',action='store_true')
    parser.add_argument('--amoled-only',action='store_true')
    args=parser.parse_args()
    if args.native_only and args.amoled_only:parser.error('Choose one application or both')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'}
    if not args.amoled_only:
        cache=(args.native_cache or out/'native').resolve();cache.mkdir(parents=True,exist_ok=True)
        binary=build(cache,sanitize=args.sanitize,host_source=ROOT/'tools/home_library_host.cpp')
        subprocess.run([str(binary),str(ROOT/'data'),str(out)],cwd=ROOT,env=env,check=True)
    if not args.native_only:
        dest=out/'amoled';dest.mkdir(exist_ok=True)
        binary=build_amoled_host(out/'amoled-host',[ROOT/'tools/home_library_amoled_host.cpp',ROOT/'tools/amoled_checksum_guard.cpp'],debug=False)
        subprocess.run([str(binary),str(ROOT/'data'),str(dest)],cwd=ROOT,env=env,check=True)
    from PIL import Image
    for ppm in out.rglob('*.ppm'):
        Image.open(ppm).save(ppm.with_suffix('.png'))
        ppm.unlink()


if __name__=='__main__':main()
