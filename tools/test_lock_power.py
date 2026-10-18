#!/usr/bin/env python3
"""Lock scheduling and PM ownership across input bursts, failures and wraparound."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class LockPowerTests(unittest.TestCase):
    def test_runtime_and_scheduling(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            (base / 'sdkconfig.h').write_text('#define CONFIG_PM_ENABLE 1\n')
            (base / 'esp_log.h').write_text(
                '#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n'
                '#define ESP_LOGE(...) ((void)0)\n')
            (base / 'esp_pm.h').write_text('''#pragma once
#include <cassert>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_PM_CPU_FREQ_MAX = 1, ESP_PM_NO_LIGHT_SLEEP = 2;
struct TestLock { int type = 0; int count = 0; };
using esp_pm_lock_handle_t = TestLock*;
struct esp_pm_config_t { int max_freq_mhz, min_freq_mhz; bool light_sleep_enable; };
inline TestLock testLocks[4];
inline int testLockCount = 0;
inline bool testFailCreate = false;
inline esp_pm_config_t testConfig{};
inline const char* esp_err_to_name(int) { return "test"; }
inline int esp_pm_lock_create(int type, int, const char*, TestLock** lock) {
    if (testFailCreate) return -1;
    *lock = &testLocks[testLockCount++]; (*lock)->type = type; return ESP_OK;
}
inline int esp_pm_lock_acquire(TestLock* lock) { ++lock->count; return ESP_OK; }
inline int esp_pm_lock_release(TestLock* lock) {
    assert(lock && lock->count > 0); --lock->count; return ESP_OK;
}
inline int esp_pm_configure(const esp_pm_config_t* config) {
    testConfig = *config; return ESP_OK;
}
''')
            binary = base / 'lock-power'
            subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I' + directory, '-Ifirmware/common', '-Ifirmware/amoled_1_8_v1/main',
                            'tools/lock_power_host.cpp', '-o', str(binary)],
                           cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    unittest.main()
