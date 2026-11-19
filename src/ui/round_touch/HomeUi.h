// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/common/HomeUiBase.h"
#include "presentation/HomePhotoFrame.h"

namespace Ui {
// The room keeps the full native canvas; menu surfaces have separate geometry.
class HomeUi final : public HomeUiBase {
public:
    explicit HomeUi(HomeHost& host) : HomeUiBase(host) {}
    void enter(uint16_t placedFurniture);
    void onExit() override;
    void render() override;
    bool roomVisualFrozen() const override { return photoFrame_.active(); }
    void closeTransientOverlay() override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    void focus(float x, float y) { focusCamera(x, y, 180); }
    int touchTarget(int16_t x, int16_t y) override;
    bool onButton(const ButtonEvent& event) override;
    bool onTap(int target) override;
protected:
    void drawFloor(const HomeFrame& frame) override;
    void drawHud(const HomeFrame& frame) override;
    void drawToast() override;
    void drawHomeMenu(const HomeFrame& frame) override;
    void drawContact(const HomeFrame& frame) override;
private:
    HomePhotoFrame photoFrame_;
    bool wallFramePressed_ = false;
};
}  // namespace Ui
