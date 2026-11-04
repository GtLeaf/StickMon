#include "ui/stick/HomeUi.h"

#include <cstdio>
#include "hardware/Hal.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"

namespace Ui {

void HomeUi::focus(float x, float y) {
    focusCamera(x, y, RoomRenderer::PIXEL_SCALE == 1
        ? 84.0f : Hal::DISPLAY_H / (2.0f * RoomRenderer::PIXEL_SCALE));
}

int HomeUi::touchTarget(int16_t x, int16_t y) {
    return HomeUiContent::touchTarget(x, y, host.homeFrame(), camera, false);
}

void HomeUi::drawHud(const HomeFrame& frame) {
    auto& canvas = PixelRenderer::canvas();
    static constexpr int PANEL_X = Hal::DISPLAY_W - 72;
    static constexpr int PANEL_Y = 2, PANEL_W = 68, PANEL_RADIUS = 5;
    const bool marginHud = RoomRenderer::viewport().y >= PANEL_Y + 22;
    const int clockPanelWidth = marginHud ? 96 : PANEL_W;
    const int clockPanelX = marginHud ? Hal::DISPLAY_W - 28 - clockPanelWidth : PANEL_X;
    const int panelHeight = marginHud ? 22 : (frame.hudCount > 1 ? 57 : 38);
    HomeUiContent::fillRoundRectAlpha(clockPanelX, PANEL_Y, clockPanelWidth, panelHeight,
        PANEL_RADIUS, PixelRenderer::rgb(8, 10, 14), 150);
    canvas.drawRoundRect(clockPanelX, PANEL_Y, clockPanelWidth, panelHeight, PANEL_RADIUS,
        PixelRenderer::rgb(72, 83, 98));
    char clock[8];
    std::snprintf(clock, sizeof(clock), "%02u:%02u", frame.gameMinutes / 60, frame.gameMinutes % 60);
    PixelRenderer::text(clockPanelX + (marginHud ? 8 : 20), PANEL_Y + 3, clock,
        PixelRenderer::rgb(245, 246, 232));
    if (marginHud)
        HudRenderer::drawBattery(canvas, clockPanelX + 54, PANEL_Y + 3, Hal::ins().batteryLevel());
    for (uint8_t index = 0; index < frame.hudCount; ++index) {
        const int statusX = marginHud ? 14 + index * 76 : PANEL_X;
        if (marginHud) {
            HomeUiContent::fillRoundRectAlpha(statusX, PANEL_Y, PANEL_W, 22,
                PANEL_RADIUS, PixelRenderer::rgb(8, 10, 14), 150);
            canvas.drawRoundRect(statusX, PANEL_Y, PANEL_W, 22, PANEL_RADIUS,
                PixelRenderer::rgb(72, 83, 98));
        }
        const int rowY = marginHud ? PANEL_Y + 2 : PANEL_Y + 18 + index * 19;
        HomeHudRow row = frame.hud[index];
        row.hunger = displayedHunger(row.slot, row.hunger, frame.nowMs);
        HomeUiContent::drawHudRow(row, statusX, rowY);
    }
}

void HomeUi::drawToast() {
    if (!toast) return;
    auto& canvas = PixelRenderer::canvas();
    const auto view = RoomRenderer::viewport();
    const bool bottomMargin = Hal::DISPLAY_H - view.y - view.height >= 31;
    const int x = bottomMargin ? (Hal::DISPLAY_W - 96) / 2 : 68;
    const int y = bottomMargin ? Hal::DISPLAY_H - 31 : 6;
    canvas.fillRect(x, y, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(x + 8, y + 2, toast, PixelRenderer::rgb(255, 255, 255));
}

void HomeUi::drawHomeMenu(const HomeFrame& frame) {
    (void)frame;
}

}  // namespace Ui
