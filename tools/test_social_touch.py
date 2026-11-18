#!/usr/bin/env python3
"""Exercise production communication rendering and touch on maintained targets."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from amoled_host import build_amoled_host


class SocialTouchTests(unittest.TestCase):
    def test_native_round_touch(self):
        sources = [
            'tools/social_touch_host.cpp', 'tools/amoled_checksum_guard.cpp',
            'src/platform/desktop/DesktopPlatform.cpp',
            'src/platform/api/PlatformServices.cpp', 'src/core/FontResource.cpp',
            'src/core/ResourceFS.cpp', 'src/core/ResourcePack.cpp',
            'src/core/DeflateDecoder.cpp', 'src/presentation/Canvas565.cpp',
            'src/presentation/PixelRenderer.cpp', 'src/presentation/TouchUi.cpp',
            'src/presentation/MenuGrid.cpp', 'src/presentation/VisitMenu.cpp',
            'src/assets/FontFallbackCN.cpp', 'src/assets/VisitIcons.cpp', 'src/ui/common/SocialUiBase.cpp',
            'src/ui/round_touch/SocialUi.cpp', 'src/ui/round_touch/RoundPage.cpp',
            'src/ui/round_touch/SceneSurface.cpp',
        ]
        flags = [
            '-DSTICKMON_UI_ROUND_TOUCH=1', '-DSTICKMON_UI_COMPACT_TOUCH=1',
            '-DSTICKMON_TOUCH_INPUT=1', '-DSTICKMON_DISPLAY_WIDTH=360',
            '-DSTICKMON_DISPLAY_HEIGHT=360',
        ]
        with tempfile.TemporaryDirectory() as tmp:
            obj, binary = Path(tmp) / 'tinflate.o', Path(tmp) / 'social'
            common = ['-O1', '-fsanitize=address,undefined',
                      '-ffunction-sections', '-fdata-sections']
            subprocess.run(['cc', *common, '-c', 'src/third_party/uzlib/tinflate.c',
                            '-o', str(obj)], cwd=ROOT, check=True)
            subprocess.run(['c++', '-std=c++17', *common, '-Wl,--gc-sections',
                            '-Isrc', *flags, *sources, str(obj), '-o', str(binary)],
                           cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True,
                           env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1',
                                'UBSAN_OPTIONS': 'halt_on_error=1'})

    def test_amoled_touch_and_bands(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = build_amoled_host(Path(tmp) / 'amoled', [
                'tools/amoled_communication_host.cpp', 'tools/amoled_checksum_guard.cpp'])
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == '__main__':
    unittest.main()
