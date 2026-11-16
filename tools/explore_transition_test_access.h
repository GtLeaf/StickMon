#pragma once
#include "app/ExploreTransitionFlow.h"
namespace AmoledV1 {
// Fixture seeding only. Production clients receive const state and explicit commands.
struct ExploreTransitionTestAccess {
    static ExploreTransitionFlow::State &state(ExploreTransitionFlow &flow) { return flow.state_; }
    static void resetFade(ExploreTransitionFlow &flow) { flow.fade_.reset(); }
};
} // namespace AmoledV1
