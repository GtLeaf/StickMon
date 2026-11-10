// SPDX-License-Identifier: Apache-2.0
#include "ui/round_touch/ProgressionUi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "platform/api/FlashStorage.h"
#include "core/UiStrings.h"
#include "game/MoveManagementService.h"
#include "presentation/PixelRenderer.h"
#include "ui/SceneSurface.h"
#include "ui/common/MovesUiBase.h"
#include "ui/common/StatusUiContent.h"

namespace Ui {
namespace {
constexpr int SIZE = 360;
constexpr int HEADER_H = 52;
constexpr int CARD_X = 52;
constexpr int CARD_W = 256;
constexpr int CARD_H = 44;
constexpr int CARD_GAP = 6;
constexpr int TEXT_X = 64;
constexpr int TEXT_W = 228;
constexpr int LINE_H = 16;
constexpr uint8_t NEW_SLOT = Game::MOVE_SLOT_COUNT;

uint16_t background() { return PixelRenderer::rgb(10, 14, 20); }
uint16_t headerColor() { return PixelRenderer::rgb(7, 12, 17); }
uint16_t cardColor() { return PixelRenderer::rgb(24, 34, 42); }
uint16_t selectedColor() { return PixelRenderer::rgb(42, 61, 68); }
uint16_t textColor() { return PixelRenderer::rgb(226, 238, 233); }
uint16_t mutedColor() { return PixelRenderer::rgb(91, 104, 104); }
uint16_t accentColor() { return PixelRenderer::rgb(255, 216, 72); }
uint16_t greenColor() { return PixelRenderer::rgb(115, 226, 183); }
uint16_t hintColor() { return PixelRenderer::rgb(135, 214, 238); }
uint16_t descriptionColor() { return PixelRenderer::rgb(178, 194, 190); }

int textWidth(const char* value) { return value ? MovesDrawing::textPixelWidth(value) : 0; }

void centered(const char* value, int y, uint16_t color) {
    if (value) PixelRenderer::text((SIZE - textWidth(value)) / 2, y, value, color, 1);
}

void header() {
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRect(0, 0, SIZE, SIZE, 0);
    canvas.fillCircle(SIZE / 2, SIZE / 2, SIZE / 2, background());
    canvas.fillRect(0, 0, SIZE, HEADER_H, headerColor());
    // Two CJK glyphs at the native 20px header size.
    PixelRenderer::textSmooth((SIZE - 40) / 2, 16, Ui::Team::ACTION_MOVES, 0xffff, 20);
}

void typeBadge(int x, int y, TypeId type) {
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRoundRect(x, y, 40, 22, 11, MovesDrawing::typeColor(type));
    const char* name = typeName(type);
    PixelRenderer::text(x + (40 - textWidth(name)) / 2, y + 3, name, PixelRenderer::rgb(10, 14, 20), 1);
}

void powerLine(const MoveInfo& move, int x, int y) {
    char power[8], accuracy[8], line[48];
    if (move.power > 0) std::snprintf(power, sizeof(power), "%u", move.power);
    else std::snprintf(power, sizeof(power), "--");
    if (move.accuracy > 0) std::snprintf(accuracy, sizeof(accuracy), "%u", move.accuracy);
    else std::snprintf(accuracy, sizeof(accuracy), "--");
    std::snprintf(line, sizeof(line), Ui::Team::MOVE_POWER_ACCURACY_FMT, power, accuracy);
    PixelRenderer::text(x, y, line, textColor(), 1);
}

const MoveInfo* cardMove(const MoveLearnFrame& frame, uint8_t slot) {
    if (slot == NEW_SLOT) return findMove(frame.moveId);
    if (!frame.monster || !frame.species) return nullptr;
    return Game::MoveManagementService::learnedMove(*frame.species, *frame.monster, slot);
}

int selectedCard(const MoveLearnState& state) {
    if (state.stage != MoveLearnStage::REPLACEMENT) return -1;
    for (int card = 0; card < ProgressionUi::CARD_COUNT; ++card)
        if (ProgressionUi::cardSlot(card) == state.replacementSlot) return card;
    return -1;
}

void actionPill(const char* label, uint16_t color, bool enabled) {
    auto& canvas = PixelRenderer::canvas();
    const auto rect = ProgressionUi::actionButton();
    const int radius = rect.height / 2;
    // Filled rings keep the 2px border even through the rounded ends.
    canvas.fillRoundRect(rect.x, rect.y, rect.width, rect.height, radius, color);
    canvas.fillRoundRect(rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4, radius - 2,
                         enabled ? selectedColor() : cardColor());
    centered(label, rect.y + (rect.height - 16) / 2, color);
}
}  // namespace

ProgressionLayout ProgressionUi::layout() const {
    return {Ui::SceneSurface::width(), Ui::SceneSurface::height(), true, true};
}

uint8_t ProgressionUi::cardSlot(int card) {
    return card == 0 ? NEW_SLOT : static_cast<uint8_t>(card);
}

MenuGrid::Rect ProgressionUi::listViewport() { return {CARD_X, 80, CARD_W, 218}; }
MenuGrid::Rect ProgressionUi::actionButton() { return {110, 302, 140, 38}; }

void ProgressionUi::syncFrame(const MoveLearnFrame& frame) {
    if (frame.monster == shownMonster && frame.moveId == shownMove) return;
    shownMonster = frame.monster;
    shownMove = frame.moveId;
    scroll = 0.0f;
    revealSelection = false;
    listTouch.cancel();
}

int ProgressionUi::detailHeight(const MoveLearnState& state, const MoveLearnFrame& frame) const {
    const int card = selectedCard(state);
    if (card < 0) return 0;
    const MoveInfo* move = cardMove(frame, cardSlot(card));
    if (!move) return 0;
    const int lines = std::max(1, StatusContent::wrappedTextLineCount(move->description, TEXT_W));
    const int proficiency = cardSlot(card) == NEW_SLOT ? 0 : LINE_H + 6;
    return 8 + LINE_H + 6 + proficiency + lines * LINE_H + 10;
}

int ProgressionUi::contentHeight(const MoveLearnState& state, const MoveLearnFrame& frame) const {
    return CARD_COUNT * CARD_H + (CARD_COUNT - 1) * CARD_GAP + detailHeight(state, frame);
}

float ProgressionUi::maxListScroll(const MoveLearnState& state, const MoveLearnFrame& frame) const {
    return static_cast<float>(std::max(0, contentHeight(state, frame) - listViewport().height));
}

MenuGrid::Rect ProgressionUi::cardRect(const MoveLearnState& state, const MoveLearnFrame& frame,
                                       int card) const {
    const int selected = selectedCard(state);
    const int push = selected >= 0 && card > selected ? detailHeight(state, frame) : 0;
    return {CARD_X, listViewport().y - static_cast<int>(scroll) + card * (CARD_H + CARD_GAP) + push,
            CARD_W, CARD_H};
}

int ProgressionUi::moveLearnTarget(const MoveLearnState& state, const MoveLearnFrame& frame,
                                   int x, int y) const {
    if (actionButton().contains(x, y)) return ACTION_TARGET;
    if (!frame.needsReplacement || !listViewport().contains(x, y)) return -1;
    const int selected = selectedCard(state);
    for (int card = 0; card < CARD_COUNT; ++card) {
        auto rect = cardRect(state, frame, card);
        // The open detail belongs to its card, so tapping it also collapses.
        if (card == selected) rect.height += detailHeight(state, frame);
        if (rect.contains(x, y)) return card;
    }
    return -1;
}

void ProgressionUi::select(MoveLearnState& state, uint8_t slot) {
    state.stage = MoveLearnStage::REPLACEMENT;
    state.replacementSlot = slot;
    state.confirmOpen = false;
    state.confirmYes = false;
    revealSelection = true;
}

MoveLearnAction ProgressionUi::handleMoveLearnInput(MoveLearnState& state, uint8_t button,
                                                    const MoveLearnFrame& frame) {
    if (!frame.pending) return MoveLearnAction::COMPLETE;
    syncFrame(frame);
    if (!frame.needsReplacement) {
        // A free slot is learned directly; the card only reports the result.
        if (button != 0) return MoveLearnAction::NONE;
        state = {};
        return ProgressionUiBase::handleMoveLearnInput(state, 0, frame);
    }
    if (button == 1) {
        const int card = selectedCard(state);
        select(state, cardSlot(card < 0 ? 0 : (card + 1) % CARD_COUNT));
        return MoveLearnAction::NONE;
    }
    if (button != 0 || selectedCard(state) < 0) return MoveLearnAction::NONE;
    // The action itself is the confirmation, as on AMOLED.
    state.confirmOpen = true;
    state.confirmYes = true;
    return ProgressionUiBase::handleMoveLearnInput(state, 0, frame);
}

void ProgressionUi::cancelMoveLearnTouch() {
    learnTap.cancel();
    listTouch.cancel();
    touchMonster = nullptr;
    touchMove = 0;
    touchState = 0;
    evolutionHold = evolutionTapArmed = false;
}

MoveLearnAction ProgressionUi::handleMoveLearnTouch(MoveLearnState& state,
        const TouchTap::Event& event, const MoveLearnFrame& frame) {
    if (!frame.pending) { cancelMoveLearnTouch(); return MoveLearnAction::NONE; }
    syncFrame(frame);
    const uint16_t key = 1 + static_cast<uint16_t>(state.stage) + state.replacementSlot * 4 +
        (frame.needsReplacement ? 64 : 0);
    if (event.type == TouchTap::Type::DOWN) {
        cancelMoveLearnTouch();
        touchMonster = frame.monster;
        touchMove = frame.moveId;
        touchState = key;
    } else if (touchMonster != frame.monster || touchMove != frame.moveId || touchState != key) {
        cancelMoveLearnTouch();
        return MoveLearnAction::NONE;
    }
    const int target = moveLearnTarget(state, frame, event.x, event.y);
    const auto result = listTouch.update(event, target, scroll, maxListScroll(state, frame),
        frame.needsReplacement && listViewport().contains(event.x, event.y));
    if (result.tap < 0) return MoveLearnAction::NONE;
    cancelMoveLearnTouch();
    if (result.tap == ACTION_TARGET) return handleMoveLearnInput(state, 0, frame);
    const uint8_t slot = cardSlot(result.tap);
    if (selectedCard(state) == result.tap) {
        // Tapping the open card folds it again and disables the action.
        state.stage = MoveLearnStage::PROMPT;
        state.confirmOpen = state.confirmYes = false;
        scroll = std::min(scroll, maxListScroll(state, frame));
    } else {
        select(state, slot);
    }
    return MoveLearnAction::NONE;
}

bool ProgressionUi::handleLevelUpTouch(const TouchTap::Event& event) {
    if (!layout().bottomLevelUp) return false;
    if (event.type == TouchTap::Type::DOWN) {
        cancelMoveLearnTouch();
        touchState = 2048;
    } else if (touchState != 2048) {
        cancelMoveLearnTouch();
        return false;
    }
    const int target = layout().levelUpPanel().contains(event.x, event.y) ? 0 : -1;
    const bool tapped = learnTap.update(event, target) == 0;
    if (tapped) cancelMoveLearnTouch();
    return tapped;
}

bool ProgressionUi::handleMoveReplacementTouch(const TouchTap::Event& event) {
    if (event.type == TouchTap::Type::DOWN) {
        cancelMoveLearnTouch();
        touchState = 1024;
    } else if (touchState != 1024) {
        cancelMoveLearnTouch();
        return false;
    }
    const int target = actionButton().contains(event.x, event.y) ? 0 : -1;
    const bool tapped = learnTap.update(event, target) == 0;
    if (tapped) cancelMoveLearnTouch();
    return tapped;
}

void ProgressionUi::renderMoveLearn(const MoveLearnState& state, const MoveLearnFrame& frame) {
    syncFrame(frame);
    if (frame.needsReplacement) renderReplacementList(state, frame);
    else renderLearnedCard(frame);
}

void ProgressionUi::renderReplacementList(const MoveLearnState& state, const MoveLearnFrame& frame) {
    auto& canvas = PixelRenderer::canvas();
    header();
    centered(Ui::Explore::LEARN_CHOOSE_REPLACEMENT, 58, hintColor());

    const int selected = selectedCard(state);
    const int detail = detailHeight(state, frame);
    const auto view = listViewport();
    if (revealSelection && selected >= 0) {
        const int top = selected * (CARD_H + CARD_GAP);
        const int bottom = top + CARD_H + detail;
        if (bottom - scroll > view.height) scroll = static_cast<float>(bottom - view.height);
        if (top < scroll) scroll = static_cast<float>(top);
    }
    revealSelection = false;
    scroll = std::clamp(scroll, 0.0f, maxListScroll(state, frame));

    canvas.setClipRect(view.x, view.y, view.width, view.height);
    for (int card = 0; card < CARD_COUNT; ++card) {
        const auto rect = cardRect(state, frame, card);
        const bool open = card == selected;
        const int height = rect.height + (open ? detail : 0);
        if (rect.y + height < view.y || rect.y >= view.y + view.height) continue;
        const uint8_t slot = cardSlot(card);
        const MoveInfo* move = cardMove(frame, slot);
        if (open) {
            canvas.fillRoundRect(rect.x, rect.y, rect.width, height, 8, accentColor());
            canvas.fillRoundRect(rect.x + 2, rect.y + 2, rect.width - 4, height - 4, 6, selectedColor());
        } else {
            canvas.fillRoundRect(rect.x, rect.y, rect.width, height, 8, cardColor());
        }
        if (move) typeBadge(rect.x + 10, rect.y + 11, move->type);
        PixelRenderer::text(rect.x + 60, rect.y + 14, move ? move->name : Ui::Status::MOVE_UNKNOWN,
                            open ? accentColor() : textColor(), 1);
        if (slot == NEW_SLOT)
            PixelRenderer::text(rect.x + rect.width - 28, rect.y + 14, Ui::Explore::LEARN_NEW_TAG,
                                greenColor(), 1);
        if (!open || !move) continue;
        int y = rect.y + CARD_H + 8;
        canvas.drawFastHLine(TEXT_X, rect.y + CARD_H, TEXT_W, PixelRenderer::rgb(56, 87, 89));
        powerLine(*move, TEXT_X, y);
        y += LINE_H + 6;
        if (slot != NEW_SLOT && frame.monster) {
            PixelRenderer::text(TEXT_X, y, Ui::Team::MOVE_PROFICIENCY, hintColor(), 1);
            MovesDrawing::drawProficiencyBar(TEXT_X + 48, y + 4, TEXT_W - 52, 9,
                                             frame.monster->moveProficiency[slot]);
            y += LINE_H + 6;
        }
        StatusContent::wrapText(move->description, TEXT_W, TEXT_X, y, descriptionColor(), true);
    }
    canvas.clearClipRect();

    // Soft edges show that the list continues, like the AMOLED scroll shade.
    const float maximum = maxListScroll(state, frame);
    for (int i = 0; i < 20; i += 4) {
        const uint8_t alpha = static_cast<uint8_t>(200 * (20 - i) / 20);
        if (scroll > 0.5f)
            PixelRenderer::fillRectAlpha(view.x, view.y + i, view.width, 4, background(), alpha);
        if (scroll < maximum - 0.5f)
            PixelRenderer::fillRectAlpha(view.x, view.y + view.height - 4 - i, view.width, 4,
                                         background(), alpha);
    }

    if (selected < 0) actionPill(Ui::Explore::LEARN_ACTION_CONFIRM, mutedColor(), false);
    else if (cardSlot(selected) == NEW_SLOT)
        actionPill(Ui::Explore::LEARN_ACTION_GIVE_UP, textColor(), true);
    else actionPill(Ui::Explore::LEARN_ACTION_REPLACE, accentColor(), true);
}

void ProgressionUi::renderLearnedCard(const MoveLearnFrame& frame) {
    auto& canvas = PixelRenderer::canvas();
    header();
    const MoveInfo* move = findMove(frame.moveId);
    canvas.fillRoundRect(CARD_X, 76, CARD_W, 216, 12, cardColor());
    canvas.drawRoundRect(CARD_X, 76, CARD_W, 216, 12, PixelRenderer::rgb(56, 87, 89));
    centered(Ui::Explore::LEARN_NEW_MOVE_TITLE, 90, accentColor());
    const char* name = move ? move->name : Ui::Status::MOVE_UNKNOWN;
    if (move) {
        const int rowWidth = 40 + 8 + textWidth(name);
        const int x = (SIZE - rowWidth) / 2;
        typeBadge(x, 116, move->type);
        PixelRenderer::text(x + 48, 119, name, textColor(), 1);
    }
    char line[64];
    std::snprintf(line, sizeof(line), Ui::Explore::MOVE_LEARNED_FMT, name);
    centered(line, 148, greenColor());
    if (move) {
        powerLine(*move, TEXT_X, 176);
        canvas.setClipRect(TEXT_X, 198, TEXT_W, 84);
        StatusContent::wrapText(move->description, TEXT_W, TEXT_X, 200, descriptionColor(), true);
        canvas.clearClipRect();
    }
    actionPill(Ui::Common::CONTINUE, greenColor(), true);
}

void ProgressionUi::renderMoveReplacement(Game::MoveId oldId, Game::MoveId newId) {
    auto& canvas = PixelRenderer::canvas();
    header();
    canvas.fillRoundRect(CARD_X, 96, CARD_W, 180, 12, cardColor());
    canvas.drawRoundRect(CARD_X, 96, CARD_W, 180, 12, PixelRenderer::rgb(56, 87, 89));
    centered(Ui::Explore::MOVE_REPLACED_TITLE, 120, accentColor());
    const MoveInfo* oldMove = findMove(oldId);
    const MoveInfo* newMove = findMove(newId);
    char line[64];
    std::snprintf(line, sizeof(line), Ui::Explore::MOVE_FORGOT_FMT,
                  oldMove ? oldMove->name : Ui::Status::MOVE_UNKNOWN);
    centered(line, 168, descriptionColor());
    std::snprintf(line, sizeof(line), Ui::Explore::MOVE_LEARNED_FMT,
                  newMove ? newMove->name : Ui::Status::MOVE_UNKNOWN);
    centered(line, 200, greenColor());
    actionPill(Ui::Common::CONTINUE, greenColor(), true);
}

// ---- Native round evolution -------------------------------------------------
namespace {
using Sequence = Game::EvolutionSequence;

const PokemonSprites::SpriteFrame* evolutionSprite(uint16_t speciesId) {
    const auto* frame = PokemonSprites::findCachedSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
    return frame ? frame : PokemonSprites::findSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
}

bool morphShowsTarget(uint32_t morphElapsed) {
    static constexpr uint16_t intervals[] = {
        280, 260, 230, 210, 190, 170, 150, 130, 115, 100, 90, 80, 70, 60, 55, 50,
    };
    uint32_t cursor = 0;
    for (uint8_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i) {
        cursor += intervals[i];
        if (morphElapsed < cursor) return (i & 1U) != 0;
    }
    return true;
}

// Blend toward black (dim) and then toward white (flash), both 0-255.
uint16_t evoColor(uint8_t r, uint8_t g, uint8_t b, uint8_t dim, uint8_t white) {
    auto channel = [&](int value) {
        value = value * (255 - dim) / 255;
        return value + (255 - value) * white / 255;
    };
    return PixelRenderer::rgb(channel(r), channel(g), channel(b));
}

// Concentric glow centered on the sprite instead of the 240x135 band image.
void drawEvolutionGlow(uint8_t dim, uint8_t white) {
    auto& canvas = PixelRenderer::canvas();
    static constexpr uint8_t rings[][4] = {
        {180, 10, 24, 30}, {150, 16, 44, 50}, {124, 24, 70, 72},
        {100, 34, 100, 94}, {78, 48, 134, 118}, {58, 70, 168, 144},
    };
    canvas.fillRect(0, 0, 360, 360, evoColor(10, 24, 30, dim, white));
    for (const auto& ring : rings)
        canvas.fillCircle(ProgressionUi::EVOLUTION_CENTER_X, ProgressionUi::EVOLUTION_CENTER_Y,
                          ring[0], evoColor(ring[1], ring[2], ring[3], dim, white));
    canvas.fillEllipse(ProgressionUi::EVOLUTION_CENTER_X, ProgressionUi::EVOLUTION_GROUND_Y,
                       70, 10, evoColor(28, 84, 80, dim, white));
}

MenuGrid::Rect evolutionSpriteRect(const PokemonSprites::SpriteFrame* frame) {
    if (!frame) return {0, 0, 0, 0};
    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    // Feet rest on the ground ellipse; very tall frames stay below the top arc.
    const int y = std::max(48, ProgressionUi::EVOLUTION_GROUND_Y - height);
    return {ProgressionUi::EVOLUTION_CENTER_X - width / 2, y, width, height};
}

void drawEvolutionSprite(const PokemonSprites::SpriteFrame* frame, bool silhouette,
                         uint16_t color = PixelRenderer::rgb(236, 255, 246)) {
    if (!frame) return;
    const auto rect = evolutionSpriteRect(frame);
    if (silhouette) PokemonSprites::drawFrameSilhouette(frame, rect.x, rect.y, color);
    else PokemonSprites::drawFrame(frame, rect.x, rect.y);
}

int smoothWidth(const char* text, int height) {
    int width = 0;
    for (const auto* p = reinterpret_cast<const uint8_t*>(text); p && *p;) {
        if (*p < 0x80) { width += height / 2; ++p; continue; }
        width += height;
        ++p;
        while (*p && (*p & 0xC0) == 0x80) ++p;
    }
    return width;
}

void centeredSmooth(const char* text, int y, uint16_t color, int height) {
    if (!text) return;
    PixelRenderer::textSmooth(180 - smoothWidth(text, height) / 2, y, text, color, height);
}

// Rays and the ring stay above the message band below the sprite.
void roundEvolutionRays(uint32_t elapsed, int base, int span) {
    auto& canvas = PixelRenderer::canvas();
    const uint16_t color = PixelRenderer::rgb(214, 255, 240);
    for (int i = 0; i < 11; ++i) {
        const float angle = (-195.0f + i * 21.0f) * 3.14159265f / 180.0f;
        const float c = std::cos(angle), s = std::sin(angle);
        const int inner = base + static_cast<int>((elapsed / 18U + i * 7U) % uint32_t(span));
        const int outer = inner + 14 + (i & 3) * 3;
        for (int w = 0; w < 2; ++w)
            canvas.drawLine(ProgressionUi::EVOLUTION_CENTER_X + int(c * inner) + w,
                            ProgressionUi::EVOLUTION_CENTER_Y + int(s * inner),
                            ProgressionUi::EVOLUTION_CENTER_X + int(c * outer) + w,
                            ProgressionUi::EVOLUTION_CENTER_Y + int(s * outer), color);
    }
}

void drawUpperRing(int radius, uint16_t color) {
    auto& canvas = PixelRenderer::canvas();
    for (int step = 0; step < 720; ++step) {
        const float angle = step * 3.14159265f / 360.0f;
        const int dy = int(std::sin(angle) * radius);
        if (dy > 56) continue;
        const int x = ProgressionUi::EVOLUTION_CENTER_X + int(std::cos(angle) * radius);
        canvas.drawPixel(x, ProgressionUi::EVOLUTION_CENTER_Y + dy, color);
        canvas.drawPixel(x, ProgressionUi::EVOLUTION_CENTER_Y + dy + 1, color);
    }
}

void roundEvolutionSparkles(uint32_t elapsed) {
    auto& canvas = PixelRenderer::canvas();
    static constexpr int8_t offsets[][2] = {
        {-72, -40}, {61, -52}, {-86, 14}, {78, 26},
        {-45, 44}, {38, 50}, {0, -70}, {96, -8},
    };
    const uint16_t color = PixelRenderer::rgb(255, 244, 166);
    for (uint8_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        const uint32_t local = (elapsed + i * 93U) % 620U;
        if (local > 300) continue;
        const int radius = local < 150 ? 2 + int(local / 50) : 5 - int((local - 150) / 50);
        const int x = ProgressionUi::EVOLUTION_CENTER_X + offsets[i][0];
        const int y = ProgressionUi::EVOLUTION_CENTER_Y + offsets[i][1];
        for (int w = 0; w < 2; ++w) {
            canvas.drawLine(x - radius, y + w, x + radius, y + w, color);
            canvas.drawLine(x + w, y - radius, x + w, y + radius, color);
        }
    }
}

// Progress ring on the physical display edge while a cancel hold is active.
void drawHoldRing(float progress) {
    auto& canvas = PixelRenderer::canvas();
    const uint16_t track = PixelRenderer::rgb(39, 58, 62);
    const uint16_t fill = PixelRenderer::rgb(115, 226, 183);
    const int filled = int(progress * 720.0f);
    for (int step = 0; step < 720; ++step) {
        const float angle = (step / 2.0f - 90.0f) * 3.14159265f / 180.0f;
        const float c = std::cos(angle), s = std::sin(angle);
        const uint16_t color = step < filled ? fill : track;
        for (int r = 170; r <= 176; ++r) canvas.drawPixel(180 + int(c * r), 180 + int(s * r), color);
    }
}

void drawEvolutionMessage(const char* text, uint16_t color) {
    centeredSmooth(text, ProgressionUi::EVOLUTION_MESSAGE_Y, color, 20);
}

void drawContinueButton() {
    auto& canvas = PixelRenderer::canvas();
    const auto rect = ProgressionUi::evolutionContinueButton();
    canvas.fillRoundRect(rect.x, rect.y, rect.width, rect.height, rect.height / 2,
                         PixelRenderer::rgb(255, 216, 72));
    centeredSmooth(Ui::Common::CONTINUE, rect.y + (rect.height - 16) / 2,
                   PixelRenderer::rgb(20, 28, 32), 16);
}
}  // namespace

bool ProgressionUi::evolutionAnimating(uint32_t nowMs) const {
    return evolutionAnimation.initialized() && !evolutionAnimation.cancelling() &&
           !evolutionAnimation.animationComplete(nowMs);
}

bool ProgressionUi::evolutionReady(uint32_t nowMs) const {
    if (!evolutionAnimation.initialized()) return true;
    return evolutionAnimation.cancelling() ? evolutionAnimation.cancellationComplete(nowMs)
                                           : evolutionAnimation.animationComplete(nowMs);
}

void ProgressionUi::drawEvolutionHint(uint32_t nowMs, bool cancelling) {
    const uint16_t muted = PixelRenderer::rgb(150, 190, 182);
    if (cancelling) {
        centeredSmooth(Ui::Amoled::CANCELLING_EVOLUTION, 300, muted, 16);
        return;
    }
    if (evolutionHold) {
        const uint32_t held = nowMs - evolutionHoldStarted;
        drawHoldRing(std::min(1.0f, float(held) / float(EVOLUTION_CANCEL_HOLD_MS)));
        centeredSmooth(Ui::Amoled::HOLD_TO_CANCEL_EVOLUTION, 300,
                       PixelRenderer::rgb(115, 226, 183), 16);
        return;
    }
    centeredSmooth(Ui::Amoled::HOLD_TO_CANCEL_EVOLUTION, 300, muted, 16);
}

void ProgressionUi::renderEvolution(uint16_t fromSpeciesId, uint16_t toSpeciesId,
                                    uint32_t nowMs) {
    ensureEvolutionAnimation(fromSpeciesId, toSpeciesId, nowMs);
    const uint32_t elapsed = evolutionElapsed(nowMs);
    const Species* from = findSpecies(fromSpeciesId);
    const Species* to = findSpecies(toSpeciesId);
    const auto* fromFrame = evolutionSprite(fromSpeciesId);
    const auto* toFrame = evolutionSprite(toSpeciesId);
    char line[64];
    std::snprintf(line, sizeof(line), Ui::Common::EVOLUTION_FMT,
                  from ? from->name : Ui::Status::MOVE_UNKNOWN);
    const uint16_t text = PixelRenderer::rgb(239, 246, 240);

    if (elapsed < Sequence::INTRO_MS) {
        drawEvolutionGlow(0, 0);
        drawEvolutionSprite(fromFrame, false);
        drawEvolutionMessage(line, text);
        drawEvolutionHint(nowMs, false);
        return;
    }
    if (elapsed < Sequence::MORPH_END_MS) {
        const uint32_t morph = elapsed - Sequence::INTRO_MS;
        drawEvolutionGlow(90, 0);
        roundEvolutionRays(morph, 100, 26);
        drawUpperRing(84 + int((morph / 12U) % 30U), PixelRenderer::rgb(204, 255, 239));
        drawEvolutionSprite(morphShowsTarget(morph) ? toFrame : fromFrame, true);
        roundEvolutionSparkles(morph);
        drawEvolutionMessage(line, text);
        drawEvolutionHint(nowMs, false);
        return;
    }
    if (elapsed < Sequence::FLASH_END_MS) {
        const uint32_t flash = elapsed - Sequence::MORPH_END_MS;
        const uint8_t white = flash < 170
            ? uint8_t(flash * 255U / 170U)
            : uint8_t((Sequence::FLASH_END_MS - elapsed) * 255U /
                      (Sequence::FLASH_END_MS - Sequence::MORPH_END_MS - 170U));
        drawEvolutionGlow(0, white);
        drawEvolutionSprite(toFrame, true, PixelRenderer::rgb(255, 255, 255));
        return;
    }
    const uint32_t reveal = elapsed - Sequence::FLASH_END_MS;
    uint8_t white = 0;
    if (elapsed < Sequence::REVEAL_END_MS)
        white = uint8_t((Sequence::REVEAL_END_MS - elapsed) * 220U /
                        (Sequence::REVEAL_END_MS - Sequence::FLASH_END_MS));
    drawEvolutionGlow(0, white);
    drawEvolutionSprite(toFrame, false);
    if (white) {
        const auto rect = evolutionSpriteRect(toFrame);
        PixelRenderer::fillRectAlpha(rect.x, rect.y, rect.width, rect.height, 0xFFFF, white);
    }
    roundEvolutionSparkles(reveal);
    if (!evolutionAnimation.cryPlayed() && elapsed >= Sequence::REVEAL_END_MS) {
        evolutionAnimation.markCryPlayed();
        effects.playProgressionCry(toSpeciesId);
    }
    if (elapsed >= Sequence::REVEAL_END_MS) {
        std::snprintf(line, sizeof(line), Ui::Common::EVOLUTION_COMPLETE_FMT,
                      to ? to->name : Ui::Status::MOVE_UNKNOWN);
        drawEvolutionMessage(line, PixelRenderer::rgb(255, 236, 150));
    }
    if (elapsed >= Sequence::COMPLETE_MS) drawContinueButton();
    else drawEvolutionHint(nowMs, false);
}

void ProgressionUi::renderEvolutionCancelled(uint16_t speciesId, uint32_t nowMs) {
    const Species* species = findSpecies(speciesId);
    const auto* fromFrame = evolutionSprite(speciesId);
    const auto* toFrame = evolutionSprite(evolutionAnimation.toSpeciesId());
    const uint32_t elapsed = evolutionAnimation.cancelling()
        ? evolutionAnimation.cancellationElapsed(nowMs) : Sequence::CANCEL_COMPLETE_MS;
    char line[64];
    if (elapsed < Sequence::CANCEL_MORPH_MS) {
        drawEvolutionGlow(90, 0);
        // Rays contract toward the sprite while the old form returns.
        const uint32_t remaining = Sequence::CANCEL_MORPH_MS - elapsed;
        roundEvolutionRays(elapsed, 70 + int(remaining * 40U / Sequence::CANCEL_MORPH_MS), 6);
        const bool showOld = elapsed >= 420U || ((elapsed / 90U) & 1U) == 0;
        drawEvolutionSprite(showOld || !toFrame ? fromFrame : toFrame, true);
        roundEvolutionSparkles(elapsed);
        drawEvolutionHint(nowMs, true);
        return;
    }
    if (elapsed < Sequence::CANCEL_COMPLETE_MS) {
        const uint8_t white = uint8_t((Sequence::CANCEL_COMPLETE_MS - elapsed) * 180U /
            (Sequence::CANCEL_COMPLETE_MS - Sequence::CANCEL_MORPH_MS));
        drawEvolutionGlow(0, white);
        drawEvolutionSprite(fromFrame, false);
        const auto rect = evolutionSpriteRect(fromFrame);
        PixelRenderer::fillRectAlpha(rect.x, rect.y, rect.width, rect.height, 0xFFFF, white);
        roundEvolutionSparkles(elapsed);
        drawEvolutionHint(nowMs, true);
        return;
    }
    drawEvolutionGlow(0, 0);
    drawEvolutionSprite(fromFrame, false);
    std::snprintf(line, sizeof(line), Ui::Common::EVOLUTION_CANCELLED_FMT,
                  species ? species->name : Ui::Status::MOVE_UNKNOWN);
    drawEvolutionMessage(line, PixelRenderer::rgb(239, 246, 240));
    drawContinueButton();
}

EvolutionTouchAction ProgressionUi::handleEvolutionTouch(const TouchTap::Event& event,
                                                         uint32_t nowMs) {
    using Type = TouchTap::Type;
    if (event.type == Type::CANCEL) { cancelMoveLearnTouch(); return EvolutionTouchAction::NONE; }
    if (event.type == Type::DOWN) {
        cancelMoveLearnTouch();
        touchState = 4096;
        if (evolutionReady(nowMs)) {
            // The whole screen continues; the button is only the visual cue.
            evolutionTapArmed = true;
        } else if (evolutionAnimating(nowMs)) {
            evolutionHold = true;
            evolutionHoldStarted = nowMs;
        }
        return EvolutionTouchAction::NONE;
    }
    if (touchState != 4096) { cancelMoveLearnTouch(); return EvolutionTouchAction::NONE; }
    if (evolutionHold) {
        if (event.type == Type::UP) { evolutionHold = false; return EvolutionTouchAction::NONE; }
        return evolutionCancelHeld(nowMs) ? EvolutionTouchAction::CANCEL : EvolutionTouchAction::NONE;
    }
    if (!evolutionTapArmed) return EvolutionTouchAction::NONE;
    // Unlike TouchTap, a slow release still continues once the screen is ready.
    if (event.type != Type::UP) return EvolutionTouchAction::NONE;
    cancelMoveLearnTouch();
    return evolutionReady(nowMs) ? EvolutionTouchAction::CONTINUE : EvolutionTouchAction::NONE;
}

bool ProgressionUi::evolutionCancelHeld(uint32_t nowMs) {
    if (!evolutionHold) return false;
    if (!evolutionAnimating(nowMs)) { evolutionHold = false; return false; }
    if (nowMs - evolutionHoldStarted < EVOLUTION_CANCEL_HOLD_MS) return false;
    // Keep the gesture owned so the release cannot continue the cancelled screen.
    evolutionHold = false;
    evolutionTapArmed = false;
    return true;
}
}  // namespace Ui
