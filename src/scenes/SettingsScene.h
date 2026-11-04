#pragma once

#include "core/Scene.h"
#include "ui/SettingsUi.h"

// Application adapter; the selected family owns settings layout and input.
class SettingsScene : public Scene, private Ui::SettingsHost {
public:
    SettingsScene() : ui(*this) {}
    void onEnter() override;
    void onExit() override;
    SceneUpdateResult update(uint32_t nowMs, float dtSeconds) override;
    void render() override;
    bool onButton(const ButtonEvent& event) override;
    bool touchNavigation() const override;
    bool onTouch(const Platform::IInputDevice::TouchEvent& event) override;
    void cancelTouch() override;
    int touchTarget(int16_t x, int16_t y) override;
    bool onTap(int target) override;

private:
    Ui::SettingsUi ui;
    const Game::GameState& settingsState() const override;
    uint8_t settingsBrightness() const override;
    float settingsGameSpeed() const override;
    const char* settingsIdleLabel() const override;
    void initializeSettings(bool continuousVolume) override;
    void cycleSetting(Ui::SettingsPage::Item item) override;
    bool setSettingValue(Ui::SettingsPage::Item item, uint8_t value) override;
    void previewSettingsVolume() override;
    void leaveSettings() override;
    void replaySettingsTutorial() override;
    bool resetSettingsGame() override;
    void finishSettingsReset() override;
    void markSettingsDirty();
};
