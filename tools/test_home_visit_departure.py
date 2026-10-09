#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class HomeVisitDepartureTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_visit_departure_phases_and_timeouts(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "home_visit_departure_host"
            subprocess.run([
                "c++", "-std=c++17", "-Werror", "-Isrc",
                "tools/home_visit_departure_host.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
