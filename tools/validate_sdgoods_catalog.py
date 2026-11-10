#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Validate an extracted SDGOODS catalog with the real firmware startup code."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('tools/sdgoods_catalog_host.cpp', 'src/core/ContentValidation.cpp',
           'src/game/Species.cpp', 'src/assets/PokemonSprites.cpp',
           'src/game/ExploreEcology.cpp', 'src/game/ExploreEcologyFlight.cpp',
           'src/core/ResourceFS.cpp', 'src/core/ResourcePack.cpp', 'src/core/DeflateDecoder.cpp',
           'src/platform/desktop/DesktopPlatform.cpp', 'src/platform/api/PlatformServices.cpp')


def validate(root):
    fingerprint = hashlib.sha256()
    inputs = [ROOT / name for name in SOURCES]
    inputs.append(Path(__file__).resolve())
    inputs += list((ROOT / 'src').rglob('*.h'))
    inputs += [ROOT / 'src/third_party/uzlib/tinflate.c']
    compiler = os.environ.get('STICKMON_HOST_CXX', '/usr/bin/c++')
    fingerprint.update(subprocess.check_output([compiler, '--version']))
    for path in sorted(inputs):
        fingerprint.update(str(path.relative_to(ROOT)).encode())
        fingerprint.update(path.read_bytes())
    scratch = ROOT / '.stick-local/scratch/sdgoods-catalog-validator' / fingerprint.hexdigest()[:20]
    scratch.mkdir(parents=True, exist_ok=True)
    binary = scratch / 'validate'
    if not binary.exists():
        with tempfile.TemporaryDirectory(dir=scratch) as temporary:
            obj = Path(temporary) / 'tinflate.o'
            output = Path(temporary) / 'validate'
            subprocess.run(['cc', '-O1', '-ffunction-sections', '-fdata-sections', '-c',
                            str(ROOT / 'src/third_party/uzlib/tinflate.c'), '-o', str(obj)], check=True)
            subprocess.run([compiler, '-std=c++17', '-O1', '-ffunction-sections', '-fdata-sections',
                            '-DSTICKMON_EXTERNAL_CONTENT=1', '-DSTICKMON_ENABLE_DEBUG_FEATURES=0',
                            '-I' + str(ROOT / 'src'), *[str(ROOT / name) for name in SOURCES], str(obj),
                            '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
                            '-o', str(output)], check=True)
            os.replace(output, binary)
    subprocess.run([str(binary), str(Path(root).resolve())], check=True, timeout=30)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    validate(parser.parse_args().root)
