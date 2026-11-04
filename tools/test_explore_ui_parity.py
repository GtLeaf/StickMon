#!/usr/bin/env python3
"""Check release/debug explore pixels and real journeys in both UI families."""
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
                    debug=False, sanitize=False, host=None):
    """Build the journey from production or an explicitly frozen source tree."""
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
    commands = [
        ['cc', *cflags, '-c', str(source_root / 'src/third_party/uzlib/tinflate.c'),
         '-o', str(obj)],
        ['c++', '-std=c++17', *cflags,
         '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections',
         '-I' + str(source_root / 'src'),
         '-DSTICKMON_ENABLE_DEBUG_FEATURES=' + str(int(debug)),
         '-DSTICKMON_ENABLE_TRACE_LOGS=0', '-DSTICKMON_HAS_CLAW=0',
         '-DSTICKMON_EXPLORE_UI_BASELINE=' + str(int(baseline)), *flags,
         str(host or ROOT / 'tools/explore_ui_parity_host.cpp'),
         str(source_root / 'src/platform/desktop/DesktopPlatform.cpp'),
         *[str(path) for path in sources if path.suffix == '.cpp'],
         str(obj), '-o', str(output / 'explore-ui-host')],
    ]
    with (output / 'compile.log').open('w') as log:
        for command in commands:
            completed = subprocess.run(command, cwd=ROOT, stdout=log, stderr=log)
            if completed.returncode:
                log.flush()
                raise AssertionError((output / 'compile.log').read_text())
    environment = os.environ.copy()
    if sanitize:
        environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
        environment['UBSAN_OPTIONS'] = 'halt_on_error=1'
    run = subprocess.run([str(output / 'explore-ui-host'), str(ROOT / 'data'), str(output)],
                         cwd=ROOT, capture_output=True, text=True, env=environment)
    (output / 'run.log').write_text(run.stdout + run.stderr)
    if run.returncode:
        raise AssertionError(run.stdout + run.stderr)
    images = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(output.glob('*.rgb565'))}
    return run.stdout, images


class ExploreUiParityTests(unittest.TestCase):
    def test_release_and_debug_semantics_and_preserved_pixels(self):
        expected = json.loads((ROOT / 'tools/fixtures/explore_ui_parity.json').read_text())
        for fixture in ('explore_ui_ecology.json', 'explore_ui_touch.json', 'explore_ui_hud.json',
                        'explore_ui_battle_footer.json', 'explore_ui_route_anchor.json',
                        'explore_ui_lower_third.json', 'explore_ui_battle_fx.json'):
            overlay = json.loads((ROOT / 'tools/fixtures' / fixture).read_text())
            for key, frames in overlay.items():
                expected[key].update(frames)
        with tempfile.TemporaryDirectory() as directory:
            for debug in (False, True):
                results = []
                for target in ('sticks3', 'ostb'):
                    key = target + ('-debug' if debug else '-release')
                    with self.subTest(configuration=key):
                        result, actual = compile_and_run(target, Path(directory) / key, debug=debug)
                        results.append(result)
                        differences = sorted(name for name in expected[key].keys() | actual.keys()
                                             if expected[key].get(name) != actual.get(name))
                        self.assertEqual([], differences,
                                         'UI pixels differ from the frozen or reviewed family frames')
                self.assertEqual(len(results), 2)
                self.assertEqual(results[0], results[1])
                self.assertEqual(results[0], 'explore-checkpoints=36\n')


if __name__ == '__main__':
    unittest.main()
