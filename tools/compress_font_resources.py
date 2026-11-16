#!/usr/bin/env python3
"""Idempotently compress existing fonts without rasterizing or changing glyphs."""
import argparse
from pathlib import Path
from font_pack import compress_font, unpack_font

def compress_directory(directory):
    results = []
    for path in sorted(Path(directory).glob('*.smonfont')):
        before = path.read_bytes()
        after = compress_font(before)
        if unpack_font(after) != unpack_font(before):
            raise ValueError(f'Font changed: {path}')
        if before != after:
            path.write_bytes(after)
        results.append((path.name,len(before),len(after)))
    return results

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path,nargs='?',default=Path(__file__).resolve().parents[1]/'data/packs/dev/fonts')
    for name,before,after in compress_directory(parser.parse_args().directory):
        print(f'{name}: {before} -> {after} bytes')
