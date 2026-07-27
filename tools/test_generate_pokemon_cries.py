#!/usr/bin/env python3

import math
import struct
import tempfile
import unittest
import wave
import zlib
from pathlib import Path

import generate_pokemon_cries as generator


ROOT = Path(__file__).resolve().parents[1]


def write_test_wav(path, seconds=0.1):
    frame_count = int(generator.SOURCE_SAMPLE_RATE * seconds)
    payload = bytearray()
    for frame in range(frame_count):
        value = int(math.sin(frame * 2.0 * math.pi * 440.0 /
                             generator.SOURCE_SAMPLE_RATE) * 12000)
        payload.extend(struct.pack("<hh", value, value // 2))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(generator.SOURCE_SAMPLE_RATE)
        output.writeframes(payload)


class GeneratePokemonCriesTests(unittest.TestCase):
    def test_encode_source_downsamples_and_compresses_pcm(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "001_test.wav"
            write_test_wav(source)

            pcm, compressed, gain = generator.encode_source(source)

            self.assertEqual(2205, len(pcm))
            self.assertEqual(pcm, zlib.decompress(compressed, wbits=-15))
            self.assertGreater(gain, 0.0)
            self.assertNotEqual({128}, set(pcm))

    def test_pack_header_identifies_species_and_validates_crc(self):
        pcm = bytes(range(256)) * 4
        compressor = zlib.compressobj(level=6, wbits=-15)
        compressed = compressor.compress(pcm) + compressor.flush()

        packed = generator.make_pack(25, pcm, compressed)
        header = struct.unpack(
            generator.CRY_HEADER_FORMAT,
            packed[:generator.CRY_HEADER_SIZE],
        )

        self.assertEqual(generator.CRY_PACK_MAGIC, header[0])
        self.assertEqual(generator.CRY_PACK_VERSION, header[1])
        self.assertEqual(25, header[2])
        self.assertEqual(generator.TARGET_SAMPLE_RATE, header[3])
        self.assertEqual(len(pcm), header[4])
        self.assertEqual(len(pcm), header[5])
        self.assertEqual(len(compressed), header[6])
        self.assertEqual(zlib.crc32(pcm) & 0xFFFFFFFF, header[7])
        self.assertEqual(generator.CRY_PACK_FLAGS, header[8])
        self.assertEqual(pcm, zlib.decompress(packed[generator.CRY_HEADER_SIZE:], -15))

    def test_generated_pack_matches_current_species(self):
        expected = generator.current_species_ids()
        generated = {
            int(path.stem)
            for path in (ROOT / "data" / "packs" / "dev" / "cries").glob("*.smoncry")
        }
        self.assertEqual(expected, generated)

        for species_id in sorted(expected):
            payload = (ROOT / "data" / "packs" / "dev" / "cries" /
                       f"{species_id:03d}.smoncry").read_bytes()
            header = struct.unpack(
                generator.CRY_HEADER_FORMAT,
                payload[:generator.CRY_HEADER_SIZE],
            )
            pcm = zlib.decompress(payload[generator.CRY_HEADER_SIZE:], -15)
            self.assertEqual(species_id, header[2])
            self.assertEqual(len(payload) - generator.CRY_HEADER_SIZE, header[6])
            self.assertEqual(header[5], len(pcm))
            self.assertEqual(header[7], zlib.crc32(pcm) & 0xFFFFFFFF)


if __name__ == "__main__":
    unittest.main()
