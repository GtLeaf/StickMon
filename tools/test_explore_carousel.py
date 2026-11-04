#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check round carousel pixels, real engine cadence, loading and memory fallback."""
import argparse
import subprocess
import sys
from pathlib import Path

from build_web import ROOT, sources_for


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=ROOT / '.stick-local/artifacts/explore-carousel-host')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    replacements = {p.name for p in (ROOT / 'src/ui/round_touch').glob('*.cpp')}
    sources = [p for p in sources_for('ostb') if '/platform/web/' not in str(p)
               and not (p.parent.name == 'compact_touch' and p.name in replacements)]
    sources += sorted((ROOT / 'src/ui/round_touch').glob('*.cpp'))
    sources += [ROOT / 'src/platform/desktop/DesktopPlatform.cpp',
                ROOT / 'tools/explore_carousel_host.cpp']
    flags = ['-DSTICKMON_UI_COMPACT_TOUCH=1', '-DSTICKMON_UI_ROUND_TOUCH=1',
             '-DSTICKMON_TOUCH_INPUT=1', '-DSTICKMON_DISPLAY_WIDTH=360',
             '-DSTICKMON_DISPLAY_HEIGHT=360', '-DSTICKMON_UI_HEADER_INSET_X=60',
             '-DSTICKMON_UI_HEADER_TOP=32', '-DSTICKMON_ROOM_PIXEL_SCALE=1',
             '-DSTICKMON_ENABLE_DEBUG_FEATURES=0', '-DSTICKMON_ENABLE_TRACE_LOGS=0',
             '-DSTICKMON_HAS_CLAW=0']
    checks = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    obj = output / 'tinflate.o'
    binary = output / 'explore-carousel-host'
    with (output / 'compile.log').open('w') as log:
        subprocess.run(['cc', *checks, '-c', str(ROOT / 'src/third_party/uzlib/tinflate.c'),
                        '-o', str(obj)], check=True, stdout=log, stderr=log)
        subprocess.run(['c++', '-std=c++17', *checks, '-ffunction-sections',
                        '-fdata-sections', '-I' + str(ROOT / 'src'), *flags,
                        *[str(p) for p in sources if p.suffix == '.cpp'], str(obj),
                        '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
                        '-o', str(binary)], check=True, stdout=log, stderr=log)
    with (output / 'run.log').open('w') as log:
        subprocess.run([str(binary), str(ROOT / 'data')], check=True, stdout=log, stderr=log)
    print((output / 'run.log').read_text())


if __name__ == '__main__':
    main()
