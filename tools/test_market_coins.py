#!/usr/bin/env python3
"""Unified market transaction and rolling coin HUD native regressions."""
from pathlib import Path
import argparse
import os
import subprocess
from amoled_host import build_amoled_host

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/market-coins-20261114/native')
    p.add_argument('--native-only',action='store_true')
    args=p.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    binary=out/'coins';env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-Isrc','tools/market_coins_host.cpp','-o',str(binary)],cwd=ROOT,check=True,env=env)
    subprocess.run([str(binary)],cwd=ROOT,check=True,env=env)
    if args.native_only:return
    stub=out/'uzlib_stub.c'
    stub.write_text('unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n'
                    'unsigned uzlib_crc32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n')
    binary=build_amoled_host(out/'journey',[ROOT/'tools/market_coins_amoled_host.cpp',stub],debug=True)
    subprocess.run([str(binary),str(ROOT/'data'),str(out)],cwd=ROOT,check=True,env=env)

if __name__=='__main__':main()
