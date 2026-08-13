#!/usr/bin/env python3

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ReleaseDebugGateTests(unittest.TestCase):
    def test_platformio_defaults_to_release_environment(self):
        config = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("default_envs = m5stick-s3", config)
        release = config.split("[env:m5stick-s3]", 1)[1].split(
            "[env:m5stick-s3-debug]", 1
        )[0]
        self.assertIn("-DSTICKMON_ENABLE_DEBUG_FEATURES=0", release)
        self.assertIn("-DSTICKMON_ENABLE_TRACE_LOGS=0", release)
        self.assertIn("-DSTICKMON_ENABLE_RENDER_STATS=0", release)

    @unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
    def test_release_and_debug_menu_contracts(self):
        source_text = r'''
#include "core/BuildConfig.h"
#include "core/UiStrings.h"

constexpr unsigned menuItemCount =
    sizeof(Ui::Menu::ITEMS) / sizeof(Ui::Menu::ITEMS[0]);
constexpr unsigned debugMotionItemCount =
    sizeof(Ui::Debug::MOTION_ITEMS) / sizeof(Ui::Debug::MOTION_ITEMS[0]);
static_assert(debugMotionItemCount == 4,
              "debug motion menu must include pair interaction and back");

#if EXPECT_DEBUG
static_assert(BuildConfig::DEBUG_FEATURES, "debug build must enable features");
static_assert(menuItemCount == 9, "debug menu must include its test entry");
static_assert(STICKMON_ENABLE_TRACE_LOGS == 1, "debug trace flag changed");
static_assert(STICKMON_ENABLE_RENDER_STATS == 1, "debug stats flag changed");
#else
static_assert(!BuildConfig::DEBUG_FEATURES, "release build enabled debug features");
static_assert(menuItemCount == 8, "release menu exposed its debug entry");
static_assert(STICKMON_ENABLE_TRACE_LOGS == 0, "release trace logs must be off");
static_assert(STICKMON_ENABLE_RENDER_STATS == 0, "release stats must be off");
#endif

int main() { return 0; }
'''
        with tempfile.TemporaryDirectory() as temp_dir:
            source = Path(temp_dir) / "release_debug_gate.cpp"
            source.write_text(source_text, encoding="utf-8")
            for debug in (0, 1):
                binary = Path(temp_dir) / f"release_debug_gate_{debug}"
                subprocess.run(
                    [
                        "c++",
                        "-std=c++17",
                        f"-I{ROOT / 'src'}",
                        f"-DEXPECT_DEBUG={debug}",
                        f"-DSTICKMON_ENABLE_DEBUG_FEATURES={debug}",
                        "-DSTICKMON_ENABLE_TRACE_LOGS=1",
                        "-DSTICKMON_ENABLE_RENDER_STATS=1",
                        str(source),
                        "-o",
                        str(binary),
                    ],
                    check=True,
                    capture_output=True,
                    text=True,
                )


if __name__ == "__main__":
    unittest.main()
