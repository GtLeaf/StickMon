#include "ui/compact_touch/SettingsUi.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "ui/TouchHelp.h"

namespace Ui {

constexpr uint8_t SettingsUi::TOUCH_ORDER[];

namespace {
MenuGrid::Layout pageLayout() {
    return MenuGrid::fit({0, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH,
        Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H},
        Ui::Page::WIDTH - 16, 44, 0, 6, 8, 4);
}

constexpr int SLIDER_LEFT = 120;
constexpr int SLIDER_RIGHT = Ui::Page::WIDTH - 24;
constexpr int HELP_PAGE_COUNT = sizeof(Ui::Settings::HELP_PAGE_TITLES) / sizeof(Ui::Settings::HELP_PAGE_TITLES[0]);
}  // namespace

void SettingsUi::onEnter() {
    cancelTouch();
    resetState();
    touchScroll = 0;
    host.initializeSettings(true);
}

SceneUpdateResult SettingsUi::update(uint32_t nowMs, float) {
    return updateNotice(nowMs);
}

void SettingsUi::cancelTouch() {
    Scene::cancelTouch();
    pageTouch.cancel();
    sliderCandidate = -1;
    sliderDragging = sliderChanged = false;
}

bool SettingsUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    using Type = Platform::IInputDevice::TouchType;
    const int target = touchTarget(event.x, event.y);
    if (event.type == Type::CANCEL) {
        const bool changed = pageTouch.pressed() >= 0 || sliderDragging;
        cancelTouch();
        return changed;
    }
    if (event.type == Type::DOWN) {
        cancelTouch();
        sliderStartX = event.x; sliderStartY = event.y;
        if (viewMode == ViewMode::MENU && (target == 16 + BRIGHTNESS || target == 16 + VOLUME))
            sliderCandidate = target - 16;
    }
    if (event.type == Type::MOVE && sliderCandidate >= 0 && !sliderDragging) {
        const int dx = std::abs(event.x - sliderStartX), dy = std::abs(event.y - sliderStartY);
        if (std::max(dx, dy) > 10) {
            if (dx > dy) { sliderDragging = true; pageTouch.cancel(); }
            else sliderCandidate = -1;
        }
    }
    if (sliderDragging && (event.type == Type::MOVE || event.type == Type::UP)) {
        sliderChanged = setSliderValue(sliderCandidate, event.x) || sliderChanged;
        if (event.type == Type::UP) {
            if (sliderCandidate == VOLUME && sliderChanged) host.previewSettingsVolume();
            cancelTouch();
        }
        return true;
    }
    const auto layout = pageLayout();
    const auto result = pageTouch.update(event, target, touchScroll, layout.maxScroll(COUNT),
        viewMode == ViewMode::MENU && layout.viewport.contains(event.x, event.y));
    if (result.tap == 16 + BRIGHTNESS || result.tap == 16 + VOLUME) {
        const int item = result.tap - 16;
        const bool changed = setSliderValue(item, event.x);
        if (changed && item == VOLUME) host.previewSettingsVolume();
        cancelTouch();
        return true;
    }
    if (event.type == Type::UP) sliderCandidate = -1;
    // Activation may synchronously destroy this page through its host.
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

bool SettingsUi::setSliderValue(int item, int x) {
    if (viewMode != ViewMode::MENU || (item != BRIGHTNESS && item != VOLUME)) return false;
    const int minimum = item == BRIGHTNESS ? 32 : 0;
    const int maximum = item == BRIGHTNESS ? 255 : 100;
    const int width = SLIDER_RIGHT - SLIDER_LEFT;
    const uint8_t value = minimum + ((std::clamp(x, SLIDER_LEFT, SLIDER_RIGHT) - SLIDER_LEFT) * (maximum - minimum) + width / 2) / width;
    cursor = item;
    return host.setSettingValue(static_cast<Item>(item), value);
}

int SettingsUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const int mode = static_cast<int>(viewMode) * 256;
    if (TouchUi::safeBack().contains(x, y)) return mode + 230;
    if (viewMode != ViewMode::MENU) {
        if (y >= Ui::Page::HEIGHT - 40 && y < Ui::Page::HEIGHT - 6) {
            if (x >= 8 && x < Ui::Page::WIDTH / 2 - 6) return mode + 200;
            if (x >= Ui::Page::WIDTH / 2 + 6 && x < Ui::Page::WIDTH - 8) return mode + 201;
        }
        return -1;
    }
    const int row = pageLayout().itemAt(x, y, touchScroll, COUNT);
    if (row < 0) return -1;
    const int item = TOUCH_ORDER[row];
    if ((item == BRIGHTNESS || item == VOLUME) && x >= SLIDER_LEFT - 12) return 16 + item;
    return item;
}

bool SettingsUi::onTap(int target) {
    if (target / 256 != static_cast<int>(viewMode)) return false;
    const int item = target % 256;
    if (item == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (viewMode == ViewMode::HELP) {
        if (item == 200) { helpPage = (helpPage + HELP_PAGE_COUNT - 1) % HELP_PAGE_COUNT; return true; }
        if (item == 201) return onButton({uint8_t(helpPage + 1 == HELP_PAGE_COUNT ? 0 : 1), BtnAction::PRESSED});
        return false;
    }
    if (viewMode == ViewMode::RESET_CONFIRM) {
        if (item != 200 && item != 201) return false;
        resetConfirmYes = item == 201;
        return onButton({0, BtnAction::PRESSED});
    }
    if (viewMode != ViewMode::MENU || target < 0 || target >= COUNT) return false;
    cursor = target;
    return onButton({0, BtnAction::PRESSED});
}

bool SettingsUi::onButton(const ButtonEvent& event) {
    if (viewMode == ViewMode::MENU && event.btn == 1 && event.action == BtnAction::PRESSED) {
        int index = 0;
        while (index < COUNT && TOUCH_ORDER[index] != cursor) ++index;
        index = (index + 1) % COUNT;
        cursor = TOUCH_ORDER[index];
        touchScroll = pageLayout().reveal(index, touchScroll, COUNT);
        return true;
    }
    return handleButton(event);
}

void SettingsUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    const int mode = static_cast<int>(viewMode) * 256;
    const auto& settings = host.settingsState().settings;
    if (viewMode == ViewMode::MENU) {
        const auto layout = pageLayout();
        c.setClipRect(0, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH, Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H);
        for (int row = 0; row < COUNT; ++row) {
            const auto rect = layout.cell(row, touchScroll);
            if (rect.y + rect.height <= TouchUi::SAFE_HEADER_H || rect.y >= Ui::Page::HEIGHT) continue;
            const int item = TOUCH_ORDER[row];
            const bool pressed = pageTouch.pressed() == item || pageTouch.pressed() == 16 + item ||
                (sliderDragging && sliderCandidate == item);
            MenuGrid::card(c, rect, pressed, item == cursor);
            PixelRenderer::text(rect.x + 10, rect.y + 14, Ui::Settings::ITEMS[item], item == RESET_GAME ? 0xf471 : 0xffff);
            if (item == BRIGHTNESS || item == VOLUME) {
                TouchUi::slider(SLIDER_LEFT, SLIDER_RIGHT, rect.y + 22,
                    item == BRIGHTNESS ? host.settingsBrightness() : settings.volume,
                    item == BRIGHTNESS ? 32 : 0, item == BRIGHTNESS ? 255 : 100, pressed);
            } else {
                const char* value = ">";
                char speed[12];
                if (item == GAME_SPEED) { std::snprintf(speed, sizeof(speed), "%.0fx", host.settingsGameSpeed()); value = speed; }
                if (item == POWER_SAVE) value = host.settingsIdleLabel();
                MenuGrid::label(c, Ui::Page::WIDTH - 48, rect.y + 14, value, 72, 0xffe0);
            }
        }
        c.clearClipRect();
    } else if (viewMode == ViewMode::HELP) {
        helpPage %= HELP_PAGE_COUNT;
        MenuGrid::card(c, {8, TouchUi::SAFE_HEADER_H + 6, Ui::Page::WIDTH - 16, 144}, false);
        PixelRenderer::text(18, TouchUi::SAFE_HEADER_H + 16, Ui::TouchHelp::title(helpPage), 0x77b6);
        char page[12]; std::snprintf(page, sizeof(page), "%u/%u", helpPage + 1, HELP_PAGE_COUNT);
        PixelRenderer::text(Ui::Page::WIDTH - 44, TouchUi::SAFE_HEADER_H + 16, page, 0x7bef);
        for (int i = 0; i < 4; ++i)
            PixelRenderer::text(18, TouchUi::SAFE_HEADER_H + 46 + i * 24, Ui::TouchHelp::line(helpPage, i), 0xffff);
        const int width = Ui::Page::WIDTH / 2 - 14;
        TouchUi::action(8, Ui::Page::HEIGHT - 40, width, "<", pageTouch.pressed() == mode + 200);
        TouchUi::action(Ui::Page::WIDTH / 2 + 6, Ui::Page::HEIGHT - 40, width,
            helpPage + 1 == HELP_PAGE_COUNT ? Ui::Settings::HELP_PAGE_TITLES[HELP_PAGE_COUNT - 1] : ">",
            pageTouch.pressed() == mode + 201);
    } else {
        MenuGrid::card(c, {8, 50, Ui::Page::WIDTH - 16, 128}, false);
        MenuGrid::label(c, Ui::Page::WIDTH / 2, 80, Ui::Settings::RESET_WARNING, Ui::Page::WIDTH - 32, 0xf471);
        MenuGrid::label(c, Ui::Page::WIDTH / 2, 120, Ui::Settings::RESET_QUESTION, Ui::Page::WIDTH - 32, 0xffff);
        const int width = Ui::Page::WIDTH / 2 - 14;
        TouchUi::action(8, Ui::Page::HEIGHT - 40, width, Ui::Settings::RESET_NO, pageTouch.pressed() == mode + 200);
        const int x = Ui::Page::WIDTH / 2 + 6, y = Ui::Page::HEIGHT - 40;
        c.fillRoundRect(x, y, width, 34, 4,
            pageTouch.pressed() == mode + 201 ? PixelRenderer::rgb(129, 59, 68) : PixelRenderer::rgb(91, 49, 55));
        c.drawRoundRect(resetConfirmYes ? x : 8, y, width, 34, 4, 0xffe0);
        MenuGrid::label(c, x + width / 2, y + 9, Ui::Settings::RESET_YES, width - 8, 0xf471);
    }
    TouchUi::safePageHeader(viewMode == ViewMode::HELP ? Ui::Settings::HELP :
        viewMode == ViewMode::RESET_CONFIRM ? Ui::Settings::RESET_GAME : Ui::SETTINGS,
        pageTouch.pressed() == mode + 230);
    if (toast) {
        c.fillRoundRect(8, Ui::Page::HEIGHT - 72, Ui::Page::WIDTH - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
        MenuGrid::label(c, Ui::Page::WIDTH / 2, Ui::Page::HEIGHT - 67, toast, Ui::Page::WIDTH - 32, 0xffff);
    }
}


}  // namespace Ui
