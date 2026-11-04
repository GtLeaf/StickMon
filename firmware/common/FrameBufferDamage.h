#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Stickmon {

struct FrameRect {
    int left = 0, top = 0, right = 0, bottom = 0;
};

// Both buffers contained the same image before previousDamage was drawn into
// front. Bring back up to date, excluding pixels the next render will replace.
// front is read-only and may still be owned by the display transfer task.
inline void syncFrameOutsideDamage(uint16_t* back, const uint16_t* front,
                                   size_t stride, FrameRect previousDamage,
                                   FrameRect nextDamage) {
    const auto copy = [&](int left, int top, int right, int bottom) {
        if (left >= right || top >= bottom) return;
        for (int y = top; y < bottom; ++y) {
            const size_t offset = static_cast<size_t>(y) * stride + left;
            std::memcpy(back + offset, front + offset,
                        static_cast<size_t>(right - left) * sizeof(uint16_t));
        }
    };
    const int left = std::max(previousDamage.left, nextDamage.left);
    const int top = std::max(previousDamage.top, nextDamage.top);
    const int right = std::min(previousDamage.right, nextDamage.right);
    const int bottom = std::min(previousDamage.bottom, nextDamage.bottom);
    if (left >= right || top >= bottom) {
        copy(previousDamage.left, previousDamage.top,
             previousDamage.right, previousDamage.bottom);
        return;
    }
    copy(previousDamage.left, previousDamage.top, previousDamage.right, top);
    copy(previousDamage.left, bottom, previousDamage.right, previousDamage.bottom);
    copy(previousDamage.left, top, left, bottom);
    copy(right, top, previousDamage.right, bottom);
}

}  // namespace Stickmon
