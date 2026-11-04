// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cstdint>

namespace Sdgoods {
// UI estimate only. Thresholds require board measurements, not charger control.
class ChargingTrend {
public:
    static constexpr uint32_t SAMPLE_MS = 2000;
    static constexpr uint32_t MAX_GAP_MS = 6000;
    static constexpr uint32_t HOLD_MS = 120000;
    static constexpr int BLOCK_SAMPLES = 5;
    static constexpr int WINDOW_COUNT = 4;
    static constexpr int ENTER_RISE_MV = 24;
    static constexpr int STEP_RISE_MV = 4;
    static constexpr int EXIT_DROP_MV = 24;

    void reset() { *this = ChargingTrend{}; }
    bool charging() const { return charging_; }

    void sample(uint32_t now, int millivolts) {
        if (millivolts < 2500 || millivolts > 4500) { reset(); return; }
        if (sampled_) {
            const uint32_t delta = now - sampledAt_;
            if (delta < SAMPLE_MS) return;
            if (delta > MAX_GAP_MS) reset();
        }
        sampled_ = true;
        sampledAt_ = now;
        total_ += millivolts;
        if (++samples_ < BLOCK_SAMPLES) return;
        const int average = (total_ + BLOCK_SAMPLES / 2) / BLOCK_SAMPLES;
        total_ = samples_ = 0;
        if (windows_) {
            falling_ = means_[windows_ - 1] - average >= STEP_RISE_MV ? falling_ + 1 : 0;
        }
        if (windows_ == WINDOW_COUNT) {
            for (int i = 1; i < WINDOW_COUNT; ++i) means_[i - 1] = means_[i];
            --windows_;
        }
        means_[windows_++] = average;
        bool rising = windows_ == WINDOW_COUNT &&
            means_[WINDOW_COUNT - 1] - means_[0] >= ENTER_RISE_MV;
        for (int i = 1; rising && i < windows_; ++i)
            rising = means_[i] - means_[i - 1] >= STEP_RISE_MV;
        if (rising) {
            if (!charging_) peak_ = average;
            charging_ = true;
            lastRiseAt_ = now;
        }
        if (charging_) {
            peak_ = std::max(peak_, average);
            if (falling_ >= 2 || peak_ - average >= EXIT_DROP_MV ||
                now - lastRiseAt_ >= HOLD_MS) charging_ = false;
        }
    }

private:
    int means_[WINDOW_COUNT]{};
    int total_ = 0, samples_ = 0, windows_ = 0, falling_ = 0, peak_ = 0;
    uint32_t sampledAt_ = 0, lastRiseAt_ = 0;
    bool sampled_ = false, charging_ = false;
};
}  // namespace Sdgoods
