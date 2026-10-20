import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

import generate_sprite_previews as sprites


class SpritePackTests(unittest.TestCase):
    def fixture(self, version=2, checksum=None, frame_offset=0):
        # Two opaque red pixels; the ground marker carries one pixel of padding.
        raw = sprites.PACK_FRAME.pack(0, 2, 1, 0, 0, 0xA501, frame_offset, 3, 0)
        raw += struct.pack("<3H", 2, 0xF800, 0xF800)
        if version == 1:
            return sprites.PACK_HEADER.pack(sprites.PACK_MAGIC, 1, 1, 1, 3, 0, 0) + raw
        encoder = zlib.compressobj(wbits=-15)
        compressed = encoder.compress(raw) + encoder.flush()
        crc = zlib.crc32(raw) if checksum is None else checksum
        return sprites.PACK_HEADER_V2.pack(sprites.PACK_MAGIC, 2, 1, 1, 3, 0, 1, len(raw), len(compressed), crc) + compressed

    def load(self, payload):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.smonsp"
            path.write_bytes(payload)
            return sprites.load_sprite_pack(path, ["FRONT"])

    def test_v1_and_v2_decode_identical_pixels_and_ground(self):
        for version in (1, 2):
            with self.subTest(version=version):
                species, frames, words, palettes = self.load(self.fixture(version))
                self.assertEqual(species, 1)
                self.assertEqual(frames[0]["groundOffsetY"], -1)
                image = sprites.decode_frame(frames[0], words, palettes)
                self.assertEqual([image.getpixel((x, 0)) for x in range(2)],
                                 [(255, 0, 0, 255), (255, 0, 0, 255)])

    def test_corrupt_checksum_and_truncated_payload_are_rejected(self):
        for payload in (self.fixture(checksum=0), self.fixture()[:-1], self.fixture()[:20]):
            with self.subTest(size=len(payload)), self.assertRaises(RuntimeError):
                self.load(payload)

    def test_frame_outside_rle_buffer_is_rejected(self):
        with self.assertRaises(RuntimeError):
            self.load(self.fixture(frame_offset=100))

    def test_all_current_packs_decode(self):
        names = sprites.parse_kind_names(sprites.ASSET_H.read_text())
        packs = sorted(sprites.PACK_DIR.glob("*.smonsp"))
        self.assertTrue(packs)
        for path in packs:
            with self.subTest(pack=path.name):
                _, frames, words, palettes = sprites.load_sprite_pack(path, names)
                self.assertTrue(frames)
                for frame in frames:
                    image = sprites.decode_frame(frame, words, palettes)
                    self.assertEqual(image.size, (frame["width"], frame["height"]))

    def test_generated_catalog_matches_current_pack_dimensions_and_ground(self):
        names = sprites.parse_kind_names(sprites.ASSET_H.read_text())
        manifest = sprites.MANIFEST.read_text().removeprefix("window.STICKMON_PROJECT_SPRITES = ").strip().removesuffix(";")
        entries = {entry["speciesId"]: entry for entry in json.loads(manifest)}
        packs = sorted(sprites.PACK_DIR.glob("*.smonsp"))
        self.assertEqual(len(entries), len(packs))
        for path in packs:
            species, frames, _, _ = sprites.load_sprite_pack(path, names)
            expected = {frame["kindName"]: frame for frame in frames}
            actions = entries[species]["actions"]
            self.assertTrue(actions, path.name)
            for action in actions.values():
                for frame in action:
                    source = expected[frame["kindName"]]
                    for key in ("width", "height", "groundOffsetY"):
                        self.assertEqual(frame[key], source[key], (path.name, key))
                    self.assertTrue((sprites.OUT_DIR / frame["file"]).is_file())


if __name__ == "__main__":
    unittest.main()
