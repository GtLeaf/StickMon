#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise smart ROM installation against flash bytes, including failures."""
import contextlib
import hashlib
import io
from pathlib import Path
import struct
import tempfile
import types
import unittest
from unittest.mock import patch

import sdgoods_release as release
import sdgoods_flash_install as installer


def partition_table(layout=None):
    records = b''.join(struct.pack('<HBBII16sI', 0x50aa, typ, sub, at, size, name.encode(), 0)
                       for name, (typ, sub, at, size) in (layout or release.EXPECTED).items())
    records += struct.pack('<H14s16s', 0xebeb, b'\xff' * 14, hashlib.md5(records).digest())
    return records.ljust(4096, b'\xff')


class Flash:
    def __init__(self, folder):
        self.data = {at: installer.image_bytes(folder, name) for at, name in release.write_plan(folder)}
        self.digests, self.writes = [], []
        self.CHIP_NAME, self.secure_download_mode = 'ESP32-S3', False
        self.closed = self.reset = False
        self.stub_ready = False
        self._port = types.SimpleNamespace(close=lambda: setattr(self, 'closed', True))
        self.mac = bytes.fromhex('ecda3b67105c')
        self.encrypted = self.secure = False

    def read_mac(self, kind):
        return self.mac

    def get_flash_encryption_enabled(self):
        return self.encrypted

    def get_secure_boot_enabled(self):
        return self.secure

    def run_stub(self):
        self.stub_ready = True
        return self

    def change_baud(self, baud):
        assert baud == 921600

    def flash_set_parameters(self, size):
        assert size == 32 * 1024 * 1024

    def hard_reset(self):
        self.reset = True

    def read_flash(self, at, size):
        return self.data[at][:size]

    def flash_md5sum(self, at, size):
        self.digests.append((at, size))
        return hashlib.md5(self.data[at][:size]).hexdigest()

    def write(self, folder, plan, verify=True):
        self.writes.extend(plan)
        for at, name in plan:
            self.data[at] = installer.image_bytes(folder, name)
            if verify:
                self.flash_md5sum(at, len(self.data[at]))


class SmartInstallTests(unittest.TestCase):
    def setUp(self):
        out = release.ROOT / '.stick-local/artifacts/sdgoods-flash-speed-20261106'
        out.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=out)
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        for name, data in {'app.bin': b'\xe9' + b'a' * 5010, 'resources.bin': b'r' * 8201,
                           'bootloader.bin': b'\xe9' + b'b' * 510,
                           'partition-table.bin': partition_table(), 'otadata.bin': release.ota_data(),
                           'resmeta.bin': b'm' * 8192}.items():
            (self.folder / name).write_bytes(data)
        self.flash = Flash(self.folder)
        self.output = io.StringIO()
        context = contextlib.redirect_stdout(self.output)
        context.__enter__()
        self.addCleanup(context.__exit__, None, None, None)

    def run_install(self, writer=None):
        return installer.install_connected(self.folder, self.flash,
            writer or (lambda plan: self.flash.write(self.folder, plan)))

    def change(self, at, index=0):
        data = bytearray(self.flash.data[at])
        data[index] ^= 0xff
        self.flash.data[at] = bytes(data)

    def test_all_equal_never_erases_or_writes(self):
        result = self.run_install()
        self.assertEqual(result['skipped_regions'], 7)
        self.assertEqual(result['written_regions'], 0)
        self.assertEqual(len(self.flash.digests), 7)
        self.assertEqual(self.flash.writes, [])

    def test_code_only_updates_both_apps_and_binding(self):
        for at in (0x20000, 0x320000, 0xfe0000):
            self.change(at)
        result = self.run_install()
        self.assertEqual(result['written_regions'], 3)
        self.assertEqual(result['write_verified_regions'], 3)
        self.assertEqual(result['skipped_regions'], 4)
        self.assertEqual([at for at, _ in self.flash.writes], [0x20000, 0x320000, 0xfe0000])
        # Every region is hashed exactly once: pre-check when equal, post-write otherwise.
        self.assertEqual(len(self.flash.digests), 7)

    def test_corruption_after_equal_prefix_is_repaired_independently(self):
        self.change(0x620000, 6000)
        result = self.run_install()
        self.assertEqual(result['written_regions'], 1)
        self.assertEqual(self.flash.writes, [(0x620000, 'resources.bin')])
        self.assertEqual(len(self.flash.digests), 8)

    def test_blank_device_writes_all_without_pre_hashing(self):
        for at, data in self.flash.data.items():
            self.flash.data[at] = b'\xff' * len(data)
        result = self.run_install()
        self.assertEqual(result['written_regions'], 7)
        self.assertEqual(result['write_verified_regions'], 7)
        self.assertEqual(len(self.flash.digests), 7)

    def test_unknown_table_fails_before_writes(self):
        self.flash.data[0x8000] = b'\x00' * 4096
        with self.assertRaises(ValueError):
            self.run_install()
        self.assertEqual(self.flash.writes, [])

    def test_known_previous_layouts_can_migrate(self):
        for layout in (release.PREVIOUS_15M, release.PREVIOUS_SINGLE_24M, release.PREVIOUS_AB, release.LEGACY):
            with self.subTest(layout=layout):
                self.flash.data[0x8000] = partition_table(layout)
                self.assertEqual(self.run_install()['written_regions'], 1)

    def test_layout_ends_at_16_mib_without_overlapping_partitions(self):
        regions = sorted((at, at + size) for _, _, at, size in release.EXPECTED.values())
        self.assertEqual(regions[-1][1], 16 * 1024 * 1024)
        for (_, high), (low, _) in zip(regions, regions[1:]):
            self.assertLessEqual(high, low)

    def test_default_matches_planned_16_mib_shared_geometry(self):
        def rows(path):
            return [tuple(field.strip() for field in line.split(','))
                    for line in path.read_text().splitlines()
                    if line.strip() and not line.startswith('#')]
        self.assertEqual(rows(release.ROOT / 'firmware/sdgoods/partitions.csv'),
                         rows(release.ROOT / 'specs/resource-storage/partitions-16m-proposal.csv'))

    def test_previous_15_mib_layout_is_migratable_but_not_publishable(self):
        release.validate_install_table(partition_table(release.PREVIOUS_15M))
        with self.assertRaisesRegex(ValueError, 'unexpected partition table'):
            release.table_layout(release.PREVIOUS_15M)

    def test_full_storage_uses_high_binding_and_32_mib_bounds(self):
        (self.folder / 'partition-table.bin').write_bytes(partition_table(release.PREVIOUS_SINGLE_24M))
        self.assertEqual(release.write_plan(self.folder)[-1], (0x1fe0000, 'resmeta.bin'))
        release.validate_write_plan(self.folder, release.write_plan(self.folder))
        with self.assertRaisesRegex(ValueError, '32 MiB layout'):
            release.validate_write_plan(self.folder, [(0x2000000, 'app.bin')])
        with self.assertRaisesRegex(ValueError, 'retained data'):
            release.validate_write_plan(self.folder, [(0x1e20000, 'app.bin')])

    def test_out_of_bounds_plan_fails_before_any_device_access(self):
        for at in (0xfff000, 0x1000000, 0x1fe0000, -4096):
            with self.subTest(address=at), patch.object(release, 'write_plan', return_value=[(at, 'app.bin')]), \
                 patch.object(self.flash, 'read_flash', side_effect=AssertionError('must not read device')):
                with self.assertRaisesRegex(ValueError, '16 MiB layout'):
                    self.run_install()

    def test_overlapping_or_unaligned_writes_are_rejected(self):
        for plan in ([(0x20000, 'app.bin'), (0x21000, 'app.bin')], [(0x20001, 'app.bin')]):
            with self.subTest(plan=plan), patch.object(release, 'write_plan', return_value=plan):
                with self.assertRaises(ValueError):
                    self.run_install()

    def test_short_read_fails_without_writes(self):
        self.flash.data[0] = b''
        with self.assertRaisesRegex(ValueError, 'short flash read'):
            self.run_install()
        self.assertEqual(self.flash.writes, [])

    def test_expanding_write_plan_over_retained_data_is_rejected(self):
        for at in (0x9000, 0x11000, 0xf20000, 0x8000):
            with self.subTest(address=hex(at)), patch.object(release, 'write_plan', return_value=[(at, 'app.bin')]):
                with self.assertRaisesRegex(ValueError, 'write overlaps retained data'):
                    self.run_install()
        self.assertEqual(self.flash.writes, [])

    def test_digest_transport_error_fails_without_writes(self):
        self.flash.flash_md5sum = lambda *_: (_ for _ in ()).throw(TimeoutError('serial timeout'))
        with self.assertRaises(TimeoutError):
            self.run_install()
        self.assertEqual(self.flash.writes, [])

    def test_invalid_digest_fails_without_writes(self):
        self.flash.flash_md5sum = lambda *_: 'not a digest'
        with self.assertRaisesRegex(ValueError, 'invalid device digest'):
            self.run_install()
        self.assertEqual(self.flash.writes, [])

    def test_missing_post_write_verification_is_rejected(self):
        self.change(0x20000)
        with self.assertRaisesRegex(ValueError, 'did not verify every written region'):
            self.run_install(lambda plan: self.flash.write(self.folder, plan, verify=False))

    def test_corrupt_post_write_digest_is_rejected(self):
        self.change(0x20000)
        def bad_write(plan):
            self.flash.write(self.folder, plan, verify=False)
            self.change(0x20000, 4500)
            self.flash.flash_md5sum(0x20000, len(self.flash.data[0x20000]))
        with self.assertRaisesRegex(ValueError, 'write digest mismatch'):
            self.run_install(bad_write)

    def test_write_exception_is_not_reported_as_success(self):
        self.change(0x20000)
        def fail(_):
            raise RuntimeError('write failed')
        with self.assertRaisesRegex(RuntimeError, 'write failed'):
            self.run_install(fail)
        self.assertNotIn('Summary:', self.output.getvalue())

    def test_write_alignment_is_included_in_checks(self):
        result = self.run_install()
        self.assertEqual(result['skipped_regions'], 7)
        self.assertIn((0x20000, 5012), self.flash.digests)

    def test_sdk_wrapper_keeps_headers_and_resets_only_after_all_checks(self):
        self.change(0x20000)
        def sdk_write(esp, args):
            for flag in ('flash_mode', 'flash_size', 'flash_freq'):
                self.assertEqual(getattr(args, flag), 'keep')
            self.assertFalse(args.erase_all or args.force or args.encrypt or args.no_stub)
            self.assertTrue(args.compress)
            esp.write(self.folder, [(at, Path(f.name).name) for at, f in args.addr_filename])
            self.assertFalse(esp.reset)
        sdk = types.SimpleNamespace(__version__='4.12.dev3', detect_chip=lambda _: self.flash, write_flash=sdk_write)
        def detect_size(esp):
            self.assertTrue(esp.stub_ready, 'attach flash before detecting capacity')
            return '32MB'
        cmds = types.SimpleNamespace(detect_flash_size=detect_size)
        with patch.dict('sys.modules', {'esptool': sdk, 'esptool.cmds': cmds}), patch.object(release, 'validate_bundle'):
            result = installer.install_device(self.folder, 'fake-port', 'ec:da:3b:67:10:5c')
        self.assertEqual(result['written_regions'], 1)
        self.assertTrue(self.flash.reset and self.flash.closed)

    def test_identity_security_and_write_failures_never_reset_to_application(self):
        for case in ('mac', 'size', 'chip', 'encrypted', 'secure', 'sdk'):
            with self.subTest(case=case):
                self.flash = Flash(self.folder)
                if case == 'mac': self.flash.mac = b'\x00' * 6
                if case == 'chip': self.flash.CHIP_NAME = 'ESP32'
                if case == 'encrypted': self.flash.encrypted = True
                if case == 'secure': self.flash.secure = True
                if case == 'sdk': self.change(0x20000)
                def fail(*_, **__): raise RuntimeError('SDK failure')
                sdk = types.SimpleNamespace(__version__='4.12.dev3', detect_chip=lambda _: self.flash, write_flash=fail)
                cmds = types.SimpleNamespace(detect_flash_size=lambda _: '16MB' if case == 'size' else '32MB')
                with patch.dict('sys.modules', {'esptool': sdk, 'esptool.cmds': cmds}), patch.object(release, 'validate_bundle'):
                    with self.assertRaises((ValueError, RuntimeError)):
                        installer.install_device(self.folder, 'fake', 'ec:da:3b:67:10:5c')
                self.assertTrue(self.flash.closed)
                self.assertFalse(self.flash.reset)

    def test_bad_bundle_never_connects_to_device(self):
        sdk = types.SimpleNamespace(__version__='4.12.dev3', detect_chip=lambda _: self.fail('must not connect'))
        cmds = types.SimpleNamespace(detect_flash_size=lambda _: '32MB')
        with patch.dict('sys.modules', {'esptool': sdk, 'esptool.cmds': cmds}), \
             patch.object(release, 'validate_bundle', side_effect=ValueError('invalid signature')):
            with self.assertRaisesRegex(ValueError, 'invalid signature'):
                installer.install_device(self.folder, 'fake', 'ec:da:3b:67:10:5c')


if __name__ == '__main__':
    unittest.main()
