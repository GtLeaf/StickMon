// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/MainMenuUi.h"

#include <algorithm>
#include "assets/MenuAssets.h"
#include "core/UiStrings.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/RoomControls.h"
#include "ui/round_touch/RoundPage.h"

namespace Ui {
namespace {
constexpr int CARD_H = 72;
constexpr int NORMAL_TOP = 128, NORMAL_BOTTOM = 272;

bool panelPoint(int x, int y) {
    const int dx = 2 * x - 359, dy = 2 * y - 359;
    return x >= 0 && x < 360 && y >= 0 && y < 360 && dx * dx + dy * dy < 360 * 360;
}

void drawCard(MenuGrid::Rect rect, int index, bool pressed, bool selected) {
    auto& c = PixelRenderer::canvas();
    MenuGrid::card(c, rect, pressed, selected);
    const int cx = rect.x + rect.width / 2;
    const auto entry = AppSceneFlow::mainMenuEntry(index, STICKMON_ENABLE_DEBUG_FEATURES != 0);
    if (entry.iconIndex < MenuAssets::MAIN_ICON_COUNT) {
        const auto& frame = MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex];
        PixelRenderer::drawRgb565RleScaled(cx - MenuAssets::FRAME_W / 2, rect.y + 4,
            MenuAssets::FRAME_W, MenuAssets::FRAME_H, MenuAssets::MAIN_ICON_RLE,
            FlashStorage::readWord(&frame.offset), FlashStorage::readWord(&frame.length), 1.0f);
    }
    MenuGrid::label(c, cx, rect.y + 50, entry.shortLabel, rect.width - 8, 0xffff);
}

}  // namespace

MenuGrid::Layout MainMenuUi::layout() {
    return MenuGrid::fit({60, 52, 240, 308}, 98, CARD_H, 8, 6, 8, 2);
}

MenuGrid::Rect MainMenuUi::itemBounds(int index, float offset) {
    return layout().cell(index, offset);
}

float MainMenuUi::maximumScroll() {
    const auto grid = layout();
    const auto last = grid.cell(ROUND_ITEM_COUNT - 1);
    // The final row must reach the broad central part of the round panel.
    return std::max(0.0f, float(last.y + last.height / 2 - NORMAL_BOTTOM));
}

void MainMenuUi::revealCursor() {
    const auto rect = layout().cell(cursor, scroll);
    const int center = rect.y + rect.height / 2;
    if (center < NORMAL_TOP) scroll -= NORMAL_TOP - center;
    if (center > NORMAL_BOTTOM) scroll += center - NORMAL_BOTTOM;
    scroll = std::clamp(scroll, 0.0f, maximumScroll());
}

void MainMenuUi::enter(bool fromHome) {
    resetState(fromHome);
    if (cursor < 0 || cursor >= ROUND_ITEM_COUNT) cursor = 0;
    resetNavigation();
}

void MainMenuUi::resetNavigation() {
    scroll = 0;
    revealCursor();
    cancelTouch();
}

void MainMenuUi::render() {
    RoundPage::background();
    auto& c = PixelRenderer::canvas();
    const auto grid = layout();
    c.setClipRect(grid.viewport.x, grid.viewport.y, grid.viewport.width, grid.viewport.height);
    for (int i = 0; i < ROUND_ITEM_COUNT; ++i) {
        const auto rect = itemBounds(i, scroll);
        if (rect.y + rect.height <= grid.viewport.y || rect.y >= grid.viewport.y + grid.viewport.height) continue;
        drawCard(rect, i, touch.pressed() == i, cursor == i);
    }
    c.clearClipRect();
    RoundPage::nativeHeader(Ui::MENU, touch.pressed() == RoundPage::BACK_TARGET);
    if (!RoundRoom::lastSaveOk) {
        c.fillRoundRect(68, 284, 224, 26, 8, PixelRenderer::rgb(34, 45, 52));
        MenuGrid::label(c, 180, 289, RoundRoom::SAVE_FAILED, 208, 0xffff);
    }
}

bool MainMenuUi::onButton(const ButtonEvent& event) {
    if (event.action == BtnAction::LONG_PRESS && (event.btn == 0 || event.btn == 1)) {
        host.leaveMainMenu();
        return true;
    }
    if (event.action != BtnAction::PRESSED) return false;
    if (event.btn == 1) {
        cursor = (cursor + 1) % ROUND_ITEM_COUNT;
        revealCursor();
        return true;
    }
    return event.btn == 0 && onTap(cursor);
}

void MainMenuUi::cancelTouch() { Scene::cancelTouch(); touch.cancel(); }

bool MainMenuUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto grid = layout();
    const auto result = touch.update(event, touchTarget(event.x, event.y), scroll,
        maximumScroll(), grid.viewport.contains(event.x, event.y));
    if (result.tap >= 0) return onTap(result.tap) || result.changed;
    return result.changed;
}

int MainMenuUi::touchTarget(int16_t x, int16_t y) {
    if (!panelPoint(x, y)) return -1;
    if (backBounds().contains(x, y)) return RoundPage::BACK_TARGET;
    if (!layout().viewport.contains(x, y)) return -1;
    for (int i = 0; i < ROUND_ITEM_COUNT; ++i)
        if (itemBounds(i, scroll).contains(x, y)) return i;
    return -1;
}

bool MainMenuUi::onTap(int target) {
    if (target == RoundPage::BACK_TARGET || target == ROUND_ITEM_COUNT - 1) {
        host.leaveMainMenu();
        return true;
    }
    if (target < 0 || target >= ROUND_ITEM_COUNT) return false;
    cursor = target;
    host.pageNavigationTutorial();
    return host.activateMainMenu(static_cast<uint8_t>(target));
}
}  // namespace Ui
