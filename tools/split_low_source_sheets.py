#!/usr/bin/env python3
import argparse
from pathlib import Path
import shutil
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
LOW_DIR = ROOT / "origin_asset" / "source_sheets" / "low"
SPLIT_DIR = LOW_DIR / "split_by_species"

SHEETS = {
    "001_025": "Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#001-025).png",
    "026_050": "Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#026-050).png",
    "051_075": "Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#051-075).png",
    "076_100": "Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#076-100).png",
    "101_125": "Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#101-125).png",
}

# y ranges are source-sheet vertical slices. The sheets place Pokemon in
# Pokedex order from top to bottom, but each Pokemon has a variable number of
# action rows, so these slices are intentionally per species.
LOW_REQUIRED_SPECIES = [
    ("001_025", 1, "bulbasaur", 6, 96),
    ("001_025", 2, "ivysaur", 103, 160),
    ("001_025", 3, "venusaur", 165, 227),
    ("001_025", 4, "charmander", 231, 324),
    ("001_025", 5, "charmeleon", 327, 392),
    ("001_025", 6, "charizard", 395, 498),
    ("001_025", 7, "squirtle", 500, 580),
    ("001_025", 8, "wartortle", 585, 644),
    ("001_025", 9, "blastoise", 646, 706),
    ("001_025", 25, "pikachu", 1748, 1855),
    ("026_050", 26, "raichu", 4, 112),
    ("076_100", 92, "gastly", 1155, 1220),
    ("076_100", 93, "haunter", 1222, 1291),
    ("076_100", 94, "gengar", 1297, 1358),
    ("101_125", 123, "scyther", 1911, 1974),
]

LOW_EXTRA_SPLIT_SPECIES = [
    ("001_025", 10, "caterpie", 710, 768),
    ("001_025", 11, "metapod", 768, 831),
    ("001_025", 12, "butterfree", 833, 898),
    ("001_025", 13, "weedle", 894, 958),
    ("001_025", 14, "kakuna", 958, 1008),
    ("001_025", 15, "beedrill", 1008, 1065),
    ("001_025", 16, "pidgey", 1070, 1118),
    ("001_025", 17, "pidgeotto", 1122, 1182),
    ("001_025", 18, "pidgeot", 1186, 1248),
    ("001_025", 19, "rattata", 1256, 1313),
    ("001_025", 20, "raticate", 1317, 1369),
    ("001_025", 21, "spearow", 1373, 1430),
    ("001_025", 22, "fearow", 1436, 1499),
    ("001_025", 23, "ekans", 1508, 1615),
    ("001_025", 24, "arbok", 1620, 1741),
    ("026_050", 27, "sandshrew", 119, 162),
    ("026_050", 28, "sandslash", 169, 229),
    ("026_050", 29, "nidoran_f", 237, 289),
    ("026_050", 30, "nidorina", 297, 351),
    ("026_050", 31, "nidoqueen", 359, 419),
    ("026_050", 32, "nidoran_m", 429, 491),
    ("026_050", 33, "nidorino", 497, 559),
    ("026_050", 34, "nidoking", 568, 674),
    ("026_050", 35, "clefairy", 680, 738),
    ("026_050", 36, "clefable", 746, 805),
    ("026_050", 37, "vulpix", 815, 868),
    ("026_050", 38, "ninetales", 873, 971),
    ("026_050", 39, "jigglypuff", 979, 1031),
    ("026_050", 40, "wigglytuff", 1039, 1089),
    ("026_050", 41, "zubat", 1100, 1128),
    ("026_050", 42, "golbat", 1135, 1162),
    ("026_050", 43, "oddish", 1170, 1220),
    ("026_050", 44, "gloom", 1227, 1288),
    ("026_050", 45, "vileplume", 1295, 1354),
    ("026_050", 46, "paras", 1362, 1411),
    ("026_050", 47, "parasect", 1418, 1498),
    ("026_050", 48, "venonat", 1500, 1531),
    ("026_050", 49, "venomoth", 1539, 1596),
    ("026_050", 50, "diglett", 1600, 1646),
    ("051_075", 74, "geodude", 1898, 1962),
    ("051_075", 75, "graveler", 1968, 2032),
    ("076_100", 76, "golem", 6, 88),
    ("076_100", 77, "ponyta", 94, 155),
    ("076_100", 78, "rapidash", 161, 240),
    ("076_100", 79, "slowpoke", 247, 308),
    ("076_100", 80, "slowbro", 314, 376),
    ("076_100", 81, "magnemite", 385, 432),
    ("076_100", 82, "magneton", 439, 530),
    ("076_100", 83, "farfetchd", 538, 596),
    ("076_100", 84, "doduo", 607, 665),
    ("076_100", 85, "dodrio", 670, 761),
    ("076_100", 86, "seel", 768, 796),
    ("076_100", 87, "dewgong", 802, 862),
    ("076_100", 88, "grimer", 863, 922),
    ("076_100", 89, "muk", 928, 1022),
    ("076_100", 90, "shellder", 1026, 1080),
    ("076_100", 91, "cloyster", 1086, 1150),
    ("076_100", 95, "onix", 1363, 1487),
    ("076_100", 96, "drowzee", 1495, 1554),
    ("076_100", 97, "hypno", 1560, 1651),
    ("076_100", 98, "krabby", 1653, 1711),
    ("076_100", 99, "kingler", 1717, 1814),
    ("076_100", 100, "voltorb", 1818, 1879),
    ("101_125", 101, "electrode", 16, 67),
    ("101_125", 102, "exeggcute", 76, 172),
    ("101_125", 103, "exeggutor", 179, 269),
    ("101_125", 104, "cubone", 279, 391),
    ("101_125", 105, "marowak", 397, 480),
    ("101_125", 106, "hitmonlee", 494, 594),
    ("101_125", 107, "hitmonchan", 605, 676),
    ("101_125", 108, "lickitung", 687, 753),
    ("101_125", 109, "koffing", 768, 829),
    ("101_125", 110, "weezing", 838, 983),
    ("101_125", 111, "rhyhorn", 997, 1054),
    ("101_125", 112, "rhydon", 1061, 1126),
    ("101_125", 113, "chansey", 1136, 1195),
    ("101_125", 114, "tangela", 1203, 1265),
    ("101_125", 115, "kangaskhan", 1274, 1378),
    ("101_125", 116, "horsea", 1387, 1440),
    ("101_125", 117, "seadra", 1449, 1511),
    ("101_125", 118, "goldeen", 1519, 1574),
    ("101_125", 119, "seaking", 1583, 1677),
    ("101_125", 120, "staryu", 1684, 1738),
    ("101_125", 121, "starmie", 1746, 1801),
    ("101_125", 122, "mr_mime", 1808, 1902),
    ("101_125", 124, "jynx", 1983, 2078),
    ("101_125", 125, "electabuzz", 2086, 2193),
]

LOW_EXTRA_COPY_IDS = set(range(10, 19)) | {41, 42, 74, 75, 76}

CLEANUP_RECTS = {
    ("001_025", 12): [(253, 861, 610, 898)],
    ("076_100", 100): [(80, 1854, 370, 1879)],
}


def trim_horizontal(img):
    bg = img.getpixel((0, 0))[:3]
    bbox = None
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = img.getpixel((x, y))
            if a > 16 and (r, g, b) != bg:
                if bbox is None:
                    bbox = [x, y, x + 1, y + 1]
                else:
                    bbox[0] = min(bbox[0], x)
                    bbox[1] = min(bbox[1], y)
                    bbox[2] = max(bbox[2], x + 1)
                    bbox[3] = max(bbox[3], y + 1)
    if bbox is None:
        return img
    pad = 2
    content = img.crop(tuple(bbox))
    padded = Image.new("RGBA", (content.width + pad * 2, content.height + pad * 2), img.getpixel((0, 0)))
    padded.alpha_composite(content, (pad, pad))
    return padded


def erase_source_rects(img, sheet_key, species_id, y1):
    rects = CLEANUP_RECTS.get((sheet_key, species_id), [])
    if not rects:
        return img
    img = img.copy()
    bg = img.getpixel((0, 0))
    for left, top, right, bottom in rects:
        crop_top = max(0, top - y1)
        crop_bottom = min(img.height, bottom - y1)
        if crop_top >= crop_bottom:
            continue
        for y in range(crop_top, crop_bottom):
            for x in range(max(0, left), min(img.width, right)):
                img.putpixel((x, y), bg)
    return img


def main():
    parser = argparse.ArgumentParser(description="Split low-confidence PMD species sheets.")
    parser.add_argument(
        "species",
        nargs="*",
        type=int,
        help="Optional Pokedex ids to process; all configured species are processed when omitted.",
    )
    args = parser.parse_args()

    SPLIT_DIR.mkdir(parents=True, exist_ok=True)
    sheet_cache = {}

    entries = [(entry, True) for entry in LOW_REQUIRED_SPECIES]
    entries.extend((entry, entry[1] in LOW_EXTRA_COPY_IDS) for entry in LOW_EXTRA_SPLIT_SPECIES)
    if args.species:
        requested = set(args.species)
        entries = [(entry, copy_to_low) for entry, copy_to_low in entries if entry[1] in requested]
        found = {entry[1] for entry, _ in entries}
        missing = sorted(requested - found)
        if missing:
            parser.error(f"unknown species ids: {', '.join(str(value) for value in missing)}")

    for (sheet_key, species_id, slug, y1, y2), copy_to_low in entries:
        sheet = sheet_cache.get(sheet_key)
        if sheet is None:
            sheet_path = LOW_DIR / SHEETS[sheet_key]
            sheet = Image.open(sheet_path).convert("RGBA")
            sheet_cache[sheet_key] = sheet

        out_name = f"{species_id:03d}_{slug}.png"
        cropped = sheet.crop((0, y1, sheet.width, y2))
        cropped = erase_source_rects(cropped, sheet_key, species_id, y1)
        cropped = trim_horizontal(cropped)

        split_path = SPLIT_DIR / out_name
        low_path = LOW_DIR / out_name
        cropped.save(split_path)
        if copy_to_low:
            shutil.copy2(split_path, low_path)
        print(f"{out_name}: {cropped.width}x{cropped.height}")


if __name__ == "__main__":
    main()
