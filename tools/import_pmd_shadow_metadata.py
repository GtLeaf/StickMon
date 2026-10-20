#!/usr/bin/env python3
"""Register SpriteCollab shadow markers against the existing, cropped PMD art.

Run explicitly with a local SpriteCollab sprite directory (or a downloaded
subset). Firmware asset generation is offline and only reads the JSON output.
"""
import argparse
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageChops, ImageOps

import generate_pokemon_sprites as sprites

REVISION = "350b68033bed372c630a1384dbaef86f42f84fb8"
OUTPUT = sprites.ROOT / "origin_asset" / "pmd_shadow_metadata.json"
OVERRIDES = sprites.ROOT / "origin_asset" / "pmd_shadow_overrides.json"
ROWS = dict(front=0, down_right=1, right=2, up_right=3,
            back=4, up_left=5, left=6, down_left=7)


def silhouette(image):
    return image.getchannel("A").point(lambda value: 255 if value > 16 else 0)


def candidates(directory, direction, sleeping=False):
    root = ET.parse(directory / "AnimData.xml").getroot()
    size = int(root.findtext("ShadowSize"))
    color = [(0, 255, 0), (255, 0, 0), (0, 0, 255)][size]
    anims = {a.findtext("Name"): a for a in root.findall("./Anims/Anim")}
    result = []
    for name in (("Sleep",) if sleeping else ("Idle", "Walk")):
        anim = anims[name]
        while anim.findtext("CopyOf"):
            name = anim.findtext("CopyOf")
            anim = anims[name]
        width, height = int(anim.findtext("FrameWidth")), int(anim.findtext("FrameHeight"))
        bodies = Image.open(directory / f"{name}-Anim.png").convert("RGBA")
        shadows = Image.open(directory / f"{name}-Shadow.png").convert("RGBA")
        row = ROWS[direction] if bodies.height > height else 0
        for mirror in (False, True):
            source_row = ((8 - row) % 8 if mirror else row) if bodies.height > height else 0
            for index in range(bodies.width // width):
                box = (index * width, source_row * height, (index + 1) * width, (source_row + 1) * height)
                body, shadow = bodies.crop(box), shadows.crop(box)
                if mirror:
                    body, shadow = ImageOps.mirror(body), ImageOps.mirror(shadow)
                bounds = silhouette(body).getbbox()
                if not bounds:
                    continue
                centers, region = [], []
                for y in range(height):
                    for x in range(width):
                        pixel = shadow.getpixel((x, y))
                        if pixel[3] and pixel[:3] == (255, 255, 255):
                            centers.append((x, y))
                        if pixel[3] and pixel[:3] in (color, (255, 255, 255)):
                            region.append((x, y))
                if not centers or not region:
                    raise ValueError(f"Missing shadow markers: {directory}/{name} {source_row}:{index}")
                cx = sum(p[0] for p in centers) / len(centers)
                cy = sum(p[1] for p in centers) / len(centers)
                rx = (max(p[0] for p in region) - min(p[0] for p in region) + 1) / 2
                ry = (max(p[1] for p in region) - min(p[1] for p in region) + 1) / 2
                result.append((body.crop(bounds), bounds, (cx, cy, rx, ry),
                               [name, source_row, index, mirror]))
    return result


def register(image, references):
    bounds = silhouette(image).getbbox()
    crop = image.crop(bounds)
    mask = silhouette(crop)
    best = None
    for body, source_bounds, shadow, reference in references:
        scaled = body.resize(crop.size, Image.Resampling.NEAREST)
        other = silhouette(scaled)
        intersection = ImageChops.darker(mask, other).histogram()[255]
        union = ImageChops.lighter(mask, other).histogram()[255]
        iou = intersection / union
        # Palette changes are common between the PSD and SpriteCollab art.
        # Use color only to break silhouette ties, never as the main signal.
        color_error = sum(sum(abs(a - b) for a, b in zip(p[:3], q[:3]))
                          for p, q in zip(crop.getdata(), scaled.getdata())
                          if p[3] > 16 and q[3] > 16) / max(1, intersection * 765)
        sx, sy = crop.width / body.width, crop.height / body.height
        score = iou - abs(sx - sy) * 0.015 - color_error * 0.01
        if best is None or score > best[0]:
            cx, cy, rx, ry = shadow
            geometry = [round(bounds[0] + (cx - source_bounds[0]) * sx),
                        round(bounds[1] + (cy - source_bounds[1]) * sy),
                        max(1, round(rx * sx)), max(1, round(ry * sy))]
            best = (score, iou, geometry, reference)
    return best[1:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sprite_directory", type=Path)
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    frames = {}
    low = []
    overrides = json.loads(OVERRIDES.read_text())["frames"] if OVERRIDES.exists() else {}
    for spec in sprites.PMD_SPECS:
        directory = args.sprite_directory / f"{spec.species_id:04d}"
        paths = set()
        for direction in spec.directions:
            paths.update(sprites.pmd_idle_frame_path(spec, direction, i) for i in range(spec.idle_frames))
            paths.update(sprites.pmd_walking_frame_path(spec, direction, i) for i in range(spec.walking_frames))
        paths.update(sprites.pmd_path(spec, "sleeping", str(i)) for i in range(spec.sleeping_frames))
        refs = {}
        for path in sorted(paths):
            direction = path.stem.rsplit("_", 1)[0] if path.parent.name != "sleeping" else "front"
            ref_key = (direction, path.parent.name == "sleeping")
            if ref_key not in refs:
                refs[ref_key] = candidates(directory, *ref_key)
            iou, shadow, reference = register(sprites.to_rgba(path), refs[ref_key])
            key = path.relative_to(sprites.PROCESSED).as_posix()
            frames[key] = dict(shadow=shadow, sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                               iou=round(iou, 4), reference=reference)
            if key in overrides:
                override = overrides[key]
                if override["sha256"] != frames[key]["sha256"]:
                    raise ValueError(f"Stale manual shadow calibration: {key}")
                frames[key].update(shadow=override["shadow"], calibration="local-review")
            if iou < 0.90:
                low.append((key, round(iou, 3), shadow))
    payload = dict(schema=1, source="https://github.com/PMDCollab/SpriteCollab",
                   revision=REVISION, coordinates="canvas pixels; center x/y, ellipse radii x/y",
                   frames=frames)
    args.output.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"Registered {len(frames)} source frames; {len(low)} below 0.90 IoU")
    for item in low:
        print(*item)


if __name__ == "__main__":
    main()
