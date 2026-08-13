#include "TouchInput.h"

#include <algorithm>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "bsp/touch.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace AmoledV1 {
namespace {

constexpr int PHYSICAL_SCALE = 2;
constexpr int LOGICAL_WIDTH = 184;
constexpr int LOGICAL_HEIGHT = 224;
constexpr uint8_t FT3168_ADDRESS = 0x38;
constexpr int PROBE_ATTEMPTS = 10;
constexpr uint32_t RELEASE_TIMEOUT_MS = 120;
constexpr char TAG[] = "TouchInput";

int16_t toLogical(uint16_t value, int limit) {
    return static_cast<int16_t>(
        std::clamp<int>(value / PHYSICAL_SCALE, 0, limit - 1));
}

}  // namespace

esp_err_t TouchInput::begin() {
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return ESP_FAIL;

    esp_err_t probeResult = ESP_ERR_NOT_FOUND;
    for (int attempt = 1; attempt <= PROBE_ATTEMPTS; ++attempt) {
        probeResult = i2c_master_probe(bus, FT3168_ADDRESS,
                                       pdMS_TO_TICKS(100));
        if (probeResult == ESP_OK) {
            ESP_LOGI(TAG, "FT3168 detected at I2C address 0x%02x",
                     FT3168_ADDRESS);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (probeResult != ESP_OK) {
        ESP_LOGE(TAG, "FT3168 not detected at I2C address 0x%02x: %s",
                 FT3168_ADDRESS, esp_err_to_name(probeResult));
        return probeResult;
    }

    bsp_touch_config_t config{};
    esp_err_t result = bsp_touch_new(&config, &handle);
    if (result != ESP_OK) return result;

    return esp_lcd_touch_register_interrupt_callback_with_data(
        handle, onInterrupt, this);
}

void TouchInput::onInterrupt(esp_lcd_touch_handle_t touch) {
    auto* input = static_cast<TouchInput*>(touch->config.user_data);
    if (input) input->interruptPending = true;
}

bool TouchInput::poll(uint32_t nowMs, TouchEvent& event) {
    if (!handle) return false;

    if (!interruptPending) {
        if (pointerDown && nowMs - lastTouchMs >= RELEASE_TIMEOUT_MS) {
            pointerDown = false;
            event = {TouchEventType::UP, lastX, lastY, nowMs};
            return true;
        }
        return false;
    }

    interruptPending = false;
    if (esp_lcd_touch_read_data(handle) != ESP_OK) return false;

    esp_lcd_touch_point_data_t point{};
    uint8_t pointCount = 0;
    if (esp_lcd_touch_get_data(handle, &point, &pointCount, 1) != ESP_OK) {
        return false;
    }

    if (pointCount > 0) {
        lastTouchMs = nowMs;
        int16_t x = toLogical(point.x, LOGICAL_WIDTH);
        int16_t y = toLogical(point.y, LOGICAL_HEIGHT);
        if (!pointerDown) {
            pointerDown = true;
            lastX = x;
            lastY = y;
            event = {TouchEventType::DOWN, x, y, nowMs};
            return true;
        }
        if (x != lastX || y != lastY) {
            lastX = x;
            lastY = y;
            event = {TouchEventType::MOVE, x, y, nowMs};
            return true;
        }
        return false;
    }

    if (pointerDown) {
        pointerDown = false;
        event = {TouchEventType::UP, lastX, lastY, nowMs};
        return true;
    }
    return false;
}

}  // namespace AmoledV1
