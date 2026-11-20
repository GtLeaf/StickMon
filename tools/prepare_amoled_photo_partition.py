#!/usr/bin/env python3
"""Prepare a V2 partition table that preserves the existing Meets save area."""

import argparse
import hashlib
from pathlib import Path
import struct
import zlib

from sdgoods_release import partitions

ROOT = Path(__file__).resolve().parents[1]
FLASH_SIZE = 0x1000000
BRAIN_BASE = 0xA30000
MEETS_BASE = 0xB00000
BRAIN_SIZE = FLASH_SIZE - BRAIN_BASE
PHOTO_SIZE = MEETS_BASE - BRAIN_BASE
SLOT_SIZE = (FLASH_SIZE - MEETS_BASE) // 2
HEADER_SIZE = 160
BLOCK_SIZE = 0x10000
TABLE_SIZE = 0xC00


def stock_layout():
    expected = {}
    path = ROOT / "firmware/amoled_1_8_v2/partitions.csv"
    for line in path.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        name, kind, subtype, offset, size, flags = (v.strip() for v in line.split(","))
        types = {"data": 1, "app": 0}
        subtypes = {"nvs": 2, "ota": 0, "phy": 1, "ota_0": 0x10, "ota_1": 0x11,
                    "spiffs": 0x82, "coredump": 3, "fat": 0x81}
        if flags:
            raise ValueError("Unexpected flags in stock V2 table")
        expected[name] = (types[kind], subtypes[subtype], int(offset, 0), int(size, 0))
    return expected


def committed_meets_slot(slot):
    if slot[:12] != b"TMCKP001\x01\x00\x00\x00" or slot[-4:] != b"CMIT":
        return False
    sequence, count, payload_crc = struct.unpack_from("<III", slot, 12)
    if not sequence or not 0 <= count <= 32:
        return False
    header_crc, = struct.unpack_from("<I", slot, 28)
    header = slot[:28] + bytes(4) + slot[32:HEADER_SIZE]
    payload = slot[HEADER_SIZE:HEADER_SIZE + count * (BLOCK_SIZE + 4)]
    return zlib.crc32(header) == header_crc and zlib.crc32(payload) == payload_crc


def prepare(old_table, brainfs):
    current = partitions(old_table)
    if current != stock_layout() or current["brainfs"] != (1, 0x81, BRAIN_BASE, BRAIN_SIZE):
        raise ValueError("Expected the stock 16 MiB AMOLED V2 partition table")
    if len(brainfs) != BRAIN_SIZE:
        raise ValueError("Expected a complete read of brainfs")
    if brainfs[:PHOTO_SIZE] != b"\xff" * PHOTO_SIZE:
        raise ValueError("The proposed photo region is not entirely erased")
    slots = [i + 1 for i in range(2) if committed_meets_slot(
        brainfs[PHOTO_SIZE + i * SLOT_SIZE:PHOTO_SIZE + (i + 1) * SLOT_SIZE])]
    if not slots:
        raise ValueError("No committed Meets checkpoint passed CRC validation")

    # Copy every stock entry verbatim except for the brainfs length, then
    # reserve the Meets range using the same type and subtype as its firmware.
    records = bytearray(old_table[:len(current) * 32])
    index = list(current).index("brainfs")
    struct.pack_into("<I", records, index * 32 + 8, PHOTO_SIZE)
    records.extend(struct.pack("<HBBII16sI", 0x50AA, 1, 0x41, MEETS_BASE,
                               FLASH_SIZE - MEETS_BASE, b"tama_save", 0))
    digest = hashlib.md5(records).digest()
    records.extend(b"\xeb\xeb" + b"\xff" * 14 + digest)
    records.extend(b"\xff" * (TABLE_SIZE - len(records)))
    return bytes(records), slots


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("partition_table", type=Path, help="Read from flash at 0x8000")
    parser.add_argument("brainfs", type=Path, help="Read 0x5d0000 bytes from flash at 0xa30000")
    parser.add_argument("output", type=Path, help="Output partition table image")
    args = parser.parse_args()
    try:
        table, slots = prepare(args.partition_table.read_bytes(), args.brainfs.read_bytes())
        with args.output.open("xb") as output:
            output.write(table)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"brainfs: 0x{BRAIN_BASE:x} to 0x{MEETS_BASE:x} ({PHOTO_SIZE // 1024} KiB)")
    print(f"tama_save: 0x{MEETS_BASE:x} to 0x{FLASH_SIZE:x}")
    print(f"Committed Meets slots with matching CRCs: {', '.join(map(str, slots))}")
    print(f"Partition table SHA-256: {hashlib.sha256(table).hexdigest()}")


if __name__ == "__main__":
    main()
