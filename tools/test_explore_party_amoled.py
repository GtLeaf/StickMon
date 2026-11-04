#!/usr/bin/env python3
"""Exercise the real AMOLED session with optional frozen-source comparison."""

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    "firmware/amoled_1_8_v1/main/app/ExploreSession.cpp",
    "src/core/SaveCodec.cpp",
    "src/game/ExploreMapGenerator.cpp",
    "src/game/GameRandom.cpp",
    "src/game/ItemInventory.cpp",
    "src/game/MonsterFactory.cpp",
    "src/game/Species.cpp",
]


class ExplorePartyAmoledTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which("c++"):
            raise unittest.SkipTest("host C++ compiler is unavailable")
        cls.temporary = tempfile.TemporaryDirectory(prefix="explore-party-amoled-")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = Path(cls.temporary.name)
        cls.capture_dir = Path(os.environ["EXPLORE_PARTY_CAPTURE_DIR"]).resolve() \
            if os.environ.get("EXPLORE_PARTY_CAPTURE_DIR") else None
        if cls.capture_dir:
            cls.capture_dir.mkdir(parents=True, exist_ok=True)
        cls.records = {"builds": [], "scenarios": {}}
        cls.binaries = {"current": cls.build(ROOT, "current")}
        if baseline := os.environ.get("EXPLORE_PARTY_BASELINE_ROOT"):
            cls.binaries["baseline"] = cls.build(Path(baseline).resolve(), "baseline")
        if os.environ.get("EXPLORE_PARTY_SANITIZE") == "1":
            cls.binaries["sanitize"] = cls.build(ROOT, "sanitize", sanitize=True)

    @classmethod
    def build(cls, source_root, label, sanitize=False):
        binary = cls.directory / label
        command = [
            "c++", "-std=c++17", "-O1", "-g", "-ffunction-sections", "-fdata-sections",
            "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
            "-Wall", "-Wextra", "-pedantic",
            "-Wno-gnu-anonymous-struct", "-Wno-nested-anon-types",
            "-I" + str(source_root / "src"),
            "-I" + str(source_root / "firmware/amoled_1_8_v1/main"),
            str(ROOT / "tools/explore_party_amoled_host.cpp"),
            *[str(source_root / source) for source in SOURCES],
            "-o", str(binary),
        ]
        if sanitize:
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        strict_command = ["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                          "-Wno-gnu-anonymous-struct", "-Wno-nested-anon-types", "-fsyntax-only",
                          "-I" + str(source_root / "src"),
                          "-I" + str(source_root / "firmware/amoled_1_8_v1/main"),
                          str(ROOT / "tools/explore_party_amoled_host.cpp"),
                          str(source_root / SOURCES[0])]
        strict_result = subprocess.run(strict_command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT)
        if strict_result.returncode:
            raise AssertionError(strict_result.stdout)
        result = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        record = {"label": label, "sourceRoot": str(source_root), "command": command,
                  "exitCode": result.returncode,
                  "strictProbeAndSessionCommand": strict_command,
                  "strictProbeAndSessionExitCode": strict_result.returncode,
                  "sourceSha256": {source: hashlib.sha256((source_root / source).read_bytes()).hexdigest()
                                   for source in SOURCES}}
        cls.records["builds"].append(record)
        if cls.capture_dir:
            (cls.capture_dir / f"amoled-party-build-{label}.log").write_text(result.stdout)
        if result.returncode:
            raise AssertionError(result.stdout)
        return binary

    @classmethod
    def tearDownClass(cls):
        if cls.capture_dir:
            (cls.capture_dir / "amoled-party-results.json").write_text(
                json.dumps(cls.records, indent=2) + "\n")

    def run_scenario(self, name):
        outputs = {}
        records = {}
        for label, binary in self.binaries.items():
            environment = dict(os.environ)
            if label == "sanitize":
                environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
                environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
            result = subprocess.run([str(binary), name], cwd=ROOT, env=environment,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            records[label] = {"exitCode": result.returncode,
                              "sha256": hashlib.sha256(result.stdout.encode()).hexdigest(),
                              "lines": len(result.stdout.splitlines())}
            if self.capture_dir:
                (self.capture_dir / f"amoled-party-{name}-{label}.txt").write_text(result.stdout)
                (self.capture_dir / f"amoled-party-{name}-{label}.log").write_text(result.stderr)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(result.stdout.startswith(("leader ", "recover ", "step ", "return ", "finished ")))
            outputs[label] = result.stdout
        self.records["scenarios"][name] = records
        for label, output in outputs.items():
            self.assertEqual(outputs["current"], output, f"{name}: current differs from {label}")

    def test_leaders_followers_and_zero_species(self):
        self.run_scenario("leaders")

    def test_recovery_rounding_health_visitors_status_and_bounds(self):
        self.run_scenario("recovery")

    def test_actual_completed_step_effects_and_wrap(self):
        self.run_scenario("steps")

    def test_return_bond_status_items_unconditional_save_and_latch(self):
        self.run_scenario("settlement")

    def test_finish_return_and_new_session_reset(self):
        self.run_scenario("lifecycle")


if __name__ == "__main__":
    unittest.main()
