"""Check candidate equivalence, RNG state, boundaries and temporary-buffer safety."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ParkTreeSelectionTests(unittest.TestCase):
    def test_reference_equivalence_with_sanitizers(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'park-tree'
            subprocess.run(['c++', '-std=c++17', '-O1', '-g',
                            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                            '-I' + str(ROOT/'src'), str(ROOT/'tools/park_tree_selection_host.cpp'),
                            '-o', str(binary)], check=True, capture_output=True, text=True)
            run = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertIn('park-tree=ok cases=9360 distances=49920', run.stdout)


if __name__ == '__main__':
    unittest.main()
