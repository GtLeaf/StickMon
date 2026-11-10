// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "ui/common/HatchUiBase.h"
#include "game/HatchInteraction.h"

namespace Ui {
class HatchUi : public HatchUiBase {
public:
    HatchLayout layout() const override { return {360, 360}; }
    void enter(const HatchFrame& frame) override;
    void leave() override;
    HatchAction readMotion(uint32_t nowMs, RenderDemand& demand) override;
    uint32_t visualFrameIntervalMs() const override { return 33; }
    void updateVisuals(const HatchFrame& frame, RenderDemand& demand) override;
    void render(const HatchFrame& frame) override;
    int touchTarget(int16_t x, int16_t y) const;
    HatchAction onTap(int target, uint32_t nowMs);

private:
    Game::HatchShakeDetector shakeDetector;
    uint32_t nextMotionMs = 0;
    uint32_t reactionStartMs = 0;
    bool reacting = false;
    bool tapping = false;
    int8_t lastOffset = 0;
    bool lastHeart = false;
    bool lastNight = false;
    uint16_t lastSecond = 0;
    void react(uint32_t nowMs, bool tap);
    int8_t eggOffset(uint32_t nowMs) const;
    bool heartVisible(uint32_t nowMs) const;
};
}  // namespace Ui
