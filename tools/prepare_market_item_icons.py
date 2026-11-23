#!/usr/bin/env python3
"""Prepare Emerald market icons and append resources without replacing old pixels.

Sources are retained under origin_asset/icon/market/items/source with Git blob
provenance. Five vitamins share vitamin.png and use the original item palettes.
No external Essentials checkout or network connection is required.
"""
import argparse
import json
import struct
import zlib
from pathlib import Path

from PIL import Image
from generate_game_assets import (ROOT, KIND_ORDER, MARKET_ITEMS, OUTPUTS, Writer,
                                  indexed4_rle, write_pack, MAGIC, VERSION)
from generate_font16cn import collect_ui_chars
from extend_ui_font import with_ui_glyphs

ICON_ROOT = ROOT / 'origin_asset/icon/market/items'
VITAMINS = {'protein', 'iron', 'calcium', 'zinc', 'carbos'}


def prepare_icon(path, palette=None):
    with Image.open(path) as source:
        if source.size != (24, 24) or source.mode != 'P':
            raise ValueError(f'Expected indexed 24x24 Emerald source: {path}')
        if palette is not None:
            lines = Path(palette).read_text().splitlines()
            if lines[:3] != ['JASC-PAL', '0100', '16'] or len(lines) != 19:
                raise ValueError(f'Expected 16-color JASC palette: {palette}')
            colors = [int(value) for line in lines[3:] for value in line.split()]
            if len(colors) != 48 or any(value < 0 or value > 255 for value in colors):
                raise ValueError(f'Invalid palette: {palette}')
            source.putpalette(colors)
        image = source.convert('RGBA')
    if image.getchannel('A').getextrema() != (0, 255):
        raise ValueError(f'Expected transparent Emerald sprite: {path}')
    return image.resize((48, 48), Image.Resampling.NEAREST)


def read_writer(path):
    content = Path(path).read_bytes()
    if len(content) < 32:
        raise ValueError('Truncated UI pack')
    magic, version, count, words, palettes, flags, raw_size, compressed, crc = struct.unpack_from('<IHHIIIIII', content)
    if (magic, version, flags) != (MAGIC, VERSION, 1) or len(content) != 32 + compressed:
        raise ValueError('Unsupported UI pack')
    raw = zlib.decompress(content[32:], -15)
    if zlib.crc32(raw) & 0xffffffff != crc:
        raise ValueError('Existing UI pack CRC mismatch')
    if len(raw) != raw_size or raw_size != count * 22 + (words + palettes) * 2:
        raise ValueError('Invalid UI pack payload size')
    writer = Writer()
    writer.data = list(struct.unpack_from(f'<{words}H', raw, count * 22))
    writer.palettes = list(struct.unpack_from(f'<{palettes}H', raw, count * 22 + words * 2))
    for i in range(count):
        kind, width, height, fmt, colors, reserved, offset, length, palette_offset = struct.unpack_from('<HHHBBHIII', raw, i * 22)
        if (reserved or kind >= len(KIND_ORDER) or offset + length > words or
                palette_offset + colors > palettes):
            raise ValueError('Unsupported UI frame')
        writer.frames.append(dict(kind=KIND_ORDER[kind], width=width, height=height,
            format=fmt, palette_size=colors, offset=offset, length=length,
            palette_offset=palette_offset))
    return writer


def install_icons(images, path=None):
    path = path or OUTPUTS['ui']
    writer = read_writer(path)
    changed = False
    for kind, image in images.items():
        encoded, palette = indexed4_rle(image)
        old = next((frame for frame in writer.frames if frame['kind'] == kind), None)
        old_index = writer.frames.index(old) if old is not None else None
        if old is not None:
            if ((old['width'], old['height']) == image.size and
                    writer.data[old['offset']:old['offset'] + old['length']] == encoded and
                    writer.palettes[old['palette_offset']:old['palette_offset'] + old['palette_size']] == palette):
                continue
            writer.frames.remove(old)
        writer.add(kind, image)
        if old_index is not None:
            writer.frames.insert(old_index, writer.frames.pop())
        changed = True
    if changed:
        write_pack('ui', writer, path)


def extend_font(path, chars):
    source = path.read_bytes()
    updated = with_ui_glyphs(source, chars)
    if updated != source:
        path.write_bytes(updated)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', action='store_true')
    args = parser.parse_args()
    images = {}
    for kind, filename in MARKET_ITEMS:
        name = Path(filename).stem
        shared = name in VITAMINS
        image = prepare_icon(ICON_ROOT / 'source' / ('vitamin.png' if shared else filename),
                             ICON_ROOT / 'source' / f'{name}.pal' if shared else None)
        image.save(ICON_ROOT / filename)
        images[kind] = image
    if args.install:
        install_icons(images)
        native_path = OUTPUTS['ui'].with_name('ui_amoled.smonfx')
        with Image.open(ROOT / 'origin_asset/icon/market/mystery-box.png') as source:
            native_images = {'MARKET_MYSTERY_BOX': source.convert('RGBA'), **images}
        install_icons(native_images, native_path)
        chars = collect_ui_chars()
        for name in ('zh16', 'zh32', 'round-zh16-a4'):
            extend_font(ROOT / f'data/packs/dev/fonts/{name}.smonfont', chars)
        manifest_path = ROOT / 'data/packs/dev/manifest.json'
        manifest = json.loads(manifest_path.read_text())
        manifest['gameAssetCount'] = sum(struct.unpack_from('<H', p.read_bytes(), 6)[0]
                                         for p in OUTPUTS.values())
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    print(f'Prepared {len(images)} Emerald market icons')


if __name__ == '__main__':
    main()
