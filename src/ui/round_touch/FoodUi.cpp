// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/FoodUi.h"

#include <algorithm>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/RoundPage.h"

namespace Ui {
void FoodUi::reset() { foodScroll = 0; enter(); }

void FoodUi::enter() {
    cancelTouch();
    foodCursor = Features::Room::selectedFoodIndex(host.pageState());
    foodScroll = RoundPage::listLayout().reveal(foodCursor, 0, Game::ROOM_FOOD_COUNT);
}

SceneUpdateResult FoodUi::update(uint32_t, float) {
    foodScroll = std::clamp(foodScroll, 0.0f, RoundPage::listLayout().maxScroll(Game::ROOM_FOOD_COUNT));
    return SceneUpdateResult::parked();
}

void FoodUi::render() {
    RoundPage::background();
    const auto layout = RoundPage::listLayout();
    const auto& state = host.pageState();
    auto& c = PixelRenderer::canvas();
    c.setClipRect(layout.viewport.x, layout.viewport.y, layout.viewport.width, layout.viewport.height);
    for (int i = 0; i < Game::ROOM_FOOD_COUNT; ++i) {
        const auto rect = layout.cell(i, foodScroll);
        if (rect.y + rect.height <= layout.viewport.y || rect.y >= layout.viewport.y + layout.viewport.height) continue;
        MenuGrid::card(c, rect, touch.pressed() == i, foodCursor == i);
        const auto stock = Features::Room::foodStock(state, i);
        const bool selected = i == Features::Room::selectedFoodIndex(state);
        if (selected) c.fillRect(rect.x + 3, rect.y + 8, 3, rect.height - 16, 0xffe0);
        GameAssets::drawCentered(GameAssets::itemKind(Game::itemIdForFoodIndex(i)), rect.x + 27, rect.y + 25, 0.9f);
        MenuGrid::label(c, rect.x + 119, rect.y + 17, Ui::Room::FOOD_NAMES[i], 126,
            selected ? 0xffe0 : stock ? 0xffff : 0x7bef);
        char value[12];
        std::snprintf(value, sizeof(value), "x%u", stock);
        MenuGrid::label(c, rect.x + 210, rect.y + 17, value, 48, stock ? 0x77b6 : 0x7bef);
    }
    c.clearClipRect();
    RoundPage::header(Ui::Room::FOOD_ITEM, touch.pressed() == RoundPage::BACK_TARGET);
    // The scroll thumb shares the viewport and remains inside the round safe area.
    const float maximum = layout.maxScroll(Game::ROOM_FOOD_COUNT);
    if (maximum > 0) {
        const int top = layout.viewport.y + 8, height = layout.viewport.height - 16;
        c.fillRoundRect(236, top, 3, height, 1, PixelRenderer::rgb(36, 51, 58));
        c.fillRoundRect(236, top + int((height - 40) * foodScroll / maximum), 3, 40, 1, 0x77b6);
    }
}

bool FoodUi::onButton(const ButtonEvent& event) {
    if (event.action == BtnAction::LONG_PRESS && event.btn == 0) { host.pageHome(); return true; }
    if (event.action == BtnAction::LONG_PRESS && event.btn == 1) { host.pageBack(); return true; }
    if (event.action != BtnAction::PRESSED) return false;
    if (event.btn == 1) {
        foodCursor = (foodCursor + 1) % Game::ROOM_FOOD_COUNT;
        foodScroll = RoundPage::listLayout().reveal(foodCursor, foodScroll, Game::ROOM_FOOD_COUNT);
        return true;
    }
    return event.btn == 0 && onTap(foodCursor);
}

void FoodUi::cancelTouch() { Scene::cancelTouch(); touch.cancel(); }

bool FoodUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto layout = RoundPage::listLayout();
    const auto result = touch.update(event, touchTarget(event.x, event.y), foodScroll,
        layout.maxScroll(Game::ROOM_FOOD_COUNT), layout.viewport.contains(event.x, event.y));
    if (result.tap >= 0) return onTap(result.tap) || result.changed;
    return result.changed;
}

int FoodUi::touchTarget(int16_t x, int16_t y) {
    if (!RoundPage::safePoint(x, y)) return -1;
    if (RoundPage::backHit(x, y)) return RoundPage::BACK_TARGET;
    return RoundPage::listLayout().itemAt(x, y, foodScroll, Game::ROOM_FOOD_COUNT);
}

bool FoodUi::onTap(int target) {
    if (target == RoundPage::BACK_TARGET) { host.pageBack(); return true; }
    if (target < 0 || target >= Game::ROOM_FOOD_COUNT) return false;
    foodCursor = target;
    activateFood(foodCursor);
    return true;
}
}  // namespace Ui
