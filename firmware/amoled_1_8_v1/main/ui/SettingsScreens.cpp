#include "ui/SettingsScreens.h"

#include <algorithm>
#include <cstdio>

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
constexpr int SETTINGS_ROW_HEIGHT = 62;

void drawSettingsSlider(Canvas565& canvas, int y, uint8_t value,
                        uint8_t minimum, uint8_t maximum, bool pressed) {
    const int trackY = y + SETTINGS_SLIDER_OFFSET_Y;
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

int settingsItemAt(int x, int y) {
    if (x < 12 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
    int index = (y - MENU_CONTENT_TOP) / SETTINGS_ROW_HEIGHT;
    return index < 6 ? index : -1;
}

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* LABELS[] = {
        Ui::BRIGHTNESS, Ui::Amoled::VOLUME, Ui::Amoled::GAME_SPEED,
        Ui::Amoled::POWER_SAVE, Ui::Amoled::VOICE_CALL, Ui::BACK,
    };
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    UiCommon::drawPageHeader(canvas, Ui::SETTINGS);
    for (int index = 0; index < 6; ++index) {
        int y = MENU_CONTENT_TOP + index * SETTINGS_ROW_HEIGHT;
        bool pressed = index == model.pressedItem;
        if (pressed) canvas.fillRect((12), (y + 4), (344), (54), rgb(42, 61, 68));
        text(canvas, 28, y + 19, LABELS[index],
             index == 5 ? rgb(115, 226, 183) : rgb(226, 238, 233));
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
            if (index == 4) std::snprintf(value, sizeof(value), "%s",
                                          settings.voiceCallEnabled
                                              ? Ui::Amoled::VALUE_ON
                                              : Ui::Amoled::VALUE_OFF);
        }
        if (value[0]) text(canvas, 284, y + 19, value, rgb(248, 210, 105));
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}

}  // namespace AmoledV1
