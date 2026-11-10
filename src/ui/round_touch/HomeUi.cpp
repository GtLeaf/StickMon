// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/HomeUi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "ui/round_touch/RoomControls.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"

namespace Ui {
using namespace RoundRoom;
namespace {
bool visibleToast(const char* text) {
    return text && std::strcmp(text, Ui::Menu::FOOD_ADDED) != 0;
}

void drawClock20(Canvas565& canvas, const char* clock) {
    // Resize only the five ASCII clock cells from 8x16 to 10x20.
    uint16_t pixels[40 * 16] = {};
    Canvas565 textCanvas;
    textCanvas.attach({pixels, 40, 16, false});
    PixelRenderer::text(textCanvas, 0, 0, clock, 0xffff);
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 50; ++x) {
            if (pixels[(y * 16 / 20) * 40 + x * 40 / 50])
                canvas.drawPixel(142 + x, 14 + y, 0xffff);
        }
    }
}
}  // namespace

int HomeUi::touchTarget(int16_t x, int16_t y) {
    const auto frame = host.homeFrame();
    if (!frame.touchBlocked) {
        if (HOME_MENU_RECT.contains(x, y)) return 2;
    }
    return HomeUiContent::touchTarget(x, y, frame, camera, false);
}

bool HomeUi::onTap(int target) {
    return HomeUiBase::onTap(target);
}

bool HomeUi::onButton(const ButtonEvent& event) {
    return HomeUiBase::onButton(event);
}

void HomeUi::drawHud(const HomeFrame& frame) {
    auto& canvas = PixelRenderer::canvas();
    char clock[8];
    std::snprintf(clock, sizeof(clock), "%02u:%02u",
        frame.gameMinutes / 60, frame.gameMinutes % 60);
    drawClock20(canvas, clock);
    HudRenderer::drawBattery(canvas, 200, 20, Hal::ins().batteryLevel(), 1,
        Platform::power().batteryCharging());
    const uint8_t count = std::min<uint8_t>(frame.hudCount, Game::TEAM_CAP);
    constexpr int statusWidth = 72, statusGap = 16;
    const int firstX = (360 - count * statusWidth - (count ? count - 1 : 0) * statusGap) / 2;
    for (uint8_t i = 0; i < count; ++i) {
        const auto& row = frame.hud[i];
        constexpr int y = 42, barWidth = 50;
        const int x = firstX + i * (statusWidth + statusGap);
        HudRenderer::drawHungerIcon(canvas, x, y,
            displayedHunger(row.slot, row.hunger, frame.nowMs), 1);
        const int barX = x + 22;
        canvas.fillRoundRect(barX, y + 3, barWidth, 8, 2, PixelRenderer::rgb(39, 45, 50));
        const int filled = row.hp ? std::max(1, (barWidth - 2) * row.hp / 100) : 0;
        const uint16_t color = row.hp > 50 ? PixelRenderer::rgb(92, 222, 112)
            : row.hp > 20 ? PixelRenderer::rgb(246, 204, 72) : PixelRenderer::rgb(232, 80, 84);
        if (filled) canvas.fillRoundRect(barX + 1, y + 4, filled, 6, 1, color);
        canvas.drawRoundRect(barX, y + 3, barWidth, 8, 2, PixelRenderer::rgb(220, 224, 218));
    }
    if (!lastSaveOk) MenuGrid::label(canvas, 180, 264, SAVE_FAILED, 200, 0xf800);
}

void HomeUi::drawToast() {
    if (!visibleToast(toast)) return;
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRoundRect(76, 264, 208, 28, 5, PixelRenderer::rgb(27, 37, 47));
    MenuGrid::label(canvas, 180, 270, toast, 192, 0xffff);
}

void HomeUi::drawHomeMenu(const HomeFrame& frame) {
    if (frame.touchBlocked) return;
    auto& canvas = PixelRenderer::canvas();
    drawMenuButton();
    if (frame.tutorialVisible && !visibleToast(toast)) {
        const char* hint = frame.tutorialLongPress ? "点击底部菜单" :
            frame.tutorialButton == 1 ? "点击食碗喂食" : "点击精灵，与它互动";
        canvas.fillRoundRect(76, 230, 208, 28, 6, PixelRenderer::rgb(23, 37, 49));
        MenuGrid::label(canvas, 180, 236, hint, 192, 0xffe0);
    }
}

void HomeUi::drawContact(const HomeFrame& frame) {
    HomeUiContent::drawContact(frame, 76, 218, 208);
}
}  // namespace Ui
