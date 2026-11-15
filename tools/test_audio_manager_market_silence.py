#!/usr/bin/env python3
"""Check complete market performances, PCM fade, and independent quiet intervals."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import wave

import generate_audio_assets as audio

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='market-music-') as directory:
        temporary = Path(directory)
        fixture = temporary / 'fixture'
        tracks = fixture / 'packs/dev/audio'
        tracks.mkdir(parents=True)
        shutil.copy2(ROOT / 'data/active.json', fixture / 'active.json')
        shutil.copy2(ROOT / 'data/packs/dev/manifest.json', tracks.parent / 'manifest.json')
        for pack in (ROOT / 'data/packs/dev/audio').glob('*.smonaudio'):
            shutil.copy2(pack, tracks / pack.name)
        samples = [((i * 271) % 40000) - 20000 for i in range(8000 * 4 + 137)]
        (tracks / 'bgm_market.smonaudio').write_bytes(audio.make_audio_pack(samples, 8000, True, 317))
        for queue_target in (2, 3, 4):
            binary = temporary / f'market-{queue_target}'
            subprocess.run([
                'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                f'-DSTICKMON_MUSIC_QUEUE_TARGET_BLOCKS={queue_target}', '-Isrc',
                'tools/audio_manager_market_silence_host.cpp', 'src/core/AudioManager.cpp',
                'src/core/ResourcePack.cpp', 'src/core/ResourceFS.cpp',
                'src/platform/api/PlatformServices.cpp', 'src/platform/desktop/DesktopPlatform.cpp',
                '-o', str(binary),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            for label, resources in (('real', ROOT / 'data'), ('8khz', fixture)):
                command = [str(binary), str(resources)]
                raw = temporary / 'market.pcm'
                if args.output_dir and label == 'real' and queue_target == 2:
                    command.append(str(raw))
                result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
                if result.returncode:
                    raise AssertionError(result.stdout + result.stderr)
                print(f'queue={queue_target}, {label}: {result.stdout.strip()}')
                if len(command) == 3:
                    pcm = raw.read_bytes()
                    for name, data in (('market-complete.wav', pcm), ('market-ending.wav', pcm[-6*16000:])):
                        with wave.open(str(args.output_dir / name), 'wb') as output:
                            output.setnchannels(1); output.setsampwidth(1); output.setframerate(16000)
                            output.writeframes(data)


if __name__ == '__main__':
    main()
