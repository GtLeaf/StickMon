#pragma once

#include <cstdint>

#include "esp_err.h"
#include "esp_lcd_touch.h"

namespace AmoledV1 {

enum class TouchEventType : uint8_t {
    DOWN,
    MOVE,
    UP,
};

struct TouchEvent {
    TouchEventType type = TouchEventType::UP;
    int16_t x = 0;
    int16_t y = 0;
    uint32_t timestampMs = 0;
};

class TouchInput {
public:
    esp_err_t begin();
    bool poll(uint32_t nowMs, TouchEvent& event);

private:
    static void onInterrupt(esp_lcd_touch_handle_t touch);

    esp_lcd_touch_handle_t handle = nullptr;
    volatile bool interruptPending = false;
    bool pointerDown = false;
    uint32_t lastTouchMs = 0;
    int16_t lastX = 0;
    int16_t lastY = 0;
};

}  // namespace AmoledV1
