#include "TouchInput.h"

#include <algorithm>
#include <cmath>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "bsp/touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace AmoledV2 {
namespace {

constexpr int DISPLAY_WIDTH = 368;
constexpr int DISPLAY_HEIGHT = 448;
constexpr uint32_t RELEASE_TIMEOUT_MS = 120;
constexpr char TAG[] = "TouchInput";
constexpr float CAL_X_SCALE = 0.8985547f;
constexpr float CAL_X_OFFSET = 15.9703f;
constexpr float CAL_Y_SCALE = 0.8800800f;
constexpr float CAL_Y_OFFSET = 22.6573f;

int16_t toDisplayCoordinate(uint16_t value, int limit, float scale, float offset) {
    const int calibrated = static_cast<int>(std::lround(value * scale + offset));
    return static_cast<int16_t>(std::clamp<int>(calibrated, 0, limit - 1));
}


}  // namespace

esp_err_t TouchInput::begin() {
    bsp_touch_config_t config{};
    esp_err_t result = bsp_touch_new(&config, &handle);
    if (result != ESP_OK) return result;

    ESP_LOGI(TAG, "V2 touch initialized through BSP auto-detection");

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
            event = {TouchEventType::UP, lastX, lastY, lastX, lastY, nowMs};
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
        int16_t x = toDisplayCoordinate(point.x, DISPLAY_WIDTH, CAL_X_SCALE, CAL_X_OFFSET);
        int16_t y = toDisplayCoordinate(point.y, DISPLAY_HEIGHT, CAL_Y_SCALE, CAL_Y_OFFSET);
        int16_t rawX = static_cast<int16_t>(point.x);
        int16_t rawY = static_cast<int16_t>(point.y);
        if (!pointerDown) {
            pointerDown = true;
            lastX = x;
            lastY = y;
            event = {TouchEventType::DOWN, x, y, rawX, rawY, nowMs};
            return true;
        }
        if (x != lastX || y != lastY) {
            lastX = x;
            lastY = y;
            event = {TouchEventType::MOVE, x, y, rawX, rawY, nowMs};
            return true;
        }
        return false;
    }

    if (pointerDown) {
        pointerDown = false;
        event = {TouchEventType::UP, lastX, lastY, lastX, lastY, nowMs};
        return true;
    }
    return false;
}

}  // namespace AmoledV2
