#!/usr/bin/env python3

import array
import json
import math
import re
import struct
import sys
import wave
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "origin_asset" / "pokemon_cries"
SPECIES_SOURCE = ROOT / "src" / "game" / "Species.cpp"
PACK_OUT = ROOT / "data" / "packs" / "dev"
CRY_OUT = PACK_OUT / "cries"
MANIFEST_PATH = PACK_OUT / "manifest.json"

SOURCE_SAMPLE_RATE = 44100
TARGET_SAMPLE_RATE = 22050
TARGET_RMS_DBFS = -16.0
PEAK_LIMIT_DBFS = -1.0
MAX_GAIN_DB = 6.0

CRY_PACK_MAGIC = int.from_bytes(b"SMCR", "little")
CRY_PACK_VERSION = 1
CRY_PACK_FLAG_RAW_DEFLATE = 1 << 0
CRY_PACK_FLAG_PCM_U8_MONO = 1 << 1
CRY_PACK_FLAGS = CRY_PACK_FLAG_RAW_DEFLATE | CRY_PACK_FLAG_PCM_U8_MONO
CRY_HEADER_FORMAT = "<IHHIIIIIHH"
CRY_HEADER_SIZE = struct.calcsize(CRY_HEADER_FORMAT)


def current_species_ids(path=SPECIES_SOURCE):
    source = Path(path).read_text(encoding="utf-8")
    ids = {int(value) for value in re.findall(r"^\s*\{(\d+),", source, re.MULTILINE)}
    if not ids:
        raise ValueError(f"No species rows found in {path}")
    return ids


def source_cries(path=SOURCE_DIR):
    cries = {}
    for source_path in sorted(Path(path).glob("*.wav")):
        match = re.fullmatch(r"(\d{3})_[a-z0-9_]+\.wav", source_path.name)
        if not match:
            raise ValueError(f"Invalid cry filename: {source_path.name}")
        species_id = int(match.group(1))
        if species_id in cries:
            raise ValueError(f"Duplicate cry for species {species_id}")
        cries[species_id] = source_path
    return cries


def _load_and_downsample(path):
    with wave.open(str(path), "rb") as source:
        if source.getnchannels() != 2:
            raise ValueError(f"{path.name}: expected stereo PCM")
        if source.getsampwidth() != 2:
            raise ValueError(f"{path.name}: expected 16-bit PCM")
        if source.getframerate() != SOURCE_SAMPLE_RATE:
            raise ValueError(f"{path.name}: expected {SOURCE_SAMPLE_RATE} Hz")
        if source.getcomptype() != "NONE":
            raise ValueError(f"{path.name}: compressed WAV input is unsupported")
        frame_count = source.getnframes()
        samples = array.array("h")
        samples.frombytes(source.readframes(frame_count))

    if sys.byteorder != "little":
        samples.byteswap()
    if len(samples) != frame_count * 2:
        raise ValueError(f"{path.name}: truncated PCM payload")

    downsampled = []
    for frame in range(0, frame_count, 2):
        first = frame * 2
        total = samples[first] + samples[first + 1]
        count = 2
        if frame + 1 < frame_count:
            second = first + 2
            total += samples[second] + samples[second + 1]
            count += 2
        downsampled.append(total / count)
    return downsampled


def _normalize_and_quantize(samples):
    if not samples:
        raise ValueError("Cry contains no samples")
    peak = max(abs(value) for value in samples)
    rms = math.sqrt(sum(value * value for value in samples) / len(samples))
    if peak <= 0.0 or rms <= 0.0:
        raise ValueError("Cry contains only silence")

    full_scale = 32768.0
    target_rms = full_scale * (10.0 ** (TARGET_RMS_DBFS / 20.0))
    peak_limit = 32767.0 * (10.0 ** (PEAK_LIMIT_DBFS / 20.0))
    max_gain = 10.0 ** (MAX_GAIN_DB / 20.0)
    gain = min(target_rms / rms, peak_limit / peak, max_gain)

    pcm = bytearray(len(samples))
    for index, value in enumerate(samples):
        normalized = max(-32768.0, min(32767.0, value * gain))
        quantized = int(round(normalized / 256.0)) + 128
        pcm[index] = max(0, min(255, quantized))
    return bytes(pcm), gain


def encode_source(path):
    samples = _load_and_downsample(Path(path))
    pcm, gain = _normalize_and_quantize(samples)
    compressor = zlib.compressobj(level=6, wbits=-15)
    compressed = compressor.compress(pcm) + compressor.flush()
    return pcm, compressed, gain


def make_pack(species_id, pcm, compressed):
    header = struct.pack(
        CRY_HEADER_FORMAT,
        CRY_PACK_MAGIC,
        CRY_PACK_VERSION,
        species_id,
        TARGET_SAMPLE_RATE,
        len(pcm),
        len(pcm),
        len(compressed),
        zlib.crc32(pcm) & 0xFFFFFFFF,
        CRY_PACK_FLAGS,
        0,
    )
    return header + compressed


def merge_manifest(cry_count, pack_out=PACK_OUT):
    manifest_path = Path(pack_out) / "manifest.json"
    payload = {}
    if manifest_path.exists():
        payload = json.loads(manifest_path.read_text(encoding="utf-8"))
        if not isinstance(payload, dict):
            raise ValueError(f"Invalid resource manifest: {manifest_path}")
    payload.update({
        "cries": "cries",
        "cryCount": cry_count,
        "cryEncoding": "raw-deflate-pcm-u8-mono-22050",
        "format": "smon-resource-pack-v1",
        "id": payload.get("id", "dev"),
        "schema": 1,
        "version": payload.get("version", "0.0.0-dev"),
    })
    manifest_path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def generate(source_dir=SOURCE_DIR, species_source=SPECIES_SOURCE, pack_out=PACK_OUT):
    species_ids = current_species_ids(species_source)
    cries = source_cries(source_dir)
    missing = sorted(species_ids - cries.keys())
    if missing:
        raise ValueError("Missing cries for species: " + ", ".join(map(str, missing)))

    extra = sorted(cries.keys() - species_ids)
    output_dir = Path(pack_out) / "cries"
    output_dir.mkdir(parents=True, exist_ok=True)
    for old_path in output_dir.glob("*.smoncry"):
        old_path.unlink()

    source_bytes = 0
    raw_bytes = 0
    compressed_bytes = 0
    gain_min = float("inf")
    gain_max = 0.0
    for species_id in sorted(species_ids):
        source_path = cries[species_id]
        pcm, compressed, gain = encode_source(source_path)
        pack = make_pack(species_id, pcm, compressed)
        (output_dir / f"{species_id:03d}.smoncry").write_bytes(pack)
        source_bytes += source_path.stat().st_size
        raw_bytes += len(pcm)
        compressed_bytes += len(compressed)
        gain_min = min(gain_min, gain)
        gain_max = max(gain_max, gain)

    merge_manifest(len(species_ids), pack_out)
    print(
        f"cries={len(species_ids)} source={source_bytes} raw={raw_bytes} "
        f"compressed={compressed_bytes} ratio={compressed_bytes / raw_bytes:.3f} "
        f"gain={gain_min:.3f}..{gain_max:.3f}"
    )
    if extra:
        print("unused source cries: " + ", ".join(f"{value:03d}" for value in extra))
    return len(species_ids)


def main():
    try:
        generate()
    except (OSError, ValueError, wave.Error, json.JSONDecodeError) as error:
        raise SystemExit(f"[pokemon-cries] {error}") from error


if __name__ == "__main__":
    main()
