#!/usr/bin/env python3
import json
import os
import re
import struct
import zlib
from pathlib import Path

from PIL import Image

from generate_explore_map import autotile_variant, regular_tile


ROOT = Path(__file__).resolve().parents[1]
ESSENTIALS = Path(os.environ.get(
    "ESSENTIALS_DIR",
    "${ESSENTIALS_DIR}",
))
GRAPHICS = ESSENTIALS / "Graphics"
DATA_DIR = ROOT / "data"
PACK_OUT = DATA_DIR / "packs" / "dev"
OUTPUTS = {
    "ui": PACK_OUT / "game" / "ui.smonfx",
    "battle": PACK_OUT / "game" / "battle.smonfx",
    "map": PACK_OUT / "game" / "maps.smonfx",
    "hatch": PACK_OUT / "game" / "hatch.smonfx",
}
GENERATED_GAME_DIR = ROOT / "origin_asset" / "generated" / "game"
EGG_PREVIEW = GENERATED_GAME_DIR / "egg_32.png"
GAME_ASSETS_HEADER = ROOT / "src" / "assets" / "GameAssets.h"
GAME_ASSETS_SOURCE = ROOT / "src" / "assets" / "GameAssets.cpp"

MAGIC = 0x58464753  # SGFX
VERSION = 2
FLAG_RAW_DEFLATE = 1
FORMAT_INDEXED4_RLE = 1

ITEM_SIZE = 16
BALL_W = 32
BALL_H = 64
BACKGROUND_W = 240
BACKGROUND_H = 135
EGG_SIZE = 32

ITEMS = [
    ("ITEM_POKE_BALL", "POKEBALL.png"),
    ("ITEM_GREAT_BALL", "GREATBALL.png"),
    ("ITEM_HEAVY_BALL", "HEAVYBALL.png"),
    ("ITEM_TIMER_BALL", "TIMERBALL.png"),
    ("ITEM_NORMAL_FOOD", "ORANBERRY.png"),
    ("ITEM_POTION", "POTION.png"),
    ("ITEM_SUPER_POTION", "SUPERPOTION.png"),
    ("ITEM_ANTIDOTE", "ANTIDOTE.png"),
    ("ITEM_CANDY", "RARECANDY.png"),
]

BALLS = [
    ("POKE_BALL", "POKEBALL"),
    ("GREAT_BALL", "GREATBALL"),
    ("HEAVY_BALL", "HEAVYBALL"),
    ("TIMER_BALL", "TIMERBALL"),
]

BACKGROUNDS = [
    ("BATTLE_BG_GRASS", "field_bg.png"),
    ("BATTLE_BG_RIVERSIDE", "water_bg.png"),
    ("BATTLE_BG_DEEP_FOREST", "forest_bg.png"),
]

EXPLORE_TILES = [
    ("EXPLORE_TILE_0072", 72),
    ("EXPLORE_TILE_0144", 144),
    ("EXPLORE_TILE_0168", 168),
    ("EXPLORE_TILE_0385", 385),
    ("EXPLORE_TILE_0386", 386),
    ("EXPLORE_TILE_0387", 387),
    ("EXPLORE_TILE_0388", 388),
    ("EXPLORE_TILE_0389", 389),
    ("EXPLORE_TILE_0390", 390),
    ("EXPLORE_TILE_0415", 415),
    ("EXPLORE_TILE_0537", 537),
    ("EXPLORE_TILE_0538", 538),
    ("EXPLORE_TILE_0539", 539),
    ("EXPLORE_TILE_0540", 540),
    ("EXPLORE_TILE_0542", 542),
    ("EXPLORE_TILE_0545", 545),
    ("EXPLORE_TILE_0546", 546),
    ("EXPLORE_TILE_0547", 547),
    ("EXPLORE_TILE_0553", 553),
    ("EXPLORE_TILE_0554", 554),
    ("EXPLORE_TILE_0555", 555),
    ("EXPLORE_TILE_0556", 556),
    ("EXPLORE_TILE_0558", 558),
    ("EXPLORE_TILE_0800", 800),
    ("EXPLORE_TILE_0801", 801),
    ("EXPLORE_TILE_0802", 802),
    ("EXPLORE_TILE_0804", 804),
    ("EXPLORE_TILE_0805", 805),
    ("EXPLORE_TILE_0808", 808),
    ("EXPLORE_TILE_0809", 809),
    ("EXPLORE_TILE_0810", 810),
    ("EXPLORE_TILE_0811", 811),
    ("EXPLORE_TILE_0818", 818),
    ("EXPLORE_TILE_0819", 819),
    ("EXPLORE_TILE_1662", 1662),
    ("EXPLORE_TILE_1665", 1665),
    ("EXPLORE_TILE_1681", 1681),
    ("EXPLORE_TILE_1682", 1682),
]

OUTSIDE_AUTOTILES = (
    "Sea",
    "Sea without shore",
    "Sea deep",
    "Sand shore",
    "Flowers1",
    "Water rock",
    "Fountain1",
)

KIND_ORDER = [kind for kind, _ in ITEMS]
for kind_prefix, _ in BALLS:
    KIND_ORDER.extend(f"BALL_{kind_prefix}_{index}" for index in range(8))
    KIND_ORDER.append(f"BALL_{kind_prefix}_OPEN")
KIND_ORDER.append("BALL_BURST_STAR")
KIND_ORDER.extend(kind for kind, _ in BACKGROUNDS)
KIND_ORDER.extend(kind for kind, _ in EXPLORE_TILES)
KIND_ORDER.append("EGG")
KIND_IDS = {kind: index for index, kind in enumerate(KIND_ORDER)}


def validate_kind_order():
    source = GAME_ASSETS_HEADER.read_text(encoding="utf-8")
    match = re.search(r"enum class Kind\s*:\s*uint16_t\s*\{(.*?)\bCOUNT\s*,", source, re.S)
    if not match:
        raise ValueError(f"unable to parse Kind enum: {GAME_ASSETS_HEADER}")
    header_order = re.findall(r"^\s*([A-Z][A-Z0-9_]*)\s*,", match.group(1), re.M)
    if header_order != KIND_ORDER:
        raise ValueError("GameAssets::Kind order does not match generated pack order")


def validate_explore_tile_mapping():
    source = GAME_ASSETS_SOURCE.read_text(encoding="utf-8")
    mapping = {
        int(tile_id): kind
        for tile_id, kind in re.findall(
            r"case\s+(\d+):\s+kind\s*=\s*Kind::(EXPLORE_TILE_\d+);",
            source,
        )
    }
    expected = {tile_id: kind for kind, tile_id in EXPLORE_TILES}
    if mapping != expected:
        raise ValueError("GameAssets::drawExploreTile mapping does not match EXPLORE_TILES")


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def quantize_rgba(image, colors):
    rgba = image.convert("RGBA")
    quantized = rgba.convert("RGB").quantize(
        colors=colors,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.NONE,
    ).convert("RGBA")
    quantized.putalpha(rgba.getchannel("A"))
    return quantized


def indexed4_rle(image):
    pixels = list(image.get_flattened_data()) if hasattr(image, "get_flattened_data") else list(image.getdata())
    palette = []
    palette_map = {}
    for r, g, b, a in pixels:
        if a <= 16:
            continue
        color = rgb565(r, g, b)
        if color not in palette_map:
            if len(palette) >= 16:
                raise ValueError("indexed image exceeds 16 colors")
            palette_map[color] = len(palette)
            palette.append(color)

    if not palette:
        raise ValueError("asset has no opaque pixels")

    out = []
    i = 0
    while i < len(pixels):
        transparent = pixels[i][3] <= 16
        if transparent:
            j = i + 1
            while j < len(pixels) and pixels[j][3] <= 16 and j - i < 0x7FFF:
                j += 1
            out.append(0x8000 | (j - i))
        else:
            j = i
            while j < len(pixels) and pixels[j][3] > 16 and j - i < 0x7FFF:
                j += 1
            indices = [palette_map[rgb565(r, g, b)] for r, g, b, _a in pixels[i:j]]
            out.append(len(indices))
            for base in range(0, len(indices), 4):
                packed = 0
                for nibble, index in enumerate(indices[base:base + 4]):
                    packed |= index << (nibble * 4)
                out.append(packed)
        i = j
    return out, palette


class Writer:
    def __init__(self):
        self.frames = []
        self.data = []
        self.palettes = []
        self.data_cache = {}
        self.palette_cache = {}

    def add(self, kind, image):
        encoded, palette = indexed4_rle(image)
        data_key = tuple(encoded)
        if data_key in self.data_cache:
            data_offset, data_length = self.data_cache[data_key]
        else:
            data_offset = len(self.data)
            data_length = len(encoded)
            self.data.extend(encoded)
            self.data_cache[data_key] = (data_offset, data_length)

        palette_key = tuple(palette)
        if palette_key in self.palette_cache:
            palette_offset = self.palette_cache[palette_key]
        else:
            palette_offset = len(self.palettes)
            self.palettes.extend(palette)
            self.palette_cache[palette_key] = palette_offset

        self.frames.append({
            "kind": kind,
            "width": image.width,
            "height": image.height,
            "format": FORMAT_INDEXED4_RLE,
            "palette_size": len(palette),
            "offset": data_offset,
            "length": data_length,
            "palette_offset": palette_offset,
        })


def load_rgba(path):
    if not path.exists():
        raise FileNotFoundError(path)
    return Image.open(path).convert("RGBA")


def prepare_egg(path):
    source = load_rgba(path)
    bbox = source.getchannel("A").getbbox()
    if not bbox:
        raise ValueError(f"egg asset has no opaque pixels: {path}")
    egg = source.crop(bbox)
    if egg.width > EGG_SIZE or egg.height > EGG_SIZE:
        scale = min(EGG_SIZE / egg.width, EGG_SIZE / egg.height)
        egg = egg.resize(
            (max(1, round(egg.width * scale)), max(1, round(egg.height * scale))),
            Image.Resampling.NEAREST,
        )
    canvas = Image.new("RGBA", (EGG_SIZE, EGG_SIZE), (0, 0, 0, 0))
    canvas.alpha_composite(egg, ((EGG_SIZE - egg.width) // 2, (EGG_SIZE - egg.height) // 2))
    return canvas


def prepare_explore_tile(tile_id, tileset, autotiles):
    if tile_id < 384:
        autotile_index = tile_id // 48 - 1
        if not 0 <= autotile_index < len(autotiles):
            raise ValueError(f"invalid explore autotile id: {tile_id}")
        tile = autotile_variant(autotiles[autotile_index], tile_id % 48)
    else:
        tile = regular_tile(tileset, tile_id)
    if tile is None or tile.size != (32, 32):
        raise ValueError(f"unable to render explore tile: {tile_id}")
    return tile.resize((26, 26), Image.Resampling.NEAREST)


def build_assets():
    writers = {
        "ui": Writer(),
        "battle": Writer(),
        "map": Writer(),
        "hatch": Writer(),
    }

    for kind, filename in ITEMS:
        image = load_rgba(GRAPHICS / "Items" / filename)
        image = image.resize((ITEM_SIZE, ITEM_SIZE), Image.Resampling.NEAREST)
        writers["ui"].add(kind, quantize_rgba(image, 15))

    for kind_prefix, filename in BALLS:
        sheet = load_rgba(GRAPHICS / "Battle animations" / f"ball_{filename}.png")
        if sheet.size != (BALL_W * 8, BALL_H):
            raise ValueError(f"unexpected ball sheet size: {sheet.size} {filename}")
        for index in range(8):
            frame = sheet.crop((index * BALL_W, 0, (index + 1) * BALL_W, BALL_H))
            writers["battle"].add(f"BALL_{kind_prefix}_{index}", quantize_rgba(frame, 15))

        opened = load_rgba(GRAPHICS / "Battle animations" / f"ball_{filename}_open.png")
        if opened.size != (BALL_W, BALL_H):
            raise ValueError(f"unexpected open ball size: {opened.size} {filename}")
        writers["battle"].add(f"BALL_{kind_prefix}_OPEN", quantize_rgba(opened, 15))

    burst = load_rgba(GRAPHICS / "Battle animations" / "ballBurst_star.png")
    burst = burst.resize((32, 32), Image.Resampling.NEAREST)
    writers["battle"].add("BALL_BURST_STAR", quantize_rgba(burst, 15))

    for kind, filename in BACKGROUNDS:
        image = load_rgba(GRAPHICS / "Battlebacks" / filename)
        image = image.resize((BACKGROUND_W, BACKGROUND_H), Image.Resampling.LANCZOS)
        writers["battle"].add(kind, quantize_rgba(image, 16))

    tileset = load_rgba(GRAPHICS / "Tilesets" / "Outside.png")
    autotiles = [load_rgba(GRAPHICS / "Autotiles" / f"{name}.png") for name in OUTSIDE_AUTOTILES]
    for kind, tile_id in EXPLORE_TILES:
        tile = prepare_explore_tile(tile_id, tileset, autotiles)
        writers["map"].add(kind, quantize_rgba(tile, 16))

    egg = prepare_egg(GRAPHICS / "Pokemon" / "Eggs" / "000.png")
    GENERATED_GAME_DIR.mkdir(parents=True, exist_ok=True)
    egg.save(EGG_PREVIEW)
    writers["hatch"].add("EGG", quantize_rgba(egg, 15))

    return writers


def words_to_bytes(words):
    return struct.pack(f"<{len(words)}H", *words) if words else b""


def raw_deflate(payload):
    compressor = zlib.compressobj(level=6, wbits=-15)
    return compressor.compress(payload) + compressor.flush()


def write_pack(name, writer):
    payload = bytearray()
    for frame in writer.frames:
        payload.extend(struct.pack(
            "<HHHBBHIII",
            KIND_IDS[frame["kind"]],
            frame["width"],
            frame["height"],
            frame["format"],
            frame["palette_size"],
            0,
            frame["offset"],
            frame["length"],
            frame["palette_offset"],
        ))
    payload.extend(words_to_bytes(writer.data))
    payload.extend(words_to_bytes(writer.palettes))

    compressed = raw_deflate(payload)
    header = struct.pack(
        "<IHHIIIIII",
        MAGIC,
        VERSION,
        len(writer.frames),
        len(writer.data),
        len(writer.palettes),
        FLAG_RAW_DEFLATE,
        len(payload),
        len(compressed),
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    output = OUTPUTS[name]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + compressed)
    return {
        "name": name,
        "frames": len(writer.frames),
        "data_words": len(writer.data),
        "palette_words": len(writer.palettes),
        "raw_bytes": len(payload),
        "pack_bytes": len(header) + len(compressed),
        "output": output,
    }


def write_manifest(writers):
    legacy_output = PACK_OUT / "game" / "game.smonfx"
    if legacy_output.exists():
        legacy_output.unlink()

    manifest_path = PACK_OUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    manifest.pop("gameAssets", None)
    manifest.update({
        "format": "smon-resource-pack-v1",
        "id": "dev",
        "schema": 1,
        "version": "0.0.0-dev",
        "uiAssets": "game/ui.smonfx",
        "battleAssets": "game/battle.smonfx",
        "mapAssets": "game/maps.smonfx",
        "hatchAssets": "game/hatch.smonfx",
        "gameAssetCount": sum(len(writer.frames) for writer in writers.values()),
    })
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    active_path = DATA_DIR / "active.json"
    active_path.write_text(json.dumps({
        "activePack": "dev",
        "packPath": "/packs/dev",
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main():
    validate_kind_order()
    validate_explore_tile_mapping()
    writers = build_assets()
    results = [write_pack(name, writers[name]) for name in ("ui", "battle", "map", "hatch")]
    write_manifest(writers)
    for result in results:
        ratio = result["pack_bytes"] / result["raw_bytes"] if result["raw_bytes"] else 0
        print(
            f"game_pack={result['name']} frames={result['frames']} "
            f"data_words={result['data_words']} palette_words={result['palette_words']} "
            f"raw_bytes={result['raw_bytes']} pack_bytes={result['pack_bytes']} "
            f"ratio={ratio:.3f} output={result['output']}"
        )


if __name__ == "__main__":
    main()
