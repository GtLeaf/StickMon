#include "TouchInput.h"

#include <algorithm>
#include <cmath>

#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "bsp/touch.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace AmoledV2 {
namespace {

constexpr int DISPLAY_WIDTH = 368;
constexpr int DISPLAY_HEIGHT = 448;
constexpr uint32_t RELEASE_TIMEOUT_MS = 120;
constexpr char TAG[] = "TouchInput";
// V2 nine-point DOWN fit; see README.md. V1 uses a different controller/fit.
constexpr float CAL_X_SCALE = 1.1179618f;
constexpr float CAL_X_OFFSET = -20.711235f;
constexpr float CAL_Y_SCALE = 1.0494687f;
constexpr float CAL_Y_OFFSET = -21.692282f;

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

    ownerTask_ = xTaskGetCurrentTaskHandle();
    result = esp_lcd_touch_register_interrupt_callback_with_data(
        handle, onInterrupt, this);
    if (result != ESP_OK) return result;
    const gpio_num_t pin = handle->config.int_gpio_num;
    wakeReady_ = pin != GPIO_NUM_NC &&
        gpio_wakeup_enable(pin, handle->config.levels.interrupt
            ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL) == ESP_OK &&
        esp_sleep_enable_gpio_wakeup() == ESP_OK;
    ESP_LOGI(TAG, "Touch light-sleep wake %s gpio=%d",
             wakeReady_ ? "ready" : "unavailable", static_cast<int>(pin));
    return ESP_OK;
}

void TouchInput::onInterrupt(esp_lcd_touch_handle_t touch) {
    auto* input = static_cast<TouchInput*>(touch->config.user_data);
    if (!input) return;
    // GPIO wake requires a level interrupt. Mask it until the worker observes
    // deassertion, otherwise a held finger can spin inside the ISR forever.
    gpio_intr_disable(touch->config.int_gpio_num);
    input->interruptMasked_ = true;
    input->interruptPending = true;
    if (input->ownerTask_) {
        BaseType_t wake = pdFALSE;
        vTaskNotifyGiveFromISR(static_cast<TaskHandle_t>(input->ownerTask_), &wake);
        if (wake == pdTRUE) portYIELD_FROM_ISR();
    }
}

void TouchInput::waitForInput(uint32_t durationMs) {
    if (pointerDown || interruptMasked_) durationMs = std::min<uint32_t>(durationMs, 20);
    if (interruptPending || durationMs == 0) return;
    ulTaskNotifyTake(pdTRUE, std::max<TickType_t>(1, pdMS_TO_TICKS(durationMs)));
}

bool TouchInput::poll(uint32_t nowMs, TouchEvent& event) {
    if (!handle) return false;
    if (interruptMasked_) {
        if (gpio_get_level(handle->config.int_gpio_num) !=
            static_cast<int>(handle->config.levels.interrupt)) {
            interruptMasked_ = false;
            gpio_intr_enable(handle->config.int_gpio_num);
        } else {
            // Some controllers hold INT until release. Read at the bounded
            // worker cadence instead of enabling a continuously asserted IRQ.
            interruptPending = true;
        }
    }

    if (!interruptPending) {
        if (pointerDown && nowMs - lastTouchMs >= RELEASE_TIMEOUT_MS) {
            pointerDown = false;
            event = {TouchEventType::UP, lastX, lastY, lastRawX, lastRawY, nowMs};
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
        // Keep the last sensor sample even if rounding suppresses a MOVE.
        lastRawX = rawX;
        lastRawY = rawY;
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
        event = {TouchEventType::UP, lastX, lastY, lastRawX, lastRawY, nowMs};
        return true;
    }
    return false;
}

}  // namespace AmoledV2
