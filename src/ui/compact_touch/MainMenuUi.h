#pragma once

#include "core/TouchScroll.h"
#include "ui/common/MainMenuUiBase.h"

namespace Ui {

class MainMenuUi : public MainMenuUiBase {
public:
    explicit MainMenuUi(MainMenuHost& host) : MainMenuUiBase(host) {}
    float menuGridScroll = 0.0f;
    TouchScroll gridTouch;

    void enter(bool fromHome);
    void resetNavigation();
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
};

}  // namespace Ui
