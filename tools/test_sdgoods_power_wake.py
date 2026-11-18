#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise native standby double taps without a device or SDK dependency."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / 'sdgoods-power-wake'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-Isrc',
                        '-Ifirmware/sdgoods/main', 'tools/sdgoods_power_wake_host.cpp',
                        '-Ifirmware/common',
                        '-o', str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], cwd=ROOT, check=True)

if __name__ == '__main__':
    main()
