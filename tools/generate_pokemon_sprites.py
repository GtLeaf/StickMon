#!/usr/bin/env python3
from dataclasses import dataclass
from pathlib import Path
import os
import re
import zlib
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
SPRITE_SOURCE_GLOBAL = 0
SPRITE_SOURCE_SPECIES_BLOCK = 1

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
    PmdSpec(148, "DRAGONAIR", "dragonair", 1, 3, 2),
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

    def add_frame(self, species_id, ident, kind, image, source=SPRITE_SOURCE_GLOBAL):
        fmt, palette_size, offset, length, palette_offset = self.encode(image)
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
        })


def add_base_frames(writer, species_id, ident, missing, include_back=True):
    icon_path = GRAPHICS / "Icons" / f"{ident}.png"
    if not icon_path.exists():
        missing.append(str(icon_path))
        return

    icon = to_rgba(icon_path)
    writer.add_frame(species_id, ident, "ICON_0", icon.crop((0, 0, ICON_SIZE, ICON_SIZE)))

    spec = PMD_BY_ID.get(species_id)
    if spec and (PROCESSED / spec.slug).exists():
        front = to_rgba(pmd_path(spec, "walking", "front_0"))
        writer.add_frame(species_id, ident, "FRONT", front)
        if include_back:
            add_pmd_back_frame(writer, spec)
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
    if include_back:
        writer.add_frame(species_id, ident, "BACK", resize_nearest(to_rgba(back_path), BATTLE_SIZE))


def add_pmd_back_frame(writer, spec):
    writer.add_frame(
        spec.species_id, spec.ident,
        "BACK",
        to_rgba(pmd_path(spec, "walking", "back_0")),
        SPRITE_SOURCE_SPECIES_BLOCK,
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


def format_words(values):
    rows = []
    for i in range(0, len(values), 12):
        rows.append("    " + ", ".join(f"0x{v:04X}" for v in values[i:i + 12]) + ",")
    return "\n".join(rows)


def format_bytes(values):
    rows = []
    for i in range(0, len(values), 16):
        rows.append("    " + ", ".join(f"0x{v:02X}" for v in values[i:i + 16]) + ",")
    return "\n".join(rows)


def raw_deflate(data):
    compressor = zlib.compressobj(6, zlib.DEFLATED, -15)
    return compressor.compress(data) + compressor.flush()


def words_to_bytes(words):
    out = bytearray()
    for value in words:
        out.append(value & 0xFF)
        out.append((value >> 8) & 0xFF)
    return bytes(out)


def main():
    base_writer = AssetWriter()
    missing = []
    frames = []
    compressed_blocks = []
    compressed_data = bytearray()

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
        spec = PMD_BY_ID.get(species_id)
        before = len(base_writer.frames)
        add_base_frames(
            base_writer, species_id, ident, missing,
            include_back=not (spec and (PROCESSED / spec.slug).exists()),
        )
        frames.extend(base_writer.frames[before:])
        if spec and (PROCESSED / spec.slug).exists():
            action_writer = AssetWriter()
            add_pmd_back_frame(action_writer, spec)
            add_pmd_frames(action_writer, spec)
            frames.extend(action_writer.frames)
            payload = words_to_bytes(action_writer.data) + words_to_bytes(action_writer.palettes)
            compressed = raw_deflate(payload)
            compressed_blocks.append({
                "species_id": spec.species_id,
                "ident": spec.ident,
                "offset": len(compressed_data),
                "length": len(compressed),
                "rle_words": len(action_writer.data),
                "palette_words": len(action_writer.palettes),
            })
            compressed_data.extend(compressed)

    for spec in PMD_SPECS:
        if spec.species_id in present_species_ids or not (PROCESSED / spec.slug).exists():
            continue
        before = len(base_writer.frames)
        add_base_frames(base_writer, spec.species_id, spec.ident, missing, include_back=False)
        frames.extend(base_writer.frames[before:])
        action_writer = AssetWriter()
        add_pmd_back_frame(action_writer, spec)
        add_pmd_frames(action_writer, spec)
        frames.extend(action_writer.frames)
        payload = words_to_bytes(action_writer.data) + words_to_bytes(action_writer.palettes)
        compressed = raw_deflate(payload)
        compressed_blocks.append({
            "species_id": spec.species_id,
            "ident": spec.ident,
            "offset": len(compressed_data),
            "length": len(compressed),
            "rle_words": len(action_writer.data),
            "palette_words": len(action_writer.palettes),
        })
        compressed_data.extend(compressed)

    if missing:
        raise SystemExit("Missing Pokemon sprite source files:\n" + "\n".join(missing))

    egg_path = GRAPHICS / "Eggs" / "000.png"
    egg_image = resize_nearest(to_rgba(egg_path), EGG_SIZE)
    egg_format, egg_palette_size, egg_offset, egg_length, egg_palette_offset = base_writer.encode(egg_image)

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
    uint8_t source;
    uint8_t reserved;
    uint32_t offset;
    uint32_t length;
    uint32_t paletteOffset;
}};

struct CompressedSpeciesBlock {{
    uint16_t speciesId;
    uint16_t rleWords;
    uint16_t paletteWords;
    uint16_t reserved;
    uint32_t offset;
    uint32_t length;
}};

struct SpriteCacheStats {{
    uint8_t cachedSpecies;
    uint32_t reloadCount;
    uint32_t lastReloadMs;
    uint32_t decodedBytes;
    uint32_t compressedBytes;
    uint32_t freePsram;
    bool psram;
}};

extern const uint16_t SPRITE_FRAME_COUNT;
extern const SpriteFrame SPRITE_FRAMES[] PROGMEM;
extern const SpriteFrame EGG_FRAME PROGMEM;
extern const uint16_t SPRITE_RLE[] PROGMEM;
extern const uint16_t SPRITE_PALETTES[] PROGMEM;
extern const CompressedSpeciesBlock SPRITE_COMPRESSED_BLOCKS[] PROGMEM;
extern const uint16_t SPRITE_COMPRESSED_BLOCK_COUNT;
extern const uint8_t SPRITE_COMPRESSED_DATA[] PROGMEM;

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind);
void syncTeamCache(const uint16_t* speciesIds, uint8_t count);
const SpriteCacheStats& cacheStats();
bool drawFrame(const SpriteFrame* frame, int x, int y, bool flipX = false);

}}  // namespace PokemonSprites
"""
    OUT_H.write_text(h_text, encoding="utf-8")

    frame_rows = []
    for frame in frames:
        frame_rows.append(
            "    "
            f"{{{frame['species_id']}, {kind_map[frame['kind']]}, "
            f"{frame['width']}, {frame['height']}, {frame['format']}, {frame['palette_size']}, "
            f"{frame['source']}, 0, "
            f"{frame['offset']}, {frame['length']}, {frame['palette_offset']}}}, "
            f"// {frame['ident']} {frame['kind']}"
        )

    block_rows = []
    for block in compressed_blocks:
        block_rows.append(
            "    "
            f"{{{block['species_id']}, {block['rle_words']}, {block['palette_words']}, 0, "
            f"{block['offset']}, {block['length']}}}, "
            f"// {block['ident']}"
        )

    cpp_text = """#include "assets/PokemonSprites.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include "hardware/PixelRenderer.h"
extern "C" {
#include "third_party/uzlib/uzlib.h"
}

namespace PokemonSprites {

const uint16_t SPRITE_FRAME_COUNT = %d;

const SpriteFrame SPRITE_FRAMES[] PROGMEM = {
%s
};

const SpriteFrame EGG_FRAME PROGMEM = {0, 0, %d, %d, %d, %d, %d, %d, %d, %d, %d};

const uint16_t SPRITE_RLE[] PROGMEM = {
%s
};

const uint16_t SPRITE_PALETTES[] PROGMEM = {
%s
};

const uint16_t SPRITE_COMPRESSED_BLOCK_COUNT = %d;

const CompressedSpeciesBlock SPRITE_COMPRESSED_BLOCKS[] PROGMEM = {
%s
};

const uint8_t SPRITE_COMPRESSED_DATA[] PROGMEM = {
%s
};

namespace {
static constexpr uint8_t SPRITE_SOURCE_GLOBAL = %d;
static constexpr uint8_t SPRITE_SOURCE_SPECIES_BLOCK = %d;
static constexpr uint8_t CACHE_CAP = 2;

struct CachedSpecies {
    uint16_t speciesId = 0;
    uint16_t rleWords = 0;
    uint16_t paletteWords = 0;
    uint16_t* data = nullptr;
    uint16_t* palettes = nullptr;
};

CachedSpecies gCache[CACHE_CAP];
SpriteCacheStats gStats = {};
uint16_t gTeamSignature[CACHE_CAP] = {};
uint8_t gTeamCount = 0;

void freeCache() {
    for (auto& entry : gCache) {
        if (entry.data) free(entry.data);
        entry = CachedSpecies{};
    }
    gStats.cachedSpecies = 0;
    gStats.decodedBytes = 0;
    gStats.compressedBytes = 0;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
}

const CompressedSpeciesBlock* compressedBlockFor(uint16_t speciesId) {
    for (uint16_t i = 0; i < SPRITE_COMPRESSED_BLOCK_COUNT; ++i) {
        if (pgm_read_word(&SPRITE_COMPRESSED_BLOCKS[i].speciesId) == speciesId) {
            return &SPRITE_COMPRESSED_BLOCKS[i];
        }
    }
    return nullptr;
}

CachedSpecies* cachedSpeciesFor(uint16_t speciesId) {
    for (auto& entry : gCache) {
        if (entry.speciesId == speciesId && entry.data) return &entry;
    }
    return nullptr;
}

bool inflateRawDeflate(const uint8_t* compressed, uint32_t compressedSize, uint8_t* out, uint32_t outSize) {
    TINF_DATA d;
    memset(&d, 0, sizeof(d));
    uzlib_init();
    uzlib_uncompress_init(&d, nullptr, 0);
    d.source = compressed;
    d.source_limit = compressed + compressedSize;
    d.dest_start = out;
    d.dest = out;
    d.dest_limit = out + outSize;

    int result = TINF_OK;
    while (d.dest < d.dest_limit) {
        result = uzlib_uncompress(&d);
        if (result == TINF_DONE) break;
        if (result != TINF_OK) return false;
    }
    return result == TINF_DONE || d.dest == d.dest_limit;
}

bool loadSpeciesIntoCache(uint8_t slot, uint16_t speciesId) {
    if (slot >= CACHE_CAP) return false;
    const CompressedSpeciesBlock* block = compressedBlockFor(speciesId);
    if (!block) return false;

    uint16_t rleWords = pgm_read_word(&block->rleWords);
    uint16_t paletteWords = pgm_read_word(&block->paletteWords);
    uint32_t compressedOffset = pgm_read_dword(&block->offset);
    uint32_t compressedSize = pgm_read_dword(&block->length);
    uint32_t decodedBytes = ((uint32_t)rleWords + paletteWords) * sizeof(uint16_t);
    if (decodedBytes == 0 || compressedSize == 0) return false;

    uint8_t* compressed = psramFound()
        ? (uint8_t*)ps_malloc(compressedSize)
        : (uint8_t*)malloc(compressedSize);
    uint16_t* decoded = psramFound()
        ? (uint16_t*)ps_malloc(decodedBytes)
        : (uint16_t*)malloc(decodedBytes);
    if (!compressed || !decoded) {
        if (compressed) free(compressed);
        if (decoded) free(decoded);
        return false;
    }

    for (uint32_t i = 0; i < compressedSize; ++i) {
        compressed[i] = pgm_read_byte(&SPRITE_COMPRESSED_DATA[compressedOffset + i]);
    }

    bool ok = inflateRawDeflate(compressed, compressedSize, (uint8_t*)decoded, decodedBytes);
    free(compressed);
    if (!ok) {
        free(decoded);
        return false;
    }

    CachedSpecies& entry = gCache[slot];
    if (entry.data) free(entry.data);
    entry.speciesId = speciesId;
    entry.rleWords = rleWords;
    entry.paletteWords = paletteWords;
    entry.data = decoded;
    entry.palettes = decoded + rleWords;

    gStats.cachedSpecies++;
    gStats.decodedBytes += decodedBytes;
    gStats.compressedBytes += compressedSize;
    return true;
}
}

const SpriteFrame* findSpeciesSprite(uint16_t speciesId, SpriteKind kind) {
    for (uint16_t i = 0; i < SPRITE_FRAME_COUNT; ++i) {
        if (pgm_read_word(&SPRITE_FRAMES[i].speciesId) == speciesId &&
            pgm_read_word(&SPRITE_FRAMES[i].kind) == static_cast<uint16_t>(kind)) {
            return &SPRITE_FRAMES[i];
        }
    }
    return nullptr;
}

void syncTeamCache(const uint16_t* speciesIds, uint8_t count) {
    if (!speciesIds) count = 0;
    if (count > CACHE_CAP) count = CACHE_CAP;

    uint16_t next[CACHE_CAP] = {};
    for (uint8_t i = 0; i < count; ++i) next[i] = speciesIds[i];
    if (count == gTeamCount) {
        bool same = true;
        for (uint8_t i = 0; i < CACHE_CAP; ++i) {
            if (next[i] != gTeamSignature[i]) {
                same = false;
                break;
            }
        }
        if (same) return;
    }

    uint32_t start = millis();
    freeCache();
    gTeamCount = count;
    for (uint8_t i = 0; i < CACHE_CAP; ++i) gTeamSignature[i] = next[i];
    for (uint8_t i = 0; i < count; ++i) {
        if (next[i] == 0) continue;
        if (!loadSpeciesIntoCache(i, next[i])) {
            Serial.printf("[PokemonSprites] cache miss species=%%u\\n", next[i]);
        }
    }
    gStats.reloadCount++;
    gStats.lastReloadMs = millis() - start;
    gStats.freePsram = ESP.getFreePsram();
    gStats.psram = psramFound();
    Serial.printf(
        "[PokemonSprites] cache reload species=%%u,%%u cached=%%u decoded=%%u compressed=%%u ms=%%u psram=%%u free=%%u\\n",
        gTeamSignature[0], gTeamSignature[1], gStats.cachedSpecies,
        gStats.decodedBytes, gStats.compressedBytes, gStats.lastReloadMs,
        gStats.psram ? 1 : 0, gStats.freePsram);
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
    const uint16_t* rle = SPRITE_RLE;
    const uint16_t* palettes = SPRITE_PALETTES;
    if (source == SPRITE_SOURCE_SPECIES_BLOCK) {
        uint16_t speciesId = pgm_read_word(&frame->speciesId);
        CachedSpecies* cached = cachedSpeciesFor(speciesId);
        if (!cached || offset + length > cached->rleWords) return false;
        rle = cached->data;
        palettes = cached->palettes;
    }

    if (format == static_cast<uint8_t>(SpriteFormat::INDEXED4_RLE)) {
        uint32_t paletteOffset = pgm_read_dword(&frame->paletteOffset);
        uint8_t paletteSize = pgm_read_byte(&frame->paletteSize);
        if (source == SPRITE_SOURCE_SPECIES_BLOCK) {
            uint16_t speciesId = pgm_read_word(&frame->speciesId);
            CachedSpecies* cached = cachedSpeciesFor(speciesId);
            if (!cached || paletteOffset + paletteSize > cached->paletteWords) return false;
        }
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

}  // namespace PokemonSprites
""" % (
        len(frames),
        "\n".join(frame_rows),
        egg_image.width,
        egg_image.height,
        egg_format,
        egg_palette_size,
        SPRITE_SOURCE_GLOBAL,
        0,
        egg_offset,
        egg_length,
        egg_palette_offset,
        format_words(base_writer.data),
        format_words(base_writer.palettes),
        len(compressed_blocks),
        "\n".join(block_rows),
        format_bytes(compressed_data),
        SPRITE_SOURCE_GLOBAL,
        SPRITE_SOURCE_SPECIES_BLOCK,
    )
    OUT_CPP.write_text(cpp_text, encoding="utf-8")

    raw_pixels = sum(frame["width"] * frame["height"] for frame in frames)
    raw_pixels += egg_image.width * egg_image.height
    raw_bytes = raw_pixels * 2
    encoded_bytes = len(base_writer.data) * 2
    palette_bytes = len(base_writer.palettes) * 2
    total_bytes = encoded_bytes + palette_bytes + len(compressed_data)
    print(
        f"species={len(species_rows())} frames={len(frames)} "
        f"base_rle_words={len(base_writer.data)} base_palette_words={len(base_writer.palettes)} "
        f"compressed_blocks={len(compressed_blocks)} compressed_bytes={len(compressed_data)} "
        f"indexed4={base_writer.indexed4_count} rgb565={base_writer.rgb565_count}"
    )
    print(
        f"base_rle_duplicates={base_writer.rle_duplicate_count} "
        f"base_palette_duplicates={base_writer.palette_duplicate_count}"
    )
    print(f"raw_rgb565_bytes={raw_bytes} encoded_asset_bytes={total_bytes}")


if __name__ == "__main__":
    main()
