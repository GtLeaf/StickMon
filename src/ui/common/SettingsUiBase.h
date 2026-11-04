#pragma once

#include "core/Scene.h"
#include "game/GameState.h"

namespace Ui {

struct SettingsPage {
    enum Item : uint8_t {
        BRIGHTNESS = 0,
        GAME_SPEED,
        VOLUME,
        POWER_SAVE,
        HELP,
        RESET_GAME,
        COUNT,
    };
    enum class ViewMode : uint8_t { MENU, HELP, RESET_CONFIRM };
};

class SettingsHost {
public:
    virtual ~SettingsHost() = default;
    virtual const Game::GameState& settingsState() const = 0;
    virtual uint8_t settingsBrightness() const = 0;
    virtual float settingsGameSpeed() const = 0;
    virtual const char* settingsIdleLabel() const = 0;
    virtual void initializeSettings(bool continuousVolume) = 0;
    virtual void cycleSetting(SettingsPage::Item item) = 0;
    virtual bool setSettingValue(SettingsPage::Item item, uint8_t value) = 0;
    virtual void previewSettingsVolume() = 0;
    virtual void leaveSettings() = 0;
    virtual void replaySettingsTutorial() = 0;
    virtual bool resetSettingsGame() = 0;
    virtual void finishSettingsReset() = 0;
};

class SettingsUiBase : public Scene {
public:
    using Item = SettingsPage::Item;
    using ViewMode = SettingsPage::ViewMode;
    static constexpr Item BRIGHTNESS = SettingsPage::BRIGHTNESS;
    static constexpr Item GAME_SPEED = SettingsPage::GAME_SPEED;
    static constexpr Item VOLUME = SettingsPage::VOLUME;
    static constexpr Item POWER_SAVE = SettingsPage::POWER_SAVE;
    static constexpr Item HELP = SettingsPage::HELP;
    static constexpr Item RESET_GAME = SettingsPage::RESET_GAME;
    static constexpr Item COUNT = SettingsPage::COUNT;

    explicit SettingsUiBase(SettingsHost& host) : host(host) {}
    uint8_t cursor = 0;
    uint8_t helpPage = 0;
    ViewMode viewMode = ViewMode::MENU;
    bool resetConfirmYes = false;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;

protected:
    SettingsHost& host;
    void resetState();
    bool handleButton(const ButtonEvent& event);
    void activateCurrent();
    SceneUpdateResult updateNotice(uint32_t nowMs);
};

}  // namespace Ui
