#!/usr/bin/env python3

import binascii
import math
import struct
import unittest

import generate_audio_assets as audio


class AudioAssetGeneratorTests(unittest.TestCase):
    def test_ima_pack_header_and_size(self):
        samples = [int(12000 * ((index % 80) / 40.0 - 1.0)) for index in range(5000)]
        pack = audio.make_audio_pack(samples, 16000, True)
        header = struct.unpack(
            audio.AUDIO_HEADER_FORMAT, pack[:audio.AUDIO_HEADER_SIZE]
        )
        self.assertEqual(audio.AUDIO_PACK_MAGIC, header[0])
        self.assertEqual(audio.AUDIO_PACK_VERSION, header[1])
        self.assertTrue(header[2] & audio.AUDIO_FLAG_LOOP)
        self.assertEqual(16000, header[3])
        self.assertEqual(len(samples), header[4])
        self.assertEqual(audio.IMA_BLOCK_BYTES, header[5])
        self.assertEqual(audio.IMA_BLOCK_SAMPLES, header[6])
        self.assertEqual(len(pack) - audio.AUDIO_HEADER_SIZE, header[9])

    def test_all_runtime_audio_ids_have_sources(self):
        self.assertEqual(4, len(audio.MUSIC_SOURCES))
        self.assertEqual(15, len(audio.SFX_SOURCES))
        self.assertEqual(
            len({**audio.MUSIC_SOURCES, **audio.SFX_SOURCES}),
            len(audio.MUSIC_SOURCES) + len(audio.SFX_SOURCES),
        )

    def test_music_normalization_reaches_target_without_clipping(self):
        samples = [24000 if index % 20 == 0 else 2000 for index in range(8000)]
        normalized = audio.normalize(samples, -11.0, soft_limit=True)
        rms = math.sqrt(sum(value * value for value in normalized) / len(normalized))
        dbfs = 20.0 * math.log10(rms / 32768.0)
        self.assertAlmostEqual(-11.0, dbfs, places=1)
        self.assertLess(max(normalized), 32767)

    def test_music_is_trimmed_to_complete_blocks(self):
        samples = list(range(audio.MUSIC_SAMPLE_RATE * 2))
        prepared, loop_start = audio.prepare_music(
            samples, audio.MUSIC_SAMPLE_RATE
        )
        self.assertEqual(0, len(prepared) % audio.IMA_BLOCK_SAMPLES)
        self.assertEqual(0, loop_start)

    def test_tagged_music_keeps_intro_and_exact_loop_range(self):
        samples = list(range(10000))
        prepared, loop_start = audio.prepare_music(
            samples, 16000, (1234, 8765)
        )
        self.assertEqual(8765, len(prepared))
        self.assertEqual(1234, loop_start)

    def test_generated_assets_have_valid_headers_and_crc(self):
        audio_ids = {*audio.MUSIC_SOURCES, *audio.SFX_SOURCES}
        generated = {
            path.stem: path for path in audio.AUDIO_OUT.glob("*.smonaudio")
        }
        self.assertEqual(audio_ids, set(generated))

        for audio_id, path in generated.items():
            packed = path.read_bytes()
            header = struct.unpack(
                audio.AUDIO_HEADER_FORMAT, packed[:audio.AUDIO_HEADER_SIZE]
            )
            payload = packed[audio.AUDIO_HEADER_SIZE:]
            self.assertEqual(audio.AUDIO_PACK_MAGIC, header[0], audio_id)
            self.assertEqual(audio.AUDIO_PACK_VERSION, header[1], audio_id)
            self.assertEqual(header[7] * header[5], len(payload), audio_id)
            self.assertEqual(header[9], len(payload), audio_id)
            self.assertEqual(header[10], binascii.crc32(payload), audio_id)
            if audio_id in audio.MUSIC_SOURCES:
                self.assertLess(header[8], header[4], audio_id)
                if audio_id == "bgm_battle":
                    self.assertGreater(header[8], 0, audio_id)


if __name__ == "__main__":
    unittest.main()
