#include "ui/ProgressionScreens.h"
#include "ui/TeamScreensInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/UiStrings.h"
#include "game/MoveManagementService.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"
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
    if (textWidth(line) <= DIALOG_W - 24) {
        text(canvas, (AmoledUi::WIDTH - textWidth(line)) / 2, DIALOG_Y + 18,
             line, body);
    } else {
        char firstLine[64];
        char secondLine[32];
        std::snprintf(firstLine, sizeof(firstLine), "%s等级", name);
        std::snprintf(secondLine, sizeof(secondLine), "提升到%u", level);
        text(canvas, (AmoledUi::WIDTH - textWidth(firstLine)) / 2,
             DIALOG_Y + 8, firstLine, body);
        text(canvas, (AmoledUi::WIDTH - textWidth(secondLine)) / 2,
             DIALOG_Y + 40, secondLine, body);
    }
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

const PokemonSprites::SpriteFrame* evolutionFrame(uint16_t speciesId) {
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findCachedSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    return frame ? frame : PokemonSprites::findSpeciesSprite(
                               speciesId, PokemonSprites::SpriteKind::FRONT);
}

void drawEvolutionBackground(Canvas565& canvas) {
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT,
                    rgb(10, 25, 31));
    if (!GameAssets::draw(GameAssets::Kind::EVOLUTION_BACKGROUND,
                          -56, 0, 2.0f)) {
        const uint16_t bands[] = {
            rgb(20, 58, 64), rgb(28, 91, 88), rgb(44, 128, 113),
            rgb(76, 164, 137), rgb(133, 202, 171),
        };
        constexpr int bandCount = sizeof(bands) / sizeof(bands[0]);
        for (int index = 0; index < bandCount; ++index) {
            const int top = index * 280 / bandCount;
            const int bottom = (index + 1) * 280 / bandCount;
            canvas.fillRect(0, top, AmoledUi::WIDTH, bottom - top,
                            bands[index]);
        }
    }
    PixelRenderer::fillRectAlpha(0, 0, AmoledUi::WIDTH, 292,
                                 rgb(3, 12, 18), 70);
}

void drawCenteredEvolutionText(Canvas565& canvas, int y, const char* value,
                               uint16_t color) {
    const int width = textWidth(value);
    text(canvas, std::max(8, (AmoledUi::WIDTH - width) / 2), y, value, color);
}

void drawEvolutionSprite(const PokemonSprites::SpriteFrame* frame,
                         bool silhouette, uint16_t silhouetteColor) {
    if (!frame) return;
    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return;
    const float scale = std::min(
        1.5f, std::min(184.0f / static_cast<float>(width),
                       184.0f / static_cast<float>(height)));
    const int drawnWidth = std::max(1, static_cast<int>(std::lround(width * scale)));
    const int drawnHeight = std::max(1, static_cast<int>(std::lround(height * scale)));
    const int x = 184 - drawnWidth / 2;
    const int y = 168 - drawnHeight / 2;
    if (silhouette && std::fabs(scale - 1.0f) < 0.01f) {
        PokemonSprites::drawFrameSilhouette(frame, x, y, silhouetteColor);
    } else if (silhouette) {
        // FRONT resources generated at 2x already fit this page. Keep the
        // silhouette centered when a larger legacy frame has to be fitted.
        const int nativeX = 184 - width / 2;
        const int nativeY = 168 - height / 2;
        PokemonSprites::drawFrameSilhouette(
            frame, nativeX, nativeY, silhouetteColor);
    } else {
        PokemonSprites::drawFrameScaled(frame, x, y, scale, false);
    }
}

bool evolutionMorphShowsTarget(uint32_t elapsed) {
    static constexpr uint16_t intervals[] = {
        280, 260, 230, 210, 190, 170, 150, 130,
        115, 100, 90, 80, 70, 60, 55, 50,
    };
    uint32_t cursor = 0;
    for (uint8_t index = 0;
         index < sizeof(intervals) / sizeof(intervals[0]); ++index) {
        cursor += intervals[index];
        if (elapsed < cursor) return (index & 1U) != 0;
    }
    return true;
}

void drawEvolutionRays(Canvas565& canvas, uint32_t elapsed, bool inward) {
    static constexpr int8_t directions[][2] = {
        {0, -10}, {5, -9}, {9, -5}, {10, 0}, {9, 5}, {5, 9},
        {0, 10}, {-5, 9}, {-9, 5}, {-10, 0}, {-9, -5}, {-5, -9},
    };
    const uint16_t color = rgb(220, 255, 244);
    for (uint8_t index = 0;
         index < sizeof(directions) / sizeof(directions[0]); ++index) {
        int inner = 52 + static_cast<int>((elapsed / 22U + index * 5U) % 24U);
        int outer = inner + 24 + (index & 3U) * 3;
        if (inward) {
            const uint32_t remaining =
                elapsed < Game::EvolutionSequence::CANCEL_MORPH_MS
                    ? Game::EvolutionSequence::CANCEL_MORPH_MS - elapsed : 0;
            outer = 58 + static_cast<int>(remaining * 58U /
                Game::EvolutionSequence::CANCEL_MORPH_MS);
            inner = std::max(36, outer - 28);
        }
        canvas.drawLine(
            184 + directions[index][0] * inner / 10,
            168 + directions[index][1] * inner / 10,
            184 + directions[index][0] * outer / 10,
            168 + directions[index][1] * outer / 10, color);
    }
}

void drawEvolutionSparkles(Canvas565& canvas, uint32_t elapsed) {
    static constexpr int8_t offsets[][2] = {
        {-116, -62}, {-78, 70}, {-34, -104}, {22, 92},
        {68, -86}, {108, 48}, {126, -20}, {-126, 18},
    };
    const uint16_t color = rgb(244, 255, 226);
    for (uint8_t index = 0;
         index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        if (((elapsed / 90U) + index) % 3U == 0) continue;
        const int x = 184 + offsets[index][0];
        const int y = 168 + offsets[index][1];
        const int radius = 2 + static_cast<int>((elapsed / 120U + index) & 1U);
        canvas.drawFastHLine(x - radius, y, radius * 2 + 1, color);
        canvas.drawFastVLine(x, y - radius, radius * 2 + 1, color);
    }
}

void renderEvolutionProgression(Canvas565& canvas,
                                const ProgressionViewModel& model) {
    using Phase = Game::EvolutionSequence::Phase;
    drawEvolutionBackground(canvas);

    const Species* from = findSpecies(model.fromSpeciesId);
    const Species* target = findSpecies(model.toSpeciesId);
    const auto* fromFrame = evolutionFrame(model.fromSpeciesId);
    const auto* targetFrame = evolutionFrame(model.toSpeciesId);
    const Phase phase = model.evolutionPhase;
    const uint32_t elapsed = model.evolutionElapsedMs;
    const uint16_t silhouette = rgb(236, 255, 247);

    if (phase == Phase::INTRO || phase == Phase::IDLE) {
        drawEvolutionSprite(fromFrame, false, silhouette);
    } else if (phase == Phase::MORPH) {
        PixelRenderer::fillRectAlpha(0, 0, AmoledUi::WIDTH, 292,
                                     rgb(0, 8, 12), 80);
        const uint32_t morphElapsed = elapsed - Game::EvolutionSequence::INTRO_MS;
        drawEvolutionRays(canvas, morphElapsed, false);
        drawEvolutionSprite(
            evolutionMorphShowsTarget(morphElapsed) ? targetFrame : fromFrame,
            true, silhouette);
        canvas.drawCircle(184, 168,
                          72 + static_cast<int>((morphElapsed / 20U) % 28U),
                          rgb(201, 255, 238));
        drawEvolutionSparkles(canvas, morphElapsed);
    } else if (phase == Phase::FLASH) {
        drawEvolutionSprite(targetFrame, true, silhouette);
        const uint32_t flashElapsed = elapsed - Game::EvolutionSequence::MORPH_END_MS;
        const uint8_t alpha = flashElapsed < 170U
            ? static_cast<uint8_t>(flashElapsed * 255U / 170U)
            : static_cast<uint8_t>(
                  (Game::EvolutionSequence::FLASH_END_MS - elapsed) * 255U /
                  (Game::EvolutionSequence::FLASH_END_MS -
                   Game::EvolutionSequence::MORPH_END_MS - 170U));
        PixelRenderer::fillRectAlpha(0, 0, AmoledUi::WIDTH, 292,
                                     rgb(255, 255, 255), alpha);
    } else if (phase == Phase::CANCEL_MORPH) {
        drawEvolutionRays(canvas, elapsed, true);
        const bool showOld = elapsed >= 420U || ((elapsed / 90U) & 1U) == 0;
        drawEvolutionSprite(showOld ? fromFrame : targetFrame, true, silhouette);
        drawEvolutionSparkles(canvas, elapsed);
    } else {
        const bool cancelled = phase == Phase::CANCEL_REVEAL ||
                               phase == Phase::CANCELLED;
        drawEvolutionSprite(cancelled ? fromFrame : targetFrame, false,
                            silhouette);
        if (phase == Phase::REVEAL) {
            const uint8_t alpha = static_cast<uint8_t>(
                (Game::EvolutionSequence::REVEAL_END_MS - elapsed) * 220U /
                (Game::EvolutionSequence::REVEAL_END_MS -
                 Game::EvolutionSequence::FLASH_END_MS));
            PixelRenderer::fillRectAlpha(0, 0, AmoledUi::WIDTH, 292,
                                         rgb(255, 255, 255), alpha);
        } else if (phase == Phase::CANCEL_REVEAL) {
            const uint32_t remaining =
                Game::EvolutionSequence::CANCEL_COMPLETE_MS - elapsed;
            const uint8_t alpha = static_cast<uint8_t>(
                remaining * 180U /
                (Game::EvolutionSequence::CANCEL_COMPLETE_MS -
                 Game::EvolutionSequence::CANCEL_MORPH_MS));
            PixelRenderer::fillRectAlpha(0, 0, AmoledUi::WIDTH, 292,
                                         rgb(255, 255, 255), alpha);
        }
        drawEvolutionSparkles(canvas, elapsed);
    }

    char message[64];
    if (phase == Phase::CANCELLED) {
        std::snprintf(message, sizeof(message),
                      Ui::Common::EVOLUTION_CANCELLED_FMT,
                      from ? from->name : Ui::Status::MOVE_UNKNOWN);
    } else if (model.evolutionReady) {
        std::snprintf(message, sizeof(message),
                      Ui::Common::EVOLUTION_COMPLETE_FMT,
                      target ? target->name : Ui::Status::MOVE_UNKNOWN);
    } else {
        std::snprintf(message, sizeof(message), Ui::Common::EVOLUTION_FMT,
                      from ? from->name : Ui::Status::MOVE_UNKNOWN);
    }
    drawCenteredEvolutionText(canvas, 270, message, rgb(239, 246, 240));

    const bool cancelling = phase == Phase::CANCEL_MORPH ||
                            phase == Phase::CANCEL_REVEAL;
    if (!model.evolutionReady && phase != Phase::CANCELLED) {
        drawCenteredEvolutionText(
            canvas, 388,
            cancelling ? Ui::Amoled::CANCELLING_EVOLUTION
                       : Ui::Amoled::HOLD_TO_CANCEL_EVOLUTION,
            rgb(175, 204, 198));
        if (!cancelling && model.evolutionCancelHoldProgress > 0) {
            constexpr int HOLD_BAR_X = 104;
            constexpr int HOLD_BAR_Y = 340;
            constexpr int HOLD_BAR_WIDTH = 160;
            constexpr int HOLD_BAR_HEIGHT = 6;
            canvas.fillRoundRect(HOLD_BAR_X, HOLD_BAR_Y, HOLD_BAR_WIDTH,
                                 HOLD_BAR_HEIGHT, 3, rgb(39, 58, 62));
            canvas.fillRoundRect(
                HOLD_BAR_X, HOLD_BAR_Y,
                HOLD_BAR_WIDTH * model.evolutionCancelHoldProgress / 100,
                HOLD_BAR_HEIGHT, 3, rgb(115, 226, 183));
        }
    }
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
        renderEvolutionProgression(canvas, model);
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
        text(canvas, 88, 224, move ? move->name : Ui::Status::MOVE_UNKNOWN,
             rgb(226, 238, 233));
    }
    canvas.fillRoundRect((40), (348), (288), (80), (8), model.pressedItem == 0 ? rgb(48, 74, 68) : rgb(24, 34, 42));
    text(canvas, 130, 380, Ui::Amoled::CONTINUE,
         rgb(115, 226, 183));
    drawToast(canvas, model.toast);
    pageClip.reset();
}

}  // namespace AmoledV1
