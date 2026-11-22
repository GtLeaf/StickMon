#!/usr/bin/env python3
"""Compare real native sprite rendering between two pack directories.

Inputs are read-only; only the explicit output directory is written. Baseline
kind enumeration uses an independent legacy v3/v4 parser. Every logical frame
gets both outer flips, all five draw APIs, 0.5..2.5x scales, dirty row/column and
rectangle clipping, both framebuffer byte orders, and Canvas assetScale 1/2.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import gzip
import hashlib
import itertools
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    'tools/pokemon_pose_parity_host.cpp', 'src/assets/PokemonSprites.cpp',
    'src/presentation/PixelRenderer.cpp', 'src/presentation/Canvas565.cpp',
    'src/core/ResourcePack.cpp', 'src/core/ResourceFS.cpp',
    'src/core/DeflateDecoder.cpp', 'src/platform/api/PlatformServices.cpp',
    'src/platform/desktop/DesktopPlatform.cpp',
)


def build(output, sanitize=False):
    binary = output / 'pokemon-pose-parity-host'
    flags = ['-O2', '-ffunction-sections', '-fdata-sections']
    if sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    inflate = output / 'tinflate.o'
    subprocess.run(['cc', *flags, '-c', 'src/third_party/uzlib/tinflate.c',
                    '-o', str(inflate)], cwd=ROOT, check=True)
    common = ['c++', '-std=c++17', '-Wall', '-Wextra', '-Isrc', *flags]
    objects = output / ('objects-' + hashlib.sha256(repr(common).encode()).hexdigest()[:12])
    objects.mkdir(exist_ok=True)

    def compile_one(source):
        target = objects / (source.replace('/', '_') + '.o')
        dependency = target.with_suffix('.d')
        dependencies = []
        if dependency.exists():
            dependencies = [ROOT / p for p in dependency.read_text().replace('\\\n', ' ').split(':', 1)[1].split()]
        if not target.exists() or not dependencies or any(
                not p.exists() or p.stat().st_mtime > target.stat().st_mtime for p in dependencies):
            subprocess.run([*common, '-MMD', '-MF', str(dependency), '-c', source,
                            '-o', str(target)], cwd=ROOT, check=True)
        return target

    with ThreadPoolExecutor(max_workers=3) as pool:
        compiled = list(pool.map(compile_one, SOURCES))
    strip_unused = '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections'
    command = ['c++', strip_unused, *(['-fsanitize=address,undefined'] if sanitize else []),
               *map(str, compiled), str(inflate), '-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    (output / 'build.json').write_text(json.dumps({'common': common, 'sources': SOURCES,
                                                'link': command}, indent=2) + '\n')
    return binary


def legacy_kinds(path):
    blob = path.read_bytes()
    magic, version, species, count, _, _, flags, size, encoded, crc = struct.unpack_from('<I6H3I', blob)
    assert magic == 0x5350534d and version in (3, 4) and flags == 1, path
    header = 30 if version == 4 else 28
    payload = zlib.decompress(blob[header:], -15)
    assert len(payload) == size and len(blob) == header + encoded and zlib.crc32(payload) == crc, path
    return species, [struct.unpack_from('<H', payload, i * 24)[0] for i in range(count)]


def stage(directory, destination, selected):
    sprites = destination / 'packs/dev/sprites'
    sprites.mkdir(parents=True, exist_ok=True)
    (sprites.parent / 'manifest.json').write_text(json.dumps({
        'schema': 1, 'format': 'smon-resource-pack-v1', 'sprites': 'sprites'}))
    for species in selected:
        source = directory / f'{species:03d}.smonsp'
        assert source.is_file(), source
        target = sprites / source.name
        if target.is_symlink():
            target.unlink()
        target.symlink_to(source.resolve())


def compare(before, after):
    with before.open() as old, after.open() as new:
        for number, (left, right) in enumerate(itertools.zip_longest(old, new), 1):
            if left != right:
                return {'line': number, 'before': left, 'after': right}
    return None


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path, help='directory containing legacy .smonsp files')
    parser.add_argument('--candidate', required=True, type=Path, help='directory containing candidate .smonsp files')
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--species', help='comma separated IDs; default: every baseline pack')
    parser.add_argument('--large', action='store_true', help='also run 384x448 full-raster profiles')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--binary', type=Path, help='reuse a previously compiled production runtime')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    baseline, candidate = args.baseline.resolve(), args.candidate.resolve()
    selected = sorted(map(int, args.species.split(','))) if args.species else sorted(int(p.stem) for p in baseline.glob('*.smonsp'))
    assert selected, 'no packs selected'
    frames = output / 'logical-frames.txt'
    rows = []
    for species in selected:
        actual, kinds = legacy_kinds(baseline / f'{species:03d}.smonsp')
        assert actual == species and len(kinds) == len(set(kinds))
        rows.extend(f'{species} {kind}\n' for kind in kinds)
    frames.write_text(''.join(rows))
    binary = args.binary.resolve() if args.binary else build(output, args.sanitize)
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'}
    mode = 'large' if args.large else 'small'
    roots = []
    for label, directory in [('before', baseline), ('after', candidate)]:
        resource_root = output / (label + '-resources')
        stage(directory, resource_root, selected)
        roots.append(resource_root)

    def run_one(item):
        label, resource_root = item
        with (output / (label + '.log')).open('w') as log:
            subprocess.run([str(binary), str(resource_root), str(frames),
                            str(output / (label + '.hashes')), mode],
                           cwd=ROOT, check=True, env=env, stdout=log, stderr=log)

    # Separate processes keep ResourcePack singletons and sprite caches isolated.
    with ThreadPoolExecutor(max_workers=2) as pool:
        list(pool.map(run_one, zip(('before', 'after'), roots)))
    failure = compare(output / 'before.hashes', output / 'after.hashes')
    result = {'passed': failure is None, 'species': selected, 'logicalFrames': len(rows),
              'rasterCases': len(rows) * (8 if args.large else 6) * 2 * (2 + 3 * 7),
              'binary': str(binary), 'binarySha256': file_sha256(binary),
              'large': args.large, 'failure': failure,
              'command': [sys.executable, *sys.argv],
              'rasterRecordFields': ['record', 'case', 'species', 'kind', 'profile',
                                     'outerFlipX', 'operation', 'scale', 'hashA', 'hashB'],
              'operations': ['normal', 'silhouette', 'scaled-coverage', 'scaled-nearest', 'tinted-scaled'],
              'scales': [0.5, 0.75, 1, 1.25, 1.5, 2, 2.5],
              'profiles': [
                  {'size': [160, 128], 'assetScale': 1, 'clip': 'full', 'byteSwapped': False},
                  {'size': [160, 128], 'assetScale': 1, 'clip': 'full', 'byteSwapped': True},
                  {'size': [160, 128], 'assetScale': 1, 'clip': 'one-row', 'byteSwapped': False},
                  {'size': [160, 128], 'assetScale': 2, 'clip': 'rectangle', 'byteSwapped': True},
                  {'size': [160, 128], 'assetScale': 2, 'clip': 'full', 'byteSwapped': False},
                  {'size': [160, 128], 'assetScale': 2, 'clip': 'one-column', 'byteSwapped': True},
                  *([
                      {'size': [384, 448], 'assetScale': 1, 'clip': 'full', 'byteSwapped': False},
                      {'size': [384, 448], 'assetScale': 2, 'clip': 'full', 'byteSwapped': True},
                  ] if args.large else [])],
              'packSha256': {
                  label: {f'{species:03d}': file_sha256(directory / f'{species:03d}.smonsp')
                          for species in selected}
                  for label, directory in [('baseline', baseline), ('candidate', candidate)]}}
    if failure and failure['before'] and failure['before'].startswith('R '):
        case = failure['before'].split()[1]
        for label, root in zip(('before', 'after'), roots):
            subprocess.run([str(binary), str(root), str(frames), str(output / (label + '-failure.txt')),
                            mode, case, str(output / (label + '-failure.rgb565'))],
                           cwd=ROOT, check=True, env=env, capture_output=True)
        left = (output / 'before-failure.rgb565').read_bytes()
        right = (output / 'after-failure.rgb565').read_bytes()
        pixels = [i // 2 for i in range(0, len(left), 2) if left[i:i+2] != right[i:i+2]]
        result['differentPixels'] = len(pixels)
        result['firstDifferentPixel'] = pixels[0] if pixels else None
        profile = int(failure['before'].split()[4])
        width = 384 if profile >= 6 else 160
        result['firstDifferentXY'] = [pixels[0] % width, pixels[0] // width] if pixels else None
    result['hashFileSha256'] = {label: file_sha256(output / (label + '.hashes'))
                               for label in ('before', 'after')}
    if failure is None:
        # Matching streams need only one compressed copy, while both original
        # SHA256 identities remain in the durable result for auditing.
        with (output / 'before.hashes').open('rb') as source, gzip.open(
                output / 'matching.hashes.gz', 'wb', compresslevel=6) as destination:
            shutil.copyfileobj(source, destination)
        result['hashEvidence'] = str(output / 'matching.hashes.gz')
        for label in ('before', 'after'):
            (output / (label + '.hashes')).unlink()
    (output / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return int(failure is not None)


if __name__ == '__main__':
    sys.exit(main())
