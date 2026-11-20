#!/usr/bin/env python3
"""Verify the V2 photo partition repair without writing to a device."""

import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

from prepare_amoled_photo_partition import (
    BRAIN_BASE, BRAIN_SIZE, FLASH_SIZE, HEADER_SIZE, MEETS_BASE, PHOTO_SIZE,
    SLOT_SIZE, prepare, stock_layout,
)
from sdgoods_release import idf_paths, partitions


def factory_table():
    records = b"".join(struct.pack("<HBBII16sI", 0x50AA, *row, name.encode(), 0)
                       for name, row in stock_layout().items())
    return records + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(records).digest()


def flash_with_meets_saves():
    image = bytearray(b"\xff" * BRAIN_SIZE)
    payload = struct.pack("<I", 2) + b"x" * 0x10000
    header = bytearray(HEADER_SIZE)
    struct.pack_into("<8sIIIII", header, 0, b"TMCKP001", 1, 4, 1, zlib.crc32(payload), 0)
    struct.pack_into("<I", header, 28, zlib.crc32(header))
    for slot in range(2):
        start = PHOTO_SIZE + slot * SLOT_SIZE
        end = start + SLOT_SIZE
        image[start:start + HEADER_SIZE + len(payload)] = header + payload
        image[end - 4:end] = b"CMIT"
    # The original, larger FAT volume may already have written WL metadata.
    image[-4096:] = bytes(4096)
    return image


class PartitionRepairTests(unittest.TestCase):
    def setUp(self):
        self.table = factory_table()
        self.flash = flash_with_meets_saves()

    def test_repair_reserves_both_meets_slots_and_keeps_stock_partitions(self):
        before = bytes(self.flash)
        repaired, slots = prepare(self.table, self.flash)
        self.assertEqual(slots, [1])
        self.assertEqual(self.flash, before)
        expected = stock_layout()
        expected["brainfs"] = (1, 0x81, BRAIN_BASE, PHOTO_SIZE)
        expected["tama_save"] = (1, 0x41, MEETS_BASE, FLASH_SIZE - MEETS_BASE)
        self.assertEqual(partitions(repaired), expected)
        self.assertGreater(PHOTO_SIZE, 2 * 5 * (44 + (192**2 + 48**2) * 2))

    def test_first_or_last_dirty_sector_of_photo_area_is_rejected(self):
        for address in (0, PHOTO_SIZE - 1):
            with self.subTest(address=address):
                self.flash[address] = 0
                with self.assertRaisesRegex(ValueError, "not entirely erased"):
                    prepare(self.table, self.flash)
                self.flash[address] = 255

    def test_checkpoint_with_bad_payload_crc_is_rejected(self):
        self.flash[PHOTO_SIZE + HEADER_SIZE + 4] ^= 1
        with self.assertRaisesRegex(ValueError, "CRC"):
            prepare(self.table, self.flash)

    def test_incomplete_read_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "complete read"):
            prepare(self.table, self.flash[:-1])

    def test_rejects_corrupt_or_already_migrated_table(self):
        corrupt = bytearray(self.table)
        corrupt[4] ^= 1
        with self.assertRaisesRegex(ValueError, "MD5"):
            prepare(corrupt, self.flash)
        repaired, _ = prepare(self.table, self.flash)
        with self.assertRaisesRegex(ValueError, "stock"):
            prepare(repaired, self.flash)


def verify_photo_workflow():
    idf, idf_python = idf_paths()
    fat_tools = idf / "components/fatfs"
    with tempfile.TemporaryDirectory(prefix="stickmon-photo-fat-") as directory:
        work = Path(directory)
        subprocess.run([sys.executable, str(Path(__file__).with_name("test_photo_album.py")),
                        "--output", str(work / "app")], check=True)
        album = work / "app/run/photos"
        originals = {path.name: path.read_bytes() for path in album.glob("slot*.photo")}
        if len(originals) != 5:
            raise AssertionError("Expected five real photos from the AMOLED workflow")

        source = work / "source/photos"
        source.mkdir(parents=True)
        for name, data in originals.items():
            (source / name).write_bytes(data)
        image = work / "photos.bin"
        subprocess.run([str(idf_python), str(fat_tools / "wl_fatfsgen.py"),
                        str(source.parent), "--output_file", str(image),
                        "--partition_size", str(PHOTO_SIZE), "--sector_size", "4096",
                        "--fat_count", "2", "--long_name_support"], check=True, cwd=work)
        subprocess.run([str(idf_python), str(fat_tools / "fatfsparse.py"), str(image),
                        "--wl-layer", "enabled"], check=True, cwd=work)
        recovered = {path.name.lower(): path.read_bytes()
                     for path in (work / "Espressif/PHOTOS").glob("*.photo")}
        if image.stat().st_size != PHOTO_SIZE or recovered != originals:
            raise AssertionError("The 832 KiB FAT image did not preserve all five photos")
        print("832 KiB FAT image: all five AMOLED photos read back byte for byte")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--workflow", action="store_true",
                        help="Also run the native AMOLED app and validate its photos in a FAT image")
    args, unittest_args = parser.parse_known_args()
    result = unittest.main(argv=[sys.argv[0], *unittest_args], exit=False).result
    if not result.wasSuccessful():
        sys.exit(1)
    if args.workflow:
        verify_photo_workflow()
