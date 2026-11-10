#include "ui/common/HatchUiContent.h"

#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "game/HatchService.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui::HatchUiContent {
namespace {
float hatchCameraX(const HatchLayout& layout) {
    return RoomRenderer::clampCameraX(RoomResource::ins().bedX() -
        layout.width / (2.0f * RoomRenderer::PIXEL_SCALE));
}

float hatchCameraY(const HatchLayout& layout) {
    RoomResource& room = RoomResource::ins();
    if (!room.available()) return 0.0f;
    const float focus = RoomRenderer::PIXEL_SCALE == 1
        ? 84.0f : layout.height / (2.0f * RoomRenderer::PIXEL_SCALE);
    return RoomRenderer::clampCameraY(room.roomY() + room.bedY() -
        (RoomRenderer::PIXEL_SCALE == 1 ? 16 : 26) - focus);
}

void drawRoom(const HatchFrame& frame, const HatchLayout& layout) {
    RoomRenderer::draw(hatchCameraY(layout), frame.night, hatchCameraX(layout));
}

void drawEgg(const HatchLayout& layout, int8_t eggOffset) {
    auto& c = PixelRenderer::canvas();
    RoomResource& room = RoomResource::ins();
    int x = room.available() ? RoomRenderer::screenX(room.bedX(), hatchCameraX(layout)) : layout.width / 2;
    int y = room.available()
        ? RoomRenderer::screenY(room.roomY() + room.bedY() - 26, hatchCameraY(layout))
        : 84;
    x += eggOffset * RoomRenderer::PIXEL_SCALE;
    if (!GameAssets::drawCentered(GameAssets::Kind::EGG, x, y, RoomRenderer::PIXEL_SCALE)) {
        c.fillRect(x - 12 * RoomRenderer::PIXEL_SCALE, y - 16 * RoomRenderer::PIXEL_SCALE,
                   24 * RoomRenderer::PIXEL_SCALE, 32 * RoomRenderer::PIXEL_SCALE, PixelRenderer::rgb(255, 216, 72));
    }
}

void drawHud(const HatchFrame& frame, const HatchLayout& layout) {
    auto& c = PixelRenderer::canvas();
    const int PANEL_X = layout.width - 72 - (layout.width > 240 ? 10 : 0);
    const int PANEL_Y = layout.height > 135 ? 12 : 2;
    static constexpr int PANEL_W = 68;
    static constexpr int ASCII_ADVANCE = 8;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, 22,
               PixelRenderer::rgb(34, 39, 47));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, 22,
               PixelRenderer::rgb(72, 83, 98));

    uint16_t remaining = frame.remainingSeconds;
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%02u:%02u",
             remaining / 60, remaining % 60);
    int textX = PANEL_X +
        (PANEL_W - static_cast<int>(strlen(countdown)) * ASCII_ADVANCE) / 2;
    PixelRenderer::text(textX, PANEL_Y + 3, countdown,
                        PixelRenderer::rgb(245, 246, 232));
}

void drawToast(const HatchLayout& layout, const char* toast) {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect((layout.width - 96) / 2, 6, 96, 20, PixelRenderer::rgb(41, 45, 55));
    PixelRenderer::text((layout.width - 44) / 2, 8, toast, PixelRenderer::rgb(255, 255, 255));
}

}  // namespace

Point eggCenter(const HatchLayout& layout) {
    auto& room = RoomResource::ins();
    return {
        room.available() ? RoomRenderer::screenX(room.bedX(), hatchCameraX(layout)) : layout.width / 2,
        room.available() ? RoomRenderer::screenY(room.roomY() + room.bedY() - 26, hatchCameraY(layout)) : 84,
    };
}

void drawWorld(const HatchFrame& frame, const HatchLayout& layout, int8_t eggOffset) {
    drawRoom(frame, layout);
    drawEgg(layout, eggOffset);
}

void draw(const HatchFrame& frame, const HatchLayout& layout, const char* toast) {
    drawWorld(frame, layout, Game::HatchService::shakeOffset(frame.progress, frame.nowMs));
    drawHud(frame, layout);
    drawToast(layout, toast);
    TouchUi::drawHomeMenu();
}
}  // namespace Ui::HatchUiContent
