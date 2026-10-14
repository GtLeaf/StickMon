#include "ui/HatchScreens.h"

#include <algorithm>

#include "HomeScreen.h"
#include "assets/GameAssets.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "presentation/Canvas565.h"
#include "ui/UiCommon.h"
#include "ui/models/ScreenModels.h"

namespace AmoledV1 {
namespace {

constexpr int HEADER_HEIGHT = HOME_HEADER_HEIGHT;
constexpr int ROOM_BOTTOM = HOME_STATUS_TOP;

}  // namespace

bool hatchMenuAt(int x, int y) {
    return homeHitTargetAt(x, y, -1000, -1000, -1000, -1000) == HomeHitTarget::MENU;
}

void renderHatchScreen(Canvas565& canvas, const HatchViewModel& model,
                       uint16_t rowBegin, uint16_t rowEnd, AmoledUi::Rect* eggHitBounds) {
    UiCommon::PageClip clip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    clip.setRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT);
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT, UiMetrics::PAGE_BACKGROUND);

    clip.setRect(0, HEADER_HEIGHT, AmoledUi::WIDTH, ROOM_BOTTOM - HEADER_HEIGHT);
    RoomResource& room = RoomResource::ins();
    const int cameraX = room.available() ? std::clamp<int>(
        room.bedX() - AmoledUi::WIDTH / 4, 0,
        std::max<int>(0, room.width() - AmoledUi::WIDTH / 2)) : 0;
    const int cameraY = room.available() ? std::clamp<int>(
        room.roomY() + room.bedY() - 26 - (200 - HEADER_HEIGHT) / 2, room.roomY(),
        std::max<int>(room.roomY(), room.roomY() + room.height() -
                     (ROOM_BOTTOM - HEADER_HEIGHT) / 2)) : 0;
    const bool roomDrawn = RoomRenderer::drawViewport(
            0, HEADER_HEIGHT, AmoledUi::WIDTH, ROOM_BOTTOM - HEADER_HEIGHT,
            cameraX, cameraY, model.night, AmoledUi::RESOURCE_SCALE);
    if (!roomDrawn) {
        canvas.fillRect(0, HEADER_HEIGHT, AmoledUi::WIDTH,
                        ROOM_BOTTOM - HEADER_HEIGHT, UiCommon::rgb(31, 43, 49));
        canvas.fillEllipse(184, 266, 90, 24, UiCommon::rgb(45, 85, 82));
    }
    const int eggX = (roomDrawn
        ? (room.bedX() - cameraX) * AmoledUi::RESOURCE_SCALE : 184) +
        model.shakeOffset * AmoledUi::RESOURCE_SCALE;
    const int eggY = roomDrawn
        ? HEADER_HEIGHT + (room.roomY() + room.bedY() - 26 - cameraY) *
                          AmoledUi::RESOURCE_SCALE : 200;
    if (!GameAssets::drawCentered(GameAssets::Kind::EGG, eggX, eggY, 2.0f)) {
        canvas.fillEllipse(eggX, eggY, 32, 40, UiCommon::rgb(235, 230, 201));
        canvas.fillEllipse(eggX - 12, eggY - 18, 10, 12, UiCommon::rgb(99, 151, 116));
        canvas.fillEllipse(eggX + 14, eggY + 12, 12, 14, UiCommon::rgb(99, 151, 116));
    }
    if (eggHitBounds) {
        // Include touch padding and the full extent of the short shake.
        *eggHitBounds = {eggX - 54, eggY - 64, 108, 128};
    }
    if (model.showHeart) drawHatchHeart(canvas, eggX, eggY - 58);

    // Use the same header, buttons and status card as the normal room.
    clip.reset();
    HomeViewModel home;
    home.hatching = true;
    home.hatchRemainingSeconds = model.remainingSeconds;
    home.gameMinutesOfDay = model.gameMinutesOfDay;
    home.petVisible = false;
    home.monsterCount = 0;
    renderHomeScreen(canvas, home, rowBegin, std::min<uint16_t>(rowEnd, HEADER_HEIGHT));
    renderHomeScreen(canvas, home, std::max<uint16_t>(rowBegin, ROOM_BOTTOM), rowEnd);
    clip.setRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT);
    UiCommon::drawToast(canvas, model.toast);
}

}  // namespace AmoledV1
