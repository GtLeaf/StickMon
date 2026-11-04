#include "ui/stick/RoomUi.h"
#include <cstdio>
#include "core/MathUtil.h"
#include "core/UiMotion.h"
#include "core/UiStrings.h"
#include "assets/GameAssets.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {
using namespace Features::Room;

void RoomUi::cancelTouch() { Scene::cancelTouch(); }
bool RoomUi::onTouch(const Platform::IInputDevice::TouchEvent& event) { return Scene::onTouch(event); }

bool RoomUi::onButton(const ButtonEvent& event) {
    if (!host.pageExploreContext() && event.btn == 1 && event.action == BtnAction::LONG_PRESS)
        host.pageBackTutorial();
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        host.pageBack();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        roomCursor = (roomCursor + 1) % ROOM_ITEM_COUNT;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateRoom(roomCursor);
        return true;
    }
    return false;
}

void RoomUi::reset() { cancelTouch(); roomCursor = 0; }
SceneUpdateResult RoomUi::update(uint32_t, float) { return SceneUpdateResult::parked(); }

int RoomUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Hal::DISPLAY_W || y < 5 || y >= TouchUi::CONTENT_H) return -1;
    const int index = (y - 5) / 21;
    return index < ROOM_ITEM_COUNT ? index : -1;
}

bool RoomUi::onTap(int target) {
    if (target < 0 || target >= ROOM_ITEM_COUNT) return false;
    roomCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

void RoomUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, 0x0000);

    static constexpr int ROW_X = 8;
    static constexpr int TEXT_X = 22;
    static constexpr int VALUE_X = 158;
    static constexpr int ROW_Y = 5;
    static constexpr int ROW_H = 21;
    static constexpr int SEP_W = Hal::DISPLAY_W - ROW_X * 2;

    for (uint8_t i = 0; i < ROOM_ITEM_COUNT; ++i) {
        int y = ROW_Y + i * ROW_H;
        bool selected = i == roomCursor;
        uint16_t color = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) {
            c.fillRect(ROW_X, y + 1, 4, 16, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, y, Ui::Room::ITEMS[i], color, 1);
        if (i == 0) {
            char countBuf[12];
            snprintf(countBuf, sizeof(countBuf), Ui::Bag::COUNT_X_FMT, displayedFoodStock(host.pageState()));
            PixelRenderer::text(VALUE_X, y, countBuf,
                                selected ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                1);
        }
        if (i + 1 < ROOM_ITEM_COUNT) {
            c.drawFastHLine(ROW_X, y + ROW_H - 4, SEP_W, PixelRenderer::rgb(55, 63, 76));
        }
    }

}

}  // namespace Ui
