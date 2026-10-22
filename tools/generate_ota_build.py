#!/usr/bin/env python3
"""Generate V2 OTA identity from the exact staged SPIFFS file contents."""
import hashlib
import json
import re
import sys
from pathlib import Path


def resource_identity(stage):
    entries = []
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            entries.append((path.relative_to(stage).as_posix(),
                            hashlib.sha256(path.read_bytes()).hexdigest()))
    serialized = ''.join(f'{path}\t{digest}\n' for path, digest in entries)
    return hashlib.sha256(serialized.encode()).hexdigest(), entries


def generate(stage, output, config, flavor, endpoint, public_key, debug=False):
    version = json.loads(config.read_text())
    if not isinstance(version['build'], int) or not 0 < version['build'] < 2**32:
        raise ValueError('build must be a monotonic uint32')
    if flavor not in ('lite', 'claw') or len(version['version']) > 31:
        raise ValueError('invalid flavor/version')
    if endpoint and (not endpoint.startswith('https://') or len(endpoint) > 200):
        raise ValueError('OTA endpoint must use HTTPS')
    resource_id, entries = resource_identity(stage)
    source_root = Path(__file__).resolve().parents[1]
    save_schema = int(re.search(r'SCHEMA_VERSION\s*=\s*(\d+)',
        (source_root / 'src/core/SaveCodec.h').read_text()).group(1))
    save_version = int(re.search(r'SAVE_VERSION\s*=\s*(\d+)',
        (source_root / 'src/game/GameState.h').read_text()).group(1))
    min_save_schema = int(re.search(r'MIN_SUPPORTED_SCHEMA_VERSION\s*=\s*(\d+)',
        (source_root / 'src/core/SaveCodec.h').read_text()).group(1))
    min_save_version = int(re.search(r'MIN_SUPPORTED_SAVE_VERSION\s*=\s*(\d+)',
        (source_root / 'src/game/GameState.h').read_text()).group(1))
    identity = dict(schema=1, target='waveshare-amoled-1.8-v2', flavor=flavor,
                    version=version['version'], build=version['build'],
                    layout='waveshare-v2-16m-v1', resourceSha256=resource_id,
                    saveSchema=save_schema, saveVersion=save_version,
                    minSaveSchema=min_save_schema, minSaveVersion=min_save_version,
                    debug=debug)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'ota-build.json').write_text(json.dumps(identity, indent=2) + '\n')
    header = '#pragma once\nnamespace OtaBuild {\n'
    for key, value in identity.items():
        if isinstance(value, str):
            header += f'inline constexpr char {key}[] = {json.dumps(value)};\n'
        elif isinstance(value, bool):
            header += f'inline constexpr bool {key} = {str(value).lower()};\n'
        else:
            header += f'inline constexpr unsigned {key} = {value}U;\n'
    header += f'inline constexpr char endpoint[] = {json.dumps(endpoint)};\n'
    header += f'inline constexpr char publicKey[] = {json.dumps(public_key.read_text())};\n'
    header += 'struct Resource { const char* path; const char* sha256; };\n'
    header += 'inline constexpr Resource resources[] = {\n'
    header += ''.join(f'{{{json.dumps(path)},"{digest}"}},\n' for path, digest in entries)
    header += '};\n}\n'
    dest = output / 'OtaBuild.h'
    if not dest.exists() or dest.read_text() != header:
        dest.write_text(header)


if __name__ == '__main__':
    generate(Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]),
             sys.argv[4], sys.argv[5], Path(sys.argv[6]), len(sys.argv) > 7 and sys.argv[7] == 'ON')
