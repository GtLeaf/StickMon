#!/usr/bin/env python3
"""Build the portable photo file service and exercise real file persistence."""
import argparse,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    sources=[ROOT/'tools/photo_library_host.cpp',ROOT/'src/platform/api/PlatformServices.cpp']
    if (ROOT/'src/core/PhotoLibrary.cpp').exists():sources.append(ROOT/'src/core/PhotoLibrary.cpp')
    binary=out/'photo-library'
    subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc',*[str(x) for x in sources],'-o',str(binary)],cwd=ROOT,check=True)
    import tempfile
    with tempfile.TemporaryDirectory(prefix='files-',dir=out) as files:
        subprocess.run([str(binary),files],cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':main()
