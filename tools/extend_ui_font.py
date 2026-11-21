"""Append needed UI glyphs to an installed font pack, retaining its existing artwork."""
import struct

from PIL import ImageFont

from font_pack import HEADER, compress_font, unpack_font
from generate_font16cn import SARASA_FONT_PATH, UNSCII_CHARS, glyph_bytes, glyph_bytes_large
from generate_round_ui_fonts import glyph_alpha


def with_ui_glyphs(source, chars):
    data = unpack_font(source)
    _, version, count, width, _, stride, _, _ = HEADER.unpack_from(data)
    existing = {}
    for offset in range(HEADER.size, len(data), 4 + stride):
        char = chr(struct.unpack_from('<I', data, offset)[0])
        existing[char] = data[offset + 4:offset + 4 + stride]
    missing = set(chars) - existing.keys() - UNSCII_CHARS
    if not missing:
        return source
    font = ImageFont.truetype(str(SARASA_FONT_PATH), width)
    for char in missing:
        existing[char] = (glyph_alpha(font, char, width) if version == 2 else
                          bytes((glyph_bytes if width == 16 else glyph_bytes_large)(font, char)))
    header = bytearray(data[:HEADER.size])
    struct.pack_into('<H', header, 6, count + len(missing))
    payload = b''.join(struct.pack('<I', ord(char)) + existing[char]
                       for char in sorted(existing, key=ord))
    return compress_font(header + payload)
