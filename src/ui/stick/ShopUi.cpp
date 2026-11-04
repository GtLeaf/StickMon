#include "ui/stick/ShopUi.h"
#include "presentation/TouchUi.h"
#include "presentation/PixelRenderer.h"
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "core/UiMotion.h"
#include "game/ExploreItemProgression.h"
#include "hardware/Hal.h"
#include <cmath>
#include <cstdio>

namespace Ui {
namespace {
static constexpr float CATEGORY_ICON_COLUMN_X = 128.0f;
static constexpr float SUBMENU_ICON_COLUMN_X = 28.0f;
static constexpr float ICON_COLUMN_SPEED = 420.0f;
static constexpr float ITEM_SCROLL_LERP = 0.25f;
static constexpr float SHOP_ICON_SCALE = 1.0f;
static constexpr float SELECTED_SHOP_ICON_SCALE = 1.2f;
static constexpr int CONTENT_TOP = 24;
static constexpr int ICON_ROW_H = 37;
static constexpr int ICON_CENTER_Y = TouchUi::CONTENT_H / 2;
static constexpr int SUBMENU_DIVIDER_X = 58;

uint16_t darkenRgb565(uint16_t color, uint8_t keep) {
    uint16_t red = (uint16_t)((((color >> 11) & 0x1F) * keep) / 255);
    uint16_t green = (uint16_t)((((color >> 5) & 0x3F) * keep) / 255);
    uint16_t blue = (uint16_t)(((color & 0x1F) * keep) / 255);
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

void shadeRect(int x, int y, int w, int h, uint8_t keep) {
    auto& c = PixelRenderer::canvas();
    const uint16_t background = PixelRenderer::rgb(7, 9, 14);
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > Hal::DISPLAY_W ? Hal::DISPLAY_W : x + w;
    int y1 = y + h > TouchUi::CONTENT_H ? TouchUi::CONTENT_H : y + h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            uint16_t color = (uint16_t)c.readPixel(px, py);
            if (color == background) continue;
            c.drawPixel(px, py, darkenRgb565(color, keep));
        }
    }
}

}

void ShopUi::onEnter() {
    cancelTouch();
    bind(host.shopState());
    reset(true);
    itemColumnX = CATEGORY_ICON_COLUMN_X;
    itemAnimCursor = 0.0f;
    toast = nullptr;
    toastUntil = 0;
    columnTraceMask = 0;
    iconTraceMask = 0;
    const Game::GameState& state = host.shopState();
    Platform::logf(
        "[ShopIcon] enter daily=%u explore=%u sell=%u shopArea=%u assets=%u\n",
        Game::ShopService::buyItemCount(Game::ShopService::Category::DAILY, state),
        Game::ShopService::buyItemCount(Game::ShopService::Category::EXPLORE, state),
        Game::ShopService::sellItemCount(state),
        ExploreItemProgression::shopUnlockedArea(state),
        GameAssets::available() ? 1U : 0U);
}

SceneUpdateResult ShopUi::update(uint32_t nowMs, float dtSeconds) {
    RenderDemand demand;
    bool animating = false;
    float targetX = viewMode == ViewMode::CATEGORY
        ? CATEGORY_ICON_COLUMN_X : SUBMENU_ICON_COLUMN_X;
    float clampedDt = dtSeconds > 0.05f ? 0.05f : dtSeconds;
    UiMotion::StepResult columnStep = UiMotion::approach(
        itemColumnX, targetX, ICON_COLUMN_SPEED * clampedDt);
    demand.changed(columnStep.changed);
    animating = columnStep.active;

    float targetCursor =
        viewMode == ViewMode::CATEGORY || !itemColumnSettled()
            ? 0.0f : static_cast<float>(cursor);
    UiMotion::StepResult cursorStep = UiMotion::lerp(
        itemAnimCursor, targetCursor, ITEM_SCROLL_LERP);
    demand.changed(cursorStep.changed);
    animating = animating || cursorStep.active;

    demand.animate(animating);
    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool ShopUi::touchNavigation() const { return true; }

int ShopUi::touchTarget(int16_t x, int16_t y) {
    if (!itemColumnSettled() || y >= TouchUi::CONTENT_H) return -1;
    if (viewMode == ViewMode::CATEGORY && x >= 6 && x < 76 && y >= 26 && y < 26 + CATEGORY_COUNT * 24) {
        return (y - 26) / 24;
    }
    // Selecting a product only previews it; the separate Confirm control buys.
    if (viewMode != ViewMode::CATEGORY && x < 58 && y >= CONTENT_TOP) {
        const int index = int(std::lround(itemAnimCursor + float(y - ICON_CENTER_Y) / ICON_ROW_H));
        if (index >= 0 && index < currentItemCount()) return 256 + index;
    }
    return -1;
}

bool ShopUi::onTap(int target) {
    if (!itemColumnSettled()) return false;
    if (target >= 256 && viewMode != ViewMode::CATEGORY) {
        cursor = target - 256;
        return true;
    }
    if (viewMode != ViewMode::CATEGORY || target < 0 || target >= CATEGORY_COUNT) return false;
    cursor = target;
    return onButton({0, BtnAction::PRESSED});
}

bool ShopUi::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && !itemColumnSettled()) return true;
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        if (viewMode == ViewMode::CATEGORY) {
            host.leaveShop();
        } else {
            returnToCategories();
        }
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        uint8_t count = viewMode == ViewMode::CATEGORY ? CATEGORY_COUNT : currentItemCount();
        cursor = (cursor + 1) % count;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        if (viewMode == ViewMode::CATEGORY) {
            activateCategory();
        } else if (viewMode == ViewMode::SELL) {
            sellCurrent();
        } else {
            buyCurrent();
        }
        return true;
    }
    return false;
}

void ShopUi::activateCategory() {
    if (!Features::ShopFlow::activateCategory()) host.leaveShop();
}

void ShopUi::returnToCategories() {
    cancelTouch();
    Features::ShopFlow::returnToCategories();
    toast = nullptr;
}

bool ShopUi::itemColumnSettled() const {
    float targetX = viewMode == ViewMode::CATEGORY
        ? CATEGORY_ICON_COLUMN_X : SUBMENU_ICON_COLUMN_X;
    return fabsf(itemColumnX - targetX) < 0.5f;
}

void ShopUi::render() {
    static_assert(sizeof(Ui::Shop::NAMES) / sizeof(Ui::Shop::NAMES[0]) == COUNT,
                  "shop labels must match ShopUi::Item");
    static_assert(sizeof(Ui::Shop::DESCS) / sizeof(Ui::Shop::DESCS[0]) == COUNT,
                  "shop descriptions must match ShopUi::Item");
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(7, 9, 14));
    c.fillRect(0, 0, Hal::DISPLAY_W, 20, PixelRenderer::rgb(25, 25, 40));
    PixelRenderer::text(4, 2, Ui::SHOP, PixelRenderer::rgb(67, 213, 224), 1);

    char coins[20];
    snprintf(coins, sizeof(coins), Ui::Shop::COINS_SHORT_FMT, (unsigned long)host.shopState().coins);
    PixelRenderer::text(188, 2, coins, PixelRenderer::rgb(255, 216, 72), 1);

    renderList();
    renderToast();
}

void ShopUi::renderList() {
    if (viewMode == ViewMode::CATEGORY) {
        renderCategoryList();
    } else {
        renderSubmenu();
    }
}

void ShopUi::renderCategoryList() {
    auto& c = PixelRenderer::canvas();
    if (itemColumnSettled()) {
        static constexpr int DIVIDER_X = 76;
        static constexpr int START_Y = 26;
        static constexpr int ROW_H = 24;
        static constexpr int TEXT_X = 16;
        c.drawFastVLine(DIVIDER_X, CONTENT_TOP, TouchUi::CONTENT_H - CONTENT_TOP - 5,
                        PixelRenderer::rgb(70, 74, 84));

        for (uint8_t i = 0; i < CATEGORY_COUNT; ++i) {
            int y = START_Y + i * ROW_H;
            bool selected = i == cursor;
            uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72)
                                   : PixelRenderer::rgb(241, 242, 232);
            if (selected) {
                c.fillRect(7, y + 2, 3, 16, PixelRenderer::rgb(255, 216, 72));
            }
            PixelRenderer::text(TEXT_X, y + 2, Ui::Shop::CATEGORY_ITEMS[i], fg, 1);
        }
    }

    renderIconColumn((int)roundf(itemColumnX), true, false);
}

void ShopUi::renderSubmenu() {
    auto& c = PixelRenderer::canvas();
    bool settled = itemColumnSettled();
    renderIconColumn((int)roundf(itemColumnX), false, settled);
    if (!settled) return;

    c.drawFastVLine(SUBMENU_DIVIDER_X, CONTENT_TOP, TouchUi::CONTENT_H - CONTENT_TOP - 5,
                    PixelRenderer::rgb(70, 74, 84));
    renderItemDetail();
}

void ShopUi::renderIconColumn(int centerX, bool dimmed, bool selectable) {
    auto& c = PixelRenderer::canvas();
    Category category = viewMode == ViewMode::CATEGORY
        ? (Category)cursor : activeCategory;
    uint8_t itemCount = itemCountForCategory(category, viewMode != ViewMode::CATEGORY);
    uint8_t traceIndex = static_cast<uint8_t>(viewMode) * CATEGORY_COUNT +
                         static_cast<uint8_t>(category);
    uint16_t traceBit = traceIndex < 16 ? static_cast<uint16_t>(1U << traceIndex) : 0;
    if (traceBit != 0 && (columnTraceMask & traceBit) == 0) {
        columnTraceMask |= traceBit;
        Platform::logf(
            "[ShopIcon] column mode=%u category=%u count=%u cursor=%u center=%d dim=%u selectable=%u\n",
            static_cast<unsigned>(viewMode), static_cast<unsigned>(category),
            itemCount, cursor, centerX, dimmed ? 1U : 0U, selectable ? 1U : 0U);
    }
    if (itemCount == 0) return;

    c.setClipRect(centerX - 29, CONTENT_TOP, 58, TouchUi::CONTENT_H - CONTENT_TOP);
    for (uint8_t pass = 0; pass < 2; ++pass) {
        for (uint8_t i = 0; i < itemCount; ++i) {
            bool selected = selectable && i == cursor;
            if (selected != (pass == 1)) continue;

            int centerY = ICON_CENTER_Y +
                (int)roundf(((float)i - itemAnimCursor) * ICON_ROW_H);
            if (centerY + 24 < CONTENT_TOP || centerY - 24 >= TouchUi::CONTENT_H) continue;

            Item item = itemForCategory(category, i);
            if (selected) {
                c.fillRect(centerX - 27, centerY - 8, 3, 16,
                           PixelRenderer::rgb(255, 216, 72));
            }
            drawItemIcon(item, centerX, centerY,
                         selected ? SELECTED_SHOP_ICON_SCALE : SHOP_ICON_SCALE,
                         selected ? PixelRenderer::rgb(255, 216, 72)
                                  : PixelRenderer::rgb(135, 214, 238));
            if (dimmed) {
                shadeRect(centerX - 20, centerY - 20, 40, 40, 112);
            }
        }
    }
    c.clearClipRect();

    uint16_t arrowColor = dimmed ? PixelRenderer::rgb(55, 88, 94)
                                 : PixelRenderer::rgb(135, 214, 238);
    if (itemAnimCursor > 0.05f) {
        c.fillTriangle(centerX + 21, CONTENT_TOP + 5,
                       centerX + 27, CONTENT_TOP + 5,
                       centerX + 24, CONTENT_TOP + 1, arrowColor);
    }
    if (itemAnimCursor < (float)(itemCount - 1) - 0.05f) {
        c.fillTriangle(centerX + 21, TouchUi::CONTENT_H - 7,
                       centerX + 27, TouchUi::CONTENT_H - 7,
                       centerX + 24, TouchUi::CONTENT_H - 3, arrowColor);
    }
}

void ShopUi::renderItemDetail() {
    static constexpr int DETAIL_X = SUBMENU_DIVIDER_X + 10;
    static constexpr int DETAIL_W = Hal::DISPLAY_W - DETAIL_X - 6;
    Item item = selectedItem();
    uint16_t titleColor = PixelRenderer::rgb(241, 242, 232);
    uint16_t accentColor = PixelRenderer::rgb(255, 216, 72);
    uint16_t descColor = PixelRenderer::rgb(135, 214, 238);

    PixelRenderer::text(DETAIL_X, 27, nameFor(item), titleColor, 1);
    if (item == BACK) {
        if (viewMode == ViewMode::SELL && sellItemCount() == 1) {
            drawShopWrappedText(Ui::Shop::SELL_EMPTY, DETAIL_X, 52, DETAIL_W,
                            PixelRenderer::rgb(255, 218, 178), 2);
            drawShopWrappedText(Ui::Shop::BACK_TO_CATEGORY, DETAIL_X, 91, DETAIL_W,
                            descColor, 2);
        } else {
            drawShopWrappedText(Ui::Shop::BACK_TO_CATEGORY, DETAIL_X, 60, DETAIL_W,
                            descColor, 3);
        }
        return;
    }

    char price[20];
    snprintf(price, sizeof(price),
             viewMode == ViewMode::SELL ? Ui::Shop::SELL_PRICE_FMT : Ui::Shop::PRICE_FMT,
             viewMode == ViewMode::SELL ? sellPriceFor(item) : priceFor(item));
    PixelRenderer::text(DETAIL_X, 50, price, accentColor, 1);

    char count[20];
    snprintf(count, sizeof(count), Ui::Shop::OWNED_FMT, ownedCountFor(item));
    PixelRenderer::text(DETAIL_X + 88, 50, count,
                        PixelRenderer::rgb(255, 218, 178), 1);

    drawShopWrappedText(descFor(item), DETAIL_X, 76, DETAIL_W, descColor, 3);
}

void ShopUi::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

}  // namespace Ui
