#pragma once

#include "core/Scene.h"
#include "core/TouchScroll.h"

class SettingsScene : public Scene {
public:
    void onEnter() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;

private:
    enum Item : uint8_t {
        BRIGHTNESS = 0,
        GAME_SPEED,
        VOLUME,
        POWER_SAVE,
        HELP,
        RESET_GAME,
        COUNT,
    };

    enum class ViewMode : uint8_t {
        MENU,
        HELP,
        RESET_CONFIRM,
    };

    uint8_t cursor = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    ViewMode viewMode = ViewMode::MENU;
    bool resetConfirmYes = false;
    float menuScroll = 0.0f;
    uint8_t helpPage = 0;
    TouchScroll pageTouch;
    float touchScroll = 0;
    int sliderCandidate = -1;
    int16_t sliderStartX = 0, sliderStartY = 0;
    bool sliderDragging = false, sliderChanged = false;

    static constexpr uint8_t TOUCH_ORDER[] = {BRIGHTNESS, VOLUME, GAME_SPEED, POWER_SAVE, HELP, RESET_GAME};
    bool setSliderValue(int item, int x);
    void previewVolume();
    void renderTouch();

    void activateCurrent();
    void cycleBrightness();
    void normalizeVolumeSetting();
    void markSettingsDirty();
    void renderMenu();
    void renderHelp();
    void renderResetConfirm();
    void renderToast();
};
