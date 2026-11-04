#include "ui/common/ExploreBattleUiContent.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include "assets/PokemonSprites.h"
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "platform/api/FlashStorage.h"
#include "presentation/PixelRenderer.h"

namespace Ui::ExploreBattleUiContent {
namespace {

constexpr int BATTLE_ASCII_ADVANCE = 8;
constexpr uint8_t BATTLE_FOOTER_ALPHA = 153;

constexpr bool validLayout(const ExploreBattleLayout& layout) {
    return layout.expLabelY + layout.expLabelVisualH / 2 ==
               layout.expBarY + layout.expBarH / 2 &&
           layout.wildGroundY - layout.wildMaxH == 0 &&
           layout.playerX - layout.playerMaxW / 2 >= 0 &&
           layout.playerGroundY - layout.playerMaxH == 42 + layout.height - 135;
}
static_assert(validLayout(ExploreBattleLayout(240, 135)),
              "Stick battle bounds and EXP centering must remain unchanged");
static_assert(validLayout(ExploreBattleLayout(296, 240)),
              "Compact battle bounds and EXP centering must remain unchanged");
constexpr ExploreBattleLayout COMPACT_BATTLE_LAYOUT(296, 240, 28, true, 10);
static_assert(COMPACT_BATTLE_LAYOUT.playerGroundY == COMPACT_BATTLE_LAYOUT.footerY,
              "Compact player sprite must end at the footer top");

void drawBattleText(int x, int y, const char* value, uint16_t color) {
    PixelRenderer::textOutlined(
        x, y, value, color, PixelRenderer::rgb(255, 255, 255), 1);
}

void drawBattleDarkText(int x, int y, const char* value) {
    drawBattleText(x, y, value, PixelRenderer::rgb(25, 31, 40));
}

void drawBattleFooterText(int x, int y, const char* value, uint16_t color) {
    PixelRenderer::text(x, y, value, color, 1);
}

void drawBattleFooterDarkText(int x, int y, const char* value) {
    drawBattleFooterText(x, y, value, PixelRenderer::rgb(25, 31, 40));
}

void drawBattleAsciiRightAligned(int rightX, int y, const char* value) {
    drawBattleDarkText(rightX - static_cast<int>(strlen(value)) * BATTLE_ASCII_ADVANCE,
                       y, value);
}

void drawBattleCompactExpLabel(int x, int y, uint16_t color) {
    // Native 8x8 Unscii E/X/P glyphs keep the compact label pixel-aligned.
    static constexpr uint32_t FOREGROUND[8] = {
        0x000000U, 0x3EC37EU, 0x666606U, 0x663C06U,
        0x3E183EU, 0x063C06U, 0x066606U, 0x06C37EU,
    };
    static constexpr uint32_t OUTLINE[10] = {
        0x00000000U, 0x007D86FCU, 0x00824902U, 0x010332F2U,
        0x01028472U, 0x00824882U, 0x00728472U, 0x001332F2U,
        0x00124902U, 0x000D86FCU,
    };
    auto& canvas = PixelRenderer::canvas();
    uint16_t outline = PixelRenderer::rgb(255, 255, 255);
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; col < 26; ++col) {
            if (OUTLINE[row] & (1UL << col)) {
                canvas.drawPixel(x + col - 1, y + row - 1, outline);
            }
        }
    }
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 24; ++col) {
            if (FOREGROUND[row] & (1UL << col)) {
                canvas.drawPixel(x + col, y + row, color);
            }
        }
    }
}

uint16_t panelInk() { return PixelRenderer::rgb(23, 45, 50); }
uint16_t panelPaper() { return PixelRenderer::rgb(243, 235, 207); }
void drawFloatingPanel(const MenuGrid::Rect& r) {
    auto& c = PixelRenderer::canvas();
    c.fillRoundRect(r.x, r.y, r.width, r.height, 5, panelPaper());
    c.drawRoundRect(r.x, r.y, r.width, r.height, 5, PixelRenderer::rgb(61, 89, 94));
}
int textWidth(const char* text) {
    int width = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); p && *p; ++p)
        if ((*p & 0xc0) != 0x80) width += *p < 0x80 ? 8 : 16;
    return width;
}
void drawFloatingLabel(int centerX, int y, const char* text, int maxWidth,
                       uint16_t color, int height = 16) {
    char buffer[96] = {};
    int bytes = 0, width = 0;
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    while (p && *p) {
        const int length = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : 4;
        const int advance = *p < 0x80 ? 8 : height;
        if (width + advance > maxWidth || bytes + length >= int(sizeof(buffer))) break;
        bool complete = true;
        for (int i = 1; i < length; ++i) if (!p[i]) { complete = false; break; }
        if (!complete) break;
        memcpy(buffer + bytes, p, length);
        bytes += length; width += advance; p += length;
    }
    if (p && *p && width + 8 <= maxWidth && bytes + 1 < int(sizeof(buffer))) {
        buffer[bytes++] = '.'; width += 8;
    }
    int x = centerX - width / 2;
    for (const auto* glyph = reinterpret_cast<const unsigned char*>(buffer); *glyph;) {
        const int length = *glyph < 0x80 ? 1 : (*glyph & 0xe0) == 0xc0 ? 2 : (*glyph & 0xf0) == 0xe0 ? 3 : 4;
        char cell[5] = {};
        memcpy(cell, glyph, length);
        const bool pixel = *glyph < 0x80 || strcmp(cell, "\xe2\x99\x80") == 0 ||
                           strcmp(cell, "\xe2\x99\x82") == 0;
        if (pixel) PixelRenderer::text(x, y, cell, color);
        else PixelRenderer::textSmooth(x, y, cell, color, height);
        x += *glyph < 0x80 ? 8 : height;
        glyph += length;
    }
}
void drawHudName(const ExploreBattleHudLayout& hud, const char* name,
                 const char* level, bool floating) {
    if (!floating) {
        drawBattleDarkText(hud.nameX, hud.nameY, name);
        drawBattleAsciiRightAligned(hud.levelRight, hud.nameY, level);
        return;
    }
    drawFloatingPanel(hud.panel);
    const int space = hud.levelRight - textWidth(level) - 8 - hud.nameX;
    // Reserve the level column even for the longest species names.
    drawFloatingLabel(hud.nameX + MathUtil::min(space, textWidth(name)) / 2,
                      hud.nameY, name, space, panelInk());
    PixelRenderer::text(hud.levelRight - textWidth(level), hud.nameY, level, panelInk());
}
void drawFloatingMessage(const ExploreBattleFrame& frame, MenuGrid::Rect panel) {
    if (!frame.battleLogVisibleCount && !frame.defeatAwaitInput) return;
    // Keep the newest message complete. Older context shares the second line
    // only when the new message fits on one line; playback timing is untouched.
    char lines[3][64] = {};
    unsigned count = 1, used = 0;
    int width = 0;
    const char* newest = frame.battleLogVisibleCount
        ? frame.battleLogVisible[frame.battleLogVisibleCount - 1] : Ui::Explore::ANY_KEY_RETURN;
    const auto* p = reinterpret_cast<const unsigned char*>(newest);
    while (p && *p && count <= 3) {
        unsigned length = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : 4;
        const int glyphWidth = *p < 0x80 ? 8 : 16;
        if (width + glyphWidth > panel.width - 16 || used + length >= sizeof(lines[0])) {
            if (count == 3) break;
            ++count; used = 0; width = 0;
        }
        // Scene logs can end at the fixed buffer limit; never read past NUL.
        bool complete = true;
        for (unsigned i = 1; i < length; ++i) if (!p[i]) { complete = false; break; }
        if (!complete) break;
        memcpy(lines[count - 1] + used, p, length);
        used += length; width += glyphWidth; p += length;
    }
    if (count == 1 && frame.battleLogVisibleCount > 1) {
        memcpy(lines[1], lines[0], sizeof(lines[0]));
        snprintf(lines[0], sizeof(lines[0]), "%s", frame.battleLogVisible[0]);
        count = 2;
    }
    const int extra = count > 2 ? 18 : 0;
    panel.y -= extra; panel.height += extra;
    drawFloatingPanel(panel);
    for (unsigned i = 0; i < count; ++i)
        drawFloatingLabel(panel.x + panel.width / 2,
            panel.y + (panel.height - int(count) * 18) / 2 + int(i) * 18,
            lines[i], panel.width - 16, panelInk());
}

void drawBattleConditionEffects(int centerX, int groundY,
                                Game::MajorStatus majorStatus,
                                const BattleSystem::BattleActorState& battleState,
                                uint32_t nowMs) {
    auto& c = PixelRenderer::canvas();
    const uint8_t pulse = static_cast<uint8_t>((nowMs / 180U) % 4U);
    const uint16_t outline = PixelRenderer::rgb(48, 45, 55);

    switch (majorStatus) {
    case Game::MajorStatus::POISON:
    case Game::MajorStatus::TOXIC: {
        const uint16_t color = majorStatus == Game::MajorStatus::TOXIC
            ? PixelRenderer::rgb(151, 68, 190)
            : PixelRenderer::rgb(185, 91, 204);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 16 + i * 15;
            int y = groundY - 8 - ((pulse + i) % 4) * 4;
            int radius = 2 + ((pulse + i) & 1);
            c.fillCircle(x, y, radius + 1, outline);
            c.fillCircle(x, y, radius, color);
        }
        break;
    }
    case Game::MajorStatus::PARALYSIS: {
        const uint16_t color = PixelRenderer::rgb(255, 215, 55);
        const int jitter = (pulse & 1) ? 2 : 0;
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * (23 + jitter);
            int y = groundY - 35 + (side > 0 ? 5 : 0);
            c.drawLine(x, y, x - side * 5, y + 6, outline);
            c.drawLine(x - side * 5, y + 6, x + side * 1, y + 6, outline);
            c.drawLine(x + side * 1, y + 6, x - side * 4, y + 13, outline);
            c.drawLine(x + side, y, x - side * 4, y + 6, color);
            c.drawLine(x - side * 4, y + 6, x + side * 2, y + 6, color);
            c.drawLine(x + side * 2, y + 6, x - side * 3, y + 13, color);
        }
        break;
    }
    case Game::MajorStatus::SLEEP: {
        const uint16_t color = PixelRenderer::rgb(116, 169, 232);
        int rise = static_cast<int>((nowMs / 240U) % 3U);
        PixelRenderer::textOutlined(centerX + 18, groundY - 45 - rise * 2,
                                    "Z", color, outline, 1);
        PixelRenderer::textOutlined(centerX + 28, groundY - 55 - rise * 2,
                                    "Z", color, outline, 1);
        break;
    }
    case Game::MajorStatus::BURN: {
        const int sway = (pulse & 1) ? 2 : -1;
        const uint16_t red = PixelRenderer::rgb(226, 76, 42);
        const uint16_t yellow = PixelRenderer::rgb(255, 191, 46);
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * 19;
            int y = groundY - 10;
            c.fillTriangle(x - 5, y + 7, x + 5, y + 7,
                           x + sway * side, y - 7, outline);
            c.fillTriangle(x - 4, y + 6, x + 4, y + 6,
                           x + sway * side, y - 6, red);
            c.fillTriangle(x - 2, y + 5, x + 2, y + 5,
                           x - sway * side, y, yellow);
        }
        break;
    }
    case Game::MajorStatus::FREEZE: {
        const uint16_t color = PixelRenderer::rgb(116, 219, 239);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 20 + i * 20;
            int y = groundY - 13 - ((pulse + i) & 1) * 5;
            c.drawLine(x - 4, y, x + 4, y, outline);
            c.drawLine(x, y - 4, x, y + 4, outline);
            c.drawLine(x - 3, y - 3, x + 3, y + 3, outline);
            c.drawLine(x - 3, y + 3, x + 3, y - 3, outline);
            c.drawLine(x - 3, y, x + 3, y, color);
            c.drawLine(x, y - 3, x, y + 3, color);
        }
        break;
    }
    default:
        break;
    }

    if (battleState.confusionTurns > 0) {
        const uint16_t colors[] = {
            PixelRenderer::rgb(255, 215, 55),
            PixelRenderer::rgb(239, 119, 116),
            PixelRenderer::rgb(111, 211, 221),
        };
        float angle = nowMs * 0.006f;
        for (uint8_t i = 0; i < 3; ++i) {
            float pointAngle = angle + i * 2.0943951f;
            int x = centerX + static_cast<int>(roundf(cosf(pointAngle) * 17.0f));
            int y = groundY - 50 + static_cast<int>(roundf(sinf(pointAngle) * 4.0f));
            c.fillCircle(x, y, 3, outline);
            c.fillCircle(x, y, 2, colors[i]);
        }
    }

    if (battleState.bindTurns > 0) {
        const uint16_t color = PixelRenderer::rgb(205, 154, 86);
        int y = groundY - 22 + ((pulse & 1) ? 1 : -1);
        c.drawFastHLine(centerX - 24, y, 48, outline);
        c.drawFastHLine(centerX - 24, y + 5, 48, outline);
        c.drawFastHLine(centerX - 23, y + 1, 46, color);
        c.drawFastHLine(centerX - 23, y + 4, 46, color);
        c.drawLine(centerX - 24, y, centerX - 20, y + 5, color);
        c.drawLine(centerX + 23, y, centerX + 19, y + 5, color);
    }

    if (battleState.yawnTurns > 0 && majorStatus != Game::MajorStatus::SLEEP) {
        const uint16_t color = PixelRenderer::rgb(181, 202, 225);
        int drift = static_cast<int>((nowMs / 260U) % 3U);
        for (uint8_t i = 0; i < 3; ++i) {
            c.fillCircle(centerX + 15 + i * 6,
                         groundY - 45 - i * 2 - drift,
                         2, outline);
            c.fillCircle(centerX + 15 + i * 6,
                         groundY - 45 - i * 2 - drift,
                         1, color);
        }
    }
}

struct BattleMoveVisual {
    GameAssets::BattleFx fx = GameAssets::BattleFx::HIT;
    uint8_t frame = 0;
    bool hit = false, status = false;
    int playerOffsetX = 0, wildOffsetX = 0;
};

BattleMoveVisual battleMoveVisual(const ExploreBattleFrame& frame,
                                  const ExploreBattleLayout& layout) {
    BattleMoveVisual visual;
    if (!layout.moveEffects) return visual;
    const auto* move = findMove(frame.battleMoveId);
    if (move && !frame.battleActionSelfHit)
        visual.fx = GameAssets::battleFxForMove(move->id, move->type);
    const uint32_t elapsed = frame.nowMs - frame.battleActionStarted;
    if (elapsed >= 480) return visual;
    visual.status = frame.encounterPhase && frame.battleDefenderStatChanged &&
                    GameAssets::battleFxIsStatus(visual.fx);
    if (!frame.battleTurnAnimating && !visual.status) return visual;
    // Match the AMOLED six-step attack, with four asset frames at steps 2..5.
    visual.frame = static_cast<uint8_t>(elapsed / 80U + 1U);
    visual.hit = frame.battleTurnAnimating && frame.battleHpFrom > frame.battleHpTo;
    const float scale = layout.width / 368.0f;
    if (!GameAssets::battleFxIsStatus(visual.fx) && !frame.battleActionSelfHit) {
        static constexpr int LUNGE[] = {0, 12, 26, 38, 26, 12, 0};
        const int lunge = static_cast<int>(roundf(LUNGE[visual.frame] * scale));
        if (frame.battleActionAttackerWild) visual.wildOffsetX -= lunge;
        else visual.playerOffsetX += lunge;
    }
    if (visual.hit && visual.frame >= 3 && visual.frame <= 5) {
        const int shake = static_cast<int>(roundf(
            ((visual.frame & 1U) ? -8 : 8) * scale));
        const bool targetWild = frame.battleActionSelfHit
            ? frame.battleActionAttackerWild : !frame.battleActionAttackerWild;
        if (targetWild) visual.wildOffsetX += shake;
        else visual.playerOffsetX += shake;
    }
    return visual;
}

void drawMoveEffects(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout,
                     const BattleMoveVisual& visual, const MenuGrid::Rect& player,
                     const MenuGrid::Rect& wild) {
    if (visual.frame < 2 || visual.frame > 5 || player.width == 0 || wild.width == 0) return;
    const uint8_t fxFrame = visual.frame - 2;
    const float scale = layout.width / 368.0f;
    if (visual.status) {
        const auto& from = frame.battleActionAttackerWild ? wild : player;
        const auto& to = frame.battleActionAttackerWild ? player : wild;
        const int direction = frame.battleActionAttackerWild ? -1 : 1;
        const int sourceX = from.x + from.width / 2 + direction * from.width / 5;
        const int sourceY = from.y + from.height / 3;
        int x = sourceX, y = sourceY;
        float fxScale = 0.85f;
        if (visual.fx != GameAssets::BattleFx::LEER) {
            const int progress = (fxFrame + 1) * 25;
            const int targetX = to.x + to.width / 2;
            const int targetY = to.y + to.height / 2;
            const int startY = visual.fx == GameAssets::BattleFx::SAND_ATTACK
                ? from.y + from.height - static_cast<int>(roundf(12 * scale)) : sourceY;
            const int middleX = static_cast<int>(roundf(156 * scale));
            const int middleY = MathUtil::min(from.y, to.y) +
                MathUtil::min(from.height, to.height) * 2 / 3;
            if (progress <= 50) {
                x = sourceX + (middleX - sourceX) * progress / 50;
                y = startY + (middleY - startY) * progress / 50;
            } else {
                x = middleX + (targetX - middleX) * (progress - 50) / 50;
                y = middleY + (targetY - middleY) * (progress - 50) / 50;
            }
            fxScale = 0.65f + fxFrame * 0.18f;
        }
        GameAssets::drawCentered(GameAssets::battleFxKind(visual.fx, fxFrame),
                                 x, y, fxScale * scale);
    }
    if (!visual.hit) return;
    const bool targetWild = frame.battleActionSelfHit
        ? frame.battleActionAttackerWild : !frame.battleActionAttackerWild;
    const auto& target = targetWild ? wild : player;
    const int x = target.x + target.width / 2;
    const int y = target.y + target.height / 2;
    if (GameAssets::drawCentered(GameAssets::battleFxKind(visual.fx, fxFrame),
                                 x, y, 1.35f * scale)) return;
    // Keep impact feedback if an older device resource pack lacks the FX frames.
    auto& canvas = PixelRenderer::canvas();
    const uint16_t outline = PixelRenderer::rgb(43, 39, 44);
    const uint16_t flash = fxFrame == 0 ? PixelRenderer::rgb(248, 210, 105)
                                      : PixelRenderer::rgb(239, 143, 148);
    const int radius = static_cast<int>(roundf((fxFrame == 0 ? 20 : 30) * scale));
    const int ray = static_cast<int>(roundf(10 * scale));
    canvas.drawCircle(x, y, radius + 3, outline);
    canvas.drawCircle(x, y, radius, flash);
    canvas.drawLine(x - radius - ray, y, x - radius + 2, y, flash);
    canvas.drawLine(x + radius - 2, y, x + radius + ray, y, flash);
    canvas.drawLine(x, y - radius - ray, x, y - radius + 2, flash);
    canvas.drawLine(x, y + radius - 2, x, y + radius + ray, flash);
}

}  // namespace

using ExploreBattleTiming::EXP_ANIMATION_MS;
using ExploreBattleTiming::BATTLE_HIT_DELAY_MS;
using ExploreBattleTiming::BATTLE_HIT_SHAKE_MS;
using ExploreBattleTiming::BATTLE_HP_DRAIN_MS;
using ExploreBattleTiming::BATTLE_ACTION_MS;
using ExploreBattleTiming::BATTLE_SWITCH_PHASE_MS;
using ExploreBattleTiming::FOOD_THROW_DURATION_MS;
using ExploreBattleTiming::BATTLE_SWITCH_TRAVEL_X;

void drawEncounter(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout) {
    auto& c = PixelRenderer::canvas();
    if (!GameAssets::drawBattleBackground(frame.background)) {
        c.fillRect(0, 0, layout.width, layout.height, PixelRenderer::rgb(197, 220, 192));
        c.fillRect(0, 78 + (layout.height - 135) / 2, layout.width, 24 + (layout.height - 135) / 2, PixelRenderer::rgb(228, 225, 190));
    }

    uint32_t nowMs = frame.nowMs;
    const auto visual = battleMoveVisual(frame, layout);
    MenuGrid::Rect playerBounds{}, wildBounds{};
    const int wildOffsetX = layout.moveEffects ? visual.wildOffsetX
        : battleHitShakeX(frame, true, nowMs);
    if (frame.wildSpecies) {
        drawMonsterSprite(*frame.wildSpecies,
                          layout.wildX, layout.wildGroundY,
                          layout.wildMaxW, layout.wildMaxH,
                          false, wildOffsetX, &wildBounds);
        if (frame.wildHp > 0) {
            drawBattleConditionEffects(layout.wildX + wildOffsetX,
                                       layout.wildGroundY,
                                       frame.wildStatus,
                                       frame.wildBattleState, nowMs);
        }
    }
    int playerOffsetX =
        (layout.moveEffects ? visual.playerOffsetX : battleHitShakeX(frame, false, nowMs)) +
        battleSwitchOffsetX(frame, nowMs);
    drawMonsterSprite(*frame.playerSpecies,
                      layout.playerX, layout.playerGroundY,
                      layout.playerMaxW, layout.playerMaxH,
                      true, playerOffsetX, &playerBounds);
    if (frame.playerHp > 0) {
        drawBattleConditionEffects(layout.playerX + playerOffsetX,
                                   layout.playerGroundY,
                                   frame.playerStatus,
                                   frame.playerBattleState, nowMs);
    }
    drawFoodThrow(frame, layout);
    if (layout.moveEffects) drawMoveEffects(frame, layout, visual, playerBounds, wildBounds);
    drawBattleHud(frame, layout);
    drawCommandBox(frame, layout);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (frame.drawBounds) {
        c.drawRect(layout.wildX - layout.wildMaxW / 2,
                   layout.wildGroundY - layout.wildMaxH,
                   layout.wildMaxW, layout.wildMaxH,
                   PixelRenderer::rgb(255, 0, 0));
        c.drawRect(layout.playerX - layout.playerMaxW / 2,
                   layout.playerGroundY - layout.playerMaxH,
                   layout.playerMaxW, layout.playerMaxH,
                   PixelRenderer::rgb(0, 220, 255));
    }
#endif
}

void drawFoodThrow(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout) {
    if (!frame.foodThrowActive) return;

    const int FOOD_THROW_START_X = layout.foodStartX;
    const int FOOD_THROW_START_Y = layout.foodStartY;
    const int FOOD_THROW_END_X = layout.foodEndX;
    const int FOOD_THROW_END_Y = layout.foodEndY;
    constexpr int FOOD_THROW_ARC_HEIGHT = 34;
    uint32_t elapsed = frame.nowMs - frame.foodThrowStarted;
    float progress = MathUtil::min<uint32_t>(elapsed, FOOD_THROW_DURATION_MS) /
                     static_cast<float>(FOOD_THROW_DURATION_MS);
    float inverse = 1.0f - progress;
    float x = FOOD_THROW_START_X +
              (FOOD_THROW_END_X - FOOD_THROW_START_X) * progress;
    float y = FOOD_THROW_START_Y +
              (FOOD_THROW_END_Y - FOOD_THROW_START_Y) * progress -
              FOOD_THROW_ARC_HEIGHT * 4.0f * progress * inverse;
    GameAssets::Kind kind = GameAssets::itemKind(
        Game::itemIdForFoodIndex(frame.foodThrowIndex));
    if (!GameAssets::drawCentered(
            kind, static_cast<int>(roundf(x)), static_cast<int>(roundf(y)),
            0.55f)) {
        PixelRenderer::canvas().fillCircle(
            static_cast<int>(roundf(x)), static_cast<int>(roundf(y)), 4,
            PixelRenderer::rgb(240, 174, 76));
    }
}

void drawFriendshipPrompt(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout) {
    if (!frame.friendshipPromptVisible) {
        return;
    }

    auto& c = PixelRenderer::canvas();
    const auto panel = layout.friendshipPrompt();
    const int PANEL_X = panel.x, PANEL_Y = panel.y;
    const int PANEL_W = panel.width, PANEL_H = panel.height;
    const int ROW_H = layout.friendshipRowHeight;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(20, 25, 32));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(190, 200, 205));

    static constexpr const char* ITEMS[] = {
        Ui::Explore::FRIEND_YES,
        Ui::Explore::FRIEND_NO,
    };
    for (uint8_t i = 0; i < 2; ++i) {
        bool selected = frame.friendshipConfirmYes == (i == 0);
        int y = PANEL_Y + 7 + i * ROW_H + (ROW_H - 28) / 2;
        if (selected) {
            c.fillRect(PANEL_X + 5, y, 3, 18,
                       PixelRenderer::rgb(255, 216, 72));
        }
        const auto color = selected ? PixelRenderer::rgb(255, 216, 72)
                                    : PixelRenderer::rgb(235, 239, 232);
        if (layout.floatingControls)
            drawFloatingLabel(PANEL_X + 17 + textWidth(ITEMS[i]) / 2, y,
                              ITEMS[i], PANEL_W - 34, color);
        else PixelRenderer::text(PANEL_X + 17, y, ITEMS[i], color, 1);
    }
}

void drawMonsterSprite(const Species& species, int x, int groundY,
                                     int maxWidth, int maxHeight, bool back,
                                     int spriteOffsetX, MenuGrid::Rect* drawnBounds) {
    const PokemonSprites::SpriteFrame* frame = PokemonSprites::findSpeciesSprite(
        species.id, back ? PokemonSprites::SpriteKind::BACK : PokemonSprites::SpriteKind::FRONT);
    if (!frame) return;

    uint8_t w = FlashStorage::readByte(&frame->width);
    uint8_t h = FlashStorage::readByte(&frame->height);
    float scale = 1.0f;
    if (w * scale > maxWidth) scale = static_cast<float>(maxWidth) / w;
    if (h * scale > maxHeight) scale = static_cast<float>(maxHeight) / h;

    int drawW = MathUtil::max<int>(1, static_cast<int>(roundf(w * scale)));
    int drawH = MathUtil::max<int>(1, static_cast<int>(roundf(h * scale)));
    int drawX = x - drawW / 2 + spriteOffsetX;
    int drawY = groundY - drawH;
    if (drawnBounds) *drawnBounds = {drawX, drawY, drawW, drawH};

    if (scale < 0.999f || scale > 1.001f) {
        PokemonSprites::drawFrameScaled(frame, drawX, drawY, scale);
    } else {
        PokemonSprites::drawFrame(frame, drawX, drawY);
    }
}

void drawBattleHud(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout) {
    auto& c = PixelRenderer::canvas();
    char buf[24];
    auto drawHpWithStatus = [](int statusX, int statusY, int hpX, int hpY, int hpWidth,
                               uint16_t hp, uint16_t hpMax, Game::MajorStatus status) {
        GameAssets::Kind statusAsset = GameAssets::statusKind(status);
        if (statusAsset != GameAssets::Kind::COUNT) {
            GameAssets::draw(statusAsset, statusX, statusY);
        }
        PixelRenderer::bar(hpX, hpY, hpWidth, 6,
                           hpMax ? (hp * 100 / hpMax) : 0,
                           PixelRenderer::rgb(92, 222, 112),
                           PixelRenderer::rgb(59, 70, 59));
    };

    if (frame.wildSpecies) {
        uint16_t shownWildHp = battleHpForRender(frame, true, frame.wildHp, frame.nowMs);
        const auto& hud = layout.wildHud;
        snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, frame.wildLevel);
        drawHudName(hud, frame.wildSpecies->name, buf, layout.floatingControls);
        drawHpWithStatus(hud.statusX, hud.statusY, hud.hpX, hud.hpY, hud.hpWidth,
                         shownWildHp, frame.wildHpMax, frame.wildStatus);
    }

    const Species& species = *frame.playerSpecies;
    bool showVictoryExp = frame.wildSpecies && frame.wildHp == 0 && frame.battleResultPending &&
                          frame.battleExpVisible;
    bool holdPreRewardLevel = frame.wildSpecies && frame.wildHp == 0 && frame.battleResultPending &&
                              frame.expAnimationPending;
    uint32_t shownExp = showVictoryExp
        ? battleExpForRender(frame, frame.nowMs)
        : (holdPreRewardLevel ? frame.expAnimationFrom : frame.playerExp);
    uint8_t shownLevel = (showVictoryExp || holdPreRewardLevel)
        ? levelForExp(species.growthRate, shownExp)
        : frame.playerLevel;

    const auto& hud = layout.playerHud;
    snprintf(buf, sizeof(buf), Ui::Common::LEVEL_FMT, shownLevel);
    drawHudName(hud, species.name, buf, layout.floatingControls);

    if (!showVictoryExp) {
        uint16_t shownPlayerHp = battleHpForRender(
            frame, false, frame.playerHp, frame.nowMs);
        drawHpWithStatus(hud.statusX, hud.statusY, hud.hpX, hud.hpY, hud.hpWidth,
                         shownPlayerHp, frame.playerHpMax,
                         frame.playerStatus);
        return;
    }

    uint32_t levelFloor = minimumExpForLevel(species.growthRate, shownLevel);
    uint32_t levelCeiling = shownLevel >= Game::LEVEL_MAX
        ? levelFloor
        : minimumExpForLevel(species.growthRate, shownLevel + 1);
    uint8_t expPercent = shownLevel >= Game::LEVEL_MAX
        ? 100
        : static_cast<uint8_t>(MathUtil::min<uint32_t>(
            100,
            (shownExp - levelFloor) * 100UL / MathUtil::max<uint32_t>(1, levelCeiling - levelFloor)));
    drawBattleCompactExpLabel(
        layout.expLabelX, layout.expLabelY,
        PixelRenderer::rgb(35, 86, 126));
    c.fillRect(layout.expBarX, layout.expBarY,
               layout.expBarW, layout.expBarH,
               PixelRenderer::rgb(38, 51, 59));
    c.fillRect(layout.expBarX + 1, layout.expBarY + 1,
               layout.expBarW - 2, layout.expBarH - 2,
               PixelRenderer::rgb(184, 198, 194));
    int expFill = (layout.expBarW - 2) * expPercent / 100;
    if (expFill > 0) {
        c.fillRect(layout.expBarX + 1, layout.expBarY + 1,
                   expFill, layout.expBarH - 2,
                   PixelRenderer::rgb(35, 118, 184));
    }
}

void drawCommandBox(const ExploreBattleFrame& frame, const ExploreBattleLayout& layout) {
    auto& c = PixelRenderer::canvas();
    if (layout.floatingControls) {
        if (frame.battleLogActive || frame.battleLogVisibleCount > 0 ||
            (frame.encounterPhase && frame.defeatAwaitInput)) {
            drawFloatingMessage(frame, layout.message);
            return;
        }
        if (!frame.encounterPhase || !frame.battleTurnIdle ||
            frame.battleSwitchStage != BattleSwitchVisualStage::NONE ||
            frame.battleResultPending || frame.expAnimationPending ||
            frame.expAnimationActive || frame.foodThrowActive) return;
        static constexpr const char* ITEMS[] = {
            Ui::Explore::CMD_BATTLE, Ui::Explore::CMD_BAG,
            Ui::Explore::CMD_SWITCH, Ui::Explore::CMD_FLEE,
        };
        for (int i = 0; i < 4; ++i) {
            const auto r = layout.commandTarget(i);
            const bool selected = frame.battleCursor == i;
            c.fillRoundRect(r.x, r.y, r.width, r.height, 5,
                selected ? panelPaper() : PixelRenderer::rgb(37, 65, 71));
            c.drawRoundRect(r.x, r.y, r.width, r.height, 5,
                selected ? PixelRenderer::rgb(255, 246, 219) : PixelRenderer::rgb(85, 114, 121));
            drawFloatingLabel(r.x + r.width / 2, r.y + (r.height - 20) / 2,
                ITEMS[i], r.width - 8, selected ? panelInk() : PixelRenderer::rgb(247, 246, 233), 20);
        }
        return;
    }
    PixelRenderer::fillRectAlpha(
        0, layout.footerY, layout.width, layout.footerH,
        PixelRenderer::rgb(204, 204, 204), BATTLE_FOOTER_ALPHA);
    c.drawRect(0, layout.footerY, layout.width, layout.footerH,
               PixelRenderer::rgb(74, 91, 75));

    if (frame.battleLogActive || frame.battleLogVisibleCount > 0) {
        for (uint8_t i = 0; i < frame.battleLogVisibleCount; ++i) {
            drawBattleFooterDarkText(12, layout.footerY + (layout.footerH - 32) / 2 + i * 16, frame.battleLogVisible[i]);
        }
        return;
    }
    if (!frame.encounterPhase) return;
    if (frame.defeatAwaitInput) {
        drawBattleFooterDarkText((layout.width - 96) / 2, layout.footerY + (layout.footerH - 16) / 2, Ui::Explore::ANY_KEY_RETURN);
        return;
    }
    if (!frame.battleTurnIdle ||
        frame.battleSwitchStage != BattleSwitchVisualStage::NONE) {
        return;
    }

    const int commandY = layout.footerY + (layout.footerH - 16) / 2;
    static constexpr const char* items[] = {
        Ui::Explore::CMD_BATTLE,
        Ui::Explore::CMD_BAG,
        Ui::Explore::CMD_SWITCH,
        Ui::Explore::CMD_FLEE,
    };
    for (uint8_t i = 0; i < 4; ++i) {
        if (frame.battleCursor == i) {
            drawBattleFooterDarkText(layout.commandTextX(i) - 10, commandY, ">");
        }
        drawBattleFooterDarkText(layout.commandTextX(i), commandY, items[i]);
    }
}

int battleSwitchOffsetX(const ExploreBattleFrame& frame, uint32_t nowMs) {
    if (frame.battleSwitchStage == BattleSwitchVisualStage::NONE) return 0;
    float progress = MathUtil::min<uint32_t>(
        BATTLE_SWITCH_PHASE_MS, nowMs - frame.battleSwitchStarted) /
        static_cast<float>(BATTLE_SWITCH_PHASE_MS);
    if (frame.battleSwitchStage == BattleSwitchVisualStage::RETREATING) {
        return -static_cast<int>(
            roundf(BATTLE_SWITCH_TRAVEL_X * progress * progress));
    }
    float remaining = 1.0f - progress;
    return -static_cast<int>(
        roundf(BATTLE_SWITCH_TRAVEL_X * remaining * remaining));
}

uint16_t battleHpForRender(const ExploreBattleFrame& frame, bool wildSide,
                          uint16_t currentHp, uint32_t nowMs) {
    bool targetIsWild = frame.battleActionSelfHit
        ? frame.battleActionAttackerWild
        : !frame.battleActionAttackerWild;
    if (!frame.battleTurnAnimating || wildSide != targetIsWild) {
        return currentHp;
    }
    uint32_t elapsed = nowMs - frame.battleActionStarted;
    uint32_t drainStart = BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS;
    if (elapsed <= drainStart) return frame.battleHpFrom;
    if (elapsed >= BATTLE_ACTION_MS) return frame.battleHpTo;
    uint32_t drainElapsed = elapsed - drainStart;
    uint32_t drained = static_cast<uint32_t>(frame.battleHpFrom - frame.battleHpTo) * drainElapsed /
                       BATTLE_HP_DRAIN_MS;
    return frame.battleHpFrom - drained;
}

int battleHitShakeX(const ExploreBattleFrame& frame, bool wildSide, uint32_t nowMs) {
    bool targetIsWild = frame.battleActionSelfHit
        ? frame.battleActionAttackerWild
        : !frame.battleActionAttackerWild;
    if (!frame.battleTurnAnimating || wildSide != targetIsWild) return 0;
    uint32_t elapsed = nowMs - frame.battleActionStarted;
    if (elapsed < BATTLE_HIT_DELAY_MS ||
        elapsed >= BATTLE_HIT_DELAY_MS + BATTLE_HIT_SHAKE_MS) {
        return 0;
    }
    static constexpr int8_t OFFSETS[] = {-3, 4, -4, 3, -2, 2, 0};
    uint32_t local = elapsed - BATTLE_HIT_DELAY_MS;
    size_t index = MathUtil::min<size_t>(sizeof(OFFSETS) - 1,
                               local * sizeof(OFFSETS) / BATTLE_HIT_SHAKE_MS);
    return OFFSETS[index];
}

uint32_t battleExpForRender(const ExploreBattleFrame& frame, uint32_t nowMs) {
    if (frame.expAnimationPending) return frame.expAnimationFrom;
    if (!frame.expAnimationActive || nowMs <= frame.expAnimationStarted) {
        return frame.expAnimationActive ? frame.expAnimationFrom : frame.playerExp;
    }
    uint32_t elapsed = MathUtil::min<uint32_t>(EXP_ANIMATION_MS, nowMs - frame.expAnimationStarted);
    uint64_t distance = static_cast<uint64_t>(frame.expAnimationTo - frame.expAnimationFrom) * elapsed;
    return frame.expAnimationFrom + static_cast<uint32_t>(distance / EXP_ANIMATION_MS);
}

}  // namespace Ui::ExploreBattleUiContent
