#!/usr/bin/env python3
"""Sync missing Pokemon sprites into the local essentials_graphics mirror.

The mirror at origin_asset/essentials_graphics/Graphics/Pokemon keeps a curated
subset of Essentials sprites named {id:03d}_{IDENT}.png, while a full Pokemon
Essentials install names them {IDENT}.png. This script copies the Front, Back
and Icons sprites for every species in src/game/Species.cpp that the mirror is
missing. Existing files are left untouched, so it is safe to re-run.
"""
from pathlib import Path
import re
import shutil

from asset_paths import essentials_dir

ROOT = Path(__file__).resolve().parents[1]
SPECIES_CPP = ROOT / "src" / "game" / "Species.cpp"
MIRROR = ROOT / "origin_asset" / "essentials_graphics" / "Graphics" / "Pokemon"
ESSENTIALS = essentials_dir() / "Graphics" / "Pokemon"

SUBDIRS = ("Front", "Back", "Icons")


def species_rows():
    text = SPECIES_CPP.read_text(encoding="utf-8")
    return [(int(m.group(1)), m.group(2)) for m in re.finditer(
        r"\{(\d+),\s*Ui::SpeciesName::(\w+),", text)]


def main():
    copied = []
    present = 0
    missing_source = []
    for species_id, ident in species_rows():
        for subdir in SUBDIRS:
            dest = MIRROR / subdir / f"{species_id:03d}_{ident}.png"
            if dest.exists():
                present += 1
                continue
            source = ESSENTIALS / subdir / f"{ident}.png"
            if not source.exists():
                missing_source.append(f"{subdir}/{ident}.png")
                continue
            shutil.copy2(source, dest)
            copied.append(f"{subdir}/{dest.name}")

    print(f"already present: {present}")
    print(f"copied: {len(copied)}")
    for name in copied:
        print(f"  {name}")
    if missing_source:
        print(f"source missing: {len(missing_source)}")
        for name in missing_source:
            print(f"  {name}")


if __name__ == "__main__":
    main()
