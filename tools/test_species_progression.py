#!/usr/bin/env python3

import re
import shutil
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FONT_PACK_MAGIC = 0x4E464D53  # SMFN
# 与 tools/generate_font16cn.py 的 UNSCII_CHARS 口径一致：可打印 ASCII 加性别符号。
UNSCII_CHARS = frozenset(chr(codepoint) for codepoint in range(0x21, 0x7F))
UNSCII_CHARS = UNSCII_CHARS.union({"♀", "♂"})


def load_smonfont_codepoints(path):
    data = path.read_bytes()
    magic, _, count, _, _, glyph_bytes, _, _ = struct.unpack_from("<IHHBBBBI", data, 0)
    if magic != FONT_PACK_MAGIC:
        raise AssertionError(f"invalid font pack magic: {path}")
    codepoints = set()
    offset = 16
    for _ in range(count):
        (codepoint,) = struct.unpack_from("<I", data, offset)
        codepoints.add(codepoint)
        offset += 4 + glyph_bytes
    return codepoints


class SpeciesProgressionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_growth_curves_and_level_move_milestones(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "species_progression_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    f"-I{ROOT / 'src'}",
                    str(ROOT / "tools" / "species_progression_host.cpp"),
                    str(ROOT / "src" / "game" / "Species.cpp"),
                    str(ROOT / "src" / "game" / "FriendshipSystem.cpp"),
                    "-o",
                    str(binary),
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_evolution_targets_have_dev_pack_sprites(self):
        species_source = (ROOT / "src" / "game" / "Species.cpp").read_text()
        target_ids = set()
        for line in species_source.splitlines():
            match = re.search(
                r"\bEV\([^)]*\),\s*(\d+),\s*EvolutionMethod::", line
            )
            if not match:
                continue
            target_id = int(match.group(1))
            if target_id != 0:
                target_ids.add(target_id)

        self.assertTrue(target_ids)
        missing = [
            species_id
            for species_id in sorted(target_ids)
            if not (ROOT / "data" / "packs" / "dev" / "sprites" /
                    f"{species_id:03d}.smonsp").is_file()
        ]
        self.assertEqual([], missing)

    def test_area_bosses_have_dev_pack_sprites(self):
        boss_source = (ROOT / "src" / "game" / "ExploreBoss.h").read_text()
        pools = re.findall(
            r"\{\{([^{}]+)\},\s*\d+,\s*\d+\}", boss_source
        )
        self.assertEqual(6, len(pools))
        boss_ids = set()
        for pool in pools:
            candidate_ids = [int(value) for value in re.findall(r"\d+", pool)]
            self.assertEqual(4, len(candidate_ids))
            self.assertEqual(4, len(set(candidate_ids)))
            boss_ids.update(candidate_ids)
        missing = [
            species_id
            for species_id in sorted(boss_ids)
            if not (ROOT / "data" / "packs" / "dev" / "sprites" /
                    f"{species_id:03d}.smonsp").is_file()
        ]
        self.assertEqual([], missing)

    def test_displayed_text_has_font_glyphs(self):
        fonts_dir = ROOT / "data" / "packs" / "dev" / "fonts"
        zh16 = load_smonfont_codepoints(fonts_dir / "zh16.smonfont")
        unscii = load_smonfont_codepoints(fonts_dir / "ascii16-unscii.smonfont")

        # 运行时会上屏的文案来源：UiStrings 与招式数据表。
        text_sources = [
            ROOT / "src" / "core" / "UiStrings.h",
            ROOT / "src" / "game" / "OfficialMoveData.inc",
        ]
        missing = []
        for source in text_sources:
            text = source.read_text(encoding="utf-8")
            for ch in sorted({ch for ch in text
                              if ch.isprintable() and not ch.isspace()}):
                if ch in UNSCII_CHARS:
                    if ord(ch) not in unscii:
                        missing.append(f"{source.name}: U+{ord(ch):04X} {ch}")
                elif ord(ch) >= 0x80:
                    if ord(ch) not in zh16:
                        missing.append(f"{source.name}: U+{ord(ch):04X} {ch}")
                elif 0x21 <= ord(ch) < 0x7F and ord(ch) not in unscii:
                    missing.append(f"{source.name}: U+{ord(ch):04X} {ch}")
        self.assertEqual([], missing)


if __name__ == "__main__":
    unittest.main()
