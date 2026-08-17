#!/usr/bin/env python3

"""Generate a complete, status-labelled cold-region Caves tile catalog."""

import argparse
import json
import math
import textwrap
from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from asset_paths import portable_asset_path
from generate_explore_map import ESSENTIALS, export_map


ROOT = Path(__file__).resolve().parents[1]
TILESET_PATH = ESSENTIALS / "Graphics" / "Tilesets" / "Caves.png"
DEFAULT_OUTPUT_DIR = (
    ROOT
    / "origin_asset"
    / "generated"
    / "game"
    / "tileset_reference"
    / "frost_crystal_cave"
)

FIRST_TILE_ID = 1120
LAST_TILE_ID = 1543
CORE_SNOW_FIRST_TILE_ID = 1240
TILE_SIZE = 32
TILESET_COLUMNS = 8
PAGE_TILE_COUNT = 64
PAGE_COLUMNS = 8
PAGE_ROWS = 8
CELL_WIDTH = 172
CELL_HEIGHT = 116
PAGE_MARGIN = 18
HEADER_HEIGHT = 82

STATUS_STYLE = {
    "confirmed": {
        "short": "CONF",
        "color": (75, 214, 132, 255),
        "description": "User-confirmed meaning",
    },
    "inferred": {
        "short": "INFER",
        "color": (255, 203, 87, 255),
        "description": "Visual or metadata inference; needs confirmation",
    },
    "map034_unknown": {
        "short": "M34?",
        "color": (91, 169, 255, 255),
        "description": "Used by Map034, but exact meaning is unknown",
    },
    "unknown": {
        "short": "UNK",
        "color": (161, 174, 180, 255),
        "description": "Candidate tile with no confirmed meaning",
    },
    "placeholder": {
        "short": "EMPTY",
        "color": (242, 92, 92, 255),
        "description": "Red-cross placeholder or empty source slot",
    },
}

CONFIRMED_LABELS = {
    1120: "transparent rock island NW",
    1121: "transparent rock island N",
    1122: "transparent rock island NE",
    1123: "background rock island NW",
    1124: "background rock island N",
    1125: "background rock island NE",
    1128: "transparent rock island W",
    1129: "transparent rock island center",
    1130: "transparent rock island E",
    1131: "background rock island W",
    1132: "background rock island center",
    1133: "background rock island E",
    1136: "transparent rock island SW",
    1137: "transparent rock island S",
    1138: "transparent rock island SE",
    1139: "background rock island SW",
    1140: "background rock island S",
    1141: "background rock island SE",
    1299: "cave exit left",
    1300: "cave exit center",
    1301: "cave exit right",
    1321: "cracked ice",
    1322: "broken-ice hole",
    1326: "round water SW",
    1327: "round water SE",
    1333: "stairs down",
    1341: "hole",
    1344: "edge trace start",
    1345: "edge turn R>D",
    1353: "edge down",
    1361: "edge turn D>R",
    1362: "edge right",
    1363: "edge turn R>U",
    1355: "edge up",
    1347: "edge turn U>R",
    1348: "edge trace end",
}

# These labels are deliberately weaker than confirmed semantics. They are
# retained only to make the catalog useful while the user reviews every tile.
INFERRED_LABELS = {
    1296: "snow mass NW / outside SE?",
    1297: "wall south?",
    1298: "snow mass NE / outside SW?",
    1302: "wall NW?",
    1303: "wall NE?",
    1304: "wall east?",
    1305: "blocked snow fill?",
    1306: "wall west?",
    1310: "wall SW?",
    1311: "wall SE?",
    1312: "snow mass SW / outside NE?",
    1313: "wall north?",
    1314: "snow mass SE / outside NW?",
    1318: "inner wall turn?",
    1319: "inner wall turn?",
    1328: "ice surface?",
    1332: "ladder top?",
    1340: "ladder bottom?",
    1349: "snow boulder?",
    1350: "object upper half?",
    1351: "snow boulder alt?",
    1357: "snow rock?",
    1358: "ice crystal base?",
    1359: "ice crystal alt?",
    1366: "small snow rock?",
    1367: "small snow rock alt?",
    1386: "walkable snow floor?",
    1490: "ice edge south?",
    1492: "ice edge NW?",
    1494: "ice edge NE?",
    1497: "ice edge east?",
    1499: "ice edge west?",
    1506: "ice edge north?",
    1508: "ice edge SW?",
    1510: "ice edge SE?",
}


def load_font(size, bold=False):
    candidates = (
        Path("/System/Library/Fonts/SFNSMono.ttf"),
        Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf")
        if bold
        else Path("/System/Library/Fonts/Supplemental/Arial.ttf"),
    )
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def source_tile(tileset, tile_id):
    index = tile_id - 384
    if index < 0:
        raise ValueError(f"tile {tile_id} is not a regular RPG Maker XP tile")
    x = (index % TILESET_COLUMNS) * TILE_SIZE
    y = (index // TILESET_COLUMNS) * TILE_SIZE
    return tileset.crop((x, y, x + TILE_SIZE, y + TILE_SIZE))


def is_placeholder(tile):
    pixels = tile.convert("RGBA").tobytes()
    red_pixels = sum(
        1
        for offset in range(0, len(pixels), 4)
        if pixels[offset + 3] > 0
        and pixels[offset] > 220
        and pixels[offset + 1] < 100
        and pixels[offset + 2] < 100
    )
    return red_pixels >= 20 or tile.getbbox() is None


def checkerboard(size, square=8):
    image = Image.new("RGBA", (size, size), (199, 205, 208, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size, square):
        for x in range(0, size, square):
            if (x // square + y // square) % 2:
                draw.rectangle(
                    (x, y, x + square - 1, y + square - 1),
                    fill=(231, 234, 236, 255),
                )
    return image


def map034_usage():
    data = export_map(34)
    counts = Counter()
    layers = {tile_id: [0, 0, 0] for tile_id in range(FIRST_TILE_ID, LAST_TILE_ID + 1)}
    for layer_index, layer in enumerate(data["layers"]):
        for tile_id in layer:
            if FIRST_TILE_ID <= tile_id <= LAST_TILE_ID:
                counts[tile_id] += 1
                layers[tile_id][layer_index] += 1
    return data, counts, layers


def classify_tile(tile_id, tile, usage_count):
    if tile_id in CONFIRMED_LABELS:
        return "confirmed", CONFIRMED_LABELS[tile_id]
    if tile_id in INFERRED_LABELS:
        return "inferred", INFERRED_LABELS[tile_id]
    if is_placeholder(tile):
        return "placeholder", "source placeholder"
    if usage_count:
        return "map034_unknown", "Map034 use; meaning?"
    return "unknown", "meaning unknown"


def tile_record(tileset, data, counts, layer_counts, tile_id):
    tile = source_tile(tileset, tile_id)
    status, label = classify_tile(tile_id, tile, counts[tile_id])
    return {
        "tileId": tile_id,
        "zone": "ice-rock-candidate"
        if tile_id < CORE_SNOW_FIRST_TILE_ID
        else "snow-ice-candidate",
        "status": status,
        "label": label,
        "map034Count": counts[tile_id],
        "map034Layers": layer_counts[tile_id],
        "sourceColumn": (tile_id - 384) % TILESET_COLUMNS,
        "sourceRow": (tile_id - 384) // TILESET_COLUMNS,
        "passage": data["passages"][tile_id],
        "priority": data["priorities"][tile_id],
        "terrainTag": data["terrainTags"][tile_id],
    }


def draw_wrapped(draw, position, value, font, fill, width=20, line_limit=2):
    lines = textwrap.wrap(value, width=width)[:line_limit] or [""]
    x, y = position
    for line_index, line in enumerate(lines):
        draw.text((x, y + line_index * 13), line, font=font, fill=fill)


def render_page(tileset, records, page_index, page_count, output_path):
    page_width = PAGE_MARGIN * 2 + PAGE_COLUMNS * CELL_WIDTH
    page_height = HEADER_HEIGHT + PAGE_MARGIN + PAGE_ROWS * CELL_HEIGHT
    page = Image.new("RGBA", (page_width, page_height), (17, 22, 25, 255))
    draw = ImageDraw.Draw(page)
    title_font = load_font(21, True)
    body_font = load_font(11)
    id_font = load_font(18, True)
    status_font = load_font(11, True)
    meta_font = load_font(9)

    first_id = records[0]["tileId"]
    last_id = records[-1]["tileId"]
    draw.text(
        (PAGE_MARGIN, 12),
        f"Frost Crystal Cave complete candidates  {first_id}-{last_id}  page {page_index + 1}/{page_count}",
        font=title_font,
        fill=(255, 228, 117, 255),
    )
    legend = "  ".join(
        f"{style['short']}={style['description']}"
        for style in STATUS_STYLE.values()
    )
    draw.text(
        (PAGE_MARGIN, 42),
        legend,
        font=load_font(10),
        fill=(184, 201, 207, 255),
    )
    draw.text(
        (PAGE_MARGIN, 61),
        "M34=count in Map034; L=layer counts; p=passage; pr=priority; t=terrain tag",
        font=load_font(10),
        fill=(151, 176, 185, 255),
    )

    preview_size = TILE_SIZE * 2
    checker = checkerboard(preview_size)
    for local_index, record in enumerate(records):
        column = local_index % PAGE_COLUMNS
        row = local_index // PAGE_COLUMNS
        x = PAGE_MARGIN + column * CELL_WIDTH
        y = HEADER_HEIGHT + row * CELL_HEIGHT
        style = STATUS_STYLE[record["status"]]
        fill = (31, 38, 42, 255) if (row + column) % 2 else (36, 43, 47, 255)
        draw.rectangle(
            (x, y, x + CELL_WIDTH - 5, y + CELL_HEIGHT - 5),
            fill=fill,
            outline=style["color"],
            width=2,
        )
        draw.rectangle((x, y, x + 5, y + CELL_HEIGHT - 5), fill=style["color"])
        page.alpha_composite(checker, (x + 12, y + 8))
        tile = source_tile(tileset, record["tileId"]).resize(
            (preview_size, preview_size), Image.Resampling.NEAREST
        )
        page.alpha_composite(tile, (x + 12, y + 8))

        text_x = x + 84
        draw.text(
            (text_x, y + 6),
            str(record["tileId"]),
            font=id_font,
            fill=(255, 229, 118, 255),
        )
        draw.text(
            (text_x, y + 30),
            style["short"],
            font=status_font,
            fill=style["color"],
        )
        draw.text(
            (text_x, y + 48),
            f"M34:{record['map034Count']}",
            font=meta_font,
            fill=(207, 218, 221, 255),
        )
        draw.text(
            (text_x, y + 61),
            f"L:{'/'.join(str(value) for value in record['map034Layers'])}",
            font=meta_font,
            fill=(169, 187, 192, 255),
        )
        draw_wrapped(
            draw,
            (x + 10, y + 76),
            record["label"],
            body_font,
            (229, 235, 236, 255),
        )
        draw.text(
            (x + 92, y + 95),
            f"p{record['passage']} pr{record['priority']} t{record['terrainTag']}",
            font=meta_font,
            fill=(145, 166, 173, 255),
        )

    page.save(output_path)


def make_contact_sheet(page_paths, output_path):
    columns = 2
    gap = 18
    thumb_width = 680
    label_height = 26
    thumbs = []
    for path in page_paths:
        image = Image.open(path).convert("RGBA")
        height = round(image.height * thumb_width / image.width)
        thumbs.append(image.resize((thumb_width, height), Image.Resampling.LANCZOS))
    thumb_height = max(image.height for image in thumbs)
    rows = math.ceil(len(thumbs) / columns)
    sheet = Image.new(
        "RGBA",
        (
            gap + columns * (thumb_width + gap),
            58 + rows * (thumb_height + label_height + gap),
        ),
        (15, 20, 23, 255),
    )
    draw = ImageDraw.Draw(sheet)
    draw.text(
        (gap, 14),
        "Frost Crystal Cave complete candidate catalog",
        font=load_font(24, True),
        fill=(255, 228, 117, 255),
    )
    for index, (path, thumb) in enumerate(zip(page_paths, thumbs)):
        x = gap + (index % columns) * (thumb_width + gap)
        y = 58 + (index // columns) * (thumb_height + label_height + gap)
        draw.text((x, y), path.stem, font=load_font(12, True), fill=(215, 227, 230, 255))
        sheet.alpha_composite(thumb, (x, y + label_height))
    sheet.save(output_path)


def write_readme(output_dir, page_paths, manifest_path, contact_path):
    lines = [
        "# Frost Crystal Cave complete tile candidates",
        "",
        f"- Candidate range: `{FIRST_TILE_ID}-{LAST_TILE_ID}`",
        f"- Ice-rock candidates: `{FIRST_TILE_ID}-{CORE_SNOW_FIRST_TILE_ID - 1}`",
        f"- Core snow/ice candidates: `{CORE_SNOW_FIRST_TILE_ID}-{LAST_TILE_ID}`",
        "- No tile is omitted, including placeholders and unknown meanings.",
        "- This catalog does not regenerate any exploration map.",
        "",
        "## Status",
        "",
    ]
    for status, style in STATUS_STYLE.items():
        lines.append(f"- `{style['short']}` (`{status}`): {style['description']}")
    lines.extend(
        [
            "",
            "## Files",
            "",
            f"- Contact sheet: `{contact_path.name}`",
            f"- Manifest: `{manifest_path.name}`",
        ]
    )
    lines.extend(f"- Page: `{path.name}`" for path in page_paths)
    (output_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    tileset = Image.open(TILESET_PATH).convert("RGBA")
    data, counts, layer_counts = map034_usage()
    records = [
        tile_record(tileset, data, counts, layer_counts, tile_id)
        for tile_id in range(FIRST_TILE_ID, LAST_TILE_ID + 1)
    ]

    page_count = math.ceil(len(records) / PAGE_TILE_COUNT)
    page_paths = []
    for page_index in range(page_count):
        page_records = records[
            page_index * PAGE_TILE_COUNT:(page_index + 1) * PAGE_TILE_COUNT
        ]
        path = args.output_dir / (
            f"frost_candidates_{page_records[0]['tileId']:04d}_"
            f"{page_records[-1]['tileId']:04d}.png"
        )
        render_page(tileset, page_records, page_index, page_count, path)
        page_paths.append(path)

    contact_path = args.output_dir / "frost_candidates_contact_sheet.png"
    make_contact_sheet(page_paths, contact_path)
    status_counts = Counter(record["status"] for record in records)
    manifest = {
        "tileset": portable_asset_path(TILESET_PATH, ESSENTIALS),
        "referenceMap": "Map034 Ice Cave",
        "candidateRange": [FIRST_TILE_ID, LAST_TILE_ID],
        "iceRockCandidateRange": [FIRST_TILE_ID, CORE_SNOW_FIRST_TILE_ID - 1],
        "coreSnowIceCandidateRange": [CORE_SNOW_FIRST_TILE_ID, LAST_TILE_ID],
        "tileCount": len(records),
        "statusCounts": dict(sorted(status_counts.items())),
        "statusLegend": STATUS_STYLE,
        "contactSheet": contact_path.name,
        "pages": [path.name for path in page_paths],
        "tiles": records,
    }
    manifest_path = args.output_dir / "frost_candidates_manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    write_readme(args.output_dir, page_paths, manifest_path, contact_path)

    print(
        f"tiles={len(records)} pages={len(page_paths)} "
        f"status={dict(sorted(status_counts.items()))}"
    )
    print(f"contact={contact_path}")
    print(f"manifest={manifest_path}")


if __name__ == "__main__":
    main()
