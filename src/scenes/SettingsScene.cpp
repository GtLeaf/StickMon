#include "scenes/SettingsScene.h"
#include <cstdio>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

void SettingsScene::onEnter() {
    cursor = 0;
    viewMode = ViewMode::MENU;
    settingsDirty = false;
}

void SettingsScene::onExit() {
    saveSettingsIfDirty();
}

void SettingsScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
}

bool SettingsScene::onButton(const ButtonEvent& event) {
    if (viewMode == ViewMode::HELP) {
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::PRESSED) {
            saveSettingsIfDirty();
            viewMode = ViewMode::MENU;
            return true;
        }
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
            GameEngine::ins().requestScene(SceneID::MENU);
            return true;
        }
        return false;
    }

    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        cursor = (cursor + 1) % COUNT;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateCurrent();
        return true;
    }
    return false;
}

void SettingsScene::activateCurrent() {
    switch (cursor) {
    case BRIGHTNESS:
        cycleBrightness();
        toast = Ui::Settings::BRIGHTNESS_CHANGED;
        break;
    case GAME_SPEED:
        GameEngine::ins().cycleGameSpeed();
        markSettingsDirty();
        toast = Ui::Common::SPEED_CHANGED;
        break;
    case VOLUME: {
        auto& settings = GameEngine::ins().gameState().settings;
        settings.volume = (settings.volume + 1) % 4;
        markSettingsDirty();
        toast = Ui::Settings::VOLUME_CHANGED;
        break;
    }
    case POWER_SAVE:
        GameEngine::ins().cycleIdleTimeout();
        markSettingsDirty();
        toast = nullptr;
        toastUntil = 0;
        return;
        break;
    case HELP:
        viewMode = ViewMode::HELP;
        toast = nullptr;
        return;
    case BACK:
        GameEngine::ins().requestScene(SceneID::MENU);
        return;
    default:
        break;
    }
    toastUntil = Hal::ins().millis() + 1100;
}

void SettingsScene::cycleBrightness() {
    uint8_t cur = Hal::ins().getBrightness();
    uint8_t next = 128;
    if (cur < 96) next = 128;
    else if (cur < 160) next = 192;
    else if (cur < 224) next = 255;
    else next = 64;
    Hal::ins().setBrightness(next);
    GameEngine::ins().gameState().settings.brightness = next;
    markSettingsDirty();
}

void SettingsScene::markSettingsDirty() {
    settingsDirty = true;
    GameEngine::ins().markDirty(false);
}

void SettingsScene::saveSettingsIfDirty() {
    if (!settingsDirty) return;
    GameEngine::ins().saveNow();
    settingsDirty = false;
}

void SettingsScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));
    c.fillRect(0, 0, Hal::DISPLAY_W, 24, PixelRenderer::rgb(25, 25, 40));
    PixelRenderer::text(4, 5, Ui::SETTINGS, PixelRenderer::rgb(67, 213, 224), 1);
    if (viewMode == ViewMode::HELP) {
        renderHelp();
    } else {
        renderMenu();
    }
    renderToast();
}

void SettingsScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    const int rowH = 24;
    const int startY = 34;

    for (int i = 0; i < COUNT; ++i) {
        int y = startY + i * rowH;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) c.fillRect(4, y + 2, 5, 18, PixelRenderer::rgb(255, 216, 72));
        PixelRenderer::text(16, y, Ui::Settings::ITEMS[i], fg, 1);

        char value[16] = "";
        const auto& settings = GameEngine::ins().gameState().settings;
        if (i == BRIGHTNESS) snprintf(value, sizeof(value), "%u", Hal::ins().getBrightness());
        if (i == GAME_SPEED) snprintf(value, sizeof(value), "%.0fx", GameEngine::ins().gameSpeed());
        if (i == VOLUME) snprintf(value, sizeof(value), "%u", settings.volume);
        if (i == POWER_SAVE) snprintf(value, sizeof(value), "%s", GameEngine::ins().idleTimeoutLabel());
        if (value[0]) PixelRenderer::text(84, y, value, PixelRenderer::rgb(135, 214, 238), 1);

        if (i < COUNT - 1) {
            c.drawFastHLine(4, y + rowH - 6, Hal::DISPLAY_W - 8, PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SettingsScene::renderHelp() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(6, 34, 123, 162, PixelRenderer::rgb(25, 31, 40));
    c.drawRect(6, 34, 123, 162, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(12, 44, Ui::Settings::HELP_TITLE, PixelRenderer::rgb(67, 213, 224), 1);
    PixelRenderer::text(12, 70, Ui::Settings::HELP_A_KEY, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(12, 90, Ui::Settings::HELP_B_KEY, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(12, 116, Ui::Settings::HELP_ROOM_A, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(12, 136, Ui::Settings::HELP_MENU_B, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(12, 164, Ui::Settings::HELP_BASIC, PixelRenderer::rgb(156, 164, 176), 1);
}

void SettingsScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(14, 210, 107, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(22, 216, toast, PixelRenderer::rgb(255, 255, 255), 1);
}
