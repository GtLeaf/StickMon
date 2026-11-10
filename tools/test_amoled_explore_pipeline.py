#!/usr/bin/env python3
"""Exercise the production display worker and framebuffer ownership on a host."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    (root / 'freertos').mkdir()
    (root / 'esp_err.h').write_text('''#pragma once
using esp_err_t=int;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_TIMEOUT 2
''')
    (root / 'esp_timer.h').write_text('''#pragma once
#include <cstdint>
int64_t esp_timer_get_time();
''')
    (root / 'freertos/FreeRTOS.h').write_text('''#pragma once
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffffU
#define pdMS_TO_TICKS(x) (x)
using BaseType_t=int;
using TickType_t=unsigned;
''')
    (root / 'freertos/semphr.h').write_text('''#pragma once
using SemaphoreHandle_t=void*;
int xSemaphoreTake(SemaphoreHandle_t,unsigned);
int xSemaphoreGive(SemaphoreHandle_t);
SemaphoreHandle_t xSemaphoreCreateBinary();
SemaphoreHandle_t xSemaphoreCreateMutex();
void vSemaphoreDelete(SemaphoreHandle_t);
''')
    (root / 'freertos/queue.h').write_text('''#pragma once
#include <cstddef>
using QueueHandle_t=void*;
QueueHandle_t xQueueCreate(unsigned,size_t);
void vQueueDelete(QueueHandle_t);
int xQueueSend(QueueHandle_t,const void*,unsigned);
int xQueueReceive(QueueHandle_t,void*,unsigned);
''')
    (root / 'freertos/task.h').write_text('''#pragma once
using TaskHandle_t=void*;
int xTaskCreatePinnedToCore(void (*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*,int);
TaskHandle_t xTaskGetCurrentTaskHandle();
''')
    binary = root / 'pipeline'
    subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-pthread',
                    '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer', '-I'+str(root), '-Ifirmware/common',
                    '-Ifirmware/amoled_1_8_v2/main',
                    'tools/amoled_explore_pipeline_host.cpp',
                    'firmware/common/Rgb565Transfer.cpp', '-o', str(binary)],
                   cwd=ROOT, check=True)
    subprocess.run([str(binary)], check=True, timeout=30)
print('Explore pipeline: overlap, dirty-frame coherence, panel calls, drain and failure checks passed')
