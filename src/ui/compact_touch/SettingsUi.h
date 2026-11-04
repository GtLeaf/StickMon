#pragma once

#include "core/TouchScroll.h"
#include "ui/common/SettingsUiBase.h"

namespace Ui {

class SettingsUi : public SettingsUiBase {
public:
    explicit SettingsUi(SettingsHost& host) : SettingsUiBase(host) {}
    TouchScroll pageTouch;
    float touchScroll = 0;
    int sliderCandidate = -1;
    int16_t sliderStartX = 0, sliderStartY = 0;
    bool sliderDragging = false, sliderChanged = false;

    static constexpr uint8_t TOUCH_ORDER[] = {BRIGHTNESS, VOLUME, GAME_SPEED, POWER_SAVE, HELP, RESET_GAME};
    void onEnter() override;
    void onExit() override { cancelTouch(); }
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override { return false; }
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;
    bool setSliderValue(int item, int x);
};

}  // namespace Ui
