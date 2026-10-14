#!/usr/bin/env python3
"""Exercise the shared board driver through a simulated IDF I2C bus."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Qmi8658Tests(unittest.TestCase):
    def test_sampling_signed_units_standby_and_bus_failures(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            (base / "driver").mkdir()
            (base / "driver/i2c_master.h").write_text(
                "#pragma once\n#include <cstdint>\n#include <cstddef>\n"
                "using esp_err_t = int;\nconstexpr int ESP_OK = 0, ESP_FAIL = -1;\n"
                "using i2c_master_bus_handle_t = void*;\n"
                "using i2c_master_dev_handle_t = void*;\n"
                "constexpr int I2C_ADDR_BIT_LEN_7 = 0;\n"
                "struct i2c_device_config_t { int dev_addr_length; "
                "uint16_t device_address; uint32_t scl_speed_hz; };\n"
                "esp_err_t i2c_master_probe(void*, uint16_t, int);\n"
                "esp_err_t i2c_master_bus_add_device(void*, const i2c_device_config_t*, void**);\n"
                "esp_err_t i2c_master_bus_rm_device(void*);\n"
                "esp_err_t i2c_master_transmit(void*, const uint8_t*, size_t, int);\n"
                "esp_err_t i2c_master_transmit_receive(void*, const uint8_t*, size_t, "
                "uint8_t*, size_t, int);\n")
            binary = str(base / "qmi8658")
            subprocess.run([
                "c++", "-std=c++17", "-I" + directory, "-Ifirmware/common",
                "tools/qmi8658_host.cpp", "firmware/common/Qmi8658.cpp",
                "-o", binary,
            ], cwd=ROOT, check=True)
            subprocess.run([binary], check=True)


if __name__ == "__main__":
    unittest.main()
