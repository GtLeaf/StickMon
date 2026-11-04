#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Embed runtime resources and the market BGM within the SDGOODS app budget."""
import argparse
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
ENTRY = struct.Struct('<192sII')
EXTENSIONS = {'.json', '.smonsp', '.smonroom', '.smonfurn', '.smonfx', '.smonfont', '.smondialogue'}


def pack(data: Path, output: Path):
    files = sorted(p for p in data.rglob('*') if p.is_file()
                   and (p.suffix in EXTENSIONS or p.name == 'bgm_market.smonaudio')
                   and p.name != 'ui_amoled.smonfx')
    offset = 8 + ENTRY.size * len(files)
    index, payload = [], []
    for source in files:
        name = ('/' + source.relative_to(data).as_posix()).encode('ascii')
        if len(name) >= 192:
            raise ValueError(f'Resource path too long: {source}')
        content = source.read_bytes()
        index.append(ENTRY.pack(name, offset, len(content)))
        payload.append(content)
        offset += len(content)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(b'SMRA' + struct.pack('<I', len(files))
                       + b''.join(index) + b''.join(payload))
    print(f'SDGOODS resources: {len(files)} files, {offset} bytes -> {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--output', type=Path,
                        default=ROOT / '.stick-local/firmware/sdgoods/stickmon_resources.bin')
    args = parser.parse_args()
    pack(args.data, args.output)
