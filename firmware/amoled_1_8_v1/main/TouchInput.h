#pragma once

#include <cstdint>
#include "TouchEvent.h"

#include "esp_err.h"
#include "esp_lcd_touch.h"

namespace AmoledV1 {

// Touch events use native panel pixels: x in [0,368), y in [0,448).
class TouchInput {
public:
    esp_err_t begin();
    bool poll(uint32_t nowMs, TouchEvent& event);
    void waitForInput(uint32_t durationMs);
    bool canSleep() const { return wakeReady_ && !pointerDown && !interruptPending && !interruptMasked_; }

private:
    static void onInterrupt(esp_lcd_touch_handle_t touch);

    esp_lcd_touch_handle_t handle = nullptr;
    volatile bool interruptPending = false;
    volatile bool interruptMasked_ = false;
    void* ownerTask_ = nullptr;
    bool wakeReady_ = false;
    bool pointerDown = false;
    uint32_t lastTouchMs = 0;
    int16_t lastX = 0;
    int16_t lastY = 0;
};

}  // namespace AmoledV1
