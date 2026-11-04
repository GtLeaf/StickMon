#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check native-board mute, ADC bounds and power persistence ordering."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory() as directory:
        binary = Path(directory) / 'sdgoods-hardware'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-Ifirmware/sdgoods/main',
                        '-Ifirmware/common', 'tools/sdgoods_hardware_host.cpp',
                        '-o', str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
