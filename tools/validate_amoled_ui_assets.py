#!/usr/bin/env python3
"""Reject outdated AMOLED UI packs before packaging firmware (stdlib only)."""

import argparse
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = struct.Struct("<IHHIIIIII")
FRAME = struct.Struct("<HHHBBHIII")


def read_frames(path):
    content = Path(path).read_bytes()
    if len(content) < HEADER.size:
        raise ValueError(f"UI pack is truncated: {path}")
    magic, version, count, words, palettes, flags, raw_size, size, crc = HEADER.unpack_from(content)
    if magic != 0x58464753 or version != 2 or flags != 1:
        raise ValueError(f"unsupported UI pack format: {path}")
    if len(content) != HEADER.size + size:
        raise ValueError(f"UI pack size mismatch: {path}")
    payload = zlib.decompress(content[HEADER.size:], -15)
    if len(payload) != raw_size or zlib.crc32(payload) != crc:
        raise ValueError(f"UI pack checksum/size mismatch: {path}")
    if count * FRAME.size + (words + palettes) * 2 != len(payload):
        raise ValueError(f"UI pack layout mismatch: {path}")
    return [FRAME.unpack_from(payload, index * FRAME.size) for index in range(count)]


def validate(input_path, native_path, header_path=None):
    header_path = header_path or ROOT / "src/assets/GameAssets.h"
    source = Path(header_path).read_text()
    match = re.search(r"enum class Kind\s*:\s*uint16_t\s*\{(.*?)\};", source, re.S)
    if not match:
        raise ValueError("cannot read GameAssets::Kind")
    names = re.sub(r"//[^\n]*", "", match.group(1))
    names = [name.strip() for name in names.split(",") if name.strip()]
    kinds = {name: index for index, name in enumerate(names)}
    expected = set(range(kinds["SHOWER_BACKGROUND"] + 1)) | {
        kinds["EXPLORE_PICKUP_BALL"], kinds["ITEM_HEART_SCALE"]}
    for path in (input_path, native_path):
        frames = read_frames(path)
        ids = [frame[0] for frame in frames]
        if len(ids) != len(expected) or set(ids) != expected:
            raise ValueError(
                f"outdated UI asset ids: {path}; missing={sorted(expected - set(ids))}, "
                f"unexpected={sorted(set(ids) - expected)}. Regenerate AMOLED UI assets.")
        if Path(path) == Path(native_path):
            for kind, width, height, *_rest in frames:
                if names[kind].startswith("ITEM_") and (width, height) != (48, 48):
                    raise ValueError(f"AMOLED item {names[kind]} must be 48x48: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("native", type=Path)
    args = parser.parse_args()
    try:
        validate(args.input, args.native)
    except (OSError, ValueError, zlib.error, struct.error) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
