#pragma once

#include "ui/common/ShowerUiBase.h"

namespace Ui {

class ShowerUi : public ShowerUiBase {
public:
    explicit ShowerUi(ShowerHost& host) : ShowerUiBase(host) {}
    ShowerLayout layout() const override;
    void render() override;
    UiMotion::StepResult updateMenu() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    bool usesMotionInput() const override { return false; }
    ShowerMotion readMotion(uint32_t nowMs, float dtSeconds) override;
    void moveTool(bool, float, const ShowerMotion&) override {}
private:
    TouchTap tap;
    bool dragging = false;
    ShowerMode dragMode = ShowerMode::MENU;
    float dragTravel = 0;
    void placeTool(int x, int y);
};

}  // namespace Ui
