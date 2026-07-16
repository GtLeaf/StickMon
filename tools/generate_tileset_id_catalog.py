#!/usr/bin/env python3

import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ESSENTIALS = Path(
    "${ESSENTIALS_DIR}"
)
DEFAULT_TILESET = ESSENTIALS / "Graphics" / "Tilesets" / "Caves.png"
DEFAULT_OUTPUT_DIR = (
    ROOT / "origin_asset" / "generated" / "game" / "tileset_reference" / "caves"
)
TILE_SIZE = 32
FIRST_REGULAR_TILE_ID = 384
ROWS_PER_PAGE = 16
SCALE = 2
CELL_WIDTH = 80
CELL_HEIGHT = 92
PAGE_MARGIN = 20
HEADER_HEIGHT = 58


def load_font(size, bold=False):
    candidates = (
        Path("/System/Library/Fonts/SFNSMono.ttf"),
        Path("/System/Library/Fonts/Supplemental/Arial Bold.ttf") if bold else
        Path("/System/Library/Fonts/Supplemental/Arial.ttf"),
    )
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), size)
    return ImageFont.load_default()


def checkerboard(size, square=8):
    image = Image.new("RGBA", (size, size), (202, 207, 210, 255))
    draw = ImageDraw.Draw(image)
    for y in range(0, size, square):
        for x in range(0, size, square):
            if (x // square + y // square) % 2:
                draw.rectangle(
                    (x, y, x + square - 1, y + square - 1),
                    fill=(231, 234, 236, 255),
                )
    return image


def render_page(tileset, columns, start_row, row_count, page_index, page_count):
    page_width = PAGE_MARGIN * 2 + columns * CELL_WIDTH
    page_height = HEADER_HEIGHT + PAGE_MARGIN + row_count * CELL_HEIGHT
    page = Image.new("RGBA", (page_width, page_height), (21, 25, 27, 255))
    draw = ImageDraw.Draw(page)
    header_font = load_font(20, bold=True)
    id_font = load_font(14, bold=True)
    small_font = load_font(12)

    first_id = FIRST_REGULAR_TILE_ID + start_row * columns
    last_id = first_id + row_count * columns - 1
    title = (
        f"Caves.png  page {page_index + 1:02d}/{page_count:02d}  "
        f"tile IDs {first_id}-{last_id}"
    )
    draw.text((PAGE_MARGIN, 14), title, fill=(248, 248, 244, 255), font=header_font)
    draw.text(
        (PAGE_MARGIN, 40),
        "ID = 384 + source row * 8 + source column",
        fill=(157, 177, 184, 255),
        font=small_font,
    )

    preview_size = TILE_SIZE * SCALE
    checker = checkerboard(preview_size)
    for local_row in range(row_count):
        source_row = start_row + local_row
        for column in range(columns):
            tile_id = FIRST_REGULAR_TILE_ID + source_row * columns + column
            cell_x = PAGE_MARGIN + column * CELL_WIDTH
            cell_y = HEADER_HEIGHT + local_row * CELL_HEIGHT
            tile_x = cell_x + (CELL_WIDTH - preview_size) // 2
            tile_y = cell_y + 5

            fill = (36, 42, 45, 255) if (local_row + column) % 2 else (42, 48, 51, 255)
            draw.rectangle(
                (cell_x, cell_y, cell_x + CELL_WIDTH - 1, cell_y + CELL_HEIGHT - 1),
                fill=fill,
                outline=(80, 91, 96, 255),
                width=1,
            )
            page.alpha_composite(checker, (tile_x, tile_y))
            tile = tileset.crop((
                column * TILE_SIZE,
                source_row * TILE_SIZE,
                (column + 1) * TILE_SIZE,
                (source_row + 1) * TILE_SIZE,
            )).resize((preview_size, preview_size), Image.Resampling.NEAREST)
            page.alpha_composite(tile, (tile_x, tile_y))

            label = str(tile_id)
            label_box = draw.textbbox((0, 0), label, font=id_font)
            label_width = label_box[2] - label_box[0]
            draw.text(
                (cell_x + (CELL_WIDTH - label_width) // 2, tile_y + preview_size + 5),
                label,
                fill=(255, 224, 116, 255),
                font=id_font,
            )
    return page, first_id, last_id


def make_contact_sheet(page_records, output_path):
    thumbnail_width = 250
    gap = 18
    columns = 3
    label_height = 28
    rows = math.ceil(len(page_records) / columns)
    thumbnails = []
    for record in page_records:
        page = Image.open(record["path"]).convert("RGBA")
        height = round(page.height * thumbnail_width / page.width)
        thumbnails.append(page.resize((thumbnail_width, height), Image.Resampling.NEAREST))
    thumbnail_height = max(image.height for image in thumbnails)
    sheet = Image.new(
        "RGBA",
        (
            gap + columns * (thumbnail_width + gap),
            56 + rows * (thumbnail_height + label_height + gap),
        ),
        (18, 22, 24, 255),
    )
    draw = ImageDraw.Draw(sheet)
    title_font = load_font(22, bold=True)
    label_font = load_font(13, bold=True)
    draw.text((gap, 15), "Caves.png numbered tile catalog", fill=(255, 232, 128, 255), font=title_font)
    for index, (record, thumbnail) in enumerate(zip(page_records, thumbnails)):
        column = index % columns
        row = index // columns
        x = gap + column * (thumbnail_width + gap)
        y = 56 + row * (thumbnail_height + label_height + gap)
        label = f"page {index + 1:02d}: {record['firstId']}-{record['lastId']}"
        draw.text((x, y), label, fill=(235, 239, 240, 255), font=label_font)
        sheet.alpha_composite(thumbnail, (x, y + label_height))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path)


def generate_catalog(tileset_path, output_dir, rows_per_page=ROWS_PER_PAGE):
    tileset = Image.open(tileset_path).convert("RGBA")
    if tileset.width % TILE_SIZE or tileset.height % TILE_SIZE:
        raise ValueError("tileset dimensions must be divisible by 32")
    columns = tileset.width // TILE_SIZE
    total_rows = tileset.height // TILE_SIZE
    page_count = math.ceil(total_rows / rows_per_page)
    output_dir.mkdir(parents=True, exist_ok=True)

    page_records = []
    for page_index in range(page_count):
        start_row = page_index * rows_per_page
        row_count = min(rows_per_page, total_rows - start_row)
        page, first_id, last_id = render_page(
            tileset,
            columns,
            start_row,
            row_count,
            page_index,
            page_count,
        )
        page_path = output_dir / f"caves_tiles_ids_{first_id:04d}_{last_id:04d}.png"
        page.save(page_path)
        page_records.append({
            "file": page_path.name,
            "path": str(page_path),
            "firstId": first_id,
            "lastId": last_id,
            "sourceRowStart": start_row,
            "sourceRowCount": row_count,
        })

    contact_path = output_dir / "caves_tiles_ids_contact_sheet.png"
    make_contact_sheet(page_records, contact_path)
    manifest = {
        "tileset": str(tileset_path),
        "tileSize": TILE_SIZE,
        "columns": columns,
        "rows": total_rows,
        "firstId": FIRST_REGULAR_TILE_ID,
        "lastId": FIRST_REGULAR_TILE_ID + columns * total_rows - 1,
        "tileCount": columns * total_rows,
        "rowsPerPage": rows_per_page,
        "contactSheet": contact_path.name,
        "pages": [
            {key: value for key, value in record.items() if key != "path"}
            for record in page_records
        ],
    }
    manifest_path = output_dir / "caves_tiles_ids_manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return manifest_path, contact_path, page_records


def main():
    parser = argparse.ArgumentParser(
        description="Generate paginated RPG Maker XP regular-tile ID catalogs"
    )
    parser.add_argument("--tileset", type=Path, default=DEFAULT_TILESET)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--rows-per-page", type=int, default=ROWS_PER_PAGE)
    args = parser.parse_args()
    if args.rows_per_page < 1:
        parser.error("--rows-per-page must be at least 1")

    manifest_path, contact_path, pages = generate_catalog(
        args.tileset,
        args.output_dir,
        args.rows_per_page,
    )
    print(f"tiles={pages[0]['firstId']}-{pages[-1]['lastId']} pages={len(pages)}")
    print(f"contact={contact_path}")
    print(f"manifest={manifest_path}")


if __name__ == "__main__":
    main()
