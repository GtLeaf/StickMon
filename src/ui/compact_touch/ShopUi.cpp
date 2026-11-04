#include "ui/compact_touch/ShopUi.h"
#include "ui/PageProfile.h"

#include "presentation/TouchUi.h"
#include "presentation/PixelRenderer.h"
#include "presentation/MenuGrid.h"
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "game/ExploreItemProgression.h"
#include "hardware/Hal.h"
#include <algorithm>
#include <cstdio>

namespace Ui {

void ShopUi::onEnter() {
    cancelTouch();
    bind(host.shopState());
    reset(false);
    touchGridScroll = 0;
    touchDetailOpen = false;
    toast = nullptr;
    toastUntil = 0;
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
    (void)dtSeconds;
    RenderDemand demand;
    touchGridScroll = std::clamp(touchGridScroll, 0.0f, touchShopGrid().maxScroll());
    if (demand.expired(toast != nullptr, nowMs, toastUntil)) toast = nullptr;
    return demand.result();
}

bool ShopUi::touchNavigation() const { return false; }

void ShopUi::cancelTouch() { Scene::cancelTouch(); gridTouch.cancel(); }

bool ShopUi::onTouch(const Platform::IInputDevice::TouchEvent& event) {
    const auto layout = touchShopGrid();
    const auto result = gridTouch.update(event, touchTarget(event.x, event.y), touchGridScroll,
        layout.maxScroll(), !touchDetailOpen && layout.layout.viewport.contains(event.x, event.y));
    const bool activated = result.tap >= 0 && onTap(result.tap);
    return result.changed || activated;
}

int ShopUi::touchTarget(int16_t x, int16_t y) {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const int mode = static_cast<int>(viewMode) * 256;
    if (TouchUi::safeBack().contains(x, y)) return mode + 230;
    if (touchDetailOpen) {
        if (ItemTouchUi::back().contains(x, y)) return mode + 230;
        if (ItemTouchUi::action().contains(x, y)) return mode + 231;
        return -1;
    }
    if (ItemTouchUi::rail(0).contains(x, y)) return mode + 232;
    if (ItemTouchUi::rail(1).contains(x, y)) return mode + 233;
    const int item = touchShopGrid().itemAt(x, y, touchGridScroll);
    return item < 0 ? -1 : mode + item;
}

bool ShopUi::onTap(int target) {
    if (target / 256 != static_cast<int>(viewMode)) return false;
    const int index = target % 256;
    if (index == 230) return onButton({1, BtnAction::LONG_PRESS});
    if (index == 231 && touchDetailOpen) return onButton({0, BtnAction::PRESSED});
    if (index == 232 || index == 233) {
        const bool sell = index == 233;
        if ((viewMode == ViewMode::SELL) == sell) return true;
        cancelTouch();
        viewMode = sell ? ViewMode::SELL : ViewMode::DAILY;
        activeCategory = sell ? CATEGORY_SELL : CATEGORY_DAILY;
        cursor = 0;
        touchGridScroll = 0;
        toast = nullptr;
        return true;
    }
    if (index < 0 || index >= touchShopGrid().count() || touchDetailOpen) return false;
    touchSelect(index);
    cancelTouch();
    touchDetailOpen = true;
    return true;
}

bool ShopUi::onButton(const ButtonEvent& event) {
    if ((event.btn == 0 || event.btn == 1) && event.action == BtnAction::LONG_PRESS) {
        cancelTouch();
        if (touchDetailOpen) touchDetailOpen = false;
        else host.leaveShop();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        const auto grid = touchShopGrid();
        const int next = (touchSelectedIndex() + 1) % (grid.count() + 1);
        touchSelect(next);
        touchGridScroll = grid.reveal(next, touchGridScroll);
        if (selectedItem() == BACK) touchDetailOpen = false;
        return true;
    }
    if (event.btn == 0 && event.action == BtnAction::PRESSED) {
        cancelTouch();
        if (selectedItem() == BACK) host.leaveShop();
        else if (!touchDetailOpen) touchDetailOpen = true;
        else if (viewMode == ViewMode::SELL) {
            const Item item = selectedItem();
            sellCurrent();
            if (selectedItem() != item) touchDetailOpen = false;
        } else buyCurrent();
        return true;
    }
    return false;
}

void ShopUi::returnToCategories() {
    cancelTouch();
    touchDetailOpen = false;
    touchGridScroll = 0;
    Features::ShopFlow::returnToCategories();
    toast = nullptr;
}

ItemTouchUi::Grid ShopUi::touchShopGrid() const {
    const auto& state = host.shopState();
    if (viewMode == ViewMode::SELL)
        return ItemTouchUi::Grid(ItemTouchUi::body(true), Game::ShopService::sellItemCount(state));
    return ItemTouchUi::Grid(ItemTouchUi::body(true),
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

void ShopUi::render() {
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    const int mode = static_cast<int>(viewMode) * 256;
    const auto grid = touchShopGrid();
    const bool sell = viewMode == ViewMode::SELL;
    if (!touchDetailOpen) {
        const auto v = grid.layout.viewport;
        c.fillRect(0, v.y, v.x, v.height, PixelRenderer::rgb(17, 24, 31));
        c.drawFastVLine(v.x - 1, v.y, v.height, PixelRenderer::rgb(67, 74, 84));
        for (int i = 0; i < 2; ++i) {
            const auto r = ItemTouchUi::rail(i);
            const bool selected = sell == (i == 1);
            if (gridTouch.pressed() == mode + 232 + i)
                c.fillRoundRect(r.x, r.y, r.width, r.height, 6, PixelRenderer::rgb(42, 61, 68));
            if (selected) c.fillRect(r.x, r.y + 22, 3, 16, PixelRenderer::rgb(248, 210, 105));
            MenuGrid::label(c, r.x + r.width / 2 + 2, r.y + 22,
                i == 0 ? Ui::Amoled::BUY : Ui::Amoled::SELL, r.width - 8,
                selected ? PixelRenderer::rgb(248, 210, 105) : PixelRenderer::rgb(194, 210, 207));
        }
        c.setClipRect(v.x, v.y, v.width, v.height);
        grid.section(0, touchGridScroll, sell ? Ui::BAG : Ui::Shop::CATEGORY_DAILY);
        if (!sell) grid.section(1, touchGridScroll, Ui::Shop::CATEGORY_EXPLORE);
        const auto& state = host.shopState();
        for (int i = 0; i < grid.count(); ++i) {
            const auto rect = grid.cell(i, touchGridScroll);
            if (rect.y + rect.height <= v.y || rect.y >= Ui::Page::HEIGHT) continue;
            MenuGrid::card(c, rect, gridTouch.pressed() == mode + i, i == touchSelectedIndex());
            const auto item = sell ? Game::ShopService::sellItemAt(state, i)
                : Game::ShopService::buyItemAt(i < grid.firstCount ? Game::ShopService::Category::DAILY
                    : Game::ShopService::Category::EXPLORE, state, i < grid.firstCount ? i : i - grid.firstCount);
            drawItemIcon(itemFromGameItemId(item), rect.x + rect.width / 2, rect.y + rect.height / 2, 1.2f, 0xffff);
        }
        if (grid.count() == 0) MenuGrid::label(c, v.x + v.width / 2, 134,
            sell ? Ui::Amoled::NOTHING_TO_SELL : Ui::Amoled::NOTHING, v.width - 16, 0x7bef);
        c.clearClipRect();
    } else {
        const Item item = selectedItem();
        drawItemIcon(item, 48, 88, 1.5f, 0xffff);
        PixelRenderer::text(88, 56, nameFor(item), PixelRenderer::rgb(226, 238, 233));
        char value[24];
        std::snprintf(value, sizeof(value), Ui::Shop::OWNED_FMT, ownedCountFor(item));
        PixelRenderer::text(88, 80, value, PixelRenderer::rgb(239, 196, 154));
        std::snprintf(value, sizeof(value), sell ? Ui::Shop::SELL_PRICE_FMT : Ui::Shop::PRICE_FMT,
            sell ? sellPriceFor(item) : priceFor(item));
        PixelRenderer::text(88, 104, value, PixelRenderer::rgb(248, 210, 105));
        c.setClipRect(8, 132, Ui::Page::WIDTH - 16, 52);
        drawShopWrappedText(descFor(item), 12, 134, Ui::Page::WIDTH - 24, PixelRenderer::rgb(126, 175, 175), 3);
        c.clearClipRect();
        ItemTouchUi::detailButton(ItemTouchUi::action(), sell ? Ui::Amoled::SELL : Ui::Amoled::BUY,
            gridTouch.pressed() == mode + 231);
        ItemTouchUi::detailButton(ItemTouchUi::back(), Ui::BACK, gridTouch.pressed() == mode + 230, true);
    }
    char coins[20]; std::snprintf(coins, sizeof(coins), Ui::Shop::COINS_SHORT_FMT,
        static_cast<unsigned long>(host.shopState().coins));
    TouchUi::safePageHeader(Ui::SHOP, gridTouch.pressed() == mode + 230,
        !touchDetailOpen && selectedItem() == BACK, coins);
    renderToast();
}

void ShopUi::renderToast() {
    if (!toast) return;
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(8, Ui::Page::HEIGHT - 72, Ui::Page::WIDTH - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
    MenuGrid::label(c, Ui::Page::WIDTH / 2, Ui::Page::HEIGHT - 67, toast, Ui::Page::WIDTH - 32, 0xffff);
}

}  // namespace Ui
