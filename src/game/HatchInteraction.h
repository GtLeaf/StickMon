#pragma once

#include <cmath>
#include <cstdint>

namespace Game {

// Reject single impacts and slow orientation changes; one gesture needs a
// quiet interval before it can be counted again. Timing is real time.
class HatchShakeDetector {
public:
    static constexpr uint32_t ANIMATION_MS = 1000;

    void reset() { *this = HatchShakeDetector{}; }

    bool sample(float x, float y, float z, uint32_t nowMs) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
        const uint32_t dt = nowMs - lastSampleMs_;
        if (!initialized_ || dt > 250) {
            gravityX_ = x;
            gravityY_ = y;
            gravityZ_ = z;
            initialized_ = true;
            startedMs_ = nowMs;
            quietMs_ = 0;
            peaks_ = 0;
            lastSampleMs_ = nowMs;
            return false;
        }
        if (dt == 0) return false;
        lastSampleMs_ = nowMs;
        const float alpha = 1.0f - std::exp(-3.5f * dt / 1000.0f);
        gravityX_ += alpha * (x - gravityX_);
        gravityY_ += alpha * (y - gravityY_);
        gravityZ_ += alpha * (z - gravityZ_);
        const float dx = x - gravityX_, dy = y - gravityY_, dz = z - gravityZ_;
        const float motion = std::sqrt(dx * dx + dy * dy + dz * dz);
        quietMs_ = motion < 0.22f ? quietMs_ + dt : 0;
        if (!armed_ && nowMs - lastGestureMs_ >= 800 && quietMs_ >= 120) armed_ = true;
        if (nowMs - startedMs_ < 200 || !armed_) return false;
        peaks_ = motion >= 0.55f ? peaks_ + 1 : 0;
        if (peaks_ < 2) return false;
        peaks_ = 0;
        armed_ = false;
        lastGestureMs_ = nowMs;
        return true;
    }

    static int8_t animationOffset(uint32_t ageMs) {
        constexpr int8_t offsets[] = {
            0, -4, 4, -4, 4, -3, 3, -3, 3, -3, 3, -2, 2,
            -2, 2, -2, 2, -1, 1, -1, 1, -1, 1, -1, 1};
        return ageMs < ANIMATION_MS ? offsets[ageMs / 40] : 0;
    }

private:
    bool initialized_ = false;
    bool armed_ = true;
    uint8_t peaks_ = 0;
    uint32_t lastSampleMs_ = 0;
    uint32_t startedMs_ = 0;
    uint32_t lastGestureMs_ = 0;
    uint32_t quietMs_ = 0;
    float gravityX_ = 0, gravityY_ = 0, gravityZ_ = 0;
};

}  // namespace Game
