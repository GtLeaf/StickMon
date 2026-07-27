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
        VOICE_CALL,
        POWER_SAVE,
        HELP,
        RESET_GAME,
        BACK,
        COUNT,
    };

    enum class ViewMode : uint8_t {
        MENU,
        HELP,
        VOICE_CALL,
        VOICE_ENROLL,
        RESET_CONFIRM,
    };

    uint8_t cursor = 0;
    const char* toast = nullptr;
    uint32_t toastUntil = 0;
    ViewMode viewMode = ViewMode::MENU;
    bool resetConfirmYes = false;
    float menuScroll = 0.0f;
    uint8_t voiceCursor = 0;
    uint32_t enrollmentFinishedAt = 0;

    void activateCurrent();
    void cycleBrightness();
    void normalizeVolumeSetting();
    void markSettingsDirty();
    void renderMenu();
    void renderHelp();
    void renderVoiceCall();
    void renderVoiceEnrollment();
    void renderResetConfirm();
    void renderToast();
    void handleVoiceCallButton(const ButtonEvent& event);
    const char* enrollmentMessage() const;
};
