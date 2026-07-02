#include "scenes/ShopScene.h"
#include <cstdio>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

void ShopScene::onEnter() {
    viewMode = ViewMode::CATEGORY;
    cursor = 0;
    toast = nullptr;
    toastUntil = 0;
}

void ShopScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
}

bool ShopScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        if (viewMode == ViewMode::CATEGORY) {
            GameEngine::ins().requestScene(SceneID::MENU);
        } else {
            viewMode = ViewMode::CATEGORY;
            cursor = 0;
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
    case CATEGORY_EXPLORE:
        viewMode = ViewMode::EXPLORE;
        cursor = 0;
        break;
    case CATEGORY_DAILY:
        viewMode = ViewMode::DAILY;
        cursor = 0;
        break;
    case CATEGORY_SELL:
        viewMode = ViewMode::SELL;
        cursor = 0;
        break;
    default:
        GameEngine::ins().requestScene(SceneID::MENU);
        break;
    }
}

void ShopScene::buyCurrent() {
    Item item = currentItem();
    if (item == BACK) {
        viewMode = ViewMode::CATEGORY;
        cursor = 0;
        return;
    }

    uint16_t price = priceFor(item);
    if (!GameEngine::ins().spendCoins(price)) {
        toast = Ui::Shop::NOT_ENOUGH_COINS;
        toastUntil = Hal::ins().millis() + 1200;
        return;
    }

    switch (item) {
    case BALL:
        GameEngine::ins().addBalls(1);
        toast = Ui::Shop::BOUGHT_BALL;
        break;
    case GREAT_BALL:
        if (!GameEngine::ins().addGreatBalls(1)) {
            GameEngine::ins().addCoins(price);
            toast = Ui::Shop::BAG_FULL;
        } else toast = Ui::Shop::BOUGHT_GREAT_BALL;
        break;
    case FOOD:
        GameEngine::ins().addFood();
        toast = Ui::Shop::BOUGHT_FOOD;
        break;
    case POTION:
        if (!GameEngine::ins().addPotion(1)) {
            GameEngine::ins().addCoins(price);
            toast = Ui::Shop::BAG_FULL;
        } else toast = Ui::Shop::BOUGHT_POTION;
        break;
    case SUPER_POTION:
        if (!GameEngine::ins().addSuperPotion(1)) {
            GameEngine::ins().addCoins(price);
            toast = Ui::Shop::BAG_FULL;
        } else toast = Ui::Shop::BOUGHT_POTION;
        break;
    case ANTIDOTE:
        if (!GameEngine::ins().addAntidote(1)) {
            GameEngine::ins().addCoins(price);
            toast = Ui::Shop::BAG_FULL;
        } else toast = Ui::Shop::BOUGHT_ANTIDOTE;
        break;
    case CANDY:
        GameEngine::ins().addCandy(1);
        toast = Ui::Shop::BOUGHT_CANDY;
        break;
    default:
        break;
    }
    toastUntil = Hal::ins().millis() + 1200;
}

void ShopScene::sellCurrent() {
    Item item = sellItemAtIndex(cursor);
    if (item == BACK) {
        viewMode = ViewMode::CATEGORY;
        cursor = 0;
        return;
    }

    uint16_t sellPrice = sellPriceFor(item);
    if (GameEngine::ins().removeItem(gameItemIdFor(item), 1, false)) {
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

ShopScene::Item ShopScene::itemAtIndex(uint8_t index) const {
    static constexpr Item EXPLORE_ITEMS[] = {
        BALL,
        GREAT_BALL,
        POTION,
        ANTIDOTE,
        BACK,
    };
    static constexpr Item DAILY_ITEMS[] = {
        FOOD,
        CANDY,
        BACK,
    };

    if (viewMode == ViewMode::DAILY) {
        uint8_t idx = index < (sizeof(DAILY_ITEMS) / sizeof(DAILY_ITEMS[0])) ? index : 0;
        return DAILY_ITEMS[idx];
    }
    uint8_t idx = index < (sizeof(EXPLORE_ITEMS) / sizeof(EXPLORE_ITEMS[0])) ? index : 0;
    return EXPLORE_ITEMS[idx];
}

ShopScene::Item ShopScene::sellItemAtIndex(uint8_t index) const {
    static constexpr Item SELL_ITEMS[] = {
        BALL,
        GREAT_BALL,
        FOOD,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        CANDY,
    };

    uint8_t visible = 0;
    for (Item item : SELL_ITEMS) {
        if (ownedCountFor(item) == 0) continue;
        if (visible == index) return item;
        ++visible;
    }
    return BACK;
}

uint8_t ShopScene::currentItemCount() const {
    switch (viewMode) {
    case ViewMode::EXPLORE:
        return 5;
    case ViewMode::DAILY:
        return 3;
    case ViewMode::SELL:
        return sellItemCount();
    default:
        return CATEGORY_COUNT;
    }
}

uint8_t ShopScene::sellItemCount() const {
    uint8_t count = 1; // Back is always visible.
    static constexpr Item SELL_ITEMS[] = {
        BALL,
        GREAT_BALL,
        FOOD,
        POTION,
        SUPER_POTION,
        ANTIDOTE,
        CANDY,
    };
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

void ShopScene::render() {
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
    } else if (viewMode == ViewMode::SELL) {
        renderSellPage();
    } else {
        renderItemList();
    }
}

void ShopScene::renderCategoryList() {
    auto& c = PixelRenderer::canvas();
    const int startY = 26;
    const int rowH = 24;
    const int iconX = 22;
    const int textX = 48;

    for (uint8_t i = 0; i < CATEGORY_COUNT; ++i) {
        int y = startY + i * rowH;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) c.fillRect(8, y + 3, 4, 14, PixelRenderer::rgb(255, 216, 72));
        c.fillRect(iconX, y + 3, 14, 14,
                   i == CATEGORY_BACK ? PixelRenderer::rgb(92, 98, 110) : PixelRenderer::rgb(135, 214, 238));
        PixelRenderer::text(textX, y + 2, Ui::Shop::CATEGORY_ITEMS[i], fg, 1);
        if (i + 1 < CATEGORY_COUNT) {
            c.drawFastHLine(textX, y + rowH - 1, 150, PixelRenderer::rgb(70, 74, 84));
        }
    }
}

void ShopScene::renderItemList() {
    auto& c = PixelRenderer::canvas();
    const int startY = 24;
    const int rowH = 20;
    const int itemCount = currentItemCount();
    const int visibleRows = min<int>(itemCount, (Hal::DISPLAY_H - startY - 4) / rowH);
    int first = 0;
    if (cursor >= visibleRows) {
        first = cursor - visibleRows + 1;
    }

    for (int row = 0; row < visibleRows; ++row) {
        int i = first + row;
        if (i >= itemCount) break;
        Item item = itemAtIndex(i);
        int y = startY + row * rowH;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) c.fillRect(8, y + 4, 4, 12, PixelRenderer::rgb(255, 216, 72));

        c.fillRect(20, y + 4, 12, 12, item == BACK ? PixelRenderer::rgb(92, 98, 110) : PixelRenderer::rgb(92, 222, 112));
        PixelRenderer::text(42, y + 2, nameFor(item), fg, 1);
        if (item != BACK) {
            char price[16];
            snprintf(price, sizeof(price), "%uC", priceFor(item));
            PixelRenderer::text(164, y + 2, price, PixelRenderer::rgb(92, 222, 112), 1);
        }
        if (row < visibleRows - 1 && i < itemCount - 1) {
            c.drawFastHLine(42, y + rowH - 1, 154, PixelRenderer::rgb(70, 74, 84));
        }
    }

    if (itemCount > visibleRows && first > 0) {
        c.fillTriangle(226, 31, 233, 31, 229, 27, PixelRenderer::rgb(135, 214, 238));
    }
    if (itemCount > visibleRows && first + visibleRows < itemCount) {
        c.fillTriangle(226, 124, 233, 124, 229, 128, PixelRenderer::rgb(135, 214, 238));
    }

    Item selected = currentItem();
    if (selected != BACK) {
        c.fillRect(16, 104, 188, 14, PixelRenderer::rgb(7, 9, 14));
        PixelRenderer::text(18, 104, descFor(selected), PixelRenderer::rgb(135, 214, 238), 1);
    }
}

void ShopScene::renderSellPage() {
    auto& c = PixelRenderer::canvas();
    const int startY = 24;
    const int rowH = 20;
    const int itemCount = sellItemCount();
    const int visibleRows = min<int>(itemCount, (Hal::DISPLAY_H - startY - 4) / rowH);
    int first = 0;
    if (cursor >= visibleRows) {
        first = cursor - visibleRows + 1;
    }

    if (itemCount == 1) {
        PixelRenderer::text(42, 48, Ui::Shop::SELL_EMPTY, PixelRenderer::rgb(241, 242, 232), 1);
    }

    for (int row = 0; row < visibleRows; ++row) {
        int i = first + row;
        if (i >= itemCount) break;
        Item item = sellItemAtIndex(i);
        int y = startY + row * rowH;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) c.fillRect(8, y + 4, 4, 12, PixelRenderer::rgb(255, 216, 72));

        c.fillRect(20, y + 4, 12, 12, item == BACK ? PixelRenderer::rgb(92, 98, 110) : PixelRenderer::rgb(247, 167, 74));
        PixelRenderer::text(42, y + 2, nameFor(item), fg, 1);
        if (item != BACK) {
            char countText[8];
            snprintf(countText, sizeof(countText), Ui::Bag::COUNT_X_FMT, ownedCountFor(item));
            PixelRenderer::text(116, y + 2, countText, PixelRenderer::rgb(255, 218, 178), 1);

            char price[16];
            snprintf(price, sizeof(price), "%uC", sellPriceFor(item));
            PixelRenderer::text(164, y + 2, price, PixelRenderer::rgb(92, 222, 112), 1);
        }
        if (row < visibleRows - 1 && i < itemCount - 1) {
            c.drawFastHLine(42, y + rowH - 1, 154, PixelRenderer::rgb(70, 74, 84));
        }
    }

    if (itemCount > visibleRows && first > 0) {
        c.fillTriangle(226, 31, 233, 31, 229, 27, PixelRenderer::rgb(135, 214, 238));
    }
    if (itemCount > visibleRows && first + visibleRows < itemCount) {
        c.fillTriangle(226, 124, 233, 124, 229, 128, PixelRenderer::rgb(135, 214, 238));
    }

    Item selected = sellItemAtIndex(cursor);
    if (selected != BACK) {
        c.fillRect(16, 104, 188, 14, PixelRenderer::rgb(7, 9, 14));
        char line[24];
        snprintf(line, sizeof(line), Ui::Shop::SELL_PRICE_FMT, sellPriceFor(selected));
        PixelRenderer::text(18, 104, line, PixelRenderer::rgb(135, 214, 238), 1);
    }
}

void ShopScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

uint16_t ShopScene::priceFor(Item item) {
    switch (item) {
    case BALL: return 50;
    case GREAT_BALL: return 120;
    case FOOD: return 20;
    case POTION: return 60;
    case SUPER_POTION: return 120;
    case ANTIDOTE: return 40;
    case CANDY: return 200;
    default: return 0;
    }
}

uint16_t ShopScene::sellPriceFor(Item item) {
    return priceFor(item) / 2;
}

Game::ItemId ShopScene::gameItemIdFor(Item item) {
    switch (item) {
    case BALL: return Game::ItemId::POKE_BALL;
    case GREAT_BALL: return Game::ItemId::GREAT_BALL;
    case POTION: return Game::ItemId::POTION;
    case SUPER_POTION: return Game::ItemId::SUPER_POTION;
    case ANTIDOTE: return Game::ItemId::ANTIDOTE;
    case FOOD: return Game::ItemId::NORMAL_FOOD;
    case CANDY: return Game::ItemId::CANDY;
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
