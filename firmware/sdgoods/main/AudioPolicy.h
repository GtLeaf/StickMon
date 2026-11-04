// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Sdgoods::AudioPolicy {
inline bool audible(uint8_t volume, bool active, bool suspended) {
    return volume > 0 && active && !suspended;
}
inline void stereo(const int16_t* mono, int16_t* output, size_t count, uint8_t volume) {
    const int gain = std::min<int>(volume, 100);
    for (size_t i = 0; i < count; ++i) {
        const int16_t sample = int32_t(mono[i]) * gain / 100;
        output[i * 2] = output[i * 2 + 1] = sample;
    }
}
inline int16_t microphoneSample(int32_t raw) {
    return std::clamp<int32_t>(raw >> 14, -32768, 32767);
}
}  // namespace Sdgoods::AudioPolicy
