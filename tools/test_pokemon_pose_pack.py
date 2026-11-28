"""Pixel and metadata contracts for the v5 pose bank and frame script."""
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from PIL import Image

import generate_pokemon_sprites as generator
from room_editor import generate_sprite_previews as preview


HEADER = struct.Struct('<IHHHHHHIIIHH')
STEP = struct.Struct('<HHBBBBhhBBBB')
POSE = struct.Struct('<BBBBIII')


class PosePackTests(unittest.TestCase):
    def make_writer(self):
        writer = generator.AssetWriter()
        ident = generator.RETIMED_BY_ID[381].ident
        original = Image.new('RGBA', (12, 10))
        original.putpixel((2, 3), (255, 0, 0, 255))
        original.putpixel((3, 3), (0, 255, 0, 255))
        original.putpixel((2, 4), (0, 0, 255, 17))
        translated = Image.new('RGBA', original.size)
        translated.paste(original.crop((2, 3, 4, 5)), (6, 4))
        mirrored = Image.new('RGBA', original.size)
        mirrored.paste(original.crop((2, 3, 4, 5)).transpose(Image.Transpose.FLIP_LEFT_RIGHT), (5, 1))
        images = [original, original.copy(), translated, mirrored]
        for index, image in enumerate(images):
            writer.add_frame(381, ident, f'{ident}_IDLE_FRONT_{index}', image,
                             shadow=(6, 8, 3, 1), ground_y=8)
        writer.add_frame(381, ident, 'FRONT', original)
        return writer, images + [original]

    def generate(self, writer, pose_reuse=True):
        kinds = {frame['kind']: index for index, frame in enumerate(writer.frames)}
        names = list(kinds)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / '381.smonsp'
            generator.write_species_pack(path, 381, writer, kinds, pose_reuse=pose_reuse)
            return path.read_bytes(), preview.load_sprite_pack(path, names)

    def test_reuses_exact_translated_and_mirrored_quantized_pixels(self):
        writer, images = self.make_writer()
        packed, (_, frames, words, palettes) = self.generate(writer)
        header = HEADER.unpack_from(packed)
        self.assertEqual(5, header[1])
        self.assertEqual(5, header[3])
        self.assertEqual(2, header[-1])
        self.assertEqual([0, 0, 0, 0, 1], [frame['poseIndex'] for frame in frames])
        self.assertEqual([False, False, False, True, False], [frame['imageFlipX'] for frame in frames])
        self.assertEqual([(2, 3), (2, 3), (6, 4), (5, 1), (0, 0)],
                         [(frame['imageOffsetX'], frame['imageOffsetY']) for frame in frames])
        for index, (frame, image) in enumerate(zip(frames, images)):
            expected = [preview.rgb565_to_rgba(generator.rgb565(r, g, b)) if a > 16 else (0, 0, 0, 0)
                        for r, g, b, a in generator.image_pixels(image)]
            self.assertEqual(expected, generator.image_pixels(preview.decode_frame(frame, words, palettes)))
            self.assertEqual((12, 10), (frame['width'], frame['height']))
            self.assertEqual(list(writer.frames[index]['shadow']), frame['shadow'])
            self.assertEqual(5 - writer.frames[index]['ground_padding'], frame['groundOffsetY'])
        self.assertEqual((12, 10), (frames[-1]['imageWidth'], frames[-1]['imageHeight']))

    def test_legacy_opt_out_preserves_full_frames(self):
        writer, _ = self.make_writer()
        packed, (_, frames, _, _) = self.generate(writer, pose_reuse=False)
        self.assertEqual(3, struct.unpack_from('<H', packed, 4)[0])
        self.assertNotIn('poseIndex', frames[0])

    def test_reuse_compares_quantized_color_and_transparency_exactly(self):
        writer = generator.AssetWriter()
        ident = generator.RETIMED_BY_ID[381].ident
        images = [Image.new('RGBA', (1, 1), pixel) for pixel in
                  ((255, 0, 0, 255), (254, 0, 0, 17), (247, 0, 0, 255),
                   (255, 123, 45, 16), (0, 0, 0, 0))]
        for index, image in enumerate(images):
            writer.add_frame(381, ident, f'{ident}_IDLE_FRONT_{index}', image)
        _, (_, frames, _, _) = self.generate(writer)
        self.assertEqual([0, 0, 1, 2, 2], [frame['poseIndex'] for frame in frames])

    def test_untimed_species_and_obsolete_blocks_keep_legacy_format(self):
        for sid, ident in ((352, 'KECLEON'), (381, 'LATIOS')):
            writer = generator.AssetWriter()
            name = f'{ident}_IDLE_FRONT_0'
            writer.add_frame(sid, ident, name, Image.new('RGBA', (2, 2), 'red'))
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / 'legacy.smonsp'
                generator.write_species_pack(path, sid, writer, {name: 0})
                self.assertEqual(3, struct.unpack_from('<H', path.read_bytes(), 4)[0])

    def test_rgb565_pose_roundtrip_and_legacy_rle_alias_sharing(self):
        writer = generator.AssetWriter()
        ident = generator.RETIMED_BY_ID[381].ident
        image = Image.new('RGBA', (20, 1))
        image.putdata([(i * 8, 0, 0, 255) for i in range(20)])
        for kind in (f'{ident}_IDLE_FRONT_0', 'FRONT', 'BACK'):
            writer.add_frame(381, ident, kind, image)
        _, (_, frames, words, palettes) = self.generate(writer)
        self.assertEqual([0, 0, 0], [frame['format'] for frame in frames])
        self.assertEqual(21, len(words))
        self.assertEqual([], palettes)
        expected = [preview.rgb565_to_rgba(generator.rgb565(r, g, b))
                    for r, g, b, _ in generator.image_pixels(image)]
        for frame in frames:
            self.assertEqual(expected, generator.image_pixels(preview.decode_frame(frame, words, palettes)))

    def test_v5_preserves_extended_rle_word_count(self):
        writer = generator.AssetWriter()
        ident = generator.RETIMED_BY_ID[381].ident
        for index in range(4):
            image = Image.new('RGBA', (128, 128))
            image.putdata([((pixel & 31) * 8, ((pixel >> 5) & 63) * 4,
                            ((pixel >> 11) + index) * 8, 255)
                           for pixel in range(128 * 128)])
            writer.add_frame(381, ident, f'{ident}_IDLE_FRONT_{index}', image)
        packed, (_, frames, words, palettes) = self.generate(writer)
        header = HEADER.unpack_from(packed)
        self.assertEqual(1, header[-2])
        self.assertEqual(65540, len(words))
        self.assertEqual(4, len(frames))
        self.assertEqual([], palettes)
        self.assertEqual([0, 16385, 32770, 49155], [frame['offset'] for frame in frames])

    def test_v5_rejects_bad_pose_reference_geometry_flags_and_crc(self):
        writer, _ = self.make_writer()
        packed, _ = self.generate(writer)
        header = list(HEADER.unpack_from(packed))
        original = zlib.decompress(packed[HEADER.size:], -15)
        mutations = [(2, '<H', header[-1]), (8, '<h', -1), (8, '<h', 12),
                     (7, '<B', 2), (6, '<B', 255)]
        for offset, fmt, value in mutations:
            raw = bytearray(original)
            struct.pack_into(fmt, raw, offset, value)
            encoder = zlib.compressobj(wbits=-15)
            compressed = encoder.compress(raw) + encoder.flush()
            altered = header[:]
            altered[8] = len(compressed)
            altered[9] = zlib.crc32(raw)
            self.assert_invalid(HEADER.pack(*altered) + compressed)
        corrupted = header[:]
        corrupted[9] ^= 1
        self.assert_invalid(HEADER.pack(*corrupted) + packed[HEADER.size:])

    def assert_invalid(self, payload):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'invalid.smonsp'
            path.write_bytes(payload)
            with self.assertRaises(RuntimeError):
                preview.load_sprite_pack(path, [])


if __name__ == '__main__':
    unittest.main()
