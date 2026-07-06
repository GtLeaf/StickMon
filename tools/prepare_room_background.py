#!/usr/bin/env python3
import argparse
import json
import math
import shutil
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOM_DIR = ROOT / "origin_asset" / "room" / "standar"
DEFAULT_SOURCE = DEFAULT_ROOM_DIR / "empty_room.png"
DEFAULT_LAYOUT = DEFAULT_ROOM_DIR / "room_layout.json"
GENERATED_ROOM_DIR = ROOT / "origin_asset" / "generated" / "room"
SOURCE_OUT = GENERATED_ROOM_DIR / "standard_room_source.png"
PNG_OUT = GENERATED_ROOM_DIR / "standard_room_240.png"
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
        return 0, 0, w, h

    left = max(0, min_x - TRIM_PADDING)
    top = max(0, min_y - TRIM_PADDING)
    right = min(w, max_x + TRIM_PADDING + 1)
    bottom = min(h, max_y + TRIM_PADDING + 1)
    return left, top, right - left, bottom - top


def prepare_image(source_path):
    source = Image.open(source_path).convert("RGB")
    crop_box = trim_dark_border(source)
    left, top, crop_w, crop_h = crop_box
    trimmed = source.crop((left, top, left + crop_w, top + crop_h))
    target_h = max(1, round(trimmed.height * DISPLAY_W / trimmed.width))
    resized = trimmed.resize((DISPLAY_W, target_h), Image.Resampling.NEAREST)
    room_y = max(0, (DISPLAY_H - resized.height) // 2)
    return resized, crop_box, room_y


def clamp_room_point(x, y, room_y, room_w, room_h):
    return (
        max(0, min(room_w - 1, round(x))),
        max(room_y, min(room_y + room_h - 1, round(y))),
    )


def load_walk_polygon(layout_path, crop_box, room_y, room_w, room_h):
    if not layout_path or not Path(layout_path).exists():
        return []

    layout = json.loads(Path(layout_path).read_text(encoding="utf-8"))
    faces = layout.get("roomGeometry", {}).get("faces", [])
    floor = next((face for face in faces if face.get("type") == "floor"), None)
    if not floor:
        return []

    source_points = floor.get("sourcePoints") or []
    if source_points:
        left, top, crop_w, _crop_h = crop_box
        scale = DISPLAY_W / crop_w
        return [
            clamp_room_point((x - left) * scale, (y - top) * scale + room_y, room_y, room_w, room_h)
            for x, y in source_points
        ]

    return [clamp_room_point(x, y, room_y, room_w, room_h) for x, y in floor.get("points", [])]


def bounds_for(points):
    if not points:
        return 0, 0, DISPLAY_W - 1, DISPLAY_H - 1
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    return min(xs), min(ys), max(xs), max(ys)


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


def format_points(points):
    return "\n".join(f"    {{{x}, {y}}}," for x, y in points)


def write_room_assets(img, room_y, walk_polygon):
    rle = rle_rgb565_image(img)
    min_x, min_y, max_x, max_y = bounds_for(walk_polygon)
    polygon_count = len(walk_polygon)

    HEADER_OUT.write_text(f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {{

struct RoomPoint {{
    int16_t x;
    int16_t y;
}};

static constexpr uint16_t STANDARD_ROOM_W = {img.width};
static constexpr uint16_t STANDARD_ROOM_H = {img.height};
static constexpr int16_t STANDARD_ROOM_Y = {room_y};
static constexpr uint32_t STANDARD_ROOM_RLE_LEN = {len(rle)};

static constexpr uint8_t ROOM_WALK_POLYGON_COUNT = {polygon_count};
static constexpr int16_t ROOM_WALK_MIN_X = {min_x};
static constexpr int16_t ROOM_WALK_MIN_Y = {min_y};
static constexpr int16_t ROOM_WALK_MAX_X = {max_x};
static constexpr int16_t ROOM_WALK_MAX_Y = {max_y};

extern const uint16_t STANDARD_ROOM_RLE[] PROGMEM;
extern const RoomPoint ROOM_WALK_POLYGON[] PROGMEM;

}}
""", encoding="utf-8")

    CPP_OUT.write_text(f"""#include "RoomAssets.h"

namespace RoomAssets {{

const uint16_t STANDARD_ROOM_RLE[] PROGMEM = {{
{format_words(rle)}
}};

const RoomPoint ROOM_WALK_POLYGON[] PROGMEM = {{
{format_points(walk_polygon)}
}};

}}
""", encoding="utf-8")

    return len(rle)


def main():
    parser = argparse.ArgumentParser(description="Trim and install the StickMon standard room background.")
    parser.add_argument("source", nargs="?", default=str(DEFAULT_SOURCE), help="Generated room image to install")
    parser.add_argument("--layout", default=str(DEFAULT_LAYOUT), help="room_layout.json containing the floor walk polygon")
    parser.add_argument("--source-out", default=str(SOURCE_OUT), help="Raw source copy kept under origin_asset/generated")
    parser.add_argument("--png-out", default=str(PNG_OUT), help="Trimmed 240px-wide room PNG under origin_asset/generated")
    args = parser.parse_args()

    source_path = Path(args.source)
    source_out = Path(args.source_out)
    png_out = Path(args.png_out)
    source_out.parent.mkdir(parents=True, exist_ok=True)
    png_out.parent.mkdir(parents=True, exist_ok=True)

    shutil.copyfile(source_path, source_out)
    prepared, crop_box, room_y = prepare_image(source_path)
    prepared.save(png_out)
    walk_polygon = load_walk_polygon(args.layout, crop_box, room_y, prepared.width, prepared.height)
    rle_len = write_room_assets(prepared, room_y, walk_polygon)
    left, top, crop_w, crop_h = crop_box
    print(f"source={source_path}")
    print(f"source_copy={source_out}")
    print(f"png={png_out} size={prepared.width}x{prepared.height} y={room_y}")
    print(f"trim={left},{top},{crop_w},{crop_h}")
    print(f"walk_polygon_points={len(walk_polygon)}")
    print(f"assets={HEADER_OUT}, {CPP_OUT} rle_words={rle_len}")


if __name__ == "__main__":
    main()
