#include "scenes/ShopScene.h"
#include <cmath>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "core/UiMotion.h"
#include "game/ExploreItemProgression.h"
#include "game/ShopService.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

// Out-of-class definitions required by C++11 for odr-used constexpr arrays.
constexpr ShopScene::Item ShopScene::EXPLORE_ITEMS[];
constexpr ShopScene::Item ShopScene::DAILY_ITEMS[];
constexpr ShopScene::Item ShopScene::SELL_ITEMS[];

namespace {
static constexpr float CATEGORY_ICON_COLUMN_X = 128.0f;
static constexpr float SUBMENU_ICON_COLUMN_X = 28.0f;
static constexpr float ICON_COLUMN_SPEED = 420.0f;
static constexpr float ITEM_SCROLL_LERP = 0.25f;
static constexpr float SHOP_ICON_SCALE = 1.0f;
static constexpr float SELECTED_SHOP_ICON_SCALE = 1.2f;
static constexpr int CONTENT_TOP = 24;
static constexpr int ICON_ROW_H = 37;
static constexpr int ICON_CENTER_Y = Hal::DISPLAY_H / 2;
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
    int y1 = y + h > Hal::DISPLAY_H ? Hal::DISPLAY_H : y + h;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            uint16_t color = (uint16_t)c.readPixel(px, py);
            if (color == background) continue;
            c.drawPixel(px, py, darkenRgb565(color, keep));
        }
    }
}

uint8_t utf8CharBytes(uint8_t lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

int utf8GlyphWidth(uint8_t lead) {
    if (lead >= 0x80) return 16;
    return lead == ' ' ? 5 : 8;
}

void drawWrappedText(const char* value, int x, int y, int maxWidth,
                     uint16_t color, uint8_t maxLines) {
    if (!value || !*value || maxWidth <= 0 || maxLines == 0) return;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    char line[64] = {};
    uint8_t lineBytes = 0;
    int lineWidth = 0;
    uint8_t lineIndex = 0;

    auto flushLine = [&]() {
        if (lineBytes == 0 || lineIndex >= maxLines) return;
        line[lineBytes] = '\0';
        PixelRenderer::text(x, y + lineIndex * 16, line, color, 1);
        ++lineIndex;
        lineBytes = 0;
        lineWidth = 0;
    };

    while (*p && lineIndex < maxLines) {
        if (*p == '\n') {
            ++p;
            flushLine();
            continue;
        }
        uint8_t bytes = utf8CharBytes(*p);
        int glyphWidth = utf8GlyphWidth(*p);
        if (lineBytes > 0 && lineWidth + glyphWidth > maxWidth) {
            flushLine();
            if (lineIndex >= maxLines) break;
        }
        if (lineBytes + bytes >= sizeof(line)) {
            flushLine();
            if (lineIndex >= maxLines) break;
        }
        for (uint8_t i = 0; i < bytes && p[i]; ++i) line[lineBytes++] = (char)p[i];
        lineWidth += glyphWidth;
        p += bytes;
    }
    flushLine();
}
}

void ShopScene::onEnter() {
    viewMode = ViewMode::CATEGORY;
    activeCategory = CATEGORY_DAILY;
    cursor = 0;
    itemColumnX = CATEGORY_ICON_COLUMN_X;
    itemAnimCursor = 0.0f;
    toast = nullptr;
    toastUntil = 0;
}

SceneUpdateResult ShopScene::update(uint32_t nowMs, float dtSeconds) {
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

bool ShopScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && !itemColumnSettled()) return true;
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        if (viewMode == ViewMode::CATEGORY) {
            GameEngine::ins().requestScene(SceneID::MENU);
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

void ShopScene::activateCategory() {
    switch (cursor) {
    case CATEGORY_DAILY:
        activeCategory = CATEGORY_DAILY;
        viewMode = ViewMode::DAILY;
        cursor = 0;
        break;
    case CATEGORY_EXPLORE:
        activeCategory = CATEGORY_EXPLORE;
        viewMode = ViewMode::EXPLORE;
        cursor = 0;
        break;
    case CATEGORY_SELL:
        activeCategory = CATEGORY_SELL;
        viewMode = ViewMode::SELL;
        cursor = 0;
        break;
    default:
        GameEngine::ins().requestScene(SceneID::MENU);
        break;
    }
}

void ShopScene::returnToCategories() {
    viewMode = ViewMode::CATEGORY;
    cursor = (uint8_t)activeCategory;
    toast = nullptr;
}

void ShopScene::buyCurrent() {
    Item item = currentItem();
    if (item == BACK) {
        returnToCategories();
        return;
    }

    auto& engine = GameEngine::ins();
    if (item == CANDY && !engine.candyPurchaseAvailable()) {
        toast = Ui::Shop::CANDY_DAILY_LIMIT;
        toastUntil = Hal::ins().millis() + 1200;
        return;
    }

    uint16_t price = priceFor(item);
    if (!engine.spendCoins(price)) {
        toast = Ui::Shop::NOT_ENOUGH_COINS;
        toastUntil = Hal::ins().millis() + 1200;
        return;
    }

    Game::ItemId gameItem = gameItemIdFor(item);
    int8_t foodIndex = Game::foodIndexForItemId(gameItem);
    bool added = foodIndex >= 0
        ? engine.addFoodStock((uint8_t)foodIndex, 1)
        : engine.addItem(gameItem, 1);
    if (!added) {
        engine.addCoins(price);
        toast = Ui::Shop::BAG_FULL;
    } else if (item == CANDY && !engine.recordCandyPurchase()) {
        engine.removeItem(gameItem, 1);
        engine.addCoins(price);
        toast = Ui::Shop::CANDY_DAILY_LIMIT;
    } else {
        snprintf(toastBuffer, sizeof(toastBuffer), Ui::Shop::BOUGHT_FMT, nameFor(item));
        toast = toastBuffer;
    }
    toastUntil = Hal::ins().millis() + 1200;
}

void ShopScene::sellCurrent() {
    Item item = sellItemAtIndex(cursor);
    if (item == BACK) {
        returnToCategories();
        return;
    }

    uint16_t sellPrice = sellPriceFor(item);
    if (GameEngine::ins().removeItem(gameItemIdFor(item))) {
        GameEngine::ins().addCoins(sellPrice);
        snprintf(toastBuffer, sizeof(toastBuffer), Ui::Shop::SOLD_FMT, sellPrice);
        toast = toastBuffer;
        toastUntil = Hal::ins().millis() + 1000;
    }

    uint8_t count = sellItemCount();
    if (count > 0 && cursor >= count) cursor = count - 1;
}

ShopScene::Item ShopScene::currentItem() const {
    return itemAtIndex(cursor);
}

ShopScene::Item ShopScene::selectedItem() const {
    return viewMode == ViewMode::SELL ? sellItemAtIndex(cursor) : currentItem();
}

ShopScene::Item ShopScene::itemForCategory(Category category, uint8_t index) const {
    if (category == CATEGORY_SELL) return sellItemAtIndex(index);
    if (category == CATEGORY_BACK) return BACK;

    const Item* items = category == CATEGORY_DAILY
        ? DAILY_ITEMS : EXPLORE_ITEMS;
    uint8_t itemCount = category == CATEGORY_DAILY
        ? DAILY_ITEM_COUNT : EXPLORE_ITEM_COUNT;
    uint8_t visible = 0;
    for (uint8_t i = 0; i < itemCount; ++i) {
        Item item = items[i];
        if (item == BACK || !itemUnlocked(item)) continue;
        if (visible == index) return item;
        ++visible;
    }
    return BACK;
}

bool ShopScene::itemUnlocked(Item item) const {
    Game::ItemId gameItem = gameItemIdFor(item);
    return gameItem != Game::ItemId::COUNT &&
        ExploreItemProgression::isShopItemUnlocked(
            gameItem, GameEngine::ins().gameState());
}

ShopScene::Item ShopScene::itemAtIndex(uint8_t index) const {
    return itemForCategory(activeCategory, index);
}

ShopScene::Item ShopScene::sellItemAtIndex(uint8_t index) const {
    uint8_t visible = 0;
    for (Item item : SELL_ITEMS) {
        if (ownedCountFor(item) == 0) continue;
        if (visible == index) return item;
        ++visible;
    }
    return BACK;
}

uint8_t ShopScene::currentItemCount() const {
    return viewMode == ViewMode::CATEGORY
        ? CATEGORY_COUNT : itemCountForCategory(activeCategory, true);
}

uint8_t ShopScene::itemCountForCategory(Category category, bool includeBack) const {
    if (category == CATEGORY_SELL) {
        uint8_t count = sellItemCount();
        return !includeBack && count > 0 ? static_cast<uint8_t>(count - 1) : count;
    }
    if (category != CATEGORY_DAILY && category != CATEGORY_EXPLORE) return 0;

    const Item* items = category == CATEGORY_DAILY
        ? DAILY_ITEMS : EXPLORE_ITEMS;
    uint8_t itemCount = category == CATEGORY_DAILY
        ? DAILY_ITEM_COUNT : EXPLORE_ITEM_COUNT;
    uint8_t count = includeBack ? 1 : 0;
    for (uint8_t i = 0; i < itemCount; ++i) {
        if (items[i] != BACK && itemUnlocked(items[i])) ++count;
    }
    return count;
}

uint8_t ShopScene::sellItemCount() const {
    uint8_t count = 1; // Back is always visible.
    for (Item item : SELL_ITEMS) {
        if (ownedCountFor(item) > 0) ++count;
    }
    return count;
}

uint8_t ShopScene::ownedCountFor(Item item) const {
    Game::ItemId gameItem = gameItemIdFor(item);
    if (gameItem == Game::ItemId::COUNT) return 0;
    return GameEngine::ins().itemCount(gameItem);
}

bool ShopScene::itemColumnSettled() const {
    float targetX = viewMode == ViewMode::CATEGORY
        ? CATEGORY_ICON_COLUMN_X : SUBMENU_ICON_COLUMN_X;
    return fabsf(itemColumnX - targetX) < 0.5f;
}

void ShopScene::render() {
    static_assert(sizeof(Ui::Shop::NAMES) / sizeof(Ui::Shop::NAMES[0]) == COUNT,
                  "shop labels must match ShopScene::Item");
    static_assert(sizeof(Ui::Shop::DESCS) / sizeof(Ui::Shop::DESCS[0]) == COUNT,
                  "shop descriptions must match ShopScene::Item");
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));
    c.fillRect(0, 0, Hal::DISPLAY_W, 20, PixelRenderer::rgb(25, 25, 40));
    PixelRenderer::text(4, 2, Ui::SHOP, PixelRenderer::rgb(67, 213, 224), 1);

    char coins[20];
    snprintf(coins, sizeof(coins), Ui::Shop::COINS_SHORT_FMT, (unsigned long)GameEngine::ins().coinCount());
    PixelRenderer::text(188, 2, coins, PixelRenderer::rgb(255, 216, 72), 1);

    renderList();
    renderToast();
}

void ShopScene::renderList() {
    if (viewMode == ViewMode::CATEGORY) {
        renderCategoryList();
    } else {
        renderSubmenu();
    }
}

void ShopScene::renderCategoryList() {
    auto& c = PixelRenderer::canvas();
    if (itemColumnSettled()) {
        static constexpr int DIVIDER_X = 76;
        static constexpr int START_Y = 26;
        static constexpr int ROW_H = 24;
        static constexpr int TEXT_X = 16;
        c.drawFastVLine(DIVIDER_X, CONTENT_TOP, Hal::DISPLAY_H - CONTENT_TOP - 5,
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

void ShopScene::renderSubmenu() {
    auto& c = PixelRenderer::canvas();
    bool settled = itemColumnSettled();
    renderIconColumn((int)roundf(itemColumnX), false, settled);
    if (!settled) return;

    c.drawFastVLine(SUBMENU_DIVIDER_X, CONTENT_TOP, Hal::DISPLAY_H - CONTENT_TOP - 5,
                    PixelRenderer::rgb(70, 74, 84));
    renderItemDetail();
}

void ShopScene::renderIconColumn(int centerX, bool dimmed, bool selectable) {
    auto& c = PixelRenderer::canvas();
    Category category = viewMode == ViewMode::CATEGORY
        ? (Category)cursor : activeCategory;
    uint8_t itemCount = itemCountForCategory(category, viewMode != ViewMode::CATEGORY);
    if (itemCount == 0) return;

    c.setClipRect(centerX - 29, CONTENT_TOP, 58, Hal::DISPLAY_H - CONTENT_TOP);
    for (uint8_t pass = 0; pass < 2; ++pass) {
        for (uint8_t i = 0; i < itemCount; ++i) {
            bool selected = selectable && i == cursor;
            if (selected != (pass == 1)) continue;

            int centerY = ICON_CENTER_Y +
                (int)roundf(((float)i - itemAnimCursor) * ICON_ROW_H);
            if (centerY + 24 < CONTENT_TOP || centerY - 24 >= Hal::DISPLAY_H) continue;

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
        c.fillTriangle(centerX + 21, Hal::DISPLAY_H - 7,
                       centerX + 27, Hal::DISPLAY_H - 7,
                       centerX + 24, Hal::DISPLAY_H - 3, arrowColor);
    }
}

void ShopScene::drawItemIcon(Item item, int centerX, int centerY, float scale,
                             uint16_t fallbackColor) const {
    auto& c = PixelRenderer::canvas();
    if (item == BACK) {
        int halfW = (int)roundf(6.0f * scale);
        int halfH = (int)roundf(5.0f * scale);
        c.drawFastHLine(centerX - halfW, centerY, halfW * 2, fallbackColor);
        c.drawLine(centerX - halfW, centerY, centerX - 1, centerY - halfH, fallbackColor);
        c.drawLine(centerX - halfW, centerY, centerX - 1, centerY + halfH, fallbackColor);
        return;
    }
    if (GameAssets::drawCentered(GameAssets::itemKind(gameItemIdFor(item)),
                                 centerX, centerY, scale)) return;
    PixelRenderer::text(centerX - 4, centerY - 8, "?", fallbackColor, 1);
}

void ShopScene::renderItemDetail() {
    static constexpr int DETAIL_X = SUBMENU_DIVIDER_X + 10;
    static constexpr int DETAIL_W = Hal::DISPLAY_W - DETAIL_X - 6;
    Item item = selectedItem();
    uint16_t titleColor = PixelRenderer::rgb(241, 242, 232);
    uint16_t accentColor = PixelRenderer::rgb(255, 216, 72);
    uint16_t descColor = PixelRenderer::rgb(135, 214, 238);

    PixelRenderer::text(DETAIL_X, 27, nameFor(item), titleColor, 1);
    if (item == BACK) {
        if (viewMode == ViewMode::SELL && sellItemCount() == 1) {
            drawWrappedText(Ui::Shop::SELL_EMPTY, DETAIL_X, 52, DETAIL_W,
                            PixelRenderer::rgb(255, 218, 178), 2);
            drawWrappedText(Ui::Shop::BACK_TO_CATEGORY, DETAIL_X, 91, DETAIL_W,
                            descColor, 2);
        } else {
            drawWrappedText(Ui::Shop::BACK_TO_CATEGORY, DETAIL_X, 60, DETAIL_W,
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

    drawWrappedText(descFor(item), DETAIL_X, 76, DETAIL_W, descColor, 3);
}

void ShopScene::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

uint16_t ShopScene::priceFor(Item item) {
    return Game::ShopService::buyPrice(gameItemIdFor(item));
}

uint16_t ShopScene::sellPriceFor(Item item) {
    return Game::ShopService::sellPrice(gameItemIdFor(item));
}

Game::ItemId ShopScene::gameItemIdFor(Item item) {
    switch (item) {
    case POTION: return Game::ItemId::POTION;
    case SUPER_POTION: return Game::ItemId::SUPER_POTION;
    case ANTIDOTE: return Game::ItemId::ANTIDOTE;
    case PARALYZE_HEAL: return Game::ItemId::PARALYZE_HEAL;
    case AWAKENING: return Game::ItemId::AWAKENING;
    case BURN_HEAL: return Game::ItemId::BURN_HEAL;
    case ICE_HEAL: return Game::ItemId::ICE_HEAL;
    case FOOD: return Game::ItemId::NORMAL_FOOD;
    case TASTY_FOOD: return Game::ItemId::TASTY_FOOD;
    case SWEET_FOOD: return Game::ItemId::SWEET_FOOD;
    case SPICY_FOOD: return Game::ItemId::SPICY_FOOD;
    case SOUR_FOOD: return Game::ItemId::SOUR_FOOD;
    case BITTER_FOOD: return Game::ItemId::BITTER_FOOD;
    case DRY_FOOD: return Game::ItemId::DRY_FOOD;
    case CANDY: return Game::ItemId::CANDY;
    case MAX_POTION: return Game::ItemId::MAX_POTION;
    case FULL_RESTORE: return Game::ItemId::FULL_RESTORE;
    case FULL_HEAL: return Game::ItemId::FULL_HEAL;
    case FIRE_STONE: return Game::ItemId::FIRE_STONE;
    case WATER_STONE: return Game::ItemId::WATER_STONE;
    case THUNDER_STONE: return Game::ItemId::THUNDER_STONE;
    case REVIVE: return Game::ItemId::REVIVE;
    case MAX_REPEL: return Game::ItemId::MAX_REPEL;
    case HONEY: return Game::ItemId::HONEY;
    case NUGGET: return Game::ItemId::NUGGET;
    case BIG_PEARL: return Game::ItemId::BIG_PEARL;
    case STAR_PIECE: return Game::ItemId::STAR_PIECE;
    case SOAP_0: return Game::ItemId::SOAP_0;
    case SOAP_1: return Game::ItemId::SOAP_1;
    case SOAP_2: return Game::ItemId::SOAP_2;
    default: return Game::ItemId::COUNT;
    }
}

const char* ShopScene::nameFor(Item item) {
    uint8_t idx = (uint8_t)item;
    if (idx >= COUNT) return Ui::BACK;
    return Ui::Shop::NAMES[idx];
}

const char* ShopScene::descFor(Item item) {
    uint8_t idx = (uint8_t)item;
    if (idx >= COUNT) return "";
    return Ui::Shop::DESCS[idx];
}
