#!/usr/bin/env python3
"""Compare Settings production behavior with frozen pre-extraction pixels."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from build_web import ROOT, sources_for, ui_flags_for


def compile_and_run(target, output, source_root=ROOT, manifest=None, baseline=False,
                    sanitize=False):
    """Run one journey against production or an explicitly frozen source tree."""
    output.mkdir(parents=True, exist_ok=True)
    if manifest is None:
        sources = [path for path in sources_for(target)
                   if '/platform/web/' not in str(path)]
        flags = ui_flags_for(target)
    else:
        entry = manifest[target]
        sources = [source_root / path for path in entry['sources']]
        flags = entry['flags']
    obj = output / 'tinflate.o'
    cflags = ['-O1', '-ffunction-sections', '-fdata-sections']
    if sanitize:
        cflags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
    subprocess.run(['cc', *cflags, '-c',
                    str(source_root / 'src/third_party/uzlib/tinflate.c'),
                    '-o', str(obj)], check=True)
    binary = output / 'settings-ui-host'
    command = ['c++', '-std=c++17', *cflags,
               '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
               '-I' + str(source_root / 'src'),
               '-DSTICKMON_ENABLE_DEBUG_FEATURES=0',
               '-DSTICKMON_ENABLE_TRACE_LOGS=0', '-DSTICKMON_HAS_CLAW=0',
               '-DSTICKMON_SETTINGS_UI_BASELINE=' + str(int(baseline)),
               *flags, str(ROOT / 'tools/settings_ui_parity_host.cpp'),
               str(source_root / 'src/platform/desktop/DesktopPlatform.cpp'),
               *[str(path) for path in sources if path.suffix == '.cpp'],
               str(obj), '-o', str(binary)]
    subprocess.run(command, cwd=ROOT, check=True)
    environment = os.environ.copy()
    if sanitize:
        environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1'
    run = subprocess.run([str(binary), str(ROOT / 'data'), str(output)],
                         cwd=ROOT, capture_output=True, text=True, env=environment)
    if run.returncode:
        raise AssertionError(run.stdout + run.stderr)
    images = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(output.glob('*.rgb565'))}
    return run.stdout, images


class SettingsUiParityTests(unittest.TestCase):
    def test_production_ui_semantics_and_preserved_pixels(self):
        expected = json.loads((ROOT / 'tools/fixtures/settings_ui_parity.json').read_text())
        results = []
        with tempfile.TemporaryDirectory() as directory:
            for target in ('sticks3', 'ostb'):
                with self.subTest(target=target):
                    result, actual = compile_and_run(target, Path(directory) / target)
                    results.append(result)
                    self.assertEqual(expected[target], actual,
                                     'UI pixels differ from frozen pre-extraction source')
        self.assertEqual(len(results), 2)
        self.assertEqual(results[0], results[1])
        self.assertEqual(results[0], 'settings-checkpoints=20 reset=2 tutorial=0\n')


if __name__ == '__main__':
    unittest.main()
