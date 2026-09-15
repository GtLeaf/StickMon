#pragma once

#include <algorithm>
#include <cstdint>

namespace AmoledV1 {

// Advance only after the previous full frame was submitted successfully.
// Slow resource loads cannot skip the initial frame or either endpoint.
class SceneFade {
public:
    void beginOut() { begin(false); }
    void beginIn() { begin(true); }
    void reset() { *this = SceneFade{}; }
    bool active() const { return active_; }
    uint8_t alpha() const { return alpha_; }
    bool inward() const { return inward_; }
    bool complete() const { return active_ && elapsedMs_ == DURATION_MS && presented_; }

    void update(uint32_t nowMs) {
        if (!active_ || !presented_ || complete()) return;
        // Keep intermediate shades visible even when a frame takes unusually long.
        const uint32_t step = std::min<uint32_t>(nowMs - lastStepMs_, 50);
        if (step == 0) return;
        lastStepMs_ = nowMs;
        elapsedMs_ = std::min<uint32_t>(DURATION_MS, elapsedMs_ + step);
        const uint8_t progress = static_cast<uint8_t>(elapsedMs_ * 255 / DURATION_MS);
        alpha_ = inward_ ? 255 - progress : progress;
        presented_ = false;
    }

    void markPresented(uint32_t nowMs) {
        if (!active_) return;
        // A repeated initial frame must not restart the clock. The next update
        // can share the transfer's millisecond on a busy device loop.
        if (elapsedMs_ == 0 && !presented_) lastStepMs_ = nowMs;
        presented_ = true;
    }

private:
    void begin(bool inward) {
        reset();
        active_ = true;
        inward_ = inward;
        alpha_ = inward ? 255 : 0;
    }

    static constexpr uint32_t DURATION_MS = 300;
    uint32_t elapsedMs_ = 0;
    uint32_t lastStepMs_ = 0;
    uint8_t alpha_ = 0;
    bool active_ = false;
    bool inward_ = false;
    bool presented_ = false;
};

}  // namespace AmoledV1
