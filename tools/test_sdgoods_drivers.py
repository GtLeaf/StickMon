#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile actual native-board audio/radio/ADC against deterministic hardware fakes."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory() as directory:
        base = Path(directory)
        for name in ('driver/i2s_std.h', 'driver/gpio.h', 'freertos/FreeRTOS.h',
                     'freertos/semphr.h', 'freertos/task.h', 'freertos/queue.h',
                     'board_pins.h', 'esp_now.h', 'esp_event.h', 'esp_netif.h', 'esp_wifi.h',
                     'esp_adc/adc_oneshot.h', 'esp_adc/adc_cali.h', 'esp_adc/adc_cali_scheme.h'):
            path = base / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#include "sdgoods_driver_fakes.h"\n')
        binary = base / 'sdgoods-drivers'
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', '-I'+directory, '-Itools', '-Isrc',
                        '-Ifirmware/sdgoods/main', 'tools/sdgoods_drivers_host.cpp',
                        'firmware/sdgoods/main/Audio.cpp', 'firmware/sdgoods/main/PeerRadio.cpp',
                        'firmware/sdgoods/main/Battery.cpp',
                        'src/core/PcmMixer.cpp', '-o', str(binary)], cwd=ROOT, check=True)
        subprocess.run([str(binary)], check=True)

if __name__ == '__main__':
    main()
