#pragma once

#include <algorithm>
#include <cmath>
#include "AmoledGeometry.h"

namespace AmoledV1 {
namespace TouchTest {

struct Point { int x = 0; int y = 0; };
inline constexpr Point TARGETS[] = {
    {24, 32}, {184, 32}, {344, 32},
    {24, 224}, {184, 224}, {344, 224},
    {24, 416}, {184, 416}, {344, 416},
};
inline constexpr int COUNT = 9;
inline constexpr AmoledUi::Rect CLEAR_RECT{60, 340, 116, 48};
inline constexpr AmoledUi::Rect BACK_RECT{192, 340, 116, 48};
enum class Result { NONE, SAMPLE, CLEAR, BACK };
struct Sample {
    Point down;
    Point up;
    Point minimum;
    Point maximum;
};

inline int controlAt(Point point) {
    if (CLEAR_RECT.contains(point.x, point.y)) return 0;
    if (BACK_RECT.contains(point.x, point.y)) return 1;
    return -1;
}

struct State {
    Sample samples[COUNT]{};
    Sample pending{};
    Point last{};
    int count = 0;
    bool pressed = false;
    bool lastValid = false;
    bool lastWasUp = false;

    void down(Point point) {
        pending = {point, point, point, point};
        last = point;
        pressed = lastValid = true;
        lastWasUp = false;
    }
    void move(Point point) {
        if (!pressed) return;
        last = point;
        pending.minimum.x = std::min(pending.minimum.x, point.x);
        pending.minimum.y = std::min(pending.minimum.y, point.y);
        pending.maximum.x = std::max(pending.maximum.x, point.x);
        pending.maximum.y = std::max(pending.maximum.y, point.y);
    }
    Result up(Point point) {
        if (!pressed) return Result::NONE;
        move(point);
        pending.up = point;
        pressed = false;
        lastWasUp = true;
        const int control = controlAt(pending.down);
        if (control >= 0) {
            if (control != controlAt(point)) return Result::NONE;
            if (control == 1) return Result::BACK;
            *this = State{};
            return Result::CLEAR;
        }
        // The expected point is selected by sequence, never by hit testing.
        if (count == COUNT) return Result::NONE;
        samples[count++] = pending;
        return Result::SAMPLE;
    }
    void statistics(float& dx, float& dy, float& maxError) const {
        dx = dy = maxError = 0;
        for (int i = 0; i < count; ++i) {
            const int x = samples[i].down.x - TARGETS[i].x;
            const int y = samples[i].down.y - TARGETS[i].y;
            dx += x;
            dy += y;
            maxError = std::max(maxError, std::sqrt(float(x * x + y * y)));
        }
        if (count) { dx /= count; dy /= count; }
    }
};
}  // namespace TouchTest
}  // namespace AmoledV1
