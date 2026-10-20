"""Read the project's room sources and active firmware resources for the editor."""
from __future__ import annotations

import base64
import io
import json
import re
import struct
import sys
from functools import lru_cache
from pathlib import Path

from PIL import Image

import generate_sprite_previews as sprites
from compose_room import resolve_background_layer_path, resolve_item_path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import prepare_room_background as room_pipeline  # noqa: E402


def numeric_constant(path: Path, name: str) -> int | float:
    source = re.sub(r"/\*.*?\*/|//[^\n]*", "", path.read_text(), flags=re.S)
    match = re.search(rf"\b{re.escape(name)}\s*=\s*(-?\d+(?:\.\d+)?)[fFuU]?\s*;", source)
    if not match:
        raise ValueError(f"Cannot read firmware constant {name} in {path.name}")
    value = float(match[1])
    return int(value) if value.is_integer() else value


def firmware_devices(root: Path) -> dict:
    platform = root / "src/platform/api/PlatformServices.h"
    geometry = root / "firmware/amoled_1_8_v1/main/AmoledGeometry.h"
    metrics = root / "firmware/amoled_1_8_v1/main/ui/UiMetrics.h"
    amoled = root / "firmware/amoled_1_8_v1/main/AmoledApp.cpp"
    stick = root / "src/scenes/MainScene.cpp"
    return {
        "sticks3": {
            "label": "StickS3",
            "width": numeric_constant(platform, "LOGICAL_DISPLAY_W"),
            "height": numeric_constant(platform, "LOGICAL_DISPLAY_H"),
            "roomTop": 0,
            "roomHeight": numeric_constant(platform, "LOGICAL_DISPLAY_H"),
            "scale": 1,
            "camera": {"focusY": numeric_constant(stick, "CAMERA_FOCUS_Y")},
        },
        "amoled": {
            "label": "微雪 AMOLED 1.8 V1 / V2",
            "width": numeric_constant(geometry, "WIDTH"),
            "height": numeric_constant(geometry, "HEIGHT"),
            "roomTop": numeric_constant(metrics, "HOME_HEADER_HEIGHT"),
            "roomHeight": numeric_constant(metrics, "HOME_ROOM_HEIGHT"),
            "scale": numeric_constant(geometry, "RESOURCE_SCALE"),
            "camera": {
                key: numeric_constant(amoled, name)
                for key, name in (("left", "CAMERA_SAFE_LEFT"), ("right", "CAMERA_SAFE_RIGHT"),
                                  ("top", "CAMERA_SAFE_TOP"), ("bottom", "CAMERA_SAFE_BOTTOM"))
            },
        },
    }


def project_path(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def image_asset(path: Path) -> dict:
    payload = path.read_bytes()
    with Image.open(io.BytesIO(payload)) as image:
        width, height = image.size
        mime = Image.MIME.get(image.format, "image/png")
        image.verify()
    return {"width": width, "height": height,
            "dataUrl": f"data:{mime};base64,{base64.b64encode(payload).decode('ascii')}"}


def active_pack(root: Path) -> tuple[Path, dict]:
    active_path = root / "data/active.json"
    config = json.loads(active_path.read_text()) if active_path.exists() else {}
    value = config.get("packPath", config.get("path"))
    if not value:
        name = config.get("activePack", config.get("pack", "dev"))
        if not re.fullmatch(r"[a-zA-Z0-9_-]+", name):
            raise ValueError("Invalid active resource pack name")
        value = f"/packs/{name}"
    directory = (root / "data" / str(value).lstrip("/")).resolve()
    if not directory.is_relative_to((root / "data/packs").resolve()):
        raise ValueError("Active pack must be inside data/packs")
    manifest = json.loads((directory / "manifest.json").read_text())
    if manifest.get("format") != "smon-resource-pack-v1" or manifest.get("schema") != 1:
        raise ValueError("Unsupported resource pack manifest")
    return directory, manifest


def pack_child(directory: Path, value: str) -> Path:
    child = (directory / value).resolve()
    if not child.is_relative_to(directory.resolve()):
        raise ValueError("Resource path leaves the active pack")
    return child


def room_pack_info(path: Path) -> dict:
    payload = path.read_bytes()
    if len(payload) < 76:
        raise ValueError("Room pack header is truncated")
    values = struct.unpack_from(room_pipeline.ROOM_PACK_HEADER_FORMAT_V3, payload)
    magic, version, width, height, room_y = values[:5]
    if magic != room_pipeline.ROOM_PACK_MAGIC or version not in (3, 4):
        raise ValueError("Unsupported room pack; expected v3 or v4")
    if version == 4 and len(payload) < 80:
        raise ValueError("Room pack art dimensions are truncated")
    art_width, art_height = struct.unpack_from("<HH", payload, 76) if version == 4 else (width, height)
    header_size = 80 if version == 4 else 76
    expected = header_size + values[6] + values[7] * 10 + values[8] * 2
    expected += sum(values[9:12]) * 4 + values[12] * 6
    if len(payload) != expected:
        raise ValueError("Room pack length does not match its header")
    return {"version": version, "width": width, "height": height, "y": room_y,
            "artWidth": art_width, "artHeight": art_height, "bytes": len(payload)}


@lru_cache(maxsize=128)
def sprite_entry(pack_path: str, modified: int, size: int, kind_names: tuple, ident: str) -> dict:
    # File metadata forms the cache key; no generated preview files are written.
    species_id, frames, words, palettes = sprites.load_sprite_pack(Path(pack_path), list(kind_names))
    chosen = sprites.choose_preview_frame(frames)
    slug = sprites.slugify(ident)
    actions = {}
    preview = None
    for frame in frames:
        action = sprites.frame_action(frame["kindName"])
        if action is None and frame is not chosen:
            continue
        buffer = io.BytesIO()
        sprites.decode_frame(frame, words, palettes).save(buffer, format="PNG")
        data_url = "data:image/png;base64," + base64.b64encode(buffer.getvalue()).decode("ascii")
        file_name = f"{species_id:03d}_{slug}_{frame['kindName']}.png"
        record = {"file": file_name, "path": f"./generated/pokemon_sprites/{file_name}",
                  "dataUrl": data_url, "width": frame["width"], "height": frame["height"],
                  "kindName": frame["kindName"], "groundOffsetY": frame["groundOffsetY"]}
        actions.setdefault(action or "front", []).append(record)
        if frame is chosen:
            preview = record
    if not preview:
        raise ValueError(f"No preview frame in {pack_path}")
    return {"speciesId": species_id, "name": f"{species_id:03d} {ident.title()}",
            "slug": slug, "frame": chosen["kindName"], "width": chosen["width"],
            "height": chosen["height"], "path": preview["path"],
            "dataUrl": preview["dataUrl"], "actions": actions}


def sprite_catalog(root: Path, directory: Path) -> list:
    names = tuple(sprites.parse_kind_names((root / "src/assets/PokemonSprites.h").read_text()))
    idents = sprites.parse_species_idents((root / "src/game/Species.cpp").read_text())
    catalog = []
    for path in sorted(directory.glob("*.smonsp")):
        with path.open("rb") as stream:
            header = stream.read(sprites.PACK_HEADER.size)
        if len(header) != sprites.PACK_HEADER.size:
            raise ValueError(f"Sprite pack is truncated: {path.name}")
        species_id = sprites.PACK_HEADER.unpack(header)[2]
        stat = path.stat()
        catalog.append(sprite_entry(str(path), stat.st_mtime_ns, stat.st_size, names,
                                    idents.get(species_id, f"SPECIES_{species_id}")))
    if not catalog:
        raise ValueError("The active pack contains no sprite packs")
    return catalog


def build_project_scene(root: Path = ROOT, layout_path: Path | None = None,
                        furniture_dir: Path | None = None) -> dict:
    root = root.resolve()
    directory, manifest = active_pack(root)
    packed_room = pack_child(directory, manifest.get("room", "rooms/standard.smonroom"))
    if layout_path is None and packed_room.stem != "standard":
        raise ValueError(f"No editable source mapping for {packed_room.name}; start with --layout PATH")
    layout_path = (layout_path or root / room_pipeline.DEFAULT_LAYOUT.relative_to(ROOT)).resolve()
    furniture_dir = furniture_dir or root / room_pipeline.DEFAULT_FURNITURE_DIR.relative_to(ROOT)
    layout = room_pipeline.read_layout(layout_path)
    compiled = {"path": project_path(packed_room, root), **room_pack_info(packed_room)}
    layers = layout.get("backgroundLayers") or (layout.get("roomGeometry", {}).get("backgrounds") or {}).get("layers")
    if not layers:
        merged = {}
        for mode in ("day", "night"):
            path = room_pipeline.background_path(layout_path, layout, mode)
            layer = merged.setdefault(str(path), {"fileName": str(path), "visibleInDay": False, "visibleInNight": False})
            layer["visibleInDay" if mode == "day" else "visibleInNight"] = True
        layers = list(merged.values())
    backgrounds = []
    for index, layer in enumerate(layers):
        path = resolve_background_layer_path(layer, layout_dir=layout_path.parent)
        if path is None:
            raise ValueError("Background layer is missing its file name")
        backgrounds.append({**layer, **image_asset(path), "id": layer.get("id", f"b{index + 1}"),
                            "name": layer.get("name", path.name), "fileName": layer.get("fileName", path.name),
                            "projectPath": project_path(path, root)})
    items = []
    for index, entry in enumerate(layout.get("furniture", [])):
        if entry.get("source", "furniture") != "furniture":
            continue
        path = resolve_item_path(entry, furniture_dir)
        asset = image_asset(path)
        width, height = asset["width"], asset["height"]
        scale_x = entry.get("targetWidth", width * entry.get("scaleX", entry.get("scale", 1))) / width
        scale_y = entry.get("targetHeight", height * entry.get("scaleY", entry.get("scale", 1))) / height
        items.append({**entry, "id": entry.get("id", f"f{index + 1}"), "dataUrl": asset["dataUrl"],
                      "source": "furniture", "sourceWidth": width, "sourceHeight": height,
                      "scale": scale_x, "scaleX": scale_x, "scaleY": scale_y,
                      "projectPath": project_path(path, root)})
    warnings = []
    width, height = room_pipeline.room_size(layout)
    if (width, height) != (compiled["width"], compiled["height"]):
        warnings.append("布局尺寸与固件房间包不同，源资源可能尚未重新生成。")
    paths = [layout_path, *(root / item["projectPath"] for item in backgrounds + items)]
    if max(path.stat().st_mtime_ns for path in paths) > packed_room.stat().st_mtime_ns:
        warnings.append("部分源资源晚于固件房间包，编辑场景可能包含尚未打包的修改。")
    return {"type": "stickmon-project-scene", "version": 1, "layout": layout,
            "backgroundItems": backgrounds, "items": items,
            "devices": firmware_devices(root),
            "sprites": sprite_catalog(root, pack_child(directory, manifest.get("sprites", "sprites"))),
            "source": {"layout": project_path(layout_path, root),
                       "manifest": project_path(directory / "manifest.json", root),
                       "packId": manifest.get("id", directory.name), "compiledRoom": compiled},
            "warnings": warnings}
