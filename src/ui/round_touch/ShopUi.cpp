// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/ShopUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/RoundPage.h"
#include "ui/round_touch/ItemGrid.h"
#include "assets/GameAssets.h"

namespace Ui {
namespace {
constexpr MenuGrid::Rect PRODUCTS{60, 98, 240, 262};

bool panelPoint(int x, int y) {
    const int dx = 2 * x - 359, dy = 2 * y - 359;
    return x >= 0 && x < 360 && y >= 0 && y < 360 && dx * dx + dy * dy < 360 * 360;
}

}  // namespace

void ShopUi::onEnter() {
    bind(host.shopState());
    reset(false);
    touchGridScroll = 0;
    touchDetailOpen = false;
    rememberedScroll[0] = rememberedScroll[1] = 0;
    rememberedIndex[0] = rememberedIndex[1] = 0;
    cancelTouch();
    toast = nullptr;
    toastUntil = 0;
    iconTraceMask = 0;
}

SceneUpdateResult ShopUi::update(uint32_t nowMs, float) {
    RenderDemand demand;
    const float clamped = std::clamp(touchGridScroll, 0.0f, touchShopGrid().maxScroll());
    demand.changed(clamped != touchGridScroll);
    touchGridScroll = clamped;
    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

MenuGrid::Rect ShopUi::tabBounds(int index) const {
    return {66 + index * 68, 56, 60, 36};
}

MenuGrid::Rect ShopUi::tabHitBounds(int index) const { return tabBounds(index); }

void ShopUi::cancelTouch() {
    Scene::cancelTouch();
    gridTouch.cancel();
}

bool ShopUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto grid = touchShopGrid();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), touchGridScroll,
        grid.maxScroll(), !touchDetailOpen && PRODUCTS.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int ShopUi::touchTarget(int16_t x, int16_t y) {
    if (!panelPoint(x, y)) return -1;
    const int mode = static_cast<int>(viewMode) * 256;
    if (RoundPage::NATIVE_BACK.contains(x, y)) return mode + 230;
    if (coinBounds().contains(x, y)) return mode + 234;
    for (int i = 0; i < 2; ++i) if (tabHitBounds(i).contains(x, y)) return mode + 232 + i;
    if (touchDetailOpen) {
        if (detailBackBounds().contains(x, y)) return mode + 230;
        if (actionBounds().contains(x, y)) return mode + 231;
        return -1;
    }
    const int item = touchShopGrid().itemAt(x, y, touchGridScroll);
    return item < 0 ? -1 : mode + item;
}

void ShopUi::switchMode(bool sell) {
    const bool wasSell = viewMode == ViewMode::SELL;
    if (wasSell == sell) return;
    cancelTouch();
    rememberedScroll[wasSell] = touchGridScroll;
    rememberedIndex[wasSell] = touchSelectedIndex();
    viewMode = sell ? ViewMode::SELL : ViewMode::DAILY;
    activeCategory = sell ? CATEGORY_SELL : CATEGORY_DAILY;
    cursor = 0;
    touchSelect(std::min(rememberedIndex[sell], std::max(0, touchShopGrid().count() - 1)));
    touchGridScroll = std::clamp(rememberedScroll[sell], 0.0f, touchShopGrid().maxScroll());
    touchDetailOpen = false;
    toast = nullptr;
}

bool ShopUi::onTap(int target) {
    if (target < 0 || target / 256 != static_cast<int>(viewMode)) return false;
    const int index = target % 256;
    if (index == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (index == 232 || index == 233) { switchMode(index == 233); return true; }
    if (index == 234) {
        std::snprintf(exactBalance, sizeof(exactBalance), Ui::Shop::COINS_SHORT_FMT,
            static_cast<unsigned long>(host.shopState().coins));
        toast = exactBalance;
        toastUntil = Hal::ins().millis() + 2000;
        return true;
    }
    if (index == 231 && touchDetailOpen) return onButton({0, BtnAction::PRESSED});
    if (index >= touchShopGrid().count() || touchDetailOpen) return false;
    touchSelect(index);
    cancelTouch();
    touchDetailOpen = true;
    return true;
}

bool ShopUi::onButton(const ButtonEvent& event) {
    if (event.action == BtnAction::LONG_PRESS && (event.btn == 0 || event.btn == 1)) {
        cancelTouch();
        if (touchDetailOpen) touchDetailOpen = false;
        else host.leaveShop();
        return true;
    }
    if (event.action != BtnAction::PRESSED || event.btn > 1) return false;
    cancelTouch();
    if (event.btn == 1) {
        const auto grid = touchShopGrid();
        const int next = (touchSelectedIndex() + 1) % (grid.count() + 1);
        touchSelect(next);
        touchGridScroll = grid.reveal(next, touchGridScroll);
        if (selectedItem() == BACK) touchDetailOpen = false;
    } else if (selectedItem() == BACK) host.leaveShop();
    else if (!touchDetailOpen) touchDetailOpen = true;
    else if (viewMode == ViewMode::SELL) {
        const Item item = selectedItem();
        sellCurrent();
        toast = nullptr;
        toastUntil = 0;
        if (selectedItem() != item) touchDetailOpen = false;
    } else buyCurrent();
    return true;
}

void ShopUi::returnToCategories() { touchDetailOpen = false; cancelTouch(); }

ItemTouchUi::Grid ShopUi::touchShopGrid() const {
    const auto& state = host.shopState();
    if (viewMode == ViewMode::SELL)
        return RoundItems::grid(PRODUCTS, Game::ShopService::sellItemCount(state));
    return RoundItems::grid(PRODUCTS,
        Game::ShopService::buyItemCount(Game::ShopService::Category::DAILY, state),
        Game::ShopService::buyItemCount(Game::ShopService::Category::EXPLORE, state));
}

void ShopUi::touchSelect(int index) {
    if (viewMode == ViewMode::SELL) { cursor = index; return; }
    const int daily = touchShopGrid().firstCount;
    activeCategory = index < daily ? CATEGORY_DAILY : CATEGORY_EXPLORE;
    viewMode = index < daily ? ViewMode::DAILY : ViewMode::EXPLORE;
    cursor = index < daily ? index : index - daily;
}

int ShopUi::touchSelectedIndex() const {
    return cursor + (viewMode == ViewMode::EXPLORE ? touchShopGrid().firstCount : 0);
}

void ShopUi::renderTabs() {
    const bool sell = viewMode == ViewMode::SELL;
    for (int i = 0; i < 2; ++i) {
        const auto r = tabBounds(i);
        const bool selected = i == int(sell);
        const auto color = selected ? uint16_t(0xffff) : PixelRenderer::rgb(112, 112, 112);
        PixelRenderer::textSmooth(r.x + 10, r.y + 6,
            i == 0 ? Ui::Amoled::BUY : Ui::Amoled::SELL, color, 20);
        if (selected) PixelRenderer::canvas().fillRoundRect(r.x + 16, r.y + 31, 28, 2, 1, 0xffff);
    }
}

int ShopUi::formatBalance(uint32_t coins, char (&text)[24]) {
    if (coins < 10000) {
        std::snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(coins));
        return int(std::strlen(text)) * 8;
    }
    const auto whole = static_cast<unsigned long>(coins / 10000);
    const auto hundredths = static_cast<unsigned long>((coins % 10000) / 100);
    // Truncate compact amounts so the displayed balance never overstates funds.
    // Large amounts use whole ten-thousands to keep every uint32 balance legible.
    char number[16];
    if (whole >= 100 || !hundredths) std::snprintf(number, sizeof(number), "%lu", whole);
    else if (hundredths % 10 == 0)
        std::snprintf(number, sizeof(number), "%lu.%lu", whole, hundredths / 10);
    else std::snprintf(number, sizeof(number), "%lu.%02lu", whole, hundredths);
    std::snprintf(text, sizeof(text), "%s万", number);
    return int(std::strlen(number)) * 8 + 16;
}

void ShopUi::renderCoins() {
    char coins[24];
    const int width = formatBalance(host.shopState().coins, coins);
    const auto bounds = coinBounds();
    auto& c = PixelRenderer::canvas();
    const auto gold = PixelRenderer::rgb(248, 210, 105);
    const int textX = bounds.x + bounds.width - width;
    // Keep the amount right-aligned and the coin beside it, without resampling text.
    const int coinX = textX - 12, coinY = bounds.y + bounds.height / 2;
    c.fillCircle(coinX, coinY, 5, gold);
    c.drawCircle(coinX, coinY, 5, PixelRenderer::rgb(166, 119, 38));
    c.drawFastVLine(coinX, coinY - 2, 5, PixelRenderer::rgb(166, 119, 38));
    PixelRenderer::textSmooth(textX, bounds.y + 10, coins, gold, 16);
}

void ShopUi::renderDetail() {
    const Item item = selectedItem();
    GameAssets::drawItemIcon48(gameItemIdFor(item), 100, 140);
    drawShopWrappedText(nameFor(item), 142, 112, 152, PixelRenderer::rgb(226, 238, 233), 2);
    char value[32];
    std::snprintf(value, sizeof(value), Ui::Shop::OWNED_FMT, ownedCountFor(item));
    PixelRenderer::text(142, 156, value, PixelRenderer::rgb(239, 196, 154));
    const bool sell = viewMode == ViewMode::SELL;
    std::snprintf(value, sizeof(value), sell ? Ui::Shop::SELL_PRICE_FMT : Ui::Shop::PRICE_FMT,
        sell ? sellPriceFor(item) : priceFor(item));
    PixelRenderer::text(142, 178, value, PixelRenderer::rgb(248, 210, 105));
    drawShopWrappedText(descFor(item), 70, 210, 220, PixelRenderer::rgb(126, 175, 175), 3);
    const int mode = static_cast<int>(viewMode) * 256;
    ItemTouchUi::detailButton(actionBounds(), sell ? Ui::Amoled::SELL : Ui::Amoled::BUY,
        gridTouch.pressed() == mode + 231);
    ItemTouchUi::detailButton(detailBackBounds(), Ui::BACK, gridTouch.pressed() == mode + 230, true);
}

void ShopUi::render() {
    RoundPage::background();
    auto& c = PixelRenderer::canvas();
    const int mode = static_cast<int>(viewMode) * 256;
    if (touchDetailOpen) renderDetail();
    else {
        const auto grid = touchShopGrid();
        c.setClipRect(PRODUCTS.x, PRODUCTS.y, PRODUCTS.width, PRODUCTS.height);
        const bool sell = viewMode == ViewMode::SELL;
        grid.section(0, touchGridScroll, sell ? Ui::BAG : Ui::Shop::CATEGORY_DAILY);
        if (!sell) grid.section(1, touchGridScroll, Ui::Shop::CATEGORY_EXPLORE);
        const auto& state = host.shopState();
        for (int i = 0; i < grid.count(); ++i) {
            const auto r = grid.cell(i, touchGridScroll);
            if (r.y + r.height <= PRODUCTS.y || r.y >= PRODUCTS.y + PRODUCTS.height) continue;
            MenuGrid::card(c, r, gridTouch.pressed() == mode + i, i == touchSelectedIndex());
            const auto item = sell ? Game::ShopService::sellItemAt(state, i)
                : Game::ShopService::buyItemAt(i < grid.firstCount ? Game::ShopService::Category::DAILY
                    : Game::ShopService::Category::EXPLORE, state, i < grid.firstCount ? i : i - grid.firstCount);
            GameAssets::drawItemIcon48(item, r.x + r.width / 2, r.y + r.height / 2);
        }
        if (!grid.count()) MenuGrid::label(c, 180, 156,
            sell ? Ui::Amoled::NOTHING_TO_SELL : Ui::Amoled::NOTHING, 220, 0x7bef);
        c.clearClipRect();
    }
    renderTabs();
    renderCoins();
    RoundPage::nativeHeader(Ui::SHOP, gridTouch.pressed() == mode + 230);
    renderToast();
}

void ShopUi::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(70, 310, 220, 28, 6, PixelRenderer::rgb(34, 39, 47));
    MenuGrid::label(c, 180, 316, toast, 208, 0xffff);
}
}  // namespace Ui
