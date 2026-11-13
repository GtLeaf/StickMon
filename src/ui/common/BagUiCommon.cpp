#include "ui/common/BagUi.h"
#include "ui/PageProfile.h"

#include <cstdio>
#include "core/UiStrings.h"
#include "game/Species.h"
#include "hardware/Hal.h"
#include "presentation/PixelRenderer.h"

int BagUi::textWidth(const char* value) {
    int width = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p < 0x80) {
            width += *p == ' ' ? 5 : 8;
            ++p;
        } else if ((*p & 0xE0) == 0xC0) {
            width += 16;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            width += 16;
            p += 3;
        } else {
            width += 8;
            ++p;
        }
    }
    return width;
}

int BagUi::wrappedText(int x, int y, const char* value, int maxWidth, uint16_t color) {
    if (!value || !*value || maxWidth <= 0) return 0;

    char line[96];
    int lineBytes = 0;
    int lineWidth = 0;
    int lines = 0;
    auto flushLine = [&]() {
        if (lineBytes == 0) return;
        line[lineBytes] = '\0';
        PixelRenderer::text(x, y + lines * 16, line, color, 1);
        ++lines;
        lineBytes = 0;
        lineWidth = 0;
    };

    const uint8_t* p = reinterpret_cast<const uint8_t*>(value);
    while (*p) {
        if (*p == '\n') {
            flushLine();
            ++p;
            continue;
        }

        int bytes = 1;
        int glyphWidth = *p == ' ' ? 5 : 8;
        if ((*p & 0xE0) == 0xC0) {
            bytes = 2;
            glyphWidth = 16;
        } else if ((*p & 0xF0) == 0xE0) {
            bytes = 3;
            glyphWidth = 16;
        } else if ((*p & 0xF8) == 0xF0) {
            bytes = 4;
            glyphWidth = 16;
        }

        if (lineBytes > 0 &&
            (lineWidth + glyphWidth > maxWidth || lineBytes + bytes >= (int)sizeof(line))) {
            flushLine();
        }
        for (int index = 0; index < bytes && p[index]; ++index) {
            line[lineBytes++] = static_cast<char>(p[index]);
        }
        lineWidth += glyphWidth;
        p += bytes;
    }
    flushLine();
    return lines;
}

static_assert(sizeof(Ui::Bag::NAMES) / sizeof(Ui::Bag::NAMES[0]) == Features::Bag::ITEM_COUNT, "bag labels must match item definitions");
static_assert(sizeof(Ui::Bag::DESCS) / sizeof(Ui::Bag::DESCS[0]) == Features::Bag::ITEM_COUNT, "bag descriptions must match item definitions");

void BagUi::renderConfirm(const Game::GameState& state, const Context& context) {
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = (Ui::Page::WIDTH - 190) / 2;
    static constexpr int POP_Y = 32;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 72;
    c.fillRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(241, 242, 232));

    char line[48];
    bool isThrow = confirmSource >= Features::Bag::SOURCE_FOOD_BASE &&
                   confirmSource < Features::Bag::SOURCE_FOOD_BASE + Game::ROOM_FOOD_COUNT;
    const char* format = isThrow
        ? Ui::Bag::THROW_CONFIRM_FMT : Ui::Bag::USE_CONFIRM_FMT;
    if (confirmSource == Features::Bag::SOURCE_EXTRA_BASE + 7 ||
        confirmSource == Features::Bag::SOURCE_EXTRA_BASE + 8) {
        snprintf(line, sizeof(line), Ui::Bag::USE_ITEM_CONFIRM_FMT,
                 Ui::Bag::NAMES[confirmSource]);
    } else {
        uint8_t targetSlot = Features::Bag::targetSlot(confirmSource, context, state);
        const auto* species = findSpecies(state.team[targetSlot].speciesId);
        const char* target = isThrow && context.targetName
            ? context.targetName
            : (species ? *species : starterSpecies()).name;
        snprintf(line, sizeof(line), format, target);
    }
    int lineW = textWidth(line);
    int lineX = POP_X + (POP_W - lineW) / 2;
    if (lineX < POP_X + 6) lineX = POP_X + 6;
    PixelRenderer::text(lineX, POP_Y + 17, line, PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t yesColor = confirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !confirmYes ? PixelRenderer::rgb(255, 216, 72) : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 60, POP_Y + 49, Ui::Bag::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 130, POP_Y + 49, Ui::Bag::NO, noColor, 1);
}


BagUi::Action BagUi::activate(const Game::GameState& state, const Context& context) {
    using namespace Features::Bag;
    if (confirmOpen) {
        const Action action{true, confirmYes ? Action::Kind::USE : Action::Kind::NONE, confirmSource};
        confirmOpen = false;
        return action;
    }
    const auto source = sourceAt(state, context.battle, cursor);
    switch (select(source, context.battle)) {
    case Selection::BACK: return {true, Action::Kind::BACK};
    case Selection::RECALL: return {true, Action::Kind::RECALL, source};
    case Selection::CONFIRM:
        confirmSource = source;
        confirmYes = true;
        confirmOpen = true;
        break;
    default: break;
    }
    return {true};
}

const char* BagUi::notice(const Features::Bag::Outcome& outcome, char* buffer, size_t capacity) {
    using Features::Bag::Notice;
    switch (outcome.notice) {
    case Notice::USED_ITEM:
        for (uint8_t source = Features::Bag::SOURCE_MARKET_BASE; source < Features::Bag::SOURCE_BACK; ++source) {
            if (Features::Bag::itemId(source) != outcome.item) continue;
            std::snprintf(buffer, capacity, Ui::Bag::USED_ITEM_FMT, Ui::Bag::NAMES[source]);
            return buffer;
        }
        return nullptr;
    case Notice::NOTHING_ELSE_NEEDED: return Ui::Bag::NOTHING_ELSE_NEEDED;
    case Notice::FAINTED_CANNOT_HEAL: return Ui::Bag::FAINTED_CANNOT_HEAL;
    case Notice::USED_POTION: return Ui::Bag::USED_POTION;
    case Notice::HP_FULL: return Ui::Bag::HP_FULL;
    case Notice::USED_SUPER_POTION: return Ui::Bag::USED_SUPER_POTION;
    case Notice::USED_ANTIDOTE: return Ui::Bag::USED_ANTIDOTE;
    case Notice::STATUS_NORMAL: return Ui::Bag::STATUS_NORMAL;
    case Notice::USED_PARALYZE_HEAL: return Ui::Bag::USED_PARALYZE_HEAL;
    case Notice::USED_AWAKENING: return Ui::Bag::USED_AWAKENING;
    case Notice::USED_BURN_HEAL: return Ui::Bag::USED_BURN_HEAL;
    case Notice::USED_ICE_HEAL: return Ui::Bag::USED_ICE_HEAL;
    case Notice::USED_MAX_POTION: return Ui::Bag::USED_MAX_POTION;
    case Notice::USED_FULL_RESTORE: return Ui::Bag::USED_FULL_RESTORE;
    case Notice::FULL_RESTORE_NOT_NEEDED: return Ui::Bag::FULL_RESTORE_NOT_NEEDED;
    case Notice::USED_FULL_HEAL: return Ui::Bag::USED_FULL_HEAL;
    case Notice::STONE_EVOLUTION_BEGIN: return Ui::Bag::STONE_EVOLUTION_BEGIN;
    case Notice::STONE_NO_EFFECT: return Ui::Bag::STONE_NO_EFFECT;
    case Notice::USED_REVIVE: return Ui::Bag::USED_REVIVE;
    case Notice::REVIVE_NO_FAINTED: return Ui::Bag::REVIVE_NO_FAINTED;
    case Notice::USED_MAX_REPEL: return Ui::Bag::USED_MAX_REPEL;
    case Notice::MAX_REPEL_ACTIVE: return Ui::Bag::MAX_REPEL_ACTIVE;
    case Notice::USED_HONEY: return Ui::Bag::USED_HONEY;
    case Notice::HONEY_ACTIVE: return Ui::Bag::HONEY_ACTIVE;
    case Notice::FOOD_NO_STOCK: return Ui::Room::FOOD_NO_STOCK;
    case Notice::THREW_FOOD:
        std::snprintf(buffer, capacity, Ui::Bag::THREW_FOOD_FMT, Ui::Room::FOOD_NAMES[outcome.foodIndex]);
        return buffer;
    default: return nullptr;
    }
}
