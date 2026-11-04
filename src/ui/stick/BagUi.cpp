#include "ui/common/BagUi.h"
#include <algorithm>
#include <cstdio>
#include "assets/GameAssets.h"
#include "core/UiMotion.h"
#include "core/UiStrings.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TouchUi.h"

static void renderSplitList(const Features::Bag::Row* rows, uint8_t count, uint8_t cursor, float scroll) {
    if (!rows || count == 0) return;
    auto& c = PixelRenderer::canvas();
    static constexpr int LEFT_W = 78;
    static constexpr int LIST_Y_START = 6;
    static constexpr int LIST_BOTTOM = TouchUi::CONTENT_H - 6;
    static constexpr int VISIBLE_H = LIST_BOTTOM - LIST_Y_START;
    static constexpr int ROW_H = 22;
    static constexpr int TEXT_X = 12;

    c.drawFastVLine(LEFT_W, 6, TouchUi::CONTENT_H - 12, 0x7BEF);

    c.setClipRect(0, LIST_Y_START, LEFT_W - 1, VISIBLE_H);
    for (uint8_t i = 0; i < count; ++i) {
        int y = LIST_Y_START + i * ROW_H - (int)scroll;
        if (y + ROW_H < LIST_Y_START || y > LIST_BOTTOM) continue;
        bool selected = i == cursor;
        uint16_t color = selected ? 0xFFE0 : 0xFFFF;
        int textY = y + (ROW_H - 16) / 2;
        PixelRenderer::text(TEXT_X, textY, Ui::Bag::NAMES[rows[i].source], color, 1);
    }
    c.clearClipRect();
}

static void renderBagDetail(const Features::Bag::Row& row, float descriptionScroll) {
    static constexpr int LEFT_W = 78;
    static constexpr int RIGHT_X = LEFT_W + 8;
    static constexpr int ICON_W = 58;
    static constexpr int ICON_H = 36;
    static constexpr float ICON_SCALE = 1.0f;
    int iconX = RIGHT_X + 5;
    int iconY = 8;

    if (row.source < Features::Bag::ITEM_COUNT - 1) {
        int textX = iconX + ICON_W + 5;
        int nameY = iconY + 2;
        int storageY = nameY + 16;

        Game::ItemId item = Features::Bag::itemId(row.source);
        GameAssets::drawCentered(GameAssets::itemKind(item), iconX + ICON_W / 2,
                                 iconY + ICON_H / 2, ICON_SCALE);

        PixelRenderer::text(textX, nameY, Ui::Bag::NAMES[row.source], 0xFFFF, 1);
        char buf[16];
        snprintf(buf, sizeof(buf), Ui::Bag::COUNT_X_FMT, row.count);
        PixelRenderer::text(textX, storageY, buf,
                            row.count > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF,
                            1);

        int descX = RIGHT_X + 2;
        int descY = iconY + ICON_H + 12;
        int descW = Hal::DISPLAY_W - descX - 2;
        const char* descLines[3] = {
            Ui::Bag::DESCS[row.source][0],
            Ui::Bag::DESCS[row.source][1],
            Ui::Bag::DESCS[row.source][2],
        };
        auto& c = PixelRenderer::canvas();
        c.setClipRect(descX, descY - 1, descW, 3 * 16 + 2);
        for (int i = 0; i < 3; ++i)
            PixelRenderer::text(descX - int(descriptionScroll), descY + i * 16, descLines[i],
                row.count > 0 ? PixelRenderer::rgb(255, 218, 178) : 0x7BEF, 1);
        c.clearClipRect();
    } else {
        PixelRenderer::text(RIGHT_X + 20, 80, Ui::Bag::DESCS[row.source][0], 0x7BEF, 1);
    }
}


void BagUi::reset() {
    cursor = confirmSource = 0;
    confirmOpen = false;
    confirmYes = true;
    scroll = descriptionScroll = 0;
    descriptionKey = -1;
    descriptionLastMs = 0;
}
void BagUi::enter() { scroll = 0; }
void BagUi::leave() { confirmOpen = false; }
void BagUi::cancelTouch() {}
void BagUi::afterUse(const Game::GameState&, const Context&) {}

SceneUpdateResult BagUi::update(const Game::GameState& state, const Context& context, uint32_t nowMs) {
    RenderDemand demand;
    Row rows[Features::Bag::ITEM_COUNT];
    const auto count = Features::Bag::collect(state, context.battle, rows, Features::Bag::ITEM_COUNT);
    constexpr int visibleHeight = TouchUi::CONTENT_H - 12;
    constexpr int rowHeight = 22;
    const int maximum = std::max(0, int(count) * rowHeight - visibleHeight);
    const int target = std::clamp(int(cursor) * rowHeight - visibleHeight / 2 + rowHeight / 2, 0, maximum);
    const auto step = UiMotion::lerp(scroll, float(target), 0.25f, 0.5f);
    demand.changed(step.changed);
    demand.animate(step.active);
    if (cursor < count && rows[cursor].source < Features::Bag::SOURCE_BACK) {
        const auto source = rows[cursor].source;
        int width = 0;
        for (auto* line : Ui::Bag::DESCS[source]) width = std::max(width, textWidth(line));
        const int maximumDescription = width > Hal::DISPLAY_W - 88 ? width - (Hal::DISPLAY_W - 88) + 4 : 0;
        const float before = descriptionScroll;
        const int oldKey = descriptionKey;
        if (descriptionKey != 0x200 + source) {
            descriptionKey = 0x200 + source;
            descriptionScroll = 0;
            descriptionLastMs = nowMs;
        }
        if (maximumDescription <= 0) {
            descriptionScroll = 0;
            descriptionLastMs = nowMs;
        } else {
            const auto elapsed = std::min<uint32_t>(nowMs - descriptionLastMs, 120);
            descriptionLastMs = nowMs;
            descriptionScroll += 28.0f * (float(elapsed) / 1000.0f);
            if (descriptionScroll > maximumDescription + 18) descriptionScroll = 0;
        }
        demand.changed(oldKey != descriptionKey || before != descriptionScroll);
        demand.animate(maximumDescription > 0);
    }
    return demand.result();
}

void BagUi::render(const Game::GameState& state, const Context& context) {
    Row rows[Features::Bag::ITEM_COUNT];
    const auto count = Features::Bag::collect(state, context.battle, rows, Features::Bag::ITEM_COUNT);
    if (cursor >= count) cursor = count - 1;
    auto& c = PixelRenderer::canvas();
    c.fillRect(0, 0, Hal::DISPLAY_W, TouchUi::CONTENT_H, PixelRenderer::rgb(10, 14, 20));
    renderSplitList(rows, count, cursor, scroll);
    renderBagDetail(rows[cursor], descriptionScroll);
    if (confirmOpen) renderConfirm(state, context);
}

void BagUi::renderToast(const char* value) {
    if (!value) return;
    auto& c = PixelRenderer::canvas();
    c.fillRect(70, 108, 100, 20, PixelRenderer::rgb(34, 39, 47));
    PixelRenderer::text(78, 110, value, PixelRenderer::rgb(255, 255, 255), 1);
}

BagUi::Action BagUi::button(const ButtonEvent& event, const Game::GameState& state, const Context& context) {
    if (event.action == BtnAction::LONG_PRESS) {
        if (event.btn == 0) return {true, Action::Kind::HOME};
        if (event.btn == 1) {
            if (confirmOpen) { confirmOpen = false; return {true, Action::Kind::NONE, Features::Bag::SOURCE_BACK, true}; }
            return {true, Action::Kind::BACK, Features::Bag::SOURCE_BACK, true};
        }
    }
    if (event.action != BtnAction::PRESSED) return {};
    if (event.btn == 1) {
        if (confirmOpen) confirmYes = !confirmYes;
        else {
            cursor = (cursor + 1) % Features::Bag::visibleCount(state, context.battle);
            if (!cursor) scroll = 0;
        }
        return {true};
    }
    return event.btn == 0 ? activate(state, context) : Action{};
}

int BagUi::target(int16_t x, int16_t y, const Game::GameState& state, const Context& context) const {
    if (x < 0 || x >= Hal::DISPLAY_W || y < 0 || y >= TouchUi::CONTENT_H) return -1;
    if (confirmOpen) {
        const int shift = (Hal::DISPLAY_W - 240) / 2;
        if (y < 75 || y >= 104 || x < 65 + shift || x >= 205 + shift) return -1;
        return 200 + (x >= 139 + shift);
    }
    const int index = (y - 6 + int(scroll)) / 22;
    return x < 78 && y >= 6 && index >= 0 && index < Features::Bag::visibleCount(state, context.battle) ? index : -1;
}

BagUi::Action BagUi::tap(int index, const Game::GameState& state, const Context& context) {
    if (index >= 200) {
        if (!confirmOpen) return {};
        confirmYes = index == 200;
    } else if (index >= 0 && index < Features::Bag::visibleCount(state, context.battle)) cursor = index;
    else return {};
    auto result = button({0, BtnAction::PRESSED}, state, context);
    result.navigationTutorial = true;
    return result;
}
BagUi::Action BagUi::onTouch(const Platform::IInputDevice::TouchEvent&, const Game::GameState&, const Context&) { return {}; }
