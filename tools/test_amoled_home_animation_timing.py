#!/usr/bin/env python3
"""Exercise original sprite timing through the production V2 home app."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from amoled_host import build_amoled_host

ROOT = Path(__file__).resolve().parents[1]
ARTIFACTS = ROOT / '.stick-local/artifacts/legacy55-animation-20261122/callers/v2-home'


class HomeAnimationTimingTests(unittest.TestCase):
    def test_production_home_timing(self):
        binary = build_amoled_host(ARTIFACTS / 'home-animation-timing',
                                  ['tools/amoled_home_animation_timing_host.cpp'],
                                  extra_flags=['-ffunction-sections', '-fdata-sections'])
        with tempfile.TemporaryDirectory() as directory:
            resources = Path(directory)
            (resources / 'packs/dev/sprites').mkdir(parents=True)
            for path in ['active.json', 'packs/dev/manifest.json', *[
                    f'packs/dev/sprites/{species:03}.smonsp'
                    for species in (1, 11, 41, 93, 94, 129, 133, 147, 148, 214, 264, 280, 355, 362)]]:
                shutil.copyfile(ROOT / 'data' / path, resources / path)
            result = subprocess.run([str(binary), str(resources)], cwd=ROOT,
                                    text=True, capture_output=True)
            (ARTIFACTS / 'host.log').write_text(result.stdout + result.stderr)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
