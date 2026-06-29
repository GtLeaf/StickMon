#pragma once

#include "core/Scene.h"

class SettingsScene : public Scene {
public:
    void onEnter() override;
    void onExit() override;
    void update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;

private:
    enum Item : uint8_t {
        BRIGHTNESS = 0,
        GAME_SPEED,
        VOLUME,
        POWER_SAVE,
        HELP,
        BACK,
        COUNT,
    };

    enum class ViewMode : uint8_t {
        MENU,
        HELP,
    };

    uint8_t cursor = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    ViewMode viewMode = ViewMode::MENU;
    bool settingsDirty = false;

    void activateCurrent();
    void cycleBrightness();
    void markSettingsDirty();
    void saveSettingsIfDirty();
    void renderMenu();
    void renderHelp();
    void renderToast();
};
