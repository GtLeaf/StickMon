#!/usr/bin/env python3
"""Verify packed dirty strips, DMA ownership, draining and timeout handling."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp)
    (root/'freertos').mkdir()
    (root/'esp_err.h').write_text('using esp_err_t=int;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_ARG 1\n#define ESP_ERR_TIMEOUT 2\n')
    (root/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdTRUE 1\n#define pdMS_TO_TICKS(x) (x)\n')
    (root/'freertos/semphr.h').write_text('#pragma once\nusing SemaphoreHandle_t=void*;\nint xSemaphoreTake(SemaphoreHandle_t,unsigned);\n')
    binary=root/'transfer'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root),
        '-Ifirmware/common','tools/rgb565_transfer_host.cpp','firmware/common/Rgb565Transfer.cpp',
        '-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary)],check=True)
print('RGB565 transfer ownership and dirty rectangle checks passed')
