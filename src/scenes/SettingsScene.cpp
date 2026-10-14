#include "scenes/SettingsScene.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/MathUtil.h"
#include "core/TraceLog.h"
#include "core/UiStrings.h"
#include "core/UiMotion.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

void SettingsScene::onEnter() {
    GameEngine::ins().wakeFromIdle();
    cursor = 0;
    viewMode = ViewMode::MENU;
    resetConfirmYes = false;
    menuScroll = 0.0f;
    helpPage = 0;
    normalizeVolumeSetting();
}

SceneUpdateResult SettingsScene::update(uint32_t nowMs, float dtSeconds) {
    (void)dtSeconds;
    RenderDemand demand;

    if (viewMode == ViewMode::MENU) {
        static constexpr int ROW_H = 24;
        static constexpr int START_Y = 6;
        const int contentH = START_Y * 2 + COUNT * ROW_H;
        const int maxScroll =
            contentH > Hal::DISPLAY_H ? contentH - Hal::DISPLAY_H : 0;
        int targetScroll =
            START_Y + cursor * ROW_H + ROW_H / 2 - Hal::DISPLAY_H / 2;
        targetScroll = MathUtil::clamp(targetScroll, 0, maxScroll);
        UiMotion::StepResult step = UiMotion::lerp(
            menuScroll, static_cast<float>(targetScroll), 0.25f, 0.5f);
        demand.changed(step.changed);
        demand.animate(step.active);
    }

    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool SettingsScene::onButton(const ButtonEvent& event) {
    if (viewMode == ViewMode::RESET_CONFIRM) {
        if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
            viewMode = ViewMode::MENU;
            resetConfirmYes = false;
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            resetConfirmYes = !resetConfirmYes;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (!resetConfirmYes) {
                viewMode = ViewMode::MENU;
                return true;
            }
            if (!GameEngine::ins().resetGame()) {
                viewMode = ViewMode::MENU;
                toast = Ui::Settings::RESET_FAILED;
                toastUntil = Hal::ins().millis() + 1500;
                return true;
            }
            GameEngine::ins().requestScene(SceneID::HATCH);
            return true;
        }
        return false;
    }

    if (viewMode == ViewMode::HELP) {
        static constexpr uint8_t HELP_PAGE_COUNT =
            sizeof(Ui::Settings::HELP_PAGE_TITLES) /
            sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
        if (event.btn == 1 && event.action == BtnAction::PRESSED) {
            helpPage = (helpPage + 1) % HELP_PAGE_COUNT;
            return true;
        }
        if (event.btn == 0 && event.action == BtnAction::PRESSED) {
            if (helpPage + 1 == HELP_PAGE_COUNT) {
                GameEngine::ins().resetTutorial();
                GameEngine::ins().requestScene(SceneID::MAIN);
            }
            return true;
        }
        if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
            viewMode = ViewMode::MENU;
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
        if (cursor == 0) menuScroll = 0.0f;
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
        auto& engine = GameEngine::ins();
        auto& state = engine.gameState();
        auto& settings = state.settings;
        settings.volume = settings.volume >= 100 ? 0 : settings.volume + 10;
        Hal::ins().setAudioVolume(settings.volume);
        if (settings.volume == 0) {
            CryPlayer::ins().stop();
        } else if (state.teamCount > 0) {
            CryPlayer::ins().replay(engine.activeMonster().speciesId);
        }
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
        helpPage = 0;
        toast = nullptr;
        return;
    case RESET_GAME:
        viewMode = ViewMode::RESET_CONFIRM;
        resetConfirmYes = false;
        toast = nullptr;
        return;
    default:
        break;
    }
}

void SettingsScene::cycleBrightness() {
    GameEngine::ins().wakeFromIdle();
    uint8_t cur = Hal::ins().getBrightness();
    static constexpr uint8_t LEVELS[] = {32, 64, 128, 192, 255};
    uint8_t next = LEVELS[0];
    for (uint8_t level : LEVELS) {
        if (level > cur) {
            next = level;
            break;
        }
    }
    STICKMON_TRACEF("[Settings] brightness t=%lu current=%u next=%u display=%u\n",
                    (unsigned long)Hal::ins().millis(), cur, next,
                    Hal::ins().getDisplayBrightness());
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
    Hal::ins().setAudioVolume(volume);
}

void SettingsScene::markSettingsDirty() {
    GameEngine::ins().markDirty(SaveUrgency::SOON);
}

void SettingsScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));
    if (viewMode == ViewMode::HELP) {
        renderHelp();
    } else {
        renderMenu();
        if (viewMode == ViewMode::RESET_CONFIRM) renderResetConfirm();
    }
    renderToast();
}

void SettingsScene::renderMenu() {
    auto& c = PixelRenderer::canvas();
    static_assert(sizeof(Ui::Settings::ITEMS) / sizeof(Ui::Settings::ITEMS[0]) == COUNT,
                  "Settings labels must match SettingsScene::Item");
    static constexpr int ROW_H = 24;
    static constexpr int START_Y = 6;
    static constexpr int TEXT_Y_OFFSET = 4;
    static constexpr int INDICATOR_X = 8;
    static constexpr int CONTENT_X = 20;
    static constexpr int VALUE_X = 156;
    static constexpr int SEPARATOR_W = Hal::DISPLAY_W - CONTENT_X * 2;
    for (int i = 0; i < COUNT; ++i) {
        int y = START_Y + i * ROW_H - (int)menuScroll;
        if (y + ROW_H <= 0 || y >= Hal::DISPLAY_H) continue;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        int textY = y + TEXT_Y_OFFSET;
        if (selected) {
            c.fillRect(INDICATOR_X, textY, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(CONTENT_X, textY, Ui::Settings::ITEMS[i], fg, 1);

        char value[16] = "";
        const auto& settings = GameEngine::ins().gameState().settings;
        if (i == BRIGHTNESS) snprintf(value, sizeof(value), "%u", Hal::ins().getBrightness());
        if (i == GAME_SPEED) snprintf(value, sizeof(value), "%.0fx", GameEngine::ins().gameSpeed());
        if (i == VOLUME) snprintf(value, sizeof(value), "%u%%", settings.volume);
        if (i == POWER_SAVE) snprintf(value, sizeof(value), "%s", GameEngine::ins().idleTimeoutLabel());
        if (value[0]) PixelRenderer::text(VALUE_X, textY, value, PixelRenderer::rgb(135, 214, 238), 1);

        if (i < COUNT - 1) {
            c.drawFastHLine(CONTENT_X, y + ROW_H - 1, SEPARATOR_W,
                            PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SettingsScene::renderHelp() {
    auto& c = PixelRenderer::canvas();
    static constexpr uint8_t HELP_PAGE_COUNT =
        sizeof(Ui::Settings::HELP_PAGE_TITLES) /
        sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
    static constexpr uint8_t HELP_LINE_COUNT =
        sizeof(Ui::Settings::HELP_PAGE_LINES[0]) /
        sizeof(Ui::Settings::HELP_PAGE_LINES[0][0]);
    static_assert(
        sizeof(Ui::Settings::HELP_PAGE_LINES) /
                sizeof(Ui::Settings::HELP_PAGE_LINES[0]) ==
            HELP_PAGE_COUNT,
        "help page titles and content must match");
    if (helpPage >= HELP_PAGE_COUNT) helpPage = 0;

    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(10, 14, 20));
    PixelRenderer::text(12, 8, Ui::Settings::HELP_PAGE_TITLES[helpPage],
                        PixelRenderer::rgb(67, 213, 224), 1);
    char page[12];
    snprintf(page, sizeof(page), "%u/%u", helpPage + 1, HELP_PAGE_COUNT);
    PixelRenderer::text(198, 8, page,
                        PixelRenderer::rgb(156, 164, 176), 1);
    c.drawFastHLine(8, 27, 216, PixelRenderer::rgb(55, 63, 76));
    for (uint8_t line = 0; line < HELP_LINE_COUNT; ++line) {
        PixelRenderer::text(
            12, 31 + line * 19,
            Ui::Settings::HELP_PAGE_LINES[helpPage][line],
            line == 2 && helpPage + 1 == HELP_PAGE_COUNT
                ? PixelRenderer::rgb(255, 216, 72)
                : PixelRenderer::rgb(241, 242, 232),
            1);
    }
    const char* footer = helpPage + 1 == HELP_PAGE_COUNT
        ? Ui::Settings::HELP_REPLAY
        : Ui::Settings::HELP_NEXT;
    PixelRenderer::text(88, 114, footer,
                        PixelRenderer::rgb(135, 214, 238), 1);
}

void SettingsScene::renderResetConfirm() {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 20;
    static constexpr int POP_Y = 18;
    static constexpr int POP_W = 200;
    static constexpr int POP_H = 98;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    PixelRenderer::text(88, POP_Y + 8, Ui::Settings::RESET_GAME,
                        PixelRenderer::rgb(255, 216, 72), 1);
    PixelRenderer::text(48, POP_Y + 32, Ui::Settings::RESET_WARNING,
                        PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(72, POP_Y + 51, Ui::Settings::RESET_QUESTION,
                        PixelRenderer::rgb(156, 164, 176), 1);

    uint16_t yesColor = resetConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !resetConfirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(62, POP_Y + 74, Ui::Settings::RESET_YES, yesColor, 1);
    PixelRenderer::text(150, POP_Y + 74, Ui::Settings::RESET_NO, noColor, 1);
}

void SettingsScene::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}
