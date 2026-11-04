#include "ui/common/ShowerUiContent.h"

#include <cmath>
#include <cstdio>

#include "assets/GameAssets.h"
#include "assets/PokemonSprites.h"
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "game/GameState.h"
#include "presentation/PixelRenderer.h"
#include "platform/api/FlashStorage.h"

namespace Ui {

float ShowerLayout::spriteScale(uint8_t spriteWidth, uint8_t spriteHeight) {
    if (spriteWidth == 0 || spriteHeight == 0) return 0.0f;
    float scale = MathUtil::min(108.0f / spriteWidth, 112.0f / spriteHeight);
    if (scale > 1.4f) scale = 1.4f;
    return scale;
}

int ShowerLayout::monsterBottomY(uint16_t speciesId) const {
    const auto* frame = PokemonSprites::findSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) return centerY + 42;
    const uint8_t spriteWidth = FlashStorage::readByte(&frame->width);
    const uint8_t spriteHeight = FlashStorage::readByte(&frame->height);
    if (spriteWidth == 0 || spriteHeight == 0) return centerY + 42;
    const float scale = spriteScale(spriteWidth, spriteHeight);
    return MathUtil::min(height - 1,
        centerY + static_cast<int>(spriteHeight * scale) / 2);
}

Game::BathInteraction::Layout ShowerLayout::bathLayout(uint16_t speciesId) const {
    Game::BathInteraction::Layout result;
    result.centerX += dx; result.centerY += dy;
    result.left += dx; result.right += dx;
    result.top += dy; result.bottom += dy;
    result.lowerY += dy;
    result.groundY = monsterBottomY(speciesId);
    return result;
}

}  // namespace Ui

namespace Ui::ShowerUiContent {
namespace {
using namespace ShowerTiming;

static_assert(
    static_cast<uint16_t>(GameAssets::Kind::SHOWER_BUBBLE_5) -
        static_cast<uint16_t>(GameAssets::Kind::SHOWER_BUBBLE_0) + 1 == 6,
    "shower bubble assets must stay contiguous");
static_assert(
    static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_2) -
        static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) + 1 ==
        Game::SOAP_VARIANT_COUNT,
    "shower soap assets must stay contiguous");

int textPixelWidth(const char* value) {
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

GameAssets::Kind showerKind(GameAssets::Kind first, uint8_t offset) {
    return static_cast<GameAssets::Kind>(
        static_cast<uint16_t>(first) + offset);
}

float clampFloat(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

void drawAtmosphere(const ShowerFrame& frame, const ShowerLayout& layout);
void drawMonster(const ShowerFrame& frame, const ShowerLayout& layout);
void drawFoam(const ShowerFrame& frame, const ShowerLayout& layout);
void drawWater(const ShowerFrame& frame, const ShowerLayout& layout);
void drawTool(const ShowerFrame& frame, const ShowerLayout& layout);
void drawMenu(const ShowerFrame& frame, const ShowerLayout& layout);
void drawSoapPicker(const ShowerFrame& frame, const ShowerLayout& layout);
void drawHearts(const ShowerFrame& frame, const ShowerLayout& layout);
void drawExitConfirm(const ShowerFrame& frame, const ShowerLayout& layout);
void drawIncomplete(const ShowerFrame& frame, const ShowerLayout& layout);
void drawExpFloats(const ShowerFrame& frame, const ShowerLayout& layout);
void drawToast(const ShowerFrame& frame, const ShowerLayout& layout);

void drawAtmosphere(const ShowerFrame& frame, const ShowerLayout& layout) {
    if (frame.visuals->atmosphereAlpha <= 0.0f) return;
    int centerY = layout.monsterBottomY(frame.speciesId) - 20;
    GameAssets::drawCenteredAlpha(
        GameAssets::Kind::SHOWER_BUBBLE_5,
        layout.centerX, centerY, 1.0f,
        static_cast<uint8_t>(clampFloat(frame.visuals->atmosphereAlpha, 0.0f, 255.0f)));
}

void drawMonster(const ShowerFrame& frame, const ShowerLayout& layout) {
    uint16_t speciesId = frame.speciesId;
    const auto* sprite = PokemonSprites::findSpeciesSprite(
        speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!sprite) return;
    uint32_t nowMs = frame.nowMs;
    bool flip = false;
    if (frame.visuals->wiggleStartMs != 0 && nowMs - frame.visuals->wiggleStartMs < WIGGLE_DURATION_MS) {
        flip = ((nowMs - frame.visuals->wiggleStartMs) / WIGGLE_FLIP_INTERVAL_MS) % 2 == 1;
    } else if (nowMs < frame.visuals->turnUntilMs) {
        flip = frame.visuals->turnFlip;
    }
    uint8_t width = FlashStorage::readByte(&sprite->width);
    uint8_t height = FlashStorage::readByte(&sprite->height);
    if (width == 0 || height == 0) return;
    float scale = ShowerLayout::spriteScale(width, height);
    int drawW = static_cast<int>(width * scale);
    int drawH = static_cast<int>(height * scale);
    int yOffset = 0;
    if (frame.visuals->hopStartMs != 0 && nowMs - frame.visuals->hopStartMs < HOP_DURATION_MS) {
        float progress =
            static_cast<float>(nowMs - frame.visuals->hopStartMs) / HOP_DURATION_MS;
        yOffset = -static_cast<int>(
            frame.visuals->hopHeightPx * 4.0f * progress * (1.0f - progress) + 0.5f);
    }
    PokemonSprites::drawFrameScaled(
        sprite,
        layout.centerX - drawW / 2,
        layout.centerY - drawH / 2 + yOffset,
        scale, flip);
}

void drawFoam(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    for (uint8_t stage = 0; stage < BODY_FOAM_STAGE_COUNT; ++stage) {
        for (const Game::BathInteraction::Foam& bubble : frame.bath->foam) {
            if (!bubble.active || bubble.stage != stage) continue;
            GameAssets::drawCentered(
                showerKind(GameAssets::Kind::SHOWER_BUBBLE_0, bubble.stage),
                static_cast<int>(bubble.x), static_cast<int>(bubble.y), 1.0f);
        }
    }
}

void drawWater(const ShowerFrame& frame, const ShowerLayout& layout) {
    if (frame.mode != ShowerMode::RINSING) return;
    auto& c = PixelRenderer::canvas();
    uint16_t waterColor = PixelRenderer::rgb(74, 190, 232);
    uint16_t highlight = PixelRenderer::rgb(220, 250, 255);
    uint32_t elapsed = frame.nowMs - frame.modeStartedMs;
    PixelRenderer::fillRectAlpha(20 + layout.dx, 0, 143, 121 + layout.dy, waterColor, 58);
    for (uint8_t stream = 0; stream < 12; ++stream) {
        int x = 22 + layout.dx + stream * 12;
        int width = 7 + (stream % 3);
        int phase = static_cast<int>((elapsed / 3 + stream * 23) % 76);
        PixelRenderer::fillRectAlpha(x, phase - 76, width, 58, highlight, 88);
        PixelRenderer::fillRectAlpha(x, phase, width, 58, highlight, 88);
        c.drawFastVLine(x + width / 2, 0, 121 + layout.dy,
                        PixelRenderer::rgb(145, 226, 249));
    }
    for (const ShowerWaterDrop& drop : frame.visuals->water) {
        if (!drop.active) continue;
        int x = static_cast<int>(drop.x);
        int y = static_cast<int>(drop.y);
        c.drawFastVLine(x, y, drop.vy > 180.0f ? 11 : 7, highlight);
        c.drawPixel(x, y, highlight);
    }
    c.fillEllipse(layout.centerX, 122 + layout.dy, 70, 8, waterColor);
    c.drawFastHLine(30 + layout.dx, 119 + layout.dy, 120, highlight);
    GameAssets::drawCentered(
        GameAssets::Kind::SHOWER_SPRINKLER,
        layout.centerX, 19, 1.05f);
}

void drawTool(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    if (frame.mode == ShowerMode::SOAPING) {
        GameAssets::drawCentered(
            showerKind(GameAssets::Kind::SHOWER_SOAP_0, frame.visuals->soapCursor),
            static_cast<int>(frame.visuals->soapX), static_cast<int>(frame.visuals->soapY));
    } else if (frame.mode == ShowerMode::BRUSHING) {
        GameAssets::drawCentered(
            GameAssets::Kind::SHOWER_BRUSH,
            static_cast<int>(frame.visuals->brushX), static_cast<int>(frame.visuals->brushY));
    }
}

void drawMenu(const ShowerFrame& frame, const ShowerLayout& layout) {
    auto& c = PixelRenderer::canvas();
    for (int x = layout.menuGradientX; x < layout.width; ++x) {
        uint8_t alpha = static_cast<uint8_t>(
            (x - layout.menuGradientX) * 190 /
            (layout.width - layout.menuGradientX - 1));
        PixelRenderer::fillRectAlpha(
            x, 0, 1, layout.height, PixelRenderer::rgb(0, 0, 0), alpha);
    }
    c.fillRect(layout.menuIndicatorX, layout.menuCenterY - 8, 3, 16,
               PixelRenderer::rgb(255, 216, 72));
    for (uint8_t i = 0; i < SHOWER_MENU_COUNT; ++i) {
        float rawOffset = static_cast<float>(i) - frame.visuals->menuAnimCursor;
        while (rawOffset > SHOWER_MENU_COUNT / 2.0f) rawOffset -= SHOWER_MENU_COUNT;
        while (rawOffset < -SHOWER_MENU_COUNT / 2.0f) rawOffset += SHOWER_MENU_COUNT;
        int centerY = layout.menuCenterY + static_cast<int>(rawOffset * layout.menuSpacing);
        if (centerY < -24 || centerY > layout.height + 24) continue;
        bool selected =
            (frame.mode == ShowerMode::MENU && fabsf(rawOffset) < 0.5f) ||
            ((frame.mode == ShowerMode::SOAP_SELECT || frame.mode == ShowerMode::SOAPING) && i == 0) ||
            (frame.mode == ShowerMode::BRUSHING && i == 1) ||
            (frame.mode == ShowerMode::RINSING && i == 2);
        float selectedScale = selected ? 1.2f : 1.0f;
        uint16_t color = selected
            ? PixelRenderer::rgb(255, 231, 139)
            : PixelRenderer::rgb(241, 242, 232);
        if (i == 0) {
            GameAssets::drawCentered(
                GameAssets::Kind::SHOWER_MENU_SOAP,
                layout.menuCenterX, centerY, 0.72f * selectedScale);
        } else if (i == 1) {
            GameAssets::drawCentered(
                GameAssets::Kind::SHOWER_MENU_BRUSH,
                layout.menuCenterX, centerY, 0.58f * selectedScale);
        } else if (i == 2) {
            GameAssets::drawCentered(
                GameAssets::Kind::SHOWER_MENU_SPRINKLER,
                layout.menuCenterX, centerY, 0.78f * selectedScale);
        } else {
            int half = static_cast<int>(7.2f * selectedScale);
            c.drawLine(layout.menuCenterX - half, centerY - half,
                       layout.menuCenterX + half, centerY + half, color);
            c.drawLine(layout.menuCenterX + half, centerY - half,
                       layout.menuCenterX - half, centerY + half, color);
        }
    }
}

void drawSoapPicker(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    PixelRenderer::fillRectAlpha(
        7, 96, 166, 34, PixelRenderer::rgb(13, 18, 25), 210);
    GameAssets::drawCentered(
        showerKind(GameAssets::Kind::SHOWER_SOAP_0, frame.visuals->soapCursor),
        27, 113, 0.85f);
    char count[24];
    snprintf(count, sizeof(count), "%s x%u", Ui::Shower::SOAP_NAMES[frame.visuals->soapCursor],
             static_cast<unsigned>(frame.soapCount));
    PixelRenderer::text(48, 105, count, PixelRenderer::rgb(255, 244, 210), 1);
}

void drawHearts(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    auto& c = PixelRenderer::canvas();
    uint16_t color = PixelRenderer::rgb(242, 74, 97);
    uint32_t elapsed = frame.nowMs - frame.modeStartedMs;
    uint8_t count = MathUtil::min<uint8_t>(frame.completionHearts, 3);
    if (count == 0) return;
    int startX = 88 - (count - 1) * 18;
    for (uint8_t i = 0; i < count; ++i) {
        int x = startX + i * 36;
        int y = 23 - static_cast<int>((elapsed / 90 + i * 3) % 6);
        c.fillCircle(x - 3, y, 4, color);
        c.fillCircle(x + 3, y, 4, color);
        c.fillTriangle(x - 7, y + 1, x + 7, y + 1, x, y + 10, color);
    }
}

void drawExitConfirm(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    auto& c = PixelRenderer::canvas();
    static constexpr int POP_X = 25;
    static constexpr int POP_Y = 27;
    static constexpr int POP_W = 190;
    static constexpr int POP_H = 82;
    PixelRenderer::fillRectAlpha(
        POP_X, POP_Y, POP_W, POP_H, PixelRenderer::rgb(18, 22, 29), 235);
    c.drawRect(POP_X, POP_Y, POP_W, POP_H,
               PixelRenderer::rgb(241, 242, 232));
    PixelRenderer::text(POP_X + 39, POP_Y + 12, Ui::Shower::EXIT_FOAM,
                        PixelRenderer::rgb(241, 242, 232), 1);
    PixelRenderer::text(POP_X + 55, POP_Y + 31, Ui::Shower::EXIT_QUESTION,
                        PixelRenderer::rgb(241, 242, 232), 1);
    uint16_t yesColor = frame.visuals->exitConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t noColor = !frame.visuals->exitConfirmYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    PixelRenderer::text(POP_X + 57, POP_Y + 57,
                        Ui::Shower::YES, yesColor, 1);
    PixelRenderer::text(POP_X + 128, POP_Y + 57,
                        Ui::Shower::NO, noColor, 1);
}

void drawIncomplete(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)frame;
    (void)layout;
    PixelRenderer::fillRectAlpha(
        39, 50, 162, 34, PixelRenderer::rgb(13, 18, 25), 235);
    PixelRenderer::text(55, 59, Ui::Shower::INCOMPLETE,
                        PixelRenderer::rgb(241, 242, 232), 1);
}

void drawExpFloats(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    uint32_t nowMs = frame.nowMs;
    for (const ShowerExpFloat& floater : frame.visuals->expFloats) {
        if (!floater.active) continue;
        uint32_t age = nowMs - floater.bornMs;
        if (age >= EXP_FLOAT_DURATION_MS) continue;
        float progress = static_cast<float>(age) / EXP_FLOAT_DURATION_MS;
        int y = static_cast<int>(floater.y - EXP_FLOAT_RISE_PX * progress);
        uint8_t brightness = 255;
        if (age > EXP_FLOAT_FADE_START_MS) {
            float fade = static_cast<float>(age - EXP_FLOAT_FADE_START_MS) /
                         (EXP_FLOAT_DURATION_MS - EXP_FLOAT_FADE_START_MS);
            brightness = static_cast<uint8_t>(255.0f * (1.0f - fade));
        }
        int x = static_cast<int>(floater.x);
        if (floater.heart) {
            uint16_t heartColor = PixelRenderer::rgb(
                static_cast<uint8_t>((242 * brightness) / 255),
                static_cast<uint8_t>((74 * brightness) / 255),
                static_cast<uint8_t>((97 * brightness) / 255));
            auto& c = PixelRenderer::canvas();
            c.fillCircle(x - 1, y, 2, heartColor);
            c.fillCircle(x + 1, y, 2, heartColor);
            c.fillTriangle(x - 3, y + 1, x + 3, y + 1, x, y + 5, heartColor);
            continue;
        }
        uint16_t color = PixelRenderer::rgb(brightness, brightness, brightness);
        PixelRenderer::text(x, y, floater.text, color, 1);
    }
}

void drawToast(const ShowerFrame& frame, const ShowerLayout& layout) {
    (void)layout;
    if (!frame.visuals->toast) return;
    PixelRenderer::fillRectAlpha(
        39, 103, 100, 22, PixelRenderer::rgb(18, 22, 29), 225);
    PixelRenderer::text(55, 106, frame.visuals->toast, PixelRenderer::rgb(255, 232, 150), 1);
}

}  // namespace

void draw(const ShowerFrame& frame, const ShowerLayout& layout,
          DrawControl menu, DrawControl soapPicker) {
    if (!GameAssets::drawBackgroundViewport(GameAssets::Kind::SHOWER_BACKGROUND, 0, 0)) {
        PixelRenderer::clear(PixelRenderer::rgb(21, 36, 30));
    }

    drawMonster(frame, layout);
    drawAtmosphere(frame, layout);
    drawFoam(frame, layout);
    drawWater(frame, layout);
    drawTool(frame, layout);
    (menu ? menu : drawMenu)(frame, layout);
    if (frame.mode == ShowerMode::SOAP_SELECT)
        (soapPicker ? soapPicker : drawSoapPicker)(frame, layout);
    if (frame.mode == ShowerMode::COMPLETE) {
        drawHearts(frame, layout);
        int messageX = (layout.width - textPixelWidth(frame.completionMessage)) / 2;
        if (messageX < 0) messageX = 0;
        PixelRenderer::text(messageX, 92 + layout.dy, frame.completionMessage,
                            PixelRenderer::rgb(241, 242, 232), 1);
    }
    if (frame.mode == ShowerMode::EXIT_CONFIRM) drawExitConfirm(frame, layout);
    if (frame.mode == ShowerMode::INCOMPLETE) drawIncomplete(frame, layout);
    drawExpFloats(frame, layout);
    drawToast(frame, layout);
}


}  // namespace Ui::ShowerUiContent
