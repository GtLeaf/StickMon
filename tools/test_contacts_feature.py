#!/usr/bin/env python3
"""Validate contact capabilities and shared popup flow without a device SDK."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ContactsFeatureTests(unittest.TestCase):
    def test_capabilities_gate_order_and_shared_popup_flow(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / "contacts-feature"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-Isrc",
                "tools/contacts_feature_host.cpp", "src/core/features/ContactsFeature.cpp",
                "src/ui/common/ComputerUiBase.cpp", "src/ui/common/ContactsUiBase.cpp",
                "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
