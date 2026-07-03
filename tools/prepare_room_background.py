#!/usr/bin/env python3
from pathlib import Path
import argparse
import math
import shutil

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE_OUT = ROOT / "origin_asset" / "room" / "standard_room_source.png"
PNG_OUT = ROOT / "origin_asset" / "room" / "standard_room_240.png"
HEADER_OUT = ROOT / "src" / "assets" / "RoomAssets.h"
CPP_OUT = ROOT / "src" / "assets" / "RoomAssets.cpp"

DISPLAY_W = 240
DISPLAY_H = 135
TRIM_PADDING = 4
TRIM_THRESHOLD = 34


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def corner_background(img):
    w, h = img.size
    points = [
        (0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1),
        (w // 2, 0), (w // 2, h - 1),
    ]
    values = [img.getpixel(point)[:3] for point in points]
    return tuple(round(sum(color[i] for color in values) / len(values)) for i in range(3))


def color_distance(a, b):
    return math.sqrt(sum((int(a[i]) - int(b[i])) ** 2 for i in range(3)))


def trim_dark_border(img):
    bg = corner_background(img)
    pixels = img.load()
    w, h = img.size
    min_x, min_y = w, h
    max_x, max_y = -1, -1

    for y in range(h):
        for x in range(w):
            color = pixels[x, y][:3]
            if color_distance(color, bg) <= TRIM_THRESHOLD:
                continue
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)

    if max_x < min_x or max_y < min_y:
        return img

    box = (
        max(0, min_x - TRIM_PADDING),
        max(0, min_y - TRIM_PADDING),
        min(w, max_x + TRIM_PADDING + 1),
        min(h, max_y + TRIM_PADDING + 1),
    )
    return img.crop(box)


def prepare_image(source_path):
    source = Image.open(source_path).convert("RGB")
    trimmed = trim_dark_border(source)
    target_h = max(1, round(trimmed.height * DISPLAY_W / trimmed.width))
    resized = trimmed.resize((DISPLAY_W, target_h), Image.Resampling.NEAREST)
    return resized


def rle_rgb565_image(img):
    rgb = img.convert("RGB")
    if hasattr(rgb, "get_flattened_data"):
        pixels = list(rgb.get_flattened_data())
    else:
        pixels = list(rgb.getdata())
    values = []
    i = 0
    total = len(pixels)
    while i < total:
        run = []
        while i < total and len(run) < 0x7FFF:
            run.append(rgb565(*pixels[i]))
            i += 1
        values.append(len(run))
        values.extend(run)
    return values


def format_words(values):
    rows = []
    for index in range(0, len(values), 12):
        rows.append("    " + ", ".join(f"0x{value:04X}" for value in values[index:index + 12]) + ",")
    return "\n".join(rows)


def write_room_assets(img):
    rle = rle_rgb565_image(img)
    room_y = max(0, (DISPLAY_H - img.height) // 2)

    HEADER_OUT.write_text(f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {{

static constexpr uint16_t STANDARD_ROOM_W = {img.width};
static constexpr uint16_t STANDARD_ROOM_H = {img.height};
static constexpr int16_t STANDARD_ROOM_Y = {room_y};
static constexpr uint32_t STANDARD_ROOM_RLE_LEN = {len(rle)};

extern const uint16_t STANDARD_ROOM_RLE[] PROGMEM;

}}
""", encoding="utf-8")

    CPP_OUT.write_text(f"""#include "RoomAssets.h"

namespace RoomAssets {{

const uint16_t STANDARD_ROOM_RLE[] PROGMEM = {{
{format_words(rle)}
}};

}}
""", encoding="utf-8")

    return room_y, len(rle)


def main():
    parser = argparse.ArgumentParser(description="Trim and install the StickMon standard room background.")
    parser.add_argument("source", help="Generated room image to install")
    parser.add_argument("--source-out", default=str(SOURCE_OUT), help="Raw source copy kept in origin_asset")
    parser.add_argument("--png-out", default=str(PNG_OUT), help="Trimmed 240px-wide room PNG")
    args = parser.parse_args()

    source_path = Path(args.source)
    source_out = Path(args.source_out)
    png_out = Path(args.png_out)
    source_out.parent.mkdir(parents=True, exist_ok=True)
    png_out.parent.mkdir(parents=True, exist_ok=True)

    shutil.copyfile(source_path, source_out)
    prepared = prepare_image(source_path)
    prepared.save(png_out)
    room_y, rle_len = write_room_assets(prepared)
    print(f"source={source_out}")
    print(f"png={png_out} size={prepared.width}x{prepared.height} y={room_y}")
    print(f"assets={HEADER_OUT}, {CPP_OUT} rle_words={rle_len}")


if __name__ == "__main__":
    main()
