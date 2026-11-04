#!/usr/bin/env python3
"""Check Room/Food semantics in both UIs and pixels against the pre-split UI."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from build_web import ROOT, sources_for, ui_flags_for


class RoomUiParityTests(unittest.TestCase):
    def test_production_ui_semantics_and_preserved_pixels(self):
        expected = json.loads((ROOT / 'tools/fixtures/room_ui_parity.json').read_text())
        expected.update(json.loads((ROOT / 'tools/fixtures/room_ui_food_header.json').read_text()))
        results = []
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            obj = folder / 'tinflate.o'
            subprocess.run(['cc', '-O1', '-c', str(ROOT / 'src/third_party/uzlib/tinflate.c'),
                            '-o', str(obj)], check=True)
            for target in ('sticks3', 'ostb'):
                with self.subTest(target=target):
                    output = folder / target
                    output.mkdir()
                    binary = output / 'room-ui-host'
                    sources = [p for p in sources_for(target)
                               if p.suffix == '.cpp' and '/platform/web/' not in str(p)]
                    subprocess.run(['c++', '-std=c++17', '-O1', '-ffunction-sections',
                                    '-fdata-sections',
                                    '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
                                    '-Isrc', '-DSTICKMON_ENABLE_DEBUG_FEATURES=0',
                                    '-DSTICKMON_ENABLE_TRACE_LOGS=0', '-DSTICKMON_HAS_CLAW=0',
                                    *ui_flags_for(target), 'tools/room_ui_parity_host.cpp',
                                    'src/platform/desktop/DesktopPlatform.cpp',
                                    *map(str, sources), str(obj), '-o', str(binary)],
                                   cwd=ROOT, check=True)
                    run = subprocess.run([str(binary), str(ROOT / 'data'), str(output)],
                                         cwd=ROOT, capture_output=True, text=True)
                    self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                    results.append(run.stdout)
                    actual = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(output.glob('*.rgb565'))}
                    self.assertEqual(expected[target], actual,
                                     'UI pixels differ from the captured pre-refactor baseline')
        self.assertEqual(len(results), 2)
        self.assertEqual(results[0], results[1])
        self.assertEqual(results[0], 'Room pages captured\n'
                         'Room selection, cancellation, navigation, scheduling and bath checks passed\n')


if __name__ == '__main__':
    unittest.main()
