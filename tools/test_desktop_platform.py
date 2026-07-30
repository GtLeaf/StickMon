#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class DesktopPlatformTests(unittest.TestCase):
    def test_platform_services_smoke(self):
        with tempfile.TemporaryDirectory() as temporary:
            temporary_path = Path(temporary)
            resource_root = temporary_path / "resources"
            resource_root.mkdir()
            (resource_root / "fixture.bin").write_bytes(bytes((1, 2, 3, 4)))
            executable = temporary_path / "desktop_platform_smoke"
            subprocess.run(
                [
                    "c++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-Isrc",
                    "tools/desktop_platform_smoke.cpp",
                    "src/platform/api/PlatformServices.cpp",
                    "src/platform/desktop/DesktopPlatform.cpp",
                    "src/presentation/Canvas565.cpp",
                    "src/core/GameClockService.cpp",
                    "src/core/SaveCoordinator.cpp",
                    "src/hardware/EspNowLink.cpp",
                    "-o",
                    str(executable),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(executable), str(resource_root)], check=True)


if __name__ == "__main__":
    unittest.main()
