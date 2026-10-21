#pragma once

#include <cstdint>
#include "platform/api/PlatformServices.h"

// A click belongs to the same target at both edges. A drag or cancellation
// cannot turn into a click even when the pointer returns to its origin.
class TouchTap {
public:
    using Event = Platform::IInputDevice::TouchEvent;
    using Type = Platform::IInputDevice::TouchType;
    void cancel() { target_ = -1; }
    int update(const Event& event, int target) {
        if (event.type == Type::CANCEL) { cancel(); return -1; }
        if (event.type == Type::DOWN) {
            x_ = event.x; y_ = event.y; started_ = event.timestampMs;
            target_ = target;
            return -1;
        }
        const int dx = event.x - x_, dy = event.y - y_;
        if (dx * dx + dy * dy > 100 ||
            event.timestampMs - started_ > 1000) cancel();
        if (event.type != Type::UP) return -1;
        const int result = target_ >= 0 && target == target_ ? target_ : -1;
        cancel();
        return result;
    }
private:
    int target_ = -1;
    int16_t x_ = 0, y_ = 0;
    uint32_t started_ = 0;
};
