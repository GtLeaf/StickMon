#!/usr/bin/env python3
"""Check shared party effects and frozen real horizontal save/latch behavior."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from test_explore_shared_rules import compile_adapter

ROOT = Path(__file__).resolve().parents[1]
HOST = "tools/explore_party_adapter_host.cpp"


def compile_service_host(output, sanitize=False):
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-Wno-gnu-anonymous-struct", "-Wno-nested-anon-types"]
    environment = os.environ.copy()
    if sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
        environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1"
    compiled = subprocess.run(["c++", *flags, "-I" + str(ROOT / "src"),
                               str(ROOT / "tools/explore_party_service_host.cpp"),
                               str(ROOT / "src/game/GameRandom.cpp"), "-o", str(output)],
                              capture_output=True, text=True)
    if compiled.returncode:
        raise AssertionError(compiled.stdout + compiled.stderr)
    result = subprocess.run([str(output)], capture_output=True, text=True, env=environment)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


class ExplorePartyServiceTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_independent_legacy_effects_and_untouched_state(self):
        with tempfile.TemporaryDirectory() as directory:
            result = compile_service_host(Path(directory) / "party-service",
                                          sanitize=os.environ.get("STICKMON_TEST_SANITIZE") == "1")
            self.assertEqual("party-service-cases=93600 non-target-state=unchanged rng-calls=0\n", result)

    def test_gameplay_service_has_no_runtime_or_platform_dependency(self):
        source = (ROOT / "src/game/ExplorePartyService.h").read_text()
        for dependency in ("GameEngine", "GameRandom", "hardware/", "platform/", "presentation/", "ui/"):
            self.assertNotIn(dependency, source)
        for path in ("src/scenes/ExploreScene.cpp", "src/core/GameEngine.cpp",
                     "firmware/amoled_1_8_v1/main/app/ExploreSession.cpp"):
            self.assertIn('"game/ExplorePartyService.h"', (ROOT / path).read_text())

    def test_real_horizontal_effects_save_priorities_and_return_latches(self):
        expected = (ROOT / "tools/fixtures/explore_party_adapter.txt").read_text()
        with tempfile.TemporaryDirectory() as directory:
            for target in ("sticks3", "ostb"):
                for debug in (False, True):
                    key = target + ("-debug" if debug else "-release")
                    with self.subTest(configuration=key):
                        actual = compile_adapter(target, Path(directory) / key, debug=debug,
                                                 sanitize=os.environ.get("STICKMON_TEST_SANITIZE") == "1",
                                                 host_source=HOST)
                        # Return now goes straight home. Keep the frozen checks
                        # for unchanged effects; the host asserts the new return
                        # transition and one-time settlement explicitly.
                        self.assertEqual(expected.splitlines()[:-1], actual.splitlines()[:-1])
                        self.assertRegex(actual.splitlines()[-1],
                                         r"^return-settlement-once cases=96 state-save-and-rng=[0-9a-f]{8} rng-calls=0$")


if __name__ == "__main__":
    unittest.main()
