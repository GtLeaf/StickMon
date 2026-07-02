#!/usr/bin/env python3
from dataclasses import dataclass
from pathlib import Path
import os
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SPECIES_CPP = SRC / "game" / "Species.cpp"
OUT_H = SRC / "assets" / "PokemonSprites.h"
OUT_CPP = SRC / "assets" / "PokemonSprites.cpp"
PROCESSED = ROOT / "origin_asset" / "processed"

ESSENTIALS = Path(os.environ.get(
    "ESSENTIALS_DIR",
    "${ESSENTIALS_DIR}",
))
GRAPHICS = ESSENTIALS / "Graphics" / "Pokemon"

ICON_SIZE = 64
BATTLE_SIZE = 72
EGG_SIZE = 64

RGB565_RLE = 0
INDEXED4_RLE = 1

FULL_DIRECTIONS = ["front", "down_left", "left", "up_left", "back", "up_right", "right", "down_right"]
SOURCE_DIRECTIONS = ["front", "down_left", "left", "up_left", "back"]
BASE_KINDS = ["ICON_0", "FRONT", "BACK"]


@dataclass(frozen=True)
class PmdSpec:
    species_id: int
    ident: str
    slug: str
    idle_frames: int
    walking_frames: int
    sleeping_frames: int
    directions: tuple = tuple(FULL_DIRECTIONS)


PMD_SPECS = [
    PmdSpec(1, "BULBASAUR", "bulbasaur", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(2, "IVYSAUR", "ivysaur", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(3, "VENUSAUR", "venusaur", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(4, "CHARMANDER", "charmander", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(5, "CHARMELEON", "charmeleon", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(6, "CHARIZARD", "charizard", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(7, "SQUIRTLE", "squirtle", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(8, "WARTORTLE", "wartortle", 1, 3, 2),
    PmdSpec(9, "BLASTOISE", "blastoise", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(25, "PIKACHU", "pikachu", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(26, "RAICHU", "raichu", 1, 3, 2),
    PmdSpec(92, "GASTLY", "gastly", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(93, "HAUNTER", "haunter", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(94, "GENGAR", "gengar", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(123, "SCYTHER", "scyther", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(129, "MAGIKARP", "magikarp", 2, 1, 2),
    PmdSpec(130, "GYARADOS", "gyarados", 1, 3, 2),
    PmdSpec(133, "EEVEE", "eevee", 2, 3, 2),
    PmdSpec(134, "VAPOREON", "vaporeon", 1, 4, 2),
    PmdSpec(135, "JOLTEON", "jolteon", 1, 4, 2),
    PmdSpec(136, "FLAREON", "flareon", 3, 4, 2),
    PmdSpec(196, "ESPEON", "espeon", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(197, "UMBREON", "umbreon", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(143, "SNORLAX", "snorlax", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(147, "DRATINI", "dratini", 1, 3, 2),
    PmdSpec(148, "DRAGONAIR", "dragonair", 2, 1, 2),
    PmdSpec(149, "DRAGONITE", "dragonite", 1, 2, 2),
    PmdSpec(151, "MEW", "mew", 3, 2, 2),
    PmdSpec(172, "PICHU", "pichu", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(212, "SCIZOR", "scizor", 2, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(380, "LATIAS", "latias", 1, 2, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(381, "LATIOS", "latios", 1, 2, 2, tuple(SOURCE_DIRECTIONS)),
]
PMD_BY_ID = {spec.species_id: spec for spec in PMD_SPECS}


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def to_rgba(path):
    return Image.open(path).convert("RGBA")


def resize_nearest(img, size):
    return img.resize((size, size), Image.Resampling.NEAREST)


def image_pixels(img):
    if hasattr(img, "get_flattened_data"):
        return list(img.get_flattened_data())
    return list(img.getdata())


def rle_rgb565_image(img):
    pixels = image_pixels(img)
    out = []
    i = 0
    while i < len(pixels):
        r, g, b, a = pixels[i]
        transparent = a <= 16
        j = i + 1
        if transparent:
            while j < len(pixels) and pixels[j][3] <= 16 and (j - i) < 0x7FFF:
                j += 1
            out.append(0x8000 | (j - i))
        else:
            values = []
            while j <= len(pixels) and len(values) < 0x7FFF:
                pr, pg, pb, pa = pixels[j - 1]
                if pa <= 16:
                    break
                values.append(rgb565(pr, pg, pb))
                if j == len(pixels) or pixels[j][3] <= 16:
                    break
                j += 1
            out.append(len(values))
            out.extend(values)
        i = j
    return out


def rle_indexed4_image(img):
    pixels = image_pixels(img)
    palette = []
    palette_map = {}
    for r, g, b, a in pixels:
        if a <= 16:
            continue
        color = rgb565(r, g, b)
        if color not in palette_map:
            if len(palette) >= 16:
                return None
            palette_map[color] = len(palette)
            palette.append(color)

    if not palette:
        return None

    out = []
    i = 0
    while i < len(pixels):
        r, g, b, a = pixels[i]
        transparent = a <= 16
        j = i + 1
        if transparent:
            while j < len(pixels) and pixels[j][3] <= 16 and (j - i) < 0x7FFF:
                j += 1
            out.append(0x8000 | (j - i))
        else:
            indices = []
            while j <= len(pixels) and len(indices) < 0x7FFF:
                pr, pg, pb, pa = pixels[j - 1]
                if pa <= 16:
                    break
                indices.append(palette_map[rgb565(pr, pg, pb)])
                if j == len(pixels) or pixels[j][3] <= 16:
                    break
                j += 1
            out.append(len(indices))
            for base in range(0, len(indices), 4):
                packed = 0
                for nibble, palette_index in enumerate(indices[base:base + 4]):
                    packed |= palette_index << (nibble * 4)
                out.append(packed)
        i = j
    return out, palette


def species_rows():
    text = SPECIES_CPP.read_text(encoding="utf-8")
    return [(int(m.group(1)), m.group(2)) for m in re.finditer(
        r"\{(\d+),\s*Ui::SpeciesName::(\w+),", text)]


def pmd_path(spec, action, name):
    if action == "sleeping":
        path = PROCESSED / spec.slug / action / f"frame_{name}.png"
    else:
        path = PROCESSED / spec.slug / action / f"{name}.png"
    if not path.exists():
        raise FileNotFoundError(path)
    return path


class AssetWriter:
    def __init__(self):
        self.frames = []
        self.data = []
        self.palettes = []
        self.rle_cache = {}
        self.palette_cache = {}
        self.indexed4_count = 0
        self.rgb565_count = 0
        self.rle_duplicate_count = 0
        self.palette_duplicate_count = 0

    def add_palette(self, palette):
        key = tuple(palette)
        if key in self.palette_cache:
            self.palette_duplicate_count += 1
            return self.palette_cache[key]
        offset = len(self.palettes)
        self.palette_cache[key] = offset
        self.palettes.extend(palette)
        return offset

    def encode(self, image):
        indexed = rle_indexed4_image(image)
        if indexed:
            encoded, palette = indexed
            fmt = INDEXED4_RLE
            palette_offset = self.add_palette(palette)
            palette_size = len(palette)
            self.indexed4_count += 1
        else:
            encoded = rle_rgb565_image(image)
            fmt = RGB565_RLE
            palette_offset = 0
            palette_size = 0
            self.rgb565_count += 1

        key = (fmt, tuple(encoded))
        if key in self.rle_cache:
            self.rle_duplicate_count += 1
            offset, length = self.rle_cache[key]
        else:
            offset = len(self.data)
            length = len(encoded)
            self.rle_cache[key] = (offset, length)
            self.data.extend(encoded)
        return fmt, palette_size, offset, length, palette_offset

    def add_frame(self, species_id, ident, kind, image):
        fmt, palette_size, offset, length, palette_offset = self.encode(image)
        self.frames.append({
            "species_id": species_id,
            "ident": ident,
            "kind": kind,
            "width": image.width,
            "height": image.height,
            "format": fmt,
            "palette_size": palette_size,
            "offset": offset,
            "length": length,
            "palette_offset": palette_offset,
        })


def add_base_frames(writer, species_id, ident, missing):
    icon_path = GRAPHICS / "Icons" / f"{ident}.png"
    if not icon_path.exists():
        missing.append(str(icon_path))
        return

    icon = to_rgba(icon_path)
    writer.add_frame(species_id, ident, "ICON_0", icon.crop((0, 0, ICON_SIZE, ICON_SIZE)))

    spec = PMD_BY_ID.get(species_id)
    if spec and (PROCESSED / spec.slug).exists():
        front = to_rgba(pmd_path(spec, "walking", "front_0"))
        back = to_rgba(pmd_path(spec, "walking", "back_0"))
        writer.add_frame(species_id, ident, "FRONT", front)
        writer.add_frame(species_id, ident, "BACK", back)
        return

    front_path = GRAPHICS / "Front" / f"{ident}.png"
    back_path = GRAPHICS / "Back" / f"{ident}.png"
    local_missing = []
    for path in [front_path, back_path]:
        if not path.exists():
            local_missing.append(str(path))
    if local_missing:
        missing.extend(local_missing)
        return

    writer.add_frame(species_id, ident, "FRONT", resize_nearest(to_rgba(front_path), BATTLE_SIZE))
    writer.add_frame(species_id, ident, "BACK", resize_nearest(to_rgba(back_path), BATTLE_SIZE))


def add_pmd_frames(writer, spec):
    for direction in spec.directions:
        for index in range(spec.idle_frames):
            writer.add_frame(
                spec.species_id, spec.ident,
                f"{spec.ident}_IDLE_{direction.upper()}_{index}",
                to_rgba(pmd_path(spec, "idle", f"{direction}_{index}")),
            )

    for direction in spec.directions:
        for index in range(spec.walking_frames):
            writer.add_frame(
                spec.species_id, spec.ident,
                f"{spec.ident}_WALKING_{direction.upper()}_{index}",
                to_rgba(pmd_path(spec, "walking", f"{direction}_{index}")),
            )

    for index in range(spec.sleeping_frames):
        writer.add_frame(
            spec.species_id, spec.ident,
            f"{spec.ident}_SLEEPING_{index}",
            to_rgba(pmd_path(spec, "sleeping", str(index))),
        )


def format_words(values):
    rows = []
    for i in range(0, len(values), 12):
        rows.append("    " + ", ".join(f"0x{v:04X}" for v in values[i:i + 12]) + ",")
    return "\n".join(rows)


def main():
    writer = AssetWriter()
    missing = []

    kind_names = list(BASE_KINDS)
    for spec in PMD_SPECS:
        for action, count in (("IDLE", spec.idle_frames), ("WALKING", spec.walking_frames)):
            for direction in spec.directions:
                for index in range(count):
                    kind_names.append(f"{spec.ident}_{action}_{direction.upper()}_{index}")
        for index in range(spec.sleeping_frames):
            kind_names.append(f"{spec.ident}_SLEEPING_{index}")
    kind_map = {name: index for index, name in enumerate(kind_names)}

    present_species_ids = set()
    for species_id, ident in species_rows():
        present_species_ids.add(species_id)
        add_base_frames(writer, species_id, ident, missing)
        spec = PMD_BY_ID.get(species_id)
        if spec and (PROCESSED / spec.slug).exists():
            add_pmd_frames(writer, spec)

    for spec in PMD_SPECS:
        if spec.species_id in present_species_ids or not (PROCESSED / spec.slug).exists():
            continue
        add_base_frames(writer, spec.species_id, spec.ident, missing)
        add_pmd_frames(writer, spec)

    if missing:
        raise SystemExit("Missing Pokemon sprite source files:\n" + "\n".join(missing))

    egg_path = GRAPHICS / "Eggs" / "000.png"
    egg_image = resize_nearest(to_rgba(egg_path), EGG_SIZE)
    egg_format, egg_palette_size, egg_offset, egg_length, egg_palette_offset = writer.encode(egg_image)

    enum_rows = "\n".join(f"    {name}," for name in kind_names)
    h_text = f"""#pragma once

#include <Arduino.h>
#include <cstdint>

namespace PokemonSprites {{

enum class SpriteKind : uint16_t {{
{enum_rows}
}};

enum class SpriteFormat : uint8_t {{
    RGB565_RLE = 0,
    INDEXED4_RLE = 1,
}};

struct SpriteFrame {{
    uint16_t speciesId;
    uint16_t kind;
    uint8_t width;
    uint8_t height;
    uint8_t format;
    uint8_t paletteSize;
    uint32_t offset;
    uint32_t length;
    uint32_t paletteOffset;
}};

extern const uint16_t SPRITE_FRAME_COUNT;
extern const SpriteFrame SPRITE_FRAMES[] PROGMEM;
extern const SpriteFrame EGG_FRAME PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;
extern const uint16_t SPRITE_PALETTES[] PROGMEM;

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind);
bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX = false);

}}  // namespace PokemonSprites
"""
    OUT_H.write_text(h_text, encoding="utf-8")

    frame_rows = []
    for frame in writer.frames:
        frame_rows.append(
            "    "
            f"{{{frame['species_id']}, {kind_map[frame['kind']]}, "
            f"{frame['width']}, {frame['height']}, {frame['format']}, {frame['palette_size']}, "
            f"{frame['offset']}, {frame['length']}, {frame['palette_offset']}}}, "
            f"// {frame['ident']} {frame['kind']}"
        )

    cpp_text = """#include "assets/PokemonSprites.h"
#include "hardware/PixelRenderer.h"

namespace PokemonSprites {

const uint16_t SPRITE_FRAME_COUNT = %d;

const SpriteFrame SPRITE_FRAMES[] PROGMEM = {
%s
};

const SpriteFrame EGG_FRAME PROGMEM = {0, 0, %d, %d, %d, %d, %d, %d, %d};

const uint16_t SPRITE_RLE[] PROGMEM = {
%s
};

const uint16_t SPRITE_PALETTES[] PROGMEM = {
%s
};

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind) {
    for (uint16_t i = 0; i < SPRITE_FRAME_COUNT; ++i) {
        if (pgm_read_word(&SPRITE_FRAMES[i].speciesId) == speciesId &&
            pgm_read_word(&SPRITE_FRAMES[i].kind) == static_cast<uint16_t>(kind)) {
            return &SPRITE_FRAMES[i];
        }
    }
    return nullptr;
}

bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX) {
    if (!frame) return false;

    uint8_t width = pgm_read_byte(&frame->width);
    uint8_t height = pgm_read_byte(&frame->height);
    uint32_t offset = pgm_read_dword(&frame->offset);
    uint32_t length = pgm_read_dword(&frame->length);
    if (width == 0 || height == 0 || length == 0) return false;

    uint8_t format = pgm_read_byte(&frame->format);
    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        PixelRenderer::drawIndexed4Rle(
            x, y, width, height, SPRITE_RLE, offset, length, SPRITE_PALETTES,
            pgm_read_dword(&frame->paletteOffset),
            pgm_read_byte(&frame->paletteSize),
            flipX);
        return true;
    }

    PixelRenderer::drawRgb565Rle(x, y, width, height, SPRITE_RLE, offset, length, flipX);
    return true;
}

}  // namespace PokemonSprites
""" % (
        len(writer.frames),
        "\n".join(frame_rows),
        egg_image.width,
        egg_image.height,
        egg_format,
        egg_palette_size,
        egg_offset,
        egg_length,
        egg_palette_offset,
        format_words(writer.data),
        format_words(writer.palettes),
    )
    OUT_CPP.write_text(cpp_text, encoding="utf-8")

    raw_pixels = sum(frame["width"] * frame["height"] for frame in writer.frames)
    raw_pixels += egg_image.width * egg_image.height
    raw_bytes = raw_pixels * 2
    encoded_bytes = len(writer.data) * 2
    palette_bytes = len(writer.palettes) * 2
    total_bytes = encoded_bytes + palette_bytes
    print(
        f"species={len(species_rows())} frames={len(writer.frames)} "
        f"rle_words={len(writer.data)} palette_words={len(writer.palettes)} "
        f"indexed4={writer.indexed4_count} rgb565={writer.rgb565_count}"
    )
    print(
        f"rle_duplicates={writer.rle_duplicate_count} "
        f"palette_duplicates={writer.palette_duplicate_count}"
    )
    print(f"raw_rgb565_bytes={raw_bytes} encoded_asset_bytes={total_bytes}")


if __name__ == "__main__":
    main()
