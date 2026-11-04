// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace Sdgoods {
// Experimental fallback for this panel's repeatable right-side offset.
// Anchors are medians of four nine-point diagnostic rounds, in oriented
// screen coordinates. A saved affine calibration replaces this fallback.
struct RightTouchCompensation {
    struct Point { float x, y; };
    static float ramp(float value, float low, float high) {
        if (value <= low) return 0;
        if (value >= high) return 1;
        const float t = (value - low) / (high - low);
        return t * t * (3 - 2 * t);
    }
    static Point apply(float x, float y) {
        // Preserve the left half, the header, and the lower rim. All ramps
        // have zero slope at their endpoints, including the center seam.
        const float weight = ramp(x, 180, 320) * ramp(y, 56, 113) *
            (1 - ramp(y, 248, 309));
        const float lower = ramp(y, 113, 248);
        const float dx = 35.5f - 9 * lower;
        const float dy = 13 - 27 * lower;
        return {x - weight * dx, y + weight * dy};
    }
};
}  // namespace Sdgoods
