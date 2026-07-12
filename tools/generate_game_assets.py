#!/usr/bin/env python3
import json
import os
import struct
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ESSENTIALS = Path(os.environ.get(
    "ESSENTIALS_DIR",
    "${ESSENTIALS_DIR}",
))
GRAPHICS = ESSENTIALS / "Graphics"
DATA_DIR = ROOT / "data"
PACK_OUT = DATA_DIR / "packs" / "dev"
OUTPUT = PACK_OUT / "game" / "game.smonfx"
GENERATED_GAME_DIR = ROOT / "origin_asset" / "generated" / "game"
EGG_PREVIEW = GENERATED_GAME_DIR / "egg_32.png"

MAGIC = 0x58464753  # SGFX
VERSION = 1
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
        j = i + 1
        if transparent:
            while j < len(pixels) and pixels[j][3] <= 16 and j - i < 0x7FFF:
                j += 1
            out.append(0x8000 | (j - i))
        else:
            indices = []
            while j <= len(pixels) and len(indices) < 0x7FFF:
                r, g, b, a = pixels[j - 1]
                if a <= 16:
                    break
                indices.append(palette_map[rgb565(r, g, b)])
                if j == len(pixels) or pixels[j][3] <= 16:
                    break
                j += 1
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


def build_assets():
    writer = Writer()

    for kind, filename in ITEMS:
        image = load_rgba(GRAPHICS / "Items" / filename)
        image = image.resize((ITEM_SIZE, ITEM_SIZE), Image.Resampling.NEAREST)
        writer.add(kind, quantize_rgba(image, 15))

    for kind_prefix, filename in BALLS:
        sheet = load_rgba(GRAPHICS / "Battle animations" / f"ball_{filename}.png")
        if sheet.size != (BALL_W * 8, BALL_H):
            raise ValueError(f"unexpected ball sheet size: {sheet.size} {filename}")
        for index in range(8):
            frame = sheet.crop((index * BALL_W, 0, (index + 1) * BALL_W, BALL_H))
            writer.add(f"BALL_{kind_prefix}_{index}", quantize_rgba(frame, 15))

        opened = load_rgba(GRAPHICS / "Battle animations" / f"ball_{filename}_open.png")
        if opened.size != (BALL_W, BALL_H):
            raise ValueError(f"unexpected open ball size: {opened.size} {filename}")
        writer.add(f"BALL_{kind_prefix}_OPEN", quantize_rgba(opened, 15))

    burst = load_rgba(GRAPHICS / "Battle animations" / "ballBurst_star.png")
    burst = burst.resize((32, 32), Image.Resampling.NEAREST)
    writer.add("BALL_BURST_STAR", quantize_rgba(burst, 15))

    for kind, filename in BACKGROUNDS:
        image = load_rgba(GRAPHICS / "Battlebacks" / filename)
        image = image.resize((BACKGROUND_W, BACKGROUND_H), Image.Resampling.LANCZOS)
        writer.add(kind, quantize_rgba(image, 16))

    egg = prepare_egg(GRAPHICS / "Pokemon" / "Eggs" / "000.png")
    GENERATED_GAME_DIR.mkdir(parents=True, exist_ok=True)
    egg.save(EGG_PREVIEW)
    writer.add("EGG", quantize_rgba(egg, 15))

    return writer


def words_to_bytes(words):
    return struct.pack(f"<{len(words)}H", *words) if words else b""


def write_pack(writer):
    kind_map = {frame["kind"]: index for index, frame in enumerate(writer.frames)}
    payload = bytearray(struct.pack(
        "<IHHII",
        MAGIC,
        VERSION,
        len(writer.frames),
        len(writer.data),
        len(writer.palettes),
    ))
    for frame in writer.frames:
        payload.extend(struct.pack(
            "<HHHBBHIII",
            kind_map[frame["kind"]],
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

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(payload)

    manifest_path = PACK_OUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    manifest.update({
        "format": "smon-resource-pack-v1",
        "id": "dev",
        "schema": 1,
        "version": "0.0.0-dev",
        "gameAssets": "game/game.smonfx",
        "gameAssetCount": len(writer.frames),
    })
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    active_path = DATA_DIR / "active.json"
    active_path.write_text(json.dumps({
        "activePack": "dev",
        "packPath": "/packs/dev",
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return len(payload)


def main():
    writer = build_assets()
    size = write_pack(writer)
    print(
        f"game_assets={len(writer.frames)} data_words={len(writer.data)} "
        f"palette_words={len(writer.palettes)} pack_bytes={size} output={OUTPUT}"
    )


if __name__ == "__main__":
    main()
