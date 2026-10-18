#pragma once

#include <cstdint>

// Portable event model. Device drivers and the web adapter use panel pixels.
namespace AmoledV1 {
enum class TouchEventType : uint8_t { DOWN, MOVE, UP };

struct TouchEvent {
    TouchEventType type = TouchEventType::UP;
    int16_t x = 0;
    int16_t y = 0;
    int16_t rawX = 0;
    int16_t rawY = 0;
    uint32_t timestampMs = 0;
};
}  // namespace AmoledV1
