#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import math
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


def kib(value):
    return f"{value / 1024:8.1f} KB"


def percent(value, baseline):
    return f"{value / baseline * 100:5.1f}%" if baseline else "n/a"


def raw_deflate(data, level=6):
    compressor = zlib.compressobj(level, zlib.DEFLATED, -15)
    return compressor.compress(data) + compressor.flush()


def words_to_bytes(words):
    return struct.pack("<" + "H" * len(words), *words) if words else b""


def pack_bits(values):
    out = bytearray((len(values) + 7) // 8)
    for index, value in enumerate(values):
        if value:
            out[index >> 3] |= 1 << (index & 7)
    return bytes(out)


def pack_nibbles(values):
    out = bytearray((len(values) + 1) // 2)
    for index, value in enumerate(values):
        if index & 1:
            out[index >> 1] |= (value & 0x0F) << 4
        else:
            out[index >> 1] |= value & 0x0F
    return bytes(out)


class ImageCollector:
    def __init__(self):
        self.frames = []

    def add_frame(self, species_id, ident, kind, image, source=None):
        self.frames.append({
            "species_id": species_id,
            "ident": ident,
            "kind": kind,
            "image": image.copy(),
            "source": source,
        })


def collect_images(gen):
    collector = ImageCollector()
    missing = []

    present_species_ids = set()
    for species_id, ident in gen.species_rows():
        present_species_ids.add(species_id)
        gen.add_base_frames(collector, species_id, ident, missing)
        spec = gen.PMD_BY_ID.get(species_id)
        if spec and (gen.PROCESSED / spec.slug).exists():
            gen.add_pmd_frames(collector, spec)

    for spec in gen.PMD_SPECS:
        if spec.species_id in present_species_ids or not (gen.PROCESSED / spec.slug).exists():
            continue
        gen.add_base_frames(collector, spec.species_id, spec.ident, missing)
        gen.add_pmd_frames(collector, spec)

    if missing:
        raise SystemExit("Missing Pokemon sprite source files:\n" + "\n".join(missing))
    return collector.frames


def build_current_rle_payloads(gen):
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

    by_species = defaultdict(list)
    for frame in writer.frames:
        by_species[frame["species_id"]].append(frame)

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

    payloads = {}
    for species_id, frames in by_species.items():
        chunks = []
        for frame in unique_frames(frames):
            chunks.append(words_to_bytes(writer.data[frame["offset"]:frame["offset"] + frame["length"]]))
        for frame in unique_palettes(frames):
            chunks.append(words_to_bytes(writer.palettes[frame["palette_offset"]:frame["palette_offset"] + frame["palette_size"]]))
        payloads[species_id] = b"".join(chunks)
    return payloads, writer


def rgb565_bytes(gen, r, g, b):
    return struct.pack("<H", gen.rgb565(r, g, b))


def encode_rgb565_mask(gen, image):
    pixels = gen.image_pixels(image)
    mask = []
    colors = bytearray()
    for r, g, b, a in pixels:
        opaque = a > 16
        mask.append(opaque)
        colors.extend(rgb565_bytes(gen, r, g, b) if opaque else b"\x00\x00")
    return pack_bits(mask) + bytes(colors), len(mask), sum(mask), None


def palette_for_pixels(gen, pixels, reserve_transparent=False):
    palette = []
    mapping = {}
    if reserve_transparent:
        mapping[None] = 0
        palette.append(0)
    for r, g, b, a in pixels:
        if a <= 16:
            continue
        color = gen.rgb565(r, g, b)
        if color not in mapping:
            if len(palette) >= 16:
                return None, None
            mapping[color] = len(palette)
            palette.append(color)
    return palette, mapping


def fixed_palette_bytes(palette):
    padded = list(palette) + [0] * (16 - len(palette))
    return words_to_bytes(padded)


def encode_indexed4_mask(gen, image):
    pixels = gen.image_pixels(image)
    palette, mapping = palette_for_pixels(gen, pixels, reserve_transparent=False)
    if palette is None:
        return None, len(pixels), 0, "too_many_colors"
    mask = []
    indices = []
    for r, g, b, a in pixels:
        opaque = a > 16
        mask.append(opaque)
        indices.append(mapping[gen.rgb565(r, g, b)] if opaque else 0)
    return pack_bits(mask) + pack_nibbles(indices) + fixed_palette_bytes(palette), len(mask), sum(mask), None


def encode_indexed4_transparent(gen, image):
    pixels = gen.image_pixels(image)
    palette, mapping = palette_for_pixels(gen, pixels, reserve_transparent=True)
    if palette is None:
        return None, len(pixels), 0, "too_many_colors"
    indices = []
    opaque_count = 0
    for r, g, b, a in pixels:
        if a <= 16:
            indices.append(0)
        else:
            opaque_count += 1
            indices.append(mapping[gen.rgb565(r, g, b)])
    return pack_nibbles(indices) + fixed_palette_bytes(palette), len(indices), opaque_count, None


def build_no_rle_payloads(gen, frames, encoder):
    by_species_frames = defaultdict(list)
    failures = []
    pixel_count = 0
    opaque_count = 0

    for frame in frames:
        payload, pixels, opaque, error = encoder(gen, frame["image"])
        if error:
            failures.append((frame["species_id"], frame["ident"], frame["kind"], error))
            continue
        pixel_count += pixels
        opaque_count += opaque
        by_species_frames[frame["species_id"]].append((frame, payload))

    payloads = {}
    unique_frame_count = 0
    for species_id, encoded_frames in by_species_frames.items():
        seen = set()
        chunks = []
        for _frame, payload in encoded_frames:
            if payload in seen:
                continue
            seen.add(payload)
            chunks.append(payload)
            unique_frame_count += 1
        payloads[species_id] = b"".join(chunks)
    return payloads, failures, pixel_count, opaque_count, unique_frame_count


def summarize_payloads(name, payloads, baseline_raw, baseline_deflated):
    raw = sum(len(payload) for payload in payloads.values())
    deflated = sum(len(raw_deflate(payload, 6)) for payload in payloads.values())
    print(f"{name:28s} {kib(raw):>12s} {percent(raw, baseline_raw):>10s} {kib(deflated):>12s} {percent(deflated, baseline_deflated):>10s}")
    return raw, deflated


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate sprite asset formats that remove RLE before deflate/PSRAM caching.",
    )
    parser.add_argument(
        "--pair",
        action="append",
        default=[],
        help="Print payload estimates for a two-species pair, e.g. --pair 133,151.",
    )
    parser.add_argument("--top-failures", type=int, default=12)
    args = parser.parse_args()

    gen = load_generator()
    frames = collect_images(gen)
    current_payloads, writer = build_current_rle_payloads(gen)
    current_raw = sum(len(payload) for payload in current_payloads.values())
    current_deflated = sum(len(raw_deflate(payload, 6)) for payload in current_payloads.values())

    print("No-RLE Sprite Format Evaluation")
    print("===============================")
    print(f"species:            {len(current_payloads)}")
    print(f"logical frames:     {len(frames)}")
    print(f"current RLE dups:   {writer.rle_duplicate_count}")
    print(f"palette dups:       {writer.palette_duplicate_count}")
    print()
    print(f"{'format':28s} {'raw':>12s} {'vs RLE raw':>10s} {'deflate/spec':>12s} {'vs RLE def':>10s}")
    print("-" * 80)
    summarize_payloads("current RLE", current_payloads, current_raw, current_deflated)

    results = {}
    for name, encoder in (
        ("RGB565 + 1bpp mask", encode_rgb565_mask),
        ("indexed4 + 1bpp mask", encode_indexed4_mask),
        ("indexed4 transparent", encode_indexed4_transparent),
    ):
        payloads, failures, pixel_count, opaque_count, unique_count = build_no_rle_payloads(gen, frames, encoder)
        raw, deflated = summarize_payloads(name, payloads, current_raw, current_deflated)
        results[name] = (payloads, failures, pixel_count, opaque_count, unique_count, raw, deflated)

    print("\nFrame Scan / Draw Cost Hints")
    print("----------------------------")
    total_pixels = len(frames) * 64 * 64
    total_opaque = sum(
        sum(1 for r, g, b, a in gen.image_pixels(frame["image"]) if a > 16)
        for frame in frames
    )
    print(f"average opaque pixels/frame: {total_opaque / len(frames):.1f} / 4096 ({total_opaque / total_pixels * 100:.1f}%)")
    print("RGB565+mask draw: scans 4096 mask bits/frame, writes opaque pixels, no palette lookup.")
    print("indexed4+mask draw: scans 4096 mask bits + 4096 nibbles/frame, writes opaque pixels, one palette lookup/pixel.")
    print("current RLE draw: scans RLE commands and opaque spans; cheaper for transparent runs, but harder to direct-blit.")

    print("\nFormat Caveats")
    print("--------------")
    for name, (_payloads, failures, _pixels, _opaque, unique_count, _raw, _deflated) in results.items():
        if failures:
            print(f"{name}: {len(failures)} frames cannot encode; first failures:")
            for species_id, ident, kind, error in failures[:args.top_failures]:
                print(f"  {species_id:03d} {ident} {kind}: {error}")
        else:
            print(f"{name}: all frames encodable, unique per-species frames={unique_count}")

    if args.pair:
        print("\nRequested Pair Estimates")
        print("------------------------")
        for pair_text in args.pair:
            pair = tuple(int(part) for part in pair_text.split(","))
            print(f"{pair[0]:03d},{pair[1]:03d}")
            current_pair_raw = sum(len(current_payloads.get(species_id, b"")) for species_id in pair)
            current_pair_def = sum(len(raw_deflate(current_payloads.get(species_id, b""), 6)) for species_id in pair)
            print(f"  {'current RLE':26s} raw={kib(current_pair_raw)} deflate={kib(current_pair_def)}")
            for name, (payloads, _failures, _pixels, _opaque, _unique_count, _raw, _deflated) in results.items():
                pair_raw = sum(len(payloads.get(species_id, b"")) for species_id in pair)
                pair_def = sum(len(raw_deflate(payloads.get(species_id, b""), 6)) for species_id in pair)
                print(f"  {name:26s} raw={kib(pair_raw)} deflate={kib(pair_def)}")

    print("\nRecommendation Signal")
    print("---------------------")
    print("If PSRAM is used for fully decoded current-team frames, compare deflate/spec plus draw cost.")
    print("RGB565+mask is fastest to draw but usually largest in Flash/PSRAM.")
    print("indexed4+mask is the best PSRAM compromise if its Flash penalty is acceptable.")
    print("indexed4 transparent is smaller but only works when a frame has <=15 opaque colors.")


if __name__ == "__main__":
    main()
