#!/usr/bin/env python3
"""Check stall motion and shifted map-cache pixels against the production app."""
import argparse
from pathlib import Path
import subprocess
import sys

from amoled_host import ROOT, build_amoled_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / '.stick-local/artifacts/stall-motion-fix-20261113/tests')
    parser.add_argument('--part', choices=('motion', 'cache', 'all'), default='all')
    parser.add_argument('--allow-fallback', action='store_true', help='baseline pixel comparison only')
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    if args.part in ('motion', 'all'):
        binary = output / 'motion'
        subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I' + str(ROOT / 'src'), str(ROOT / 'tools/market_stall_motion_host.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    if args.part in ('cache', 'all'):
        extra = []
        if sys.platform != 'darwin':
            stub = output / 'uzlib_stub.c'
            stub.write_text('unsigned uzlib_adler32(const void*d,unsigned l,unsigned p){(void)d;(void)l;return p;}\n'
                            'unsigned uzlib_crc32(const void*d,unsigned l,unsigned c){(void)d;(void)l;return c;}\n')
            extra.append(stub)
        binary = build_amoled_host(output / 'cache', [ROOT / 'tools/market_stall_cache_host.cpp', *extra], debug=True)
        command = [str(binary), str(ROOT / 'data')]
        if args.allow_fallback:
            command.append('--allow-fallback')
        subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
