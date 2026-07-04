#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import zlib
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ASSET_H = ROOT / "src" / "assets" / "PokemonSprites.h"
ASSET_CPP = ROOT / "src" / "assets" / "PokemonSprites.cpp"
GENERATED = ROOT / "origin_asset" / "generated"
OUT_DIR = GENERATED / "pokemon_sprites"
MANIFEST = GENERATED / "pokemon_preview_assets.js"


def numbers(body: str) -> list[int]:
    return [int(value, 0) for value in re.findall(r"0x[0-9A-Fa-f]+|\b\d+\b", body)]


def extract_array(text: str, name: str) -> str:
    pattern = re.compile(rf"{re.escape(name)}\s*\[\]\s+PROGMEM\s*=\s*\{{(.*?)\n\}};", re.S)
    match = pattern.search(text)
    if not match:
        raise RuntimeError(f"array not found: {name}")
    return match.group(1)


def rgb565_to_rgba(color: int) -> tuple[int, int, int, int]:
    r = ((color >> 11) & 0x1F) * 255 // 31
    g = ((color >> 5) & 0x3F) * 255 // 63
    b = (color & 0x1F) * 255 // 31
    return r, g, b, 255


def decode_rgb565_rle(words: list[int], offset: int, length: int, width: int, height: int) -> list[tuple[int, int, int, int]]:
    pixels = [(0, 0, 0, 0)] * (width * height)
    idx = offset
    end = offset + length
    pixel = 0
    while idx < end and pixel < len(pixels):
        token = words[idx]
        idx += 1
        run = token & 0x7FFF
        if token & 0x8000:
            pixel += run
            continue
        for _ in range(run):
            if idx >= end or pixel >= len(pixels):
                break
            pixels[pixel] = rgb565_to_rgba(words[idx])
            idx += 1
            pixel += 1
    return pixels


def decode_indexed4_rle(
    words: list[int],
    palettes: list[int],
    offset: int,
    length: int,
    palette_offset: int,
    palette_size: int,
    width: int,
    height: int,
) -> list[tuple[int, int, int, int]]:
    palette = palettes[palette_offset:palette_offset + palette_size]
    pixels = [(0, 0, 0, 0)] * (width * height)
    idx = offset
    end = offset + length
    pixel = 0
    while idx < end and pixel < len(pixels):
        token = words[idx]
        idx += 1
        run = token & 0x7FFF
        if token & 0x8000:
            pixel += run
            continue
        packed = 0
        for i in range(run):
            if i & 0x03 == 0:
                if idx >= end:
                    break
                packed = words[idx]
                idx += 1
            if pixel >= len(pixels):
                break
            palette_index = (packed >> ((i & 0x03) * 4)) & 0x0F
            if palette_index < len(palette):
                pixels[pixel] = rgb565_to_rgba(palette[palette_index])
            pixel += 1
    return pixels


def parse_kind_names(header: str) -> list[str]:
    body = re.search(r"enum class SpriteKind\s*:\s*uint16_t\s*\{(.*?)\n\};", header, re.S).group(1)
    names = []
    for line in body.splitlines():
        line = line.split("//", 1)[0].strip().rstrip(",")
        if line:
            names.append(line)
    return names


def parse_frames(source: str, kind_names: list[str]) -> list[dict]:
    body = extract_array(source, "SPRITE_FRAMES")
    pattern = re.compile(
        r"\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\},\s*//\s*(.*)"
    )
    frames = []
    for match in pattern.finditer(body):
        species_id, kind, width, height, fmt, palette_size, source_id, _reserved, offset, length, palette_offset = [
            int(match.group(i)) for i in range(1, 12)
        ]
        kind_name = kind_names[kind] if kind < len(kind_names) else f"KIND_{kind}"
        comment = match.group(12).strip()
        frames.append({
            "speciesId": species_id,
            "kind": kind,
            "kindName": kind_name,
            "speciesName": comment.split()[0].title() if comment else f"Species {species_id}",
            "width": width,
            "height": height,
            "format": fmt,
            "paletteSize": palette_size,
            "source": source_id,
            "offset": offset,
            "length": length,
            "paletteOffset": palette_offset,
        })
    return frames


def parse_blocks(source: str) -> dict[int, dict]:
    body = extract_array(source, "SPRITE_COMPRESSED_BLOCKS")
    pattern = re.compile(r"\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}")
    blocks = {}
    for species_id, rle_words, palette_words, _reserved, offset, length in pattern.findall(body):
        blocks[int(species_id)] = {
            "rleWords": int(rle_words),
            "paletteWords": int(palette_words),
            "offset": int(offset),
            "length": int(length),
        }
    return blocks


def decode_frame(frame: dict, rle: list[int], palettes: list[int]) -> Image.Image:
    if frame["format"] == 1:
        pixels = decode_indexed4_rle(
            rle,
            palettes,
            frame["offset"],
            frame["length"],
            frame["paletteOffset"],
            frame["paletteSize"],
            frame["width"],
            frame["height"],
        )
    else:
        pixels = decode_rgb565_rle(rle, frame["offset"], frame["length"], frame["width"], frame["height"])
    image = Image.new("RGBA", (frame["width"], frame["height"]))
    image.putdata(pixels)
    return image


def choose_preview_frame(frames: list[dict]) -> dict:
    preferred_suffixes = ("_IDLE_FRONT_0", "_WALKING_FRONT_0")
    for suffix in preferred_suffixes:
        match = next((frame for frame in frames if frame["kindName"].endswith(suffix)), None)
        if match:
            return match
    return next((frame for frame in frames if frame["kindName"] == "FRONT"), frames[0])


def frame_action(kind_name: str) -> str | None:
    match = re.search(r"_(IDLE|WALKING|SLEEPING)(?:_|$)", kind_name)
    if match:
        return match.group(1).lower()
    return None


def slugify(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


def main() -> None:
    header = ASSET_H.read_text(encoding="utf-8")
    source = ASSET_CPP.read_text(encoding="utf-8")
    kind_names = parse_kind_names(header)
    frames = parse_frames(source, kind_names)
    global_rle = numbers(extract_array(source, "SPRITE_RLE"))
    global_palettes = numbers(extract_array(source, "SPRITE_PALETTES"))
    compressed_data = bytes(numbers(extract_array(source, "SPRITE_COMPRESSED_DATA")))
    blocks = parse_blocks(source)

    decoded_blocks: dict[int, tuple[list[int], list[int]]] = {}
    for species_id, block in blocks.items():
        compressed = compressed_data[block["offset"]:block["offset"] + block["length"]]
        decoded = zlib.decompress(compressed, -15)
        words = [decoded[i] | (decoded[i + 1] << 8) for i in range(0, len(decoded), 2)]
        rle_words = words[:block["rleWords"]]
        palette_words = words[block["rleWords"]:block["rleWords"] + block["paletteWords"]]
        decoded_blocks[species_id] = (rle_words, palette_words)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    by_species: dict[int, list[dict]] = {}
    for frame in frames:
        by_species.setdefault(frame["speciesId"], []).append(frame)

    manifest = []
    for species_id in sorted(by_species):
        species_frames = by_species[species_id]
        preview_frame = choose_preview_frame(species_frames)
        slug = slugify(preview_frame["speciesName"])

        if preview_frame["source"] == 1:
            rle, palettes = decoded_blocks[species_id]
        else:
            rle, palettes = global_rle, global_palettes

        actions: dict[str, list[dict]] = {}
        for frame in species_frames:
            action = frame_action(frame["kindName"])
            if action is None:
                continue
            file_name = f"{species_id:03d}_{slug}_{frame['kindName']}.png"
            file_path = OUT_DIR / file_name
            try:
                if frame["source"] == 1:
                    frame_rle, frame_palettes = decoded_blocks[species_id]
                else:
                    frame_rle, frame_palettes = global_rle, global_palettes
                image = decode_frame(frame, frame_rle, frame_palettes)
                image.save(file_path)
            except Exception as error:
                print(f"warn: failed to decode {frame['kindName']} for species {species_id}: {error}")
                continue
            actions.setdefault(action, []).append({
                "file": file_name,
                "path": f"../../origin_asset/generated/pokemon_sprites/{file_name}",
                "width": frame["width"],
                "height": frame["height"],
                "kindName": frame["kindName"],
            })

        preview_file_name = f"{species_id:03d}_{slug}_{preview_frame['kindName']}.png"
        manifest.append({
            "speciesId": species_id,
            "name": f"{species_id:03d} {preview_frame['speciesName']}",
            "slug": slug,
            "frame": preview_frame["kindName"],
            "width": preview_frame["width"],
            "height": preview_frame["height"],
            "path": f"../../origin_asset/generated/pokemon_sprites/{preview_file_name}",
            "actions": actions,
        })

    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(
        "window.STICKMON_PROJECT_SPRITES = "
        + json.dumps(manifest, ensure_ascii=False, indent=2)
        + ";\n",
        encoding="utf-8",
    )
    print(f"wrote {len(manifest)} species previews to {OUT_DIR}")
    print(f"wrote manifest {MANIFEST}")


if __name__ == "__main__":
    main()
