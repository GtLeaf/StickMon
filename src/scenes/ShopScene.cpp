#include "scenes/ShopScene.h"
#include <cstdio>
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "hardware/PixelRenderer.h"

void ShopScene::update(uint32_t nowMs, float dtSeconds) {
    (void)nowMs;
    (void)dtSeconds;
}

bool ShopScene::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        GameEngine::ins().requestScene(SceneID::MENU);
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        cursor = (cursor + 1) % COUNT;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        buyCurrent();
        return true;
    }
    return false;
}

void ShopScene::buyCurrent() {
    Item item = (Item)cursor;
    if (item == BACK) {
        GameEngine::ins().requestScene(SceneID::MENU);
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

void ShopScene::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(7, 9, 14));
    c.fillRect(0, 0, Hal::DISPLAY_W, 24, PixelRenderer::rgb(25, 25, 40));
    PixelRenderer::text(4, 5, Ui::SHOP, PixelRenderer::rgb(67, 213, 224), 1);

    char coins[20];
    snprintf(coins, sizeof(coins), Ui::Shop::COINS_SHORT_FMT, (unsigned long)GameEngine::ins().coinCount());
    PixelRenderer::text(82, 5, coins, PixelRenderer::rgb(255, 216, 72), 1);

    renderList();
    renderToast();
}

void ShopScene::renderList() {
    auto& c = PixelRenderer::canvas();
    const int startY = 28;
    const int rowH = 25;
    const int visibleRows = min<int>(COUNT, (206 - startY) / rowH);
    int first = 0;
    if (cursor >= visibleRows) {
        first = cursor - visibleRows + 1;
    }

    for (int row = 0; row < visibleRows; ++row) {
        int i = first + row;
        if (i >= COUNT) break;
        int y = startY + row * rowH;
        bool selected = i == cursor;
        uint16_t fg = selected ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(241, 242, 232);
        if (selected) c.fillRect(4, y + 4, 5, 17, PixelRenderer::rgb(255, 216, 72));

        PixelRenderer::text(16, y + 4, Ui::Shop::NAMES[i], fg, 1);
        if (i != BACK) {
            char price[16];
            snprintf(price, sizeof(price), "%uC", priceFor((Item)i));
            PixelRenderer::text(96, y + 4, price, PixelRenderer::rgb(92, 222, 112), 1);
        }
        if (row < visibleRows - 1 && i < COUNT - 1) {
            c.drawFastHLine(4, y + rowH - 2, Hal::DISPLAY_W - 8, PixelRenderer::rgb(70, 74, 84));
        }
    }

    if (COUNT > visibleRows && first > 0) {
        c.fillTriangle(122, 29, 127, 29, 124, 25, PixelRenderer::rgb(135, 214, 238));
    }
    if (COUNT > visibleRows && first + visibleRows < COUNT) {
        c.fillTriangle(122, 204, 127, 204, 124, 208, PixelRenderer::rgb(135, 214, 238));
    }
}

void ShopScene::renderToast() {
    if (!toast || Hal::ins().millis() > toastUntil) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(14, 210, 107, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(22, 216, toast, PixelRenderer::rgb(255, 255, 255), 1);
}

uint16_t ShopScene::priceFor(Item item) {
    switch (item) {
    case BALL: return 50;
    case GREAT_BALL: return 120;
    case FOOD: return 20;
    case POTION: return 60;
    case ANTIDOTE: return 40;
    case CANDY: return 200;
    default: return 0;
    }
}
