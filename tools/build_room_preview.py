#!/usr/bin/env python3
from pathlib import Path
import json
import re

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
PROCESSED = ROOT / "origin_asset" / "processed"
SPECIES_CPP = ROOT / "src" / "game" / "Species.cpp"
UI_STRINGS = ROOT / "src" / "core" / "UiStrings.h"
GENERATOR = ROOT / "tools" / "generate_pokemon_sprites.py"
OUT = ROOT / "tools" / "room_preview" / "sprite_manifest.js"


def parse_pmd_specs():
    text = GENERATOR.read_text(encoding="utf-8")
    specs = {}
    for match in re.finditer(r'PmdSpec\((\d+),\s*"([A-Z0-9_]+)",\s*"([^"]+)"', text):
        species_id = int(match.group(1))
        ident = match.group(2)
        slug = match.group(3)
        specs[slug] = {"id": species_id, "ident": ident, "slug": slug}
    return specs


def parse_species_order():
    text = SPECIES_CPP.read_text(encoding="utf-8")
    return {
        match.group(2): int(match.group(1))
        for match in re.finditer(r"\{(\d+),\s*Ui::SpeciesName::(\w+),", text)
    }


def parse_cn_names():
    text = UI_STRINGS.read_text(encoding="utf-8")
    return {
        match.group(1): match.group(2)
        for match in re.finditer(r'static constexpr const char\* (\w+) = "([^"]+)";', text)
    }


def frame_info(path):
    with Image.open(path) as img:
        width, height = img.size
    return {
        "file": path.name,
        "path": "../../" + str(path.relative_to(ROOT)),
        "width": width,
        "height": height,
    }


def sorted_pngs(path):
    def key(p):
        numbers = [int(v) for v in re.findall(r"\d+", p.stem)]
        return (re.sub(r"\d+", "", p.stem), numbers, p.name)
    return sorted(path.glob("*.png"), key=key)


def build_manifest():
    specs = parse_pmd_specs()
    species_order = parse_species_order()
    cn_names = parse_cn_names()
    species = []

    for slug_dir in sorted(PROCESSED.iterdir()):
        if not slug_dir.is_dir():
            continue
        spec = specs.get(slug_dir.name)
        if not spec:
            continue
        ident = spec["ident"]
        species_id = species_order.get(ident, spec["id"])
        actions = {}
        for action in ("idle", "walking", "sleeping"):
            action_dir = slug_dir / action
            if not action_dir.exists():
                continue
            frames = [frame_info(path) for path in sorted_pngs(action_dir)]
            if frames:
                actions[action] = frames
        if not actions:
            continue
        default = (
            next((f for f in actions.get("idle", []) if f["file"] == "front_0.png"), None)
            or next((f for f in actions.get("walking", []) if f["file"] == "front_0.png"), None)
            or next(iter(actions.values()))[0]
        )
        species.append({
            "id": species_id,
            "ident": ident,
            "slug": slug_dir.name,
            "cn": cn_names.get(ident, ident),
            "en": slug_dir.name.replace("_", " ").title(),
            "defaultFrame": default,
            "actions": actions,
        })

    species.sort(key=lambda item: item["id"])
    return {
        "generatedFrom": "origin_asset/processed",
        "screen": {"width": 240, "height": 135},
        "species": species,
    }


def main():
    manifest = build_manifest()
    OUT.write_text(
        "window.STICKMON_SPRITES = "
        + json.dumps(manifest, ensure_ascii=False, indent=2)
        + ";\n",
        encoding="utf-8",
    )
    print(f"wrote {OUT} species={len(manifest['species'])}")


if __name__ == "__main__":
    main()
