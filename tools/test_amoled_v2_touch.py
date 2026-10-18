#!/usr/bin/env python3
"""Replay sensor samples through the real V2 driver without serial access."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class V2TouchTests(unittest.TestCase):
    def test_calibration_replay_boundaries_movement_and_release(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            headers = {
                "esp_err.h": "#pragma once\nusing esp_err_t = int;\nconstexpr int ESP_OK = 0;\n",
                "esp_log.h": "#define ESP_LOGI(tag, ...) ((void)(tag))\n",
                "freertos/FreeRTOS.h": "#pragma once\n#include <cstdint>\n"
                "using TickType_t = uint32_t; using BaseType_t = int;\n"
                "constexpr int pdFALSE=0, pdTRUE=1;\n#define pdMS_TO_TICKS(ms) (ms)\n",
                "freertos/task.h": "#pragma once\n#include \"freertos/FreeRTOS.h\"\n"
                "using TaskHandle_t = void*;\nTaskHandle_t xTaskGetCurrentTaskHandle();\n"
                "void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t*);\n"
                "uint32_t ulTaskNotifyTake(BaseType_t, TickType_t);\n"
                "void testYield();\n#define portYIELD_FROM_ISR() testYield()\n",
                "driver/gpio.h": "#pragma once\n#include \"esp_err.h\"\n"
                "using gpio_num_t = int;\nconstexpr int GPIO_NUM_NC=-1, GPIO_INTR_HIGH_LEVEL=1, GPIO_INTR_LOW_LEVEL=0;\n"
                "esp_err_t gpio_wakeup_enable(gpio_num_t, int);\n"
                "esp_err_t gpio_intr_disable(gpio_num_t);\nesp_err_t gpio_intr_enable(gpio_num_t);\n"
                "int gpio_get_level(gpio_num_t);\n",
                "esp_sleep.h": "#pragma once\n#include \"esp_err.h\"\nesp_err_t esp_sleep_enable_gpio_wakeup();\n",
                "bsp/esp32_s3_touch_amoled_1_8.h": "#pragma once\n",
                "bsp/touch.h": "#pragma once\n#include \"esp_lcd_touch.h\"\n"
                "struct bsp_touch_config_t {};\n"
                "esp_err_t bsp_touch_new(const bsp_touch_config_t*, esp_lcd_touch_handle_t*);\n",
                "esp_lcd_touch.h": "#pragma once\n#include <cstdint>\n#include \"esp_err.h\"\n"
                "struct esp_lcd_touch_t { struct { void* user_data; int int_gpio_num=21; struct { unsigned interrupt=0; } levels; } config; };\n"
                "using esp_lcd_touch_handle_t = esp_lcd_touch_t*;\n"
                "struct esp_lcd_touch_point_data_t { uint16_t x; uint16_t y; };\n"
                "esp_err_t esp_lcd_touch_register_interrupt_callback_with_data(\n"
                "  esp_lcd_touch_handle_t, void (*)(esp_lcd_touch_handle_t), void*);\n"
                "esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t);\n"
                "esp_err_t esp_lcd_touch_get_data(esp_lcd_touch_handle_t,\n"
                "  esp_lcd_touch_point_data_t*, uint8_t*, uint8_t);\n",
            }
            for name, content in headers.items():
                path = base / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            binary = base / "touch"
            subprocess.run([
                "c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + directory, "-Ifirmware/amoled_1_8_v2/main",
                "tools/amoled_v2_touch_host.cpp",
                "firmware/amoled_1_8_v2/main/TouchInput.cpp", "-o", str(binary),
            ], cwd=ROOT, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
