#pragma once

#include "core/TouchScroll.h"
#include "ui/common/ComputerUiBase.h"

namespace Ui {

class ComputerUi : public ComputerUiBase {
public:
    explicit ComputerUi(ComputerHost& host) : ComputerUiBase(host) {}
    uint8_t computerCursor = 0;
    float pageScroll = 0;
    TouchScroll gridTouch;

    static constexpr bool directTouch() { return true; }
    static constexpr bool showsNotices() { return true; }
    void reset();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
};

}  // namespace Ui
