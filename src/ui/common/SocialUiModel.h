#pragma once

#include <cstdint>
#include "core/VisitSessionService.h"

namespace Ui {
using SocialState = Communication::VisitSessionService::State;
inline constexpr uint32_t SOCIAL_DOTS_INTERVAL_MS = 500;

struct SocialUiState {
    uint8_t cursor = 0;
    uint8_t roomCursor = 0;
    uint8_t lastRoomCount = 0;
    uint32_t stateStartedMs = 0;
    uint32_t lastVisualTick = UINT32_MAX;
};

struct SocialFrame {
    Communication::VisitSessionService::ViewModel model;
    uint32_t nowMs = 0;
    const SocialUiState* visuals = nullptr;
};

struct SocialLayout { int width, height; };
}  // namespace Ui
