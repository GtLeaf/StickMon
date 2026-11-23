#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,os
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'.stick-local/artifacts/computer-expansion-20261123/save');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    binary=out/'save-host';sources=['tools/computer_expansion_save_host.cpp','src/core/SaveManager.cpp','src/core/SaveCodec.cpp','src/platform/api/PlatformServices.cpp','src/platform/desktop/DesktopPlatform.cpp','src/game/Species.cpp','src/game/MonsterFactory.cpp','src/game/GameRandom.cpp','src/game/ItemInventory.cpp','src/game/ExperienceService.cpp','src/game/ShopService.cpp']
    subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Isrc',*sources,'-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary),str(out)],cwd=ROOT,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
if __name__=='__main__':main()
