#!/usr/bin/env python3

import json
import tempfile
import unittest
from pathlib import Path

from package_amoled_v2_release import UPLOAD_FILES, package_release


class PackageAmoledV2ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.build_dir = Path(self.temp_dir.name)
        self.flash_files = {}
        for offset, (source, _) in UPLOAD_FILES.items():
            self.flash_files[offset] = source
            artifact = self.build_dir / source
            artifact.parent.mkdir(parents=True, exist_ok=True)
            artifact.write_bytes(source.encode())
        self.write_flash_args()

    def write_flash_args(self):
        (self.build_dir / "flasher_args.json").write_text(json.dumps({
            "flash_settings": {"flash_size": "16MB"},
            "flash_files": self.flash_files,
        }))

    def test_exports_five_upload_names_and_replaces_stale_files(self):
        output = self.build_dir / "out"
        output.mkdir()
        (output / "stale.bin").write_bytes(b"stale")

        self.assertEqual(package_release(self.build_dir), output.resolve())
        self.assertEqual(
            {file.name for file in output.iterdir()},
            {destination for _, destination in UPLOAD_FILES.values()},
        )
        for source, destination in UPLOAD_FILES.values():
            self.assertEqual((output / destination).read_bytes(),
                             (self.build_dir / source).read_bytes())

    def test_refuses_wrong_offsets_without_touching_existing_output(self):
        output = self.build_dir / "out"
        output.mkdir()
        (output / "keep.bin").write_bytes(b"keep")
        self.flash_files["0x20000"] = "wrong.bin"
        self.write_flash_args()

        with self.assertRaisesRegex(ValueError, "offsets differ"):
            package_release(self.build_dir)
        self.assertEqual((output / "keep.bin").read_bytes(), b"keep")

    def test_refuses_missing_file(self):
        (self.build_dir / "resources.bin").unlink()
        with self.assertRaisesRegex(FileNotFoundError, "resources.bin"):
            package_release(self.build_dir)


if __name__ == "__main__":
    unittest.main()
