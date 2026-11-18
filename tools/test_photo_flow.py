#!/usr/bin/env python3
"""Exercise shared round photo flow with real files and sanitizer checks."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
sources = ['tools/photo_flow_host.cpp', 'src/presentation/PhotoFlow.cpp',
           'src/core/PhotoLibrary.cpp', 'src/presentation/Canvas565.cpp',
           'src/platform/api/PlatformServices.cpp', 'src/platform/desktop/DesktopPlatform.cpp']
binary = out / 'photo-flow'
subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Isrc',
                *sources, '-o', str(binary)], cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix='files-', dir=out) as files:
    subprocess.run([str(binary), files], cwd=ROOT, check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0',
                        'UBSAN_OPTIONS': 'halt_on_error=1'})
