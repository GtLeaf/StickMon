#!/usr/bin/env python3
import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


TOOL_DIR = Path(__file__).resolve().parent
ROOT = Path(__file__).resolve().parents[2]
PROJECT_SPRITE_DIR = TOOL_DIR / "generated" / "pokemon_sprites"
FURNITURE_LIBRARY_DIR = TOOL_DIR / "generated" / "furniture_library"
FURNITURE_LIBRARY_MANIFEST = FURNITURE_LIBRARY_DIR / "manifest.json"
WALL_MOUNTED_NAME_MARKERS = (
    "sheld",
    "wall_shelf",
    "wall-shelf",
    "wall shelf",
    "shelf_wall",
    "shelf-wall",
    "shelf wall",
    "mounted_shelf",
    "mounted-shelf",
    "mounted shelf",
    "hanging_shelf",
    "hanging-shelf",
    "hanging shelf",
    "壁架",
    "挂架",
    "墙架",
    "层板",
    "搁板",
)

DEFAULT_NIGHT = {
    "lightShape": "radial",
    "lightStrength": 28,
    "lightX": 170,
    "lightY": 70,
    "lightDepth": 180,
    "lightRadius": 68,
    "lightAngle": 125,
    "lightSpread": 42,
    "castShadows": True,
    "shadowMode": "auto",
    "shadowAlpha": 38,
    "shadowLength": 28,
    "shadowBlur": 0.6,
}


def clamp_number(value, fallback, minimum, maximum):
    try:
        number = float(value)
    except (TypeError, ValueError):
        number = fallback
    return max(minimum, min(maximum, number))


def normalize_light_shape(value):
    return value if value in {"radial", "cone"} else "radial"


def normalize_shadow_mode(value):
    return value if value in {"auto", "point", "directional"} else "auto"


def normalize_shadow_surface(value, face_type="floor"):
    if value in {"floor", "left_wall", "right_wall"}:
        return value
    return "left_wall" if face_type == "wall" else "floor"


def normalize_shadow_anchor(value):
    return value if value in {"auto", "floor", "wall"} else "auto"


def normalize_shadow_face_ids(value):
    if not isinstance(value, list):
        return []
    seen = set()
    ids = []
    for raw in value:
        face_id = str(raw or "").strip()
        if face_id and face_id not in seen:
            seen.add(face_id)
            ids.append(face_id)
    return ids


def normalize_light_settings(light, fallback=None):
    light = light or {}
    fallback = fallback or DEFAULT_NIGHT
    return {
        "lightShape": normalize_light_shape(light.get("lightShape", fallback["lightShape"])),
        "lightStrength": clamp_number(light.get("lightStrength"), fallback["lightStrength"], 0, 200),
        "lightX": clamp_number(light.get("lightX"), fallback["lightX"], -2048, 2048),
        "lightY": clamp_number(light.get("lightY"), fallback["lightY"], -2048, 2048),
        "lightDepth": clamp_number(light.get("lightDepth"), fallback["lightDepth"], 40, 480),
        "lightRadius": clamp_number(light.get("lightRadius"), fallback["lightRadius"], 8, 240),
        "lightAngle": clamp_number(light.get("lightAngle"), fallback["lightAngle"], 0, 359),
        "lightSpread": clamp_number(light.get("lightSpread"), fallback["lightSpread"], 5, 160),
    }


def normalize_night(night, mode=None):
    night = night or {}
    shared_light = normalize_light_settings(night, DEFAULT_NIGHT)
    day_light = normalize_light_settings(night.get("dayLight"), shared_light)
    night_light = normalize_light_settings(night.get("nightLight"), shared_light)
    separate = night.get("separateModeLights") is True
    active_light = night_light if separate and mode == "night" else day_light if separate and mode == "day" else shared_light
    return {
        **active_light,
        "separateModeLights": separate,
        "dayLight": day_light,
        "nightLight": night_light,
        "castShadows": night.get("castShadows", DEFAULT_NIGHT["castShadows"]) is not False,
        "shadowMode": normalize_shadow_mode(night.get("shadowMode", DEFAULT_NIGHT["shadowMode"])),
        "shadowAlpha": clamp_number(night.get("shadowAlpha"), DEFAULT_NIGHT["shadowAlpha"], 0, 80),
        "shadowLength": clamp_number(night.get("shadowLength"), DEFAULT_NIGHT["shadowLength"], 0, 80),
        "shadowBlur": clamp_number(night.get("shadowBlur"), DEFAULT_NIGHT["shadowBlur"], 0, 12),
    }


def background_info(layout, mode):
    backgrounds = layout.get("backgrounds") or {}
    info = backgrounds.get(mode) or {}
    return info if isinstance(info, dict) else {}


def background_layer_visible(layer, mode):
    if layer.get("visible", True) is False:
        return False
    if mode == "night":
        return layer.get("visibleInNight", True) is not False
    return layer.get("visibleInDay", True) is not False


def background_layers_for_mode(layout, mode):
    layers = layout.get("backgroundLayers") or (layout.get("roomGeometry", {}).get("backgrounds", {}) or {}).get("layers") or []
    if not isinstance(layers, list):
        return []
    return sorted(
        [layer for layer in layers if isinstance(layer, dict) and background_layer_visible(layer, mode)],
        key=lambda layer: (layer.get("z", 0), layer.get("id", "")),
    )


def resolve_background_layer_path(layer, base_path=None, layout_dir=None):
    file_name = layer.get("fileName") or layer.get("name")
    if not file_name:
        return None
    raw = Path(file_name)
    candidates = []
    if raw.is_absolute():
        candidates.append(raw)
    else:
        if layout_dir:
            candidates.append(Path(layout_dir) / raw)
        if base_path:
            candidates.append(Path(base_path).parent / raw)
        candidates.extend([
            ROOT / "origin_asset" / "room" / raw,
            ROOT / "origin_asset" / "room" / "standar" / raw,
        ])
    for candidate in candidates:
        if candidate.exists():
            return candidate
    tried = ", ".join(str(path) for path in candidates)
    raise FileNotFoundError(f"background layer not found for {file_name}; tried {tried}")


def layout_trim(layout, img, mode=None):
    mode_info = background_info(layout, mode) if mode else {}
    base = layout.get("base") or {}
    backgrounds = layout.get("backgrounds") or {}
    geometry = layout.get("roomGeometry") or {}
    reference = geometry.get("reference") or {}
    prepared = geometry.get("prepared") or {}
    trim = mode_info.get("trim") or backgrounds.get("trim") or base.get("trim") or reference.get("trim") or prepared.get("trim") or {}
    try:
        x = int(round(float(trim.get("x", 0))))
        y = int(round(float(trim.get("y", 0))))
        w = int(round(float(trim.get("width", img.width))))
        h = int(round(float(trim.get("height", img.height))))
    except (TypeError, ValueError):
        return (0, 0, img.width, img.height)
    x = max(0, min(img.width - 1, x))
    y = max(0, min(img.height - 1, y))
    w = max(1, min(img.width - x, w))
    h = max(1, min(img.height - y, h))
    return (x, y, w, h)


def coerce_trim_tuple(trim, img):
    if not isinstance(trim, dict):
        return None
    try:
        x = int(round(float(trim.get("x", 0))))
        y = int(round(float(trim.get("y", 0))))
        w = int(round(float(trim.get("width", img.width))))
        h = int(round(float(trim.get("height", img.height))))
    except (TypeError, ValueError):
        return None
    x = max(0, min(img.width - 1, x))
    y = max(0, min(img.height - 1, y))
    w = max(1, min(img.width - x, w))
    h = max(1, min(img.height - y, h))
    return (x, y, w, h)


def fit_base(img, width, height, fit, trim=None):
    if fit == "fit_width":
        x, y, w, h = trim or (0, 0, img.width, img.height)
        return img.crop((x, y, x + w, y + h)).resize((width, height), Image.Resampling.NEAREST)

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


def angle_delta(a, b):
    return (a - b + math.pi) % (math.pi * 2) - math.pi


def light_mask(size, night, mode):
    strength = night["lightStrength"] / 100
    if strength <= 0:
        return None
    width, height = size
    cx = float(night["lightX"])
    cy = float(night["lightY"])
    radius = max(1, float(night["lightRadius"]))
    direction = math.radians(float(night["lightAngle"]))
    half_spread = math.radians(float(night["lightSpread"])) * 0.5
    mode_alpha = 0.72 if mode == "night" else 0.42
    max_alpha = min(255, 255 * strength * mode_alpha)
    shape = night["lightShape"]

    pixels = []
    for y in range(height):
        for x in range(width):
            dx = x - cx
            dy = y - cy
            dist = math.hypot(dx, dy)
            if dist > radius:
                pixels.append(0)
                continue
            if shape == "cone":
                if dist < 0.001:
                    cone_factor = 1
                else:
                    delta = abs(angle_delta(math.atan2(dy, dx), direction))
                    if delta > half_spread:
                        pixels.append(0)
                        continue
                    cone_factor = 1 - (delta / max(half_spread, 0.001)) * 0.28
            else:
                cone_factor = 1
            falloff = max(0, 1 - dist / radius) ** 1.35
            pixels.append(int(max_alpha * falloff * cone_factor))

    mask = Image.new("L", size, 0)
    mask.putdata(pixels)
    return mask


def apply_lighting(img, night, mode):
    night = normalize_night(night, mode)
    out = img
    mask = light_mask(out.size, night, mode)
    if mask:
        glow = Image.new("RGBA", out.size, (255, 250, 220, 0))
        glow.putalpha(mask)
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
        if face.get("visible", True) is False:
            continue
        if face.get("type") == "sprite_area":
            continue
        points = [(int(x), int(y)) for x, y in face.get("points", [])]
        if len(points) < 3:
            continue
        fill = (182, 145, 108, 255) if face.get("type") == "wall" else (217, 151, 82, 255)
        outline = (78, 53, 42, 255)
        draw.polygon(points, fill=fill)
        draw.line(points + [points[0]], fill=outline, width=1)
    return out


def render_base(layout, base_path, mode="day", layout_dir=None):
    width, height = room_size(layout)
    layers = background_layers_for_mode(layout, mode)
    if layers:
        out = Image.new("RGBA", (width, height), (5, 7, 12, 255))
        fit = background_info(layout, mode).get("fit") or layout.get("backgrounds", {}).get("fit") or layout.get("base", {}).get("fit", "cover")
        for layer in layers:
            layer_path = resolve_background_layer_path(layer, base_path, layout_dir)
            if not layer_path:
                continue
            base = Image.open(layer_path).convert("RGBA")
            layer_trim = coerce_trim_tuple(layer.get("trim"), base) or layout_trim(layout, base, mode)
            fitted = fit_base(base, width, height, layer.get("fit") or fit, layer_trim)
            opacity = max(0, min(1, float(layer.get("opacity", 1))))
            if opacity < 1:
                alpha = fitted.getchannel("A").point(lambda p: int(p * opacity))
                fitted.putalpha(alpha)
            out.alpha_composite(fitted)
        return out
    if base_path:
        base = Image.open(base_path).convert("RGBA")
        fit = background_info(layout, mode).get("fit") or layout.get("backgrounds", {}).get("fit") or layout.get("base", {}).get("fit", "cover")
        return fit_base(base, width, height, fit, layout_trim(layout, base, mode))
    if layout.get("roomGeometry", {}).get("faces"):
        return render_geometry(layout)
    raise ValueError("--base is required when roomGeometry.faces is empty")


def furniture_library_items():
    if not FURNITURE_LIBRARY_MANIFEST.exists():
        return []
    try:
        data = json.loads(FURNITURE_LIBRARY_MANIFEST.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    items = data.get("items", [])
    return items if isinstance(items, list) else []


def furniture_library_candidates(item):
    library_id = str(item.get("libraryId") or "")
    file_name = str(item.get("fileName") or "")
    candidates = []
    for entry in furniture_library_items():
        if not isinstance(entry, dict):
            continue
        entry_id = str(entry.get("id") or "")
        entry_file = str(entry.get("fileName") or "")
        if library_id and entry_id != library_id:
            continue
        if not library_id and entry_file != file_name:
            continue
        image = entry.get("image")
        if isinstance(image, dict):
            image = image.get("path")
        if isinstance(image, str) and image:
            candidates.append(FURNITURE_LIBRARY_DIR / image)
        if entry_id:
            candidates.append(FURNITURE_LIBRARY_DIR / "images" / f"{entry_id}.png")
        if entry_file:
            candidates.append(FURNITURE_LIBRARY_DIR / "images" / Path(entry_file).name)
    return candidates


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
        if item.get("source", "furniture") == "furniture":
            candidates.extend(furniture_library_candidates(item))
        if item.get("source") == "project_sprite":
            candidates.append(PROJECT_SPRITE_DIR / raw.name)

    for candidate in candidates:
        if candidate.exists():
            return candidate

    tried = ", ".join(str(path) for path in candidates)
    raise FileNotFoundError(f"asset not found for {file_name}; tried {tried}")


def item_size(item, src):
    target_width = item.get("targetWidth")
    target_height = item.get("targetHeight")
    if target_width and target_height:
        return (max(1, round(float(target_width))), max(1, round(float(target_height))))
    scale = float(item.get("scale", 1))
    scale_x = float(item.get("scaleX", scale))
    scale_y = float(item.get("scaleY", scale))
    return (max(1, round(src.width * scale_x)), max(1, round(src.height * scale_y)))


ITEM_HEIGHT_DEFAULTS = {
    "wall_shelf": 18,
    "bed_sofa": 34,
    "food_bowl": 8,
    "decoration_prop": 16,
    "rug": 0,
    "floor_decal": 0,
    "low_prop": 8,
    "furniture": 32,
    "tall_furniture": 56,
    "wall_furniture": 24,
    "wall_prop": 0,
}

ITEM_WALL_DEPTH_DEFAULTS = {
    "wall_shelf": 36,
    "wall_furniture": 24,
    "wall_prop": 4,
}


def item_height_px(item):
    kind = item.get("kind", "furniture")
    return clamp_number(item.get("heightPx"), ITEM_HEIGHT_DEFAULTS.get(kind, 32), 0, 160)


def item_casts_shadow(item):
    if "castsShadow" in item:
        return item.get("castsShadow") is not False
    return item.get("kind", "furniture") not in {"rug", "floor_decal", "wall_prop"}


def item_shadow_opacity(night, item):
    return clamp_number(item.get("shadowOpacity", item.get("shadowAlpha")), night["shadowAlpha"], 0, 100)


def item_shadow_length(night, item):
    return clamp_number(item.get("shadowLength"), night["shadowLength"], 0, 120)


def item_shadow_blur(night, item):
    return clamp_number(item.get("shadowBlur"), night["shadowBlur"], 0, 16)


def item_wall_shadow_offset_y(item):
    return clamp_number(item.get("wallShadowOffsetY"), 0, -80, 80)


def item_wall_shadow_depth_px(item):
    kind = item.get("kind", "furniture")
    fallback = ITEM_WALL_DEPTH_DEFAULTS.get(kind, 24 if item_uses_wall_shadow_anchor(item) else 0)
    return clamp_number(item.get("wallShadowDepthPx"), fallback, 0, 160)


def item_footprint(item):
    value = item.get("footprint", "auto")
    return value if value in {"auto", "ellipse", "rect", "polygon", "none"} else "auto"


def normalize_local_polygon(value):
    if not isinstance(value, list):
        return []
    points = []
    for raw in value:
        if isinstance(raw, dict):
            x = raw.get("x")
            y = raw.get("y")
        elif isinstance(raw, (list, tuple)) and len(raw) >= 2:
            x, y = raw[0], raw[1]
        else:
            continue
        try:
            x = max(0, min(1, float(x)))
            y = max(0, min(1, float(y)))
        except (TypeError, ValueError):
            continue
        points.append((x, y))
    return points if len(points) >= 3 else []


def default_footprint_polygon():
    return [(0.18, 0.72), (0.82, 0.72), (0.96, 0.96), (0.04, 0.96)]


def item_uses_auto_footprint_polygon(item):
    return item_footprint(item) == "polygon" and item.get("kind") not in {"rug", "floor_decal"}


def item_footprint_polygon(item):
    custom = normalize_local_polygon(item.get("footprintPolygon"))
    if custom:
        return custom
    if item_uses_auto_footprint_polygon(item):
        return default_footprint_polygon()
    return []


def item_shadow_polygon(item):
    return normalize_local_polygon(item.get("shadowPolygon"))


def item_uses_wall_shadow_anchor(item):
    anchor = normalize_shadow_anchor(item.get("shadowAnchor"))
    if anchor == "wall":
        return True
    if anchor == "floor":
        return False

    kind = item.get("kind", "furniture")
    if kind in {"wall_prop", "wall_furniture", "wall_shelf"}:
        return True
    if str(item.get("slot", "")).lower() == "wall":
        return True

    name = f"{item.get('fileName', '')} {item.get('name', '')}".lower()
    return any(marker in name for marker in WALL_MOUNTED_NAME_MARKERS)


def item_shadow_face_ids(item):
    return normalize_shadow_face_ids(item.get("shadowFaceIds"))


def item_explicitly_targets_face(item, face):
    if not face:
        return False
    return str(face.get("id", "")) in item_shadow_face_ids(item)


def item_should_cast_shadow_on_surface(item, src, surface, face=None):
    target_face_ids = item_shadow_face_ids(item)
    targeted = item_explicitly_targets_face(item, face)
    if target_face_ids and not targeted:
        return False
    if face and face.get("receivesShadow", True) is False and not targeted:
        return False
    is_wall_surface = surface in {"left_wall", "right_wall"}
    if target_face_ids:
        return is_wall_surface
    if not item_uses_wall_shadow_anchor(item):
        return True
    if face is None:
        return True
    return is_wall_surface


def active_shadow_mode(night):
    mode = normalize_shadow_mode(night.get("shadowMode"))
    if mode != "auto":
        return mode
    return "point"


def shadow_direction(night, foot_x, foot_y, light_x, light_y):
    if active_shadow_mode(night) == "directional":
        angle = math.radians(float(night["lightAngle"]))
        return math.cos(angle), math.sin(angle)

    dx = foot_x - light_x
    dy = foot_y - light_y
    distance = math.hypot(dx, dy)
    if distance < 0.001:
        return 0, 1
    return dx / distance, dy / distance


def shadow_light_factor(night, foot_x, foot_y, light_x, light_y):
    strength = night["lightStrength"] / 100
    if strength <= 0:
        return 0

    radius = max(1, float(night["lightRadius"]))
    dx = foot_x - light_x
    dy = foot_y - light_y
    dist = math.hypot(dx, dy)
    if dist > radius:
        return 0

    cone_factor = 1
    if normalize_light_shape(night["lightShape"]) == "cone" and dist > 0.001:
        half_spread = max(0.001, math.radians(float(night["lightSpread"])) * 0.5)
        delta = abs(angle_delta(math.atan2(dy, dx), math.radians(float(night["lightAngle"]))))
        if delta > half_spread:
            return 0
        cone_factor = 1 - (delta / half_spread) * 0.28

    falloff = max(0, 1 - dist / radius) ** 0.7
    return math.sqrt(strength) * (0.45 + 0.55 * falloff) * cone_factor


def targeted_shadow_face_ids(layout):
    ids = set()
    for item in layout.get("furniture", []):
        ids.update(item_shadow_face_ids(item))
    return ids


def face_receives_shadow(face, targeted_face_ids=None):
    targeted_face_ids = targeted_face_ids or set()
    return (
        face.get("type") != "sprite_area"
        and (face.get("receivesShadow", True) is not False or str(face.get("id", "")) in targeted_face_ids)
        and len(face.get("points", [])) >= 3
    )


def face_shadow_surface(face):
    return normalize_shadow_surface(face.get("shadowSurface"), face.get("type", "floor"))


def receiving_shadow_faces(layout):
    targeted_face_ids = targeted_shadow_face_ids(layout)
    return [
        face
        for face in layout.get("roomGeometry", {}).get("faces", [])
        if face_receives_shadow(face, targeted_face_ids)
    ]


def wall_shadow_skew(surface):
    if surface == "left_wall":
        return -0.16
    if surface == "right_wall":
        return 0.16
    return 0


def face_projection_basis(points):
    if not points or len(points) < 3:
        return None
    origin = points[0]
    bottom_left = points[1]
    top_right = points[-1]
    raw_u = (top_right[0] - origin[0], top_right[1] - origin[1])
    raw_v = (bottom_left[0] - origin[0], bottom_left[1] - origin[1])
    u_len = math.hypot(*raw_u)
    v_len = math.hypot(*raw_v)
    if u_len < 0.001 or v_len < 0.001:
        return None
    u = (raw_u[0] / u_len, raw_u[1] / u_len)
    v = (raw_v[0] / v_len, raw_v[1] / v_len)
    det = u[0] * v[1] - u[1] * v[0]
    if abs(det) < 0.001:
        return None
    return {"origin": origin, "u": u, "v": v, "det": det}


def screen_to_face_local(basis, point):
    dx = point[0] - basis["origin"][0]
    dy = point[1] - basis["origin"][1]
    u = (dx * basis["v"][1] - dy * basis["v"][0]) / basis["det"]
    v = (basis["u"][0] * dy - basis["u"][1] * dx) / basis["det"]
    return (u, v)


def face_local_to_screen(basis, point):
    return (
        basis["origin"][0] + basis["u"][0] * point[0] + basis["v"][0] * point[1],
        basis["origin"][1] + basis["u"][1] * point[0] + basis["v"][1] * point[1],
    )


def project_wall_local_point(point, light, light_depth, caster_depth):
    safe_depth = min(max(0, caster_depth), max(0, light_depth - 1))
    if safe_depth <= 0:
        return point
    factor = light_depth / max(1, light_depth - safe_depth)
    return (
        light[0] + (point[0] - light[0]) * factor,
        light[1] + (point[1] - light[1]) * factor,
    )


def item_wall_shadow_depth_scaled(night, item):
    depth_scale = max(0.2, item_shadow_length(night, item) / DEFAULT_NIGHT["shadowLength"])
    return item_wall_shadow_depth_px(item) * depth_scale


def item_sort_key(item):
    return (
        item.get("z", 0),
        item.get("sortY", item.get("y", 0)),
        item.get("id", ""),
    )


def item_visible_in_mode(item, mode):
    if not item.get("visible", True):
        return False
    if mode == "night":
        return item.get("visibleInNight", True) is not False
    return item.get("visibleInDay", True) is not False


def load_prepared_items(layout, furniture_root, mode):
    prepared = []
    for item in sorted(layout.get("furniture", []), key=item_sort_key):
        if not item_visible_in_mode(item, mode):
            continue
        src = Image.open(resolve_item_path(item, furniture_root)).convert("RGBA")
        size = item_size(item, src)
        if size != src.size:
            src = src.resize(size, Image.Resampling.NEAREST)
        opacity = max(0, min(1, float(item.get("opacity", 1))))
        if opacity < 1:
            alpha = src.getchannel("A").point(lambda p: int(p * opacity))
            src.putalpha(alpha)
        prepared.append((item, src, opacity))
    return prepared


def alpha_composite_clipped(base, overlay, dest):
    x, y = dest
    if x >= base.width or y >= base.height or x + overlay.width <= 0 or y + overlay.height <= 0:
        return
    crop_left = max(0, -x)
    crop_top = max(0, -y)
    crop_right = min(overlay.width, base.width - x)
    crop_bottom = min(overlay.height, base.height - y)
    if crop_right <= crop_left or crop_bottom <= crop_top:
        return
    cropped = overlay.crop((crop_left, crop_top, crop_right, crop_bottom))
    base.alpha_composite(cropped, (max(0, x), max(0, y)))


def polygon_mask_source(src, polygon):
    points = normalize_local_polygon(polygon)
    if not points:
        return None
    mask = Image.new("L", src.size, 0)
    pixel_points = [
        (round(x * max(1, src.width - 1)), round(y * max(1, src.height - 1)))
        for x, y in points
    ]
    ImageDraw.Draw(mask).polygon(pixel_points, fill=255)
    out = Image.new("RGBA", src.size, (0, 0, 0, 0))
    out.putalpha(mask)
    return out


def shadow_source_image(src, item):
    return polygon_mask_source(src, item_shadow_polygon(item)) or src


def shadow_image(src, shadow_width, shadow_height, alpha_scale, blur, angle, skew=0):
    alpha = src.getchannel("A").resize((shadow_width, shadow_height), Image.Resampling.NEAREST)
    alpha = alpha.point(lambda p: min(255, int(p * alpha_scale)))
    shadow = Image.new("RGBA", (shadow_width, shadow_height), (0, 0, 0, 0))
    shadow.putalpha(alpha)
    if blur > 0:
        shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
    if abs(skew) > 0.001:
        xshift = abs(skew) * shadow.height
        matrix = (1, skew, -xshift if skew > 0 else 0, 0, 1, 0)
        shadow = shadow.transform(
            (round(shadow.width + xshift), shadow.height),
            Image.Transform.AFFINE,
            matrix,
            resample=Image.Resampling.BICUBIC,
        )
    if abs(angle) > 0.01:
        shadow = shadow.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
    return shadow


def wall_shadow_opacity(night, item, light_factor, opacity, mode):
    height_factor = max(0.25, min(2.2, item_height_px(item) / 32))
    mode_alpha = 1 if mode == "night" else 0.42
    return min(1, max(0, (
        (item_shadow_opacity(night, item) / 100)
        * mode_alpha
        * light_factor
        * opacity
        * min(1.15, 0.65 + height_factor * 0.22)
        * 0.94
    )))


def base_shadow_opacity(night, item, light_factor, opacity, mode, is_wall_surface=False):
    height_factor = max(0.25, min(2.2, item_height_px(item) / 32))
    mode_alpha = 1 if mode == "night" else 0.42
    alpha = (
        (item_shadow_opacity(night, item) / 100)
        * mode_alpha
        * light_factor
        * opacity
        * min(1.15, 0.65 + height_factor * 0.22)
        * (0.92 if is_wall_surface else 1)
    )
    return min(1, max(0, alpha))


def shadow_height_gain(night, item):
    return item_shadow_length(night, item) / max(1, DEFAULT_NIGHT["shadowLength"])


def local_caster_height(night, item, point):
    vertical_ratio = max(0, min(1, 1 - point[1]))
    return item_height_px(item) * shadow_height_gain(night, item) * vertical_ratio


def project_elevated_point_to_receiver(night, point, z):
    if z <= 0.001:
        return point
    if active_shadow_mode(night) == "directional":
        angle = math.radians(float(night["lightAngle"]))
        return (point[0] + math.cos(angle) * z, point[1] + math.sin(angle) * z)

    light_x = float(night["lightX"])
    light_y = float(night["lightY"])
    light_z = max(z + 1, float(night["lightDepth"]))
    factor = light_z / max(1, light_z - z)
    return (
        light_x + (point[0] - light_x) * factor,
        light_y + (point[1] - light_y) * factor,
    )


def projected_shadow_polygon_2d5(night, item, src):
    polygon = item_shadow_polygon(item)
    if len(polygon) < 3:
        return []
    x = float(item.get("x", 0))
    y = float(item.get("y", 0))
    width, height = src.size
    points = []
    for local in polygon:
        screen_point = (x + local[0] * width, y + local[1] * height)
        points.append(project_elevated_point_to_receiver(
            night,
            screen_point,
            local_caster_height(night, item, local),
        ))
    return points


def draw_projected_shadow_2d5(layer, night, item, src, opacity, mode, light_factor, is_wall_surface=False):
    points = projected_shadow_polygon_2d5(night, item, src)
    if len(points) < 3:
        return False
    alpha = round(255 * base_shadow_opacity(night, item, light_factor, opacity, mode, is_wall_surface))
    if alpha <= 0:
        return True
    shadow = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).polygon([(round(px), round(py)) for px, py in points], fill=(0, 0, 0, alpha))
    blur = max(0, item_shadow_blur(night, item))
    if blur > 0:
        shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
    layer.alpha_composite(shadow)
    return True


def draw_floor_footprint_shadow(layer, night, item, src, opacity, mode, ux, uy, light_factor):
    polygon = item_footprint_polygon(item)
    if polygon:
        x = float(item.get("x", 0))
        y = float(item.get("y", 0))
        width, height = src.size
        height_factor = max(0.25, min(2.2, item_height_px(item) / 32))
        shadow_length = item_shadow_length(night, item) * height_factor
        offset_x = ux * shadow_length * 0.72
        offset_y = uy * shadow_length * 0.32
        alpha = round(255 * base_shadow_opacity(night, item, light_factor, opacity, mode))
        if alpha <= 0:
            return True
        shadow = Image.new("RGBA", layer.size, (0, 0, 0, 0))
        points = [
            (round(x + px * width + offset_x), round(y + py * height + offset_y))
            for px, py in polygon
        ]
        ImageDraw.Draw(shadow).polygon(points, fill=(0, 0, 0, alpha))
        blur = max(0.2, item_shadow_blur(night, item))
        if blur > 0:
            shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
        layer.alpha_composite(shadow)
        return True

    if item_footprint(item) != "ellipse":
        return False
    x = float(item.get("x", 0))
    y = float(item.get("y", 0))
    width, height = src.size
    height_factor = max(0.25, min(2.2, item_height_px(item) / 32))
    shadow_length = item_shadow_length(night, item) * height_factor
    foot_x = x + width * 0.5
    foot_y = y + height * 0.88
    shadow_width = max(1, width * (0.72 + min(0.22, item_shadow_length(night, item) / 220) * height_factor))
    shadow_height = max(1, height * (0.16 + min(0.12, item_height_px(item) / 360)))
    center_x = foot_x + ux * shadow_length * 0.72
    center_y = foot_y + uy * shadow_length * 0.32
    alpha = round(255 * base_shadow_opacity(night, item, light_factor, opacity, mode))
    if alpha <= 0:
        return True
    shadow = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(shadow)
    draw.ellipse(
        (
            round(center_x - shadow_width * 0.5),
            round(center_y - shadow_height * 0.5),
            round(center_x + shadow_width * 0.5),
            round(center_y + shadow_height * 0.5),
        ),
        fill=(0, 0, 0, alpha),
    )
    blur = max(0.2, item_shadow_blur(night, item))
    if blur > 0:
        shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
    layer.alpha_composite(shadow)
    return True


def affine_shadow_image(src, alpha_scale):
    alpha = src.getchannel("A").point(lambda p: min(255, int(p * alpha_scale)))
    shadow = Image.new("RGBA", src.size, (0, 0, 0, 0))
    shadow.putalpha(alpha)
    return shadow


def affine_warp_to_layer(src, layer_size, p0, p1, p2, blur):
    a = (p1[0] - p0[0]) / max(1, src.width)
    b = (p2[0] - p0[0]) / max(1, src.height)
    c = p0[0]
    d = (p1[1] - p0[1]) / max(1, src.width)
    e = (p2[1] - p0[1]) / max(1, src.height)
    f = p0[1]
    det = a * e - b * d
    if abs(det) < 0.00001:
        return None
    inv = (
        e / det,
        -b / det,
        (b * f - e * c) / det,
        -d / det,
        a / det,
        (d * c - a * f) / det,
    )
    if blur > 0:
        src = src.filter(ImageFilter.GaussianBlur(blur))
    return src.transform(
        layer_size,
        Image.Transform.AFFINE,
        inv,
        resample=Image.Resampling.BICUBIC,
        fillcolor=(0, 0, 0, 0),
    )


def draw_wall_plane_shadow_for_item(layer, night, item, src, opacity, mode, face):
    points = face.get("points", []) if face else []
    basis = face_projection_basis(points)
    if not basis:
        return False
    x = float(item.get("x", 0))
    y = float(item.get("y", 0))
    width, height = src.size
    center = (x + width * 0.5, y + height * 0.5)
    light = (night["lightX"], night["lightY"])
    light_factor = shadow_light_factor(night, center[0], center[1], light[0], light[1])
    if light_factor <= 0:
        return True

    light_local = screen_to_face_local(basis, light)
    light_depth = max(2, float(night["lightDepth"]))
    caster_depth = min(item_wall_shadow_depth_scaled(night, item), light_depth - 1)
    if caster_depth <= 0:
        return False

    offset_v = item_wall_shadow_offset_y(item)
    corners = [
        screen_to_face_local(basis, (x, y)),
        screen_to_face_local(basis, (x + width, y)),
        screen_to_face_local(basis, (x, y + height)),
    ]
    projected = [
        face_local_to_screen(
            basis,
            project_wall_local_point((u, v + offset_v), light_local, light_depth, caster_depth),
        )
        for u, v in corners
    ]
    alpha_scale = wall_shadow_opacity(night, item, light_factor, opacity, mode)
    shadow = affine_shadow_image(shadow_source_image(src, item), alpha_scale)
    warped = affine_warp_to_layer(shadow, layer.size, projected[0], projected[1], projected[2], item_shadow_blur(night, item))
    if warped:
        layer.alpha_composite(warped)
    return True


def draw_shadow_for_item(layer, night, item, src, opacity, mode, surface, face=None):
    if not item_casts_shadow(item):
        return
    if item_shadow_opacity(night, item) <= 0 or item_shadow_length(night, item) <= 0:
        return
    if not item_should_cast_shadow_on_surface(item, src, surface, face):
        return

    x = float(item.get("x", 0))
    y = float(item.get("y", 0))
    width, height = src.size
    if width <= 0 or height <= 0:
        return

    light_x = night["lightX"]
    light_y = night["lightY"]
    shadow_alpha = item_shadow_opacity(night, item) / 100
    mode_alpha = 1 if mode == "night" else 0.42
    shadow_length = item_shadow_length(night, item)
    blur = item_shadow_blur(night, item)
    foot_x = x + width * 0.5
    foot_y = y + height * 0.88
    ux, uy = shadow_direction(night, foot_x, foot_y, light_x, light_y)
    light_factor = shadow_light_factor(night, foot_x, foot_y, light_x, light_y)
    if light_factor <= 0:
        return
    height_factor = max(0.25, min(2.2, item_height_px(item) / 32))
    item_shadow_len = shadow_length * height_factor
    is_wall_surface = surface in {"left_wall", "right_wall"}
    uses_wall_anchor = is_wall_surface and item_uses_wall_shadow_anchor(item)
    if uses_wall_anchor and face and draw_wall_plane_shadow_for_item(layer, night, item, src, opacity, mode, face):
        return
    if draw_projected_shadow_2d5(layer, night, item, src, opacity, mode, light_factor, is_wall_surface):
        return
    if not is_wall_surface and draw_floor_footprint_shadow(layer, night, item, src, opacity, mode, ux, uy, light_factor):
        return

    if is_wall_surface:
        shadow_width = max(1, round(width * (0.72 + min(0.28, height_factor * 0.12))))
        shadow_height = max(1, round(height * (0.52 + min(0.32, item_height_px(item) / 200))))
        wall_anchor_y = y + height * (0.86 if uses_wall_anchor else 0.34)
        wall_shadow_lift = shadow_height * 0.08 if uses_wall_anchor else shadow_height * 0.36 + item_height_px(item) * 0.22
        wall_light_shift = uy * item_shadow_len * (0.68 if uses_wall_anchor else 0.28)
        wall_offset_y = item_wall_shadow_offset_y(item) if uses_wall_anchor else 0
        center_x = foot_x + ux * item_shadow_len * 0.82
        center_y = wall_anchor_y - wall_shadow_lift + wall_light_shift + wall_offset_y + shadow_height * 0.5
        angle = math.degrees(math.atan2(uy, ux) * 0.05)
        skew = wall_shadow_skew(surface)
        surface_alpha = 0.92
    else:
        shadow_width = max(1, round(width * (1 + min(0.55, shadow_length / 180) * height_factor)))
        shadow_height = max(1, round(height * (0.22 + min(0.18, item_height_px(item) / 320))))
        center_x = foot_x + ux * item_shadow_len
        center_y = foot_y + uy * item_shadow_len * 0.45
        angle = math.degrees(math.atan2(uy, ux) * 0.12)
        skew = 0
        surface_alpha = 1

    alpha_scale = min(1, max(0, shadow_alpha * mode_alpha * light_factor * opacity * min(1.15, 0.65 + height_factor * 0.22) * surface_alpha))
    shadow = shadow_image(shadow_source_image(src, item), shadow_width, shadow_height, alpha_scale, blur, angle, skew)
    alpha_composite_clipped(layer, shadow, (round(center_x - shadow.width / 2), round(center_y - shadow.height / 2)))


def clip_layer_to_face(layer, face):
    mask = Image.new("L", layer.size, 0)
    points = [(round(point[0]), round(point[1])) for point in face.get("points", [])]
    if len(points) < 3:
        return layer
    ImageDraw.Draw(mask).polygon(points, fill=255)
    clipped = layer.copy()
    clipped.putalpha(ImageChops.multiply(clipped.getchannel("A"), mask))
    return clipped


def composite_shadows(out, layout, night, prepared_items, mode):
    night = normalize_night(night, mode)
    if not night["castShadows"]:
        return

    faces = receiving_shadow_faces(layout)
    if not faces:
        has_room_faces = any(
            face.get("type") != "sprite_area" and len(face.get("points", [])) >= 3
            for face in layout.get("roomGeometry", {}).get("faces", [])
        )
        if has_room_faces:
            return
        layer = Image.new("RGBA", out.size, (0, 0, 0, 0))
        for item, src, opacity in prepared_items:
            draw_shadow_for_item(layer, night, item, src, opacity, mode, "floor")
        out.alpha_composite(layer)
        return

    for face in faces:
        layer = Image.new("RGBA", out.size, (0, 0, 0, 0))
        for item, src, opacity in prepared_items:
            draw_shadow_for_item(layer, night, item, src, opacity, mode, face_shadow_surface(face), face)
        out.alpha_composite(clip_layer_to_face(layer, face))


def composite(layout, base_path, furniture_dir, mode, include_lighting=True, layout_dir=None):
    out = render_base(layout, base_path, mode, layout_dir)

    furniture_root = Path(furniture_dir)
    prepared_items = load_prepared_items(layout, furniture_root, mode)
    composite_shadows(out, layout, layout.get("night", {}), prepared_items, mode)
    for item, src, _opacity in prepared_items:
        alpha_composite_clipped(out, src, (int(item.get("x", 0)), int(item.get("y", 0))))

    if include_lighting:
        return apply_lighting(out, layout.get("night", {}), mode)
    return out


def main():
    parser = argparse.ArgumentParser(description="Compose StickMon room day/night previews from room_layout.json.")
    parser.add_argument("--layout", required=True, help="Exported room_layout.json")
    parser.add_argument("--base", help="Fallback empty room base image for old layouts without roomGeometry")
    parser.add_argument("--day-base", help="Day background image. Overrides --base for the day output.")
    parser.add_argument("--night-base", help="Night background image. Overrides --base for the night output.")
    parser.add_argument("--furniture-dir", required=True, help="Directory containing furniture PNG files")
    parser.add_argument("--out", default="room_preview", help="Output prefix")
    parser.add_argument("--no-lighting", action="store_true", help="Only use lights for shadow projection; do not draw light glow.")
    args = parser.parse_args()

    layout_path = Path(args.layout)
    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    out_prefix = Path(args.out)
    day_base = args.day_base or args.base
    night_base = args.night_base or args.base or day_base

    composite(layout, day_base, args.furniture_dir, "day", include_lighting=not args.no_lighting, layout_dir=layout_path.parent).save(
        out_prefix.with_name(out_prefix.name + "_day.png")
    )
    composite(layout, night_base, args.furniture_dir, "night", include_lighting=not args.no_lighting, layout_dir=layout_path.parent).save(
        out_prefix.with_name(out_prefix.name + "_night.png")
    )


if __name__ == "__main__":
    main()
