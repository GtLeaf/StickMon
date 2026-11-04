#include "ui/stick/SettingsUi.h"

#include <cstdio>
#include "core/MathUtil.h"
#include "core/UiMotion.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {

void SettingsUi::onEnter() {
    cancelTouch();
    resetState();
    menuScroll = 0.0f;
    host.initializeSettings(false);
}

SceneUpdateResult SettingsUi::update(uint32_t nowMs, float) {
    RenderDemand demand;
    if (viewMode == ViewMode::MENU) {
        static constexpr int ROW_H = 24;
        static constexpr int START_Y = 6;
        const int contentH = START_Y * 2 + COUNT * ROW_H;
        const int maxScroll =
            contentH > TouchUi::CONTENT_H ? contentH - TouchUi::CONTENT_H : 0;
        int targetScroll =
            START_Y + cursor * ROW_H + ROW_H / 2 - TouchUi::CONTENT_H / 2;
        targetScroll = MathUtil::clamp(targetScroll, 0, maxScroll);
        const auto step = UiMotion::lerp(
            menuScroll, static_cast<float>(targetScroll), 0.25f, 0.5f);
        demand.changed(step.changed);
        demand.animate(step.active);
    }
    const auto notice = updateNotice(nowMs);
    demand.changed(notice.redraw);
    demand.wakeIn(notice.nextUpdateDelayMs);
    return demand.result();
}

bool SettingsUi::onButton(const ButtonEvent& event) {
    if (viewMode == ViewMode::MENU && event.btn == 1 && event.action == BtnAction::PRESSED) {
        cursor = (cursor + 1) % COUNT;
        if (cursor == 0) menuScroll = 0.0f;
        return true;
    }
    return handleButton(event);
}

void SettingsUi::cancelTouch() { Scene::cancelTouch(); }

bool SettingsUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    return Scene::onTouch(event);
}

int SettingsUi::touchTarget(int16_t x, int16_t y) {
    if (viewMode != ViewMode::MENU || y < 6 || x < 8 || y >= TouchUi::CONTENT_H) return -1;
    const int row = (y - 6 + int(menuScroll)) / 24;
    return row < COUNT ? row : -1;
}

bool SettingsUi::onTap(int target) {
    if (viewMode != ViewMode::MENU || target < 0 || target >= COUNT) return false;
    cursor = target;
    return onButton({0, BtnAction::PRESSED});
}

void SettingsUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(7, 9, 14));
    if (viewMode == ViewMode::HELP) {
        renderHelp();
    } else {
        renderMenu();
        if (viewMode == ViewMode::RESET_CONFIRM) renderResetConfirm();
    }
    renderToast();
}

void SettingsUi::renderMenu() {
    auto& c = PixelRenderer::canvas();
    static_assert(sizeof(Ui::Settings::ITEMS) / sizeof(Ui::Settings::ITEMS[0]) == COUNT,
                  "Settings labels must match SettingsPage::Item");
    static constexpr int ROW_H = 24;
    static constexpr int START_Y = 6;
    static constexpr int TEXT_Y_OFFSET = 4;
    static constexpr int INDICATOR_X = 8;
    static constexpr int CONTENT_X = 20;
    static constexpr int VALUE_X = 156;
    static constexpr int SEPARATOR_W = Hal::DISPLAY_W - CONTENT_X * 2;
    for (int i = 0; i < COUNT; ++i) {
        int y = START_Y + i * ROW_H - (int)menuScroll;
        if (y + ROW_H <= 0 || y >= TouchUi::CONTENT_H) continue;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        int textY = y + TEXT_Y_OFFSET;
        if (selected) {
            c.fillRect(INDICATOR_X, textY, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(CONTENT_X, textY, Ui::Settings::ITEMS[i], fg, 1);

        char value[16] = "";
        const auto& settings = host.settingsState().settings;
        if (i == BRIGHTNESS) snprintf(value, sizeof(value), "%u", host.settingsBrightness());
        if (i == GAME_SPEED) snprintf(value, sizeof(value), "%.0fx", host.settingsGameSpeed());
        if (i == VOLUME) snprintf(value, sizeof(value), "%u%%", settings.volume);
        if (i == POWER_SAVE) snprintf(value, sizeof(value), "%s", host.settingsIdleLabel());
        if (value[0]) PixelRenderer::text(VALUE_X, textY, value, PixelRenderer::rgb(135, 214, 238), 1);

        if (i < COUNT - 1) {
            c.drawFastHLine(CONTENT_X, y + ROW_H - 1, SEPARATOR_W,
                            PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void SettingsUi::renderHelp() {
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

    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(10, 14, 20));
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

void SettingsUi::renderResetConfirm() {
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

void SettingsUi::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

}  // namespace Ui
