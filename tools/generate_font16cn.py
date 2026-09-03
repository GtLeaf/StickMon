#!/usr/bin/env python3
import json
import re
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
DATA_DIR = ROOT / "data"
PACK_OUT = DATA_DIR / "packs" / "dev"
PACK_FONT_REGULAR_OUT = PACK_OUT / "fonts" / "zh16.smonfont"
PACK_FONT_UNSCII_ASCII_OUT = PACK_OUT / "fonts" / "ascii16-unscii.smonfont"
FALLBACK_H_OUT = SRC / "assets" / "FontFallbackCN.h"
FALLBACK_CPP_OUT = SRC / "assets" / "FontFallbackCN.cpp"
SARASA_FONT_PATH = (
    ROOT / "third_party" / "fonts" / "sarasa-gothic-sc" /
    "SarasaGothicSC-Regular.ttf"
)
UNSCII_FONT_PATH = (
    ROOT / "third_party" / "fonts" / "unscii" / "unscii-16.hex"
)
FONT_PACK_MAGIC = 0x4E464D53  # SMFN
FONT_PACK_VERSION = 1
FONT_GLYPH_W = 16
FONT_GLYPH_H = 16
FONT_GLYPH_BYTES = 32
ASCII_CELL_W = 8
# Keep one logical pixel of side bearing on both sides of the normal ASCII
# cell. Wide glyphs are narrowed offline before they enter the binary font.
ASCII_DRAW_W = 6
# Pillow's baseline anchor keeps glyphs with different bbox heights aligned.
# Row 15 is the last row of the 16-pixel logical text cell.
FONT_BASELINE_ROW = 15
BITMAP_THRESHOLD = 64
PRINTABLE_ASCII = frozenset(chr(codepoint) for codepoint in range(0x21, 0x7F))
UNSCII_CHARS = PRINTABLE_ASCII.union({"♀", "♂"})
TEXT_EXTENSIONS = {".json", ".txt", ".md"}


def collect_chars_from_text(text):
    return {
        ch for ch in text
        if ord(ch) >= 0x80 and ch.isprintable() and not ch.isspace()
    }


def collect_ui_chars():
    text = (SRC / "core" / "UiStrings.h").read_text(encoding="utf-8")
    chars = collect_chars_from_text(text)
    learnset_snapshot = ROOT / "tools" / "pokemon_data" / "oras_learnsets.json"
    if learnset_snapshot.exists():
        chars.update(collect_chars_from_text(
            learnset_snapshot.read_text(encoding="utf-8")))
    return chars


def collect_fallback_chars():
    text = (SRC / "core" / "UiStrings.h").read_text(encoding="utf-8")
    match = re.search(r"namespace ResourceAlert\s*\{(.*?)\n\}", text, re.S)
    if not match:
        raise RuntimeError("Ui::ResourceAlert namespace not found")
    return collect_chars_from_text(match.group(1))


def collect_pack_text_chars():
    chars = set()
    if not PACK_OUT.exists():
        return chars
    for path in PACK_OUT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_EXTENSIONS:
            continue
        try:
            chars.update(collect_chars_from_text(path.read_text(encoding="utf-8")))
        except UnicodeDecodeError:
            continue
    return chars


def sorted_chars(chars):
    return sorted(chars, key=ord)


def load_unscii_glyphs(path):
    glyphs = {}
    for line_number, raw_line in enumerate(
            path.read_text(encoding="ascii").splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        try:
            codepoint_text, bitmap_text = line.split(":", 1)
            codepoint = int(codepoint_text, 16)
        except ValueError as exc:
            raise ValueError(
                f"Invalid Unscii record at {path}:{line_number}") from exc
        if chr(codepoint) not in UNSCII_CHARS:
            continue
        if len(bitmap_text) != FONT_GLYPH_H * 2:
            raise ValueError(
                f"Unscii glyph U+{codepoint:04X} is not 8x16 at "
                f"{path}:{line_number}"
            )
        try:
            rows = bytes.fromhex(bitmap_text)
        except ValueError as exc:
            raise ValueError(
                f"Invalid Unscii bitmap at {path}:{line_number}") from exc

        bitmap = bytearray()
        for row in rows:
            bitmap.extend((row, 0))
        glyphs[chr(codepoint)] = bytes(bitmap)

    missing = UNSCII_CHARS.difference(glyphs)
    if missing:
        labels = ", ".join(f"U+{ord(ch):04X}" for ch in sorted_chars(missing))
        raise ValueError(f"Unscii font is missing printable ASCII glyphs: {labels}")
    return glyphs


def normalize_unscii_glyph(bitmap):
    """Center Unscii ink in an 8-pixel cell with a stable max width."""
    img = Image.new("L", (FONT_GLYPH_W, FONT_GLYPH_H), 0)
    for row, value in enumerate(bitmap[::2]):
        for col in range(8):
            if value & (1 << (7 - col)):
                img.putpixel((col, row), 255)

    bbox = img.getbbox()
    if not bbox:
        return bytes(FONT_GLYPH_BYTES)

    left, _, right, _ = bbox
    ink = img.crop((left, 0, right, FONT_GLYPH_H))
    if ink.width > ASCII_DRAW_W:
        ink = ink.resize((ASCII_DRAW_W, FONT_GLYPH_H), Image.Resampling.NEAREST)

    normalized = Image.new("L", (FONT_GLYPH_W, FONT_GLYPH_H), 0)
    x = (ASCII_CELL_W - ink.width) // 2
    normalized.paste(ink, (x, 0))

    out = bytearray()
    for row in range(FONT_GLYPH_H):
        left_bits = 0
        right_bits = 0
        for col in range(8):
            if normalized.getpixel((col, row)) > 0:
                left_bits |= 1 << (7 - col)
        for col in range(8, FONT_GLYPH_W):
            if normalized.getpixel((col, row)) > 0:
                right_bits |= 1 << (15 - col)
        out.extend((left_bits, right_bits))
    return bytes(out)


def glyph_bytes(font, ch):
    # Render against a shared baseline, then threshold once into a binary
    # bitmap. This is offline rasterization; firmware never blends glyphs.
    draw = ImageDraw.Draw(Image.new("L", (FONT_GLYPH_W, FONT_GLYPH_H), 0))
    bbox = draw.textbbox((0, 0), ch, font=font, anchor="ls")
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    if w <= 0 or h <= 0:
        return [0] * FONT_GLYPH_BYTES

    glyph = Image.new("L", (w, h), 0)
    ImageDraw.Draw(glyph).text((-bbox[0], -bbox[1]), ch, font=font,
                               fill=255, anchor="ls")
    baseline_offset = -bbox[1]

    cell_w = ASCII_CELL_W if ord(ch) < 0x80 else FONT_GLYPH_W
    max_draw_w = ASCII_DRAW_W if ord(ch) < 0x80 else FONT_GLYPH_W
    scale = min(1.0, max_draw_w / w, FONT_GLYPH_H / h)
    if scale < 1.0:
        scaled_w = max(1, round(w * scale))
        scaled_h = max(1, round(h * scale))
        glyph = glyph.resize((scaled_w, scaled_h), Image.Resampling.LANCZOS)
        baseline_offset = round(baseline_offset * scale)
        w, h = glyph.size

    img = Image.new("L", (FONT_GLYPH_W, FONT_GLYPH_H), 0)
    x = (cell_w - w) // 2
    y = FONT_BASELINE_ROW - baseline_offset
    img.paste(glyph, (x, y))

    out = []
    for row in range(16):
        left = 0
        right = 0
        for col in range(8):
            if img.getpixel((col, row)) > BITMAP_THRESHOLD:
                left |= 1 << (7 - col)
        for col in range(8, 16):
            if img.getpixel((col, row)) > BITMAP_THRESHOLD:
                right |= 1 << (15 - col)
        out.extend([left, right])
    return out


def write_json_file(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def merge_pack_manifest(**fields):
    path = PACK_OUT / "manifest.json"
    payload = {}
    if path.exists():
        try:
            loaded = json.loads(path.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                payload = loaded
        except (json.JSONDecodeError, OSError):
            pass
    payload.update({
        "format": "smon-resource-pack-v1",
        "id": "dev",
        "schema": 1,
        "version": "0.0.0-dev",
    })
    payload.update(fields)
    write_json_file(path, payload)


def write_active_config():
    write_json_file(DATA_DIR / "active.json", {
        "activePack": "dev",
        "packPath": "/packs/dev",
    })


def write_pack_font(path, chars, glyph_for_char):
    path.parent.mkdir(parents=True, exist_ok=True)
    glyph_rows = bytearray()
    for ch in sorted_chars(chars):
        glyph_rows += struct.pack("<I", ord(ch))
        glyph_rows += bytes(glyph_for_char(ch))

    header = struct.pack(
        "<IHHBBBBI",
        FONT_PACK_MAGIC,
        FONT_PACK_VERSION,
        len(chars),
        FONT_GLYPH_W,
        FONT_GLYPH_H,
        FONT_GLYPH_BYTES,
        0,
        0,
    )
    path.write_bytes(header + glyph_rows)


def write_firmware_fallback(chars, font):
    FALLBACK_H_OUT.parent.mkdir(parents=True, exist_ok=True)
    FALLBACK_H_OUT.write_text(
        """#pragma once

#include <cstdint>

struct FontFallbackCNGlyph {
    uint32_t codepoint;
    uint8_t bitmap[32];
};

extern const uint16_t FONT_FALLBACK_CN_COUNT;
const FontFallbackCNGlyph* findFontFallbackCNGlyph(uint32_t codepoint);
""",
        encoding="utf-8",
    )

    rows = []
    for ch in sorted_chars(chars):
        data_s = ", ".join(f"0x{value:02X}" for value in glyph_bytes(font, ch))
        label = ch if ord(ch) >= 0x80 else f"ASCII U+{ord(ch):04X}"
        rows.append(f"    {{0x{ord(ch):04X}, {{{data_s}}}}}, // {label}")

    FALLBACK_CPP_OUT.write_text(
        """#include "assets/FontFallbackCN.h"
#include "platform/api/FlashStorage.h"

const FontFallbackCNGlyph FONT_FALLBACK_CN_GLYPHS[] STICKMON_FLASH_DATA = {
"""
        + "\n".join(rows)
        + f"""
}};

static_assert(sizeof(FONT_FALLBACK_CN_GLYPHS) /
                  sizeof(FONT_FALLBACK_CN_GLYPHS[0]) == {len(chars)},
              "font fallback glyph count mismatch");

const uint16_t FONT_FALLBACK_CN_COUNT =
    sizeof(FONT_FALLBACK_CN_GLYPHS) / sizeof(FONT_FALLBACK_CN_GLYPHS[0]);

const FontFallbackCNGlyph* findFontFallbackCNGlyph(uint32_t codepoint) {{
    int lo = 0;
    int hi = FONT_FALLBACK_CN_COUNT - 1;
    while (lo <= hi) {{
        int mid = (lo + hi) / 2;
        uint32_t value = FlashStorage::readDword(&FONT_FALLBACK_CN_GLYPHS[mid].codepoint);
        if (value == codepoint) return &FONT_FALLBACK_CN_GLYPHS[mid];
        if (value < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }}
    return nullptr;
}}
""",
        encoding="utf-8",
    )


def main():
    for name, path in (
            ("Sarasa Gothic SC Regular", SARASA_FONT_PATH),
            ("Unscii 16", UNSCII_FONT_PATH)):
        if not path.exists():
            raise FileNotFoundError(f"{name} font not found: {path}")

    regular_font = ImageFont.truetype(str(SARASA_FONT_PATH), 16)
    unscii_glyphs = load_unscii_glyphs(UNSCII_FONT_PATH)
    fallback_chars = collect_fallback_chars()
    pack_chars = collect_ui_chars()
    pack_chars.update(collect_pack_text_chars())
    pack_chars.difference_update(UNSCII_CHARS)

    pack_chars = sorted_chars(pack_chars)
    write_firmware_fallback(fallback_chars, regular_font)
    write_pack_font(
        PACK_FONT_REGULAR_OUT,
        pack_chars,
        lambda ch: glyph_bytes(regular_font, ch),
    )
    write_pack_font(
        PACK_FONT_UNSCII_ASCII_OUT,
        UNSCII_CHARS,
        lambda ch: normalize_unscii_glyph(unscii_glyphs[ch]),
    )
    write_active_config()
    merge_pack_manifest(
        format="smon-resource-pack-v1",
        font="fonts/zh16.smonfont",
        fontCount=2,
        fonts="fonts",
    )
    print(
        f"Generated Sarasa Gothic SC CJK firmware fallback glyphs={len(fallback_chars)} "
        f"LittleFS CJK glyphs={len(pack_chars)} "
        f"Unscii glyphs={len(UNSCII_CHARS)}"
    )


if __name__ == "__main__":
    main()
