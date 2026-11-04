#pragma once

#include <algorithm>
#include <cmath>
#include "ui/UiProfile.h"

namespace Ui::TouchTest {

// Diagnostic coordinates use the active device surface without scaling.
struct Point { int x = 0; int y = 0; };
#if STICKMON_UI_ROUND_TOUCH
// Inset rim targets (~140px radius); upper targets keep clear of the header.
inline constexpr Point TARGETS[] = {
    {118, 56}, {242, 56}, {309, 126}, {309, 234},
    {234, 309}, {126, 309}, {51, 234}, {51, 126}, {180, 180},
};
inline constexpr int CLEAR_X = 58;
inline constexpr int CLEAR_Y = 250;
inline constexpr int CLEAR_W = 76;
inline constexpr int BACK_X = 226;
inline constexpr int BACK_W = 76;
inline constexpr int CALIBRATE_X = 142;
inline constexpr int CALIBRATE_W = 76;
#else
inline constexpr Point TARGETS[] = {
    {30, 68}, {120, 68}, {210, 68},
    {30, 118}, {120, 118}, {210, 118},
    {30, 168}, {120, 168}, {210, 168},
};
inline constexpr int CLEAR_X = 16;
inline constexpr int CLEAR_Y = 198;
inline constexpr int CLEAR_W = 96;
inline constexpr int BACK_X = 128;
inline constexpr int BACK_W = 96;
#endif
inline constexpr int COUNT = 9;
inline constexpr int CLEAR_H = 34;
inline constexpr int BACK_Y = CLEAR_Y;
inline constexpr int BACK_H = CLEAR_H;

enum class Result { NONE, SAMPLE, CLEAR, BACK };
struct Sample { Point down, up, minimum, maximum; };

inline bool inRect(Point point, int x, int y, int width, int height) {
    return point.x >= x && point.x < x + width &&
           point.y >= y && point.y < y + height;
}
inline int controlAt(Point point) {
    if (inRect(point, CLEAR_X, CLEAR_Y, CLEAR_W, CLEAR_H)) return 0;
    if (inRect(point, BACK_X, BACK_Y, BACK_W, BACK_H)) return 1;
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
        if (count == COUNT) return Result::NONE;
        samples[count++] = pending;
        return Result::SAMPLE;
    }
    void cancel() { pressed = false; }
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

}  // namespace Ui::TouchTest
