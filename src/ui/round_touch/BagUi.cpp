// SPDX-License-Identifier: Apache-2.0
#include "ui/common/BagUi.h"

#include <algorithm>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "game/ItemInventory.h"
#include "game/ShopService.h"
#include "presentation/PixelRenderer.h"
#include "ui/round_touch/RoundPage.h"
#include "ui/round_touch/ItemGrid.h"

namespace {
constexpr MenuGrid::Rect ITEMS{60, 60, 240, 300};
constexpr MenuGrid::Rect ACTION{88, 252, 84, 36};
constexpr MenuGrid::Rect BACK{188, 252, 84, 36};
Game::ItemId itemAt(const Game::GameState& state, uint8_t index) {
    return Game::ItemInventory::homeBagItemAt(state, index);
}
bool usable(Game::ItemId item, bool battle) {
    if (battle) return Game::foodIndexForItemId(item) >= 0 || Game::ItemInventory::usableInBattle(item);
    return Game::ItemInventory::usableFromHomeBag(item) ||
        item == Game::ItemId::MAX_REPEL || item == Game::ItemId::HONEY;
}
}

ItemTouchUi::Grid BagUi::grid(const Game::GameState& state, bool) const {
    return Ui::RoundItems::grid(ITEMS,
        Game::ItemInventory::homeBagDailyItemCount(state),
        Game::ItemInventory::homeBagExploreItemCount(state));
}

void BagUi::render(const Game::GameState& state, const Context& context) {
    const auto count = Game::ItemInventory::homeBagItemCount(state);
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, 360, 360, 0);
    if (cursor >= count) { cursor = 0; detailOpen = false; }
    const auto layoutGrid = grid(state, context.battle);
    if (!detailOpen) {
        const auto v = layoutGrid.layout.viewport;
        c.setClipRect(v.x, v.y, v.width, v.height);
        layoutGrid.section(0, gridScroll, Ui::Shop::CATEGORY_DAILY);
        layoutGrid.section(1, gridScroll, Ui::Shop::CATEGORY_EXPLORE);
        for (int i = 0; i < layoutGrid.count(); ++i) {
            const auto rect = layoutGrid.cell(i, gridScroll);
            if (rect.y + rect.height <= v.y || rect.y >= v.y + v.height) continue;
            MenuGrid::card(c, rect, touch.pressed() == i, i == cursor);
            GameAssets::drawItemIcon48(itemAt(state, i), rect.x + rect.width / 2, rect.y + rect.height / 2);
        }
        c.clearClipRect();
        if (count == 0) PixelRenderer::text(148, 194, Ui::Amoled::NOTHING, 0x7bef);
    } else {
        const auto item = itemAt(state, cursor);
        GameAssets::drawCentered(GameAssets::itemKind(item), 108, 148, 1.5f);
        MenuGrid::label(c, 216, 116, Game::ShopService::shortName(item), 144, PixelRenderer::rgb(226, 238, 233));
        char value[16]; std::snprintf(value, sizeof(value), Ui::Bag::COUNT_X_FMT, Game::ItemInventory::count(state, item));
        PixelRenderer::text(148, 142, value, PixelRenderer::rgb(239, 196, 154));
        c.setClipRect(68, 178, 224, 66);
        wrappedText(72, 180, Game::ShopService::shortDescription(item), 216, PixelRenderer::rgb(126, 175, 175));
        c.clearClipRect();
        if (usable(item, context.battle)) {
            const bool feeding = context.battle && Game::foodIndexForItemId(item) >= 0;
            ItemTouchUi::detailButton(ACTION, feeding ? Ui::Amoled::FEED : Ui::Amoled::USE, touch.pressed() == 231);
        }
        ItemTouchUi::detailButton(BACK, Ui::BACK, touch.pressed() == 230, true);
    }
    Ui::RoundPage::nativeHeader(Ui::BAG, touch.pressed() == 230);
}

void BagUi::reset() {
    cursor = confirmSource = 0;
    confirmOpen = false;
    confirmYes = true;
    gridScroll = 0;
    detailOpen = false;
    touch.cancel();
}
void BagUi::enter() { gridScroll = 0; detailOpen = false; }
void BagUi::leave() { detailOpen = false; confirmOpen = false; touch.cancel(); }
void BagUi::cancelTouch() { touch.cancel(); }
void BagUi::afterUse(const Game::GameState& state, const Context&) {
    detailOpen = false;
    const int count = Game::ItemInventory::homeBagItemCount(state);
    cursor = count ? std::min<int>(cursor, count - 1) : 0;
}
SceneUpdateResult BagUi::update(const Game::GameState& state, const Context& context, uint32_t) {
    const auto clamped = std::clamp(gridScroll, 0.0f, grid(state, context.battle).maxScroll());
    const bool changed = clamped != gridScroll;
    gridScroll = clamped;
    return changed ? SceneUpdateResult::frame() : SceneUpdateResult::parked();
}
void BagUi::renderToast(const char* value) {
    if (!value) return;
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(68, 228, 224, 26, 4, PixelRenderer::rgb(34, 39, 47));
    MenuGrid::label(c, 180, 233, value, 208, 0xffff);
}

BagUi::Action BagUi::button(const ButtonEvent& event, const Game::GameState& state, const Context& context) {
    if (event.action == BtnAction::LONG_PRESS) {
        if (event.btn == 0) return {true, Action::Kind::HOME};
        if (event.btn == 1) {
            if (detailOpen) { detailOpen = false; touch.cancel(); return {true}; }
            return {true, Action::Kind::BACK};
        }
    }
    if (event.action != BtnAction::PRESSED) return {};
    const auto count = Game::ItemInventory::homeBagItemCount(state);
    if (event.btn == 1) {
        cursor = count ? (cursor + 1) % count : 0;
        gridScroll = grid(state, context.battle).reveal(cursor, gridScroll);
        return {true};
    }
    if (event.btn != 0 || !count) return {};
    const auto item = itemAt(state, cursor);
    if (!detailOpen) {
        if (context.battle && !usable(item, true)) return {true, Action::Kind::UNUSABLE};
        if (!context.battle && item == Game::ItemId::HEART_SCALE) return {true, Action::Kind::RECALL};
        detailOpen = true;
        return {true};
    }
    if (!usable(item, context.battle)) return {true, Action::Kind::UNUSABLE};
    Action action{true};
    action.item = item;
    action.kind = (context.battle && Game::foodIndexForItemId(item) >= 0) ||
        item == Game::ItemId::MAX_REPEL || item == Game::ItemId::HONEY
        ? Action::Kind::USE_DIRECT : Action::Kind::SELECT_TARGET;
    return action;
}

int BagUi::target(int16_t x, int16_t y, const Game::GameState& state, const Context& context) const {
    if (x < 0 || x >= 360 || y < 0 || y >= 360 ||
        (2 * x - 359) * (2 * x - 359) + (2 * y - 359) * (2 * y - 359) >= 360 * 360) return -1;
    if (Ui::RoundPage::NATIVE_BACK.contains(x, y)) return 230;
    if (detailOpen) {
        if (BACK.contains(x, y)) return 230;
        if (usable(itemAt(state, cursor), context.battle) && ACTION.contains(x, y)) return 231;
        return -1;
    }
    return grid(state, context.battle).itemAt(x, y, gridScroll);
}

BagUi::Action BagUi::tap(int index, const Game::GameState& state, const Context& context) {
    if (index == 230) return button({1, BtnAction::LONG_PRESS}, state, context);
    if (index == 231) return button({0, BtnAction::PRESSED}, state, context);
    if (index < 0 || index >= Game::ItemInventory::homeBagItemCount(state) || detailOpen) return {};
    cursor = index;
    return button({0, BtnAction::PRESSED}, state, context);
}

BagUi::Action BagUi::onTouch(const Platform::IInputDevice::TouchEvent& event,
                            const Game::GameState& state, const Context& context) {
    const auto layout = grid(state, context.battle);
    const auto result = touch.update(event, target(event.x, event.y, state, context), gridScroll,
        layout.maxScroll(), layout.layout.viewport.contains(event.x, event.y) && !detailOpen);
    auto action = result.tap >= 0 ? tap(result.tap, state, context) : Action{};
    action.changed = action.changed || result.changed;
    return action;
}
