#!/usr/bin/env python3
"""Native bag and ordinary-shop description layout without palette changes."""
from pathlib import Path
import argparse
import subprocess
import tempfile
from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='item-detail-text-') as directory:
        binary = build_amoled_host(
            Path(directory) / 'render',
            [ROOT / 'tools/item_detail_text_host.cpp',
             ROOT / 'tools/amoled_checksum_guard.cpp'],
        )
        command = [str(binary), str(ROOT / 'data')]
        if args.output:
            args.output.mkdir(parents=True, exist_ok=True)
            command.append(str(args.output.resolve()))
        subprocess.run(command, cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
