#pragma once

#include <cmath>

namespace UiMotion {

struct StepResult {
    bool changed;
    bool active;
};

inline StepResult lerp(float& value, float target, float factor,
                       float epsilon = 0.05f) {
    float diff = target - value;
    if (fabsf(diff) <= epsilon) {
        bool changed = value != target;
        value = target;
        return StepResult{changed, false};
    }

    value += diff * factor;
    if (fabsf(target - value) <= epsilon) value = target;
    return StepResult{true, value != target};
}

inline StepResult approach(float& value, float target, float maxStep,
                           float epsilon = 0.05f) {
    float diff = target - value;
    if (fabsf(diff) <= epsilon) {
        bool changed = value != target;
        value = target;
        return StepResult{changed, false};
    }
    if (maxStep <= 0.0f) {
        return StepResult{false, true};
    }

    if (fabsf(diff) <= maxStep) {
        value = target;
        return StepResult{true, false};
    }
    value += diff > 0.0f ? maxStep : -maxStep;
    return StepResult{true, true};
}

}
