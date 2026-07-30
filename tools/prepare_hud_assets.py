#!/usr/bin/env python3
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "origin_asset" / "icon" / "home" / "icon_hun.png"
HEADER_OUT = ROOT / "src" / "assets" / "HudAssets.h"
CPP_OUT = ROOT / "src" / "assets" / "HudAssets.cpp"

TARGET_W = 14
TARGET_H = 14
ALPHA_THRESHOLD = 64


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def source_crop(img):
    alpha_bbox = img.getchannel("A").getbbox()
    if not alpha_bbox:
        return img
    left, top, right, bottom = alpha_bbox
    return img.crop((left, top, right, bottom))


def prepare_icon():
    img = Image.open(SOURCE).convert("RGBA")
    img = source_crop(img)
    img.thumbnail((TARGET_W, TARGET_H), Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (TARGET_W, TARGET_H), (255, 255, 255, 0))
    x = (TARGET_W - img.width) // 2
    y = (TARGET_H - img.height) // 2
    canvas.alpha_composite(img, (x, y))
    return canvas


def rle_rgba565(img):
    if hasattr(img, "get_flattened_data"):
        pixels = list(img.get_flattened_data())
    else:
        pixels = list(img.getdata())
    values = []
    i = 0
    while i < len(pixels):
        r, g, b, a = pixels[i]
        transparent = a < ALPHA_THRESHOLD
        run = 1
        while i + run < len(pixels) and run < 0x7FFF:
            nr, ng, nb, na = pixels[i + run]
            if (na < ALPHA_THRESHOLD) != transparent:
                break
            if not transparent and rgb565(r, g, b) != rgb565(nr, ng, nb):
                break
            run += 1

        if transparent:
            values.append(0x8000 | run)
        else:
            values.append(run)
            values.extend([rgb565(r, g, b)] * run)
        i += run
    return values


def format_words(values):
    rows = []
    for index in range(0, len(values), 12):
        rows.append("    " + ", ".join(f"0x{value:04X}" for value in values[index:index + 12]) + ",")
    return "\n".join(rows)


def main():
    icon = prepare_icon()
    rle = rle_rgba565(icon)

    HEADER_OUT.write_text(f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace HudAssets {{

static constexpr uint8_t HUNGER_ICON_W = {TARGET_W};
static constexpr uint8_t HUNGER_ICON_H = {TARGET_H};
static constexpr uint16_t HUNGER_ICON_RLE_LEN = {len(rle)};

extern const uint16_t HUNGER_ICON_RLE[] PROGMEM;

}}
""", encoding="utf-8")

    CPP_OUT.write_text(f"""#include "HudAssets.h"

namespace HudAssets {{

const uint16_t HUNGER_ICON_RLE[] PROGMEM = {{
{format_words(rle)}
}};

}}
""", encoding="utf-8")

    print(f"source={SOURCE}")
    print(f"icon={TARGET_W}x{TARGET_H} rle_words={len(rle)}")
    print(f"assets={HEADER_OUT}, {CPP_OUT}")


if __name__ == "__main__":
    main()
