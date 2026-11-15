"""Export runtime map screenshots; never invent terrain or change live assets."""
from pathlib import Path
import argparse
import subprocess

from PIL import Image
from amoled_source import amoled_shared_sources

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    # Function sections let the linker discard unused zlib/gzip wrappers.
    obj=output/'tinflate.o'
    subprocess.run(['cc','-O1','-ffunction-sections','-fdata-sections','-c',
                    str(ROOT/'src/third_party/uzlib/tinflate.c'),'-o',str(obj)],check=True)
    sources=amoled_shared_sources(ROOT)+[
        ROOT/'src/platform/desktop/DesktopPlatform.cpp',
        ROOT/'tools/capture_explore_cover_maps.cpp']
    binary=output/'capture'
    subprocess.run(['c++','-std=c++17','-O1','-ffunction-sections','-fdata-sections',
                    '-Wl,-dead_strip' if __import__('sys').platform=='darwin' else '-Wl,--gc-sections',
                    '-Isrc','-DSTICKMON_HAS_CLAW=0',
                    *[str(p) for p in sources if p.suffix=='.cpp'],str(obj),'-o',str(binary)],cwd=ROOT,check=True)
    result=subprocess.check_output([str(binary),str(ROOT/'data'),str(output)],text=True)
    (output/'fingerprints.txt').write_text(result)
    for path in output.glob('*.ppm'):
        with Image.open(path) as image:image.save(path.with_suffix('.png'))
    print(result,end='')


if __name__=='__main__':main()
