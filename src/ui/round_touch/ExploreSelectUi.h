// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/TouchTap.h"
#include "ui/common/ExploreUiModel.h"

namespace Ui::RoundExplore {
constexpr int BACK = 230, PREVIOUS = 231, NEXT = 232, DEPART = 233;
constexpr MenuGrid::Rect PREVIOUS_BOUNDS{66, 58, 44, 44};
constexpr MenuGrid::Rect NEXT_BOUNDS{250, 58, 44, 44};
constexpr MenuGrid::Rect DEPART_BOUNDS{120, 282, 120, 40};

class Selection {
public:
    Selection() = default;
    ~Selection() { release(); }
    Selection(const Selection&) = delete;
    Selection& operator=(const Selection&) = delete;
    void release() const;
    void render(const ExploreRouteFrame& frame) const;
    int target(const ExploreInputFrame& frame, const ExploreUiState& state, int x, int y) const;
    bool tap(ExploreHost& host, ExploreUiState& state, int target);
    bool touch(ExploreHost& host, ExploreUiState& state,
               const Platform::IInputDevice::TouchEvent& event);
    void cancel();
private:
    mutable uint16_t* previewBackground = nullptr;
    mutable const uint16_t* cachedTarget = nullptr;
    mutable bool allocationAttempted = false;
    mutable bool cachedByteSwapped = false;
    mutable uint32_t lastFrameSerial = 0;
    mutable char staticKey[256] = {};
    TouchTap tracker;
    int pressed = -1;
    int16_t startX = 0, startY = 0;
    bool active = false, swipe = false;
};
}  // namespace Ui::RoundExplore
