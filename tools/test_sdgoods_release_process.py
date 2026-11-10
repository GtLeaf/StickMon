#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check installer progress reporting and preservation of subprocess failures."""
import contextlib
import io
import subprocess
import sys
import unittest

import sdgoods_release as release


class InstallCommandTests(unittest.TestCase):
    def run_child(self, code):
        display=io.StringIO()
        with contextlib.redirect_stdout(display):
            output=release.run_install_command([sys.executable,'-c',code],'Test write',.05)
        return output,display.getvalue()

    def test_silent_final_block_reports_wait_then_completes(self):
        output,display=self.run_child(
            "import time; print('Writing at 0x00a8ce84... (100 %)',flush=True); "
            "time.sleep(.3); print('Hash of data verified.')")
        self.assertIn('waiting for esptool',display)
        self.assertIn('verification are still pending',display)
        self.assertIn('Hash of data verified.',output)
        self.assertNotIn('[install]',output)

    def test_success_without_output(self):
        output,display=self.run_child('pass')
        self.assertEqual(output,'')
        self.assertIn('[install] Test write',display)

    def test_failure_retains_exit_code_and_stderr(self):
        display=io.StringIO()
        with contextlib.redirect_stdout(display):
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                release.run_install_command(
                    [sys.executable,'-c',"import sys; print('failure detail',file=sys.stderr); sys.exit(7)"],
                    'Test failure',.05)
        self.assertEqual(raised.exception.returncode,7)
        self.assertIn('failure detail',raised.exception.output)


if __name__=='__main__':unittest.main()
