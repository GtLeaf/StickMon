#pragma once
#include "app/HomeFoodFlow.h"
namespace AmoledV1 {
// Fixture-only clock seeding; production exposes const state.
struct HomeFoodTestAccess {
    static void setBiteDeadline(HomeFoodFlow &flow, uint8_t actor, uint32_t nowMs) {
        flow.state_.biteAtMs[actor] = nowMs;
    }
};
} // namespace AmoledV1
