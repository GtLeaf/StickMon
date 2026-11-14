#!/usr/bin/env python3
"""Star box controller timing, touch and sound regressions against production UI."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/kecleon-box-live-20261114/native')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    binary=out/'controller'
    env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    sources=['src/game/Species.cpp','src/game/ItemInventory.cpp','src/game/ShopService.cpp','src/game/ExperienceService.cpp']
    subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-Isrc','tools/market_box_opening_host.cpp',*sources,'-o',str(binary)],cwd=ROOT,check=True,env=env)
    subprocess.run([str(binary)],cwd=ROOT,check=True,env=env)

if __name__=='__main__':main()
