#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class AppSceneFlowTests(unittest.TestCase):
    def test_host_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "app_scene_flow_host"
            subprocess.run(
                [
                    "c++",
                    "-std=c++11",
                    "-Isrc",
                    "tools/app_scene_flow_host.cpp",
                    "-o",
                    str(binary),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
