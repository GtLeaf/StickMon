#!/usr/bin/env python3
"""Check native test CLI behavior separately from firmware regressions."""
import os
from pathlib import Path
import subprocess
import unittest

from amoled_native_cases import native_render_binary

ROOT = Path(__file__).resolve().parents[1]


class AmoledNativeHarnessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = native_render_binary(os.environ.get("AMOLED_RENDER_DEBUG", "0") == "1")

    def test_unknown_behavior_case_is_rejected(self):
        result = subprocess.run(
            [self.binary, str(ROOT / "data"), "--case", "unknown-case"],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown behavior case", result.stderr)


if __name__ == "__main__":
    unittest.main()
