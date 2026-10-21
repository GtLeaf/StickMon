#pragma once

#include <algorithm>
#include <cstdlib>
#include "core/TouchTap.h"

class TouchScroll {
public:
    using Event = Platform::IInputDevice::TouchEvent;
    using Type = Platform::IInputDevice::TouchType;
    struct Result { bool changed; int tap; };
    int pressed() const { return pressed_; }
    bool active() const { return down_; }
    void cancel() { tap_.cancel(); pressed_ = -1; down_ = dragging_ = false; }
    Result update(const Event& event, int target, float& scroll, float maximum, bool canScroll) {
        const int before = pressed_;
        const float oldScroll = scroll;
        int tap = -1;
        if (event.type == Type::DOWN) {
            cancel(); down_ = true; scrollable_ = canScroll;
            startX_ = event.x; startY_ = lastY_ = event.y; pressed_ = target;
            tap_.update(event, target);
        } else if (event.type == Type::CANCEL) cancel();
        else if (down_) {
            if (event.type == Type::MOVE) {
                if (scrollable_ && std::abs(event.y - startY_) > 10) dragging_ = true;
                if (dragging_) {
                    scroll = std::clamp(scroll + lastY_ - event.y, 0.0f, maximum);
                    tap_.cancel(); pressed_ = -1;
                } else {
                    tap_.update(event, target);
                    if (target != pressed_ || std::max(std::abs(event.x-startX_), std::abs(event.y-startY_)) > 10) {
                        tap_.cancel(); pressed_ = -1;
                    }
                }
                lastY_ = event.y;
            } else if (event.type == Type::UP) {
                if (!dragging_) tap = tap_.update(event, target);
                cancel();
            }
        }
        return {before != pressed_ || oldScroll != scroll, tap};
    }
private:
    TouchTap tap_;
    bool down_ = false, dragging_ = false, scrollable_ = false;
    int pressed_ = -1, startX_ = 0, startY_ = 0, lastY_ = 0;
};
