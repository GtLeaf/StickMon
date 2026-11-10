#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""SDGOODS ROM installer worker. Uses the ESP-IDF environment's esptool 4 API."""
import argparse
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import re
import time
from types import SimpleNamespace

import sdgoods_release as release


def image_bytes(folder, name):
    data = (folder / name).read_bytes()
    # Match esptool's unencrypted write alignment; keep signed image headers.
    return data + b'\xff' * (-len(data) % 4)


def install_connected(folder, esp, write_images):
    """Skip only actual full-digest matches; require every write's MD5 check."""
    plan = release.write_plan(folder)
    images = {name: image_bytes(folder, name) for _, name in plan}
    release.validate_write_plan(folder, plan)
    release.validate_install_table(esp.read_flash(0x8000, 0x1000))
    changed, skipped = [], []
    started = time.monotonic()
    for at, name in plan:
        data = images[name]
        print(f'[install] Checking {name} @ {at:#010x} ({len(data)} bytes)', flush=True)
        prefix = data[:4096]
        actual_prefix = esp.read_flash(at, len(prefix))
        if len(actual_prefix) != len(prefix):
            raise ValueError(f'short flash read at {at:#x}')
        # A different prefix already proves a write is needed. Identical
        # prefixes are never enough to skip: compare the entire region digest.
        same = False
        if actual_prefix == prefix:
            digest = esp.flash_md5sum(at, len(data))
            if not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{32}', digest):
                raise ValueError(f'invalid device digest at {at:#x}')
            same = digest == hashlib.md5(data).hexdigest()
        if same:
            skipped.append((at, name))
            print(f'[install] Unchanged, skipping {name} @ {at:#010x} (full digest matched)', flush=True)
        else:
            changed.append((at, name))
            print(f'[install] Changed, will write and verify {name} @ {at:#010x}', flush=True)
    expected = {(at, len(images[name])): hashlib.md5(images[name]).hexdigest() for at, name in changed}
    verified = set()
    original_md5 = esp.flash_md5sum

    def checked_md5(at, size):
        digest = original_md5(at, size)
        key = (at, size)
        if key not in expected:
            raise ValueError(f'unexpected write verification range {at:#x}/{size}')
        if digest != expected[key]:
            raise ValueError(f'write digest mismatch at {at:#x}')
        verified.add(key)
        return digest

    if changed:
        # Observe the SDK's actual post-write flash reads, not success strings.
        # Missing checks, unsupported MD5, transport errors and mismatches fail.
        esp.flash_md5sum = checked_md5
        try:
            write_images(changed)
        finally:
            esp.flash_md5sum = original_md5
        if verified != set(expected):
            raise ValueError('esptool did not verify every written region')
    summary = {'checked_regions': len(plan), 'skipped_regions': len(skipped),
               'written_regions': len(changed), 'write_verified_regions': len(verified),
               'written_bytes': sum(len(images[name]) for _, name in changed),
               'skipped_bytes': sum(len(images[name]) for _, name in skipped),
               'elapsed_seconds': round(time.monotonic() - started, 2)}
    print('[install] Summary: ' + json.dumps(summary), flush=True)
    return summary


def install_device(folder, port, mac, public_key=release.PUBLIC_KEY):
    import esptool
    from esptool.cmds import detect_flash_size
    if not esptool.__version__.startswith('4.'):
        raise ValueError('This installer requires the ESP-IDF esptool 4 environment')
    release.validate_bundle(folder, public_key)
    release.validate_write_plan(folder, release.write_plan(folder))
    esp = esptool.detect_chip(port)
    try:
        if esp.CHIP_NAME != 'ESP32-S3':
            raise ValueError('device is not ESP32-S3')
        actual_mac = ':'.join(f'{b:02x}' for b in esp.read_mac('BASE_MAC'))
        if actual_mac != mac.lower():
            raise ValueError(f'device MAC mismatch: expected {mac.lower()}, got {actual_mac}')
        if esp.secure_download_mode or esp.get_flash_encryption_enabled() or esp.get_secure_boot_enabled():
            raise ValueError('secured devices require a separate installation workflow')
        esp = esp.run_stub()
        # The stub attaches SPI flash; ROM detection alone can return no ID.
        flash_size = detect_flash_size(esp)
        if flash_size != '32MB':
            raise ValueError(f'device flash size mismatch: expected 32MB, got {flash_size}')
        print(f'[install] Device verified: {actual_mac}, ESP32-S3, 32 MiB', flush=True)
        esp.change_baud(921600)
        esp.flash_set_parameters(32 * 1024 * 1024)

        def write_images(plan):
            # Keep the signed bundle's exact bootloader bytes. The SDK still
            # performs its security, capacity, erase-range and image checks.
            # Call the write operation directly to reuse this connected stub
            # and retain the observed post-write digest method throughout.
            with ExitStack() as files:
                args = SimpleNamespace(chip='esp32s3', flash_mode='keep', flash_size='keep', flash_freq='keep',
                    compress=True, no_compress=False, no_stub=False, encrypt=False, encrypt_files=None,
                    erase_all=False, force=False, ignore_flash_encryption_efuse_setting=False,
                    addr_filename=[(at, files.enter_context((folder / name).open('rb'))) for at, name in plan])
                esptool.write_flash(esp, args)

        summary = install_connected(folder, esp, write_images)
        esp.hard_reset()
        print('Installed and verified shared resources and OTA images. NVS/PHY/userdata were not written.', flush=True)
        return summary
    finally:
        esp._port.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--mac', required=True)
    parser.add_argument('--public-key', type=Path, default=release.PUBLIC_KEY)
    args = parser.parse_args()
    install_device(args.bundle, args.port, args.mac, args.public_key)


if __name__ == '__main__':
    main()
