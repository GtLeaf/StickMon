#!/usr/bin/env python3
"""Exercise sleep geometry, ownership and wake behavior using the real home state."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HomeSleepFlowTests(unittest.TestCase):
    def test_sleep_flow_contract(self):
        flow = ROOT / 'firmware/amoled_1_8_v1/main/app/HomeSleepFlow.cpp'
        self.assertTrue(flow.exists(), 'independent sleep flow is missing')
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'sleep'
            flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if os.environ.get('STICKMON_TEST_SANITIZE') == '1' else []
            sources = [
                'tools/home_sleep_flow_host.cpp', str(flow),
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
