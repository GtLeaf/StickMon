#!/usr/bin/env python3

import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ContactsControllerTests(unittest.TestCase):
    def test_state_semantics_read_only_queries_and_storage_model(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            binary = Path(temp_dir) / "contacts_controller_host"
            subprocess.run(
                [
                    "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-Isrc", "-Ifirmware/amoled_1_8_v1/main",
                    "tools/contacts_controller_host.cpp",
                    "firmware/amoled_1_8_v1/main/app/ContactsController.cpp",
                    "-o", str(binary),
                ], cwd=ROOT, check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
