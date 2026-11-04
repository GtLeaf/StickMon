#include "ui/stick/MainMenuUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "assets/MenuAssets.h"
#include "core/UiMotion.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"
#include "platform/api/FlashStorage.h"

namespace Ui {
namespace {
int textPixelWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

}  // namespace

void MainMenuUi::enter(bool fromHome) {
    cancelTouch();
    resetState(fromHome);
    animCursor = static_cast<float>(cursor);
    lastBatteryLevel = Hal::ins().filteredBatteryLevel();
}

SceneUpdateResult MainMenuUi::update(uint32_t, float) {
    RenderDemand demand;
    const auto step = UiMotion::lerp(animCursor, float(cursor), 0.25f);
    demand.changed(step.changed);
    demand.animate(step.active);
    const int battery = Hal::ins().filteredBatteryLevel();
    if (battery != lastBatteryLevel) { lastBatteryLevel = battery; demand.redraw(); }
    demand.wakeIn(30000);
    return demand.result();
}

bool MainMenuUi::onButton(const ButtonEvent& event) {
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        if (advanceCursor()) animCursor = 0.0f;
        return true;
    }
    return handleButton(event);
}

bool MainMenuUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    return Scene::onTouch(event);
}

void MainMenuUi::cancelTouch() { Scene::cancelTouch(); }

int MainMenuUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Hal::DISPLAY_W || y < 0 || y >= TouchUi::CONTENT_H) return -1;
    if (x < 12 || x >= Hal::DISPLAY_W - 8) return -1;
    for (int i = 0; i < ITEM_COUNT; ++i) {
        const int cy = TouchUi::CONTENT_H / 2 + int((i - animCursor) * 50);
        if (std::abs(y - cy) <= 23) return i;
    }
    return -1;
}

bool MainMenuUi::onTap(int target) {
    if (target >= 128) return false;
    cursor = target;
    animCursor = cursor;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

void MainMenuUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, 0x0000);
    int batt = Hal::ins().filteredBatteryLevel();
    char battBuf[8];
    if (Platform::power().batteryLevel() < 0) snprintf(battBuf, sizeof(battBuf), "--");
    else snprintf(battBuf, sizeof(battBuf), Ui::Menu::TITLE_BATTERY_FMT, batt);
    PixelRenderer::text(Hal::DISPLAY_W - textPixelWidth(battBuf) - 6, 6, battBuf,
                        batt > 20 ? 0x07E0 : 0xF800,
                        1);

    constexpr int centerY = TouchUi::CONTENT_H / 2;
    constexpr int spacing = 50;
    constexpr int boxW = 60;
    constexpr int boxH = 40;
    constexpr int iconSlotW = 60;
    constexpr int boxX = 20;

    int order[ITEM_COUNT];
    for (int i = 0; i < ITEM_COUNT; ++i) order[i] = i;
    std::sort(order, order + ITEM_COUNT, [this](int a, int b) {
        return fabsf((float)a - animCursor) < fabsf((float)b - animCursor);
    });

    for (int k = 0; k < ITEM_COUNT; ++k) {
        int i = order[k];
        float rawOffset = (float)i - animCursor;
        int y = centerY + (int)(rawOffset * spacing);

        bool isSelected = fabsf(rawOffset) < 0.5f;
        float relScale = isSelected ? 1.15f : 1.0f;
        uint16_t descColor = isSelected ? 0xFFFF : 0x7BEF;
        int leftW = boxW;

        int iconIndex = AppSceneFlow::mainMenuEntry(
            static_cast<uint8_t>(i),
            STICKMON_ENABLE_DEBUG_FEATURES != 0).iconIndex;
        if (iconIndex >= 0 && iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            if (isSelected) {
                c.fillRect(boxX - 4, y - boxH / 2, 2, boxH, 0xFFE0);
            }

            uint16_t offset = FlashStorage::readWord(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].offset);
            uint16_t length = FlashStorage::readWord(&MenuAssets::MAIN_ICON_FRAMES[iconIndex].length);
            int scaledIconW = (int)(MenuAssets::FRAME_W * relScale);
            int scaledIconH = (int)(MenuAssets::FRAME_H * relScale);
            int iconX = boxX + (iconSlotW - scaledIconW) / 2;
            int iconY = y - scaledIconH / 2;
            PixelRenderer::drawRgb565RleScaled(iconX, iconY, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                                               MenuAssets::MAIN_ICON_RLE, offset, length, relScale);
            leftW = iconSlotW;
        } else {
            int drawBoxW = (int)(boxW * relScale);
            int drawBoxH = (int)(boxH * relScale);
            uint16_t boxColor = isSelected ? 0xFFE0 : 0x7BEF;
            if (isSelected) {
                c.fillRect(boxX - 4, y - boxH / 2, 2, boxH, 0xFFE0);
            }
            c.fillRect(boxX, y - drawBoxH / 2, drawBoxW, drawBoxH, boxColor);
            leftW = drawBoxW;
        }

        const char* desc = Ui::Menu::ITEMS[i];
        int tw = textPixelWidth(desc);
        int th = 16;
        int descX = (boxX + leftW + 10 + Hal::DISPLAY_W) / 2 - 10;
        int descY = y - th / 2;
        PixelRenderer::text(descX - tw / 2, descY, desc, descColor, 1);
    }
}

}  // namespace Ui
