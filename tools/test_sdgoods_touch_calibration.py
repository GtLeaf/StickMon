#!/usr/bin/env python3
"""Validate round-screen calibration, storage and the actual sensor event mapper."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory() as directory:
        for source in ('sdgoods_touch_calibration_host', 'sdgoods_right_touch_host'):
            binary = Path(directory) / source
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-Isrc', '-Ifirmware/sdgoods/main',
                        '-DSTICKMON_UI_ROUND_TOUCH=1', '-DSTICKMON_UI_COMPACT_TOUCH=1',
                        '-DSTICKMON_TOUCH_INPUT=1',
                        f'tools/{source}.cpp', '-o', str(binary)],
                       cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
