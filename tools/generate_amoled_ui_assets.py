#!/usr/bin/env python3
"""Build the AMOLED UI pack with native 48x48 item frames.

The checked-in ui.smonfx is the shared Stick-sized pack. This converter keeps
all non-item UI frames unchanged and replaces only item frames with the
original Pokemon Essentials 48x48 frames.
"""

import argparse
import os
import struct
import zlib
from pathlib import Path

from PIL import Image

from generate_game_assets import (
    ITEMS,
    KIND_IDS,
    LATE_ITEMS,
    Writer,
    write_pack,
)


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "data" / "packs" / "dev" / "game" / "ui.smonfx"
DEFAULT_OUTPUT = ROOT / "data" / "packs" / "dev" / "game" / "ui_amoled.smonfx"
DEFAULT_ITEMS_DIR = (
    Path(os.environ["ESSENTIALS_DIR"]) / "Graphics" / "Items"
    if os.environ.get("ESSENTIALS_DIR") else None
)
HEADER = struct.Struct("<IHHIIIIII")
FRAME = struct.Struct("<HHHBBHIII")
PACK_MAGIC = 0x58464753
PACK_VERSION = 2
FORMAT_INDEXED4_RLE = 1
ORIGINAL_ITEM_SIZE = 48


def rgb565_to_rgb(value):
    red = ((value >> 11) & 0x1F) * 255 // 31
    green = ((value >> 5) & 0x3F) * 255 // 63
    blue = (value & 0x1F) * 255 // 31
    return red, green, blue


def decode_frame(frame, data, palettes):
    _kind, width, height, image_format, _palette_size, _reserved, offset, length, palette_offset = frame
    if image_format != FORMAT_INDEXED4_RLE:
        raise ValueError(f"unsupported UI frame format: {image_format}")
    palette_size = frame[4]
    palette = palettes[palette_offset:palette_offset + palette_size]
    pixels = []
    cursor = offset
    end = offset + length
    expected = width * height
    while len(pixels) < expected:
        if cursor >= end:
            raise ValueError("truncated indexed frame")
        token = data[cursor]
        cursor += 1
        if token & 0x8000:
            pixels.extend([(0, 0, 0, 0)] * (token & 0x7FFF))
            continue
        count = token
        words = (count + 3) // 4
        if cursor + words > end:
            raise ValueError("truncated indexed pixel data")
        for word_index in range(words):
            packed = data[cursor]
            cursor += 1
            for nibble in range(4):
                if len(pixels) >= expected or word_index * 4 + nibble >= count:
                    break
                palette_index = (packed >> (nibble * 4)) & 0x0F
                if palette_index >= len(palette):
                    raise ValueError("invalid indexed palette reference")
                pixels.append((*rgb565_to_rgb(palette[palette_index]), 255))
    if len(pixels) != expected:
        raise ValueError("indexed frame has an invalid pixel count")
    image = Image.new("RGBA", (width, height))
    image.putdata(pixels)
    return image


def load_pack(path):
    content = path.read_bytes()
    if len(content) < HEADER.size:
        raise ValueError(f"UI pack is too small: {path}")
    header = HEADER.unpack(content[:HEADER.size])
    magic, version, frame_count, data_words, palette_words, flags, raw_bytes, compressed_bytes, _crc = header
    if magic != PACK_MAGIC or version != PACK_VERSION or flags != 1:
        raise ValueError(f"unsupported UI pack header: {path}")
    if HEADER.size + compressed_bytes != len(content):
        raise ValueError(f"UI pack size mismatch: {path}")
    payload = zlib.decompress(content[HEADER.size:], -15)
    if len(payload) != raw_bytes:
        raise ValueError("UI pack decompressed size mismatch")
    frame_bytes = frame_count * FRAME.size
    frames = [FRAME.unpack(payload[index:index + FRAME.size])
              for index in range(0, frame_bytes, FRAME.size)]
    data_start = frame_bytes
    data_end = data_start + data_words * 2
    palette_end = data_end + palette_words * 2
    if palette_end != len(payload):
        raise ValueError("UI pack payload layout mismatch")
    data = struct.unpack(f"<{data_words}H", payload[data_start:data_end])
    palettes = struct.unpack(f"<{palette_words}H", payload[data_end:palette_end])
    return frames, data, palettes


def build(input_path, output_path, items_dir=None):
    frames, data, palettes = load_pack(input_path)
    item_kind_ids = {
        KIND_IDS[name] for name, _filename in ITEMS + LATE_ITEMS
    }
    item_files = dict(ITEMS + LATE_ITEMS)
    writer = Writer()
    for frame in frames:
        kind_id = frame[0]
        if kind_id in item_kind_ids and items_dir:
            kind_name = next(name for name, value in KIND_IDS.items()
                             if value == kind_id)
            image = Image.open(items_dir / item_files[kind_name]).convert("RGBA")
            if image.size != (ORIGINAL_ITEM_SIZE, ORIGINAL_ITEM_SIZE):
                raise ValueError(
                    f"expected {ORIGINAL_ITEM_SIZE}x{ORIGINAL_ITEM_SIZE} item: "
                    f"{items_dir / item_files[kind_name]} ({image.size})"
                )
        else:
            if kind_id in item_kind_ids:
                raise ValueError(
                    "the AMOLED item source directory is required; pass "
                    "--items-dir or set ESSENTIALS_DIR"
                )
            image = decode_frame(frame, data, palettes)
        kind_name = next(name for name, value in KIND_IDS.items()
                         if value == kind_id)
        writer.add(kind_name, image)
    result = write_pack("ui", writer, output_path)
    print(
        f"amoled_ui frames={result['frames']} raw_bytes={result['raw_bytes']} "
        f"pack_bytes={result['pack_bytes']} output={output_path}"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--items-dir", type=Path, default=DEFAULT_ITEMS_DIR)
    args = parser.parse_args()
    build(args.input, args.output, args.items_dir)


if __name__ == "__main__":
    main()
