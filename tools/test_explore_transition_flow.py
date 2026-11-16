#!/usr/bin/env python3
"""Exercise the real door flow without App, storage, rendering or device services."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ExploreTransitionFlowTests(unittest.TestCase):
    def test_transition_contract(self):
        source = ROOT / 'firmware/amoled_1_8_v1/main/app/ExploreTransitionFlow.cpp'
        self.assertTrue(source.exists(), 'independent exploration transition flow is missing')
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'flow'
            flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if os.environ.get('STICKMON_TEST_SANITIZE') == '1' else []
            sources = ['tools/explore_transition_flow_host.cpp', str(source),
                       'src/game/HomeSimulation.cpp', 'src/game/HomeRuntime.cpp',
                       'src/game/HomeCoordinator.cpp', 'src/game/HomeActor.cpp',
                       'src/core/RoomNavigator.cpp', 'src/core/RoomMovementArea.cpp',
                       'src/game/MonsterMind.cpp', 'src/game/Species.cpp', 'src/game/GameRandom.cpp']
            subprocess.run(['c++', '-std=c++17', '-O1', *flags, '-Isrc',
                            '-Ifirmware/amoled_1_8_v1/main', *sources, '-o', str(binary)], cwd=ROOT, check=True)
            subprocess.run([str(binary)], cwd=ROOT, check=True)

if __name__ == '__main__':
    unittest.main()
