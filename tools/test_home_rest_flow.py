#!/usr/bin/env python3
"""Exercise faint placement, ongoing rest and recovery using the real home state."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HomeRestFlowTests(unittest.TestCase):
    def test_rest_flow_contract(self):
        flow = ROOT / 'firmware/amoled_1_8_v1/main/app/HomeRestFlow.cpp'
        self.assertTrue(flow.exists(), 'independent faint rest flow is missing')
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'rest'
            flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if os.environ.get('STICKMON_TEST_SANITIZE') == '1' else []
            sources = [
                'tools/home_rest_flow_host.cpp', str(flow),
                'firmware/amoled_1_8_v1/main/app/HomeSleepFlow.cpp',
                'src/game/HomeActorController.cpp', 'src/game/HomeCare.cpp',
                'src/game/HomeSimulation.cpp', 'src/game/HomeRuntime.cpp',
                'src/game/HomeCoordinator.cpp', 'src/game/HomeActor.cpp',
                'src/core/RoomNavigator.cpp', 'src/core/RoomMovementArea.cpp',
                'src/game/MonsterMind.cpp', 'src/game/Species.cpp',
                'src/game/GameRandom.cpp',
            ]
            subprocess.run([
                'c++', '-std=c++17', '-O1', *flags, '-Isrc',
                '-Ifirmware/amoled_1_8_v1/main', *sources, '-o', str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)


if __name__ == '__main__':
    unittest.main()
