#pragma once

#include "ui/common/RoomUiBase.h"

namespace Ui {
class RoomUi : public RoomUiBase {
public:
    explicit RoomUi(RoomHost& host) : RoomUiBase(host) {}
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
    uint8_t roomCursor = 0;
private:
    static constexpr uint8_t ROOM_ITEM_COUNT = 3;
};
}  // namespace Ui
