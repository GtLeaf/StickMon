#!/usr/bin/env python3
"""Extract square status icons from the Pokemon Sword/Shield UI sheet."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = (
    ROOT
    / "origin_asset"
    / "icon"
    / "status"
    / "Nintendo Switch - Pokemon Sword _ Shield - UI Elements - Status Effects.png"
)
DEFAULT_OUTPUT = ROOT / "origin_asset" / "icon" / "status"
OUTPUT_SIZE = 44


@dataclass(frozen=True)
class StatusIconSpec:
    name: str
    tile_x: int
    tile_y: int
    tile_height: int
    icon_region_width: int


# The English row has the cleanest spacing. icon_region_width stops before text.
STATUS_ICONS = (
    StatusIconSpec("poison", 10, 34, 43, 41),
    StatusIconSpec("toxic", 196, 34, 43, 41),
    StatusIconSpec("fainted", 382, 34, 43, 44),
    StatusIconSpec("freeze", 568, 34, 43, 44),
    StatusIconSpec("paralysis", 10, 87, 44, 34),
    StatusIconSpec("sleep", 196, 87, 44, 44),
    StatusIconSpec("burn", 382, 87, 44, 39),
)


def color_distance(left: tuple[int, ...], right: tuple[int, ...]) -> int:
    return sum(abs(left[index] - right[index]) for index in range(3))


def extract_icon(sheet: Image.Image, spec: StatusIconSpec) -> Image.Image:
    tile = sheet.crop(
        (
            spec.tile_x,
            spec.tile_y,
            spec.tile_x + spec.icon_region_width,
            spec.tile_y + spec.tile_height,
        )
    )
    backgrounds = [tile.getpixel((0, y)) for y in range(spec.tile_height)]

    foreground = []
    for y in range(spec.tile_height):
        background = backgrounds[y]
        for x in range(spec.icon_region_width):
            pixel = tile.getpixel((x, y))
            if color_distance(pixel, background) > 2:
                foreground.append((x, y, pixel))

    if not foreground:
        raise RuntimeError(f"No icon pixels found for {spec.name}")

    min_x = min(pixel[0] for pixel in foreground)
    max_x = max(pixel[0] for pixel in foreground)
    min_y = min(pixel[1] for pixel in foreground)
    max_y = max(pixel[1] for pixel in foreground)
    icon_width = max_x - min_x + 1
    icon_height = max_y - min_y + 1
    if icon_width > OUTPUT_SIZE or icon_height > OUTPUT_SIZE:
        raise RuntimeError(f"{spec.name} icon exceeds {OUTPUT_SIZE}x{OUTPUT_SIZE}")

    output = Image.new("RGBA", (OUTPUT_SIZE, OUTPUT_SIZE))
    for y in range(OUTPUT_SIZE):
        background = backgrounds[min(y, spec.tile_height - 1)]
        for x in range(OUTPUT_SIZE):
            output.putpixel((x, y), background)

    offset_x = (OUTPUT_SIZE - icon_width) // 2 - min_x
    offset_y = (OUTPUT_SIZE - icon_height) // 2 - min_y
    for x, y, pixel in foreground:
        output.putpixel((x + offset_x, y + offset_y), pixel)

    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    sheet = Image.open(args.source).convert("RGBA")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for spec in STATUS_ICONS:
        output_path = args.output_dir / f"status_{spec.name}.png"
        extract_icon(sheet, spec).save(output_path, optimize=True)
        print(f"wrote {output_path.relative_to(ROOT)} ({OUTPUT_SIZE}x{OUTPUT_SIZE})")


if __name__ == "__main__":
    main()
