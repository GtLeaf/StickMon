#include "TouchInput.h"
#include "bsp/touch.h"
#include "bsp/esp32_s3_touch_amoled_1_8.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
esp_lcd_touch_t sensor{};
void (*interruptCallback)(esp_lcd_touch_handle_t) = nullptr;
esp_lcd_touch_point_data_t reading{};
uint8_t readingCount = 0;
bool ftPresent = true, busReady = true;
}

i2c_master_bus_handle_t bsp_i2c_get_handle() { return busReady ? &sensor : nullptr; }
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address, int timeout) {
    assert(bus == &sensor && timeout == 100);
    assert(address == 0x38);
    return ftPresent ? ESP_OK : ESP_ERR_NOT_FOUND;
}
esp_err_t bsp_touch_new(const bsp_touch_config_t*, esp_lcd_touch_handle_t* handle) {
    *handle = &sensor;
    return ESP_OK;
}
esp_err_t esp_lcd_touch_register_interrupt_callback_with_data(
    esp_lcd_touch_handle_t handle, void (*callback)(esp_lcd_touch_handle_t), void* data) {
    handle->config.user_data = data;
    interruptCallback = callback;
    return ESP_OK;
}
esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t) { return ESP_OK; }
esp_err_t esp_lcd_touch_get_data(esp_lcd_touch_handle_t,
    esp_lcd_touch_point_data_t* point, uint8_t* count, uint8_t) {
    *point = reading;
    *count = readingCount;
    return ESP_OK;
}

namespace {
uint32_t notifications = 0, waitCalls = 0, waitedMs = 0, yields = 0;
int interruptLevel = 1;
int enabledInterrupts = 0, disabledInterrupts = 0;
bool wakeFailure = false;
}
TaskHandle_t xTaskGetCurrentTaskHandle() { return &sensor; }
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t* wake) {
    assert(task == &sensor); ++notifications; *wake = pdTRUE;
}
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait) {
    assert(clear == pdTRUE); ++waitCalls; waitedMs = wait;
    uint32_t pending = notifications; notifications = 0; return pending;
}
void vTaskDelay(TickType_t) {}
void testYield() { ++yields; }
esp_err_t gpio_wakeup_enable(gpio_num_t pin, int level) {
    assert(pin == 21 && level == GPIO_INTR_LOW_LEVEL);
    return wakeFailure ? -1 : ESP_OK;
}
esp_err_t esp_sleep_enable_gpio_wakeup() { return ESP_OK; }
esp_err_t gpio_intr_disable(gpio_num_t) { ++disabledInterrupts; return ESP_OK; }
esp_err_t gpio_intr_enable(gpio_num_t) { ++enabledInterrupts; return ESP_OK; }
int gpio_get_level(gpio_num_t) { return interruptLevel; }

int main() {
    using namespace AmoledV1;
    TouchInput input;
    TouchEvent event;
    assert(!input.poll(0, event));
    assert(input.begin() == ESP_OK);
    assert(input.canSleep());
    input.waitForInput(100);
    assert(waitedMs == 100);
    uint32_t now = 100;
    auto point = [&](uint16_t x, uint16_t y) {
        reading = {x, y};
        readingCount = 1;
        interruptCallback(&sensor);
        return input.poll(now++, event);
    };
    auto release = [&]() {
        readingCount = 0;
        interruptCallback(&sensor);
        assert(input.poll(now++, event));
        assert(event.type == TouchEventType::UP);
    };

    // Physical V1 / FT3168 session, DOWN samples ordered by target.
    const int samples[][6] = {
        {44,53,24,32,56,69}, {187,42,184,32,184,60}, {329,53,344,32,312,69},
        {43,238,24,224,55,232}, {180,240,184,224,178,234}, {326,237,344,224,309,231},
        {35,405,24,416,47,379}, {179,421,184,416,177,393}, {325,418,344,416,308,391},
    };
    double oldSum = 0, sum = 0, maxError = 0;
    for (const auto& s : samples) {
        assert(point(s[0], s[1]));
        assert(event.type == TouchEventType::DOWN);
        assert(event.rawX == s[0] && event.rawY == s[1]);
        const double error = std::hypot(event.x - s[2], event.y - s[3]);
        sum += error;
        maxError = std::max(maxError, error);
        oldSum += std::hypot(s[4] - s[2], s[5] - s[3]);
        assert(error < 15);
        const auto down = event;
        release();
        assert(event.x == down.x && event.y == down.y);
        assert(!input.poll(now++, event));
    }
    assert(sum / 9 < 7 && sum < oldSum / 4);
    std::printf("V1 FT3168 replay: mean %.1f -> %.1f px, max %.1f px\n",
                oldSum / 9, sum / 9, maxError);

    // Movement and timeout release must use the same mapped/raw sample pair.
    assert(point(180, 240) && event.type == TouchEventType::DOWN);
    assert(point(187, 250) && event.type == TouchEventType::MOVE);
    const auto moved = event;
    assert(!input.poll(moved.timestampMs + 119, event));
    assert(input.poll(moved.timestampMs + 120, event));
    assert(event.type == TouchEventType::UP);
    assert(event.x == moved.x && event.y == moved.y);
    now = moved.timestampMs + 121;

    // Sensor extremes are clamped to native display bounds.
    assert(point(0, 0) && event.type == TouchEventType::DOWN);
    assert(event.x == 0 && event.y == 0);
    assert(!point(1, 1));
    release();
    assert(event.x == 0 && event.y == 0);
    assert(point(4095, 4095));
    assert(event.x == 367 && event.y == 447);
    release();

    // The correction must preserve ordering and keep every panel coordinate valid.
    int previous = -1;
    for (uint16_t x = 0; x < 368; ++x) {
        assert(point(x, 224));
        assert(event.x >= previous && event.x >= 0 && event.x < 368);
        previous = event.x;
        release();
    }
    previous = -1;
    for (uint16_t y = 0; y < 448; ++y) {
        assert(point(184, y));
        assert(event.y >= previous && event.y >= 0 && event.y < 448);
        previous = event.y;
        release();
    }
    // A held low wake pin is masked until deassertion. The worker polls at
    // 20ms, so long holds cannot cause an ISR storm or a busy spin.
    interruptLevel = 0;
    assert(point(184, 224));
    assert(!input.canSleep());
    const auto disabled = disabledInterrupts;
    const auto enabled = enabledInterrupts;
    input.waitForInput(100);
    assert(waitedMs == 20);
    assert(!input.poll(now + 20, event));
    assert(disabledInterrupts == disabled && enabledInterrupts == enabled);
    interruptLevel = 1;
    now += 21;
    release();
    assert(input.canSleep());
    assert(enabledInterrupts > enabled);
    assert(yields > 0 && notifications > 0);
    const auto waitsBefore = waitCalls;
    interruptCallback(&sensor);
    input.waitForInput(100);
    assert(waitCalls == waitsBefore); // Pending input never waits for a frame.
    assert(!input.canSleep());
    input.poll(now++, event);
    assert(input.canSleep());
    TouchInput missingWake;
    wakeFailure = true;
    assert(missingWake.begin() == ESP_OK);
    assert(!missingWake.canSleep()); // Normal touch works; shallow sleep stays disabled.


    busReady = false;
    TouchInput unavailable;
    assert(unavailable.begin() == ESP_FAIL);
    busReady = true;
    ftPresent = false;
    assert(unavailable.begin() == ESP_ERR_NOT_FOUND);
}
