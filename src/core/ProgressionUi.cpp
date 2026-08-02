#include "core/ProgressionUi.h"

#include <algorithm>
#include <cstdio>
#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/CryPlayer.h"
#include "core/GameEngine.h"
#include "core/UiStrings.h"
#include "game/Species.h"
#include "hardware/Hal.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"
#include "presentation/PixelRenderer.h"

namespace {

constexpr uint32_t EVOLUTION_INTRO_MS = 600;
constexpr uint32_t EVOLUTION_MORPH_END_MS = 2840;
constexpr uint32_t EVOLUTION_FLASH_END_MS = 3180;
constexpr uint32_t EVOLUTION_REVEAL_END_MS = 3580;
constexpr uint32_t EVOLUTION_COMPLETE_MS = 4000;
constexpr uint32_t EVOLUTION_CANCEL_MORPH_MS = 650;
constexpr uint32_t EVOLUTION_CANCEL_COMPLETE_MS = 1000;
constexpr int EVOLUTION_CENTER_X = Hal::DISPLAY_W / 2;
constexpr int EVOLUTION_CENTER_Y = 61;
constexpr uint8_t MOVE_LEARN_NEW_SLOT = Game::MOVE_SLOT_COUNT;

struct EvolutionAnimationState {
    uint16_t fromSpeciesId = 0;
    uint16_t toSpeciesId = 0;
    uint32_t startedAt = 0;
    uint32_t cancellationStartedAt = 0;
    bool initialized = false;
    bool cryPlayed = false;
    bool cancelling = false;
};

EvolutionAnimationState evolutionAnimation;

int moveLearnTextPixelWidth(const char* value) {
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

const MoveInfo* moveLearnInfoForSlot(const Species& species,
                                     const Game::MonsterRuntime& mon,
                                     uint8_t moveSlot) {
    Game::MoveId moveId = moveSlot == 0
        ? moveIdForMonster(species, mon, false)
        : specialMoveIdForMonster(mon, moveSlot - 1);
    return findMove(moveId);
}

uint16_t moveLearnProficiencyColor(uint8_t value) {
    if (value >= 90) return PixelRenderer::rgb(75, 209, 225);
    if (value >= 60) return PixelRenderer::rgb(92, 222, 112);
    if (value >= 25) return PixelRenderer::rgb(255, 216, 72);
    return PixelRenderer::rgb(239, 85, 85);
}

uint16_t moveLearnTypeColor(TypeId type) {
    switch (type) {
    case TypeId::FIRE: return PixelRenderer::rgb(239, 91, 67);
    case TypeId::WATER: return PixelRenderer::rgb(67, 145, 224);
    case TypeId::GRASS: return PixelRenderer::rgb(92, 188, 92);
    case TypeId::ELECTRIC: return PixelRenderer::rgb(245, 204, 67);
    case TypeId::ICE: return PixelRenderer::rgb(115, 206, 218);
    case TypeId::FIGHTING: return PixelRenderer::rgb(190, 85, 64);
    case TypeId::POISON: return PixelRenderer::rgb(155, 95, 196);
    case TypeId::GROUND: return PixelRenderer::rgb(184, 142, 83);
    case TypeId::FLYING: return PixelRenderer::rgb(135, 166, 220);
    case TypeId::PSYCHIC: return PixelRenderer::rgb(226, 98, 168);
    case TypeId::BUG: return PixelRenderer::rgb(142, 185, 72);
    case TypeId::ROCK: return PixelRenderer::rgb(160, 136, 84);
    case TypeId::GHOST: return PixelRenderer::rgb(105, 86, 160);
    case TypeId::DRAGON: return PixelRenderer::rgb(88, 118, 210);
    case TypeId::DARK: return PixelRenderer::rgb(86, 76, 70);
    case TypeId::STEEL: return PixelRenderer::rgb(144, 158, 172);
    case TypeId::FAIRY: return PixelRenderer::rgb(232, 138, 202);
    case TypeId::NORMAL:
    default: return PixelRenderer::rgb(154, 158, 132);
    }
}

void drawMoveLearnProficiencyBar(int x, int y, int width, int height,
                                 uint8_t value) {
    auto& canvas = PixelRenderer::canvas();
    value = std::min<uint8_t>(value, 100);
    const uint16_t trackColor = PixelRenderer::rgb(82, 87, 95);
    const uint16_t borderColor = PixelRenderer::rgb(45, 48, 56);
    const int radius = height / 2;
    canvas.fillRoundRect(x, y, width, height, radius, trackColor);
    const int fillWidth = ((width - 2) * value) / 100;
    if (fillWidth > 0) {
        const int fillHeight = height - 2;
        const int fillRadius = std::min(fillHeight / 2, fillWidth / 2);
        canvas.fillRoundRect(x + 1, y + 1, fillWidth, fillHeight,
                             fillRadius,
                             moveLearnProficiencyColor(value));
    }
    canvas.drawRoundRect(x, y, width, height, radius, borderColor);
}

int drawMoveLearnTypeBracket(int x, int y, TypeId type) {
    PixelRenderer::text(x, y, "[", PixelRenderer::rgb(241, 242, 232), 1);
    const char* name = typeName(type);
    PixelRenderer::text(x + 8, y, name, moveLearnTypeColor(type), 1);
    int closeX = x + 8 + moveLearnTextPixelWidth(name);
    PixelRenderer::text(closeX, y, "]",
                        PixelRenderer::rgb(241, 242, 232), 1);
    return closeX + 10;
}

void ensureEvolutionAnimation(uint16_t fromSpeciesId,
                              uint16_t toSpeciesId,
                              uint32_t nowMs) {
    if (evolutionAnimation.initialized &&
        evolutionAnimation.fromSpeciesId == fromSpeciesId &&
        evolutionAnimation.toSpeciesId == toSpeciesId) {
        return;
    }

    evolutionAnimation = {};
    evolutionAnimation.fromSpeciesId = fromSpeciesId;
    evolutionAnimation.toSpeciesId = toSpeciesId;
    evolutionAnimation.startedAt = nowMs;
    evolutionAnimation.initialized = true;

    const uint16_t species[] = {fromSpeciesId, toSpeciesId};
    PokemonSprites::preloadDynamicSpecies(species, 2, 2);
    Platform::logf("[EvolutionUi] begin from=%u to=%u\n",
                  fromSpeciesId, toSpeciesId);
}

uint32_t evolutionElapsed(uint32_t nowMs) {
    return nowMs >= evolutionAnimation.startedAt
        ? nowMs - evolutionAnimation.startedAt
        : 0;
}

const PokemonSprites::SpriteFrame* evolutionFrame(uint16_t speciesId) {
    const auto* frame = PokemonSprites::findCachedSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
    if (frame) return frame;
    return PokemonSprites::findSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
}

void drawEvolutionBackground() {
    auto& canvas = PixelRenderer::canvas();
    if (GameAssets::drawBattleBackground(
            GameAssets::Kind::EVOLUTION_BACKGROUND)) {
        return;
    }

    const uint16_t bands[] = {
        PixelRenderer::rgb(34, 83, 88),
        PixelRenderer::rgb(41, 116, 111),
        PixelRenderer::rgb(52, 153, 136),
        PixelRenderer::rgb(82, 190, 164),
        PixelRenderer::rgb(143, 222, 199),
        PixelRenderer::rgb(222, 246, 232),
    };
    constexpr int bandCount = sizeof(bands) / sizeof(bands[0]);
    for (int i = 0; i < bandCount; ++i) {
        int y = i * Hal::DISPLAY_H / bandCount;
        int nextY = (i + 1) * Hal::DISPLAY_H / bandCount;
        canvas.fillRect(0, y, Hal::DISPLAY_W, nextY - y, bands[i]);
    }
}

void drawCenteredEvolutionSprite(const PokemonSprites::SpriteFrame* frame,
                                 bool silhouette,
                                 uint16_t color = 0xFFFF) {
    if (!frame) return;
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    int x = EVOLUTION_CENTER_X - width / 2;
    int y = EVOLUTION_CENTER_Y - height / 2;
    if (silhouette) {
        PokemonSprites::drawFrameSilhouette(frame, x, y, color);
    } else {
        PokemonSprites::drawFrame(frame, x, y);
    }
}

bool morphShowsNewSpecies(uint32_t morphElapsed) {
    static constexpr uint16_t intervals[] = {
        280, 260, 230, 210, 190, 170, 150, 130,
        115, 100, 90, 80, 70, 60, 55, 50,
    };
    uint32_t cursor = 0;
    for (uint8_t i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i) {
        cursor += intervals[i];
        if (morphElapsed < cursor) return (i & 1U) != 0;
    }
    return true;
}

void drawEvolutionRays(uint32_t elapsed) {
    auto& canvas = PixelRenderer::canvas();
    static constexpr int8_t directions[][2] = {
        {0, -10}, {5, -9}, {9, -5}, {10, 0},
        {9, 5}, {5, 9}, {0, 10}, {-5, 9},
        {-9, 5}, {-10, 0}, {-9, -5}, {-5, -9},
    };
    const uint16_t color = PixelRenderer::rgb(224, 255, 245);
    for (uint8_t i = 0; i < sizeof(directions) / sizeof(directions[0]); ++i) {
        int inner = 12 + static_cast<int>((elapsed / 18 + i * 7) % 18);
        int outer = inner + 8 + (i & 3);
        int x1 = EVOLUTION_CENTER_X + directions[i][0] * inner / 10;
        int y1 = EVOLUTION_CENTER_Y + directions[i][1] * inner / 10;
        int x2 = EVOLUTION_CENTER_X + directions[i][0] * outer / 10;
        int y2 = EVOLUTION_CENTER_Y + directions[i][1] * outer / 10;
        canvas.drawLine(x1, y1, x2, y2, color);
    }
}

void drawCancellationRays(uint32_t elapsed) {
    auto& canvas = PixelRenderer::canvas();
    static constexpr int8_t directions[][2] = {
        {0, -10}, {5, -9}, {9, -5}, {10, 0},
        {9, 5}, {5, 9}, {0, 10}, {-5, 9},
        {-9, 5}, {-10, 0}, {-9, -5}, {-5, -9},
    };
    uint32_t remaining = elapsed < EVOLUTION_CANCEL_MORPH_MS
        ? EVOLUTION_CANCEL_MORPH_MS - elapsed
        : 0;
    int outer = 18 + static_cast<int>(
        remaining * 34U / EVOLUTION_CANCEL_MORPH_MS);
    int inner = outer - 10;
    if (inner < 10) inner = 10;
    const uint16_t color = PixelRenderer::rgb(224, 255, 245);
    for (uint8_t i = 0; i < sizeof(directions) / sizeof(directions[0]); ++i) {
        int phase = static_cast<int>((elapsed / 36U + i * 3U) % 5U);
        int localInner = inner - phase;
        if (localInner < 8) localInner = 8;
        int x1 = EVOLUTION_CENTER_X +
                 directions[i][0] * localInner / 10;
        int y1 = EVOLUTION_CENTER_Y +
                 directions[i][1] * localInner / 10;
        int x2 = EVOLUTION_CENTER_X +
                 directions[i][0] * outer / 10;
        int y2 = EVOLUTION_CENTER_Y +
                 directions[i][1] * outer / 10;
        canvas.drawLine(x1, y1, x2, y2, color);
    }
}

void drawEvolutionSparkles(uint32_t elapsed) {
    auto& canvas = PixelRenderer::canvas();
    static constexpr int8_t offsets[][2] = {
        {-45, -22}, {38, -28}, {-54, 10}, {49, 18},
        {-28, 30}, {24, 34}, {0, -38}, {60, -5},
    };
    const uint16_t color = PixelRenderer::rgb(255, 244, 166);
    for (uint8_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        uint32_t local = (elapsed + i * 93U) % 620U;
        if (local > 300) continue;
        int radius = local < 150 ? 1 + local / 75 : 3 - (local - 150) / 75;
        int x = EVOLUTION_CENTER_X + offsets[i][0];
        int y = EVOLUTION_CENTER_Y + offsets[i][1];
        canvas.drawLine(x - radius, y, x + radius, y, color);
        canvas.drawLine(x, y - radius, x, y + radius, color);
    }
}

void drawEvolutionMessage(const char* text) {
    auto& canvas = PixelRenderer::canvas();
    const uint16_t border = PixelRenderer::rgb(233, 247, 242);
    PixelRenderer::fillRectAlpha(10, 103, 220, 27,
                                 PixelRenderer::rgb(12, 32, 36), 210);
    canvas.drawRect(10, 103, 220, 27, border);
    PixelRenderer::text(20, 108, text, border, 1);
}

}  // namespace

namespace ProgressionUi {

void renderLevelUp(uint8_t level) {
    auto& canvas = PixelRenderer::canvas();
    const uint16_t background = PixelRenderer::rgb(18, 24, 32);
    const uint16_t panel = PixelRenderer::rgb(35, 42, 50);
    const uint16_t border = PixelRenderer::rgb(241, 242, 232);
    const uint16_t accent = PixelRenderer::rgb(255, 216, 72);
    const uint16_t hint = PixelRenderer::rgb(135, 214, 238);

    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, background);
    canvas.fillRect(24, 26, 192, 86, panel);
    canvas.drawRect(24, 26, 192, 86, border);
    PixelRenderer::text(96, 40, Ui::Common::LEVEL_UP_TITLE, accent, 1);

    char line[32];
    snprintf(line, sizeof(line), Ui::Common::LEVEL_UP_FMT, level);
    PixelRenderer::text(82, 64, line, border, 1);
    PixelRenderer::text(92, 92, Ui::Common::A_CONTINUE, hint, 1);
}

bool evolutionAnimationComplete(uint16_t fromSpeciesId,
                                uint16_t toSpeciesId,
                                uint32_t nowMs) {
    ensureEvolutionAnimation(fromSpeciesId, toSpeciesId, nowMs);
    return evolutionElapsed(nowMs) >= EVOLUTION_COMPLETE_MS;
}

void resetEvolutionAnimation() {
    evolutionAnimation = {};
}

void beginEvolutionCancellation(uint16_t fromSpeciesId,
                                uint16_t toSpeciesId,
                                uint32_t nowMs) {
    ensureEvolutionAnimation(fromSpeciesId, toSpeciesId, nowMs);
    CryPlayer::ins().stop();
    evolutionAnimation.cancelling = true;
    evolutionAnimation.cancellationStartedAt = nowMs;
}

bool evolutionCancellationComplete(uint32_t nowMs) {
    if (!evolutionAnimation.initialized ||
        !evolutionAnimation.cancelling) {
        return true;
    }
    return nowMs - evolutionAnimation.cancellationStartedAt >=
           EVOLUTION_CANCEL_COMPLETE_MS;
}

void renderEvolution(uint16_t fromSpeciesId,
                     uint16_t toSpeciesId,
                     uint32_t nowMs) {
    auto& canvas = PixelRenderer::canvas();
    ensureEvolutionAnimation(fromSpeciesId, toSpeciesId, nowMs);
    uint32_t elapsed = evolutionElapsed(nowMs);
    const Species* from = findSpecies(fromSpeciesId);
    const Species* to = findSpecies(toSpeciesId);
    const auto* fromFrame = evolutionFrame(fromSpeciesId);
    const auto* toFrame = evolutionFrame(toSpeciesId);

    drawEvolutionBackground();
    char line[48];
    if (elapsed < EVOLUTION_INTRO_MS) {
        drawCenteredEvolutionSprite(fromFrame, false);
        snprintf(line, sizeof(line), Ui::Common::EVOLUTION_FMT,
                 from ? from->name : Ui::Status::MOVE_UNKNOWN);
        drawEvolutionMessage(line);
        return;
    }

    if (elapsed < EVOLUTION_MORPH_END_MS) {
        PixelRenderer::darken(42);
        uint32_t morphElapsed = elapsed - EVOLUTION_INTRO_MS;
        drawEvolutionRays(morphElapsed);
        bool showNew = morphShowsNewSpecies(morphElapsed);
        drawCenteredEvolutionSprite(showNew ? toFrame : fromFrame, true);
        int ringRadius = 36 + static_cast<int>((morphElapsed / 18) % 24);
        canvas.drawCircle(EVOLUTION_CENTER_X, EVOLUTION_CENTER_Y, ringRadius,
                          PixelRenderer::rgb(204, 255, 239));
        drawEvolutionSparkles(morphElapsed);
        return;
    }

    if (elapsed < EVOLUTION_FLASH_END_MS) {
        drawCenteredEvolutionSprite(toFrame, true);
        uint32_t flashElapsed = elapsed - EVOLUTION_MORPH_END_MS;
        uint8_t alpha = flashElapsed < 170
            ? static_cast<uint8_t>(flashElapsed * 255U / 170U)
            : static_cast<uint8_t>(
                  (EVOLUTION_FLASH_END_MS - elapsed) * 255U /
                  (EVOLUTION_FLASH_END_MS - EVOLUTION_MORPH_END_MS - 170U));
        PixelRenderer::fillRectAlpha(
            0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0xFFFF, alpha);
        return;
    }

    drawCenteredEvolutionSprite(toFrame, false);
    uint32_t revealElapsed = elapsed - EVOLUTION_FLASH_END_MS;
    if (elapsed < EVOLUTION_REVEAL_END_MS) {
        uint8_t alpha = static_cast<uint8_t>(
            (EVOLUTION_REVEAL_END_MS - elapsed) * 220U /
            (EVOLUTION_REVEAL_END_MS - EVOLUTION_FLASH_END_MS));
        PixelRenderer::fillRectAlpha(
            0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0xFFFF, alpha);
    }
    drawEvolutionSparkles(revealElapsed);

    if (!evolutionAnimation.cryPlayed &&
        elapsed >= EVOLUTION_REVEAL_END_MS) {
        evolutionAnimation.cryPlayed = true;
        CryPlayer::ins().replay(toSpeciesId);
    }

    if (elapsed >= EVOLUTION_REVEAL_END_MS) {
        snprintf(line, sizeof(line), Ui::Common::EVOLUTION_COMPLETE_FMT,
                 to ? to->name : Ui::Status::MOVE_UNKNOWN);
        drawEvolutionMessage(line);
    }
    if (elapsed >= EVOLUTION_COMPLETE_MS) {
        PixelRenderer::text(174, 109, Ui::Common::A_CONTINUE,
                            PixelRenderer::rgb(255, 226, 108), 1);
    }
}

void renderEvolutionCancelled(uint16_t speciesId, uint32_t nowMs) {
    const Species* species = findSpecies(speciesId);
    const auto* fromFrame = evolutionFrame(speciesId);
    const auto* toFrame = evolutionFrame(evolutionAnimation.toSpeciesId);
    uint32_t elapsed = evolutionAnimation.cancelling
        ? nowMs - evolutionAnimation.cancellationStartedAt
        : EVOLUTION_CANCEL_COMPLETE_MS;
    drawEvolutionBackground();

    if (elapsed < EVOLUTION_CANCEL_MORPH_MS) {
        PixelRenderer::darken(42);
        drawCancellationRays(elapsed);
        bool showOld = elapsed >= 420U ||
                       ((elapsed / 90U) & 1U) == 0;
        drawCenteredEvolutionSprite(
            showOld || !toFrame ? fromFrame : toFrame, true);
        drawEvolutionSparkles(elapsed);
        return;
    }

    drawCenteredEvolutionSprite(fromFrame, false);
    if (elapsed < EVOLUTION_CANCEL_COMPLETE_MS) {
        uint32_t remaining =
            EVOLUTION_CANCEL_COMPLETE_MS - elapsed;
        uint8_t alpha = static_cast<uint8_t>(
            remaining * 180U /
            (EVOLUTION_CANCEL_COMPLETE_MS -
             EVOLUTION_CANCEL_MORPH_MS));
        PixelRenderer::fillRectAlpha(
            0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, 0xFFFF, alpha);
        drawEvolutionSparkles(elapsed);
        return;
    }

    char line[48];
    snprintf(line, sizeof(line), Ui::Common::EVOLUTION_CANCELLED_FMT,
             species ? species->name : Ui::Status::MOVE_UNKNOWN);
    drawEvolutionMessage(line);
    PixelRenderer::text(174, 109, Ui::Common::A_CONTINUE,
                        PixelRenderer::rgb(255, 226, 108), 1);
}

void resetMoveLearnState(MoveLearnState& state) {
    state = {};
    state.replacementSlot = 1;
}

bool handleMoveLearnInput(MoveLearnState& state, uint8_t button) {
    auto& engine = GameEngine::ins();
    if (!engine.hasPendingMoveLearn()) return true;

    if (state.stage == MoveLearnStage::PROMPT) {
        if (button == 1) {
            state.promptCursor = (state.promptCursor + 1) % 2;
            return false;
        }
        if (button != 0) return false;
        if (state.promptCursor != 0) {
            engine.resolvePendingMoveLearn(false);
            return true;
        }
        if (engine.pendingMoveLearnNeedsReplacement()) {
            state.stage = MoveLearnStage::REPLACEMENT;
            state.replacementSlot = 1;
            state.confirmOpen = false;
            state.confirmYes = false;
            return false;
        }
        engine.resolvePendingMoveLearn(true);
        return true;
    }

    if (state.confirmOpen) {
        if (button == 1) {
            state.confirmYes = !state.confirmYes;
            return false;
        }
        if (button != 0) return false;
        if (!state.confirmYes) {
            state.confirmOpen = false;
            return false;
        }
        bool replaced = engine.resolvePendingMoveLearnReplacing(
            state.replacementSlot);
        if (!replaced && engine.hasPendingMoveLearn()) {
            state.confirmOpen = false;
            state.confirmYes = false;
            return false;
        }
        return true;
    }

    if (button == 1) {
        state.replacementSlot = state.replacementSlot == MOVE_LEARN_NEW_SLOT
            ? 1
            : state.replacementSlot + 1;
    } else if (button == 0) {
        if (state.replacementSlot == MOVE_LEARN_NEW_SLOT) return false;
        state.confirmOpen = true;
        state.confirmYes = false;
    }
    return false;
}

void renderMoveLearnPrompt(const MoveLearnState& state) {
    auto& canvas = PixelRenderer::canvas();
    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, PixelRenderer::rgb(18, 24, 32));
    canvas.fillRect(24, 26, 192, 86, PixelRenderer::rgb(35, 42, 50));
    canvas.drawRect(24, 26, 192, 86, PixelRenderer::rgb(241, 242, 232));

    const MoveInfo* move = findMove(GameEngine::ins().pendingMoveLearnId());
    PixelRenderer::text(46, 38, Ui::Explore::LEARN_TITLE, PixelRenderer::rgb(255, 216, 72), 1);

    char line[64];
    snprintf(line, sizeof(line), Ui::Explore::LEARN_MOVE_FMT,
             move ? move->name : Ui::Status::MOVE_UNKNOWN);
    PixelRenderer::text(36, 58, line, PixelRenderer::rgb(241, 242, 232), 1);

    const auto& gameState = GameEngine::ins().gameState();
    uint8_t slot = GameEngine::ins().pendingMoveLearnSlot();
    bool fillsSecondSpecialSlot = false;
    if (slot < gameState.teamCount && slot < Game::TEAM_CAP) {
        const Game::MonsterRuntime& mon = gameState.team[slot];
        if (mon.move2Id != 0 && mon.move3Id == 0) {
            fillsSecondSpecialSlot = true;
        }
    }
    if (GameEngine::ins().pendingMoveLearnNeedsReplacement()) {
        PixelRenderer::text(48, 76,
                            Ui::Explore::LEARN_CHOOSE_REPLACEMENT,
                            PixelRenderer::rgb(135, 214, 238), 1);
    } else {
        PixelRenderer::text(64, 76,
                            fillsSecondSpecialSlot ? Ui::Explore::LEARN_EMPTY_SLOT_2
                                                   : Ui::Explore::LEARN_EMPTY_SLOT,
                            PixelRenderer::rgb(135, 214, 238), 1);
    }

    uint16_t yesColor = state.promptCursor == 0
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = state.promptCursor == 1
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(72, 96, Ui::Bag::YES, yesColor, 1);
    PixelRenderer::text(142, 96, Ui::Bag::NO, noColor, 1);
}

void renderMoveLearnReplacementChoice(const MoveLearnState& uiState) {
    auto& canvas = PixelRenderer::canvas();
    const auto& gameState = GameEngine::ins().gameState();
    uint8_t teamSlot = GameEngine::ins().pendingMoveLearnSlot();
    if (teamSlot >= gameState.teamCount || teamSlot >= Game::TEAM_CAP) {
        renderMoveLearnPrompt(uiState);
        return;
    }

    const Game::MonsterRuntime& mon = gameState.team[teamSlot];
    const Species& species = GameEngine::ins().speciesFor(mon);
    static constexpr int LEFT_W = 90;
    static constexpr int RIGHT_X = LEFT_W + 7;
    static constexpr int LIST_Y = 6;
    static constexpr int ROW_H = 24;
    static constexpr int TEXT_X = 7;

    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H,
                    PixelRenderer::rgb(10, 14, 20));
    canvas.drawFastVLine(LEFT_W, 6, Hal::DISPLAY_H - 12,
                        PixelRenderer::rgb(123, 125, 123));

    const MoveInfo* newMove =
        findMove(GameEngine::ins().pendingMoveLearnId());
    for (uint8_t moveSlot = 0;
         moveSlot <= MOVE_LEARN_NEW_SLOT; ++moveSlot) {
        int y = LIST_Y + moveSlot * ROW_H;
        bool selected = moveSlot == uiState.replacementSlot;
        bool pendingMove = moveSlot == MOVE_LEARN_NEW_SLOT;
        uint16_t color = selected
            ? PixelRenderer::rgb(255, 216, 72)
            : (moveSlot == 0
                   ? PixelRenderer::rgb(104, 111, 122)
                   : (pendingMove ? PixelRenderer::rgb(135, 214, 238)
                                  : PixelRenderer::rgb(241, 242, 232)));
        if (selected) {
            canvas.fillRect(2, y + 2, 3, 18,
                            PixelRenderer::rgb(255, 216, 72));
        }
        const MoveInfo* move = pendingMove
            ? newMove
            : moveLearnInfoForSlot(species, mon, moveSlot);
        if (move) {
            PixelRenderer::text(TEXT_X, y + 3, move->name, color, 1);
        }
        if (moveSlot < MOVE_LEARN_NEW_SLOT) {
            canvas.drawFastHLine(5, y + ROW_H - 1, LEFT_W - 10,
                                 PixelRenderer::rgb(55, 63, 76));
        }
    }

    bool showingNewMove = uiState.replacementSlot == MOVE_LEARN_NEW_SLOT;
    const MoveInfo* selectedMove = showingNewMove
        ? newMove
        : moveLearnInfoForSlot(species, mon, uiState.replacementSlot);
    if (selectedMove) {
        int moveNameX = drawMoveLearnTypeBracket(
            RIGHT_X + 2, 7, selectedMove->type);
        PixelRenderer::text(moveNameX + 3, 7, selectedMove->name,
                            PixelRenderer::rgb(241, 242, 232), 1);

        char line[64];
        char power[8];
        char accuracy[8];
        if (selectedMove->power > 0) {
            snprintf(power, sizeof(power), "%u", selectedMove->power);
        } else {
            snprintf(power, sizeof(power), "--");
        }
        if (selectedMove->accuracy > 0) {
            snprintf(accuracy, sizeof(accuracy), "%u",
                     selectedMove->accuracy);
        } else {
            snprintf(accuracy, sizeof(accuracy), "--");
        }
        snprintf(line, sizeof(line), Ui::Team::MOVE_POWER_ACCURACY_FMT,
                 power, accuracy);
        PixelRenderer::text(RIGHT_X + 2, 31, line,
                            PixelRenderer::rgb(255, 216, 72), 1);
        if (showingNewMove) {
            PixelRenderer::text(RIGHT_X + 2, 52,
                                Ui::Team::MOVE_UNLEARNED,
                                PixelRenderer::rgb(135, 214, 238), 1);
        } else {
            PixelRenderer::text(RIGHT_X + 2, 52,
                                Ui::Team::MOVE_PROFICIENCY,
                                PixelRenderer::rgb(135, 214, 238), 1);
            drawMoveLearnProficiencyBar(
                RIGHT_X + 48, 56, 86, 9,
                mon.moveProficiency[uiState.replacementSlot]);
        }

        static constexpr int DESC_X = RIGHT_X + 2;
        static constexpr int DESC_Y = 76;
        static constexpr int DESC_W = Hal::DISPLAY_W - RIGHT_X - 4;
        int descriptionWidth =
            moveLearnTextPixelWidth(selectedMove->description);
        int offset = 0;
        if (descriptionWidth > DESC_W) {
            int travel = descriptionWidth - DESC_W + 18;
            uint32_t phase = (Hal::ins().millis() / 40UL) %
                             static_cast<uint32_t>(travel * 2 + 50);
            if (phase > 25) {
                phase -= 25;
                offset = phase < static_cast<uint32_t>(travel)
                    ? static_cast<int>(phase)
                    : (phase < static_cast<uint32_t>(travel + 25)
                           ? travel
                           : std::max(
                                 0, travel -
                                    static_cast<int>(
                                        phase - travel - 25)));
            }
        }
        canvas.setClipRect(DESC_X, DESC_Y - 1, DESC_W, 18);
        PixelRenderer::text(DESC_X - offset, DESC_Y,
                            selectedMove->description,
                            PixelRenderer::rgb(255, 218, 178), 1);
        canvas.clearClipRect();
    }

    if (!uiState.confirmOpen || !selectedMove || showingNewMove) return;

    static constexpr int POP_X = 18;
    static constexpr int POP_Y = 24;
    static constexpr int POP_W = 204;
    static constexpr int POP_H = 88;
    canvas.fillRect(POP_X, POP_Y, POP_W, POP_H,
                    PixelRenderer::rgb(24, 28, 36));
    canvas.drawRect(POP_X, POP_Y, POP_W, POP_H,
                    PixelRenderer::rgb(241, 242, 232));
    char forgetLine[48];
    snprintf(forgetLine, sizeof(forgetLine),
             Ui::Explore::LEARN_FORGET_FMT, selectedMove->name);
    int forgetX =
        POP_X + (POP_W - moveLearnTextPixelWidth(forgetLine)) / 2;
    PixelRenderer::text(std::max(POP_X + 4, forgetX), POP_Y + 13,
                        forgetLine,
                        PixelRenderer::rgb(241, 242, 232), 1);
    char replaceLine[48];
    snprintf(replaceLine, sizeof(replaceLine),
             Ui::Explore::LEARN_WILL_LEARN_FMT,
             newMove ? newMove->name : Ui::Status::MOVE_UNKNOWN);
    int replaceX =
        POP_X + (POP_W - moveLearnTextPixelWidth(replaceLine)) / 2;
    PixelRenderer::text(std::max(POP_X + 4, replaceX), POP_Y + 34,
                        replaceLine,
                        PixelRenderer::rgb(135, 214, 238), 1);
    PixelRenderer::text(
        POP_X + 67, POP_Y + 61, Ui::Team::YES,
        uiState.confirmYes ? PixelRenderer::rgb(255, 216, 72)
                           : PixelRenderer::rgb(156, 164, 176),
        1);
    PixelRenderer::text(
        POP_X + 139, POP_Y + 61, Ui::Team::NO,
        uiState.confirmYes ? PixelRenderer::rgb(156, 164, 176)
                           : PixelRenderer::rgb(255, 216, 72),
        1);
}

void renderMoveLearn(const MoveLearnState& state) {
    if (state.stage == MoveLearnStage::REPLACEMENT) {
        renderMoveLearnReplacementChoice(state);
    } else {
        renderMoveLearnPrompt(state);
    }
}

void renderMoveReplacement() {
    auto& canvas = PixelRenderer::canvas();
    const uint16_t background = PixelRenderer::rgb(18, 24, 32);
    const uint16_t panel = PixelRenderer::rgb(35, 42, 50);
    const uint16_t border = PixelRenderer::rgb(241, 242, 232);
    const uint16_t accent = PixelRenderer::rgb(255, 216, 72);
    const uint16_t hint = PixelRenderer::rgb(135, 214, 238);

    canvas.fillRect(0, 0, Hal::DISPLAY_W, Hal::DISPLAY_H, background);
    canvas.fillRect(24, 20, 192, 98, panel);
    canvas.drawRect(24, 20, 192, 98, border);
    PixelRenderer::text(72, 28, Ui::Explore::MOVE_REPLACED_TITLE, accent, 1);

    const MoveInfo* oldMove = findMove(GameEngine::ins().pendingMoveReplacementOldId());
    const MoveInfo* newMove = findMove(GameEngine::ins().pendingMoveReplacementNewId());
    char line[64];
    snprintf(line, sizeof(line), Ui::Explore::MOVE_FORGOT_FMT,
             oldMove ? oldMove->name : Ui::Status::MOVE_UNKNOWN);
    PixelRenderer::text(40, 52, line, PixelRenderer::rgb(156, 164, 176), 1);
    snprintf(line, sizeof(line), Ui::Explore::MOVE_LEARNED_FMT,
             newMove ? newMove->name : Ui::Status::MOVE_UNKNOWN);
    PixelRenderer::text(40, 74, line, border, 1);
    PixelRenderer::text(92, 98, Ui::Common::A_CONTINUE, hint, 1);
}

} // namespace ProgressionUi
