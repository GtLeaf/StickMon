#include "ui/compact_touch/HomeUi.h"

#include <cstdio>
#include "hardware/Hal.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"
#include "presentation/MenuGrid.h"
#include "ui/compact_touch/HudControls.h"

namespace Ui {

void HomeUi::drawContact(const HomeFrame& frame) {
    HomeUiContent::drawContact(frame, 28, Hal::DISPLAY_H - 8, Hal::DISPLAY_W - 56);
}

void HomeUi::focus(float x, float y) {
    focusCamera(x, y, RoomRenderer::PIXEL_SCALE == 1
        ? 84.0f : Hal::DISPLAY_H / (2.0f * RoomRenderer::PIXEL_SCALE));
}

int HomeUi::touchTarget(int16_t x, int16_t y) {
    return HomeUiContent::touchTarget(x, y, host.homeFrame(), camera,
        HudControls::menuBounds().contains(x, y));
}

void HomeUi::drawHud(const HomeFrame& frame) {
    auto& c = PixelRenderer::canvas();
    static constexpr int PANEL_X = 28;
    static constexpr int PANEL_Y = 12;
    static constexpr int PANEL_W = 68;
    static constexpr int PANEL_RADIUS = 5;

    const int clockPanelW = 78;
    const int clockPanelX = Hal::DISPLAY_W - 28 - clockPanelW;
    HomeUiContent::fillRoundRectAlpha(clockPanelX, PANEL_Y, clockPanelW, 22,
        PANEL_RADIUS, PixelRenderer::rgb(8, 10, 14), 150);
    c.drawRoundRect(clockPanelX, PANEL_Y, clockPanelW, 22, PANEL_RADIUS,
        PixelRenderer::rgb(72, 83, 98));

    char clock[8];
    std::snprintf(clock, sizeof(clock), "%02u:%02u", frame.gameMinutes / 60,
        frame.gameMinutes % 60);
    PixelRenderer::text(clockPanelX + 8, PANEL_Y + 3, clock,
        PixelRenderer::rgb(245, 246, 232));
    HudRenderer::drawBattery(c, clockPanelX + 54, PANEL_Y + 7,
        Hal::ins().batteryLevel(), 1);

    if (frame.hudCount) {
        const int panelH = frame.hudCount > 1 ? 42 : 22;
        HomeUiContent::fillRoundRectAlpha(PANEL_X, PANEL_Y, PANEL_W, panelH,
            PANEL_RADIUS, PixelRenderer::rgb(8, 10, 14), 150);
        c.drawRoundRect(PANEL_X, PANEL_Y, PANEL_W, panelH, PANEL_RADIUS,
            PixelRenderer::rgb(72, 83, 98));
    }

    for (uint8_t visibleIndex = 0; visibleIndex < frame.hudCount; ++visibleIndex) {
        const int rowY = PANEL_Y + 2 + visibleIndex * 20;
        HomeHudRow row = frame.hud[visibleIndex];
        row.hunger = displayedHunger(row.slot, row.hunger, frame.nowMs);
        HomeUiContent::drawHudRow(row, PANEL_X, rowY);
    }
}

void HomeUi::drawToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    const auto view = RoomRenderer::viewport();
    const bool bottomMargin = Hal::DISPLAY_H - view.y - view.height >= 31;
    const int x = bottomMargin ? (Hal::DISPLAY_W - 96) / 2 : 68;
    const int y = bottomMargin ? Hal::DISPLAY_H - 31 : 6;
    c.fillRect(x, y, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text(x + 8, y + 2, toast, PixelRenderer::rgb(255, 255, 255));
}

void HomeUi::drawHomeMenu(const HomeFrame& frame) {
    if (!frame.showMenu || frame.contactVisible || !Platform::input().touchAvailable()) return;
    HudControls::drawMenu();
}

}  // namespace Ui
