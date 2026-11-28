#!/usr/bin/env python3
"""Expansion capacity and market transaction behavior (native ASan/UBSan)."""
from pathlib import Path
import argparse,subprocess,os
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/computer-expansion-20261123/rules');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    sources=['tools/computer_expansion_host.cpp','src/core/SaveCodec.cpp','src/game/FriendshipService.cpp','src/game/FriendshipSystem.cpp','src/game/ItemInventory.cpp','src/game/Species.cpp','src/game/ExperienceService.cpp','src/game/ShopService.cpp','src/game/MonsterFactory.cpp','src/game/GameRandom.cpp']
    binary=out/'computer-expansion';subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc',*sources,'-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary)],cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    ui=out/'ui-host'
    subprocess.run(['c++','-std=c++17','-O1','-Isrc','tools/computer_expansion_ui_host.cpp',*sources[2:],'-o',str(ui)],cwd=ROOT,check=True)
    subprocess.run([str(ui)],cwd=ROOT,check=True)

if __name__=='__main__':main()
