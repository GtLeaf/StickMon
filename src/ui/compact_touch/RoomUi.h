#pragma once

#include "core/TouchScroll.h"
#include "ui/common/RoomUiBase.h"

namespace Ui {

class RoomUi : public RoomUiBase {
public:
    explicit RoomUi(RoomHost& host) : RoomUiBase(host) {}
    uint8_t roomCursor = 0;
    float roomScroll = 0;
    TouchScroll gridTouch;

    static constexpr bool directTouch() { return true; }
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
