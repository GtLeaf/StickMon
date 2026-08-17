#!/usr/bin/env python3

import argparse
import array
import json
import math
import shutil
import struct
import subprocess
import tempfile
import wave
import zlib
from pathlib import Path

from asset_paths import essentials_dir

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ESSENTIALS = essentials_dir()
PACK_OUT = ROOT / "data" / "packs" / "dev"
AUDIO_OUT = PACK_OUT / "audio"

MUSIC_SAMPLE_RATE = 16000
SFX_SAMPLE_RATE = 22050
MUSIC_MAX_SECONDS = 30
MUSIC_TARGET_DBFS = -11.0
SFX_TARGET_DBFS = -15.0
IMA_BLOCK_BYTES = 1024
IMA_BLOCK_SAMPLES = 1 + (IMA_BLOCK_BYTES - 4) * 2

AUDIO_PACK_MAGIC = int.from_bytes(b"SMAU", "little")
AUDIO_PACK_VERSION = 2
AUDIO_FLAG_IMA_ADPCM = 1 << 0
AUDIO_FLAG_MONO = 1 << 1
AUDIO_FLAG_LOOP = 1 << 2
AUDIO_HEADER_FORMAT = "<IHHIIHHIIII"
AUDIO_HEADER_SIZE = struct.calcsize(AUDIO_HEADER_FORMAT)

MUSIC_SOURCES = {
    "bgm_home": "Audio/BGM/Lappet Town.mid",
    "bgm_explore": "Audio/BGM/Route 1.mid",
    "bgm_battle": "Audio/BGM/Battle wild.ogg",
    "bgm_battle_special": "Audio/BGM/Battle roaming.mid",
}

SFX_SOURCES = {
    "sfx_ui_cursor": "Audio/SE/GUI sel cursor.ogg",
    "sfx_ui_confirm": "Audio/SE/GUI sel decision.ogg",
    "sfx_ui_cancel": "Audio/SE/GUI sel cancel.ogg",
    "sfx_menu_open": "Audio/SE/GUI menu open.ogg",
    "sfx_menu_close": "Audio/SE/GUI menu close.ogg",
    "sfx_damage_normal": "Audio/SE/Battle damage normal.ogg",
    "sfx_damage_super": "Audio/SE/Battle damage super.ogg",
    "sfx_damage_weak": "Audio/SE/Battle damage weak.ogg",
    "sfx_throw": "Audio/SE/Battle throw.ogg",
    "sfx_faint": "Audio/SE/Pkmn faint.ogg",
    "sfx_exp_gain": "Audio/SE/Pkmn exp gain.ogg",
    "sfx_exp_full": "Audio/SE/Pkmn exp full.ogg",
    "sfx_level_up": "Audio/SE/Pkmn level up.ogg",
    "sfx_move_learnt": "Audio/SE/Pkmn move learnt.ogg",
    "sfx_contact": "Audio/ME/Register phone.ogg",
}

IMA_INDEX_TABLE = (
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
)
IMA_STEP_TABLE = (
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
    143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
    494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
    27086, 29794, 32767,
)


def require_tool(name):
    path = shutil.which(name)
    if not path:
        raise ValueError(f"Required tool is unavailable: {name}")
    return path


def render_source(source, output, sample_rate, soundfont):
    source = Path(source)
    intermediate = source
    if source.suffix.lower() in {".mid", ".midi"}:
        if not soundfont:
            raise ValueError("MIDI sources require --soundfont")
        intermediate = output.with_name(output.stem + "-synth.wav")
        subprocess.run(
            [
                require_tool("fluidsynth"), "-ni", "-g", "0.7",
                "-F", str(intermediate), str(soundfont), str(source),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
    subprocess.run(
        [
            require_tool("ffmpeg"), "-v", "error", "-y", "-i",
            str(intermediate), "-ac", "1", "-ar", str(sample_rate),
            "-c:a", "pcm_s16le", str(output),
        ],
        check=True,
        capture_output=True,
        text=True,
    )


def read_loop_points(source, target_sample_rate):
    source = Path(source)
    if source.suffix.lower() in {".mid", ".midi"}:
        return None
    result = subprocess.run(
        [
            require_tool("ffprobe"), "-v", "error", "-select_streams", "a:0",
            "-show_entries", "stream=sample_rate:stream_tags", "-of", "json",
            str(source),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    streams = json.loads(result.stdout).get("streams", [])
    if not streams:
        return None
    stream = streams[0]
    tags = {key.upper(): value for key, value in stream.get("tags", {}).items()}
    if "LOOPSTART" not in tags:
        return None
    source_rate = int(stream["sample_rate"])
    source_start = int(tags["LOOPSTART"])
    if "LOOPLENGTH" in tags:
        source_end = source_start + int(tags["LOOPLENGTH"])
    elif "LOOPEND" in tags:
        source_end = int(tags["LOOPEND"])
    else:
        return None
    if source_rate <= 0 or source_start < 0 or source_end <= source_start:
        raise ValueError(f"Invalid loop metadata: {source}")
    return (
        round(source_start * target_sample_rate / source_rate),
        round(source_end * target_sample_rate / source_rate),
    )


def read_pcm16(path):
    with wave.open(str(path), "rb") as source:
        if source.getnchannels() != 1 or source.getsampwidth() != 2:
            raise ValueError(f"Unexpected rendered WAV format: {path}")
        sample_rate = source.getframerate()
        samples = array.array("h")
        samples.frombytes(source.readframes(source.getnframes()))
    if samples.itemsize != 2:
        raise ValueError("Host signed-short width is unsupported")
    return list(samples), sample_rate


def normalize(samples, target_dbfs, peak_dbfs=-1.5, soft_limit=False):
    if not samples:
        raise ValueError("Audio source has no samples")
    peak = max(abs(value) for value in samples)
    rms = math.sqrt(sum(value * value for value in samples) / len(samples))
    if peak == 0 or rms == 0:
        raise ValueError("Audio source is silent")
    target_rms = 32768.0 * (10.0 ** (target_dbfs / 20.0))
    peak_limit = 32767.0 * (10.0 ** (peak_dbfs / 20.0))
    if soft_limit:
        def limited_rms(gain):
            return math.sqrt(sum(
                (peak_limit * math.tanh(value * gain / peak_limit)) ** 2
                for value in samples
            ) / len(samples))

        low = 0.0
        high = target_rms / rms
        while limited_rms(high) < target_rms:
            high *= 2.0
        for _ in range(14):
            middle = (low + high) * 0.5
            if limited_rms(middle) < target_rms:
                low = middle
            else:
                high = middle
        return [
            max(-32768, min(32767, int(round(
                peak_limit * math.tanh(value * high / peak_limit)
            ))))
            for value in samples
        ]
    gain = min(target_rms / rms, peak_limit / peak)
    return [
        max(-32768, min(32767, int(round(value * gain))))
        for value in samples
    ]


def prepare_music(samples, sample_rate, loop_points=None):
    if loop_points:
        loop_start, loop_end = loop_points
        if loop_start < 0 or loop_end <= loop_start or loop_end > len(samples):
            raise ValueError("Rendered music does not contain its loop range")
        return samples[:loop_end], loop_start

    wanted = MUSIC_MAX_SECONDS * sample_rate
    if len(samples) > wanted:
        samples = samples[:wanted]
    full_blocks = len(samples) // IMA_BLOCK_SAMPLES
    if full_blocks == 0:
        raise ValueError("Music source is shorter than one ADPCM block")
    samples = samples[:full_blocks * IMA_BLOCK_SAMPLES]
    fade = min(sample_rate // 2, len(samples) // 8)
    if fade > 1:
        target = samples[0]
        start = len(samples) - fade
        for offset in range(fade):
            remaining = fade - 1 - offset
            samples[start + offset] = (
                samples[start + offset] * remaining + target * offset
            ) // (fade - 1)
    return samples, 0


def encode_nibble(sample, predictor, index):
    step = IMA_STEP_TABLE[index]
    difference = sample - predictor
    code = 8 if difference < 0 else 0
    difference = abs(difference)
    delta = step >> 3
    if difference >= step:
        code |= 4
        difference -= step
        delta += step
    if difference >= step >> 1:
        code |= 2
        difference -= step >> 1
        delta += step >> 1
    if difference >= step >> 2:
        code |= 1
        delta += step >> 2
    predictor += -delta if code & 8 else delta
    predictor = max(-32768, min(32767, predictor))
    index = max(0, min(88, index + IMA_INDEX_TABLE[code]))
    return code, predictor, index


def encode_ima_blocks(samples):
    if not samples:
        raise ValueError("Cannot encode empty audio")
    blocks = bytearray()
    index = 0
    for start in range(0, len(samples), IMA_BLOCK_SAMPLES):
        block_samples = list(samples[start:start + IMA_BLOCK_SAMPLES])
        block_samples.extend([block_samples[-1]] * (IMA_BLOCK_SAMPLES - len(block_samples)))
        predictor = block_samples[0]
        block = bytearray(struct.pack("<hBB", predictor, index, 0))
        nibbles = []
        for sample in block_samples[1:]:
            code, predictor, index = encode_nibble(sample, predictor, index)
            nibbles.append(code)
        for nibble in range(0, len(nibbles), 2):
            block.append(nibbles[nibble] | (nibbles[nibble + 1] << 4))
        if len(block) != IMA_BLOCK_BYTES:
            raise AssertionError(f"Unexpected IMA block size: {len(block)}")
        blocks.extend(block)
    return bytes(blocks)


def make_audio_pack(samples, sample_rate, looping, loop_start_sample=0):
    if loop_start_sample < 0 or loop_start_sample >= len(samples):
        raise ValueError("Loop start sample is outside the audio stream")
    payload = encode_ima_blocks(samples)
    block_count = len(payload) // IMA_BLOCK_BYTES
    flags = AUDIO_FLAG_IMA_ADPCM | AUDIO_FLAG_MONO
    if looping:
        flags |= AUDIO_FLAG_LOOP
    header = struct.pack(
        AUDIO_HEADER_FORMAT,
        AUDIO_PACK_MAGIC,
        AUDIO_PACK_VERSION,
        flags,
        sample_rate,
        len(samples),
        IMA_BLOCK_BYTES,
        IMA_BLOCK_SAMPLES,
        block_count,
        loop_start_sample,
        len(payload),
        zlib.crc32(payload) & 0xFFFFFFFF,
    )
    return header + payload


def merge_manifest(music_count, sfx_count):
    manifest_path = PACK_OUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest.update({
        "audio": "audio",
        "audioCount": music_count + sfx_count,
        "audioEncoding": "ima-adpcm-mono-block-v2",
        "musicCount": music_count,
        "sfxCount": sfx_count,
    })
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def generate(essentials, soundfont):
    essentials = Path(essentials)
    soundfont = Path(soundfont) if soundfont else None
    all_sources = {**MUSIC_SOURCES, **SFX_SOURCES}
    missing = [relative for relative in all_sources.values()
               if not (essentials / relative).is_file()]
    if missing:
        raise ValueError("Missing audio sources: " + ", ".join(missing))
    if soundfont and not soundfont.is_file():
        raise ValueError(f"SoundFont does not exist: {soundfont}")

    AUDIO_OUT.mkdir(parents=True, exist_ok=True)
    for old in AUDIO_OUT.glob("*.smonaudio"):
        old.unlink()

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_dir = Path(temp_dir)
        for audio_id, relative in all_sources.items():
            music = audio_id in MUSIC_SOURCES
            rate = MUSIC_SAMPLE_RATE if music else SFX_SAMPLE_RATE
            rendered = temp_dir / f"{audio_id}.wav"
            source = essentials / relative
            loop_points = read_loop_points(source, rate) if music else None
            render_source(source, rendered, rate, soundfont)
            samples, actual_rate = read_pcm16(rendered)
            if actual_rate != rate:
                raise ValueError(f"Unexpected sample rate for {audio_id}: {actual_rate}")
            samples = normalize(
                samples, MUSIC_TARGET_DBFS if music else SFX_TARGET_DBFS,
                soft_limit=music,
            )
            loop_start_sample = 0
            if music:
                samples, loop_start_sample = prepare_music(
                    samples, actual_rate, loop_points
                )
            pack = make_audio_pack(
                samples, actual_rate, music, loop_start_sample
            )
            (AUDIO_OUT / f"{audio_id}.smonaudio").write_bytes(pack)
            print(
                f"{audio_id}: seconds={len(samples) / actual_rate:.2f} "
                f"loop={loop_start_sample / actual_rate:.2f} "
                f"pack={len(pack)}"
            )

    merge_manifest(len(MUSIC_SOURCES), len(SFX_SOURCES))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--essentials", type=Path, default=DEFAULT_ESSENTIALS)
    parser.add_argument("--soundfont", type=Path, required=True)
    args = parser.parse_args()
    try:
        generate(args.essentials, args.soundfont)
    except (OSError, ValueError, subprocess.CalledProcessError,
            json.JSONDecodeError, wave.Error) as error:
        raise SystemExit(f"[audio-assets] {error}") from error


if __name__ == "__main__":
    main()
