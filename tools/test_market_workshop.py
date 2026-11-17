#!/usr/bin/env python3
"""Run the photo-frame gift against the production AMOLED application."""
import argparse
import subprocess
from pathlib import Path

from amoled_host import ROOT, build_amoled_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--debug', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    binary = build_amoled_host(out / 'amoled-workshop', [
        ROOT / 'tools/market_workshop_amoled_host.cpp',
        ROOT / 'tools/amoled_checksum_guard.cpp',
    ], debug=args.debug)
    subprocess.run([str(binary), str(ROOT / 'data'), str(out)],
                   cwd=ROOT, check=True)
    from PIL import Image
    for ppm in out.glob('*.ppm'):
        Image.open(ppm).save(ppm.with_suffix('.png'))
        ppm.unlink()


if __name__ == '__main__':
    main()
