#pragma once

#include "core/Scene.h"
#include "ui/common/HatchUiModel.h"

namespace Ui {
class HatchUiBase {
public:
    virtual ~HatchUiBase() = default;
    virtual HatchLayout layout() const = 0;
    virtual void render(const HatchFrame& frame) = 0;
    virtual void enter(const HatchFrame& frame);
    virtual void leave() {}
    virtual HatchAction readMotion(uint32_t, RenderDemand&) { return HatchAction::NONE; }
    virtual uint32_t visualFrameIntervalMs() const { return 66; }
    virtual void updateVisuals(const HatchFrame& frame, RenderDemand& demand);
    void updateToast(uint32_t nowMs, RenderDemand& demand);
    HatchAction onButton(const ButtonEvent& event, uint32_t nowMs);
    HatchAction onTap(int target, uint32_t nowMs);
    int touchTarget(int16_t x, int16_t y) const;

private:
    uint32_t toastUntil = 0;
    const char* toast = nullptr;
    int8_t lastEggOffset = 0;
    bool lastNight = false;
    uint16_t lastRemainingSecond = 0;

protected:
    const char* notice() const { return toast; }
};
}  // namespace Ui
