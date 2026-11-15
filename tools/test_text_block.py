#!/usr/bin/env python3
"""Bounded wrapping layout and real-resource bitmap rendering."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
from amoled_host import build_amoled_host

ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--layout-only',action='store_true')
    p.add_argument('--output',type=Path)
    args=p.parse_args();env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    with tempfile.TemporaryDirectory(prefix='text-block-') as tmp:
        binary=Path(tmp)/'layout'
        subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc',
            'tools/text_block_host.cpp','-o',str(binary)],cwd=ROOT,env=env,check=True)
        subprocess.run([str(binary)],cwd=ROOT,env=env,check=True)
        if args.layout_only:return
        binary=build_amoled_host(Path(tmp)/'render',[ROOT/'tools/text_block_render_host.cpp'])
        cmd=[str(binary),str(ROOT/'data')]
        if args.output:args.output.mkdir(parents=True,exist_ok=True);cmd.append(str(args.output.resolve()))
        subprocess.run(cmd,cwd=ROOT,env=env,check=True)
if __name__=='__main__':main()
