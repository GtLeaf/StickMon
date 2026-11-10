// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Sdgoods::AudioPolicy {
inline constexpr int MAX_OUTPUT_PERCENT = 20;
inline bool audible(uint8_t volume, bool active, bool suspended) {
    return volume > 0 && active && !suspended;
}
inline void stereo(const int16_t* mono, int16_t* output, size_t count, uint8_t volume) {
    const int gain = std::min<int>(volume, 100) * MAX_OUTPUT_PERCENT;
    for (size_t i = 0; i < count; ++i) {
        const int16_t sample = int32_t(mono[i]) * gain / 10000;
        output[i * 2] = output[i * 2 + 1] = sample;
    }
}
// Keeps I2S and the PA running briefly after the mixer drains so a short
// streaming underrun becomes silence instead of a PA/I2S off-on pop. Muting or
// suspending closes the output at once without any hold.
class OutputGate {
public:
    // About 300 ms at 256 samples per block and 16 kHz.
    static constexpr uint16_t HOLD_BLOCKS = 19;
    // Returns true when the next block should be written to I2S.
    bool next(bool enabled, bool active) {
        if (!enabled) { idleBlocks_ = HOLD_BLOCKS; return false; }
        if (active) { idleBlocks_ = 0; return true; }
        if (idleBlocks_ >= HOLD_BLOCKS) return false;
        ++idleBlocks_;
        return true;
    }
private:
    uint16_t idleBlocks_ = HOLD_BLOCKS;
};
inline int16_t microphoneSample(int32_t raw) {
    return std::clamp<int32_t>(raw >> 14, -32768, 32767);
}
}  // namespace Sdgoods::AudioPolicy
