#!/usr/bin/env python3
from __future__ import annotations

import base64
import json
import re
import struct
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
TOOL_DIR = Path(__file__).resolve().parent
ASSET_H = ROOT / "src" / "assets" / "PokemonSprites.h"
SPECIES_CPP = ROOT / "src" / "game" / "Species.cpp"
PACK_DIR = ROOT / "data" / "packs" / "dev" / "sprites"
GENERATED = TOOL_DIR / "generated"
OUT_DIR = GENERATED / "pokemon_sprites"
MANIFEST = GENERATED / "pokemon_preview_assets.js"

PACK_MAGIC = 0x5350534D
PACK_VERSION = 1
PACK_HEADER = struct.Struct("<IHHHHHH")
PACK_FRAME = struct.Struct("<HBBBBHIII")


def png_data_url(path: Path) -> str:
    encoded = base64.b64encode(path.read_bytes()).decode("ascii")
    return f"data:image/png;base64,{encoded}"


def rgb565_to_rgba(color: int) -> tuple[int, int, int, int]:
    r = ((color >> 11) & 0x1F) * 255 // 31
    g = ((color >> 5) & 0x3F) * 255 // 63
    b = (color & 0x1F) * 255 // 31
    return r, g, b, 255


def decode_rgb565_rle(
    words: list[int], offset: int, length: int, width: int, height: int
) -> list[tuple[int, int, int, int]]:
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
        for index in range(run):
            if index & 0x03 == 0:
                if idx >= end:
                    break
                packed = words[idx]
                idx += 1
            if pixel >= len(pixels):
                break
            palette_index = (packed >> ((index & 0x03) * 4)) & 0x0F
            if palette_index < len(palette):
                pixels[pixel] = rgb565_to_rgba(palette[palette_index])
            pixel += 1
    return pixels


def parse_kind_names(header: str) -> list[str]:
    match = re.search(r"enum class SpriteKind\s*:\s*uint16_t\s*\{(.*?)\n\};", header, re.S)
    if not match:
        raise RuntimeError("SpriteKind enum not found")
    names = []
    for line in match.group(1).splitlines():
        name = line.split("//", 1)[0].strip().rstrip(",")
        if name:
            names.append(name)
    return names


def parse_species_idents(source: str) -> dict[int, str]:
    return {
        int(match.group(1)): match.group(2)
        for match in re.finditer(r"\{(\d+),\s*Ui::SpeciesName::(\w+),", source)
    }


def words_from_bytes(payload: bytes) -> list[int]:
    if len(payload) % 2:
        raise RuntimeError("sprite payload has an odd byte count")
    return list(struct.unpack(f"<{len(payload) // 2}H", payload))


def load_sprite_pack(path: Path, kind_names: list[str]) -> tuple[int, list[dict], list[int], list[int]]:
    payload = path.read_bytes()
    if len(payload) < PACK_HEADER.size:
        raise RuntimeError(f"truncated sprite pack: {path}")
    magic, version, species_id, frame_count, rle_words, palette_words, _ = PACK_HEADER.unpack_from(payload)
    if magic != PACK_MAGIC or version != PACK_VERSION:
        raise RuntimeError(f"invalid sprite pack header: {path}")

    frame_table_end = PACK_HEADER.size + frame_count * PACK_FRAME.size
    data_end = frame_table_end + (rle_words + palette_words) * 2
    if data_end != len(payload):
        raise RuntimeError(f"sprite pack size mismatch: {path}")

    frames = []
    offset = PACK_HEADER.size
    for _ in range(frame_count):
        kind, width, height, fmt, palette_size, _reserved, data_offset, length, palette_offset = PACK_FRAME.unpack_from(payload, offset)
        offset += PACK_FRAME.size
        frames.append({
            "speciesId": species_id,
            "kind": kind,
            "kindName": kind_names[kind] if kind < len(kind_names) else f"KIND_{kind}",
            "width": width,
            "height": height,
            "format": fmt,
            "paletteSize": palette_size,
            "offset": data_offset,
            "length": length,
            "paletteOffset": palette_offset,
        })

    words = words_from_bytes(payload[frame_table_end:])
    return species_id, frames, words[:rle_words], words[rle_words:rle_words + palette_words]


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
        pixels = decode_rgb565_rle(
            rle,
            frame["offset"],
            frame["length"],
            frame["width"],
            frame["height"],
        )
    image = Image.new("RGBA", (frame["width"], frame["height"]))
    image.putdata(pixels)
    return image


def choose_preview_frame(frames: list[dict]) -> dict:
    for suffix in ("_IDLE_FRONT_0", "_WALKING_FRONT_0"):
        match = next((frame for frame in frames if frame["kindName"].endswith(suffix)), None)
        if match:
            return match
    return next((frame for frame in frames if frame["kindName"] == "FRONT"), frames[0])


def frame_action(kind_name: str) -> str | None:
    match = re.search(r"_(IDLE|WALKING|SLEEPING)(?:_|$)", kind_name)
    return match.group(1).lower() if match else None


def slugify(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


def main() -> None:
    kind_names = parse_kind_names(ASSET_H.read_text(encoding="utf-8"))
    species_idents = parse_species_idents(SPECIES_CPP.read_text(encoding="utf-8"))
    if not PACK_DIR.exists():
        raise RuntimeError(f"sprite pack directory not found: {PACK_DIR}")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for old_preview in OUT_DIR.glob("*.png"):
        old_preview.unlink()

    manifest = []
    for pack_path in sorted(PACK_DIR.glob("*.smonsp")):
        species_id, frames, rle, palettes = load_sprite_pack(pack_path, kind_names)
        ident = species_idents.get(species_id, f"SPECIES_{species_id}")
        species_name = ident.title()
        slug = slugify(ident)
        preview_frame = choose_preview_frame(frames)
        actions: dict[str, list[dict]] = {}
        frame_data_urls: dict[str, str] = {}

        for frame in frames:
            action = frame_action(frame["kindName"])
            if action is None and frame is not preview_frame:
                continue
            file_name = f"{species_id:03d}_{slug}_{frame['kindName']}.png"
            file_path = OUT_DIR / file_name
            image = decode_frame(frame, rle, palettes)
            image.save(file_path)
            data_url = png_data_url(file_path)
            frame_data_urls[frame["kindName"]] = data_url
            if action is not None:
                actions.setdefault(action, []).append({
                    "file": file_name,
                    "path": f"./generated/pokemon_sprites/{file_name}",
                    "dataUrl": data_url,
                    "width": frame["width"],
                    "height": frame["height"],
                    "kindName": frame["kindName"],
                })

        preview_file_name = f"{species_id:03d}_{slug}_{preview_frame['kindName']}.png"
        manifest.append({
            "speciesId": species_id,
            "name": f"{species_id:03d} {species_name}",
            "slug": slug,
            "frame": preview_frame["kindName"],
            "width": preview_frame["width"],
            "height": preview_frame["height"],
            "path": f"./generated/pokemon_sprites/{preview_file_name}",
            "dataUrl": frame_data_urls.get(preview_frame["kindName"], ""),
            "actions": actions,
        })

    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(
        "window.STICKMON_PROJECT_SPRITES = "
        + json.dumps(manifest, ensure_ascii=False, indent=2)
        + ";\n",
        encoding="utf-8",
    )
    print(f"wrote {len(manifest)} LittleFS species previews to {OUT_DIR}")
    print(f"wrote manifest {MANIFEST}")


if __name__ == "__main__":
    main()
