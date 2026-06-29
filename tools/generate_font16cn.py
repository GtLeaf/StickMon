#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
OUT_H = SRC / "assets" / "Font16CN.h"
OUT_CPP = SRC / "assets" / "Font16CN.cpp"
FONT_PATH = "/System/Library/Fonts/STHeiti Medium.ttc"


def collect_chars():
    chars = set()
    text = (SRC / "core" / "UiStrings.h").read_text(encoding="utf-8")
    for ch in text:
        if "\u4e00" <= ch <= "\u9fff":
            chars.add(ch)
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


def main():
    chars = collect_chars()
    OUT_H.parent.mkdir(parents=True, exist_ok=True)
    font = ImageFont.truetype(FONT_PATH, 16, index=0)

    OUT_H.write_text(
        """#pragma once

#include <cstdint>

struct Font16CNGlyph {
    uint32_t codepoint;
    uint8_t bitmap[32];
};

extern const uint16_t FONT16CN_COUNT;
const Font16CNGlyph* findFont16CNGlyph(uint32_t codepoint);
""",
        encoding="utf-8",
    )

    rows = []
    for ch in chars:
        data = glyph_bytes(font, ch)
        data_s = ", ".join(f"0x{b:02X}" for b in data)
        rows.append(f"    {{0x{ord(ch):04X}, {{{data_s}}}}}, // {ch}")

    OUT_CPP.write_text(
        """#include "assets/Font16CN.h"
#include <Arduino.h>

const Font16CNGlyph FONT16CN_GLYPHS[] PROGMEM = {
"""
        + "\n".join(rows)
        + f"""
}};

const uint16_t FONT16CN_COUNT = {len(chars)};

const Font16CNGlyph* findFont16CNGlyph(uint32_t codepoint) {{
    int lo = 0;
    int hi = FONT16CN_COUNT - 1;
    while (lo <= hi) {{
        int mid = (lo + hi) / 2;
        uint32_t value = pgm_read_dword(&FONT16CN_GLYPHS[mid].codepoint);
        if (value == codepoint) return &FONT16CN_GLYPHS[mid];
        if (value < codepoint) lo = mid + 1;
        else hi = mid - 1;
    }}
    return nullptr;
}}
""",
        encoding="utf-8",
    )
    print(f"Generated {len(chars)} glyphs")


if __name__ == "__main__":
    main()
