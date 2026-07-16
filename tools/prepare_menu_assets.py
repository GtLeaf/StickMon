#!/usr/bin/env python3
"""Build the 40x40 main-menu icon sheet and RGB565 RLE assets."""

from __future__ import annotations

import argparse
import colorsys
import re
from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageChops


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT / "origin_asset/icon/menu/main_menu.png"
DEFAULT_SOURCE_DIR = ROOT / "origin_asset/icon/menu/main"
DEFAULT_DEBUG_SOURCE = ROOT / "origin_asset/icon/menu/debug.png"
DEFAULT_SHEET = ROOT / "origin_asset/generated/menu/main_menu_40.png"
DEFAULT_PREVIEW = ROOT / "origin_asset/generated/menu/main_menu_40_preview.png"
DEFAULT_CPP = ROOT / "src/assets/MenuAssets.cpp"
DEFAULT_HEADER = ROOT / "src/assets/MenuAssets.h"

ICON_COUNT = 9
SOURCE_ICON_COUNT = 8
ICON_SIZE = 40
CONTENT_SIZE = 36
SOURCE_ALPHA_THRESHOLD = 16
OUTPUT_ALPHA_THRESHOLD = 128
MIN_COMPONENT_PIXELS = 1000
PALETTE_COLORS = 24
DEBUG_ICON_INDEX = 7
BACK_ICON_INDEX = 8
CHECKER_MIN_CHANNEL = 225
CHECKER_MAX_CHROMA = 18


def brighten_color(color: tuple[int, int, int]) -> tuple[int, int, int]:
    red, green, blue = (channel / 255 for channel in color)
    hue, saturation, value = colorsys.rgb_to_hsv(red, green, blue)

    # Keep the dark outline intact while making fills and highlights feel lighter.
    if value < 0.23:
        value = min(0.25, value * 1.05)
    else:
        value = min(1.0, value * 1.12 + 0.04)
        if saturation > 0.12:
            saturation = min(1.0, saturation * 1.08 + 0.02)

    return tuple(round(channel * 255) for channel in colorsys.hsv_to_rgb(hue, saturation, value))


@dataclass
class Component:
    points: list[tuple[int, int]]
    bbox: tuple[int, int, int, int]


def find_components(image: Image.Image, expected_count: int) -> list[Component]:
    alpha = image.getchannel("A")
    alpha_bbox = alpha.getbbox()
    if alpha_bbox is None:
        raise ValueError("source image has no visible pixels")

    origin_x, origin_y, right, bottom = alpha_bbox
    cropped = alpha.crop(alpha_bbox)
    width, height = cropped.size
    active = bytearray(1 if value > SOURCE_ALPHA_THRESHOLD else 0 for value in cropped.tobytes())
    components: list[Component] = []

    for start in range(width * height):
        if not active[start]:
            continue

        active[start] = 0
        stack = [start]
        points: list[tuple[int, int]] = []
        min_x = width
        min_y = height
        max_x = 0
        max_y = 0

        while stack:
            index = stack.pop()
            y, x = divmod(index, width)
            points.append((x + origin_x, y + origin_y))
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)

            for next_y in range(max(0, y - 1), min(height, y + 2)):
                row = next_y * width
                for next_x in range(max(0, x - 1), min(width, x + 2)):
                    next_index = row + next_x
                    if active[next_index]:
                        active[next_index] = 0
                        stack.append(next_index)

        if len(points) < MIN_COMPONENT_PIXELS:
            continue

        components.append(
            Component(
                points=points,
                bbox=(
                    min_x + origin_x,
                    min_y + origin_y,
                    max_x + origin_x + 1,
                    max_y + origin_y + 1,
                ),
            )
        )

    components.sort(key=lambda component: component.bbox[0])
    if len(components) != expected_count:
        raise ValueError(f"expected {expected_count} icon components, found {len(components)}")
    return components


def resize_premultiplied(
    image: Image.Image, size: tuple[int, int], brighten: bool = True
) -> Image.Image:
    red, green, blue, alpha = image.split()
    premultiplied = [ImageChops.multiply(channel, alpha) for channel in (red, green, blue)]
    resized_rgb = [
        channel.resize(size, Image.Resampling.LANCZOS) for channel in premultiplied
    ]
    resized_alpha = alpha.resize(size, Image.Resampling.LANCZOS)

    alpha_values = list(resized_alpha.get_flattened_data())
    channel_values = [list(channel.get_flattened_data()) for channel in resized_rgb]
    rgb_values: list[tuple[int, int, int]] = []
    hard_alpha: list[int] = []

    for index, alpha_value in enumerate(alpha_values):
        if alpha_value < OUTPUT_ALPHA_THRESHOLD:
            rgb_values.append((0, 0, 0))
            hard_alpha.append(0)
            continue

        color = tuple(
            min(255, round(channel[index] * 255 / alpha_value))
            for channel in channel_values
        )
        rgb_values.append(brighten_color(color) if brighten else color)
        hard_alpha.append(255)

    rgb = Image.new("RGB", size)
    rgb.putdata(rgb_values)
    rgb = rgb.quantize(
        colors=PALETTE_COLORS,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    ).convert("RGB")
    output_alpha = Image.new("L", size)
    output_alpha.putdata(hard_alpha)
    rgb.putalpha(output_alpha)
    return rgb


def render_icon(
    source: Image.Image, component: Component, flip_horizontal: bool = False
) -> Image.Image:
    left, top, right, bottom = component.bbox
    crop = source.crop(component.bbox)
    component_mask = Image.new("L", crop.size, 0)
    mask_pixels = component_mask.load()
    for x, y in component.points:
        mask_pixels[x - left, y - top] = 255
    crop.putalpha(ImageChops.multiply(crop.getchannel("A"), component_mask))

    if flip_horizontal:
        crop = crop.transpose(Image.Transpose.FLIP_LEFT_RIGHT)

    width, height = crop.size
    scale = CONTENT_SIZE / max(width, height)
    target_size = (max(1, round(width * scale)), max(1, round(height * scale)))
    resized = resize_premultiplied(crop, target_size)

    icon = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
    icon.alpha_composite(
        resized,
        ((ICON_SIZE - target_size[0]) // 2, (ICON_SIZE - target_size[1]) // 2),
    )
    return icon


def render_debug_icon(debug_source_path: Path) -> Image.Image:
    debug = Image.open(debug_source_path).convert("RGBA")
    if debug.getchannel("A").getbbox() is None:
        raise ValueError(f"debug icon has no visible pixels: {debug_source_path}")
    return debug.resize((ICON_SIZE, ICON_SIZE), Image.Resampling.NEAREST)


def standalone_icon_paths(source_dir: Path) -> list[Path]:
    paths = list(source_dir.glob("*.png"))
    indexed: dict[int, Path] = {}
    for path in paths:
        match = re.search(r"\((\d+)\)\.png$", path.name)
        if not match:
            continue
        index = int(match.group(1))
        if index in indexed:
            raise ValueError(f"duplicate menu icon index {index}: {path.name}")
        indexed[index] = path

    expected = list(range(1, ICON_COUNT + 1))
    if sorted(indexed) != expected:
        raise ValueError(
            f"expected standalone menu icon indices {expected}, found {sorted(indexed)}"
        )
    return [indexed[index] for index in expected]


def remove_baked_checkerboard(image: Image.Image) -> Image.Image:
    source = image.convert("RGBA")
    alpha_values: list[int] = []
    for red, green, blue, source_alpha in source.get_flattened_data():
        chroma = max(red, green, blue) - min(red, green, blue)
        baked_background = (
            min(red, green, blue) >= CHECKER_MIN_CHANNEL and
            chroma <= CHECKER_MAX_CHROMA
        )
        alpha_values.append(0 if baked_background else source_alpha)

    alpha = Image.new("L", source.size)
    alpha.putdata(alpha_values)
    source.putalpha(alpha)
    return source


def render_standalone_icon(source_path: Path) -> Image.Image:
    source = remove_baked_checkerboard(Image.open(source_path))
    bbox = source.getchannel("A").getbbox()
    if bbox is None:
        raise ValueError(f"standalone icon has no visible pixels: {source_path}")
    crop = source.crop(bbox)
    width, height = crop.size
    scale = CONTENT_SIZE / max(width, height)
    target_size = (max(1, round(width * scale)), max(1, round(height * scale)))
    resized = resize_premultiplied(crop, target_size, brighten=False)

    icon = Image.new("RGBA", (ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
    icon.alpha_composite(
        resized,
        ((ICON_SIZE - target_size[0]) // 2, (ICON_SIZE - target_size[1]) // 2),
    )
    return icon


def build_sheet_from_directory(source_dir: Path) -> Image.Image:
    sheet = Image.new("RGBA", (ICON_COUNT * ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
    for index, source_path in enumerate(standalone_icon_paths(source_dir)):
        sheet.alpha_composite(render_standalone_icon(source_path), (index * ICON_SIZE, 0))
    return sheet


def build_sheet(source_path: Path, debug_source_path: Path) -> Image.Image:
    source = Image.open(source_path).convert("RGBA")
    components = find_components(source, SOURCE_ICON_COUNT)
    sheet = Image.new("RGBA", (ICON_COUNT * ICON_SIZE, ICON_SIZE), (0, 0, 0, 0))
    for index, component in enumerate(components[:DEBUG_ICON_INDEX]):
        sheet.alpha_composite(render_icon(source, component), (index * ICON_SIZE, 0))
    sheet.alpha_composite(render_debug_icon(debug_source_path), (DEBUG_ICON_INDEX * ICON_SIZE, 0))
    sheet.alpha_composite(
        render_icon(source, components[-1], flip_horizontal=True),
        (BACK_ICON_INDEX * ICON_SIZE, 0),
    )
    return sheet


def rgb565(red: int, green: int, blue: int) -> int:
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def encode_frame(frame: Image.Image) -> list[int]:
    pixels = list(frame.convert("RGBA").get_flattened_data())
    words: list[int] = []
    index = 0
    while index < len(pixels):
        transparent = pixels[index][3] < OUTPUT_ALPHA_THRESHOLD
        end = index + 1
        while end < len(pixels) and (pixels[end][3] < OUTPUT_ALPHA_THRESHOLD) == transparent:
            end += 1

        run = end - index
        if transparent:
            words.append(0x8000 | run)
        else:
            words.append(run)
            words.extend(rgb565(*pixel[:3]) for pixel in pixels[index:end])
        index = end
    return words


def format_words(words: list[int]) -> str:
    lines = []
    for index in range(0, len(words), 12):
        values = ", ".join(f"0x{word:04X}" for word in words[index : index + 12])
        lines.append(f"    {values},")
    return "\n".join(lines)


def update_cpp(cpp_path: Path, sheet: Image.Image) -> tuple[int, list[int]]:
    frames: list[list[int]] = []
    for index in range(ICON_COUNT):
        frames.append(encode_frame(sheet.crop((index * ICON_SIZE, 0, (index + 1) * ICON_SIZE, ICON_SIZE))))

    offsets: list[int] = []
    words: list[int] = []
    for frame in frames:
        offsets.append(len(words))
        words.extend(frame)

    frame_lines = "\n".join(
        f"    {{ {offset}, {len(frame)} }}," for offset, frame in zip(offsets, frames)
    )
    generated = (
        "const RleFrame MAIN_ICON_FRAMES[] PROGMEM = {\n"
        f"{frame_lines}\n"
        "};\n\n"
        "const uint16_t MAIN_ICON_RLE[] PROGMEM = {\n"
        f"{format_words(words)}\n"
        "};\n\n"
    )

    existing = cpp_path.read_text(encoding="utf-8")
    start = existing.index("const RleFrame MAIN_ICON_FRAMES[] PROGMEM = {")
    end = existing.index("const RleFrame BOX_ICON_FRAMES[] PROGMEM = {")
    cpp_path.write_text(existing[:start] + generated + existing[end:], encoding="utf-8")
    return len(words), [len(frame) for frame in frames]


def update_header(header_path: Path) -> None:
    existing = header_path.read_text(encoding="utf-8")
    replacements = {
        r"static constexpr uint8_t FRAME_W = \d+;": f"static constexpr uint8_t FRAME_W = {ICON_SIZE};",
        r"static constexpr uint8_t FRAME_H = \d+;": f"static constexpr uint8_t FRAME_H = {ICON_SIZE};",
        r"static constexpr uint8_t MAIN_ICON_COUNT = \d+;": (
            f"static constexpr uint8_t MAIN_ICON_COUNT = {ICON_COUNT};"
        ),
    }
    updated = existing
    for pattern, replacement in replacements.items():
        updated, count = re.subn(pattern, replacement, updated, count=1)
        if count != 1:
            raise ValueError(f"could not update header field matching {pattern}")
    header_path.write_text(updated, encoding="utf-8")


def save_preview(sheet: Image.Image, preview_path: Path) -> None:
    preview = Image.new("RGB", sheet.size, (7, 9, 14))
    preview.paste(sheet, (0, 0), sheet)
    preview.resize(
        (preview.width * 4, preview.height * 4), Image.Resampling.NEAREST
    ).save(preview_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        help=(
            f"Legacy horizontal icon source, for example {DEFAULT_SOURCE}; "
            f"when omitted, uses {DEFAULT_SOURCE_DIR}"
        ),
    )
    parser.add_argument("--source-dir", type=Path, default=DEFAULT_SOURCE_DIR)
    parser.add_argument("--debug-source", type=Path, default=DEFAULT_DEBUG_SOURCE)
    parser.add_argument("--sheet", type=Path, default=DEFAULT_SHEET)
    parser.add_argument("--preview", type=Path, default=DEFAULT_PREVIEW)
    parser.add_argument("--cpp", type=Path, default=DEFAULT_CPP)
    parser.add_argument("--header", type=Path, default=DEFAULT_HEADER)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    sheet = (
        build_sheet(args.source, args.debug_source)
        if args.source
        else build_sheet_from_directory(args.source_dir)
    )
    args.sheet.parent.mkdir(parents=True, exist_ok=True)
    args.preview.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.sheet)
    save_preview(sheet, args.preview)
    word_count, frame_lengths = update_cpp(args.cpp, sheet)
    update_header(args.header)
    print(f"generated {args.sheet} ({sheet.width}x{sheet.height})")
    print(f"generated {args.preview}")
    print(f"updated {args.cpp}: {word_count} words, frames={frame_lengths}")
    print(
        f"updated {args.header}: FRAME={ICON_SIZE}x{ICON_SIZE} "
        f"MAIN_ICON_COUNT={ICON_COUNT}"
    )


if __name__ == "__main__":
    main()
