#!/usr/bin/env python3
import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw


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


def composite(layout, base_path, furniture_dir, mode):
    width = layout["canvas"]["width"]
    height = layout["canvas"]["height"]
    base = Image.open(base_path).convert("RGBA")
    base = fit_base(base, width, height, layout.get("base", {}).get("fit", "cover"))
    out = base.copy()

    furniture_root = Path(furniture_dir)
    for item in sorted(layout.get("furniture", []), key=lambda v: (v.get("z", 0), v.get("id", ""))):
        if not item.get("visible", True):
            continue
        src = Image.open(furniture_root / item["fileName"]).convert("RGBA")
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
    parser.add_argument("--base", required=True, help="Empty room base image")
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
