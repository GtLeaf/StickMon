"""Contract checks for readable, portable native behavior comparisons."""
import copy
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from amoled_sequence import compare_frames, parse_frames, select_reference_frames


class AmoledSequenceTests(unittest.TestCase):
    def setUp(self):
        self.frame = {"seed": 7, "scenario": 0, "frame": 5,
                      "fields": {"actor.x": 100.49840545654297,
                                 "phase": 1, "nextFrameMs": 140}}

    def test_float_rounding_is_tolerated(self):
        actual = copy.deepcopy(self.frame)
        actual["fields"]["actor.x"] -= 0.00000762939453125
        compare_frames([self.frame], [actual])

    def test_visible_position_drift_is_rejected_with_context(self):
        actual = copy.deepcopy(self.frame)
        actual["fields"]["actor.x"] += 0.01
        with self.assertRaisesRegex(AssertionError, r"seed=7.*frame=5.*actor.x"):
            compare_frames([self.frame], [actual])

    def test_integer_fields_are_exact(self):
        actual = copy.deepcopy(self.frame)
        actual["fields"]["nextFrameMs"] += 1
        with self.assertRaisesRegex(AssertionError, "nextFrameMs"):
            compare_frames([self.frame], [actual])

    def test_missing_extra_and_nonfinite_values_are_rejected(self):
        for value in (float("nan"), float("inf")):
            actual = copy.deepcopy(self.frame)
            actual["fields"]["actor.x"] = value
            with self.assertRaises(AssertionError):
                compare_frames([self.frame], [actual])
        actual = copy.deepcopy(self.frame)
        del actual["fields"]["phase"]
        with self.assertRaises(AssertionError):
            compare_frames([self.frame], [actual])
        with self.assertRaises(AssertionError):
            compare_frames([self.frame], [])
        with self.assertRaises(AssertionError):
            compare_frames([self.frame], [self.frame, self.frame])

    def test_reference_samples_preserve_boundaries_and_regular_positions(self):
        frames = []
        for index in range(45):
            fields = {"actor.x": float(index), "phase": int(index >= 3),
                      "actor.route.index": int(index >= 7)}
            frames.append({"seed": 1, "scenario": 0, "frame": index, "fields": fields})
        chosen = select_reference_frames(frames)
        self.assertEqual([row["frame"] for row in chosen], [0, 2, 3, 6, 7, 20, 40, 44])

    def test_cpp_trace_preserves_float_types_and_zero_clock_sentinels(self):
        root = Path(__file__).resolve().parents[1]
        source = '''#include "amoled_sequence_trace.h"
int main() {
    for (uint32_t base : {1000U, 0xfffff000U}) {
        AmoledSequenceTrace trace(1, 0);
        trace.begin(0);
        trace.number("main.x", 100.0f);
        trace.integer("phase", 1);
        trace.clock("inactiveUntil", 0, base, false);
        trace.clock("nextFrameMs", base + 140, base);
        trace.clock("rolloverDeadline", base + 0x1000U, base);
        trace.end();
    }
}'''
        source = '#include <initializer_list>\n' + source
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "sequence-trace"
            subprocess.run(["c++", "-std=c++17", "-I" + str(root / "tools"),
                            "-x", "c++", "-o", str(binary), "-"],
                           input=source, text=True, check=True, capture_output=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True,
                                    check=True, env={**os.environ, "STICKMON_SEQUENCE_TRACE": "1"})
        frames = parse_frames(result.stdout)
        self.assertEqual(len(frames), 2)
        self.assertIs(type(frames[0]["fields"]["main.x"]), float)
        self.assertEqual(frames[0]["fields"]["inactiveUntil"], 0)
        self.assertEqual(frames[0]["fields"]["nextFrameMs"], 140)
        self.assertEqual(frames[1]["fields"]["rolloverDeadline"], 4096)
        compare_frames(frames[:1], frames[1:])


if __name__ == "__main__":
    unittest.main()
