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
PACK_FONT_OUT = PACK_OUT / "fonts" / "zh16.smonfont"
FALLBACK_H_OUT = SRC / "assets" / "FontFallbackCN.h"
FALLBACK_CPP_OUT = SRC / "assets" / "FontFallbackCN.cpp"
FONT_PATH = "/System/Library/Fonts/STHeiti Medium.ttc"
FONT_PACK_MAGIC = 0x4E464D53  # SMFN
FONT_PACK_VERSION = 1
FONT_GLYPH_W = 16
FONT_GLYPH_H = 16
FONT_GLYPH_BYTES = 32
TEXT_EXTENSIONS = {".json", ".txt", ".md"}


def collect_chars_from_text(text):
    chars = set()
    for ch in text:
        if "\u4e00" <= ch <= "\u9fff":
            chars.add(ch)
    return chars


def collect_ui_chars():
    text = (SRC / "core" / "UiStrings.h").read_text(encoding="utf-8")
    return collect_chars_from_text(text)


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


def glyph_bytes(font, ch):
    img = Image.new("L", (16, 16), 0)
    draw = ImageDraw.Draw(img)
    bbox = draw.textbbox((0, 0), ch, font=font)
    w = bbox[2] - bbox[0]
    h = bbox[3] - bbox[1]
    x = (16 - w) // 2 - bbox[0]
    y = (16 - h) // 2 - bbox[1]
    draw.text((x, y), ch, font=font, fill=255)

    out = []
    for row in range(16):
        left = 0
        right = 0
        for col in range(8):
            if img.getpixel((col, row)) > 64:
                left |= 1 << (7 - col)
        for col in range(8, 16):
            if img.getpixel((col, row)) > 64:
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


def write_pack_font(chars, font):
    PACK_FONT_OUT.parent.mkdir(parents=True, exist_ok=True)
    glyph_rows = bytearray()
    for ch in sorted_chars(chars):
        glyph_rows += struct.pack("<I", ord(ch))
        glyph_rows += bytes(glyph_bytes(font, ch))

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
    PACK_FONT_OUT.write_bytes(header + glyph_rows)
    write_active_config()
    merge_pack_manifest(
        format="smon-resource-pack-v1",
        font="fonts/zh16.smonfont",
        fontCount=1,
        fonts="fonts",
    )


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
        rows.append(f"    {{0x{ord(ch):04X}, {{{data_s}}}}}, // {ch}")

    FALLBACK_CPP_OUT.write_text(
        """#include "assets/FontFallbackCN.h"
#include <Arduino.h>

const FontFallbackCNGlyph FONT_FALLBACK_CN_GLYPHS[] PROGMEM = {
"""
        + "\n".join(rows)
        + f"""
}};

const uint16_t FONT_FALLBACK_CN_COUNT = {len(chars)};

const FontFallbackCNGlyph* findFontFallbackCNGlyph(uint32_t codepoint) {{
    int lo = 0;
    int hi = FONT_FALLBACK_CN_COUNT - 1;
    while (lo <= hi) {{
        int mid = (lo + hi) / 2;
        uint32_t value = pgm_read_dword(&FONT_FALLBACK_CN_GLYPHS[mid].codepoint);
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
    font = ImageFont.truetype(FONT_PATH, 16, index=0)
    fallback_chars = collect_fallback_chars()
    pack_chars = collect_ui_chars()
    pack_chars.update(collect_pack_text_chars())

    write_firmware_fallback(fallback_chars, font)
    write_pack_font(sorted_chars(pack_chars), font)
    print(
        f"Generated firmware fallback glyphs={len(fallback_chars)} "
        f"LittleFS font glyphs={len(pack_chars)}"
    )


if __name__ == "__main__":
    main()
