#include "ui/common/BagUi.h"
#include "ui/PageProfile.h"

#include <algorithm>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

ItemTouchUi::Grid BagUi::grid(const Game::GameState& state, bool battle) const {
    Row rows[Features::Bag::ITEM_COUNT];
    const int count = Features::Bag::collect(state, battle, rows, Features::Bag::ITEM_COUNT) - 1;
    auto daily = [&](int i) { return rows[i].source == 3 ||
        (rows[i].source >= Features::Bag::SOURCE_FOOD_BASE && rows[i].source < Features::Bag::SOURCE_HEAL_BASE); };
    int first = 0;
    for (int i = 0; i < count; ++i) if (daily(i)) ++first;
    ItemTouchUi::Grid grid(ItemTouchUi::body(), first, count - first);
    int a = 0, b = first;
    for (int i = 0; i < count; ++i) grid.targets[daily(i) ? a++ : b++] = i;
    return grid;
}

void BagUi::render(const Game::GameState& state, const Context& context) {
    Row rows[Features::Bag::ITEM_COUNT];
    const auto count = Features::Bag::collect(state, context.battle, rows, Features::Bag::ITEM_COUNT);
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Ui::Page::WIDTH, Ui::Page::HEIGHT, 0);
    if (cursor >= count) cursor = count - 1;
    const auto layoutGrid = grid(state, context.battle);
    if (!detailOpen) {
        const auto v = layoutGrid.layout.viewport;
        c.setClipRect(v.x, v.y, v.width, v.height);
        layoutGrid.section(0, gridScroll, Ui::Shop::CATEGORY_DAILY);
        layoutGrid.section(1, gridScroll, Ui::Shop::CATEGORY_EXPLORE);
        for (int i = 0; i < layoutGrid.count(); ++i) {
            const int target = layoutGrid.targets[i];
            const auto rect = layoutGrid.cell(i, gridScroll);
            if (rect.y + rect.height <= v.y || rect.y >= Ui::Page::HEIGHT) continue;
            MenuGrid::card(c, rect, touch.pressed() == target, target == cursor);
            GameAssets::drawCentered(GameAssets::itemKind(Features::Bag::itemId(rows[target].source)),
                rect.x + rect.width / 2, rect.y + rect.height / 2, 1.2f);
        }
        c.clearClipRect();
        if (layoutGrid.count() == 0) PixelRenderer::text(112, 134, Ui::Amoled::NOTHING, 0x7bef);
    } else {
        const auto& row = rows[cursor];
        if (row.source < Features::Bag::ITEM_COUNT - 1) {
            GameAssets::drawCentered(GameAssets::itemKind(Features::Bag::itemId(row.source)), 48, 88, 1.5f);
            PixelRenderer::text(88, 56, Ui::Bag::NAMES[row.source], PixelRenderer::rgb(226, 238, 233));
            char value[16]; std::snprintf(value, sizeof(value), Ui::Bag::COUNT_X_FMT, row.count);
            PixelRenderer::text(88, 82, value, PixelRenderer::rgb(239, 196, 154));
            c.setClipRect(8, 118, Ui::Page::WIDTH - 16, 66);
            int y = 120;
            for (int i = 0; i < 3; ++i)
                y += wrappedText(12, y, Ui::Bag::DESCS[row.source][i], Ui::Page::WIDTH - 24,
                    PixelRenderer::rgb(126, 175, 175)) * 16;
            c.clearClipRect();
        }
        ItemTouchUi::detailButton(ItemTouchUi::action(), Ui::Amoled::USE, touch.pressed() == 231);
        ItemTouchUi::detailButton(ItemTouchUi::back(), Ui::BACK, touch.pressed() == 230, true);
    }
    TouchUi::safePageHeader(Ui::BAG, touch.pressed() == 230,
        !detailOpen && cursor == count - 1);
    if (confirmOpen) renderConfirm(state, context);
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
void BagUi::afterUse(const Game::GameState& state, const Context& context) {
    if (Features::Bag::sourceAt(state, context.battle, cursor) != confirmSource) detailOpen = false;
}
SceneUpdateResult BagUi::update(const Game::GameState& state, const Context& context, uint32_t) {
    gridScroll = std::clamp(gridScroll, 0.0f, grid(state, context.battle).maxScroll());
    return SceneUpdateResult::parked();
}
void BagUi::renderToast(const char* value) {
    if (!value) return;
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(8, Ui::Page::HEIGHT - 72, Ui::Page::WIDTH - 16, 26, 4, PixelRenderer::rgb(34, 39, 47));
    MenuGrid::label(c, Ui::Page::WIDTH / 2, Ui::Page::HEIGHT - 67, value, Ui::Page::WIDTH - 32, 0xffff);
}

BagUi::Action BagUi::button(const ButtonEvent& event, const Game::GameState& state, const Context& context) {
    if (event.action == BtnAction::LONG_PRESS) {
        if (event.btn == 0) return {true, Action::Kind::HOME};
        if (event.btn == 1) {
            if (!confirmOpen && detailOpen) { detailOpen = false; touch.cancel(); return {true}; }
            if (confirmOpen) { confirmOpen = false; return {true, Action::Kind::NONE, Features::Bag::SOURCE_BACK, true}; }
            return {true, Action::Kind::BACK, Features::Bag::SOURCE_BACK, true};
        }
    }
    if (event.action != BtnAction::PRESSED) return {};
    if (event.btn == 1) {
        if (confirmOpen) confirmYes = !confirmYes;
        else {
            cursor = (cursor + 1) % Features::Bag::visibleCount(state, context.battle);
            gridScroll = grid(state, context.battle).reveal(cursor, gridScroll);
            if (Features::Bag::sourceAt(state, context.battle, cursor) == Features::Bag::SOURCE_BACK) detailOpen = false;
        }
        return {true};
    }
    if (event.btn == 0) {
        if (!confirmOpen && !detailOpen && Features::Bag::sourceAt(state, context.battle, cursor) != Features::Bag::SOURCE_BACK) {
            detailOpen = true;
            return {true};
        }
        const bool closingConfirmation = confirmOpen;
        const auto action = activate(state, context);
        if (closingConfirmation && action.kind != Action::Kind::USE) afterUse(state, context);
        return action;
    }
    return {};
}

int BagUi::target(int16_t x, int16_t y, const Game::GameState& state, const Context& context) const {
    if (x < 0 || x >= Ui::Page::WIDTH || y < 0 || y >= Ui::Page::HEIGHT) return -1;
    const auto header = TouchUi::safeHeaderBounds();
    if (y < header.y + header.height && !header.contains(x, y)) return -1;
    if (confirmOpen) {
        const int shift = (Ui::Page::WIDTH - 240) / 2;
        if (y < 75 || y >= 104 || x < 65 + shift || x >= 205 + shift) return -1;
        return 200 + (x >= 139 + shift);
    }
    if (TouchUi::safeBack().contains(x, y)) return 230;
    if (detailOpen) {
        if (ItemTouchUi::back().contains(x, y)) return 230;
        if (ItemTouchUi::action().contains(x, y)) return 231;
        return -1;
    }
    return grid(state, context.battle).itemAt(x, y, gridScroll);
}

BagUi::Action BagUi::tap(int index, const Game::GameState& state, const Context& context) {
    if (index == 230) return button({1, BtnAction::LONG_PRESS}, state, context);
    if (index == 231) return button({0, BtnAction::PRESSED}, state, context);
    if (index >= 0 && index < 128 && !confirmOpen) {
        cursor = index;
        if (index >= Features::Bag::visibleCount(state, context.battle)) return {};
        if (Features::Bag::sourceAt(state, context.battle, index) == Features::Bag::SOURCE_BACK)
            return button({0, BtnAction::PRESSED}, state, context);
        detailOpen = true;
        return {true};
    }
    if (index >= 200 && confirmOpen) {
        confirmYes = index == 200;
        auto action = button({0, BtnAction::PRESSED}, state, context);
        action.navigationTutorial = true;
        return action;
    }
    return {};
}

BagUi::Action BagUi::onTouch(const Platform::IInputDevice::TouchEvent& event,
                            const Game::GameState& state, const Context& context) {
    const auto layout = grid(state, context.battle);
    const auto result = touch.update(event, target(event.x, event.y, state, context), gridScroll,
        layout.maxScroll(), layout.layout.viewport.contains(event.x, event.y) && !detailOpen && !confirmOpen);
    auto action = result.tap >= 0 ? tap(result.tap, state, context) : Action{};
    action.changed = action.changed || result.changed;
    return action;
}
