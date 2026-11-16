#!/usr/bin/env python3
"""Pack the embedded main-menu RGB565 RLE stream without changing a pixel.

V1 SMMN: little-endian magic, version/count/width/height (uint16), decoded
bytes/encoded bytes/CRC32/reserved (uint32), then raw DEFLATE. The decoded
payload is count {uint16 wordOffset, wordLength} records followed by RLE words.
"""
import argparse
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CPP = ROOT / 'src/assets/MenuAssets.cpp'
DEFAULT_OUTPUT = ROOT / 'data/packs/dev/ui/menu.smonmenu'
HEADER = struct.Struct('<4sHHHHIIII')


def generate_pack(cpp_path: Path, output_path: Path) -> int:
    source = cpp_path.read_text(encoding='utf-8')
    frame_source = source.split('const RleFrame MAIN_ICON_FRAMES[] STICKMON_FLASH_DATA = {', 1)[1].split('};', 1)[0]
    word_source = source.split('const uint16_t MAIN_ICON_RLE[] STICKMON_FLASH_DATA = {', 1)[1].split('};', 1)[0]
    frames = [(int(a), int(b)) for a, b in re.findall(r'\{\s*(\d+),\s*(\d+)\s*\}', frame_source)]
    words = [int(word, 16) for word in re.findall(r'0x[0-9a-fA-F]+', word_source)]
    if len(frames) != 9 or len(words) > 9*40*40*2:
        raise ValueError('invalid main-menu icon count or payload size')
    offset = 0
    for start, length in frames:
        if start != offset or not length or start+length > len(words):
            raise ValueError('invalid main-menu frame bounds')
        end = start+length
        pixels = 0
        while start < end:
            control = words[start]
            start += 1
            run = control & 0x7fff
            pixels += run
            if not run or pixels > 1600:
                raise ValueError('invalid main-menu RLE run')
            if not control & 0x8000:
                start += run
            if start > end:
                raise ValueError('truncated main-menu literal run')
        if pixels != 1600:
            raise ValueError('incomplete main-menu frame')
        offset = end
    if offset != len(words):
        raise ValueError('unreferenced main-menu RLE words')
    payload = b''.join(struct.pack('<HH', *frame) for frame in frames)
    payload += struct.pack('<' + 'H'*len(words), *words)
    compressor = zlib.compressobj(9, wbits=-15)
    encoded = compressor.compress(payload) + compressor.flush()
    blob = HEADER.pack(b'SMMN', 1, 9, 40, 40, len(payload), len(encoded), zlib.crc32(payload), 0) + encoded
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(blob)
    return len(blob)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpp', type=Path, default=DEFAULT_CPP)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    print(f'generated {args.output}: {generate_pack(args.cpp, args.output)} bytes')


if __name__ == '__main__':
    main()
