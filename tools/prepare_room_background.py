#!/usr/bin/env python3
import argparse
import json
import struct
import sys
import zlib
from copy import deepcopy
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ROOM_EDITOR_DIR = ROOT / "tools" / "room_editor"
sys.path.insert(0, str(ROOM_EDITOR_DIR))

from compose_room import background_info, background_layers_for_mode, composite, fit_base, layout_trim, render_base  # noqa: E402


DEFAULT_ROOM_DIR = ROOT / "origin_asset" / "room" / "standar"
DEFAULT_LAYOUT = DEFAULT_ROOM_DIR / "room_layout.json"
DEFAULT_FURNITURE_DIR = DEFAULT_ROOM_DIR.parent / "forniture"
GENERATED_ROOM_DIR = ROOT / "origin_asset" / "generated" / "room"
DAY_SOURCE_OUT = GENERATED_ROOM_DIR / "standard_room_day_source.png"
NIGHT_SOURCE_OUT = GENERATED_ROOM_DIR / "standard_room_night_source.png"
DAY_PNG_OUT = GENERATED_ROOM_DIR / "standard_room_day_240.png"
NIGHT_PNG_OUT = GENERATED_ROOM_DIR / "standard_room_night_240.png"
LEGACY_PNG_OUT = GENERATED_ROOM_DIR / "standard_room_240.png"
HEADER_OUT = ROOT / "src" / "assets" / "RoomAssets.h"
CPP_OUT = ROOT / "src" / "assets" / "RoomAssets.cpp"
DATA_DIR = ROOT / "data"
PACK_OUT = DATA_DIR / "packs" / "dev"
PACK_ROOM_OUT = PACK_OUT / "rooms"

DISPLAY_H = 135
ROOM_PACK_MAGIC = 0x4D4F5253
ROOM_PACK_VERSION = 3
ROOM_PACK_HEADER_FORMAT = "<IHHHhIIIIBBBBhhhhhhhhhhhhhhhhhhhhHH"
ROOM_BEHAVIOR_ANCHOR_FORMAT = "<BBhh"
ROOM_ANCHOR_WINDOW_GAZE = 1
ROOM_FACING_BACK = 4


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
    layers = background_layers_for_mode(layout, mode)
    if layers:
        file_name = layers[0].get("fileName") or layers[0].get("name")
        path = resolve_room_path(layout_path, file_name)
        if path:
            return path
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


def compose_room_image(layout, background, furniture_dir, mode, layout_dir):
    prepared_layout = layout_without_preview_sprites(layout)
    return composite(
        prepared_layout,
        str(background) if background else None,
        str(furniture_dir),
        mode,
        include_lighting=False,
        layout_dir=layout_dir,
    ).convert("RGB")


def expected_background_size(layout, mode):
    geometry = layout.get("roomGeometry") or {}
    candidates = [
        background_info(layout, mode),
        layout.get("base") or {},
        geometry.get("reference") or {},
    ]
    for info in candidates:
        try:
            width = int(info.get("sourceWidth") or 0)
            height = int(info.get("sourceHeight") or 0)
        except (TypeError, ValueError):
            continue
        if width > 0 and height > 0:
            return width, height
    return None


def direct_room_image(layout, image_path, mode):
    width, height = room_size(layout)
    base = Image.open(image_path).convert("RGBA")
    if base.size == (width, height):
        return base.convert("RGB")

    fit = (
        background_info(layout, mode).get("fit")
        or (layout.get("backgrounds") or {}).get("fit")
        or (layout.get("base") or {}).get("fit", "cover")
    )
    expected_size = expected_background_size(layout, mode)
    use_layout_trim = expected_size is None or expected_size == base.size
    trim = layout_trim(layout, base, mode) if use_layout_trim else (0, 0, base.width, base.height)
    return fit_base(base, width, height, fit, trim).convert("RGB")


def clamp_room_point(x, y, room_w, room_h):
    return (
        max(0, min(room_w - 1, round(float(x)))),
        max(0, min(room_h - 1, round(float(y)))),
    )


def load_walk_polygon(layout, room_w, room_h):
    geometry = layout.get("roomGeometry") or {}
    faces = geometry.get("faces") or []
    sprite_face = next((face for face in faces if face.get("type") == "sprite_area"), None)
    if sprite_face:
        return [clamp_room_point(x, y, room_w, room_h) for x, y in sprite_face.get("points", [])]

    sprite_areas = geometry.get("spriteAreas") or []
    if sprite_areas:
        points = sprite_areas[0].get("points") or []
        return [clamp_room_point(x, y, room_w, room_h) for x, y in points]

    floor = next((face for face in faces if face.get("type") == "floor"), None)
    if not floor:
        return []
    return [clamp_room_point(x, y, room_w, room_h) for x, y in floor.get("points", [])]


def doorway_region(layout, room_w, room_h, walk_polygon):
    geometry = layout.get("roomGeometry") or {}
    faces = geometry.get("faces") or []
    doorway = next((face for face in faces if face.get("type") == "doorway"), None)
    if not doorway:
        return [], (0, 0), (0, 0)

    points = [clamp_room_point(x, y, room_w, room_h) for x, y in doorway.get("points", [])]
    if len(points) < 3:
        return [], (0, 0), (0, 0)

    walk_x, walk_y = polygon_anchor(walk_polygon)
    edge_midpoints = []
    for index, (x0, y0) in enumerate(points):
        x1, y1 = points[(index + 1) % len(points)]
        midpoint = clamp_room_point((x0 + x1) * 0.5, (y0 + y1) * 0.5, room_w, room_h)
        distance = (midpoint[0] - walk_x) ** 2 + (midpoint[1] - walk_y) ** 2
        edge_midpoints.append((distance, midpoint))

    inside = min(edge_midpoints, key=lambda item: item[0])[1]
    outside = max(edge_midpoints, key=lambda item: item[0])[1]
    return points, inside, outside


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


def point_in_polygon(x, y, points):
    inside = False
    previous = points[-1]
    for current in points:
        x0, y0 = previous
        x1, y1 = current
        if (y0 > y) != (y1 > y):
            edge_x = (x1 - x0) * (y - y0) / float(y1 - y0) + x0
            if x < edge_x:
                inside = not inside
        previous = current
    return inside


def generic_monster_footprint_inside(x, foot_y, walk_polygon):
    radius_x = 16.0
    radius_y = 8.0
    diagonal_x = radius_x * 0.70
    diagonal_y = radius_y * 0.70
    samples = (
        (x, foot_y),
        (x - radius_x, foot_y),
        (x + radius_x, foot_y),
        (x, foot_y - radius_y),
        (x - diagonal_x, foot_y - diagonal_y),
        (x + diagonal_x, foot_y - diagonal_y),
    )
    return all(point_in_polygon(px, py, walk_polygon) for px, py in samples)


def behavior_anchors_for_layout(layout, room_w, room_h, walk_polygon):
    window = furniture_item(layout, "window", "窗")
    if not window or len(walk_polygon) < 3:
        return []

    manual = window.get("interactionFoot")
    if isinstance(manual, list) and len(manual) == 2:
        foot_x, foot_y = clamp_room_point(manual[0], manual[1], room_w, room_h)
        if generic_monster_footprint_inside(foot_x, foot_y, walk_polygon):
            return [(ROOM_ANCHOR_WINDOW_GAZE, ROOM_FACING_BACK, foot_x, foot_y)]

    window_x = float(window.get("x", 0)) + float(window.get("targetWidth") or 0) * 0.5
    window_y = float(window.get("y", 0)) + float(window.get("targetHeight") or 0) * 0.5
    best = None
    for foot_y in range(room_h):
        for foot_x in range(room_w):
            if not generic_monster_footprint_inside(foot_x, foot_y, walk_polygon):
                continue
            # Prefer standing horizontally in front of the window instead of
            # selecting a much lower point that merely has a similar distance.
            distance = 4.0 * (foot_x - window_x) ** 2 + (foot_y - window_y) ** 2
            if foot_y < window_y + 20:
                distance += 10000
            candidate = (distance, foot_x, foot_y)
            if best is None or candidate < best:
                best = candidate
    if best is None:
        return []
    return [(ROOM_ANCHOR_WINDOW_GAZE, ROOM_FACING_BACK, best[1], best[2])]


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


def rgb565_pixels(img):
    rgb = img.convert("RGB")
    if hasattr(rgb, "get_flattened_data"):
        pixels = list(rgb.get_flattened_data())
    else:
        pixels = list(rgb.getdata())
    return [rgb565(*pixel) for pixel in pixels]


def rgb565_bytes(img):
    values = rgb565_pixels(img)
    out = bytearray()
    for value in values:
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    return bytes(out)


def raw_deflate(data):
    compressor = zlib.compressobj(6, zlib.DEFLATED, -15)
    return compressor.compress(data) + compressor.flush()


def night_patch_runs(day_img, night_img):
    if day_img.size != night_img.size:
        raise ValueError(f"day/night room sizes differ: {day_img.size} vs {night_img.size}")

    width, height = day_img.size
    day_pixels = rgb565_pixels(day_img)
    night_pixels = rgb565_pixels(night_img)
    runs = []
    patch_pixels = []
    for y in range(height):
        row_offset = y * width
        x = 0
        while x < width:
            index = row_offset + x
            if day_pixels[index] == night_pixels[index]:
                x += 1
                continue

            start = x
            color_offset = len(patch_pixels)
            while x < width:
                index = row_offset + x
                if day_pixels[index] == night_pixels[index]:
                    break
                patch_pixels.append(night_pixels[index])
                x += 1
            runs.append((y, start, x - start, color_offset))
    return runs, patch_pixels


def format_words(values):
    rows = []
    for index in range(0, len(values), 12):
        rows.append("    " + ", ".join(f"0x{value:04X}" for value in values[index:index + 12]) + ",")
    return "\n".join(rows)


def format_bytes(values):
    rows = []
    for index in range(0, len(values), 16):
        rows.append("    " + ", ".join(f"0x{value:02X}" for value in values[index:index + 16]) + ",")
    return "\n".join(rows)


def format_points(points):
    return "\n".join(f"    {{{x}, {y}}}," for x, y in points)


def format_patch_runs(runs):
    if not runs:
        return "    {0, 0, 0, 0},"
    return "\n".join(f"    {{{y}, {x}, {length}, {offset}}}," for y, x, length, offset in runs)


def write_json_file(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def merge_pack_manifest(**updates):
    manifest_path = PACK_OUT / "manifest.json"
    if manifest_path.exists():
        try:
            loaded = json.loads(manifest_path.read_text(encoding="utf-8"))
            payload = loaded if isinstance(loaded, dict) else {}
        except (json.JSONDecodeError, OSError):
            payload = {}
    else:
        payload = {}
    payload.update({
        "format": "smon-resource-pack-v1",
        "id": "dev",
        "schema": 1,
        "version": "0.0.0-dev",
    })
    payload.update(updates)
    write_json_file(manifest_path, payload)
    write_json_file(DATA_DIR / "active.json", {
        "activePack": "dev",
        "packPath": "/packs/dev",
    })


def write_room_pack(width, height, room_y, base_raw, base_compressed,
                    patch_runs, patch_pixels, walk_polygon, food_x, food_y,
                    bed_polygon, bed_x, bed_y, doorway_polygon,
                    doorway_inside, doorway_outside, behavior_anchors):
    min_x, min_y, max_x, max_y = bounds_for(walk_polygon, width, height)
    bed_min_x, bed_min_y, bed_max_x, bed_max_y = bounds_for(bed_polygon, width, height)
    door_min_x, door_min_y, door_max_x, door_max_y = bounds_for(doorway_polygon, width, height)
    if not 3 <= len(walk_polygon) <= 32 or not 3 <= len(bed_polygon) <= 32:
        raise ValueError("room polygons must contain between 3 and 32 points")
    if doorway_polygon and not 3 <= len(doorway_polygon) <= 32:
        raise ValueError("doorway polygon must contain between 3 and 32 points")
    if len(behavior_anchors) > 8:
        raise ValueError("room may contain at most 8 behavior anchors")

    payload = bytearray()
    payload.extend(struct.pack(
        ROOM_PACK_HEADER_FORMAT,
        ROOM_PACK_MAGIC,
        ROOM_PACK_VERSION,
        width,
        height,
        room_y,
        len(base_raw),
        len(base_compressed),
        len(patch_runs),
        len(patch_pixels),
        len(walk_polygon),
        len(bed_polygon),
        len(doorway_polygon),
        len(behavior_anchors),
        min_x,
        min_y,
        max_x,
        max_y,
        food_x,
        food_y,
        bed_min_x,
        bed_min_y,
        bed_max_x,
        bed_max_y,
        bed_x,
        bed_y,
        door_min_x,
        door_min_y,
        door_max_x,
        door_max_y,
        doorway_inside[0],
        doorway_inside[1],
        doorway_outside[0],
        doorway_outside[1],
        1 if not patch_runs else 0,
        0,
    ))
    payload.extend(base_compressed)
    for y, x, length, offset in patch_runs:
        payload.extend(struct.pack("<HHHI", y, x, length, offset))
    for value in patch_pixels:
        payload.extend(struct.pack("<H", value))
    for x, y in walk_polygon:
        payload.extend(struct.pack("<hh", x, y))
    for x, y in bed_polygon:
        payload.extend(struct.pack("<hh", x, y))
    for x, y in doorway_polygon:
        payload.extend(struct.pack("<hh", x, y))
    for anchor_type, facing, foot_x, foot_y in behavior_anchors:
        payload.extend(struct.pack(
            ROOM_BEHAVIOR_ANCHOR_FORMAT,
            anchor_type,
            facing,
            foot_x,
            foot_y,
        ))

    PACK_ROOM_OUT.mkdir(parents=True, exist_ok=True)
    (PACK_ROOM_OUT / "standard.smonroom").write_bytes(payload)
    merge_pack_manifest(room="rooms/standard.smonroom", rooms="rooms", roomCount=1)
    return len(payload)


def write_room_assets(day_img, night_img, walk_polygon, food_x, food_y,
                      bed_polygon, bed_x, bed_y, doorway_polygon,
                      doorway_inside, doorway_outside, behavior_anchors):
    if day_img.size != night_img.size:
        raise ValueError(f"day/night room sizes differ: {day_img.size} vs {night_img.size}")

    width, height = day_img.size
    room_y = max(0, (DISPLAY_H - height) // 2)
    base_raw = rgb565_bytes(day_img)
    base_compressed = raw_deflate(base_raw)
    patch_runs, patch_pixels = night_patch_runs(day_img, night_img)
    shared_rle = len(patch_runs) == 0
    min_x, min_y, max_x, max_y = bounds_for(walk_polygon, width, height)
    polygon_count = len(walk_polygon)
    bed_min_x, bed_min_y, bed_max_x, bed_max_y = bounds_for(bed_polygon, width, height)
    bed_polygon_count = len(bed_polygon)
    patch_pixel_words = format_words(patch_pixels or [0])

    HEADER_OUT.write_text("""#pragma once
#include "platform/api/FlashStorage.h"
#include <cstdint>

namespace RoomAssets {

struct RoomPoint {
    int16_t x;
    int16_t y;
};

struct RoomPatchRun {
    uint16_t y;
    uint16_t x;
    uint16_t len;
    uint32_t colorOffset;
};

}
""", encoding="utf-8")

    CPP_OUT.write_text("""#include "RoomAssets.h"

namespace RoomAssets {
}
""", encoding="utf-8")

    room_pack_bytes = write_room_pack(
        width, height, room_y, base_raw, base_compressed,
        patch_runs, patch_pixels, walk_polygon, food_x, food_y,
        bed_polygon, bed_x, bed_y, doorway_polygon,
        doorway_inside, doorway_outside, behavior_anchors)

    return len(base_raw), len(base_compressed), len(patch_runs), len(patch_pixels), room_y, shared_rle, room_pack_bytes


def main():
    parser = argparse.ArgumentParser(description="Compose and install StickMon room assets from room_layout.json.")
    parser.add_argument("source", nargs="?", default=None, help="Legacy day background override")
    parser.add_argument("--layout", default=str(DEFAULT_LAYOUT), help="room_layout.json source of truth")
    parser.add_argument("--day-base", help="Day background image. Defaults to backgrounds.day.fileName")
    parser.add_argument("--night-base", help="Night background image. Defaults to backgrounds.night.fileName")
    parser.add_argument("--direct-room-image", help="Already composed room image used for both day/night; bypasses furniture composition")
    parser.add_argument("--direct-day-image", help="Already composed day room image; bypasses furniture composition for day")
    parser.add_argument("--direct-night-image", help="Already composed night room image; bypasses furniture composition for night")
    parser.add_argument("--furniture-dir", default=str(DEFAULT_FURNITURE_DIR), help="Directory containing furniture PNG files")
    parser.add_argument("--day-source-out", default=str(DAY_SOURCE_OUT), help="Copy of the day source background")
    parser.add_argument("--night-source-out", default=str(NIGHT_SOURCE_OUT), help="Copy of the night source background")
    parser.add_argument("--day-png-out", default=str(DAY_PNG_OUT), help="Composed day room PNG")
    parser.add_argument("--night-png-out", default=str(NIGHT_PNG_OUT), help="Composed night room PNG")
    parser.add_argument("--png-out", default=str(LEGACY_PNG_OUT), help="Legacy composed day PNG copy")
    args = parser.parse_args()

    layout_path = Path(args.layout)
    layout = read_layout(layout_path)
    direct_day_value = args.direct_day_image or args.direct_room_image
    direct_night_value = args.direct_night_image or args.direct_room_image
    direct_day = Path(direct_day_value) if direct_day_value else None
    direct_night = Path(direct_night_value) if direct_night_value else None
    day_base = direct_day or background_path(layout_path, layout, "day", args.day_base or args.source)
    night_base = direct_night or background_path(layout_path, layout, "night", args.night_base, day_base)
    furniture_dir = Path(args.furniture_dir)

    day_source_out = Path(args.day_source_out)
    night_source_out = Path(args.night_source_out)
    day_png_out = Path(args.day_png_out)
    night_png_out = Path(args.night_png_out)
    legacy_png_out = Path(args.png_out)
    for path in (day_source_out, night_source_out, day_png_out, night_png_out, legacy_png_out):
        path.parent.mkdir(parents=True, exist_ok=True)

    if direct_day:
        Image.open(day_base).save(day_source_out)
    elif background_layers_for_mode(layout, "day"):
        render_base(layout, str(day_base), "day", layout_path.parent).convert("RGB").save(day_source_out)
    else:
        Image.open(day_base).save(day_source_out)
    if direct_night:
        Image.open(night_base).save(night_source_out)
    elif background_layers_for_mode(layout, "night"):
        render_base(layout, str(night_base), "night", layout_path.parent).convert("RGB").save(night_source_out)
    else:
        Image.open(night_base).save(night_source_out)

    day_img = (
        direct_room_image(layout, direct_day, "day")
        if direct_day else compose_room_image(layout, day_base, furniture_dir, "day", layout_path.parent)
    )
    night_img = (
        direct_room_image(layout, direct_night, "night")
        if direct_night else compose_room_image(layout, night_base, furniture_dir, "night", layout_path.parent)
    )
    day_img.save(day_png_out)
    night_img.save(night_png_out)
    day_img.save(legacy_png_out)

    width, height = room_size(layout)
    walk_polygon = load_walk_polygon(layout, width, height)
    food_x, food_y = food_position(layout)
    bed_polygon, bed_x, bed_y = bed_region(layout, width, height, walk_polygon)
    doorway_polygon, doorway_inside, doorway_outside = doorway_region(
        layout, width, height, walk_polygon
    )
    behavior_anchors = behavior_anchors_for_layout(layout, width, height, walk_polygon)
    base_raw_bytes, base_compressed_len, patch_run_count, patch_pixel_count, room_y, shared_rle, room_pack_bytes = write_room_assets(
        day_img, night_img, walk_polygon, food_x, food_y,
        bed_polygon, bed_x, bed_y, doorway_polygon,
        doorway_inside, doorway_outside, behavior_anchors
    )

    print(f"layout={layout_path}")
    print(f"day_base={day_base}")
    print(f"night_base={night_base}")
    if direct_day or direct_night:
        print(f"direct_day_image={direct_day or '-'}")
        print(f"direct_night_image={direct_night or '-'}")
    print(f"furniture_dir={furniture_dir}")
    print(f"day_png={day_png_out} size={day_img.width}x{day_img.height}")
    print(f"night_png={night_png_out} size={night_img.width}x{night_img.height}")
    print(f"legacy_png={legacy_png_out}")
    print(f"room_y={room_y}")
    print(f"walk_polygon_points={len(walk_polygon)}")
    print(f"food={food_x},{food_y}")
    print(f"bed={bed_x},{bed_y} bed_polygon_points={len(bed_polygon)}")
    print(
        f"doorway_inside={doorway_inside[0]},{doorway_inside[1]} "
        f"doorway_outside={doorway_outside[0]},{doorway_outside[1]} "
        f"doorway_polygon_points={len(doorway_polygon)}"
    )
    print(f"behavior_anchors={behavior_anchors}")
    print(
        f"assets={HEADER_OUT}, {CPP_OUT} base_raw_bytes={base_raw_bytes} "
        f"base_compressed_bytes={base_compressed_len} "
        f"night_patch_runs={patch_run_count} night_patch_pixels={patch_pixel_count} "
        f"shared_rle={str(shared_rle).lower()} room_pack_bytes={room_pack_bytes}"
    )


if __name__ == "__main__":
    main()
