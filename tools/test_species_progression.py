#!/usr/bin/env python3

import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


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


if __name__ == "__main__":
    unittest.main()
