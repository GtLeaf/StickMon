#!/usr/bin/env python3
from __future__ import annotations

import argparse
import importlib.util
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "generate_pokemon_sprites.py"
DEFAULT_THIRD_PARTY = Path("/tmp/stickmon-compress-eval")
DEFAULT_XTENSA_BIN = Path.home() / ".platformio/packages/toolchain-xtensa-esp32s3/bin"


def load_generator():
    spec = importlib.util.spec_from_file_location("generate_pokemon_sprites", GENERATOR)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load {GENERATOR}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def run(cmd, cwd=None, check=True, capture=True):
    return subprocess.run(
        [str(part) for part in cmd],
        cwd=cwd,
        check=check,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def words_to_bytes(words):
    return struct.pack("<" + "H" * len(words), *words) if words else b""


def kib(value):
    return f"{value / 1024:8.1f} KB"


def percent(value, baseline):
    return f"{value / baseline * 100:5.1f}%" if baseline else "n/a"


def build_sprite_payloads(gen):
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

    frames_by_species = defaultdict(list)
    for frame in writer.frames:
        frames_by_species[frame["species_id"]].append(frame)

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
    for species_id, frames in frames_by_species.items():
        chunks = []
        for frame in unique_frames(frames):
            start = frame["offset"]
            end = start + frame["length"]
            chunks.append(words_to_bytes(writer.data[start:end]))
        for frame in unique_palettes(frames):
            start = frame["palette_offset"]
            end = start + frame["palette_size"]
            chunks.append(words_to_bytes(writer.palettes[start:end]))
        payloads[species_id] = b"".join(chunks)

    all_payload = b"".join(payloads[species_id] for species_id in sorted(payloads))
    return payloads, all_payload


def raw_deflate(data, level=6):
    compressor = zlib.compressobj(level, zlib.DEFLATED, -15)
    return compressor.compress(data) + compressor.flush()


def require_sources(third_party):
    required = {
        "miniz": third_party / "miniz/miniz_tinfl.c",
        "uzlib": third_party / "uzlib/src/tinflate.c",
        "heatshrink": third_party / "heatshrink/heatshrink_decoder.c",
    }
    missing = [name for name, path in required.items() if not path.exists()]
    if missing:
        raise SystemExit(
            "Missing third-party sources: "
            + ", ".join(missing)
            + f"\nClone them under {third_party}, for example:\n"
            + f"  mkdir -p {third_party}\n"
            + f"  git -C {third_party} clone --depth 1 https://github.com/richgel999/miniz.git\n"
            + f"  git -C {third_party} clone --depth 1 https://github.com/pfalcon/uzlib.git\n"
            + f"  git -C {third_party} clone --depth 1 https://github.com/atomicobject/heatshrink.git"
        )


def write_file(path, text):
    path.write_text(text, encoding="utf-8")


def build_size_delta(name, third_party, xtensa_bin, work):
    gcc = xtensa_bin / "xtensa-esp32s3-elf-gcc"
    size_tool = xtensa_bin / "xtensa-esp32s3-elf-size"
    if not gcc.exists():
        raise SystemExit(f"Missing ESP32-S3 compiler: {gcc}")

    common_flags = [
        "-Os",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-exceptions",
        "-fno-unwind-tables",
        "-fno-asynchronous-unwind-tables",
        "-Wl,--gc-sections",
    ]

    baseline_c = work / "baseline.c"
    write_file(baseline_c, "int main(void) { return 0; }\n")
    baseline_elf = work / "baseline.elf"
    run([gcc, *common_flags, baseline_c, "-o", baseline_elf])
    baseline_size = parse_size(run([size_tool, "-A", baseline_elf]).stdout)

    wrapper = work / f"{name}_wrapper.c"
    elf = work / f"{name}.elf"
    if name == "miniz":
        miniz_export = work / "miniz_export.h"
        write_file(miniz_export, "#pragma once\n#define MINIZ_EXPORT\n")
        write_file(wrapper, """
#include <stddef.h>
#include <stdint.h>
#include "miniz_tinfl.h"
volatile size_t sink_size;
volatile uint8_t *volatile_in_ptr;
volatile uint8_t *volatile_out_ptr;
int main(void) {
    static uint8_t in[16] = {3, 0};
    static uint8_t out[16];
    volatile_in_ptr = in;
    volatile_out_ptr = out;
    sink_size = tinfl_decompress_mem_to_mem(
        (void *)volatile_out_ptr, sizeof(out),
        (const void *)volatile_in_ptr, 2,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    return (int)sink_size;
}
""")
        run([
            gcc, *common_flags,
            "-DMINIZ_NO_STDIO", "-DMINIZ_NO_MALLOC",
            "-I", work,
            "-I", third_party / "miniz",
            wrapper, third_party / "miniz/miniz_tinfl.c",
            "-o", elf,
        ])
    elif name == "uzlib":
        write_file(wrapper, """
#include <stdint.h>
#include <string.h>
#include "uzlib.h"
volatile int sink_status;
volatile const uint8_t *volatile_in_ptr;
volatile uint8_t *volatile_out_ptr;
int main(void) {
    static const uint8_t in[2] = {3, 0};
    static uint8_t out[16];
    TINF_DATA d;
    memset(&d, 0, sizeof(d));
    uzlib_init();
    uzlib_uncompress_init(&d, NULL, 0);
    volatile_in_ptr = in;
    volatile_out_ptr = out;
    d.source = (const unsigned char *)volatile_in_ptr;
    d.source_limit = in + sizeof(in);
    d.dest_start = (unsigned char *)volatile_out_ptr;
    d.dest = (unsigned char *)volatile_out_ptr;
    d.dest_limit = out + sizeof(out);
    sink_status = uzlib_uncompress(&d);
    return sink_status;
}
""")
        run([
            gcc, *common_flags,
            "-I", third_party / "uzlib/src",
            wrapper, third_party / "uzlib/src/tinflate.c",
            "-o", elf,
        ])
    elif name == "heatshrink":
        write_file(wrapper, """
#include <stdint.h>
#include "heatshrink_decoder.h"
volatile int sink_status;
volatile uint8_t *volatile_in_ptr;
volatile uint8_t *volatile_out_ptr;
int main(void) {
    heatshrink_decoder dec;
    uint8_t in[2] = {0};
    uint8_t out[16];
    size_t used = 0, produced = 0;
    heatshrink_decoder_reset(&dec);
    volatile_in_ptr = in;
    volatile_out_ptr = out;
    sink_status = heatshrink_decoder_sink(&dec, (uint8_t *)volatile_in_ptr, sizeof(in), &used);
    sink_status += heatshrink_decoder_poll(&dec, (uint8_t *)volatile_out_ptr, sizeof(out), &produced);
    return sink_status;
}
""")
        run([
            gcc, *common_flags,
            "-DHEATSHRINK_DYNAMIC_ALLOC=0",
            "-DHEATSHRINK_STATIC_INPUT_BUFFER_SIZE=256",
            "-DHEATSHRINK_STATIC_WINDOW_BITS=11",
            "-DHEATSHRINK_STATIC_LOOKAHEAD_BITS=4",
            "-I", third_party / "heatshrink",
            wrapper, third_party / "heatshrink/heatshrink_decoder.c",
            "-o", elf,
        ])
    else:
        raise ValueError(name)

    total_size = parse_size(run([size_tool, "-A", elf]).stdout)
    return total_size - baseline_size


def parse_size(size_output):
    total = 0
    for line in size_output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] in {
            ".text", ".literal", ".rodata", ".data", ".srodata", ".sdata"
        }:
            try:
                total += int(parts[1])
            except ValueError:
                pass
    return total


def build_heatshrink_cli(third_party):
    cli = third_party / "heatshrink/heatshrink"
    if cli.exists():
        return cli
    run(["make", "heatshrink"], cwd=third_party / "heatshrink", capture=False)
    return cli


def heatshrink_compress(cli, data, work, window=11, lookahead=4):
    work.mkdir(parents=True, exist_ok=True)
    src = work / "heatshrink_input.bin"
    dst = work / "heatshrink_output.bin"
    src.write_bytes(data)
    run([cli, "-e", "-w", str(window), "-l", str(lookahead), src, dst])
    return dst.read_bytes()


def desktop_decode_proxy(payload, raw_deflated, heatshrink_blob, heatshrink_cli, work, loops):
    results = []

    start = time.perf_counter()
    for _ in range(loops):
        out = zlib.decompress(raw_deflated, -15)
    elapsed = time.perf_counter() - start
    assert out == payload
    results.append(("deflate via Python zlib", elapsed, len(payload) * loops / elapsed))

    src = work / "hs_decode_input.bin"
    dst = work / "hs_decode_output.bin"
    src.write_bytes(heatshrink_blob)
    start = time.perf_counter()
    for _ in range(max(1, min(loops, 20))):
        run([heatshrink_cli, "-d", src, dst])
    hs_loops = max(1, min(loops, 20))
    elapsed = time.perf_counter() - start
    assert dst.read_bytes() == payload
    results.append(("heatshrink CLI process", elapsed, len(payload) * hs_loops / elapsed))

    return results


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate miniz/uzlib/heatshrink decode footprint and sprite payload compression.",
    )
    parser.add_argument("--third-party", type=Path, default=DEFAULT_THIRD_PARTY)
    parser.add_argument("--xtensa-bin", type=Path, default=DEFAULT_XTENSA_BIN)
    parser.add_argument("--loops", type=int, default=80)
    args = parser.parse_args()

    require_sources(args.third_party)
    gen = load_generator()
    payloads, all_payload = build_sprite_payloads(gen)

    with tempfile.TemporaryDirectory(prefix="stickmon-decompress-eval-") as tmp:
        work = Path(tmp)
        hs_cli = build_heatshrink_cli(args.third_party)

        raw_deflated = raw_deflate(all_payload, 6)
        zlib_wrapped = zlib.compress(all_payload, 6)
        heatshrink_blob = heatshrink_compress(hs_cli, all_payload, work, 11, 4)

        per_species_deflate = sum(len(raw_deflate(payload, 6)) for payload in payloads.values())
        per_species_hs = 0
        for species_id, payload in payloads.items():
            per_species_hs += len(heatshrink_compress(hs_cli, payload, work / f"hs_{species_id}", 11, 4))

        print("ESP32-S3 Decompressor Evaluation")
        print("================================")
        print(f"payload source:      generated Pokemon sprite RLE+palettes")
        print(f"species blocks:      {len(payloads)}")
        print(f"current payload:     {kib(len(all_payload))}")
        print(f"raw deflate global:  {kib(len(raw_deflated))} {percent(len(raw_deflated), len(all_payload))}")
        print(f"zlib global:         {kib(len(zlib_wrapped))} {percent(len(zlib_wrapped), len(all_payload))}")
        print(f"heatshrink global:   {kib(len(heatshrink_blob))} {percent(len(heatshrink_blob), len(all_payload))}")
        print(f"raw deflate/species: {kib(per_species_deflate)} {percent(per_species_deflate, len(all_payload))}")
        print(f"heatshrink/species:  {kib(per_species_hs)} {percent(per_species_hs, len(all_payload))}")

        print("\nLinked Decoder Code Size Delta")
        print("------------------------------")
        for name in ("miniz", "uzlib", "heatshrink"):
            try:
                delta = build_size_delta(name, args.third_party, args.xtensa_bin, work)
                print(f"{name:12s} {kib(delta)}")
            except subprocess.CalledProcessError as exc:
                print(f"{name:12s} build failed")
                if exc.stderr:
                    print(exc.stderr)

        print("\nDesktop Decode Proxy")
        print("--------------------")
        for name, elapsed, throughput in desktop_decode_proxy(
            all_payload, raw_deflated, heatshrink_blob, hs_cli, work, args.loops
        ):
            print(f"{name:24s} {throughput / (1024 * 1024):7.1f} MB/s over {elapsed:.3f}s")

        print("\nNotes")
        print("-----")
        print("- Code size is an xtensa-esp32s3 empty-program link delta with --gc-sections.")
        print("- Desktop speed is only a relative proxy; exact ESP32-S3 speed needs an on-device millis() benchmark.")
        print("- miniz and uzlib both consume the same raw deflate stream; heatshrink uses its own LZSS format.")


if __name__ == "__main__":
    main()
