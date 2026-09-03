#include "TouchInput.h"

#include <algorithm>

#include "bsp/esp32_s3_touch_amoled_1_75c.h"
#include "bsp/touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace AmoledV2 {
namespace {

constexpr int PHYSICAL_SCALE = 2;
constexpr int LOGICAL_WIDTH = 184;
constexpr int LOGICAL_HEIGHT = 224;
constexpr int PHYSICAL_OFFSET_X = 49;
constexpr int PHYSICAL_OFFSET_Y = 9;
constexpr uint32_t RELEASE_TIMEOUT_MS = 120;
constexpr char TAG[] = "TouchInput";

int16_t toLogical(uint16_t value, int offset, int limit) {
    int adjusted = static_cast<int>(value) - offset;
    return static_cast<int16_t>(
        std::clamp<int>(adjusted / PHYSICAL_SCALE, 0, limit - 1));
}

}  // namespace

esp_err_t TouchInput::begin() {
    bsp_display_cfg_t config{};
    config.touch_flags.swap_xy = 0;
    config.touch_flags.mirror_x = 1;
    config.touch_flags.mirror_y = 1;
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
        int16_t x = toLogical(point.x, PHYSICAL_OFFSET_X, LOGICAL_WIDTH);
        int16_t y = toLogical(point.y, PHYSICAL_OFFSET_Y, LOGICAL_HEIGHT);
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

}  // namespace AmoledV2
