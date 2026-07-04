#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw


TOOL_DIR = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[2]
PROJECT_SPRITE_DIR = ROOT / "origin_asset" / "generated" / "pokemon_sprites"


def fit_base(img, width, height, fit):
    if fit == "stretch":
        return img.resize((width, height), Image.Resampling.NEAREST)

    src_ratio = img.width / img.height
    dst_ratio = width / height

    if fit == "contain":
        scale = min(width / img.width, height / img.height)
        resized = img.resize(
            (round(img.width * scale), round(img.height * scale)),
            Image.Resampling.NEAREST,
        )
        out = Image.new("RGBA", (width, height), (17, 19, 24, 255))
        out.alpha_composite(resized, ((width - resized.width) // 2, (height - resized.height) // 2))
        return out

    if src_ratio > dst_ratio:
        crop_w = round(img.height * dst_ratio)
        left = (img.width - crop_w) // 2
        img = img.crop((left, 0, left + crop_w, img.height))
    else:
        crop_h = round(img.width / dst_ratio)
        top = (img.height - crop_h) // 2
        img = img.crop((0, top, img.width, top + crop_h))
    return img.resize((width, height), Image.Resampling.NEAREST)


def apply_night(img, night):
    tint = int(night.get("tint", 46)) / 100
    darken = int(night.get("darken", 34)) / 100
    lamp = int(night.get("lamp", 36)) / 100

    blue = Image.new("RGBA", img.size, (8, 18, 42, round(255 * tint)))
    out = Image.alpha_composite(img, blue)
    black = Image.new("RGBA", img.size, (0, 0, 0, round(255 * darken)))
    out = Image.alpha_composite(out, black)

    if lamp > 0:
        glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(glow)
        cx, cy = 170, 70
        for radius in range(68, 2, -2):
            alpha = int(255 * lamp * (1 - radius / 68) * 0.16)
            draw.ellipse(
                (cx - radius, cy - radius, cx + radius, cy + radius),
                fill=(255, 190, 95, alpha),
            )
        out = Image.alpha_composite(out, glow)
    return out


def room_size(layout):
    geometry = layout.get("roomGeometry") or {}
    room = geometry.get("room") or {}
    canvas = layout.get("canvas") or {}
    return int(room.get("width") or canvas.get("width") or 240), int(room.get("height") or canvas.get("height") or 135)


def render_geometry(layout):
    width, height = room_size(layout)
    out = Image.new("RGBA", (width, height), (5, 7, 12, 255))
    draw = ImageDraw.Draw(out)
    for face in sorted(layout.get("roomGeometry", {}).get("faces", []), key=lambda v: v.get("drawOrder", 0)):
        points = [(int(x), int(y)) for x, y in face.get("points", [])]
        if len(points) < 3:
            continue
        fill = (182, 145, 108, 255) if face.get("type") == "wall" else (217, 151, 82, 255)
        outline = (78, 53, 42, 255)
        draw.polygon(points, fill=fill)
        draw.line(points + [points[0]], fill=outline, width=1)
    return out


def render_base(layout, base_path):
    width, height = room_size(layout)
    if layout.get("roomGeometry", {}).get("faces"):
        return render_geometry(layout)
    if not base_path:
        raise ValueError("--base is required when roomGeometry.faces is empty")
    base = Image.open(base_path).convert("RGBA")
    return fit_base(base, width, height, layout.get("base", {}).get("fit", "cover"))


def resolve_item_path(item, furniture_root):
    file_name = item.get("fileName")
    if not file_name:
        raise FileNotFoundError("layout item is missing fileName")

    raw = Path(file_name)
    candidates = []
    if raw.is_absolute():
        candidates.append(raw)
    else:
        candidates.extend([
            furniture_root / raw,
            TOOL_DIR / raw,
        ])
        if item.get("source") == "project_sprite":
            candidates.append(PROJECT_SPRITE_DIR / raw.name)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    tried = ", ".join(str(path) for path in candidates)
    raise FileNotFoundError(f"asset not found for {file_name}; tried {tried}")


def composite(layout, base_path, furniture_dir, mode):
    out = render_base(layout, base_path)

    furniture_root = Path(furniture_dir)
    for item in sorted(layout.get("furniture", []), key=lambda v: (v.get("z", 0), v.get("id", ""))):
        if not item.get("visible", True):
            continue
        src = Image.open(resolve_item_path(item, furniture_root)).convert("RGBA")
        scale = float(item.get("scale", 1))
        if scale != 1:
            size = (max(1, round(src.width * scale)), max(1, round(src.height * scale)))
            src = src.resize(size, Image.Resampling.NEAREST)
        opacity = float(item.get("opacity", 1))
        if opacity < 1:
            alpha = src.getchannel("A").point(lambda p: int(p * opacity))
            src.putalpha(alpha)
        out.alpha_composite(src, (int(item.get("x", 0)), int(item.get("y", 0))))

    if mode == "night":
        out = apply_night(out, layout.get("night", {}))
    return out


def main():
    parser = argparse.ArgumentParser(description="Compose StickMon room day/night previews from room_layout.json.")
    parser.add_argument("--layout", required=True, help="Exported room_layout.json")
    parser.add_argument("--base", help="Fallback empty room base image for old layouts without roomGeometry")
    parser.add_argument("--furniture-dir", required=True, help="Directory containing furniture PNG files")
    parser.add_argument("--out", default="room_preview", help="Output prefix")
    args = parser.parse_args()

    layout = json.loads(Path(args.layout).read_text(encoding="utf-8"))
    out_prefix = Path(args.out)
    composite(layout, args.base, args.furniture_dir, "day").save(
        out_prefix.with_name(out_prefix.name + "_day.png")
    )
    composite(layout, args.base, args.furniture_dir, "night").save(
        out_prefix.with_name(out_prefix.name + "_night.png")
    )


if __name__ == "__main__":
    main()
