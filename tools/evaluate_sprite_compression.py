#!/usr/bin/env python3
from __future__ import annotations

import argparse
import bz2
import gzip
import importlib.util
import lzma
import struct
import sys
import zlib
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "generate_pokemon_sprites.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("generate_pokemon_sprites", GENERATOR)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load {GENERATOR}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def words_to_bytes(words):
    return struct.pack("<" + "H" * len(words), *words) if words else b""


def percent(value, baseline):
    if baseline == 0:
        return "n/a"
    return f"{value / baseline * 100:5.1f}%"


def kib(value):
    return f"{value / 1024:8.1f} KB"


def codec_table():
    codecs = {
        "store": lambda data: data,
        "zlib-1": lambda data: zlib.compress(data, 1),
        "zlib-6": lambda data: zlib.compress(data, 6),
        "zlib-9": lambda data: zlib.compress(data, 9),
        "gzip-6": lambda data: gzip.compress(data, compresslevel=6),
        "bz2-9": lambda data: bz2.compress(data, compresslevel=9),
        "lzma-6": lambda data: lzma.compress(data, preset=6),
    }

    try:
        import lz4.frame  # type: ignore
    except Exception:
        pass
    else:
        codecs["lz4"] = lambda data: lz4.frame.compress(data, compression_level=0)

    try:
        import heatshrink2  # type: ignore
    except Exception:
        pass
    else:
        codecs["heatshrink"] = lambda data: heatshrink2.compress(data)

    return codecs


def build_assets(gen):
    writer = gen.AssetWriter()
    missing = []

    present_species_ids = set()
    for species_id, ident in gen.species_rows():
        present_species_ids.add(species_id)
        gen.add_base_frames(writer, species_id, ident, missing)
        spec = gen.PMD_BY_ID.get(species_id)
        if spec and (gen.PROCESSED / spec.slug).exists():
            gen.add_pmd_frames(writer, spec)

    for spec in gen.PMD_SPECS:
        if spec.species_id in present_species_ids or not (gen.PROCESSED / spec.slug).exists():
            continue
        gen.add_base_frames(writer, spec.species_id, spec.ident, missing)
        gen.add_pmd_frames(writer, spec)

    if missing:
        raise SystemExit("Missing Pokemon sprite source files:\n" + "\n".join(missing))

    egg_path = gen.GRAPHICS / "Eggs" / "000.png"
    egg_image = gen.resize_nearest(gen.to_rgba(egg_path), gen.EGG_SIZE)
    egg_format, egg_palette_size, egg_offset, egg_length, egg_palette_offset = writer.encode(egg_image)
    writer.frames.append({
        "species_id": 0,
        "ident": "EGG",
        "kind": "EGG",
        "width": egg_image.width,
        "height": egg_image.height,
        "format": egg_format,
        "palette_size": egg_palette_size,
        "offset": egg_offset,
        "length": egg_length,
        "palette_offset": egg_palette_offset,
    })
    return writer


def frame_table_bytes(frame_count):
    return frame_count * 20


def segment_bytes(writer, frame):
    start = frame["offset"]
    end = start + frame["length"]
    return words_to_bytes(writer.data[start:end])


def palette_bytes(writer, frame):
    if frame["palette_size"] <= 0:
        return b""
    start = frame["palette_offset"]
    end = start + frame["palette_size"]
    return words_to_bytes(writer.palettes[start:end])


def unique_frames(frames):
    seen = set()
    out = []
    for frame in frames:
        key = (frame["format"], frame["offset"], frame["length"])
        if key in seen:
            continue
        seen.add(key)
        out.append(frame)
    return out


def unique_palettes(frames):
    seen = set()
    out = []
    for frame in frames:
        if frame["palette_size"] <= 0:
            continue
        key = (frame["palette_offset"], frame["palette_size"])
        if key in seen:
            continue
        seen.add(key)
        out.append(frame)
    return out


def payload_for_frames(writer, frames, include_palettes=True):
    chunks = [segment_bytes(writer, frame) for frame in unique_frames(frames)]
    if include_palettes:
        chunks.extend(palette_bytes(writer, frame) for frame in unique_palettes(frames))
    return b"".join(chunks)


def action_group(frame):
    kind = frame["kind"]
    if kind in ("ICON_0", "FRONT", "BACK", "EGG"):
        return kind
    if "_IDLE_" in kind:
        return "IDLE"
    if "_WALKING_" in kind:
        return "WALKING"
    if "_SLEEPING_" in kind:
        return "SLEEPING"
    return "OTHER"


def grouped_payload_size(writer, groups, codec):
    total = 0
    for frames in groups.values():
        total += len(codec(payload_for_frames(writer, frames)))
    return total


def frame_block_size(writer, frames, codec):
    total = 0
    for frame in unique_frames(frames):
        total += len(codec(segment_bytes(writer, frame)))
    total += len(words_to_bytes(writer.palettes))
    return total


def parse_pair(text):
    parts = text.split(",")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError("pair must look like 133,151")
    try:
        return tuple(int(part) for part in parts)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("pair must contain numeric species ids") from exc


def print_plan_table(title, rows, baseline):
    print(f"\n{title}")
    print("-" * len(title))
    print(f"{'plan':24s} {'codec':10s} {'flash':>12s} {'vs current':>10s}")
    for plan, codec_name, size in rows:
        print(f"{plan:24s} {codec_name:10s} {kib(size):>12s} {percent(size, baseline):>10s}")


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate extra compression options for generated Pokemon sprite assets.",
    )
    parser.add_argument(
        "--pair",
        action="append",
        type=parse_pair,
        default=[],
        help="Estimate active two-species RAM/cache payload for a pair, e.g. --pair 133,151.",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=8,
        help="How many largest per-species payloads to print.",
    )
    args = parser.parse_args()

    gen = load_generator()
    writer = build_assets(gen)
    frames = writer.frames
    sprite_frames = [frame for frame in frames if frame["species_id"] != 0]

    rle_bytes = len(writer.data) * 2
    palette_bytes_total = len(writer.palettes) * 2
    frame_bytes = frame_table_bytes(len(sprite_frames))
    current_payload = rle_bytes + palette_bytes_total
    current_total = current_payload + frame_bytes
    raw_rgb565 = sum(frame["width"] * frame["height"] * 2 for frame in frames)

    codecs = codec_table()

    print("Sprite Compression Evaluation")
    print("=============================")
    print(f"species:              {len({frame['species_id'] for frame in sprite_frames})}")
    print(f"sprite frames:         {len(sprite_frames)} (+ egg)")
    print(f"raw RGB565:            {kib(raw_rgb565)}")
    print(f"current RLE payload:   {kib(current_payload)}")
    print(f"current frame table:   {kib(frame_bytes)}")
    print(f"current total est.:    {kib(current_total)}")
    print(f"RLE duplicates:        {writer.rle_duplicate_count}")
    print(f"palette duplicates:    {writer.palette_duplicate_count}")
    print(f"available codecs:      {', '.join(codecs)}")

    global_payload = payload_for_frames(writer, frames)

    by_species = defaultdict(list)
    by_species_action = defaultdict(list)
    for frame in frames:
        by_species[frame["species_id"]].append(frame)
        by_species_action[(frame["species_id"], action_group(frame))].append(frame)

    plan_rows = []
    for name, codec in codecs.items():
        plan_rows.append(("global block", name, len(codec(global_payload)) + frame_bytes))
        plan_rows.append(("per species block", name, grouped_payload_size(writer, by_species, codec) + frame_bytes))
        plan_rows.append(("per species/action", name, grouped_payload_size(writer, by_species_action, codec) + frame_bytes))
        plan_rows.append(("per unique frame", name, frame_block_size(writer, frames, codec) + frame_bytes))

    print_plan_table("Flash Layout Estimates", plan_rows, current_total)

    species_rows = []
    for species_id, group in by_species.items():
        if species_id == 0:
            continue
        payload = payload_for_frames(writer, group)
        species_rows.append((len(payload) + frame_table_bytes(len(group)), species_id, group[0]["ident"], len(group)))
    species_rows.sort(reverse=True)

    print(f"\nLargest Per-Species Current Payloads")
    print("------------------------------------")
    print(f"{'species':10s} {'frames':>6s} {'current':>12s}")
    for size, species_id, ident, count in species_rows[:args.top]:
        print(f"{species_id:03d} {ident:6s} {count:6d} {kib(size):>12s}")

    decoded_rgb565_per_frame = 64 * 64 * 2
    decoded_indexed_mask_per_frame = 64 * 64 // 2 + 64 * 64 // 8 + 16 * 2
    print("\nRuntime RAM Cache Estimates")
    print("---------------------------")
    print(f"stream current RLE draw:        ~0 sprite-cache KB")
    print(f"1 decoded RGB565 frame:         {kib(decoded_rgb565_per_frame)}")
    print(f"1 decoded indexed+mask frame:   {kib(decoded_indexed_mask_per_frame)}")
    print(f"2 species x current+next RGB565:{kib(decoded_rgb565_per_frame * 4)}")
    print(f"2 species x current+next indexed+mask:{kib(decoded_indexed_mask_per_frame * 4)}")

    if args.pair:
        by_species_current = {species_id: size for size, species_id, _ident, _count in species_rows}
        print("\nRequested Pair Payloads")
        print("-----------------------")
        for pair in args.pair:
            size = sum(by_species_current.get(species_id, 0) for species_id in pair)
            print(f"{pair[0]:03d},{pair[1]:03d}: current compressed payload {kib(size)}")


if __name__ == "__main__":
    main()
