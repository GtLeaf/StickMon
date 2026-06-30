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
    normalizeVolumeSetting();
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
    toast = nullptr;
    toastUntil = 0;
    switch (cursor) {
    case BRIGHTNESS:
        cycleBrightness();
        break;
    case GAME_SPEED:
        GameEngine::ins().cycleGameSpeed();
        markSettingsDirty();
        break;
    case VOLUME: {
        auto& settings = GameEngine::ins().gameState().settings;
        settings.volume = settings.volume >= 100 ? 0 : settings.volume + 10;
        markSettingsDirty();
        break;
    }
    case POWER_SAVE:
        GameEngine::ins().cycleIdleTimeout();
        markSettingsDirty();
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

void SettingsScene::normalizeVolumeSetting() {
    auto& volume = GameEngine::ins().gameState().settings.volume;
    uint8_t normalized = volume > 100 ? 100 : (uint8_t)((volume / 10) * 10);

    if (volume != normalized) {
        volume = normalized;
        markSettingsDirty();
    }
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
    if (viewMode == ViewMode::HELP) {
        renderHelp();
    } else {
        renderMenu();
    }
    renderToast();
}

void SettingsScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    const int rowH = 18;
    const int startY = 12;

    for (int i = 0; i < COUNT; ++i) {
        int y = startY + i * rowH;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) c.fillRect(8, y + 2, 4, 12, PixelRenderer::rgb(255, 216, 72));
        PixelRenderer::text(20, y, Ui::Settings::ITEMS[i], fg, 1);

        char value[16] = "";
        const auto& settings = GameEngine::ins().gameState().settings;
        if (i == BRIGHTNESS) snprintf(value, sizeof(value), "%u", Hal::ins().getBrightness());
        if (i == GAME_SPEED) snprintf(value, sizeof(value), "%.0fx", GameEngine::ins().gameSpeed());
        if (i == VOLUME) snprintf(value, sizeof(value), "%u%%", settings.volume);
        if (i == POWER_SAVE) snprintf(value, sizeof(value), "%s", GameEngine::ins().idleTimeoutLabel());
        if (value[0]) PixelRenderer::text(156, y, value, PixelRenderer::rgb(135, 214, 238), 1);

        if (i < COUNT - 1) {
            c.drawFastHLine(20, y + rowH - 1, 190, PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SettingsScene::renderHelp() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    PixelRenderer::text(12, 10, Ui::Settings::HELP_TITLE, PixelRenderer::rgb(67, 213, 224), 1);
    c.drawFastHLine(8, 32, 216, PixelRenderer::rgb(55, 63, 76));
    PixelRenderer::text(18, 44, Ui::Settings::HELP_A_KEY, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(18, 66, Ui::Settings::HELP_B_KEY, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(128, 44, Ui::Settings::HELP_ROOM_A, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(128, 66, Ui::Settings::HELP_MENU_B, PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(18, 100, Ui::Settings::HELP_BASIC, PixelRenderer::rgb(156, 164, 176), 1);
}

void SettingsScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}
