#include "ui/compact_touch/FoodUi.h"

#include "ui/PageProfile.h"

#include <algorithm>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/MenuGrid.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

namespace Ui {
namespace {
MenuGrid::Layout foodLayout() {
    return MenuGrid::fit({0, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH,
        Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H},
        Ui::Page::WIDTH - 16, 44, 0, 6, 8, 4);
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

void FoodUi::reset() {
    foodCursor = Features::Room::selectedFoodIndex(host.pageState());
    foodScroll = 0;
    cancelTouch();
}

void FoodUi::enter() {
    foodCursor = Features::Room::selectedFoodIndex(host.pageState());
    foodScroll = foodLayout().reveal(foodCursor, 0, Game::ROOM_FOOD_COUNT);
}

SceneUpdateResult FoodUi::update(uint32_t, float) {
    foodScroll = std::clamp(foodScroll, 0.0f, foodLayout().maxScroll(Game::ROOM_FOOD_COUNT));
    return SceneUpdateResult::parked();
}

void FoodUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    const auto layout = foodLayout();
    const auto& state = host.pageState();
    c.setClipRect(0, TouchUi::SAFE_HEADER_H, Ui::Page::WIDTH,
        Ui::Page::HEIGHT - TouchUi::SAFE_HEADER_H);
    for (int i = 0; i < Game::ROOM_FOOD_COUNT; ++i) {
        const auto rect = layout.cell(i, foodScroll);
        if (rect.y + rect.height <= TouchUi::SAFE_HEADER_H || rect.y >= Ui::Page::HEIGHT) continue;
        MenuGrid::card(c, rect, gridTouch.pressed() == i, foodCursor == i);
        const auto stock = Features::Room::foodStock(state, i);
        const bool active = i == Features::Room::selectedFoodIndex(state);
        if (active) c.fillRect(rect.x + 3, rect.y + 8, 3, rect.height - 16, 0xffe0);
        GameAssets::drawCentered(GameAssets::itemKind(Game::itemIdForFoodIndex(i)),
            rect.x + 26, rect.y + 22, 0.8f);
        PixelRenderer::text(rect.x + 52, rect.y + 14, Ui::Room::FOOD_NAMES[i],
            active ? 0xffe0 : stock ? 0xffff : 0x7bef);
        char value[12];
        std::snprintf(value, sizeof(value), "x%u", stock);
        PixelRenderer::text(Ui::Page::WIDTH - 18 - textPixelWidth(value), rect.y + 14,
            value, stock ? 0x77b6 : 0x7bef);
    }
    c.clearClipRect();
    TouchUi::safePageHeader(Ui::FOOD, gridTouch.pressed() == 230);
}

bool FoodUi::onButton(const ButtonEvent& event) {
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
        foodCursor = (foodCursor + 1) % Game::ROOM_FOOD_COUNT;
        if (foodCursor == 0) foodScroll = 0;
        foodScroll = foodLayout().reveal(foodCursor, foodScroll, Game::ROOM_FOOD_COUNT);
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateFood(foodCursor);
        return true;
    }
    return false;
}

void FoodUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
}

bool FoodUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto layout = foodLayout();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), foodScroll,
        layout.maxScroll(Game::ROOM_FOOD_COUNT), layout.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int FoodUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const auto header = TouchUi::safeHeaderBounds();
    if (y < header.y + header.height && !header.contains(x, y)) return -1;
    if (TouchUi::safeBack().contains(x, y)) return 230;
    return foodLayout().itemAt(x, y, foodScroll, Game::ROOM_FOOD_COUNT);
}

bool FoodUi::onTap(int target) {
    if (target == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (target < 0 || target >= 128) return false;
    foodCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

}  // namespace Ui
