#!/usr/bin/env python3
from dataclasses import dataclass
from pathlib import Path
import json
import os
import re
import struct
import zlib
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
SPECIES_CPP = SRC / "game" / "Species.cpp"
OUT_H = SRC / "assets" / "PokemonSprites.h"
OUT_CPP = SRC / "assets" / "PokemonSprites.cpp"
PROCESSED = ROOT / "origin_asset" / "processed"
DATA_DIR = ROOT / "data"
PACK_OUT = DATA_DIR / "packs" / "dev"
PACK_SPRITE_OUT = PACK_OUT / "sprites"

ESSENTIALS = Path(os.environ.get(
    "ESSENTIALS_DIR",
    "${ESSENTIALS_DIR}",
))
GRAPHICS = ESSENTIALS / "Graphics" / "Pokemon"

ICON_SIZE = 64
BATTLE_SIZE = 72
EGG_SIZE = 32
ENEMY_FRONT_MAX_WIDTH = 116
ENEMY_FRONT_MAX_HEIGHT = 66
ENEMY_FRONT_MIN_TARGET_HEIGHT = 48
ENEMY_FRONT_MAX_UPSCALE = 1.75
PLAYER_BACK_MAX_WIDTH = 105
PLAYER_BACK_MAX_HEIGHT = 65
STATUS_PORTRAIT_MAX_WIDTH = 70
STATUS_PORTRAIT_MAX_HEIGHT = 78

RGB565_RLE = 0
INDEXED4_RLE = 1
SPRITE_SOURCE_GLOBAL = 0
SPRITE_SOURCE_SPECIES_BLOCK = 1
SPRITE_PACK_MAGIC = 0x5350534D
SPRITE_PACK_VERSION = 2
SPRITE_PACK_FLAG_RAW_DEFLATE = 1
SPRITE_FRAME_GROUND_MARKER = 0xA500
SPRITE_PACK_SCHEMA = 1

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


def processed_species_dir(spec):
    return PROCESSED / f"{spec.species_id:03d}_{spec.slug}"


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
    PmdSpec(10, "CATERPIE", "caterpie", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(11, "METAPOD", "metapod", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(12, "BUTTERFREE", "butterfree", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(16, "PIDGEY", "pidgey", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(17, "PIDGEOTTO", "pidgeotto", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(18, "PIDGEOT", "pidgeot", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(25, "PIKACHU", "pikachu", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(26, "RAICHU", "raichu", 1, 3, 2),
    PmdSpec(74, "GEODUDE", "geodude", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(75, "GRAVELER", "graveler", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(76, "GOLEM", "golem", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
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
    PmdSpec(148, "DRAGONAIR", "dragonair", 1, 3, 2),
    PmdSpec(149, "DRAGONITE", "dragonite", 1, 2, 2),
    PmdSpec(151, "MEW", "mew", 3, 2, 2),
    PmdSpec(161, "SENTRET", "sentret", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(162, "FURRET", "furret", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(172, "PICHU", "pichu", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(183, "MARILL", "marill", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(184, "AZUMARILL", "azumarill", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(194, "WOOPER", "wooper", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(195, "QUAGSIRE", "quagsire", 1, 2, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(212, "SCIZOR", "scizor", 2, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(261, "POOCHYENA", "poochyena", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(262, "MIGHTYENA", "mightyena", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(278, "WINGULL", "wingull", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(279, "PELIPPER", "pelipper", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(285, "SHROOMISH", "shroomish", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(286, "BRELOOM", "breloom", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(380, "LATIAS", "latias", 1, 2, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(381, "LATIOS", "latios", 1, 2, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(298, "AZURILL", "azurill", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(322, "NUMEL", "numel", 2, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(323, "CAMERUPT", "camerupt", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(361, "SNORUNT", "snorunt", 1, 4, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(362, "GLALIE", "glalie", 1, 1, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(280, "RALTS", "ralts", 2, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(281, "KIRLIA", "kirlia", 2, 2, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(282, "GARDEVOIR", "gardevoir", 3, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(41, "ZUBAT", "zubat", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(42, "GOLBAT", "golbat", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
    PmdSpec(169, "CROBAT", "crobat", 1, 3, 2, tuple(SOURCE_DIRECTIONS)),
]
PMD_BY_ID = {spec.species_id: spec for spec in PMD_SPECS}


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def to_rgba(path):
    return Image.open(path).convert("RGBA")


def trim_alpha_padding(img, alpha_threshold=16):
    alpha = img.getchannel("A").point(
        lambda value: 255 if value > alpha_threshold else 0
    )
    bounds = alpha.getbbox()
    return img.crop(bounds) if bounds else img


def prepare_enemy_battle_front(img):
    trimmed = trim_alpha_padding(img)
    base_width = trimmed.width * BATTLE_SIZE / max(1, img.width)
    base_height = trimmed.height * BATTLE_SIZE / max(1, img.height)

    scale = 1.0
    if base_height < ENEMY_FRONT_MIN_TARGET_HEIGHT:
        scale = min(
            ENEMY_FRONT_MIN_TARGET_HEIGHT / max(1.0, base_height),
            ENEMY_FRONT_MAX_UPSCALE,
        )
    if base_width * scale > ENEMY_FRONT_MAX_WIDTH:
        scale = min(scale, ENEMY_FRONT_MAX_WIDTH / max(1.0, base_width))
    if base_height * scale > ENEMY_FRONT_MAX_HEIGHT:
        scale = min(scale, ENEMY_FRONT_MAX_HEIGHT / max(1.0, base_height))

    target_width = min(
        ENEMY_FRONT_MAX_WIDTH,
        max(1, int(base_width * scale + 0.5)),
    )
    target_height = min(
        ENEMY_FRONT_MAX_HEIGHT,
        max(1, int(base_height * scale + 0.5)),
    )
    return trimmed.resize(
        (target_width, target_height), Image.Resampling.NEAREST
    )


def prepare_player_battle_back(img):
    trimmed = trim_alpha_padding(img)
    scale = min(
        PLAYER_BACK_MAX_WIDTH / max(1, trimmed.width),
        PLAYER_BACK_MAX_HEIGHT / max(1, trimmed.height),
    )
    target_width = max(1, int(trimmed.width * scale + 0.5))
    target_height = max(1, int(trimmed.height * scale + 0.5))
    return trimmed.resize(
        (target_width, target_height), Image.Resampling.NEAREST
    )


def prepare_status_portrait(img):
    trimmed = trim_alpha_padding(img)
    scale = min(
        1.0,
        STATUS_PORTRAIT_MAX_WIDTH / max(1, trimmed.width),
        STATUS_PORTRAIT_MAX_HEIGHT / max(1, trimmed.height),
    )
    if scale >= 1.0:
        return trimmed
    target_width = max(1, int(trimmed.width * scale + 0.5))
    target_height = max(1, int(trimmed.height * scale + 0.5))
    return trimmed.resize(
        (target_width, target_height), Image.Resampling.NEAREST
    )


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
        path = processed_species_dir(spec) / action / f"frame_{name}.png"
    else:
        path = processed_species_dir(spec) / action / f"{name}.png"
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

    def add_frame(self, species_id, ident, kind, image, source=SPRITE_SOURCE_GLOBAL):
        fmt, palette_size, offset, length, palette_offset = self.encode(image)
        alpha = image.getchannel("A").point(
            lambda value: 255 if value > 16 else 0
        )
        visible_bounds = alpha.getbbox()
        ground_padding = (
            image.height - visible_bounds[3] if visible_bounds else 0
        )
        self.frames.append({
            "species_id": species_id,
            "ident": ident,
            "kind": kind,
            "width": image.width,
            "height": image.height,
            "format": fmt,
            "palette_size": palette_size,
            "source": source,
            "offset": offset,
            "length": length,
            "palette_offset": palette_offset,
            "ground_padding": min(254, ground_padding),
        })


def add_base_frames(writer, species_id, ident, missing):
    icon_path = GRAPHICS / "Icons" / f"{ident}.png"
    front_path = GRAPHICS / "Front" / f"{ident}.png"
    back_path = GRAPHICS / "Back" / f"{ident}.png"
    local_missing = []
    for path in [icon_path, front_path, back_path]:
        if not path.exists():
            local_missing.append(str(path))
    if local_missing:
        missing.extend(local_missing)
        return

    icon = to_rgba(icon_path)
    writer.add_frame(species_id, ident, "ICON_0", icon.crop((0, 0, ICON_SIZE, ICON_SIZE)))
    front_source = to_rgba(front_path)
    front = prepare_enemy_battle_front(front_source)
    writer.add_frame(species_id, ident, "FRONT", front)
    writer.add_frame(
        species_id,
        ident,
        "BACK",
        prepare_player_battle_back(to_rgba(back_path)),
    )
    writer.add_frame(
        species_id,
        ident,
        "STATUS",
        prepare_status_portrait(front_source),
    )


def pmd_idle_frame_path(spec, direction, index):
    if spec.species_id == 147:
        return pmd_path(spec, "walking", f"{direction}_{index}")
    return pmd_path(spec, "idle", f"{direction}_{index}")


def pmd_walking_frame_path(spec, direction, index):
    if spec.species_id == 148:
        if index < 2:
            return pmd_path(spec, "idle", f"{direction}_{index}")
        return pmd_path(spec, "walking", f"{direction}_0")
    return pmd_path(spec, "walking", f"{direction}_{index}")


def add_pmd_frames(writer, spec):
    for direction in spec.directions:
        for index in range(spec.idle_frames):
            writer.add_frame(
                spec.species_id, spec.ident,
                f"{spec.ident}_IDLE_{direction.upper()}_{index}",
                to_rgba(pmd_idle_frame_path(spec, direction, index)),
                SPRITE_SOURCE_SPECIES_BLOCK,
            )

    for direction in spec.directions:
        for index in range(spec.walking_frames):
            writer.add_frame(
                spec.species_id, spec.ident,
                f"{spec.ident}_WALKING_{direction.upper()}_{index}",
                to_rgba(pmd_walking_frame_path(spec, direction, index)),
                SPRITE_SOURCE_SPECIES_BLOCK,
            )

    for index in range(spec.sleeping_frames):
        writer.add_frame(
            spec.species_id, spec.ident,
            f"{spec.ident}_SLEEPING_{index}",
            to_rgba(pmd_path(spec, "sleeping", str(index))),
            SPRITE_SOURCE_SPECIES_BLOCK,
        )


def words_to_bytes(words):
    out = bytearray()
    for value in words:
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    return bytes(out)


def write_json_file(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def merge_pack_manifest(**updates):
    manifest_path = PACK_OUT / "manifest.json"
    payload = {}
    if manifest_path.exists():
        try:
            loaded = json.loads(manifest_path.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                payload = loaded
        except (json.JSONDecodeError, OSError):
            pass
    payload.update({
        "format": "smon-resource-pack-v1",
        "id": "dev",
        "schema": SPRITE_PACK_SCHEMA,
        "version": "0.0.0-dev",
    })
    payload.update(updates)
    write_json_file(manifest_path, payload)


def write_species_pack(path, species_id, writer, kind_map):
    if len(writer.frames) > 0xFFFF:
        raise SystemExit(f"Too many sprite frames for species {species_id}")
    if len(writer.data) > 0xFFFF or len(writer.palettes) > 0xFFFF:
        raise SystemExit(f"Sprite pack too large for species {species_id}")

    payload = bytearray()
    for frame in writer.frames:
        payload.extend(struct.pack(
            "<HBBBBHIII",
            kind_map[frame["kind"]],
            frame["width"],
            frame["height"],
            frame["format"],
            frame["palette_size"],
            SPRITE_FRAME_GROUND_MARKER | frame["ground_padding"],
            frame["offset"],
            frame["length"],
            frame["palette_offset"],
        ))
    payload.extend(words_to_bytes(writer.data))
    payload.extend(words_to_bytes(writer.palettes))

    compressor = zlib.compressobj(level=6, wbits=-15)
    compressed = compressor.compress(payload) + compressor.flush()
    header = struct.pack(
        "<IHHHHHHIII",
        SPRITE_PACK_MAGIC,
        SPRITE_PACK_VERSION,
        species_id,
        len(writer.frames),
        len(writer.data),
        len(writer.palettes),
        SPRITE_PACK_FLAG_RAW_DEFLATE,
        len(payload),
        len(compressed),
        zlib.crc32(payload) & 0xFFFFFFFF,
    )

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + compressed)


def export_dev_pack(kind_map):
    if PACK_SPRITE_OUT.exists():
        for path in PACK_SPRITE_OUT.glob("*.smonsp"):
            path.unlink()
    PACK_SPRITE_OUT.mkdir(parents=True, exist_ok=True)

    missing = []
    exported = 0
    present_species_ids = set()
    for species_id, ident in species_rows():
        present_species_ids.add(species_id)
        writer = AssetWriter()
        add_base_frames(writer, species_id, ident, missing)
        spec = PMD_BY_ID.get(species_id)
        if spec and processed_species_dir(spec).exists():
            add_pmd_frames(writer, spec)
        if writer.frames:
            write_species_pack(PACK_SPRITE_OUT / f"{species_id:03d}.smonsp", species_id, writer, kind_map)
            exported += 1

    for spec in PMD_SPECS:
        if spec.species_id in present_species_ids or not processed_species_dir(spec).exists():
            continue
        writer = AssetWriter()
        add_base_frames(writer, spec.species_id, spec.ident, missing)
        add_pmd_frames(writer, spec)
        if writer.frames:
            write_species_pack(PACK_SPRITE_OUT / f"{spec.species_id:03d}.smonsp", spec.species_id, writer, kind_map)
            exported += 1

    if missing:
        raise SystemExit("Missing Pokemon sprite source files:\n" + "\n".join(missing))

    write_json_file(DATA_DIR / "active.json", {
        "activePack": "dev",
        "packPath": "/packs/dev",
    })
    merge_pack_manifest(spriteCount=exported, sprites="sprites")
    return exported


def main():
    kind_names = list(BASE_KINDS)
    for spec in PMD_SPECS:
        for action, count in (("IDLE", spec.idle_frames), ("WALKING", spec.walking_frames)):
            for direction in spec.directions:
                for index in range(count):
                    kind_names.append(f"{spec.ident}_{action}_{direction.upper()}_{index}")
        for index in range(spec.sleeping_frames):
            kind_names.append(f"{spec.ident}_SLEEPING_{index}")
    # Keep additions after generated PMD kinds so existing pack frame IDs stay stable.
    kind_names.append("STATUS")
    kind_map = {name: index for index, name in enumerate(kind_names)}

    walking_rows = []
    for spec in PMD_SPECS:
        right_direction = "right" if "right" in spec.directions else "left"
        walking_rows.append(
            "    {%d, SpriteKind::%s_WALKING_FRONT_0, SpriteKind::%s_WALKING_LEFT_0, "
            "SpriteKind::%s_WALKING_BACK_0, SpriteKind::%s_WALKING_%s_0, %d, %s}," % (
                spec.species_id,
                spec.ident,
                spec.ident,
                spec.ident,
                spec.ident,
                right_direction.upper(),
                spec.walking_frames,
                "false" if right_direction == "right" else "true",
            )
        )
    walking_table = "\n".join(walking_rows)

    pack_sprite_count = export_dev_pack(kind_map)

    enum_rows = "\n".join(f"    {name}," for name in kind_names)
    h_text = f"""// AUTO-GENERATED by tools/generate_pokemon_sprites.py. Do not edit manually.
#pragma once

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

enum class WalkDirection : uint8_t {{
    DOWN,
    LEFT,
    UP,
    RIGHT,
}};

struct WalkingAnimation {{
    SpriteKind base;
    uint8_t frameCount;
    bool flipX;
}};

struct SpriteFrame {{
    uint16_t speciesId;
    uint16_t kind;
    uint8_t width;
    uint8_t height;
    uint8_t format;
    uint8_t paletteSize;
    uint8_t source;
    uint8_t groundPaddingPlusOne;
    uint32_t offset;
    uint32_t length;
    uint32_t paletteOffset;
}};

struct SpriteCacheStats {{
    uint8_t cachedSpecies;
    uint8_t missingSpecies;
    uint16_t firstMissingSpecies;
    uint32_t reloadCount;
    uint32_t lastReloadMs;
    uint32_t decodedBytes;
    uint32_t compressedBytes;
    uint32_t freePsram;
    bool psram;
}};

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind);
int16_t frameGroundOffsetY(const SpriteFrame* frame);
bool walkingAnimation(uint16_t speciesId, WalkDirection direction, WalkingAnimation& animation);
bool syncTeamCache(const uint16_t* speciesIds, uint8_t count, uint8_t loadBudget = 0xFF);
void setDynamicLoadingEnabled(bool enabled);
void beginRenderFrame();
const SpriteCacheStats& cacheStats();
bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX = false);
bool drawFrameScaled(const SpriteFrame* frame, int x, int y, float scale, bool flipX = false);

}}  // namespace PokemonSprites
"""
    OUT_H.write_text(h_text, encoding="utf-8")

    cpp_text = """// AUTO-GENERATED by tools/generate_pokemon_sprites.py. Do not edit manually.
#include "assets/PokemonSprites.h"
#include "core/DeflateDecoder.h"
#include "core/ResourcePack.h"
#include <Arduino.h>
#include <FS.h>
#include <cstdlib>
#include <cstring>
#include "hardware/PixelRenderer.h"

namespace PokemonSprites {

namespace {
static constexpr uint8_t SPRITE_SOURCE_FILE_BLOCK = 2;
static constexpr uint16_t SPRITE_FRAME_GROUND_MARKER = 0xA500;
static constexpr uint8_t TEAM_CACHE_CAP = 2;
static constexpr uint8_t CACHE_CAP = 4;
static constexpr uint8_t KNOWN_MISSING_FILE_CAP = 16;
static constexpr uint8_t FRAME_MISSING_CAP = 4;
static constexpr uint8_t KNOWN_MISSING_FRAME_CAP = 16;
static constexpr uint32_t SPRITE_PACK_MAGIC = 0x5350534D;
static constexpr uint16_t SPRITE_PACK_VERSION = 2;
static constexpr uint16_t SPRITE_PACK_FLAG_RAW_DEFLATE = 1;
static constexpr uint16_t MAX_PACK_FRAMES = 256;
static constexpr uint32_t MAX_PACK_PAYLOAD_BYTES = 128000;

struct __attribute__((packed)) PackedSpriteHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t speciesId;
    uint16_t frameCount;
    uint16_t rleWords;
    uint16_t paletteWords;
    uint16_t flags;
    uint32_t payloadRawBytes;
    uint32_t payloadCompressedBytes;
    uint32_t payloadCrc32;
};

struct __attribute__((packed)) PackedSpriteFrame {
    uint16_t kind;
    uint8_t width;
    uint8_t height;
    uint8_t format;
    uint8_t paletteSize;
    uint16_t reserved;
    uint32_t offset;
    uint32_t length;
    uint32_t paletteOffset;
};

static_assert(sizeof(PackedSpriteHeader) == 28, "Unexpected sprite pack header layout");
static_assert(sizeof(PackedSpriteFrame) == 20, "Unexpected sprite pack frame layout");

struct CachedSpecies {
    uint16_t speciesId = 0;
    uint16_t rleWords = 0;
    uint16_t paletteWords = 0;
    uint16_t frameCount = 0;
    SpriteFrame* frames = nullptr;
    uint8_t* payload = nullptr;
    uint32_t payloadBytes = 0;
    uint32_t packedBytes = 0;
    uint16_t* data = nullptr;
    uint16_t* palettes = nullptr;
};

struct MissingFrameKey {
    uint16_t speciesId = 0;
    uint16_t kind = 0;
};

CachedSpecies gCache[CACHE_CAP];
SpriteCacheStats gStats = {};
uint16_t gTeamSignature[CACHE_CAP] = {};
uint16_t gKnownMissingFiles[KNOWN_MISSING_FILE_CAP] = {};
uint16_t gTeamMissing[TEAM_CACHE_CAP] = {};
uint16_t gFrameMissing[FRAME_MISSING_CAP] = {};
MissingFrameKey gKnownMissingFrames[KNOWN_MISSING_FRAME_CAP] = {};
uint8_t gTeamCount = 0;
uint8_t gDynamicSlot = TEAM_CACHE_CAP;
bool gDynamicLoadingEnabled = true;

bool containsSpecies(const uint16_t* values, uint8_t count, uint16_t speciesId) {
    for (uint8_t i = 0; i < count; ++i) {
        if (values[i] == speciesId) return true;
    }
    return false;
}

bool appendUniqueSpecies(uint16_t* values, uint8_t count, uint16_t speciesId) {
    if (speciesId == 0 || containsSpecies(values, count, speciesId)) return false;
    for (uint8_t i = 0; i < count; ++i) {
        if (values[i] == 0) {
            values[i] = speciesId;
            return true;
        }
    }
    return false;
}

void refreshMissingStats() {
    uint16_t unique[TEAM_CACHE_CAP + FRAME_MISSING_CAP] = {};
    uint8_t uniqueCount = 0;
    auto collect = [&](const uint16_t* values, uint8_t count) {
        for (uint8_t i = 0; i < count; ++i) {
            uint16_t speciesId = values[i];
            if (speciesId == 0 || containsSpecies(unique, uniqueCount, speciesId)) continue;
            unique[uniqueCount++] = speciesId;
        }
    };
    collect(gTeamMissing, TEAM_CACHE_CAP);
    collect(gFrameMissing, FRAME_MISSING_CAP);
    gStats.missingSpecies = uniqueCount;
    gStats.firstMissingSpecies = uniqueCount > 0 ? unique[0] : 0;
}

void noteMissingSpecies(uint16_t speciesId) {
    if (speciesId == 0) return;
    bool teamSpecies = false;
    for (uint8_t i = 0; i < gTeamCount; ++i) {
        if (gTeamSignature[i] == speciesId) {
            gTeamMissing[i] = speciesId;
            teamSpecies = true;
        }
    }
    if (!teamSpecies) appendUniqueSpecies(gFrameMissing, FRAME_MISSING_CAP, speciesId);
    refreshMissingStats();
}

void releaseEntry(CachedSpecies& entry) {
    if (entry.payload) {
        uint32_t decodedBytes = entry.payloadBytes +
                                static_cast<uint32_t>(entry.frameCount) * sizeof(SpriteFrame);
        gStats.decodedBytes = gStats.decodedBytes >= decodedBytes
            ? gStats.decodedBytes - decodedBytes : 0;
        gStats.compressedBytes = gStats.compressedBytes >= entry.packedBytes
            ? gStats.compressedBytes - entry.packedBytes : 0;
        if (gStats.cachedSpecies > 0) --gStats.cachedSpecies;
        free(entry.payload);
    }
    if (entry.frames) free(entry.frames);
    entry = CachedSpecies{};
}

void freeCache() {
    for (auto& entry : gCache) {
        releaseEntry(entry);
    }
    uint32_t reloadCount = gStats.reloadCount;
    gStats = SpriteCacheStats{};
    gStats.reloadCount = reloadCount;
    gStats.cachedSpecies = 0;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
    memset(gKnownMissingFiles, 0, sizeof(gKnownMissingFiles));
    memset(gTeamMissing, 0, sizeof(gTeamMissing));
    memset(gFrameMissing, 0, sizeof(gFrameMissing));
    memset(gKnownMissingFrames, 0, sizeof(gKnownMissingFrames));
    gDynamicSlot = TEAM_CACHE_CAP;
}

bool knownMissingFile(uint16_t speciesId) {
    if (speciesId == 0) return true;
    for (uint8_t i = 0; i < KNOWN_MISSING_FILE_CAP; ++i) {
        if (gKnownMissingFiles[i] == speciesId) return true;
    }
    return false;
}

bool rememberMissingFile(uint16_t speciesId) {
    if (speciesId == 0 || knownMissingFile(speciesId)) return false;
    for (uint8_t i = 0; i < KNOWN_MISSING_FILE_CAP; ++i) {
        if (gKnownMissingFiles[i] == 0) {
            gKnownMissingFiles[i] = speciesId;
            return true;
        }
    }
    return false;
}

bool rememberMissingFrame(uint16_t speciesId, uint16_t kind) {
    for (uint8_t i = 0; i < KNOWN_MISSING_FRAME_CAP; ++i) {
        const MissingFrameKey& key = gKnownMissingFrames[i];
        if (key.speciesId == speciesId && key.kind == kind) return false;
        if (key.speciesId == 0) {
            gKnownMissingFrames[i].speciesId = speciesId;
            gKnownMissingFrames[i].kind = kind;
            return true;
        }
    }
    return false;
}

CachedSpecies* cachedSpeciesFor(uint16_t speciesId) {
    for (auto& entry : gCache) {
        if (entry.speciesId == speciesId && entry.data) return &entry;
    }
    return nullptr;
}

bool readExact(fs::File& file, void* out, size_t length) {
    if (length == 0) return true;
    return file.read(reinterpret_cast<uint8_t*>(out), length) == length;
}

bool validPackedFrame(const PackedSpriteFrame& frame, const PackedSpriteHeader& header) {
    if (frame.width == 0 || frame.height == 0 || frame.length == 0) return false;
    if (frame.format != static_cast<uint8_t>(SpriteFormat::RGB565_RLE) &&
        frame.format != static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        return false;
    }
    if (frame.offset > header.rleWords || frame.length > header.rleWords - frame.offset) return false;
    if (frame.format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        if (frame.paletteSize == 0 || frame.paletteSize > 16) return false;
        if (frame.paletteOffset > header.paletteWords ||
            frame.paletteSize > header.paletteWords - frame.paletteOffset) {
            return false;
        }
    }
    return true;
}

bool loadSpeciesFromResourcePack(uint8_t slot, uint16_t speciesId) {
    ResourcePack& pack = ResourcePack::ins();
    if (!pack.begin()) return false;

    fs::File file;
    if (!pack.openSpriteBlock(speciesId, file)) return false;

    PackedSpriteHeader header = {};
    if (!readExact(file, &header, sizeof(header))) return false;
    if (header.magic != SPRITE_PACK_MAGIC ||
        header.version != SPRITE_PACK_VERSION ||
        header.speciesId != speciesId ||
        header.frameCount == 0 ||
        header.frameCount > MAX_PACK_FRAMES ||
        header.rleWords == 0 ||
        header.flags != SPRITE_PACK_FLAG_RAW_DEFLATE ||
        header.payloadRawBytes == 0 ||
        header.payloadRawBytes > MAX_PACK_PAYLOAD_BYTES ||
        header.payloadCompressedBytes == 0) {
        return false;
    }

    uint32_t frameBytes = static_cast<uint32_t>(header.frameCount) * sizeof(SpriteFrame);
    uint32_t packedFrameBytes = static_cast<uint32_t>(header.frameCount) * sizeof(PackedSpriteFrame);
    uint32_t expectedPayloadBytes = packedFrameBytes +
        (static_cast<uint32_t>(header.rleWords) + header.paletteWords) * sizeof(uint16_t);
    uint32_t fileBytes = static_cast<uint32_t>(file.size());
    if (expectedPayloadBytes != header.payloadRawBytes ||
        static_cast<uint64_t>(sizeof(PackedSpriteHeader)) + header.payloadCompressedBytes != file.size()) {
        return false;
    }

    SpriteFrame* frames = psramFound()
        ? static_cast<SpriteFrame*>(ps_malloc(frameBytes))
        : static_cast<SpriteFrame*>(malloc(frameBytes));
    uint8_t* payload = psramFound()
        ? static_cast<uint8_t*>(ps_malloc(header.payloadRawBytes))
        : static_cast<uint8_t*>(malloc(header.payloadRawBytes));
    if (!frames || !payload) {
        if (frames) free(frames);
        if (payload) free(payload);
        return false;
    }

    DeflateDecoder::Stats decodeStats{};
    if (!DeflateDecoder::inflateFile(file,
                                     header.payloadCompressedBytes,
                                     payload,
                                     header.payloadRawBytes,
                                     header.payloadCrc32,
                                     &decodeStats)) {
        free(frames);
        free(payload);
        Serial.printf(
            "[PokemonSprites] decode failed species=%u read=%u inflate=%u total=%u\\n",
            speciesId, decodeStats.readMs, decodeStats.inflateMs, decodeStats.totalMs);
        return false;
    }

    const auto* packedFrames = reinterpret_cast<const PackedSpriteFrame*>(payload);
    for (uint16_t i = 0; i < header.frameCount; ++i) {
        const PackedSpriteFrame& packed = packedFrames[i];
        if (!validPackedFrame(packed, header)) {
            free(frames);
            free(payload);
            return false;
        }
        frames[i] = SpriteFrame{
            speciesId,
            packed.kind,
            packed.width,
            packed.height,
            packed.format,
            packed.paletteSize,
            SPRITE_SOURCE_FILE_BLOCK,
            static_cast<uint8_t>(
                (packed.reserved & 0xFF00U) == SPRITE_FRAME_GROUND_MARKER
                    ? (packed.reserved & 0x00FFU) + 1U
                    : 0U),
            packed.offset,
            packed.length,
            packed.paletteOffset,
        };
    }

    uint16_t* decoded = reinterpret_cast<uint16_t*>(payload + packedFrameBytes);

    CachedSpecies& entry = gCache[slot];
    releaseEntry(entry);
    entry.speciesId = speciesId;
    entry.rleWords = header.rleWords;
    entry.paletteWords = header.paletteWords;
    entry.frameCount = header.frameCount;
    entry.frames = frames;
    entry.payload = payload;
    entry.payloadBytes = header.payloadRawBytes;
    entry.packedBytes = fileBytes;
    entry.data = decoded;
    entry.palettes = decoded + header.rleWords;

    if (gStats.cachedSpecies < 0xFF) ++gStats.cachedSpecies;
    gStats.decodedBytes += header.payloadRawBytes + frameBytes;
    gStats.compressedBytes += fileBytes;
    Serial.printf(
        "[PokemonSprites] source=littlefs species=%u frames=%u compressed=%u decoded=%u read=%u inflate=%u total=%u\\n",
        speciesId, header.frameCount, fileBytes, header.payloadRawBytes + frameBytes,
        decodeStats.readMs, decodeStats.inflateMs, decodeStats.totalMs);
    return true;
}

bool loadSpeciesIntoCache(uint8_t slot, uint16_t speciesId) {
    if (slot >= CACHE_CAP || speciesId == 0) return false;
    if (loadSpeciesFromResourcePack(slot, speciesId)) {
        if (slot < TEAM_CACHE_CAP) gTeamMissing[slot] = 0;
        refreshMissingStats();
        return true;
    }
    noteMissingSpecies(speciesId);
    if (rememberMissingFile(speciesId)) {
        Serial.printf("[PokemonSprites] missing species=%u\\n", speciesId);
    }
    return false;
}

uint8_t dynamicCacheSlot() {
    for (uint8_t i = TEAM_CACHE_CAP; i < CACHE_CAP; ++i) {
        if (gCache[i].speciesId == 0) return i;
    }
    if (gDynamicSlot < TEAM_CACHE_CAP || gDynamicSlot >= CACHE_CAP) gDynamicSlot = TEAM_CACHE_CAP;
    uint8_t slot = gDynamicSlot++;
    if (gDynamicSlot >= CACHE_CAP) gDynamicSlot = TEAM_CACHE_CAP;
    return slot;
}

CachedSpecies* ensureSpeciesLoaded(uint16_t speciesId) {
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (cached) return cached;
    if (!gDynamicLoadingEnabled) return nullptr;
    if (knownMissingFile(speciesId)) {
        noteMissingSpecies(speciesId);
        return nullptr;
    }

    uint8_t slot = dynamicCacheSlot();
    if (!loadSpeciesIntoCache(slot, speciesId)) return nullptr;
    return cachedSpeciesFor(speciesId);
}

const SpriteFrame* findFrame(CachedSpecies* cached, SpriteKind kind) {
    if (!cached || !cached->frames) return nullptr;
    uint16_t wantedKind = static_cast<uint16_t>(kind);
    for (uint16_t i = 0; i < cached->frameCount; ++i) {
        if (cached->frames[i].kind == wantedKind) return &cached->frames[i];
    }
    return nullptr;
}
}

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind) {
    CachedSpecies* cached = ensureSpeciesLoaded(speciesId);
    const SpriteFrame* frame = findFrame(cached, kind);
    if (frame) return frame;
    if (!cached || speciesId == 0) return nullptr;
    noteMissingSpecies(speciesId);
    uint16_t kindValue = static_cast<uint16_t>(kind);
    if (rememberMissingFrame(speciesId, kindValue)) {
        Serial.printf("[PokemonSprites] missing frame species=%u kind=%u\\n",
                      speciesId, static_cast<unsigned>(kindValue));
    }
    return nullptr;
}

int16_t frameGroundOffsetY(const SpriteFrame* frame) {
    if (!frame) return 0;
    uint8_t height = pgm_read_byte(&frame->height);
    uint8_t encodedPadding = pgm_read_byte(&frame->groundPaddingPlusOne);
    if (encodedPadding > 0) {
        int bottomPadding = encodedPadding - 1;
        return static_cast<int16_t>(height / 2 - bottomPadding);
    }
    return static_cast<int16_t>(constrain((int)(height * 0.42f), 16, 32));
}

bool syncTeamCache(const uint16_t* speciesIds, uint8_t count, uint8_t loadBudget) {
    if (!speciesIds) count = 0;
    if (count > TEAM_CACHE_CAP) count = TEAM_CACHE_CAP;

    uint16_t next[CACHE_CAP] = {};
    for (uint8_t i = 0; i < count; ++i) next[i] = speciesIds[i];
    bool signatureChanged = count != gTeamCount;
    if (count == gTeamCount) {
        for (uint8_t i = 0; i < CACHE_CAP; ++i) {
            if (next[i] != gTeamSignature[i]) {
                signatureChanged = true;
                break;
            }
        }
    }

    uint32_t start = millis();
    if (signatureChanged) {
        freeCache();
        gTeamCount = count;
        for (uint8_t i = 0; i < CACHE_CAP; ++i) gTeamSignature[i] = next[i];
        ++gStats.reloadCount;
    }

    uint8_t loadedThisCall = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (next[i] == 0 || gCache[i].speciesId == next[i]) continue;
        if (loadedThisCall >= loadBudget) continue;
        if (!loadSpeciesIntoCache(i, next[i])) {
            Serial.printf("[PokemonSprites] cache miss species=%u\\n", next[i]);
        }
        ++loadedThisCall;
    }

    bool ready = true;
    for (uint8_t i = 0; i < count; ++i) {
        if (next[i] != 0 && gCache[i].speciesId != next[i]) {
            ready = false;
            break;
        }
    }
    if (!signatureChanged && loadedThisCall == 0) return ready;

    gStats.lastReloadMs = millis() - start;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
    Serial.printf(
        "[PokemonSprites] cache sync species=%u,%u loaded=%u ready=%u cached=%u missing=%u decoded=%u compressed=%u ms=%u psram=%u free=%u\\n",
        gTeamSignature[0], gTeamSignature[1], loadedThisCall, ready ? 1 : 0,
        gStats.cachedSpecies, gStats.missingSpecies,
        gStats.decodedBytes, gStats.compressedBytes, gStats.lastReloadMs,
        gStats.psram ? 1 : 0, gStats.freePsram);
    return ready;
}

void setDynamicLoadingEnabled(bool enabled) {
    gDynamicLoadingEnabled = enabled;
}

void beginRenderFrame() {
    memset(gFrameMissing, 0, sizeof(gFrameMissing));
    refreshMissingStats();
}

const SpriteCacheStats& cacheStats() {
    return gStats;
}

bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX) {
    if (!frame) return false;

    uint8_t width = pgm_read_byte(&frame->width);
    uint8_t height = pgm_read_byte(&frame->height);
    uint32_t offset = pgm_read_dword(&frame->offset);
    uint32_t length = pgm_read_dword(&frame->length);
    if (width == 0 || height == 0 || length == 0) return false;

    uint8_t format = pgm_read_byte(&frame->format);
    uint8_t source = pgm_read_byte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = pgm_read_word(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;
    const uint16_t* rle = cached->data;
    const uint16_t* palettes = cached->palettes;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = pgm_read_dword(&frame->paletteOffset);
        uint8_t paletteSize = pgm_read_byte(&frame->paletteSize);
        if (paletteOffset + paletteSize > cached->paletteWords) return false;
        PixelRenderer::drawIndexed4Rle(
            x, y, width, height, rle, offset, length, palettes,
            paletteOffset,
            paletteSize,
            flipX);
        return true;
    }

    PixelRenderer::drawRgb565Rle(x, y, width, height, rle, offset, length, flipX);
    return true;
}

bool drawFrameScaled(const SpriteFrame* frame, int x, int y, float scale, bool flipX) {
    if (!frame) return false;
    if (scale == 1.0f) return drawFrame(frame, x, y, flipX);

    uint8_t width = pgm_read_byte(&frame->width);
    uint8_t height = pgm_read_byte(&frame->height);
    uint32_t offset = pgm_read_dword(&frame->offset);
    uint32_t length = pgm_read_dword(&frame->length);
    if (width == 0 || height == 0 || length == 0 || scale <= 0.0f) return false;

    uint8_t format = pgm_read_byte(&frame->format);
    uint8_t source = pgm_read_byte(&frame->source);
    if (source != SPRITE_SOURCE_FILE_BLOCK) return false;

    uint16_t speciesId = pgm_read_word(&frame->speciesId);
    CachedSpecies* cached = cachedSpeciesFor(speciesId);
    if (!cached || offset + length > cached->rleWords) return false;
    const uint16_t* rle = cached->data;
    const uint16_t* palettes = cached->palettes;

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = pgm_read_dword(&frame->paletteOffset);
        uint8_t paletteSize = pgm_read_byte(&frame->paletteSize);
        if (paletteOffset + paletteSize > cached->paletteWords) return false;
        PixelRenderer::drawIndexed4RleScaled(
            x, y, width, height, rle, offset, length, palettes,
            paletteOffset,
            paletteSize,
            scale,
            flipX);
        return true;
    }

    PixelRenderer::drawRgb565RleScaled(x, y, width, height, rle, offset, length, scale, flipX);
    return true;
}

}  // namespace PokemonSprites
"""
    walking_config = f"""
struct WalkingConfig {{
    uint16_t speciesId;
    SpriteKind downBase;
    SpriteKind leftBase;
    SpriteKind upBase;
    SpriteKind rightBase;
    uint8_t frameCount;
    bool rightFlipX;
}};

static constexpr WalkingConfig WALKING_CONFIGS[] = {{
{walking_table}
}};
"""
    cpp_text = cpp_text.replace(
        "namespace {\nstatic constexpr uint8_t SPRITE_SOURCE_FILE_BLOCK",
        "namespace {\n" + walking_config + "\nstatic constexpr uint8_t SPRITE_SOURCE_FILE_BLOCK",
        1,
    )
    walking_function = """
bool walkingAnimation(uint16_t speciesId, WalkDirection direction, WalkingAnimation& animation) {
    for (const auto& config : WALKING_CONFIGS) {
        if (config.speciesId != speciesId) continue;
        animation.frameCount = config.frameCount;
        animation.flipX = false;
        switch (direction) {
        case WalkDirection::DOWN: animation.base = config.downBase; break;
        case WalkDirection::LEFT: animation.base = config.leftBase; break;
        case WalkDirection::UP: animation.base = config.upBase; break;
        case WalkDirection::RIGHT:
            animation.base = config.rightBase;
            animation.flipX = config.rightFlipX;
            break;
        }
        return animation.frameCount > 0;
    }
    return false;
}

"""
    cpp_text = cpp_text.replace(
        "const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind)",
        walking_function + "const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind)",
        1,
    )
    OUT_CPP.write_text(cpp_text, encoding="utf-8")

    pack_bytes = sum(path.stat().st_size for path in PACK_SPRITE_OUT.glob("*.smonsp"))
    print(
        f"species={len(species_rows())} kinds={len(kind_names)} "
        f"pack_sprites={pack_sprite_count} pack_bytes={pack_bytes}"
    )


if __name__ == "__main__":
    main()
