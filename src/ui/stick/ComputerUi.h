#pragma once

#include "ui/common/ComputerUiBase.h"

namespace Ui {
class ComputerUi : public ComputerUiBase {
public:
    explicit ComputerUi(ComputerHost& host) : ComputerUiBase(host) {}
    uint8_t computerCursor = 0;
    static constexpr bool showsNotices() { return false; }
    void reset();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return true; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
private:
    static constexpr uint8_t COMPUTER_ITEM_COUNT = 2;
};
}  // namespace Ui
