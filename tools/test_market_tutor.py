#!/usr/bin/env python3
"""Gengar stall (秘技小屋): shared rules/save host and real AmoledApp journey."""
from pathlib import Path
import argparse
import subprocess
import sys
import tempfile

from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]
RULE_SOURCES = ['src/game/Species.cpp', 'src/game/ItemInventory.cpp', 'src/game/MoveManagementService.cpp',
                'src/game/MonsterFactory.cpp', 'src/core/SaveCodec.cpp', 'src/game/GameRandom.cpp',
                'src/game/ExperienceService.cpp']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='write journey frames (.ppm) here')
    args = parser.parse_args()
    subprocess.run(['python3', str(ROOT / 'tools/import_tutor_moves.py'), '--check'], check=False)
    with tempfile.TemporaryDirectory(prefix='market-tutor-') as tmp:
        rules = Path(tmp) / 'rules'
        subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', '-I' + str(ROOT / 'src'),
                        str(ROOT / 'tools/market_tutor_host.cpp'), *[str(ROOT / s) for s in RULE_SOURCES],
                        '-o', str(rules)], check=True)
        subprocess.run([str(rules)], check=True)
        extra = []
        if sys.platform != 'darwin':
            # GNU ld keeps uzlib's checksum references alive; they are unused here.
            stub = Path(tmp) / 'uzlib_stub.c'
            stub.write_text('unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n'
                            'unsigned uzlib_crc32(const void*d,unsigned l,unsigned c){(void)d;(void)l;return c;}\n')
            extra.append(stub)
        journey = build_amoled_host(Path(tmp) / 'journey', [ROOT / 'tools/market_tutor_amoled_host.cpp', *extra], debug=True)
        cmd = [str(journey), str(ROOT / 'data')]
        if args.output:
            args.output.mkdir(parents=True, exist_ok=True)
            cmd.append(str(args.output.resolve()))
        subprocess.run(cmd, check=True)


if __name__ == '__main__':
    main()
