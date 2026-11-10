#!/usr/bin/env python3
"""Validate cached profile transitions and real Release logging behavior."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FirmwareBuildProfileTests(unittest.TestCase):
    def test_cached_config_and_diagnostic_flags_follow_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "sdkconfig"
            config.write_text("CONFIG_COMPILER_OPTIMIZATION_DEBUG=y\n"
                              "CONFIG_LOG_DEFAULT_LEVEL_INFO=y\n"
                              "CONFIG_BOOTLOADER_LOG_LEVEL_INFO=y\n"
                              "CONFIG_BOARD_UNRELATED=123\n"
                              "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y\n")
            script = root / "profile.cmake"
            script.write_text(f'''
set(SDKCONFIG "{config}")
set(SDKCONFIG_DEFAULTS "base.defaults;board.defaults")
set(STICKMON_ENABLE_TRACE_LOGS ON CACHE BOOL "" FORCE)
set(STICKMON_ENABLE_ROOM_DIAGNOSTICS ON CACHE BOOL "" FORCE)
set(STICKMON_HANG_DIAG ON CACHE BOOL "" FORCE)
include("{ROOT}/firmware/common/build_profile.cmake")
file(WRITE "{root}/result.txt" "${{SDKCONFIG_DEFAULTS}}\n${{STICKMON_HANG_DIAG}}\n${{STICKMON_ENABLE_ROOM_DIAGNOSTICS}}\n${{STICKMON_ENABLE_TRACE_LOGS}}\n")
''')
            # Repeated release configuration must not leave duplicate choices;
            # switching back to Debug must restore development logs and -Og.
            for debug in (False, False, True, False):
                subprocess.run([shutil.which("cmake") or "cmake",
                                f"-DSTICKMON_ENABLE_DEBUG_FEATURES={'ON' if debug else 'OFF'}",
                                "-P", str(script)], cwd=root, check=True,
                               capture_output=True, text=True)
                text = config.read_text()
                self.assertIn("CONFIG_BOARD_UNRELATED=123\n", text)
                self.assertIn("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y\n", text)
                for prefix, value in (("COMPILER_OPTIMIZATION", "DEBUG" if debug else "SIZE"),
                                      ("LOG_DEFAULT_LEVEL", "INFO" if debug else "WARN"),
                                      ("BOOTLOADER_LOG_LEVEL", "INFO" if debug else "WARN")):
                    enabled = [line for line in text.splitlines()
                               if line.startswith(f"CONFIG_{prefix}_") and line.endswith("=y")]
                    self.assertEqual([f"CONFIG_{prefix}_{value}=y"], enabled)
                result = (root / "result.txt").read_text().splitlines()
                self.assertTrue(result[0].startswith("base.defaults;board.defaults;"))
                self.assertEqual(["ON" if debug else "OFF"] * 3, result[1:])

    def test_fresh_config_receives_profile_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / "profile.cmake"
            script.write_text(f'include("{ROOT}/firmware/common/build_profile.cmake")\n')
            subprocess.run(["cmake", "-P", str(script)], cwd=root, check=True,
                           capture_output=True, text=True)
            overlay = (root / "sdkconfig.stickmon-profile").read_text()
            self.assertIn("CONFIG_COMPILER_OPTIMIZATION_SIZE=y", overlay)
            self.assertIn("CONFIG_LOG_DEFAULT_LEVEL_WARN=y", overlay)
            self.assertFalse((root / "sdkconfig").exists())

    def test_logger_hides_routine_release_output_but_preserves_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            for debug, diagnostics in ((0, 0), (1, 0), (0, 1)):
                binary = Path(directory) / f"logging-{debug}-{diagnostics}"
                subprocess.run([
                    "c++", "-std=c++17", "-Os", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-Isrc",
                    f"-DSTICKMON_ENABLE_DEBUG_FEATURES={debug}",
                    f"-DSTICKMON_ENABLE_ROOM_DIAGNOSTICS={diagnostics}",
                    f"-DEXPECT_ROUTINE_LOGS={debug or diagnostics}",
                    "tools/release_logging_host.cpp", "src/platform/api/PlatformServices.cpp",
                    "src/platform/desktop/DesktopPlatform.cpp", "-o", str(binary),
                ], cwd=ROOT, check=True, capture_output=True, text=True)
                subprocess.run([str(binary), str(ROOT / "data")],
                               cwd=ROOT, check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
