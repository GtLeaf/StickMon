#pragma once
#include <cmath>

namespace Platform {
// Correction in already oriented screen coordinates; identity without calibration.
struct TouchTransform {
    float values[6] = {1, 0, 0, 0, 1, 0};
    float x(float x, float y) const { return values[0]*x + values[1]*y + values[2]; }
    float y(float x, float y) const { return values[3]*x + values[4]*y + values[5]; }
    bool valid(int width, int height) const {
        for (float value : values) if (!std::isfinite(value)) return false;
        const float determinant = values[0]*values[4] - values[1]*values[3];
        return values[0] >= 0.5f && values[0] <= 1.5f &&
            values[4] >= 0.5f && values[4] <= 1.5f &&
            std::abs(values[1]) <= 0.35f && std::abs(values[3]) <= 0.35f &&
            std::abs(values[2]) <= width/4.0f && std::abs(values[5]) <= height/4.0f &&
            determinant >= 0.3f && determinant <= 2.5f;
    }
};
}  // namespace Platform
