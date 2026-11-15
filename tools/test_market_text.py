#!/usr/bin/env python3
"""Market text content bounds and native component/UI rendering."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--content-only',action='store_true')
    p.add_argument('--output',type=Path)
    args=p.parse_args();env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    with tempfile.TemporaryDirectory(prefix='market-text-') as directory:
        binary=Path(directory)/'content'
        subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc',
                        'tools/market_text_content_host.cpp','src/game/Species.cpp',
                        'src/game/ItemInventory.cpp','src/game/ShopService.cpp',
                        'src/game/ExperienceService.cpp','-o',str(binary)],cwd=ROOT,check=True,env=env)
        subprocess.run([str(binary)],cwd=ROOT,check=True,env=env)
    if not args.content_only:
        command=['python3','tools/test_text_block.py']
        if args.output:command+=['--output',str(args.output.resolve())]
        subprocess.run(command,cwd=ROOT,check=True,env=env)
if __name__=='__main__':main()
