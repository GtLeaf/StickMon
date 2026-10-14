#!/usr/bin/env python3
"""Check native resource packaging against real packs, including stale ids."""

import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from validate_amoled_ui_assets import FRAME, HEADER, ROOT, validate

SHARED = ROOT / "data/packs/dev/game/ui.smonfx"
NATIVE = ROOT / "data/packs/dev/game/ui_amoled.smonfx"


class AmoledUiAssetTests(unittest.TestCase):
    def changed_pack(self, directory, change):
        content = NATIVE.read_bytes()
        header = list(HEADER.unpack_from(content))
        payload = bytearray(zlib.decompress(content[HEADER.size:], -15))
        change(header, payload)
        compressed = zlib.compress(payload)[2:-4]
        header[6] = len(payload)
        header[7] = len(compressed)
        header[8] = zlib.crc32(payload)
        path = Path(directory) / "ui.smonfx"
        path.write_bytes(HEADER.pack(*header) + compressed)
        return path

    def test_native_pack_matches_firmware_ids_and_item_sizes(self):
        validate(SHARED, NATIVE)

    def test_old_non_item_ids_are_rejected_even_with_valid_checksum(self):
        def stale(header, payload):
            # Battle FX inserted 16 ids ahead of the late UI entries.
            for index in range(header[2]):
                offset = index * FRAME.size
                kind = struct.unpack_from("<H", payload, offset)[0]
                if kind > 100:
                    struct.pack_into("<H", payload, offset, kind - 16)
        with tempfile.TemporaryDirectory() as directory:
            path = self.changed_pack(directory, stale)
            with self.assertRaisesRegex(ValueError, "outdated UI asset ids"):
                validate(SHARED, path)

    def test_stick_item_size_is_rejected_in_native_pack(self):
        def small(_header, payload):
            struct.pack_into("<HH", payload, 2, 36, 36)
        with tempfile.TemporaryDirectory() as directory:
            path = self.changed_pack(directory, small)
            with self.assertRaisesRegex(ValueError, "must be 48x48"):
                validate(SHARED, path)

    def test_checksum_corruption_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            content = bytearray(NATIVE.read_bytes())
            content[HEADER.size - 1] ^= 1
            path = Path(directory) / "ui.smonfx"
            path.write_bytes(content)
            with self.assertRaisesRegex(ValueError, "checksum"):
                validate(SHARED, path)


if __name__ == "__main__":
    unittest.main()
