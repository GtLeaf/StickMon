#!/usr/bin/env python3
"""Check shared exploration tuning against independently frozen frontend data."""

import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from build_web import sources_for, ui_flags_for

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tools/fixtures/explore_shared_rules.json"


def compile_adapter(target, output, source_root=ROOT, debug=False, sanitize=False,
                    host_source="tools/explore_shared_adapter_host.cpp"):
    """Run the identical actual-scene probe against current or frozen production."""
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    sources = [source_root / path.relative_to(ROOT) for path in sources_for(target)
               if "/platform/web/" not in str(path)]
    flags = ["-O1", "-ffunction-sections", "-fdata-sections"]
    if sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
    obj = output / "tinflate.o"
    commands = [
        ["cc", *flags, "-c", str(source_root / "src/third_party/uzlib/tinflate.c"),
         "-o", str(obj)],
        ["c++", "-std=c++17", *flags,
         "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
         "-I" + str(source_root / "src"),
         "-DSTICKMON_HAS_CLAW=0", "-DSTICKMON_ENABLE_TRACE_LOGS=0",
         "-DSTICKMON_ENABLE_DEBUG_FEATURES=" + str(int(debug)), *ui_flags_for(target),
         str(ROOT / host_source),
         str(source_root / "src/platform/desktop/DesktopPlatform.cpp"),
         *[str(path) for path in sources if path.suffix == ".cpp"], str(obj),
         "-o", str(output / "explore-adapter")],
    ]
    with (output / "compile.log").open("w") as log:
        for command in commands:
            result = subprocess.run(command, stdout=log, stderr=log, cwd=ROOT)
            if result.returncode:
                log.flush()
                raise AssertionError((output / "compile.log").read_text())
    environment = os.environ.copy()
    if sanitize:
        environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1"
    result = subprocess.run([str(output / "explore-adapter"), str(ROOT / "data")],
                            capture_output=True, text=True, env=environment, cwd=ROOT)
    (output / "run.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


def fixture_input(fixture):
    values = [len(fixture["areas"])]
    for area in fixture["areas"]:
        values.extend(area[key] for key in
                      ("minMapCount", "maxMapCount", "encounterChance", "minCoin", "maxCoin"))
        values.append(len(area["entries"]))
        values.extend(value for entry in area["entries"] for value in entry)
    values.extend(fixture[key] for key in
                  ("cooldownSteps", "maxEncountersPerMap", "mapPickupChance", "mapGenerationSafeSeed"))
    values.extend(fixture["mapGenerationRetrySalts"])
    return " ".join(map(str, values)) + "\n"


class ExploreSharedRulesTests(unittest.TestCase):
    def test_real_horizontal_adapters_preserve_state_and_random_draws(self):
        expected = (ROOT / "tools/fixtures/explore_shared_adapter.txt").read_text()
        with tempfile.TemporaryDirectory() as directory:
            for target in ("sticks3", "ostb"):
                for debug in (False, True):
                    key = target + ("-debug" if debug else "-release")
                    with self.subTest(configuration=key):
                        actual = compile_adapter(target, Path(directory) / key, debug=debug,
                                                 sanitize=os.environ.get("STICKMON_TEST_SANITIZE") == "1")
                        self.assertEqual(expected, actual)

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_frozen_tuning_exhaustive_pickup_boundaries_and_encounter_gates(self):
        fixture = json.loads(FIXTURE.read_text())
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "explore-shared-rules"
            flags = ["-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror", "-pedantic",
                     "-Wno-gnu-anonymous-struct", "-Wno-nested-anon-types"]
            environment = os.environ.copy()
            if environment.get("STICKMON_TEST_SANITIZE") == "1":
                flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
                environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
                environment["UBSAN_OPTIONS"] = "halt_on_error=1"
            subprocess.run(["c++", *flags, "-I" + str(ROOT / "src"),
                            str(ROOT / "tools/explore_shared_rules_host.cpp"),
                            str(ROOT / "src/game/GameRandom.cpp"), "-o", str(binary)],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], input=fixture_input(fixture),
                                    check=True, capture_output=True, text=True, env=environment)
            self.assertRegex(result.stdout, r"shared-rules-checks=\d+ rng-calls=0\n")

    def test_pickup_rules_are_pure_and_both_frontends_use_shared_selection(self):
        rules = (ROOT / "src/game/ExplorePickupRules.h").read_text()
        self.assertNotIn("GameEngine", rules)
        self.assertNotIn("GameRandom", rules)
        self.assertNotIn("hardware/", rules)
        for path in ("src/scenes/ExploreScene.cpp",
                     "firmware/amoled_1_8_v1/main/app/ExploreSession.cpp"):
            source = (ROOT / path).read_text()
            self.assertIn('"game/ExplorePickupRules.h"', source)
            alias = re.search(r"namespace\s+(\w+)\s*=\s*ExplorePickupRules\s*;", source)
            qualifier = alias[1] if alias else "ExplorePickupRules"
            self.assertIn(qualifier + "::weightTotal", source)
            self.assertIn(qualifier + "::forRoll", source)
            self.assertNotIn("struct PickupEntry", source)
            self.assertNotIn("struct ExplorePickupEntry", source)
            self.assertNotIn("GRASS_PATH_PICKUPS[]", source)


if __name__ == "__main__":
    unittest.main()
