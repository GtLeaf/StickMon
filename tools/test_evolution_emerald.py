#!/usr/bin/env python3
"""Native Emerald evolution rendering and opt-in streaming sprite tint."""
import argparse
from pathlib import Path
import subprocess
import tempfile
from amoled_host import build_amoled_host

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--sprites-only',action='store_true')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='evolution-emerald-') as tmp:
        scale_binary=Path(tmp)/'scale-host'
        subprocess.run(['c++','-std=c++17','-O1','-Isrc',str(ROOT/'tools/tinted_sprite_scale_host.cpp'),
                        str(ROOT/'src/presentation/Canvas565.cpp'),'-o',str(scale_binary)],cwd=ROOT,check=True)
        subprocess.run([str(scale_binary)],cwd=ROOT,check=True)
        binary=build_amoled_host(Path(tmp)/'host',[ROOT/'tools/evolution_emerald_host.cpp',ROOT/'tools/amoled_checksum_guard.cpp'])
        command=[str(binary),str(ROOT/'data')]
        if args.output:args.output.mkdir(parents=True,exist_ok=True);command.append(str(args.output.resolve()))
        if args.sprites_only:command.append('--sprites-only')
        subprocess.run(command,cwd=ROOT,check=True)

if __name__=='__main__':main()
