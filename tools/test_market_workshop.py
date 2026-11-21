#!/usr/bin/env python3
"""Run the workshop gift and wallpaper purchase against the production AMOLED application."""
import argparse
from contextlib import nullcontext
import subprocess
from pathlib import Path

from amoled_host import ROOT, build_amoled_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--catalog-fixture', action='store_true',
                        help='Run the six-entry test catalog against the production app')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    resource = ROOT / 'data'
    context = nullcontext((resource, []))
    if args.catalog_fixture:
        from wallpaper_catalog_fixture import wallpaper_catalog_fixture
        context = wallpaper_catalog_fixture(resource)
    with context as (resource, flags):
        binary = build_amoled_host(out / 'amoled-workshop', [
            ROOT / 'tools/market_workshop_amoled_host.cpp',
            ROOT / 'tools/amoled_checksum_guard.cpp',
        ], debug=args.debug, extra_flags=flags)
        subprocess.run([str(binary), str(resource), str(out)], cwd=ROOT, check=True)
    from PIL import Image
    for ppm in out.glob('*.ppm'):
        Image.open(ppm).save(ppm.with_suffix('.png'))
        ppm.unlink()


if __name__ == '__main__':
    main()
