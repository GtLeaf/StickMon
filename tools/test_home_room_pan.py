#!/usr/bin/env python3
"""AMOLED home room look-around drag through the production application."""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

from amoled_host import ROOT, build_amoled_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='keep binary and screenshots here')
    args = parser.parse_args()
    sanitize = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] \
        if os.environ.get('STICKMON_TEST_SANITIZE') == '1' else []
    with tempfile.TemporaryDirectory(prefix='home-pan-') as directory:
        out = (args.output or Path(directory)).resolve()
        out.mkdir(parents=True, exist_ok=True)
        binary = build_amoled_host(out / 'home-room-pan',
                                   [ROOT / 'tools/home_room_pan_host.cpp',
                                    ROOT / 'tools/amoled_checksum_guard.cpp'],
                                   extra_flags=sanitize)
        run = out / 'run'
        run.mkdir(exist_ok=True)
        env = dict(os.environ)
        if sanitize:
            env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
        subprocess.run([str(binary), str(ROOT / 'data'), str(run)], cwd=ROOT,
                       check=True, env=env)
        if args.output:
            from PIL import Image
            for ppm in run.glob('*.ppm'):
                Image.open(ppm).save(ppm.with_suffix('.png'))
                ppm.unlink()


if __name__ == '__main__':
    main()
