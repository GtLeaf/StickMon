#!/usr/bin/env python3
"""Check shared stall fling physics and all three production UI controllers."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/market-stall-scroll-20261114/native')
    args = parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    for host in ('market_list_scroll_host','market_stall_scroll_host'):
        sources=[] if host=='market_list_scroll_host' else [
            'src/game/ItemInventory.cpp','src/game/Species.cpp','src/game/ExperienceService.cpp',
            'src/game/ShopService.cpp','src/game/MonsterFactory.cpp','src/game/GameRandom.cpp','src/game/MoveManagementService.cpp']
        binary=output/host
        subprocess.run(['c++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc','tools/'+host+'.cpp',
            *sources,'-o',str(binary)],cwd=ROOT,check=True,env=env)
        subprocess.run([str(binary)],cwd=ROOT,check=True,env=env)

if __name__=='__main__':main()
