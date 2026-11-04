#pragma once

#include "ui/common/SettingsUiBase.h"

namespace Ui {

class SettingsUi : public SettingsUiBase {
public:
    explicit SettingsUi(SettingsHost& host) : SettingsUiBase(host) {}
    float menuScroll = 0.0f;

    void onEnter() override;
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
    void renderMenu();
    void renderHelp();
    void renderResetConfirm();
    void renderToast();
};

}  // namespace Ui
