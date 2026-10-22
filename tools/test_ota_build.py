#!/usr/bin/env python3
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from generate_ota_build import generate, resource_identity

class OtaBuildTest(unittest.TestCase):
    def test_resource_paths_and_content_both_bind_the_fingerprint(self):
        with tempfile.TemporaryDirectory() as tmp:
            stage = Path(tmp)
            file = stage / 'ui.smonfx'
            file.write_bytes(b'native AMOLED pixels')
            first, _ = resource_identity(stage)
            self.assertEqual(first, resource_identity(stage)[0])
            file.write_bytes(b'updated pixels')
            self.assertNotEqual(first, resource_identity(stage)[0])
            second = resource_identity(stage)[0]
            file.rename(stage / 'ui_amoled.smonfx')
            self.assertNotEqual(second, resource_identity(stage)[0])

    def test_flavors_share_resource_baseline_but_have_distinct_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); stage = root / 'stage'; stage.mkdir()
            (stage / 'active.json').write_text('{}')
            config = root / 'version.json'; config.write_text('{"version":"0.2.0","build":100}')
            key = Path(__file__).resolve().parents[1] / 'firmware/amoled_1_8_v2/ota/public-key.pem'
            for flavor in ('lite', 'claw'):
                generate(stage, root / flavor, config, flavor, 'https://example.test', key)
            lite = json.loads((root / 'lite/ota-build.json').read_text())
            claw = json.loads((root / 'claw/ota-build.json').read_text())
            self.assertEqual(lite['resourceSha256'], claw['resourceSha256'])
            self.assertNotEqual(lite['flavor'], claw['flavor'])
            self.assertFalse(lite['debug'])
            self.assertEqual(lite['saveSchema'], 3)
            self.assertEqual(lite['saveVersion'], 5)
            self.assertEqual(lite['minSaveSchema'], 1)
            self.assertEqual(lite['minSaveVersion'], 1)
            with self.assertRaises(ValueError):
                generate(stage, root / 'bad', config, 'lite', 'http://example.test', key)

    def test_device_save_migration_policy(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / 'ota_save_compatibility'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I.', 'tools/ota_save_compatibility_host.cpp', '-o', str(binary)],
                           cwd=root, check=True)
            subprocess.run([str(binary)], check=True)
if __name__ == '__main__':
    unittest.main()
