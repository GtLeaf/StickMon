// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "platform/api/PlatformServices.h"
#include "RightTouchCompensation.h"

namespace Sdgoods {
// One sample per update, one event per poll. A failed read never becomes a tap release.
class Cst816TouchState {
public:
    using Type = Platform::IInputDevice::TouchType;
    using Event = Platform::IInputDevice::TouchEvent;
    void setTransform(const Platform::TouchTransform& transform) { transform_ = transform; }
    void setRightCompensation(bool enabled) { rightCompensation_ = enabled; }
    void bypass(bool value) { bypass_ = value; }
    void sample(const uint8_t* data, uint32_t now) {
        pending_ = false;
        const int count = data ? data[0] & 15 : -1;
        const int rawX = data ? ((data[1] & 15) << 8) | data[2] : -1;
        const int rawY = data ? ((data[3] & 15) << 8) | data[4] : -1;
        if (!data || count > 1 || (count == 1 && (rawX >= 360 || rawY >= 360))) {
            if (contact_) emit(Type::CANCEL, x_, y_, now);
            contact_ = false;
            blocked_ = true;
            return;
        }
        const bool down = count == 1;
        if (blocked_) { if (!down) blocked_ = false; return; }
        if (!down && !contact_) return;
        // The unchanged vendor MADCTL rotates the sensor axes this way.
        const int screenX = rawY, screenY = 359 - rawX;
        const auto corrected = rightCompensation_ && !bypass_
            ? RightTouchCompensation::apply(screenX, screenY)
            : RightTouchCompensation::Point{float(screenX), float(screenY)};
        const int x = !down ? x_ : bypass_ ? screenX :
            int(std::lround(transform_.x(corrected.x, corrected.y)));
        const int y = !down ? y_ : bypass_ ? screenY :
            int(std::lround(transform_.y(corrected.x, corrected.y)));
        // Do not turn an out-of-range corrected sample into a tap at the edge.
        if (down && (x < 0 || x >= 360 || y < 0 || y >= 360)) {
            if (contact_) emit(Type::CANCEL, x_, y_, now);
            contact_ = false;
            blocked_ = true;
            return;
        }
        emit(down ? (contact_ ? Type::MOVE : Type::DOWN) : Type::UP, x, y, now);
        contact_ = down;
        x_ = x; y_ = y;
    }
    bool poll(Event& event) {
        if (!pending_) return false;
        pending_ = false; event = event_; return true;
    }
private:
    void emit(Type type, int x, int y, uint32_t now) {
        event_ = {type, int16_t(x), int16_t(y), now}; pending_ = true;
    }
    Event event_{};
    Platform::TouchTransform transform_;
    int16_t x_ = 0, y_ = 0;
    bool pending_ = false, contact_ = false, blocked_ = false, bypass_ = false;
    bool rightCompensation_ = false;
};
}  // namespace Sdgoods
