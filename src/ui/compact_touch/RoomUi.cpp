#include "ui/compact_touch/RoomUi.h"

#include "ui/PageProfile.h"

#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {
namespace {
MenuGrid::Layout roomLayout() {
    return MenuGrid::fit(TouchUi::pageBody(), Ui::Page::WIDTH - 16, 80, 0, 6, 8, 4);
}

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

void RoomUi::reset() {
    roomCursor = 0;
    roomScroll = 0;
    cancelTouch();
}

void RoomUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    const auto layout = roomLayout();
    c.setClipRect(0, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH,
        Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H);
    for (int i = 0; i < 2; ++i) {
        const auto rect = layout.cell(i, roomScroll);
        if (rect.y + rect.height <= TouchUi::SAFE_HEADER_H || rect.y >= Ui::Page::HEIGHT) continue;
        MenuGrid::card(c, rect, gridTouch.pressed() == i, roomCursor == i);
        GameAssets::drawCentered(i == 0 ? GameAssets::Kind::ITEM_NORMAL_FOOD : GameAssets::Kind::SHOWER_MENU_SPRINKLER,
            rect.x + 38, rect.y + 40, i == 0 ? 1.5f : 1.2f);
        PixelRenderer::text(rect.x + 80, rect.y + 32, Ui::Room::ITEMS[i], 0xffff);
        if (i == 0) {
            char value[12];
            std::snprintf(value, sizeof(value), "x%u", unsigned(Features::Room::totalFoodStock(host.pageState())));
            PixelRenderer::text(Ui::Page::WIDTH - 18 - textPixelWidth(value), rect.y + 13, value, 0xffe0);
        }
    }
    c.clearClipRect();
    TouchUi::safePageHeader(Ui::ROOM, gridTouch.pressed() == 230, roomCursor == 2);
}

bool RoomUi::onButton(const ButtonEvent& event) {
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        host.pageBackTutorial();
        host.pageBack();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        roomCursor = (roomCursor + 1) % 3;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateRoom(roomCursor);
        return true;
    }
    return false;
}

void RoomUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
}

bool RoomUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto layout = roomLayout();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), roomScroll,
        layout.maxScroll(2), layout.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int RoomUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const auto header = TouchUi::safeHeaderBounds();
    if (y < header.y + header.height && !header.contains(x, y)) return -1;
    if (TouchUi::safeBack().contains(x, y)) return 230;
    return roomLayout().itemAt(x, y, roomScroll, 2);
}

bool RoomUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (target < 0 || target >= 128) return false;
    roomCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

}  // namespace Ui
