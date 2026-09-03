#!/usr/bin/env python3
"""Host-side test for src/brain/ClawStatusLog (64-entry ring buffer).

Verifies ring overflow accounting, recent/since reads, and that long
messages are split into continuation entries at UTF-8 boundaries without
losing or truncating content.
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("c++"), "host C++ compiler is unavailable")
class ClawStatusLogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = Path(tempfile.mkdtemp()) / "claw_status_log_host"
        subprocess.run(
            [
                "c++",
                "-std=c++17",
                f"-I{ROOT / 'src'}",
                str(ROOT / "tools" / "claw_status_log_host.cpp"),
                str(ROOT / "src" / "brain" / "ClawStatusLog.cpp"),
                "-o",
                str(cls.binary),
            ],
            check=True,
            capture_output=True,
            text=True,
        )

    def run_host(self, *args):
        result = subprocess.run(
            [str(self.binary), *args],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.splitlines()

    @staticmethod
    def parse_entries(lines):
        # "ms|level|text"; text itself never contains "|" in these cases.
        out = []
        for line in lines:
            ms, level, text = line.split("|", 2)
            out.append((int(ms), int(level), text))
        return out

    def test_fill_overflow_ring(self):
        lines = self.run_host("fill", "70")
        self.assertEqual(lines, ["size=64 gen=70 oldest=6"])

    def test_recent_after_overflow(self):
        lines = self.run_host("recent", "5")
        self.assertEqual(lines[0], "count=5")
        entries = self.parse_entries(lines[1:])
        self.assertEqual(
            entries,
            [
                (1065, 0, "entry-65"),
                (1066, 0, "entry-66"),
                (1067, 0, "entry-67"),
                (1068, 0, "entry-68"),
                (1069, 0, "entry-69"),
            ],
        )

    def test_since_cursor(self):
        lines = self.run_host("since", "68")
        self.assertEqual(lines[0], "count=2")
        entries = self.parse_entries(lines[1:])
        self.assertEqual([text for _, _, text in entries], ["entry-68", "entry-69"])

    def test_since_before_oldest_returns_all(self):
        lines = self.run_host("since", "0")
        self.assertEqual(lines[0], "count=64")
        entries = self.parse_entries(lines[1:])
        self.assertEqual(entries[0][2], "entry-6")
        self.assertEqual(entries[-1][2], "entry-69")

    def test_long_message_split_not_truncated(self):
        lines = self.run_host("long")
        self.assertEqual(lines[0], "count=3")
        entries = self.parse_entries(lines[1:])
        expected = "".join(chr(ord("a") + i % 26) for i in range(130))
        self.assertEqual("".join(text for _, _, text in entries), expected)
        for ms, level, _ in entries:
            self.assertEqual((ms, level), (7, 2))  # WARN

    def test_utf8_split_respects_character_boundary(self):
        lines = self.run_host("utf8")
        self.assertEqual(lines[0], "count=2")
        entries = self.parse_entries(lines[1:])
        expected = "热点已启动微信二维码已生成等待手机扫码确认登录成功"
        joined = "".join(text for _, _, text in entries)
        self.assertEqual(joined, expected)
        # Each entry decodes cleanly, i.e. no multi-byte character was split.
        for _, _, text in entries:
            text.encode("utf-8").decode("utf-8")
            self.assertLessEqual(len(text.encode("utf-8")), 63)


if __name__ == "__main__":
    unittest.main()
