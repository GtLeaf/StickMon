#include "ui/RoomScreens.h"

#include <algorithm>
#include <cstdio>

#include "core/FontResource.h"
#include "core/UiStrings.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiCommon.h"
#include "ui/UiMetrics.h"
#include "ui/models/ScreenModels.h"
#include "assets/GameAssets.h"
#include "game/ItemInventory.h"

namespace AmoledV1 {

using UiCommon::rgb;
using UiCommon::text;
using UiCommon::drawToast;
using UiCommon::textWidth;

namespace {

constexpr int MENU_CONTENT_TOP = UiMetrics::CONTENT_TOP;
constexpr int FOOD_ROW_HEIGHT = 53;

}  // namespace

int roomMenuItemAt(int x, int y) {
    constexpr int rowHeight = 120;
    if (x < 12 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
    int index = (y - MENU_CONTENT_TOP) / rowHeight;
    return index < 3 ? index : -1;
}

void renderRoomMenuScreen(Canvas565& canvas, const RoomMenuViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* LABELS[] = {
        Ui::Room::FOOD_ITEM, Ui::Amoled::WASH_PET, Ui::BACK,
    };
    static constexpr const char* DETAILS[] = {
        Ui::Amoled::ROOM_SUPPLIES, Ui::Amoled::WASH_PET, Ui::Amoled::RETURN,
    };
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    UiCommon::drawPageHeader(canvas, Ui::ROOM);

    uint16_t foodCount = 0;
    if (model.state) {
        for (uint8_t stock : model.state->room.food) foodCount += stock;
    }
    for (int index = 0; index < 3; ++index) {
        int y = MENU_CONTENT_TOP + index * 120;
        uint16_t background = index == model.pressedItem
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect((12), (y + 6), (344), (108), (8), background);
        if (index == 0) {
            if (!GameAssets::drawCentered(
                    GameAssets::Kind::ITEM_NORMAL_FOOD, 62, y + 56, 1.8f)) {
                text(canvas, 56, y + 48, "?", rgb(248, 210, 105));
            }
        } else if (index == 1) {
            if (!GameAssets::drawCentered(
                    GameAssets::Kind::SHOWER_MENU_SPRINKLER,
                    62, y + 56, 1.44f)) {
                text(canvas, 56, y + 48, "?", rgb(126, 175, 175));
            }
        } else {
            canvas.drawFastHLine((38), (y + 56), (46), rgb(115, 226, 183));
            canvas.drawLine((38), (y + 56), (54), (y + 40), rgb(115, 226, 183));
            canvas.drawLine((38), (y + 56), (54), (y + 72), rgb(115, 226, 183));
        }
        text(canvas, 116, y + 26, LABELS[index],
             index == 2 ? rgb(115, 226, 183) : rgb(226, 238, 233));
        text(canvas, 116, y + 64, DETAILS[index], rgb(126, 145, 145));
        if (index == 0) {
            char stock[10];
            std::snprintf(stock, sizeof(stock), "X%u", foodCount);
            text(canvas, 328 - textWidth(stock),
                 y + 26, stock, rgb(248, 210, 105));
        }
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}

int roomFoodItemAt(int x, int y) {
    if (x < 12 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
    int index = (y - MENU_CONTENT_TOP) / FOOD_ROW_HEIGHT;
    return index < Game::ROOM_FOOD_COUNT ? index : -1;
}

bool roomFoodBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

void renderRoomFoodScreen(Canvas565& canvas, const RoomFoodViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* NAMES[] = {
        Ui::NORMAL_FOOD, Ui::TASTY_FOOD, Ui::SWEET_FOOD, Ui::SPICY_FOOD,
        Ui::SOUR_FOOD, Ui::BITTER_FOOD, Ui::DRY_FOOD,
    };
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    UiCommon::drawPageHeader(canvas, Ui::FOOD);

    for (uint8_t index = 0; index < Game::ROOM_FOOD_COUNT; ++index) {
        int y = MENU_CONTENT_TOP + index * FOOD_ROW_HEIGHT;
        bool selected = index == model.selectedFood;
        bool pressed = index == model.pressedItem;
        uint16_t background = pressed ? rgb(48, 74, 68)
                                      : (selected ? rgb(31, 49, 53)
                                                  : rgb(20, 29, 36));
        canvas.fillRoundRect((12), (y + 4), (344), (48), (6), background);
        if (selected) canvas.fillRect((18), (y + 13), (6), (30), rgb(248, 210, 105));
        Game::ItemId item = Game::itemIdForFoodIndex(index);
        GameAssets::drawCentered(GameAssets::itemKind(item), 50, y + 27, 0.75f);
        text(canvas, 84, y + 17, NAMES[index],
             selected ? rgb(248, 210, 105) : rgb(226, 238, 233));
        char stock[10];
        uint8_t count = model.state ? model.state->room.food[index] : 0;
        std::snprintf(stock, sizeof(stock), "X%u", count);
        text(canvas, 296, y + 17, stock,
             count > 0 ? rgb(115, 226, 183) : rgb(91, 104, 104));
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}

}  // namespace AmoledV1
