#!/usr/bin/env python3
from pathlib import Path
import os
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SPECIES_CPP = SRC / "game" / "Species.cpp"
OUT_H = SRC / "assets" / "PokemonSprites.h"
OUT_CPP = SRC / "assets" / "PokemonSprites.cpp"

ESSENTIALS = Path(os.environ.get(
    "ESSENTIALS_DIR",
    "${ESSENTIALS_DIR}",
))
GRAPHICS = ESSENTIALS / "Graphics" / "Pokemon"

ICON_SIZE = 64
BATTLE_SIZE = 72
EGG_SIZE = 64


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def to_rgba(path):
    return Image.open(path).convert("RGBA")


def resize_nearest(img, size):
    return img.resize((size, size), Image.Resampling.NEAREST)


def rle_image(img):
    pixels = list(img.getdata())
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


def species_rows():
    text = SPECIES_CPP.read_text(encoding="utf-8")
    return [(int(m.group(1)), m.group(2)) for m in re.finditer(
        r"\{(\d+),\s*Ui::SpeciesName::(\w+),", text)]


def add_frame(frames, data, species_id, ident, kind, image):
    encoded = rle_image(image)
    frame = {
        "species_id": species_id,
        "ident": ident,
        "kind": kind,
        "width": image.width,
        "height": image.height,
        "offset": len(data),
        "length": len(encoded),
    }
    frames.append(frame)
    data.extend(encoded)


def main():
    frames = []
    data = []
    missing = []

    for species_id, ident in species_rows():
        icon_path = GRAPHICS / "Icons" / f"{ident}.png"
        front_path = GRAPHICS / "Front" / f"{ident}.png"
        back_path = GRAPHICS / "Back" / f"{ident}.png"
        for path in [icon_path, front_path, back_path]:
            if not path.exists():
                missing.append(str(path))
        if missing:
            continue

        icon = to_rgba(icon_path)
        for index, x in enumerate((0, ICON_SIZE)):
            frame = icon.crop((x, 0, x + ICON_SIZE, ICON_SIZE))
            add_frame(frames, data, species_id, ident, f"ICON_{index}", frame)

        add_frame(frames, data, species_id, ident, "FRONT", resize_nearest(to_rgba(front_path), BATTLE_SIZE))
        add_frame(frames, data, species_id, ident, "BACK", resize_nearest(to_rgba(back_path), BATTLE_SIZE))

    if missing:
        raise SystemExit("Missing Pokemon sprite source files:\n" + "\n".join(missing))

    egg_path = GRAPHICS / "Eggs" / "000.png"
    egg_offset = len(data)
    egg_image = resize_nearest(to_rgba(egg_path), EGG_SIZE)
    egg_encoded = rle_image(egg_image)
    data.extend(egg_encoded)

    h_text = """#pragma once

#include <Arduino.h>
#include <cstdint>

namespace PokemonSprites {

enum class SpriteKind : uint8_t {
    ICON_0,
    ICON_1,
    FRONT,
    BACK,
};

struct SpriteFrame {
    uint16_t speciesId;
    uint8_t kind;
    uint8_t width;
    uint8_t height;
    uint32_t offset;
    uint32_t length;
};

extern const uint16_t SPRITE_FRAME_COUNT;
extern const SpriteFrame SPRITE_FRAMES[] PROGMEM;
extern const SpriteFrame EGG_FRAME PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind);

}  // namespace PokemonSprites
"""
    OUT_H.write_text(h_text, encoding="utf-8")

    frame_rows = []
    kind_map = {"ICON_0": 0, "ICON_1": 1, "FRONT": 2, "BACK": 3}
    for frame in frames:
        frame_rows.append(
            "    "
            f"{{{frame['species_id']}, {kind_map[frame['kind']]}, "
            f"{frame['width']}, {frame['height']}, {frame['offset']}, {frame['length']}}}, "
            f"// {frame['ident']} {frame['kind']}"
        )

    data_rows = []
    for i in range(0, len(data), 12):
        data_rows.append("    " + ", ".join(f"0x{v:04X}" for v in data[i:i + 12]) + ",")

    cpp_text = """#include "assets/PokemonSprites.h"

namespace PokemonSprites {

const uint16_t SPRITE_FRAME_COUNT = %d;

const SpriteFrame SPRITE_FRAMES[] PROGMEM = {
%s
};

const SpriteFrame EGG_FRAME PROGMEM = {0, 0, %d, %d, %d, %d};

const uint16_t SPRITE_RLE[] PROGMEM = {
%s
};

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind) {
    for (uint16_t i = 0; i < SPRITE_FRAME_COUNT; ++i) {
        if (pgm_read_word(&SPRITE_FRAMES[i].speciesId) == speciesId &&
            pgm_read_byte(&SPRITE_FRAMES[i].kind) == static_cast<uint8_t>(kind)) {
            return &SPRITE_FRAMES[i];
        }
    }
    return nullptr;
}

}  // namespace PokemonSprites
""" % (
        len(frames),
        "\n".join(frame_rows),
        egg_image.width,
        egg_image.height,
        egg_offset,
        len(egg_encoded),
        "\n".join(data_rows),
    )
    OUT_CPP.write_text(cpp_text, encoding="utf-8")

    raw_pixels = len(frames) * 0
    for frame in frames:
        raw_pixels += frame["width"] * frame["height"]
    raw_pixels += egg_image.width * egg_image.height
    raw_bytes = raw_pixels * 2
    encoded_bytes = len(data) * 2
    print(f"species={len(frames) // 4} frames={len(frames)} words={len(data)}")
    print(f"raw_rgb565_bytes={raw_bytes} encoded_rle_bytes={encoded_bytes}")


if __name__ == "__main__":
    main()
