#include "ui/ProgressionScreens.h"
#include "ui/EvolutionPresentation.h"
#include "ui/TeamScreensInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/UiStrings.h"
#include "game/MoveManagementService.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"
#include "presentation/TextBlock.h"
#include "ui/UiCommon.h"
#include "ui/UiMetrics.h"

namespace AmoledV1 {

using UiCommon::rgb;
using UiCommon::textWidth;
using UiCommon::text;
using UiCommon::drawToast;

namespace {

constexpr int HEADER_HEIGHT = UiMetrics::HOME_HEADER_HEIGHT;
constexpr int TEAM_MOVES_HEADER_HEIGHT = UiMetrics::PAGE_HEADER_HEIGHT;

}  // namespace

int progressionItemAt(int x, int y, ProgressionViewModel::Mode mode) {
    if (mode == ProgressionViewModel::Mode::EVOLUTION) return -1;
    if (mode == ProgressionViewModel::Mode::LEVEL_UP) {
        return x >= 0 && x < AmoledUi::WIDTH &&
               y >= 0 && y < AmoledUi::HEIGHT ? 0 : -1;
    }
    if (mode == ProgressionViewModel::Mode::MOVE_REPLACE) return -1;
    return x >= 40 && x < 328 && y >= 348 && y < 428 ? 0 : -1;
}

int progressionReplaceItemAt(int x, int y, int16_t scrollOffsetY,
                             uint8_t selectedItem, float detailProgress) {
    if (x < 12 || x >= 356 || y < TEAM_MOVES_HEADER_HEIGHT ||
        y >= AmoledUi::HEIGHT) return -1;
    const int push = selectedItem < 3 ? static_cast<int>(std::lround(
        UiMetrics::MOVE_DETAIL_HEIGHT * std::clamp(detailProgress, 0.0f, 1.0f))) : 0;
    for (int index = 0; index < 3; ++index) {
        const int rowY = TEAM_MOVES_HEADER_HEIGHT + index * 66 - scrollOffsetY +
                         (index > selectedItem ? push : 0);
        if (y >= rowY && y < rowY + 60) return index;
    }
    return -1;
}

namespace {

void drawLevelUpDialog(Canvas565& canvas, const Species* species,
                       uint8_t level) {
    constexpr int DIALOG_X = 20;
    constexpr int DIALOG_Y = 328;
    constexpr int DIALOG_W = 328;
    constexpr int DIALOG_H = 100;
    const uint16_t border = rgb(126, 175, 175);
    const uint16_t body = rgb(226, 238, 233);

    PixelRenderer::fillRectAlpha(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H,
                                 rgb(0, 0, 0), 190);
    canvas.drawRect(DIALOG_X, DIALOG_Y, DIALOG_W, DIALOG_H, border);

    const char* name = species && species->name ? species->name : "精灵";
    char line[96];
    std::snprintf(line, sizeof(line), Ui::Common::LEVEL_UP_DIALOG_FMT,
                  name, level);
    static Ui::TextBlock message;
    static char source[sizeof(line)] = {};
    Ui::TextBlock::Options options;
    options.width = DIALOG_W - 24;
    options.maxHeight = 64;
    options.font = BitmapText::Font::PX32;
    options.lineGap = 0;
    options.align = Ui::TextBlock::Align::CENTER;
    const bool optionsChanged = message.setOptions(options);
    if (optionsChanged || std::strcmp(source, line) != 0) {
        // Key the authored-break decision by the original text, so a two-line
        // message does not alternate source/layout on every animation paint.
        std::snprintf(source, sizeof(source), "%s", line);
        message.setText(line);
        if (message.measure().lineCount > 1) {
            // Retain the authored phrase break instead of isolating the level.
            std::snprintf(line, sizeof(line), "%s等级\n提升到%u", name, level);
            message.setText(line);
        }
    }
    const int top = DIALOG_Y + (message.measure().lineCount > 1 ? 8 : 18);
    message.draw(canvas, DIALOG_X + 12, top, body);
}

void drawLevelUpScene(Canvas565& canvas, const Species* species,
                      const ProgressionViewModel& model) {
    constexpr uint32_t DURATION_MS = 800;
    const float progress = std::min(model.levelUpElapsedMs, DURATION_MS) /
                           static_cast<float>(DURATION_MS);
    const float ease = 1.0f - (1.0f - progress) * (1.0f - progress);
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT,
                    rgb(12, 26, 31));
    text(canvas, (AmoledUi::WIDTH - textWidth(Ui::Amoled::LEVEL_UP)) / 2,
         38, Ui::Amoled::LEVEL_UP, rgb(115, 226, 183));

    if (species) {
        const PokemonSprites::SpriteFrame* frame =
            PokemonSprites::findSpeciesSprite(
                species->id, PokemonSprites::SpriteKind::FRONT);
        if (frame) {
            const int width = FlashStorage::readByte(&frame->width);
            const int height = FlashStorage::readByte(&frame->height);
            if (width > 0 && height > 0) {
                const float fit = std::min(
                    1.8f, std::min(224.0f / width, 196.0f / height));
                const float scale = fit * (0.88f + 0.12f * ease);
                const int drawnWidth = std::max(1, static_cast<int>(std::lround(width * scale)));
                const int drawnHeight = std::max(1, static_cast<int>(std::lround(height * scale)));
                PokemonSprites::drawFrameScaled(
                    frame, (AmoledUi::WIDTH - drawnWidth) / 2,
                    178 - drawnHeight / 2 + static_cast<int>(std::lround(12 * (1 - ease))),
                    scale, false);
            }
        }
    }

    char oldLabel[20];
    char newLabel[20];
    std::snprintf(oldLabel, sizeof(oldLabel), "Lv.%u", model.oldLevel);
    std::snprintf(newLabel, sizeof(newLabel), "Lv.%u", model.level);
    const int gap = 22;
    const int totalWidth = textWidth(oldLabel) + gap + textWidth(newLabel);
    const int left = (AmoledUi::WIDTH - totalWidth) / 2;
    text(canvas, left, 278, oldLabel, rgb(126, 175, 175));
    text(canvas, left + textWidth(oldLabel) + (gap - textWidth(">")) / 2,
         278, ">", rgb(126, 175, 175));
    text(canvas, left + textWidth(oldLabel) + gap, 278, newLabel,
         progress < 0.55f ? rgb(126, 175, 175) : rgb(248, 210, 105));
    drawLevelUpDialog(canvas, species, model.level);
}


}  // namespace

void renderProgressionScreen(Canvas565& canvas,
                             const ProgressionViewModel& model,
                             uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    if (model.mode == ProgressionViewModel::Mode::EVOLUTION) {
        drawEvolutionPresentation(canvas, model);
        drawToast(canvas, model.toast);
        pageClip.reset();
        return;
    }
    if (model.mode == ProgressionViewModel::Mode::MOVE_REPLACE) {
        constexpr int listTop = TEAM_MOVES_HEADER_HEIGHT;
        constexpr int detailSize = UiMetrics::MOVE_DETAIL_HEIGHT;
        const MoveInfo* moves[] = {
            findMove(model.moveId), findMove(model.oldMove2),
            findMove(model.oldMove3)};
        const float progress = std::clamp(model.detailProgress, 0.0f, 1.0f);
        const int push = model.selectedItem < 3
            ? static_cast<int>(std::lround(detailSize * progress)) : 0;
        const int detailTop = listTop + (model.selectedItem + 1) * 66 -
                              model.scrollOffsetY;
        if (model.selectedItem < 3 && moves[model.selectedItem] && push > 0) {
            UiCommon::PageClip detailClip(canvas,
                static_cast<uint16_t>(std::clamp(std::max<int>(rowBegin, detailTop),
                                                 listTop, AmoledUi::HEIGHT)),
                static_cast<uint16_t>(std::clamp(std::min<int>(rowEnd, detailTop + push),
                                                 listTop, AmoledUi::HEIGHT)));
            canvas.fillRect(0, detailTop, AmoledUi::WIDTH, push,
                            rgb(17, 27, 34));
            canvas.fillRect(0, detailTop, AmoledUi::WIDTH, 2,
                            rgb(56, 87, 89));
            drawMoveDetailContent(canvas, *moves[model.selectedItem], detailTop,
                static_cast<uint8_t>(std::lround(255.0f * progress)));
            detailClip.reset();
        }
        pageClip.setRect(0, listTop, AmoledUi::WIDTH,
                         AmoledUi::HEIGHT - listTop);
        for (int index = 0; index < 3; ++index) {
            const int y = listTop + index * 66 - model.scrollOffsetY +
                          (index > model.selectedItem ? push : 0);
            if (y + 60 <= listTop || y >= AmoledUi::HEIGHT) continue;
            canvas.fillRoundRect(12, y, 344, 60, 8,
                model.selectedItem == index ? rgb(42, 61, 68)
                                            : rgb(24, 34, 42));
            if (moves[index]) {
                drawTeamMoveTypeBadge(canvas, 20, y + 8, moves[index]->type);
            }
            const char* name = moves[index] ? moves[index]->name
                                            : Ui::Status::MOVE_UNKNOWN;
            text(canvas, 108, y + 14, name, rgb(226, 238, 233));
            if (index == 0 && textWidth(name) <= 200) {
                text(canvas, 316, y + 14, "新", rgb(115, 226, 183));
            }
        }
        pageClip.setRect(0, rowBegin, AmoledUi::WIDTH, rowEnd - rowBegin);
        if (model.scrollOffsetY > 0) {
            const int strength = std::min<int>(255, model.scrollOffsetY * 18);
            for (int y = 0; y < 28; y += 4) {
                PixelRenderer::fillRectAlpha(0, listTop + y, AmoledUi::WIDTH,
                    4, rgb(0, 0, 0), static_cast<uint8_t>(
                        strength * (28 - y) / 28));
            }
        }
        UiCommon::drawPageHeader(canvas, nullptr, nullptr, false);
        UiCommon::drawHeaderText(canvas, 28, Ui::Amoled::MOVE_LIST,
                                  rgb(102, 176, 245), listTop);
        const bool enabled = model.selectedItem < 3;
        const uint16_t iconColor = enabled ? rgb(115, 226, 183)
                                           : rgb(91, 104, 104);
        canvas.fillCircle(328, 38, 26,
                          enabled ? rgb(27, 62, 53) : rgb(27, 43, 51));
        canvas.drawCircle(328, 38, 26, iconColor);
        canvas.drawLine(315, 38, 324, 47, iconColor);
        canvas.drawLine(324, 47, 342, 28, iconColor);
        drawToast(canvas, model.toast);
        pageClip.reset();
        return;
    }
    if (model.mode == ProgressionViewModel::Mode::LEVEL_UP) {
        drawLevelUpScene(canvas, model.state && model.teamSlot < Game::TEAM_CAP
                        ? findSpecies(model.state->team[model.teamSlot].speciesId)
                        : nullptr, model);
        drawToast(canvas, model.toast);
        pageClip.reset();
        return;
    }
    UiCommon::drawHeader(canvas, HEADER_HEIGHT);
    UiCommon::drawHeaderText(canvas, 52, Ui::Amoled::GROWTH, rgb(115, 226, 183), HEADER_HEIGHT);
    const Game::MonsterRuntime* monster = model.state &&
        model.teamSlot < Game::TEAM_CAP ? &model.state->team[model.teamSlot] : nullptr;
    const Species* species = monster ? findSpecies(monster->speciesId) : nullptr;
    if (species) {
        const PokemonSprites::SpriteFrame* frame =
            PokemonSprites::findSpeciesSprite(species->id, PokemonSprites::SpriteKind::FRONT);
        if (frame) PokemonSprites::drawFrameScaled(frame, 104, 88, 1.5f, false);
    }
    const char* title = Ui::Amoled::LEVEL_UP;
    if (model.mode == ProgressionViewModel::Mode::MOVE_LEARN) {
        title = Ui::Amoled::NEW_MOVE;
    }
    text(canvas, 144, 80, title, rgb(248, 210, 105));
    {
        const MoveInfo* move = findMove(model.moveId);
        static Ui::TextBlock moveName;
        Ui::TextBlock::Options options;
        options.width = 256;
        options.maxHeight = 100;
        options.font = BitmapText::Font::PX32;
        moveName.setText(move ? move->name : Ui::Status::MOVE_UNKNOWN);
        moveName.setOptions(options);
        moveName.draw(canvas, 88, 224, rgb(226, 238, 233));
    }
    canvas.fillRoundRect((40), (348), (288), (80), (8), model.pressedItem == 0 ? rgb(48, 74, 68) : rgb(24, 34, 42));
    text(canvas, 130, 380, Ui::Amoled::CONTINUE,
         rgb(115, 226, 183));
    drawToast(canvas, model.toast);
    pageClip.reset();
}

}  // namespace AmoledV1
