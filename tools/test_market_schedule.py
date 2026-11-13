#!/usr/bin/env python3
"""Pure market calendar: game-day boundaries, opening periods and countdown."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class MarketScheduleTest(unittest.TestCase):
    def test_calendar_and_numeric_boundaries(self):
        for debug in (0, 1):
            with self.subTest(debug=debug):
                self.run_calendar_host(debug)

    def run_calendar_host(self, debug):
        with tempfile.TemporaryDirectory(prefix="market-schedule-") as tmp:
            executable = Path(tmp) / "calendar"
            compile_result = subprocess.run(
                ["c++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                 "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                 f"-DSTICKMON_ENABLE_DEBUG_FEATURES={debug}",
                 "-I", str(ROOT / "src"), str(ROOT / "tools/market_schedule_host.cpp"),
                 "-o", str(executable)], text=True, capture_output=True)
            self.assertEqual(compile_result.returncode, 0,
                             compile_result.stdout + compile_result.stderr)
            environment = os.environ.copy()
            # LeakSanitizer cannot run under the managed host's ptrace wrapper;
            # keep address/undefined-behavior checks enabled, as other hosts do.
            environment.setdefault("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1")
            result = subprocess.run([str(executable)], text=True, capture_output=True,
                                    env=environment)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            print(result.stdout, end="")


if __name__ == "__main__":
    unittest.main()
