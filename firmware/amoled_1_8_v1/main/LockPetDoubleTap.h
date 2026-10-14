#pragma once

#include <cstdint>

namespace AmoledV1 {

// Shared by both boards. A tap must end on the pet without dragging/holding.
class LockPetDoubleTap {
public:
    static constexpr uint32_t WINDOW_MS = 500;
    static constexpr uint32_t MAX_HOLD_MS = 350;
    static constexpr int MAX_MOVE_PIXELS = 20;

    void reset() { pressed_ = false; waiting_ = false; }

    void press(uint32_t nowMs, int x, int y, bool hitsPet) {
        if (!hitsPet) {
            reset();
            return;
        }
        if (waiting_ && nowMs - firstPressMs_ > WINDOW_MS) waiting_ = false;
        pressed_ = true;
        pressMs_ = nowMs;
        pressX_ = x;
        pressY_ = y;
    }

    void move(int x, int y, bool hitsPet) {
        const int dx = x - pressX_;
        const int dy = y - pressY_;
        if (!hitsPet || dx * dx + dy * dy > MAX_MOVE_PIXELS * MAX_MOVE_PIXELS) {
            reset();
        }
    }

    bool release(uint32_t nowMs, int x, int y, bool hitsPet) {
        move(x, y, hitsPet);
        if (!pressed_ || nowMs - pressMs_ > MAX_HOLD_MS) {
            reset();
            return false;
        }
        pressed_ = false;
        if (waiting_ && pressMs_ - firstPressMs_ <= WINDOW_MS) {
            reset();
            return true;
        }
        firstPressMs_ = pressMs_;
        waiting_ = true;
        return false;
    }

private:
    bool pressed_ = false;
    bool waiting_ = false;
    uint32_t pressMs_ = 0;
    uint32_t firstPressMs_ = 0;
    int pressX_ = 0;
    int pressY_ = 0;
};

}  // namespace AmoledV1
