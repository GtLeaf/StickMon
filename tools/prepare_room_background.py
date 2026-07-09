#!/usr/bin/env python3
import argparse
import json
import sys
from copy import deepcopy
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ROOM_EDITOR_DIR = ROOT / "tools" / "room_editor"
sys.path.insert(0, str(ROOM_EDITOR_DIR))

from compose_room import composite  # noqa: E402


DEFAULT_ROOM_DIR = ROOT / "origin_asset" / "room" / "standar"
DEFAULT_LAYOUT = DEFAULT_ROOM_DIR / "room_layout.json"
DEFAULT_FURNITURE_DIR = DEFAULT_ROOM_DIR / "forniture"
GENERATED_ROOM_DIR = ROOT / "origin_asset" / "generated" / "room"
DAY_SOURCE_OUT = GENERATED_ROOM_DIR / "standard_room_day_source.png"
NIGHT_SOURCE_OUT = GENERATED_ROOM_DIR / "standard_room_night_source.png"
DAY_PNG_OUT = GENERATED_ROOM_DIR / "standard_room_day_240.png"
NIGHT_PNG_OUT = GENERATED_ROOM_DIR / "standard_room_night_240.png"
LEGACY_PNG_OUT = GENERATED_ROOM_DIR / "standard_room_240.png"
HEADER_OUT = ROOT / "src" / "assets" / "RoomAssets.h"
CPP_OUT = ROOT / "src" / "assets" / "RoomAssets.cpp"

DISPLAY_H = 135


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def read_layout(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def room_size(layout):
    geometry = layout.get("roomGeometry") or {}
    room = geometry.get("room") or {}
    canvas = layout.get("canvas") or {}
    return (
        int(room.get("width") or canvas.get("width") or 240),
        int(room.get("height") or canvas.get("height") or 135),
    )


def resolve_room_path(layout_path, value):
    if not value:
        return None
    path = Path(value)
    if path.is_absolute():
        return path
    return Path(layout_path).resolve().parent / path


def background_path(layout_path, layout, mode, override=None, fallback=None):
    if override:
        return Path(override)
    backgrounds = layout.get("backgrounds") or {}
    info = backgrounds.get(mode) or {}
    file_name = info.get("fileName") if isinstance(info, dict) else None
    if not file_name and mode == "day":
        file_name = (layout.get("base") or {}).get("fileName")
    if not file_name and fallback:
        return Path(fallback)
    path = resolve_room_path(layout_path, file_name)
    if not path:
        raise FileNotFoundError(f"{mode} background is missing in room_layout.json")
    return path


def layout_without_preview_sprites(layout):
    prepared = deepcopy(layout)
    prepared["furniture"] = [
        item for item in prepared.get("furniture", [])
        if item.get("source", "furniture") == "furniture"
    ]
    return prepared


def compose_room_image(layout, background, furniture_dir, mode):
    prepared_layout = layout_without_preview_sprites(layout)
    return composite(prepared_layout, str(background), str(furniture_dir), mode, include_lighting=False).convert("RGB")


def clamp_room_point(x, y, room_w, room_h):
    return (
        max(0, min(room_w - 1, round(float(x)))),
        max(0, min(room_h - 1, round(float(y)))),
    )


def load_walk_polygon(layout, room_w, room_h):
    geometry = layout.get("roomGeometry") or {}
    sprite_areas = geometry.get("spriteAreas") or []
    if sprite_areas:
        points = sprite_areas[0].get("points") or []
        return [clamp_room_point(x, y, room_w, room_h) for x, y in points]

    faces = geometry.get("faces") or []
    floor = next((face for face in faces if face.get("type") == "floor"), None)
    if not floor:
        return []
    return [clamp_room_point(x, y, room_w, room_h) for x, y in floor.get("points", [])]


def bounds_for(points, width, height):
    if not points:
        return 0, 0, width - 1, height - 1
    xs = [point[0] for point in points]
    ys = [point[1] for point in points]
    return min(xs), min(ys), max(xs), max(ys)


def food_position(layout):
    for item in layout.get("furniture", []):
        if item.get("source", "furniture") != "furniture":
            continue
        label = furniture_label(item)
        if "bowl" not in label and "food" not in label:
            continue
        x = float(item.get("x", 0))
        y = float(item.get("y", 0))
        w = float(item.get("targetWidth") or 0)
        h = float(item.get("targetHeight") or 0)
        return round(x + w * 0.5), round(y + h * 0.42)
    return 191, 108


def furniture_label(item):
    return " ".join([
        str(item.get("furnitureType", "")),
        str(item.get("kind", "")),
        str(item.get("fileName", "")),
        str(item.get("name", "")),
    ]).lower()


def furniture_item(layout, *keywords):
    for item in layout.get("furniture", []):
        if item.get("source", "furniture") != "furniture":
            continue
        label = furniture_label(item)
        if any(keyword in label for keyword in keywords):
            return item
    return None


def furniture_polygon(item, key, room_w, room_h):
    if not item:
        return []
    raw_points = item.get(key) or []
    if not raw_points:
        return []

    x = float(item.get("x", 0))
    y = float(item.get("y", 0))
    w = float(item.get("targetWidth") or item.get("width") or 0)
    h = float(item.get("targetHeight") or item.get("height") or 0)
    normalized = all(-0.1 <= float(px) <= 1.1 and -0.1 <= float(py) <= 1.1 for px, py in raw_points)
    points = []
    for px, py in raw_points:
        px = float(px)
        py = float(py)
        if normalized:
            points.append(clamp_room_point(x + px * w, y + py * h, room_w, room_h))
        else:
            points.append(clamp_room_point(x + px, y + py, room_w, room_h))
    return points


def polygon_anchor(points):
    if not points:
        return 0, 0

    area_twice = 0.0
    cx = 0.0
    cy = 0.0
    for i, (x0, y0) in enumerate(points):
        x1, y1 = points[(i + 1) % len(points)]
        cross = float(x0 * y1 - x1 * y0)
        area_twice += cross
        cx += (x0 + x1) * cross
        cy += (y0 + y1) * cross

    if abs(area_twice) < 0.001:
        return round(sum(x for x, _ in points) / len(points)), round(sum(y for _, y in points) / len(points))

    return round(cx / (3.0 * area_twice)), round(cy / (3.0 * area_twice))


def bed_region(layout, room_w, room_h, walk_polygon):
    bed = furniture_item(layout, "bed")
    points = furniture_polygon(bed, "footprintPolygon", room_w, room_h)
    if not points and bed:
        x = float(bed.get("x", 0))
        y = float(bed.get("y", 0))
        w = float(bed.get("targetWidth") or 0)
        h = float(bed.get("targetHeight") or 0)
        points = [
            clamp_room_point(x, y + h * 0.55, room_w, room_h),
            clamp_room_point(x + w, y + h * 0.55, room_w, room_h),
            clamp_room_point(x + w, y + h, room_w, room_h),
            clamp_room_point(x, y + h, room_w, room_h),
        ]
    if not points:
        points = walk_polygon[:1] or [(room_w // 2, room_h // 2)]
    bed_x, bed_y = polygon_anchor(points)
    bed_x, bed_y = clamp_room_point(bed_x, bed_y, room_w, room_h)
    return points, bed_x, bed_y


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


def write_room_assets(day_img, night_img, walk_polygon, food_x, food_y, bed_polygon, bed_x, bed_y):
    if day_img.size != night_img.size:
        raise ValueError(f"day/night room sizes differ: {day_img.size} vs {night_img.size}")

    width, height = day_img.size
    room_y = max(0, (DISPLAY_H - height) // 2)
    day_rle = rle_rgb565_image(day_img)
    night_rle = rle_rgb565_image(night_img)
    min_x, min_y, max_x, max_y = bounds_for(walk_polygon, width, height)
    polygon_count = len(walk_polygon)
    bed_min_x, bed_min_y, bed_max_x, bed_max_y = bounds_for(bed_polygon, width, height)
    bed_polygon_count = len(bed_polygon)

    HEADER_OUT.write_text(f"""#pragma once
#include <Arduino.h>
#include <cstdint>

namespace RoomAssets {{

struct RoomPoint {{
    int16_t x;
    int16_t y;
}};

static constexpr uint16_t STANDARD_ROOM_W = {width};
static constexpr uint16_t STANDARD_ROOM_H = {height};
static constexpr int16_t STANDARD_ROOM_Y = {room_y};
static constexpr uint32_t STANDARD_ROOM_DAY_RLE_LEN = {len(day_rle)};
static constexpr uint32_t STANDARD_ROOM_NIGHT_RLE_LEN = {len(night_rle)};

static constexpr uint8_t ROOM_WALK_POLYGON_COUNT = {polygon_count};
static constexpr int16_t ROOM_WALK_MIN_X = {min_x};
static constexpr int16_t ROOM_WALK_MIN_Y = {min_y};
static constexpr int16_t ROOM_WALK_MAX_X = {max_x};
static constexpr int16_t ROOM_WALK_MAX_Y = {max_y};

static constexpr int16_t ROOM_FOOD_X = {food_x};
static constexpr int16_t ROOM_FOOD_Y = {food_y};

static constexpr uint8_t ROOM_BED_POLYGON_COUNT = {bed_polygon_count};
static constexpr int16_t ROOM_BED_MIN_X = {bed_min_x};
static constexpr int16_t ROOM_BED_MIN_Y = {bed_min_y};
static constexpr int16_t ROOM_BED_MAX_X = {bed_max_x};
static constexpr int16_t ROOM_BED_MAX_Y = {bed_max_y};
static constexpr int16_t ROOM_BED_X = {bed_x};
static constexpr int16_t ROOM_BED_Y = {bed_y};

extern const uint16_t STANDARD_ROOM_DAY_RLE[] PROGMEM;
extern const uint16_t STANDARD_ROOM_NIGHT_RLE[] PROGMEM;
extern const RoomPoint ROOM_WALK_POLYGON[] PROGMEM;
extern const RoomPoint ROOM_BED_POLYGON[] PROGMEM;

}}
""", encoding="utf-8")

    CPP_OUT.write_text(f"""#include "RoomAssets.h"

namespace RoomAssets {{

const uint16_t STANDARD_ROOM_DAY_RLE[] PROGMEM = {{
{format_words(day_rle)}
}};

const uint16_t STANDARD_ROOM_NIGHT_RLE[] PROGMEM = {{
{format_words(night_rle)}
}};

const RoomPoint ROOM_WALK_POLYGON[] PROGMEM = {{
{format_points(walk_polygon)}
}};

const RoomPoint ROOM_BED_POLYGON[] PROGMEM = {{
{format_points(bed_polygon)}
}};

}}
""", encoding="utf-8")

    return len(day_rle), len(night_rle), room_y


def main():
    parser = argparse.ArgumentParser(description="Compose and install StickMon room assets from room_layout.json.")
    parser.add_argument("source", nargs="?", default=None, help="Legacy day background override")
    parser.add_argument("--layout", default=str(DEFAULT_LAYOUT), help="room_layout.json source of truth")
    parser.add_argument("--day-base", help="Day background image. Defaults to backgrounds.day.fileName")
    parser.add_argument("--night-base", help="Night background image. Defaults to backgrounds.night.fileName")
    parser.add_argument("--furniture-dir", default=str(DEFAULT_FURNITURE_DIR), help="Directory containing furniture PNG files")
    parser.add_argument("--day-source-out", default=str(DAY_SOURCE_OUT), help="Copy of the day source background")
    parser.add_argument("--night-source-out", default=str(NIGHT_SOURCE_OUT), help="Copy of the night source background")
    parser.add_argument("--day-png-out", default=str(DAY_PNG_OUT), help="Composed day room PNG")
    parser.add_argument("--night-png-out", default=str(NIGHT_PNG_OUT), help="Composed night room PNG")
    parser.add_argument("--png-out", default=str(LEGACY_PNG_OUT), help="Legacy composed day PNG copy")
    args = parser.parse_args()

    layout_path = Path(args.layout)
    layout = read_layout(layout_path)
    day_base = background_path(layout_path, layout, "day", args.day_base or args.source)
    night_base = background_path(layout_path, layout, "night", args.night_base, day_base)
    furniture_dir = Path(args.furniture_dir)

    day_source_out = Path(args.day_source_out)
    night_source_out = Path(args.night_source_out)
    day_png_out = Path(args.day_png_out)
    night_png_out = Path(args.night_png_out)
    legacy_png_out = Path(args.png_out)
    for path in (day_source_out, night_source_out, day_png_out, night_png_out, legacy_png_out):
        path.parent.mkdir(parents=True, exist_ok=True)

    Image.open(day_base).save(day_source_out)
    Image.open(night_base).save(night_source_out)

    day_img = compose_room_image(layout, day_base, furniture_dir, "day")
    night_img = compose_room_image(layout, night_base, furniture_dir, "night")
    day_img.save(day_png_out)
    night_img.save(night_png_out)
    day_img.save(legacy_png_out)

    width, height = room_size(layout)
    walk_polygon = load_walk_polygon(layout, width, height)
    food_x, food_y = food_position(layout)
    bed_polygon, bed_x, bed_y = bed_region(layout, width, height, walk_polygon)
    day_len, night_len, room_y = write_room_assets(day_img, night_img, walk_polygon, food_x, food_y,
                                                   bed_polygon, bed_x, bed_y)

    print(f"layout={layout_path}")
    print(f"day_base={day_base}")
    print(f"night_base={night_base}")
    print(f"furniture_dir={furniture_dir}")
    print(f"day_png={day_png_out} size={day_img.width}x{day_img.height}")
    print(f"night_png={night_png_out} size={night_img.width}x{night_img.height}")
    print(f"legacy_png={legacy_png_out}")
    print(f"room_y={room_y}")
    print(f"walk_polygon_points={len(walk_polygon)}")
    print(f"food={food_x},{food_y}")
    print(f"bed={bed_x},{bed_y} bed_polygon_points={len(bed_polygon)}")
    print(f"assets={HEADER_OUT}, {CPP_OUT} day_rle_words={day_len} night_rle_words={night_len}")


if __name__ == "__main__":
    main()
