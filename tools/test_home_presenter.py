#!/usr/bin/env python3
"""Exercise the home camera and breathing phase with real sprite decoding."""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]


def sleep_pack(species, first_kind, bounds):
    """Encode two opaque rectangles inside equal, padded 16x16 canvases."""
    records, words = [], []
    for index, (width, height) in enumerate(bounds):
        offset = len(words)
        words.append(0x8000 | 16)
        for _ in range(height):
            words.extend([0x8001, width, *([0xFFFF] * width),
                          0x8000 | (15 - width)])
        words.append(0x8000 | ((15 - height) * 16))
        records.append(struct.pack('<HBBBBHIII4B', first_kind + index,
                                   16, 16, 0, 0, 0, offset,
                                   len(words) - offset, 0, 0, 0, 0, 0))
    payload = b''.join(records) + struct.pack(f'<{len(words)}H', *words)
    encoder = zlib.compressobj(wbits=-15)
    compressed = encoder.compress(payload) + encoder.flush()
    header = struct.pack('<IHHHHHHIII', 0x5350534D, 3, species, 2,
                         len(words), 0, 1, len(payload), len(compressed),
                         zlib.crc32(payload))
    return header + compressed


class HomePresenterTests(unittest.TestCase):
    def test_camera_and_resource_derived_breathing(self):
        presenter = ROOT / 'firmware/amoled_1_8_v1/main/app/HomePresenter.cpp'
        self.assertTrue(presenter.exists(), 'independent home presenter is missing')
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            resources = temporary / 'resources'
            sprites = resources / 'packs/dev/sprites'
            sprites.mkdir(parents=True)
            for path in ('active.json', 'packs/dev/manifest.json'):
                shutil.copyfile(ROOT / 'data' / path, resources / path)
            for species, first_kind, bounds in (
                    (1, 23, ((10, 5), (4, 6))),
                    (4, 89, ((4, 6), (8, 6))),
                    (7, 155, ((4, 6), (4, 6))),
                    (25, 365, ((4, 7), (10, 6)))):
                (sprites / f'{species:03d}.smonsp').write_bytes(
                    sleep_pack(species, first_kind, bounds))
            sanitize = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] \
                if os.environ.get('STICKMON_TEST_SANITIZE') == '1' else []
            inflate = temporary / 'inflate.o'
            subprocess.run(['cc', '-O1', '-ffunction-sections', '-fdata-sections',
                            *sanitize, '-c', 'src/third_party/uzlib/tinflate.c',
                            '-o', str(inflate)], cwd=ROOT, check=True)
            sources = [
                'tools/home_presenter_host.cpp', str(presenter),
                'src/assets/PokemonSprites.cpp', 'src/game/GameRandom.cpp',
                'src/platform/desktop/DesktopPlatform.cpp',
                'src/platform/api/PlatformServices.cpp',
                'src/core/ResourceFS.cpp', 'src/core/ResourcePack.cpp',
                'src/core/DeflateDecoder.cpp',
            ]
            binary = temporary / 'presenter'
            subprocess.run([
                'c++', '-std=c++17', '-O1', '-ffunction-sections', '-fdata-sections',
                '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
                *sanitize, '-Isrc', '-Ifirmware/amoled_1_8_v1/main',
                *sources, str(inflate), '-o', str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary), str(resources)], cwd=ROOT, check=True)


if __name__ == '__main__':
    unittest.main()
