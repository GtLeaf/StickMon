// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/TouchScroll.h"
#include "ui/common/RoomUiBase.h"

namespace Ui {
class FoodUi final : public FoodUiBase {
public:
    explicit FoodUi(FoodHost& host) : FoodUiBase(host) {}
    uint8_t foodCursor = 0;
    float foodScroll = 0;
    static constexpr bool directTouch() { return true; }
    void reset();
    void enter();
    void resetDescription() {}
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t, float) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
private:
    TouchScroll touch;
};
}  // namespace Ui
