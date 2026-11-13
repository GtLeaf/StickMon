#!/usr/bin/env python3
"""Prepare the box sprite and install it without re-encoding existing assets."""
import argparse
import json
import struct
import zlib
from pathlib import Path
from PIL import Image, ImageFont
from generate_game_assets import (ROOT, KIND_IDS, KIND_ORDER, OUTPUTS, Writer,
    fit_sprite_canvas, indexed4_rle, quantize_rgba, write_pack)
from generate_font16cn import (SARASA_FONT_PATH, UNSCII_CHARS, collect_ui_chars, glyph_bytes,
    glyph_bytes_large, write_pack_font)

SOURCE = ROOT / 'origin_asset/icon/market/mystery-box-generated.png'
OUTPUT = SOURCE.with_name('mystery-box.png')


def install_icon(image):
    path = OUTPUTS['ui']; content = path.read_bytes()
    _, _, count, words, palettes, _, _, _, crc = struct.unpack_from('<IHHIIIIII', content)
    raw = zlib.decompress(content[32:], -15)
    if zlib.crc32(raw) & 0xffffffff != crc:
        raise ValueError('Existing UI pack CRC mismatch')
    writer = Writer()
    writer.data = list(struct.unpack_from(f'<{words}H', raw, count * 22))
    writer.palettes = list(struct.unpack_from(f'<{palettes}H', raw, count * 22 + words * 2))
    encoded, palette = indexed4_rle(image)
    for i in range(count):
        kind, width, height, fmt, colors, reserved, offset, length, palette_offset = struct.unpack_from('<HHHBBHIII', raw, i * 22)
        if reserved or kind >= len(KIND_ORDER):
            raise ValueError('Unsupported UI frame')
        if kind == KIND_IDS['MARKET_MYSTERY_BOX']:
            if (width, height) == (48, 48) and writer.data[offset:offset + length] == encoded and writer.palettes[palette_offset:palette_offset + colors] == palette:
                return
            continue
        writer.frames.append(dict(kind=KIND_ORDER[kind], width=width, height=height, format=fmt,
            palette_size=colors, offset=offset, length=length, palette_offset=palette_offset))
    writer.add('MARKET_MYSTERY_BOX', image)
    write_pack('ui', writer)
    manifest_path = ROOT / 'data/packs/dev/manifest.json'
    manifest = json.loads(manifest_path.read_text())
    manifest['gameAssetCount'] = sum(struct.unpack_from('<H', p.read_bytes(), 6)[0] for p in OUTPUTS.values())
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')


def install_glyphs():
    for name, size, pixel_size in [('zh16', 32, 16), ('zh32', 128, 32)]:
        path = ROOT / f'data/packs/dev/fonts/{name}.smonfont'
        data = path.read_bytes(); count = struct.unpack_from('<H', data, 6)[0]
        existing = {}
        for i in range(count):
            offset = 16 + i * (4 + size)
            existing[chr(struct.unpack_from('<I', data, offset)[0])] = data[offset + 4:offset + 4 + size]
        missing = collect_ui_chars() - existing.keys() - UNSCII_CHARS
        if not missing:
            continue
        font = ImageFont.truetype(str(SARASA_FONT_PATH), pixel_size)
        rasterize = glyph_bytes if pixel_size == 16 else glyph_bytes_large
        for ch in missing:
            existing[ch] = bytes(rasterize(font, ch))
        write_pack_font(path, existing.keys(), existing.__getitem__, pixel_size, pixel_size, size)
        print(name, 'added glyphs:', ''.join(sorted(missing)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--install', action='store_true')
    args = parser.parse_args()
    with Image.open(SOURCE) as source:
        image = source.convert('RGBA')
        # Ignore near-transparent generation specks when fitting the sprite.
        image.putalpha(image.getchannel('A').point(lambda value: 255 if value >= 128 else 0))
        bounds = image.getchannel('A').getbbox()
        if not bounds or image.getchannel('A').getextrema()[0] != 0:
            raise ValueError('Mystery box source must have transparency')
        image = quantize_rgba(fit_sprite_canvas(image.crop(bounds), (48, 48), padding=4), 15)
        image.save(OUTPUT)
    if args.install:
        install_icon(image)
        install_glyphs()
    print(OUTPUT)


if __name__ == '__main__':
    main()
