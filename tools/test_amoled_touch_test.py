#!/usr/bin/env python3
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class TouchTestDiagnostics(unittest.TestCase):
    def test_sequence_records_without_hit_testing_and_controls_are_explicit(self):
        source = r'''
#include "TouchTest.h"
#include <cassert>
using namespace AmoledV1;
int main() {
  TouchTest::State state;
  state.down({24, 32}); state.move({30, 40});
  assert(state.up({31, 41}) == TouchTest::Result::SAMPLE);
  assert(state.count == 1 && state.samples[0].down.x == 24);
  state.down({24, 416});
  assert(state.up({24, 443}) == TouchTest::Result::SAMPLE);
  assert(state.count == 2 && state.samples[1].up.y == 443);
  state.down({80, 360});
  assert(state.up({80, 360}) == TouchTest::Result::CLEAR);
  assert(state.count == 0);
  state.down({210, 360});
  assert(state.up({210, 360}) == TouchTest::Result::BACK);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            cpp = Path(tmp) / "touch_test.cpp"
            binary = Path(tmp) / "touch_test"
            cpp.write_text(source)
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Werror", "-I",
                str(ROOT / "firmware/amoled_1_8_v1/main"), str(cpp), "-o", str(binary)
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
