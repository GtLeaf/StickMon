#!/usr/bin/env python3
"""Periodic market rules, special item use, choices and production App journey."""
import argparse
from pathlib import Path
import os
import subprocess
from amoled_host import ROOT, build_amoled_host

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/market-season-20261113/tests')
    parser.add_argument('--rules-only',action='store_true')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
    sources=['src/game/ItemInventory.cpp','src/game/Species.cpp','src/game/ExperienceService.cpp',
             'src/game/ShopService.cpp','src/game/MonsterFactory.cpp','src/game/GameRandom.cpp']
    for host in ['market_item_rules_host','market_season_host','market_selector_host']:
        binary=out/host
        subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc',
            'tools/'+host+'.cpp',*sources,'-o',str(binary)],cwd=ROOT,check=True,env=env)
        subprocess.run([str(binary)],check=True,env=env)
    if args.rules_only:return
    stub=out/'uzlib_stub.c';stub.write_text('unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\nunsigned uzlib_crc32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n')
    binary=build_amoled_host(out/'journey',[ROOT/'tools/market_season_amoled_host.cpp',stub],debug=True)
    subprocess.run([str(binary),str(ROOT/'data'),str(out)],check=True,env=env)

if __name__=='__main__':main()
