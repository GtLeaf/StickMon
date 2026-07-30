#!/usr/bin/env python3
import argparse
from collections import deque
import json
import os
import re
import struct
import zlib
from pathlib import Path

from PIL import Image

from generate_explore_map import autotile_variant, regular_tile
from map_generation_rules import CUSTOM_TILE_SOURCES


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
STATUS_ICON_DIR = ROOT / "origin_asset" / "icon" / "status"
EXPLORE_MENU_BG_SOURCE = (
    ROOT / "origin_asset" / "mainScreen" / "menu" / "explore" / "bg_explore.png"
)
SHOWER_SOURCE_DIR = ROOT / "origin_asset" / "icon" / "home" / "shower"
SHOWER_PREVIEW_DIR = GENERATED_GAME_DIR / "shower"
GAME_ASSETS_HEADER = ROOT / "src" / "assets" / "GameAssets.h"
GAME_ASSETS_SOURCE = ROOT / "src" / "assets" / "GameAssets.cpp"

MAGIC = 0x58464753  # SGFX
VERSION = 2
FLAG_RAW_DEFLATE = 1
FORMAT_INDEXED4_RLE = 1
MAX_PACK_FRAMES = 256
MAX_PACK_DATA_WORDS = 200000
MAX_PACK_PALETTE_WORDS = 2048
MAX_PACK_PAYLOAD_BYTES = 384000

ITEM_SIZE = 36
EXPLORE_PICKUP_SIZE = 22
EXPLORE_PICKUP_CONTENT_SIZE = 19
BALL_W = 32
BALL_H = 64
BACKGROUND_W = 240
BACKGROUND_H = 135
EGG_SIZE = 32
STATUS_ICON_SIZE = 12
SHOWER_BUBBLE_SIZES = (
    (8, 8),
    (16, 16),
    (24, 24),
    (32, 32),
    (48, 48),
    (64, 40),
)
SHOWER_SOAP_COUNT = 3
SHOWER_BRUSH_SIZE = (42, 42)
SHOWER_SOAP_SIZE = (36, 36)
SHOWER_SPRINKLER_SIZE = (42, 42)
SHOWER_BACKGROUND_SIZE = (240, 135)
SHOWER_MENU_SOAP_SIZE = (43, 43)
SHOWER_MENU_BRUSH_SIZE = (50, 50)
SHOWER_MENU_SPRINKLER_SIZE = (50, 50)

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
    ("ITEM_TASTY_FOOD", "SITRUSBERRY.png"),
    ("ITEM_SWEET_FOOD", "PECHABERRY.png"),
    ("ITEM_SPICY_FOOD", "CHERIBERRY.png"),
    ("ITEM_SOUR_FOOD", "ASPEARBERRY.png"),
    ("ITEM_BITTER_FOOD", "RAWSTBERRY.png"),
    ("ITEM_DRY_FOOD", "CHESTOBERRY.png"),
    ("ITEM_PARALYZE_HEAL", "PARALYZEHEAL.png"),
    ("ITEM_AWAKENING", "AWAKENING.png"),
    ("ITEM_BURN_HEAL", "BURNHEAL.png"),
    ("ITEM_ICE_HEAL", "ICEHEAL.png"),
    ("ITEM_MAX_POTION", "MAXPOTION.png"),
    ("ITEM_FULL_RESTORE", "FULLRESTORE.png"),
    ("ITEM_FULL_HEAL", "FULLHEAL.png"),
    ("ITEM_FIRE_STONE", "FIRESTONE.png"),
    ("ITEM_WATER_STONE", "WATERSTONE.png"),
    ("ITEM_THUNDER_STONE", "THUNDERSTONE.png"),
    ("ITEM_REVIVE", "REVIVE.png"),
    ("ITEM_MAX_REPEL", "MAXREPEL.png"),
    ("ITEM_HONEY", "HONEY.png"),
    ("ITEM_NUGGET", "NUGGET.png"),
    ("ITEM_BIG_PEARL", "BIGPEARL.png"),
    ("ITEM_STAR_PIECE", "STARPIECE.png"),
]

SHOWER_ASSETS = [
    *(f"SHOWER_BUBBLE_{index}" for index in range(len(SHOWER_BUBBLE_SIZES))),
    "SHOWER_BRUSH",
    *(f"SHOWER_SOAP_{index}" for index in range(SHOWER_SOAP_COUNT)),
    "SHOWER_SPRINKLER",
    "SHOWER_MENU_SOAP",
    "SHOWER_MENU_BRUSH",
    "SHOWER_MENU_SPRINKLER",
    "SHOWER_BACKGROUND",
]

EXPLORE_PICKUP_MARKERS = [
    ("EXPLORE_PICKUP_BALL", "POKEBALL.png"),
]

BALLS = [
    ("POKE_BALL", "POKEBALL"),
    ("GREAT_BALL", "GREATBALL"),
    ("HEAVY_BALL", "HEAVYBALL"),
    ("TIMER_BALL", "TIMERBALL"),
]

STATUS_ICONS = [
    ("STATUS_POISON", "status_poison.png"),
    ("STATUS_TOXIC", "status_toxic.png"),
    ("STATUS_PARALYSIS", "status_paralysis.png"),
    ("STATUS_SLEEP", "status_sleep.png"),
    ("STATUS_BURN", "status_burn.png"),
    ("STATUS_FREEZE", "status_freeze.png"),
]

BACKGROUNDS = [
    ("BATTLE_BG_GRASS", "field_bg.png"),
    ("BATTLE_BG_RIVERSIDE", "water_bg.png"),
    ("BATTLE_BG_DEEP_FOREST", "forest_bg.png"),
    ("BATTLE_BG_SNOW", "snow_bg.png"),
]

MENU_BACKGROUNDS = [
    ("EXPLORE_MENU_BACKGROUND", EXPLORE_MENU_BG_SOURCE),
]

EVOLUTION_BACKGROUNDS = [
    ("EVOLUTION_BACKGROUND", GRAPHICS / "UI" / "evolution_bg.png"),
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
    ("EXPLORE_TILE_0859", 859),
    ("EXPLORE_TILE_1088", 1088),
    ("EXPLORE_TILE_1090", 1090),
    ("EXPLORE_TILE_1096", 1096),
    ("EXPLORE_TILE_1097", 1097),
    ("EXPLORE_TILE_1098", 1098),
    ("EXPLORE_TILE_1104", 1104),
    ("EXPLORE_TILE_1106", 1106),
    ("EXPLORE_TILE_1161", 1161),
    ("EXPLORE_TILE_1162", 1162),
    ("EXPLORE_TILE_1185", 1185),
    ("EXPLORE_TILE_1188", 1188),
    ("EXPLORE_TILE_1506", 1506),
    ("EXPLORE_TILE_1507", 1507),
    ("EXPLORE_TILE_1514", 1514),
    ("EXPLORE_TILE_1515", 1515),
    ("EXPLORE_TILE_1532", 1532),
    ("EXPLORE_TILE_1627", 1627),
    ("EXPLORE_TILE_1635", 1635),
    ("EXPLORE_TILE_1643", 1643),
    ("EXPLORE_TILE_1662", 1662),
    ("EXPLORE_TILE_1665", 1665),
    ("EXPLORE_TILE_1681", 1681),
    ("EXPLORE_TILE_1682", 1682),
    ("EXPLORE_TILE_4400", 4400),
    ("EXPLORE_TILE_4401", 4401),
    ("EXPLORE_TILE_4402", 4402),
    ("EXPLORE_TILE_4403", 4403),
    ("EXPLORE_TILE_1231", 1231),
]

EXTERNAL_EXPLORE_TILES = [
    (f"EXPLORE_TILE_{runtime_id:04d}", runtime_id, tileset_name, source_id)
    for runtime_id, (tileset_name, source_id) in CUSTOM_TILE_SOURCES.items()
]

ANIMATED_EXPLORE_FRAMES = []
for tile_id in (48, 64, 68):
    ANIMATED_EXPLORE_FRAMES.extend(
        (f"EXPLORE_TILE_{tile_id:04d}_F{frame}", tile_id, "Sea", frame)
        for frame in range(8)
    )
ANIMATED_EXPLORE_FRAMES.extend(
    (f"EXPLORE_TILE_0072_F{frame}", 72, "Sea", frame)
    for frame in range(1, 8)
)
for tile_id in (80, 84):
    ANIMATED_EXPLORE_FRAMES.extend(
        (f"EXPLORE_TILE_{tile_id:04d}_F{frame}", tile_id, "Sea", frame)
        for frame in range(8)
    )
for kind_prefix, tile_id, source_name in (
    ("EXPLORE_WATERFALL_CREST", 273, "Waterfall crest"),
    ("EXPLORE_WATERFALL_BODY", 288, "Waterfall"),
    ("EXPLORE_WATERFALL_BOTTOM", 336, "Waterfall bottom"),
):
    ANIMATED_EXPLORE_FRAMES.extend(
        (f"{kind_prefix}_F{frame}", tile_id, source_name, frame)
        for frame in range(4)
    )

ANIMATED_EXPLORE_TILE_IDS = {
    48, 64, 68, 72, 80, 84,
    283, 273, 285,
    322, 308, 324,
    304, 288, 312,
    328, 316, 326,
    336,
}

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
KIND_ORDER.extend(SHOWER_ASSETS)
for kind_prefix, _ in BALLS:
    KIND_ORDER.extend(f"BALL_{kind_prefix}_{index}" for index in range(8))
    KIND_ORDER.append(f"BALL_{kind_prefix}_OPEN")
KIND_ORDER.append("BALL_BURST_STAR")
KIND_ORDER.extend(kind for kind, _ in BACKGROUNDS)
KIND_ORDER.extend(kind for kind, _ in MENU_BACKGROUNDS)
KIND_ORDER.extend(kind for kind, _ in EXPLORE_TILES)
KIND_ORDER.extend(kind for kind, _runtime_id, _tileset, _source_id in EXTERNAL_EXPLORE_TILES)
KIND_ORDER.extend(kind for kind, _tile_id, _source, _frame in ANIMATED_EXPLORE_FRAMES)
KIND_ORDER.append("EGG")
KIND_ORDER.extend(kind for kind, _ in STATUS_ICONS)
KIND_ORDER.extend(kind for kind, _ in EXPLORE_PICKUP_MARKERS)
KIND_ORDER.extend(kind for kind, _ in EVOLUTION_BACKGROUNDS)
KIND_IDS = {kind: index for index, kind in enumerate(KIND_ORDER)}


def validate_kind_order():
    source = GAME_ASSETS_HEADER.read_text(encoding="utf-8")
    match = re.search(r"enum class Kind\s*:\s*uint16_t\s*\{(.*?)\bCOUNT\s*,", source, re.S)
    if not match:
        raise ValueError(f"unable to parse Kind enum: {GAME_ASSETS_HEADER}")
    header_order = re.findall(r"^\s*([A-Z][A-Z0-9_]*)\s*,", match.group(1), re.M)
    if header_order != KIND_ORDER:
        raise ValueError("GameAssets::Kind order does not match generated pack order")


def validate_runtime_pack_limit():
    source = GAME_ASSETS_SOURCE.read_text(encoding="utf-8")
    match = re.search(r"constexpr\s+uint16_t\s+MAX_FRAMES\s*=\s*(\d+)\s*;", source)
    if not match:
        raise ValueError(f"unable to parse MAX_FRAMES: {GAME_ASSETS_SOURCE}")
    if int(match.group(1)) != MAX_PACK_FRAMES:
        raise ValueError(
            "game asset frame limit mismatch: "
            f"generator={MAX_PACK_FRAMES} runtime={match.group(1)}"
        )


def remove_edge_white_background(image):
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    visited = bytearray(width * height)
    pending = deque()

    def enqueue(x, y):
        index = y * width + x
        if visited[index]:
            return
        r, g, b, _a = pixels[x, y]
        if min(r, g, b) < 235 or max(r, g, b) - min(r, g, b) > 24:
            return
        visited[index] = 1
        pending.append((x, y))

    for x in range(width):
        enqueue(x, 0)
        enqueue(x, height - 1)
    for y in range(height):
        enqueue(0, y)
        enqueue(width - 1, y)

    while pending:
        x, y = pending.popleft()
        pixels[x, y] = (*pixels[x, y][:3], 0)
        if x > 0:
            enqueue(x - 1, y)
        if x + 1 < width:
            enqueue(x + 1, y)
        if y > 0:
            enqueue(x, y - 1)
        if y + 1 < height:
            enqueue(x, y + 1)
    return image


def extract_showcase_sprites(source_path, expected_count):
    image = remove_edge_white_background(Image.open(source_path))
    alpha = image.getchannel("A")
    columns = [
        x for x in range(image.width)
        if alpha.crop((x, 0, x + 1, image.height)).getbbox() is not None
    ]
    if not columns:
        raise ValueError(f"no shower sprites found: {source_path}")

    runs = []
    start = previous = columns[0]
    for x in columns[1:]:
        if x - previous > 8:
            runs.append((start, previous + 1))
            start = x
        previous = x
    runs.append((start, previous + 1))
    if len(runs) != expected_count:
        raise ValueError(
            f"expected {expected_count} shower sprites, found {len(runs)}: {source_path}"
        )

    sprites = []
    for left, right in runs:
        segment = image.crop((left, 0, right, image.height))
        bbox = segment.getchannel("A").getbbox()
        if not bbox:
            raise ValueError(f"empty shower sprite segment: {source_path}")
        sprites.append(segment.crop(bbox))
    return sprites


def fit_sprite_canvas(image, size, padding=1):
    target_w, target_h = size
    max_w = max(1, target_w - padding * 2)
    max_h = max(1, target_h - padding * 2)
    scale = min(max_w / image.width, max_h / image.height)
    resized = image.resize(
        (max(1, round(image.width * scale)), max(1, round(image.height * scale))),
        Image.Resampling.NEAREST,
    )
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    canvas.alpha_composite(
        resized,
        ((target_w - resized.width) // 2, (target_h - resized.height) // 2),
    )
    return canvas


def prepare_shower_assets():
    bubble_sources = extract_showcase_sprites(
        SHOWER_SOURCE_DIR / "bobble.png", len(SHOWER_BUBBLE_SIZES)
    )
    brush_source = Image.open(SHOWER_SOURCE_DIR / "bush.png").convert("RGBA")
    soap_paths = sorted((SHOWER_SOURCE_DIR / "soap").glob("soap_*.png*"))
    if len(soap_paths) != SHOWER_SOAP_COUNT:
        raise ValueError(
            f"expected {SHOWER_SOAP_COUNT} shower soaps, found "
            f"{len(soap_paths)}: {SHOWER_SOURCE_DIR / 'soap'}"
        )
    soap_sources = [Image.open(path).convert("RGBA") for path in soap_paths]
    sprinkler_source = Image.open(
        SHOWER_SOURCE_DIR / "sprinkle.png"
    ).convert("RGBA")

    assets = []
    for index, (source, size) in enumerate(zip(bubble_sources, SHOWER_BUBBLE_SIZES)):
        assets.append((f"SHOWER_BUBBLE_{index}", fit_sprite_canvas(source, size, 0)))
    assets.append(("SHOWER_BRUSH", fit_sprite_canvas(brush_source, SHOWER_BRUSH_SIZE)))
    for index, source in enumerate(soap_sources):
        assets.append((f"SHOWER_SOAP_{index}", fit_sprite_canvas(source, SHOWER_SOAP_SIZE)))
    assets.append((
        "SHOWER_SPRINKLER",
        fit_sprite_canvas(sprinkler_source, SHOWER_SPRINKLER_SIZE),
    ))
    assets.append((
        "SHOWER_MENU_SOAP",
        fit_sprite_canvas(soap_sources[0], SHOWER_MENU_SOAP_SIZE),
    ))
    assets.append((
        "SHOWER_MENU_BRUSH",
        fit_sprite_canvas(brush_source, SHOWER_MENU_BRUSH_SIZE),
    ))
    assets.append((
        "SHOWER_MENU_SPRINKLER",
        fit_sprite_canvas(sprinkler_source, SHOWER_MENU_SPRINKLER_SIZE),
    ))
    bathroom = Image.open(
        SHOWER_SOURCE_DIR / "bathroom" / "bathroom.png"
    ).convert("RGB")
    crop_height = round(bathroom.width * SHOWER_BACKGROUND_SIZE[1] /
                        SHOWER_BACKGROUND_SIZE[0])
    crop_top = max(0, (bathroom.height - crop_height) // 2)
    bathroom = bathroom.crop(
        (0, crop_top, bathroom.width, crop_top + crop_height)
    ).resize(SHOWER_BACKGROUND_SIZE, Image.Resampling.LANCZOS)
    # 浴室背景按原图清晰度上屏，不做模糊处理。
    bathroom = bathroom.convert("RGBA")
    assets.append(("SHOWER_BACKGROUND", bathroom))
    return assets


def validate_explore_tile_mapping():
    source = GAME_ASSETS_SOURCE.read_text(encoding="utf-8")
    match = re.search(
        r"bool drawExploreTile\(.*?\)\s*\{(.*?)\n\}\n\nKind itemKind",
        source,
        re.S,
    )
    if not match:
        raise ValueError("unable to parse GameAssets::drawExploreTile")
    mapped_ids = {int(tile_id) for tile_id in re.findall(r"case\s+(\d+)\s*:", match.group(1))}
    expected_ids = (
        {tile_id for _kind, tile_id in EXPLORE_TILES}
        | {runtime_id for _kind, runtime_id, _tileset, _source_id in EXTERNAL_EXPLORE_TILES}
        | ANIMATED_EXPLORE_TILE_IDS
    )
    if mapped_ids != expected_ids:
        raise ValueError("GameAssets::drawExploreTile mapping does not match EXPLORE_TILES")


def validate_explore_pickup_pack_mapping():
    source = GAME_ASSETS_SOURCE.read_text(encoding="utf-8")
    match = re.search(
        r"PackSlot packSlotFor\(Kind kind\)\s*\{(.*?)\n\}",
        source,
        re.S,
    )
    if not match:
        raise ValueError("unable to parse GameAssets::packSlotFor")
    for kind, _filename in EXPLORE_PICKUP_MARKERS:
        if f"Kind::{kind}" not in match.group(1):
            raise ValueError(f"{kind} is not routed to the UI asset pack")


def validate_shower_pack_mapping():
    source = GAME_ASSETS_SOURCE.read_text(encoding="utf-8")
    match = re.search(
        r"PackSlot packSlotFor\(Kind kind\)\s*\{(.*?)\n\}",
        source,
        re.S,
    )
    if not match or "Kind::SHOWER_BACKGROUND" not in match.group(1):
        raise ValueError("shower assets are not routed to the UI asset pack")


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


def prepare_explore_pickup_marker(path):
    source = load_rgba(path)
    bbox = source.getchannel("A").getbbox()
    if not bbox:
        raise ValueError(f"explore pickup marker has no opaque pixels: {path}")
    marker = source.crop(bbox)
    scale = min(
        EXPLORE_PICKUP_CONTENT_SIZE / marker.width,
        EXPLORE_PICKUP_CONTENT_SIZE / marker.height,
    )
    marker = marker.resize(
        (max(1, round(marker.width * scale)), max(1, round(marker.height * scale))),
        Image.Resampling.NEAREST,
    )
    canvas = Image.new(
        "RGBA", (EXPLORE_PICKUP_SIZE, EXPLORE_PICKUP_SIZE), (0, 0, 0, 0)
    )
    canvas.alpha_composite(
        marker,
        (
            (EXPLORE_PICKUP_SIZE - marker.width) // 2,
            (EXPLORE_PICKUP_SIZE - marker.height) // 2,
        ),
    )
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


def prepare_animated_explore_tile(tile_id, source, frame_index):
    tile = autotile_variant(source, tile_id % 48, frame_index)
    if tile is None or tile.size != (32, 32):
        raise ValueError(f"unable to render animated explore tile: {tile_id} frame={frame_index}")
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

    SHOWER_PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
    for stale_preview in SHOWER_PREVIEW_DIR.glob("*.png"):
        stale_preview.unlink()
    for kind, image in prepare_shower_assets():
        image.save(SHOWER_PREVIEW_DIR / f"{kind.lower()}.png")
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

    for kind, source_path in MENU_BACKGROUNDS:
        image = load_rgba(source_path)
        image = image.resize((BACKGROUND_W, BACKGROUND_H), Image.Resampling.LANCZOS)
        writers["battle"].add(kind, quantize_rgba(image, 16))

    for kind, source_path in EVOLUTION_BACKGROUNDS:
        image = load_rgba(source_path)
        image = image.resize((BACKGROUND_W, BACKGROUND_H), Image.Resampling.LANCZOS)
        writers["battle"].add(kind, quantize_rgba(image, 16))

    for kind, filename in STATUS_ICONS:
        image = load_rgba(STATUS_ICON_DIR / filename)
        image = image.resize((STATUS_ICON_SIZE, STATUS_ICON_SIZE), Image.Resampling.NEAREST)
        writers["battle"].add(kind, quantize_rgba(image, 15))

    for kind, filename in EXPLORE_PICKUP_MARKERS:
        image = prepare_explore_pickup_marker(GRAPHICS / "Items" / filename)
        writers["ui"].add(kind, quantize_rgba(image, 15))

    tileset = load_rgba(GRAPHICS / "Tilesets" / "Outside.png")
    autotiles = [load_rgba(GRAPHICS / "Autotiles" / f"{name}.png") for name in OUTSIDE_AUTOTILES]
    for kind, tile_id in EXPLORE_TILES:
        tile = prepare_explore_tile(tile_id, tileset, autotiles)
        writers["map"].add(kind, quantize_rgba(tile, 16))
    external_tilesets = {
        tileset_name: load_rgba(GRAPHICS / "Tilesets" / tileset_name)
        for _kind, _runtime_id, tileset_name, _source_id in EXTERNAL_EXPLORE_TILES
    }
    for kind, _runtime_id, tileset_name, source_id in EXTERNAL_EXPLORE_TILES:
        tile = prepare_explore_tile(source_id, external_tilesets[tileset_name], ())
        writers["map"].add(kind, quantize_rgba(tile, 16))
    animated_sources = {
        name: load_rgba(GRAPHICS / "Autotiles" / f"{name}.png")
        for name in {source for _kind, _tile_id, source, _frame in ANIMATED_EXPLORE_FRAMES}
    }
    for kind, tile_id, source_name, frame_index in ANIMATED_EXPLORE_FRAMES:
        tile = prepare_animated_explore_tile(
            tile_id, animated_sources[source_name], frame_index
        )
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
    if len(writer.frames) > MAX_PACK_FRAMES:
        raise ValueError(
            f"{name} pack has {len(writer.frames)} frames; limit is {MAX_PACK_FRAMES}"
        )
    if len(writer.data) > MAX_PACK_DATA_WORDS:
        raise ValueError(
            f"{name} pack has {len(writer.data)} data words; "
            f"limit is {MAX_PACK_DATA_WORDS}"
        )
    if len(writer.palettes) > MAX_PACK_PALETTE_WORDS:
        raise ValueError(
            f"{name} pack has {len(writer.palettes)} palette words; "
            f"limit is {MAX_PACK_PALETTE_WORDS}"
        )
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
    if len(payload) > MAX_PACK_PAYLOAD_BYTES:
        raise ValueError(
            f"{name} pack payload is {len(payload)} bytes; "
            f"limit is {MAX_PACK_PAYLOAD_BYTES}"
        )

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
    parser = argparse.ArgumentParser(description="Build StickMon game asset packs")
    parser.add_argument(
        "--only",
        default="ui,battle,map,hatch",
        help="comma-separated packs to write (ui,battle,map,hatch)",
    )
    args = parser.parse_args()
    selected = tuple(name.strip() for name in args.only.split(",") if name.strip())
    if not selected or any(name not in OUTPUTS for name in selected):
        raise ValueError("--only must contain ui,battle,map,hatch")

    validate_kind_order()
    validate_runtime_pack_limit()
    validate_explore_tile_mapping()
    validate_explore_pickup_pack_mapping()
    validate_shower_pack_mapping()
    writers = build_assets()
    results = [write_pack(name, writers[name]) for name in selected]
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
