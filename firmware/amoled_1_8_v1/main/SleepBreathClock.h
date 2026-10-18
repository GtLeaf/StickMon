#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace AmoledV1 {
// Presentation only. Lock/unlock never restarts the sleeping actor's clock.
class SleepBreathClock {
public:
    void start(uint32_t nowMs, uint16_t frameMs, uint8_t frames,
               uint8_t expandedFrame) {
        startedMs_ = nowMs;
        frameMs_ = std::max<uint16_t>(1, frameMs);
        frames_ = std::max<uint8_t>(1, frames);
        expandedFrame_ = expandedFrame % frames_;
    }
    uint8_t frame(uint32_t nowMs) const {
        return ((nowMs - startedMs_) / frameMs_) % frames_;
    }
    float expansion(uint32_t nowMs) const {
        const uint32_t period = frameMs_ * std::max<uint8_t>(2, frames_);
        // Each held sprite straddles its extremum; transitions occur at the
        // midpoint of inhaling/exhaling, preserving the original frame dwell.
        const float phase = static_cast<float>((nowMs - startedMs_) % period) /
            period - (expandedFrame_ + 0.5f) / std::max<uint8_t>(2, frames_);
        return 0.5f + 0.5f * std::cos(phase * 6.2831853f);
    }
private:
    uint32_t startedMs_ = 0;
    uint16_t frameMs_ = 800;
    uint8_t frames_ = 2;
    uint8_t expandedFrame_ = 0;
};
} // namespace AmoledV1
