#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path

from amoled_host import build_amoled_host


ROOT = Path(__file__).resolve().parents[1]


class HomeViewStateTests(unittest.TestCase):
    def test_header_is_self_contained(self):
        subprocess.run([
            "c++", "-std=c++17", "-fsyntax-only", "-x", "c++",
            "-Isrc", "-Ifirmware/amoled_1_8_v1/main", "-",
        ], input='#include "app/HomeViewState.h"\n', text=True,
            cwd=ROOT, check=True)

    def test_real_mapping_invalid_secondary_and_save_blob(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "home_view_state_host"
            build_amoled_host(binary, ["tools/home_view_state_host.cpp"])
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
