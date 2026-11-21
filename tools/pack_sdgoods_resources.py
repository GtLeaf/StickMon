#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Pack embedded visuals or supplemental audio for the SDGOODS resources partition."""
import argparse
from pathlib import Path
import struct
from wallpaper_catalog import CATALOG, exported_wallpapers

ROOT = Path(__file__).resolve().parents[1]
ENTRY = struct.Struct('<192sII')
EXTENSIONS = {'.json', '.smonsp', '.smonroom', '.smonwall', '.smonfurn', '.smonfx', '.smonfont', '.smondialogue', '.smonmenu'}


def pack(data: Path, output: Path, *, audio_only=False, catalog=None):
    # Deploy the source catalog with the project's packs. Standalone archive
    # callers can supply their own catalog, or package an arbitrary data tree.
    if catalog is None and data.resolve() == ROOT / 'data':
        catalog = CATALOG
    wallpapers = set(exported_wallpapers(data, catalog)) if catalog and not audio_only else None
    files = sorted(p for p in data.rglob('*') if p.is_file()
                   and (p.suffix in {'.smonaudio', '.smoncry'} if audio_only else
                        p.suffix in EXTENSIONS or p.name == 'bgm_market.smonaudio')
                   and (p.suffix != '.smonwall' or wallpapers is None or p in wallpapers)
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
    if audio_only and offset > 9 * 1024 * 1024:
        raise ValueError(f'Audio archive exceeds the current 9 MiB resources partition: {offset}')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(b'SMRA' + struct.pack('<I', len(files))
                       + b''.join(index) + b''.join(payload))
    print(f'SDGOODS resources: {len(files)} files, {offset} bytes -> {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT / 'data')
    parser.add_argument('--output', type=Path,
                        default=ROOT / '.stick-local/firmware/sdgoods/stickmon_resources.bin')
    parser.add_argument('--catalog', type=Path, help='Wallpaper catalog for a custom data tree')
    parser.add_argument('--audio-only', action='store_true',
                        help='Build supplemental audio archive for the resources data partition')
    args = parser.parse_args()
    pack(args.data, args.output, audio_only=args.audio_only, catalog=args.catalog)
