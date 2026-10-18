#!/usr/bin/env python3
"""Exercise the production web platform and AMOLED application without ESP headers."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from build_web import ROOT, sources_for, stage_resources


class WebSimulatorTests(unittest.TestCase):
    def test_native_web_adapter_and_cancelled_gesture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            resources = root / "resources"
            stage_resources("amoled-v2", resources)
            # Verify target staging uses the same native UI file as the firmware.
            self.assertEqual((resources / "packs/dev/game/ui.smonfx").read_bytes(),
                             (ROOT / "data/packs/dev/game/ui_amoled.smonfx").read_bytes())
            objects = []
            sources = [p for p in sources_for("amoled-v2") if p.name != "main_amoled.cpp"]
            for source in sources:
                if source.suffix == ".c":
                    obj = root / (source.stem + ".o")
                    subprocess.run(["cc", "-O1", "-ffunction-sections", "-fdata-sections",
                                    "-c", str(source), "-o", str(obj)], check=True)
                    objects.append(obj)
            executable = root / "web-simulator-host"
            subprocess.run(["c++", "-std=c++17", "-O1", "-ffunction-sections", "-fdata-sections",
                            "-Wl,-dead_strip" if sys.platform == "darwin" else "-Wl,--gc-sections",
                            "-DSTICKMON_HAS_CLAW=0",
                            "-DSTICKMON_ENABLE_DEBUG_FEATURES=0", "-Isrc",
                            "-Ifirmware/amoled_1_8_v1/main", "tools/web_simulator_host.cpp",
                            *[str(p) for p in sources if p.suffix == ".cpp"],
                            *map(str, objects), "-o", str(executable)], cwd=ROOT, check=True)
            subprocess.run([str(executable)], cwd=resources, check=True)


if __name__ == "__main__":
    unittest.main()
