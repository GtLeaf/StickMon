#!/usr/bin/env python3
"""Exercise real room sleep/feeding and continuous exploration departures."""
import tempfile
import unittest
from pathlib import Path

from test_home_ui_parity import ROOT, compile_and_run


class DoorDepartureTests(unittest.TestCase):
    def test_real_room_ai_journeys(self):
        with tempfile.TemporaryDirectory() as directory:
            result, _ = compile_and_run(
                'sticks3', Path(directory), sanitize=True,
                host=ROOT / 'tools/home_room_ai_host.cpp')
            self.assertEqual(result.count('PASS '), 6, result)


if __name__ == '__main__':
    unittest.main()
