#include "ui/SettingsScreens.h"

#include <algorithm>
#include <cstdio>

#include "core/BuildConfig.h"
#include "core/FontResource.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiCommon.h"
#include "ui/UiMetrics.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {

using UiCommon::rgb;
using UiCommon::text;
using UiCommon::drawToast;

namespace {

constexpr int MENU_CONTENT_TOP = UiMetrics::CONTENT_TOP;
constexpr int SETTINGS_SLIDER_ROW_HEIGHT = 64;
constexpr int SETTINGS_ITEM_ROW_HEIGHT = 60;

constexpr int settingsRowTop(int index) {
    return MENU_CONTENT_TOP +
        (index < 2 ? index * SETTINGS_SLIDER_ROW_HEIGHT
                   : 2 * SETTINGS_SLIDER_ROW_HEIGHT +
                         (index - 2) * SETTINGS_ITEM_ROW_HEIGHT);
}

constexpr int settingsRowHeight(int index) {
    return index < 2 ? SETTINGS_SLIDER_ROW_HEIGHT : SETTINGS_ITEM_ROW_HEIGHT;
}

constexpr int settingsItemCount(bool systemUpdate) {
    return STICKMON_ENABLE_DEBUG_FEATURES && systemUpdate
        ? SETTINGS_SYSTEM_UPDATE_ITEM + 1 : SETTINGS_RESET_GAME_ITEM + 1;
}

void drawResetConfirmation(Canvas565& canvas, const SettingsViewModel& model) {
    UiCommon::drawPageHeader(canvas, Ui::Settings::RESET_GAME);
    text(canvas, (AmoledUi::WIDTH -
                  UiCommon::textWidth(Ui::Settings::RESET_WARNING)) / 2,
         156, Ui::Settings::RESET_WARNING, rgb(239, 143, 148));
    text(canvas, (AmoledUi::WIDTH -
                  UiCommon::textWidth(Ui::Settings::RESET_QUESTION)) / 2,
         222, Ui::Settings::RESET_QUESTION, rgb(226, 238, 233));

    const AmoledUi::Rect buttons[] = {
        UiMetrics::DETAIL_ACTION_RECT, UiMetrics::DETAIL_BACK_RECT,
    };
    const char* labels[] = {Ui::Settings::RESET_NO, Ui::Settings::RESET_YES};
    for (int choice = 0; choice < 2; ++choice) {
        const auto& button = buttons[choice];
        const bool pressed = model.pressedItem == choice;
        const uint16_t fill = choice == 0
            ? (pressed ? rgb(53, 76, 83) : rgb(36, 54, 61))
            : (pressed ? rgb(129, 59, 68) : rgb(91, 49, 55));
        canvas.fillRoundRect(button.x, button.y, button.width, button.height,
                             8, fill);
        text(canvas, button.x +
                         (button.width - UiCommon::textWidth(labels[choice])) / 2,
             button.y + (button.height - FontResource::LARGE_GLYPH_H) / 2,
             labels[choice], choice == 0 ? rgb(226, 238, 233)
                                        : rgb(239, 143, 148));
    }
}

void drawSettingsSlider(Canvas565& canvas, int y, uint8_t value,
                        uint8_t minimum, uint8_t maximum, bool pressed) {
    const int trackY = y + SETTINGS_SLIDER_ROW_HEIGHT / 2;
    const int trackWidth = SETTINGS_SLIDER_RIGHT - SETTINGS_SLIDER_LEFT;
    const int range = std::max<int>(1, maximum - minimum);
    const int clamped = std::clamp<int>(value, minimum, maximum);
    const int knobX = SETTINGS_SLIDER_LEFT +
        (clamped - minimum) * trackWidth / range;
    const uint16_t track = pressed ? rgb(64, 83, 88) : rgb(48, 63, 70);
    const uint16_t active = pressed ? rgb(115, 226, 183) : rgb(83, 184, 157);
    canvas.fillRoundRect((SETTINGS_SLIDER_LEFT), (trackY - 8), (trackWidth), (16), (8), track);
    if (knobX > SETTINGS_SLIDER_LEFT) {
        canvas.fillRoundRect((SETTINGS_SLIDER_LEFT), (trackY - 8), (knobX - SETTINGS_SLIDER_LEFT), (16), (8), active);
    }
    canvas.fillCircle((knobX), (trackY), (pressed ? 14 : 12), rgb(226, 238, 233));
    canvas.fillCircle((knobX), (trackY), (pressed ? 8 : 6), active);
}

}  // namespace

bool settingsBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

int settingsItemAt(int x, int y, bool systemUpdate) {
    if (x < 12 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
    const int count = settingsItemCount(systemUpdate);
    for (int index = 0; index < count; ++index) {
        const int top = settingsRowTop(index);
        if (y >= top && y < top + settingsRowHeight(index)) return index;
    }
    return -1;
}

int settingsResetChoiceAt(int x, int y) {
    if (UiMetrics::DETAIL_ACTION_RECT.contains(x, y)) return 0;
    if (UiMetrics::DETAIL_BACK_RECT.contains(x, y)) return 1;
    return -1;
}

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* LABELS[] = {
        Ui::BRIGHTNESS, Ui::Amoled::VOLUME, Ui::Amoled::GAME_SPEED,
        Ui::Amoled::POWER_SAVE,
        Ui::Settings::RESET_GAME, Ui::Amoled::SYSTEM_UPDATE,
    };
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    if (model.resetConfirm) {
        drawResetConfirmation(canvas, model);
        drawToast(canvas, model.toast);
        return;
    }
    UiCommon::drawPageHeader(canvas, Ui::SETTINGS);
    const int count = settingsItemCount(model.systemUpdate);
    for (int index = 0; index < count; ++index) {
        int y = settingsRowTop(index);
        const int textY = y + (settingsRowHeight(index) -
                              FontResource::LARGE_GLYPH_H) / 2;
        bool pressed = index == model.pressedItem;
        if (pressed) {
            canvas.fillRect((12), (y + 4), (344),
                            (settingsRowHeight(index) - 8), rgb(42, 61, 68));
        }
        text(canvas, 28, textY, LABELS[index],
             index == SETTINGS_RESET_GAME_ITEM ? rgb(239, 143, 148)
                                                : rgb(226, 238, 233));
        char value[20] = {};
        if (model.state) {
            const auto& settings = model.state->settings;
            if (index == 0) {
                drawSettingsSlider(canvas, y, model.brightness, 32, 255,
                                   pressed);
            }
            if (index == 1) {
                drawSettingsSlider(canvas, y, model.volume, 0, 100, pressed);
            }
            if (index == 2) std::snprintf(value, sizeof(value), "%ux",
                                          1U << std::min<uint8_t>(settings.speedIndex, 3));
            if (index == 3) {
                static constexpr const char* POWER[] = {
                    Ui::Amoled::IDLE_30S, Ui::Amoled::IDLE_2MIN,
                    Ui::Amoled::IDLE_5MIN, Ui::Amoled::IDLE_10MIN,
                    Ui::Settings::IDLE_NEVER,
                };
                std::snprintf(value, sizeof(value), "%s",
                              POWER[std::min<uint8_t>(settings.idleTimeoutIndex, 4)]);
            }
        }
        if (value[0]) {
            text(canvas, 356 - UiCommon::textWidth(value), textY, value,
                 rgb(248, 210, 105));
        }
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}

}  // namespace AmoledV1
