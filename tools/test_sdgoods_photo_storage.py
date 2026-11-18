#!/usr/bin/env python3
"""Run the real SDGOODS file adapter against controlled ESP mount boundaries."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    """Compile actual storage method bodies without the unrelated hardware SDK."""
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def nvs_test(out):
    source = (ROOT / 'firmware/sdgoods/main/SdgoodsPlatform.cpp').read_text()
    (out / 'sdgoods-photo-nvs-open.inc').write_text(function(source, 'esp_err_t openStore('))
    signatures = ['size_t SdgoodsPlatform::blobSize(', 'bool SdgoodsPlatform::readBlob(',
                  'bool SdgoodsPlatform::writeBlob(', 'bool SdgoodsPlatform::removeBlob(',
                  'bool SdgoodsPlatform::clearNamespace(']
    declaration = ''
    if 'Platform::BlobLookup SdgoodsPlatform::lookupBlob(' in source:
        signatures.append('Platform::BlobLookup SdgoodsPlatform::lookupBlob(')
        declaration = 'Platform::BlobLookup lookupBlob(const char*,const char*,size_t&)override;'
    (out / 'sdgoods-photo-nvs-declaration.inc').write_text(declaration)
    (out / 'sdgoods-photo-nvs-methods.inc').write_text('\n'.join(function(source, s) for s in signatures))
    binary = out / 'sdgoods-photo-nvs'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Isrc',
                    '-Itools/sdgoods_photo_storage_stubs', '-I' + str(out),
                    'tools/sdgoods_photo_storage_nvs_host.cpp', '-o', str(binary)], cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    binary = out / 'sdgoods-photo-storage'
    sources = ['tools/sdgoods_photo_storage_host.cpp', 'src/core/PhotoLibrary.cpp',
               'src/platform/api/PlatformServices.cpp']
    adapter = ROOT / 'firmware/sdgoods/main/PhotoFiles.cpp'
    if adapter.exists():
        sources.append(str(adapter))
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-Isrc', '-Ifirmware/sdgoods/main', '-Itools/sdgoods_photo_storage_stubs',
                    *sources, '-Wl,--wrap=mkdir,--wrap=stat,--wrap=fopen,--wrap=rename,--wrap=unlink,--wrap=fsync',
                    '-o', str(binary)], cwd=ROOT, check=True)
    for scenario in ['blank', 'existing', 'corrupt', 'readfail', 'tail', 'missing',
                     'formatfail', 'mountfail', 'infofail', 'badinfo', 'full']:
        with tempfile.TemporaryDirectory(prefix=scenario + '-', dir=out) as directory:
            subprocess.run([str(binary), scenario, directory], cwd=ROOT, check=True)
    nvs_test(out)


if __name__ == '__main__':
    main()
