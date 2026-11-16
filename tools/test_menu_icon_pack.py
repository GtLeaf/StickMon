#!/usr/bin/env python3
"""Native icon preload validation and complete embedded/external pixel parity."""
import json
import shutil
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
HEADER = struct.Struct('<4sHHHHIIII')


def embedded_payload():
    source = (ROOT / 'src/assets/MenuAssets.cpp').read_text()
    frames = source.split('const RleFrame MAIN_ICON_FRAMES[] STICKMON_FLASH_DATA = {', 1)[1].split('};', 1)[0]
    words = source.split('const uint16_t MAIN_ICON_RLE[] STICKMON_FLASH_DATA = {', 1)[1].split('};', 1)[0]
    records = [(int(a), int(b)) for a, b in re.findall(r'\{\s*(\d+),\s*(\d+)\s*\}', frames)]
    values = [int(word, 16) for word in re.findall(r'0x[0-9A-Fa-f]+', words)]
    return b''.join(struct.pack('<HH', *record) for record in records) + struct.pack('<' + 'H'*len(values), *values)


def pack(payload):
    compressor = zlib.compressobj(9, wbits=-15)
    encoded = compressor.compress(payload) + compressor.flush()
    return HEADER.pack(b'SMMN', 1, 9, 40, 40, len(payload), len(encoded), zlib.crc32(payload), 0) + encoded


class MenuIconPackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.work = Path(cls.temporary.name)
        cls.payload = embedded_payload()
        cls.expected = cls.work / 'expected.bin'
        cls.expected.write_bytes(cls.payload)
        cls.binaries = {}

    def test_encoder_preserves_every_rle_word_and_is_reproducible(self):
        output = self.work / 'generated.smonmenu'
        result = subprocess.run([sys.executable, 'tools/generate_menu_icon_pack.py', '--output', str(output)], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        blob = output.read_bytes()
        self.assertEqual(blob, pack(self.payload))
        self.assertEqual(blob, (ROOT / 'data/packs/dev/ui/menu.smonmenu').read_bytes())
        self.assertLess(len(blob), 6000)

    def test_source_generator_refreshes_external_pack_and_preserves_guards(self):
        with tempfile.TemporaryDirectory(dir=self.work) as directory:
            root = Path(directory)
            cpp = root / 'MenuAssets.cpp'
            header = root / 'MenuAssets.h'
            shutil.copyfile(ROOT / 'src/assets/MenuAssets.cpp', cpp)
            shutil.copyfile(ROOT / 'src/assets/MenuAssets.h', header)
            command = [sys.executable, 'tools/prepare_menu_assets.py',
                '--cpp', str(cpp), '--header', str(header), '--sheet', str(root/'sheet.png'),
                '--preview', str(root/'preview.png'), '--pack', str(root/'menu.smonmenu')]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            first = cpp.read_bytes()
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(cpp.read_bytes(), first)
            for external in [0, 1]:
                result = subprocess.run(['c++', '-std=c++17', '-Isrc',
                    '-DSTICKMON_EXTERNAL_MENU_ICONS='+str(external), '-fsyntax-only', str(cpp)],
                    cwd=ROOT, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((root/'menu.smonmenu').read_bytes().startswith(b'SMMN'))

    def binary(self, external):
        if external not in self.binaries:
            obj = self.work / 'tinflate.o'
            flags = ['-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-ffunction-sections', '-fdata-sections']
            result = subprocess.run(['cc', *flags, '-c', 'src/third_party/uzlib/tinflate.c', '-o', str(obj)], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            binary = self.work / ('menu-host-' + str(external))
            result = subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', *flags, '-Wl,--gc-sections', '-Isrc', '-DSTICKMON_EXTERNAL_MENU_ICONS='+str(external), 'tools/menu_icon_pack_host.cpp', 'src/assets/MenuAssets.cpp', 'src/core/ResourcePack.cpp', 'src/core/ResourceFS.cpp', 'src/core/DeflateDecoder.cpp', 'src/platform/api/PlatformServices.cpp', 'src/platform/desktop/DesktopPlatform.cpp', str(obj), '-o', str(binary)], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.binaries[external] = binary
        return self.binaries[external]

    def run_pack(self, blob, wanted=True, external=1, fail_memory=False, active=False):
        binary = self.binary(external)
        with tempfile.TemporaryDirectory(dir=self.work) as directory:
            root = Path(directory)
            relative = 'packs/custom' if active else 'packs/dev'
            directory = root / relative / 'ui'
            directory.mkdir(parents=True)
            (directory.parent / 'manifest.json').write_text(json.dumps({'schema': 1, 'format': 'smon-resource-pack-v1'}))
            if active:
                (root / 'active.json').write_text(json.dumps({'packPath': '/packs/custom'}))
            if blob is not None:
                (directory / 'menu.smonmenu').write_bytes(blob)
            environment = os.environ.copy()
            environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
            environment['UBSAN_OPTIONS'] = 'halt_on_error=1'
            result = subprocess.run([str(binary), str(root), str(self.expected), str(int(wanted)), str(int(fail_memory))], capture_output=True, text=True, env=environment)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_external_preload_matches_all_nine_icons_without_drawing_io(self):
        self.run_pack(pack(self.payload))

    def test_embedded_fallback_needs_no_filesystem_or_allocation(self):
        self.run_pack(None, external=0)

    def test_active_pack_root_is_used(self):
        self.run_pack(pack(self.payload), active=True)

    def test_missing_resource_and_allocation_failure_are_not_exposed(self):
        self.run_pack(None, wanted=False)
        self.run_pack(pack(self.payload), wanted=False, fail_memory=True)

    def test_rejects_invalid_header_stream_crc_and_rle(self):
        good = pack(self.payload)
        bad = [b'', good[:15], good[:-1], good+b'X']
        for offset, fmt, value in [(0,'I',0), (4,'H',2), (6,'H',8), (8,'H',41), (10,'H',0), (12,'I',0xffffffff), (12,'I',2), (16,'I',0xffffffff), (20,'I',0), (24,'I',1)]:
            blob = bytearray(good)
            struct.pack_into('<'+fmt, blob, offset, value)
            bad.append(bytes(blob))
        # Size fields match each malformed stream: strict decoder must reject.
        for encoded in [good[HEADER.size:-1], good[HEADER.size:]+b'extra']:
            header = bytearray(good[:HEADER.size])
            struct.pack_into('<I', header, 16, len(encoded))
            bad.append(bytes(header)+encoded)
        c = zlib.compressobj(9, wbits=-15)
        encoded = c.compress(self.payload+b'X')+c.flush()
        header = bytearray(good[:HEADER.size])
        struct.pack_into('<I', header, 16, len(encoded))
        bad.append(bytes(header)+encoded)
        # Valid CRC, invalid contiguous frames and malformed RLE controls.
        for offset, value in [(0, 1), (2, 0), (2, 65535), (4, 0), (36, 0), (36, 0x8000), (36, 0xFFFF), (36, 1601)]:
            payload = bytearray(self.payload)
            struct.pack_into('<H', payload, offset, value)
            bad.append(pack(payload))
        for index, blob in enumerate(bad):
            with self.subTest(case=index):
                self.run_pack(blob, wanted=False)


if __name__ == '__main__':
    unittest.main()
