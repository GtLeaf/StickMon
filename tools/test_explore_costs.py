"""Exercise exploration cost rules and the production application adapters."""
import subprocess
import tempfile
import unittest
from pathlib import Path
from test_explore_shared_rules import compile_adapter
from amoled_host import build_amoled_host
ROOT = Path(__file__).resolve().parents[1]

class ExploreCostTests(unittest.TestCase):
    def test_rules_and_clock(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'costs'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-Isrc', 'tools/explore_cost_service_host.cpp',
                'src/core/GameClockService.cpp', 'src/game/Species.cpp', 'src/game/GameRandom.cpp',
                '-o', str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)

    def test_horizontal_adapters(self):
        with tempfile.TemporaryDirectory() as tmp:
            for target in ('sticks3', 'ostb'):
                with self.subTest(target=target):
                    output = compile_adapter(target, Path(tmp) / target,
                        host_source='tools/explore_cost_adapter_host.cpp')
                    self.assertIn('saved costs passed', output)

    def test_amoled_application(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = build_amoled_host(Path(tmp) / 'amoled-costs',
                                      [ROOT / 'tools/explore_cost_amoled_host.cpp'])
            arguments = [str(binary), str(ROOT / 'data')]
            subprocess.run(arguments, cwd=ROOT, check=True)

if __name__ == '__main__':
    unittest.main()
