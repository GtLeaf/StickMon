"""Generate an expanded, test-only wallpaper catalog for native host journeys."""
import json
import shutil
import tempfile
from contextlib import contextmanager
from pathlib import Path

from PIL import Image, ImageDraw

import generate_room_wallpaper as wallpaper


@contextmanager
def wallpaper_catalog_fixture(resource_root):
    """Use the production generator and complete resource root in a temporary tree."""
    with tempfile.TemporaryDirectory(prefix="wallpaper-catalog-") as directory:
        tmp = Path(directory)
        art = tmp / "art"
        shutil.copytree(wallpaper.ART, art)
        manifest = art / "catalog.json"
        official = {entry["id"]: entry for entry in json.loads(manifest.read_text(encoding="utf-8"))}
        entries = [official[0], official[1]]
        with Image.open(art / "walls-mask.png") as mask:
            size = mask.size
        with Image.open(art / "swatch-forest.png") as swatch:
            silhouette = swatch.convert("RGBA").getchannel("A")
        # Non-sequential IDs make accidentally saving a display index observable.
        for stable_id, name, price, color in (
            (37, "花", 137, (240, 40, 136)),
            (91, "海", 215, (32, 168, 224)),
            (8, "草", 55, (168, 200, 64)),
            (64, "月", 70, (160, 112, 216)),
        ):
            key = f"fixture-{stable_id}"
            overlay = Image.new("RGBA", size, color + (255,))
            accent = tuple(channel // 2 for channel in color) + (255,)
            # A mostly solid ground makes the overlay independently recognizable
            # in both the room preview and the differently scaled home camera.
            painter = ImageDraw.Draw(overlay)
            for x in range(-size[1], size[0], 24):
                painter.line((x, 0, x + size[1], size[1]), fill=accent, width=2)
            overlay.save(art / f"{key}.png")
            icon = overlay.crop((0, 0, *silhouette.size))
            icon.putalpha(silhouette)
            icon.save(art / f"swatch-{key}.png")
            entries.append({"id": stable_id, "name": name, "price": price,
                            "asset": key, "swatch": f"swatch-{key}.png"})
        manifest.write_text(json.dumps(entries, ensure_ascii=False), encoding="utf-8")
        shutil.copytree(resource_root, tmp / "data")
        wallpaper.generate(manifest, tmp)
        headers = tmp / "src/assets"
        yield tmp / "data", [
            f'-DSTICKMON_WALLPAPER_CATALOG_INCLUDE="{headers}/WallpaperCatalog.inc"',
            f'-DSTICKMON_WALLPAPER_ICONS_INCLUDE="{headers}/WallpaperIcons.inc"',
            '-DSTICKMON_TEST_WALLPAPER_CATALOG=1',
        ]
