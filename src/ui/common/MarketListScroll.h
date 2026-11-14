#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "core/TouchTap.h"

namespace Ui {

// Shared by stall lists. It only owns gesture/motion state; callers keep their
// offset, viewport and page lifetime. A captured gesture never becomes a tap.
class MarketListScroll {
public:
    using Event = Platform::IInputDevice::TouchEvent;
    using Type = Platform::IInputDevice::TouchType;
    struct Bounds {
        int x, y, w, h;
        bool contains(int px, int py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    bool moving() const { return velocity_ != 0.0f; }
    void cancel() {
        tracking_ = captured_ = brake_ = false;
        velocity_ = 0.0f;
        sampleCount_ = 0;
    }

    bool touch(const Event& event, Bounds bounds, int maximum,
               int& offset, TouchTap& tap) {
        maximum = std::max(0, maximum);
        if (event.type == Type::CANCEL) {
            const bool consumed = tracking_ || moving();
            cancel();
            tap.cancel();
            return consumed;
        }
        if (event.type == Type::DOWN) {
            const bool wasMoving = moving();
            cancel();
            if (!bounds.contains(event.x, event.y)) return false;
            tracking_ = true;
            brake_ = wasMoving;
            startX_ = event.x;
            startY_ = event.y;
            startOffset_ = std::clamp(offset, 0, maximum);
            position_ = float(startOffset_);
            lastMotionMs_ = event.timestampMs;
            record(event.timestampMs, position_);
            if (brake_) tap.cancel();
            return brake_;
        }
        if (!tracking_) return false;
        const int dx = int(event.x) - startX_;
        const int dy = int(event.y) - startY_;
        if (!captured_ && dx * dx + dy * dy > 100) {
            captured_ = true;
            tap.cancel();
        }
        if (captured_) {
            const float next = float(std::clamp(startOffset_ - dy, 0, maximum));
            if (next != position_) lastMotionMs_ = event.timestampMs;
            position_ = next;
            offset = int(position_);
            record(event.timestampMs, position_);
        }
        const bool consumed = captured_ || brake_;
        if (event.type == Type::UP) {
            velocity_ = 0.0f;
            if (captured_ && event.timestampMs - lastMotionMs_ <= 80 &&
                sampleCount_ > 1 && maximum > 0) {
                const uint32_t elapsed = event.timestampMs - samples_[0].time;
                if (elapsed > 0) {
                    const float speed = (position_ - samples_[0].position) *
                                        1000.0f / float(elapsed);
                    if (std::fabs(speed) >= 100.0f)
                        velocity_ = std::clamp(speed, -1400.0f, 1400.0f);
                }
            }
            if ((position_ <= 0.0f && velocity_ < 0.0f) ||
                (position_ >= maximum && velocity_ > 0.0f)) velocity_ = 0.0f;
            tracking_ = captured_ = brake_ = false;
        }
        return consumed;
    }

    bool update(uint32_t deltaMs, int maximum, int& offset) {
        const int before = offset;
        maximum = std::max(0, maximum);
        offset = std::clamp(offset, 0, maximum);
        if (!moving() || tracking_ || !deltaMs) return offset != before;
        // Integrate the decay analytically, including the last partial step.
        // The same fling travels the same distance at 16, 33 or 66ms cadence.
        constexpr float friction = 10.0f, stopSpeed = 30.0f;
        const float remaining = std::log(std::fabs(velocity_) / stopSpeed) / friction;
        const float seconds = std::min(float(deltaMs) / 1000.0f,
                                       std::max(0.0f, remaining));
        const float decay = std::exp(-friction * seconds);
        position_ += velocity_ * (1.0f - decay) / friction;
        velocity_ *= decay;
        if (seconds >= remaining || std::fabs(velocity_) <= stopSpeed)
            velocity_ = 0.0f;
        if (position_ <= 0.0f || position_ >= maximum) {
            position_ = std::clamp(position_, 0.0f, float(maximum));
            velocity_ = 0.0f;
        }
        offset = std::clamp(int(std::lround(position_)), 0, maximum);
        return offset != before;
    }

private:
    struct Sample { uint32_t time; float position; } samples_[8]{};
    uint8_t sampleCount_ = 0;
    bool tracking_ = false, captured_ = false, brake_ = false;
    int startX_ = 0, startY_ = 0, startOffset_ = 0;
    float position_ = 0.0f, velocity_ = 0.0f;
    uint32_t lastMotionMs_ = 0;

    void dropOldest() {
        for (uint8_t i = 1; i < sampleCount_; ++i) samples_[i - 1] = samples_[i];
        --sampleCount_;
    }
    void record(uint32_t now, float position) {
        if (sampleCount_ && samples_[sampleCount_ - 1].time == now) {
            samples_[sampleCount_ - 1].position = position;
            return;
        }
        if (sampleCount_ == 8) dropOldest();
        samples_[sampleCount_++] = {now, position};
        while (sampleCount_ > 2 && now - samples_[0].time > 100) dropOldest();
    }
};

} // namespace Ui
