#include "ui/stick/FoodUi.h"
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

void FoodUi::cancelTouch() { Scene::cancelTouch(); }
bool FoodUi::onTouch(const Platform::IInputDevice::TouchEvent& event) { return Scene::onTouch(event); }

bool FoodUi::onButton(const ButtonEvent& event) {
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
        foodCursor = (foodCursor + 1) % FOOD_ITEM_COUNT;
        if (foodCursor == 0) foodScroll = 0.0f;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        activateFood(foodCursor);
        return true;
    }
    return false;
}

void FoodUi::reset() {
    cancelTouch();
    foodCursor = selectedFoodIndex(host.pageState());
    foodScroll = 0.0f;
}

void FoodUi::resetDescription() {
    descScrollKey = -1;
    descScroll = 0.0f;
}

void FoodUi::enter() {
    cancelTouch();
    foodCursor = selectedFoodIndex(host.pageState());
    foodScroll = 0.0f;
}

int FoodUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= 78 || y < 6 || y >= TouchUi::CONTENT_H) return -1;
    const int index = (y - 6 + int(foodScroll)) / 22;
    return index >= 0 && index < FOOD_ITEM_COUNT ? index : -1;
}

bool FoodUi::onTap(int target) {
    if (target < 0 || target >= FOOD_ITEM_COUNT) return false;
    foodCursor = target;
    host.pageNavigationTutorial();
    return onButton({0, BtnAction::PRESSED});
}

namespace {
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
}

SceneUpdateResult FoodUi::update(uint32_t, float) {
    RenderDemand demand;
    constexpr int VISIBLE_H = TouchUi::CONTENT_H - 12;
    constexpr int ROW_H = 22;
    const int maximum = MathUtil::max(0, FOOD_ITEM_COUNT * ROW_H - VISIBLE_H);
    const int target = foodCursor * ROW_H - VISIBLE_H / 2 + ROW_H / 2;
    const auto step = UiMotion::lerp(foodScroll, float(MathUtil::clamp(target, 0, maximum)), 0.25f, 0.5f);
    demand.changed(step.changed);
    demand.animate(step.active);
    if (!foodBack(foodCursor)) {
        int width = 0;
        for (const auto* line : Ui::Room::FOOD_DESCS[foodCursor])
            width = MathUtil::max(width, textPixelWidth(line));
        const int maxScroll = width > Hal::DISPLAY_W - 90 ? width - (Hal::DISPLAY_W - 90) + 4 : 0;
        demand.changed(updateDescriptionScroll(0x300 + foodCursor, maxScroll));
        demand.animate(maxScroll > 0);
    }
    return demand.result();
}

bool FoodUi::updateDescriptionScroll(int scrollKey, int maxScroll) {
    float before = descScroll;
    int previousKey = descScrollKey;
    if (scrollKey != descScrollKey) {
        descScrollKey = scrollKey;
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
    }

    if (maxScroll <= 0) {
        descScroll = 0.0f;
        descScrollLastMs = Hal::ins().millis();
        return previousKey != descScrollKey || before != descScroll;
    }

    uint32_t now = Hal::ins().millis();
    uint32_t dt = now - descScrollLastMs;
    if (dt > 120) dt = 120;
    descScrollLastMs = now;
    descScroll += 28.0f * ((float)dt / 1000.0f);
    if (descScroll > (float)(maxScroll + 18)) descScroll = 0.0f;
    return previousKey != descScrollKey || before != descScroll;
}

void FoodUi::renderScrollableDescription(const char* const* lines, int lineCount,
                                            int x, int y, int w, uint16_t color,
                                            int scrollKey) {
    if (lineCount <= 0 || w <= 0) return;
    (void)scrollKey;
    auto& c = PixelRenderer::canvas();
    c.setClipRect(x, y - 1, w, lineCount * 16 + 2);
    for (int i = 0; i < lineCount; ++i) {
        PixelRenderer::text(x - (int)descScroll, y + i * 16, lines[i], color, 1);
    }
    c.clearClipRect();
}

void FoodUi::drawSelectionDiamond(int cx, int cy, uint16_t color) {
    auto& c = PixelRenderer::canvas();
    c.fillTriangle(cx, cy - 3, cx + 3, cy, cx, cy + 3, color);
    c.fillTriangle(cx, cy - 3, cx, cy + 3, cx - 3, cy, color);
}

void FoodUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, 0x0000);

    static constexpr int LEFT_W = 78;
    static constexpr int LIST_Y_START = 6;
    static constexpr int LIST_BOTTOM = TouchUi::CONTENT_H - 6;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 18;

    if (foodCursor >= FOOD_ITEM_COUNT) foodCursor = 0;

    c.drawFastVLine(LEFT_W, 6, TouchUi::CONTENT_H - 12, 0x7BEF);

    uint8_t selectedFood = selectedFoodIndex(host.pageState());
    c.setClipRect(0, LIST_Y_START, LEFT_W - 1, VISIBLE_H);
    for (uint8_t i = 0; i < FOOD_ITEM_COUNT; ++i) {
        int y = LIST_Y_START + i * ROW_H - (int)foodScroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;
        bool selected = i == foodCursor;
        bool back = foodBack(i);
        uint8_t stock = back ? 1 : foodStock(host.pageState(), i);
        uint16_t color = selected ? 0xFFE0 : (stock > 0 ? 0xFFFF : 0x7BEF);
        int textY = y + (ROW_H - 16) / 2;

        if (!back && i == selectedFood && stock > 0) {
            drawSelectionDiamond(TEXT_X - 8, textY + 8, PixelRenderer::rgb(255, 216, 72));
        }
        PixelRenderer::text(TEXT_X, textY,
                            back ? Ui::BACK : Ui::Room::FOOD_NAMES[i],
                            color, 1);
    }
    c.clearClipRect();

    if (foodBack(foodCursor)) {
        PixelRenderer::text(LEFT_W + 28, 80, Ui::Room::DESCS[ROOM_ITEM_COUNT - 1], 0x7BEF, 1);
        return;
    }

    uint8_t stock = foodStock(host.pageState(), foodCursor);
    int iconX = LEFT_W + 13;
    int iconY = 9;
    GameAssets::drawCentered(
        GameAssets::itemKind(Game::itemIdForFoodIndex(foodCursor)),
        iconX + 29, iconY + 18);

    int textX = iconX + 68;
    PixelRenderer::text(textX, iconY + 2, Ui::Room::FOOD_NAMES[foodCursor], 0xFFFF, 1);
    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), Ui::Bag::COUNT_X_FMT, stock);
    PixelRenderer::text(textX, iconY + 20, countBuf,
                        stock > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                        1);

    const char* descLines[3] = {
        Ui::Room::FOOD_DESCS[foodCursor][0],
        Ui::Room::FOOD_DESCS[foodCursor][1],
        Ui::Room::FOOD_DESCS[foodCursor][2],
    };
    renderScrollableDescription(descLines, 3, LEFT_W + 10, 58,
                                Hal::DISPLAY_W - LEFT_W - 12,
                                stock > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                                0x300 + foodCursor);
}

}  // namespace Ui
