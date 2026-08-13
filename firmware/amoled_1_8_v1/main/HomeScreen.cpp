#include "HomeScreen.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "assets/PokemonSprites.h"
#include "assets/GameAssets.h"
#include "assets/MenuAssets.h"
#include "core/AppSceneFlow.h"
#include "core/RoomRenderer.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreRouteGeometry.h"
#include "game/HomeHud.h"
#include "game/ItemInventory.h"
#include "game/Species.h"
#include "platform/api/FlashStorage.h"
#include "presentation/Canvas565.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"

namespace AmoledV1 {
namespace {

constexpr int HEADER_HEIGHT = HOME_HEADER_HEIGHT;
constexpr int MENU_BUTTON_X = 158;
constexpr int HEADER_BUTTON_WIDTH = 26;
constexpr int HOME_LOCK_BUTTON_X = 6;
constexpr int HOME_MENU_BUTTON_X = 44;
constexpr int HOME_HUD_BUTTON_Y = 182;
constexpr int HOME_HUD_BUTTON_SIZE = 32;
constexpr int HOME_MONSTER_PANEL_X = 96;
constexpr int HOME_MONSTER_PANEL_W = 82;
constexpr int MENU_CONTENT_TOP = 28;
constexpr int MENU_ROW_HEIGHT = 48;
constexpr int MENU_VIEWPORT_HEIGHT = 224 - MENU_CONTENT_TOP;
constexpr int EXPLORE_ROW_HEIGHT = 48;
constexpr int ITEM_ROW_HEIGHT = 48;
constexpr int SHOP_RAIL_DIVIDER_X = 58;
constexpr int SHOP_ICON_CENTER_X = 29;
constexpr int SHOP_ICON_CENTER_Y = 106;
constexpr int SHOP_ICON_ROW_HEIGHT = 44;
constexpr int SHOP_ACTION_X = 68;
constexpr int SHOP_ACTION_Y = 174;
constexpr int SHOP_ACTION_WIDTH = 108;
constexpr int SHOP_ACTION_HEIGHT = 36;

constexpr const char* EXPLORE_AREA_NAMES[Game::EXPLORE_AREA_COUNT] = {
    "GRASS PATH",
    "CREEK SLOPE",
    "TALL GRASS",
    "FROST CAVE",
    "MIST FOREST",
    "WATERFALL",
};

constexpr uint16_t EXPLORE_AREA_COLORS[Game::EXPLORE_AREA_COUNT] = {
    0x5E6B, 0x4D5D, 0xDDA8, 0x9E7F, 0x3C6D, 0x4D9F,
};

struct Glyph {
    char value;
    uint8_t rows[7];
};

constexpr Glyph FONT[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'/', {1, 2, 4, 8, 16, 0, 0}},
    {'%', {17, 2, 4, 8, 16, 0, 17}},
    {':', {0, 4, 4, 0, 4, 4, 0}},
    {'0', {14, 17, 19, 21, 25, 17, 14}},
    {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},
    {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},
    {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}},
    {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}},
    {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'A', {14, 17, 17, 31, 17, 17, 17}},
    {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}},
    {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}},
    {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}},
    {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},
    {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}},
    {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}},
    {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}},
    {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}},
    {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},
    {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}},
    {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}},
    {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},
    {'Z', {31, 1, 2, 4, 8, 16, 31}},
};

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint16_t>(((red & 0xF8U) << 8) |
                                 ((green & 0xFCU) << 3) |
                                 (blue >> 3));
}

const Glyph* findGlyph(char value) {
    for (const Glyph& glyph : FONT) {
        if (glyph.value == value) return &glyph;
    }
    return &FONT[0];
}

void text(Canvas565& canvas, int x, int y, const char* value,
          uint16_t color, int scale = 1) {
    if (!value || scale <= 0) return;
    int cursor = x;
    for (const char* it = value; *it; ++it) {
        const Glyph* glyph = findGlyph(*it);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph->rows[row] & (1U << (4 - column))) != 0) {
                    canvas.fillRect(cursor + column * scale,
                                    y + row * scale,
                                    scale, scale, color);
                }
            }
        }
        cursor += 6 * scale;
    }
}

void drawHeaderButton(Canvas565& canvas, int x, bool pressed = false) {
    canvas.fillRoundRect(x + 2, 2, HEADER_BUTTON_WIDTH - 4,
                         HEADER_HEIGHT - 4, 4,
                         pressed ? rgb(53, 76, 83) : rgb(27, 43, 51));
    canvas.drawRoundRect(x + 2, 2, HEADER_BUTTON_WIDTH - 4,
                         HEADER_HEIGHT - 4, 4, rgb(67, 97, 101));
}

void drawMenuIcon(Canvas565& canvas) {
    drawHeaderButton(canvas, MENU_BUTTON_X);
    const uint16_t color = rgb(222, 234, 229);
    for (int row = 0; row < 3; ++row) {
        canvas.drawFastHLine(MENU_BUTTON_X + 8, 8 + row * 4, 10, color);
    }
}

void drawHomeHudButton(Canvas565& canvas, int x) {
    canvas.fillRoundRect(x, HOME_HUD_BUTTON_Y,
                         HOME_HUD_BUTTON_SIZE, HOME_HUD_BUTTON_SIZE, 4,
                         rgb(27, 43, 51));
    canvas.drawRoundRect(x, HOME_HUD_BUTTON_Y,
                         HOME_HUD_BUTTON_SIZE, HOME_HUD_BUTTON_SIZE, 4,
                         rgb(67, 97, 101));
}

void drawHomeLockIcon(Canvas565& canvas) {
    drawHomeHudButton(canvas, HOME_LOCK_BUTTON_X);
    const uint16_t color = rgb(222, 234, 229);
    canvas.drawRoundRect(HOME_LOCK_BUTTON_X + 12,
                         HOME_HUD_BUTTON_Y + 7, 8, 9, 4, color);
    canvas.fillRoundRect(HOME_LOCK_BUTTON_X + 10,
                         HOME_HUD_BUTTON_Y + 14, 12, 10, 2, color);
    canvas.fillCircle(HOME_LOCK_BUTTON_X + 16,
                      HOME_HUD_BUTTON_Y + 18, 1, rgb(27, 43, 51));
}

void drawHomeMenuIcon(Canvas565& canvas) {
    drawHomeHudButton(canvas, HOME_MENU_BUTTON_X);
    const uint16_t color = rgb(222, 234, 229);
    for (int row = 0; row < 3; ++row) {
        canvas.drawFastHLine(HOME_MENU_BUTTON_X + 10,
                             HOME_HUD_BUTTON_Y + 10 + row * 5,
                             12, color);
    }
}

void drawBackIcon(Canvas565& canvas) {
    drawHeaderButton(canvas, 0);
    const uint16_t color = rgb(222, 234, 229);
    canvas.drawLine(16, 7, 9, 12, color);
    canvas.drawLine(9, 12, 16, 17, color);
}

void drawToast(Canvas565& canvas, const char* value) {
    if (!value || !*value) return;
    int width = static_cast<int>(std::strlen(value)) * 6 + 14;
    width = std::min(width, canvas.width() - 16);
    int x = (canvas.width() - width) / 2;
    canvas.fillRoundRect(x, 149, width, 18, 4, rgb(20, 31, 38));
    canvas.drawRoundRect(x, 149, width, 18, 4, rgb(92, 139, 137));
    text(canvas, x + 7, 155, value, rgb(234, 240, 235));
}

void drawBattery(Canvas565& canvas, int x, int y, uint8_t percent) {
    const uint16_t outline = rgb(222, 234, 229);
    canvas.drawRoundRect(x, y, 16, 8, 2, outline);
    canvas.fillRect(x + 16, y + 2, 2, 4, outline);
    int fillWidth = static_cast<int>(percent) * 12 / 100;
    canvas.fillRect(x + 2, y + 2, fillWidth, 4,
                    percent < 20 ? rgb(238, 91, 91) : rgb(92, 213, 139));
}

void drawHeart(Canvas565& canvas, int x, int y, uint16_t color) {
    canvas.fillCircle(x - 2, y - 1, 3, color);
    canvas.fillCircle(x + 2, y - 1, 3, color);
    canvas.fillTriangle(x - 5, y, x + 5, y, x, y + 6, color);
}

void drawPlant(Canvas565& canvas) {
    const uint16_t pot = rgb(197, 104, 76);
    const uint16_t leaf = rgb(60, 139, 94);
    canvas.fillRoundRect(18, 105, 22, 15, 3, pot);
    canvas.fillEllipse(24, 102, 5, 13, leaf);
    canvas.fillEllipse(34, 99, 6, 15, rgb(76, 165, 105));
    canvas.fillEllipse(29, 92, 5, 13, rgb(92, 182, 116));
}

void drawBowl(Canvas565& canvas, bool filled) {
    const uint16_t rim = rgb(222, 229, 218);
    const uint16_t bowl = rgb(76, 137, 166);
    canvas.fillEllipse(145, 143, 17, 6,
                       filled ? rgb(176, 113, 62) : rgb(42, 55, 60));
    canvas.drawFastHLine(128, 143, 35, rim);
    canvas.fillRoundRect(131, 144, 29, 11, 5, bowl);
    canvas.drawFastHLine(135, 154, 21, rgb(43, 88, 112));
}

void drawFoodContent(Canvas565& canvas, int centerX, int centerY) {
    canvas.fillEllipse(centerX, centerY, 9, 4, rgb(176, 113, 62));
    canvas.fillEllipse(centerX - 3, centerY - 2, 4, 2, rgb(221, 155, 77));
    canvas.fillEllipse(centerX + 4, centerY - 1, 3, 2, rgb(205, 91, 66));
}

void drawFallbackPet(Canvas565& canvas, int centerX, int groundY) {
    const int dx = centerX - 92;
    const int dy = groundY - 151;
    const uint16_t shadow = rgb(39, 57, 63);
    const uint16_t body = rgb(79, 185, 140);
    const uint16_t bodyLight = rgb(126, 218, 166);
    const uint16_t belly = rgb(224, 238, 194);
    const uint16_t dark = rgb(22, 38, 44);
    const uint16_t cheek = rgb(242, 112, 103);

    canvas.fillEllipse(92 + dx, 149 + dy, 31, 8, shadow);
    canvas.fillTriangle(69 + dx, 94 + dy, 79 + dx, 74 + dy,
                        86 + dx, 99 + dy, body);
    canvas.fillTriangle(98 + dx, 99 + dy, 106 + dx, 74 + dy,
                        116 + dx, 95 + dy, body);
    canvas.fillEllipse(92 + dx, 116 + dy, 30, 35, body);
    canvas.fillEllipse(92 + dx, 128 + dy, 20, 21, belly);
    canvas.fillEllipse(84 + dx, 104 + dy, 5, 7, dark);
    canvas.fillEllipse(101 + dx, 104 + dy, 5, 7, dark);
    canvas.fillRect(83 + dx, 102 + dy, 2, 2, bodyLight);
    canvas.fillRect(100 + dx, 102 + dy, 2, 2, bodyLight);
    canvas.fillEllipse(75 + dx, 115 + dy, 6, 4, cheek);
    canvas.fillEllipse(109 + dx, 115 + dy, 6, 4, cheek);
    canvas.drawFastHLine(88 + dx, 116 + dy, 9, dark);
    canvas.drawPixel(87 + dx, 115 + dy, dark);
    canvas.drawPixel(97 + dx, 115 + dy, dark);
    canvas.fillRoundRect(67 + dx, 132 + dy, 16, 10, 4, body);
    canvas.fillRoundRect(102 + dx, 132 + dy, 16, 10, 4, body);
}

void drawPet(Canvas565& canvas, const HomeViewModel& model) {
    PokemonSprites::WalkingAnimation animation{};
    PokemonSprites::WalkDirection direction = model.petFacingRight
        ? PokemonSprites::WalkDirection::RIGHT
        : PokemonSprites::WalkDirection::LEFT;
    bool animated = PokemonSprites::walkingAnimation(
        model.speciesId,
        model.petWalking ? direction : PokemonSprites::WalkDirection::DOWN,
        animation);
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    if (animated && animation.frameCount > 0) {
        uint8_t frameIndex = model.petWalking
            ? static_cast<uint8_t>(model.petFrame % animation.frameCount)
            : 0;
        auto kind = static_cast<PokemonSprites::SpriteKind>(
            static_cast<uint16_t>(animation.base) + frameIndex);
        frame = PokemonSprites::findSpeciesSprite(model.speciesId, kind);
        flipX = animation.flipX;
    }
    if (!frame) {
        drawFallbackPet(canvas, model.petCenterX, model.petGroundY);
        return;
    }

    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    const int x = model.petCenterX - width / 2;
    const int y = model.petGroundY - height;
    canvas.fillEllipse(model.petCenterX, model.petGroundY,
                       std::max(18, width / 2 - 3), 7,
                       rgb(39, 57, 63));
    if (!PokemonSprites::drawFrame(frame, x, y, flipX)) {
        drawFallbackPet(canvas, model.petCenterX, model.petGroundY);
    }
}

void drawHomeHpBar(Canvas565& canvas, int x, int y, int width,
                   uint8_t percent) {
    canvas.fillRect(x, y, width, 6, rgb(39, 45, 50));
    int filled = (width - 2) * percent / 100;
    uint16_t fillColor = percent > 50
        ? rgb(92, 222, 112)
        : (percent > 20 ? rgb(246, 204, 72) : rgb(232, 80, 84));
    if (filled > 0) canvas.fillRect(x + 1, y + 1, filled, 4, fillColor);
    canvas.drawRect(x, y, width, 6, rgb(220, 224, 218));
}

void drawExploreTileFallback(Canvas565& canvas, uint16_t tileId,
                             int x, int y, uint8_t layer,
                             uint16_t fieldColor) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE;
    if (layer == 0) {
        uint16_t color = fieldColor;
        if (ExploreMapGenerator::isWaterTile(tileId)) {
            color = rgb(40, 105, 173);
        } else if (ExploreMapGenerator::isRoadTile(tileId)) {
            color = rgb(232, 211, 135);
        } else if (tileId == 390) {
            color = rgb(55, 139, 75);
        } else if (ExploreMapGenerator::isForestTile(tileId)) {
            color = rgb(29, 91, 51);
        } else if (tileId >= 4500 && tileId <= 4544) {
            color = tileId == 4511 ? rgb(203, 202, 218)
                                   : rgb(181, 218, 232);
        }
        canvas.fillRect(x, y, tileSize, tileSize, color);
        return;
    }
    if (ExploreMapGenerator::isForestTile(tileId)) {
        canvas.fillRect(x + 3, y + 3, tileSize - 6, tileSize - 3,
                        rgb(24, 73, 42));
    } else if (tileId >= 4500 && tileId <= 4544) {
        canvas.fillTriangle(x + 5, y + 22, x + 13, y + 4,
                            x + 21, y + 22, rgb(112, 198, 232));
    }
}

void drawExploreMap(Canvas565& canvas,
                    const ExploreMapGenerator::Map& map,
                    int cameraX, int cameraY, uint16_t fieldColor,
                    uint8_t animationFrame) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE;
    int firstX = std::max(0, cameraX / tileSize);
    int firstY = std::max(0, cameraY / tileSize);
    int lastX = std::min<int>(
        ExploreMapGenerator::WIDTH - 1,
        (cameraX + canvas.width() - 1) / tileSize);
    int lastY = std::min<int>(
        ExploreMapGenerator::HEIGHT - 1,
        (cameraY + (canvas.height() - HOME_HEADER_HEIGHT) - 1) / tileSize);

    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT;
         ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                uint16_t tileId = map.layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (tileId == 0) continue;
                int x = tileX * tileSize - cameraX;
                int y = HOME_HEADER_HEIGHT + tileY * tileSize - cameraY;
                if (!GameAssets::drawExploreTile(
                        tileId, x, y, animationFrame)) {
                    drawExploreTileFallback(
                        canvas, tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
}

void drawExploreRoutePet(Canvas565& canvas,
                         const ExploreRouteViewModel& model) {
    auto direction = static_cast<PokemonSprites::WalkDirection>(
        model.walkDirection <=
                static_cast<uint8_t>(PokemonSprites::WalkDirection::RIGHT)
            ? model.walkDirection
            : static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN));
    PokemonSprites::WalkingAnimation animation{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    if (PokemonSprites::walkingAnimation(
            model.speciesId, direction, animation) &&
        animation.frameCount > 0) {
        uint8_t frameIndex = model.walking
            ? static_cast<uint8_t>(model.petFrame % animation.frameCount)
            : 0;
        auto kind = static_cast<PokemonSprites::SpriteKind>(
            static_cast<uint16_t>(animation.base) + frameIndex);
        frame = PokemonSprites::findSpeciesSprite(model.speciesId, kind);
        flipX = animation.flipX;
    }

    int screenX = static_cast<int>(std::lround(model.worldX)) - model.cameraX;
    int groundY = HOME_HEADER_HEIGHT +
                  static_cast<int>(std::lround(model.worldY)) - model.cameraY;
    if (!frame) {
        drawFallbackPet(canvas, screenX, groundY);
        return;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    canvas.fillEllipse(screenX, groundY, std::max(10, width / 2 - 4), 5,
                       rgb(27, 48, 48));
    if (!PokemonSprites::drawFrame(
            frame, screenX - width / 2, groundY - height, flipX)) {
        drawFallbackPet(canvas, screenX, groundY);
    }
}

}  // namespace

void renderHomeScreen(Canvas565& canvas, const HomeViewModel& model,
                      uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < HOME_HEADER_HEIGHT) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, HOME_HEADER_HEIGHT);
        canvas.setClipRect(0, top, canvas.width(), bottom - top);
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        text(canvas, 6, 8, "STICKMON", rgb(115, 226, 183));
        char clockText[6] = {};
        uint16_t minuteOfDay = model.gameMinutesOfDay % (24U * 60U);
        uint8_t hour = static_cast<uint8_t>(minuteOfDay / 60U);
        uint8_t minute = static_cast<uint8_t>(minuteOfDay % 60U);
        clockText[0] = static_cast<char>('0' + hour / 10);
        clockText[1] = static_cast<char>('0' + hour % 10);
        clockText[2] = ':';
        clockText[3] = static_cast<char>('0' + minute / 10);
        clockText[4] = static_cast<char>('0' + minute % 10);
        text(canvas, 116, 8, clockText, ink);
        drawBattery(canvas, 160, 8, 82);
        canvas.clearClipRect();
    }

    if (rowBegin < HOME_STATUS_TOP && rowEnd > HOME_ROOM_TOP) {
        int top = std::max<int>(rowBegin, HOME_ROOM_TOP);
        int bottom = std::min<int>(rowEnd, HOME_STATUS_TOP);
        canvas.setClipRect(0, top, HOME_ROOM_WIDTH, bottom - top);
        bool roomDrawn = RoomRenderer::drawViewport(
            0, HOME_ROOM_TOP, canvas.width(), HOME_ROOM_HEIGHT,
            model.cameraX, model.cameraY, model.night);
        if (!roomDrawn) {
            canvas.fillRect(0, HEADER_HEIGHT, canvas.width(), 100,
                            rgb(218, 204, 173));
            canvas.fillRect(0, 124, canvas.width(), 48, rgb(122, 86, 68));
            for (int y = 127; y < 172; y += 8) {
                canvas.drawFastHLine(0, y, canvas.width(), rgb(105, 72, 58));
            }
            for (int x = 8; x < canvas.width(); x += 24) {
                canvas.drawFastVLine(x, 124, 48, rgb(112, 77, 61));
            }

            canvas.fillRoundRect(17, 36, 55, 47, 3, rgb(67, 74, 75));
            canvas.fillRect(21, 40, 47, 39, rgb(93, 174, 196));
            canvas.fillCircle(58, 50, 7, rgb(246, 213, 116));
            canvas.fillRect(21, 68, 47, 11, rgb(101, 153, 119));
            canvas.drawFastVLine(44, 40, 39, rgb(67, 74, 75));
            canvas.drawFastHLine(21, 59, 47, rgb(67, 74, 75));

            canvas.fillRoundRect(132, 44, 37, 62, 3, rgb(92, 69, 59));
            canvas.fillRect(136, 50, 29, 4, rgb(184, 133, 81));
            canvas.fillRect(136, 71, 29, 4, rgb(184, 133, 81));
            canvas.fillRect(139, 59, 6, 12, rgb(87, 140, 157));
            canvas.fillRect(147, 57, 7, 14, rgb(205, 104, 83));
            canvas.fillRect(156, 61, 6, 10, rgb(220, 183, 91));
            canvas.fillRoundRect(140, 83, 21, 18, 3, rgb(65, 113, 105));
            drawPlant(canvas);
            canvas.fillEllipse(92, 151, 53, 17, rgb(49, 112, 111));
            canvas.fillEllipse(92, 149, 45, 12, rgb(71, 151, 139));
        }
        drawPet(canvas, model);
        if (roomDrawn) {
            if (model.bowlFilled) {
                drawFoodContent(canvas, model.bowlCenterX, model.bowlCenterY);
            }
        } else {
            drawBowl(canvas, model.bowlFilled);
        }
        if (model.showHearts) {
            drawHeart(canvas, model.petCenterX - 34, model.petGroundY - 58,
                      rgb(239, 103, 113));
            drawHeart(canvas, model.petCenterX + 31, model.petGroundY - 64,
                      rgb(239, 103, 113));
        }
        drawToast(canvas, model.toast);
        canvas.clearClipRect();
    }

    if (rowEnd > HOME_STATUS_TOP) {
        int top = std::max<int>(rowBegin, HOME_STATUS_TOP);
        int bottom = rowEnd;
        canvas.setClipRect(0, top, canvas.width(), bottom - top);
        canvas.fillRect(0, 172, canvas.width(), 52, rgb(18, 27, 35));
        canvas.drawFastHLine(0, 172, canvas.width(), rgb(71, 108, 108));
        drawHomeLockIcon(canvas);
        drawHomeMenuIcon(canvas);

        canvas.fillRoundRect(HOME_MONSTER_PANEL_X, 176,
                             HOME_MONSTER_PANEL_W, 44, 4,
                             rgb(24, 34, 42));
        canvas.drawRoundRect(HOME_MONSTER_PANEL_X, 176,
                             HOME_MONSTER_PANEL_W, 44, 4,
                             rgb(72, 83, 98));
        uint8_t count = std::min<uint8_t>(model.monsterCount,
                                          Game::TEAM_CAP);
        for (uint8_t index = 0; index < count; ++index) {
            int rowY = 177 + index * 21;
            HudRenderer::drawHungerIcon(
                canvas, HOME_MONSTER_PANEL_X + 7, rowY + 3,
                model.monsters[index].hunger);
            drawHomeHpBar(canvas, HOME_MONSTER_PANEL_X + 29,
                          rowY + 7, 45, model.monsters[index].hp);
        }
        canvas.clearClipRect();
    }
}

HomeHitTarget homeHitTargetAt(int x, int y, int petCenterX,
                              int petGroundY, int bowlCenterX,
                              int bowlCenterY) {
    if (y >= HOME_STATUS_TOP && y < 224) {
        if (x >= HOME_MENU_BUTTON_X - 4 &&
            x < HOME_MENU_BUTTON_X + HOME_HUD_BUTTON_SIZE + 4) {
            return HomeHitTarget::MENU;
        }
        if (x >= HOME_LOCK_BUTTON_X - 4 &&
            x < HOME_LOCK_BUTTON_X + HOME_HUD_BUTTON_SIZE + 4) {
            return HomeHitTarget::LOCK;
        }
    }
    if (x >= bowlCenterX - 20 && x < bowlCenterX + 20 &&
        y >= bowlCenterY - 18 && y < bowlCenterY + 18) {
        return HomeHitTarget::BOWL;
    }
    if (x >= petCenterX - 38 && x < petCenterX + 38 &&
        y >= petGroundY - 82 && y < petGroundY + 5) {
        return HomeHitTarget::PET;
    }
    return HomeHitTarget::NONE;
}

float mainMenuMaxScroll() {
    int contentHeight = MAIN_MENU_ITEM_COUNT * MENU_ROW_HEIGHT;
    return static_cast<float>(
        std::max(0, contentHeight - MENU_VIEWPORT_HEIGHT));
}

bool mainMenuBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int mainMenuItemAt(int x, int y, float scroll) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int contentY = static_cast<int>(y - MENU_CONTENT_TOP + scroll);
    if (contentY < 0) return -1;
    int index = contentY / MENU_ROW_HEIGHT;
    return index < MAIN_MENU_ITEM_COUNT ? index : -1;
}

void renderMainMenu(Canvas565& canvas, const MenuViewModel& model,
                    uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, top, canvas.width(), bottom - top);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(12, 18, 25));
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, "MENU", rgb(115, 226, 183));
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    int scroll = static_cast<int>(std::lround(model.scroll));
    for (int index = 0; index < MAIN_MENU_ITEM_COUNT; ++index) {
        int y = MENU_CONTENT_TOP + index * MENU_ROW_HEIGHT - scroll;
        if (y + MENU_ROW_HEIGHT <= MENU_CONTENT_TOP || y >= 224) continue;

        AppSceneFlow::MainMenuEntry entry =
            AppSceneFlow::mainMenuEntry(static_cast<uint8_t>(index), false);
        uint16_t background = index == model.pressedItem
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect(6, y + 2, 172, MENU_ROW_HEIGHT - 4, 4,
                             background);
        if (entry.iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            uint16_t offset = FlashStorage::readWord(
                &MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex].offset);
            uint16_t length = FlashStorage::readWord(
                &MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex].length);
            canvas.drawRgb565Rle(
                10, y + 4, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                MenuAssets::MAIN_ICON_RLE, offset, length);
        }
        text(canvas, 58, y + 19, entry.shortLabel, ink);
        canvas.drawLine(163, y + 19, 168, y + 23, rgb(126, 145, 145));
        canvas.drawLine(168, y + 23, 163, y + 27, rgb(126, 145, 145));
    }
    float maxScroll = mainMenuMaxScroll();
    if (maxScroll > 0.0f) {
        int trackHeight = MENU_VIEWPORT_HEIGHT - 12;
        int thumbHeight = std::max(24,
            MENU_VIEWPORT_HEIGHT * MENU_VIEWPORT_HEIGHT /
            (MAIN_MENU_ITEM_COUNT * MENU_ROW_HEIGHT));
        int thumbTravel = trackHeight - thumbHeight;
        int thumbY = MENU_CONTENT_TOP + 6 + static_cast<int>(
            (model.scroll / maxScroll) * thumbTravel);
        canvas.fillRoundRect(180, thumbY, 3, thumbHeight, 1,
                             rgb(92, 139, 137));
    }
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

float exploreMaxScroll(uint8_t visibleAreaCount) {
    int count = std::min<int>(visibleAreaCount, Game::EXPLORE_AREA_COUNT);
    int contentHeight = count * EXPLORE_ROW_HEIGHT;
    return static_cast<float>(
        std::max(0, contentHeight - MENU_VIEWPORT_HEIGHT));
}

bool exploreBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

bool exploreMenuAt(int x, int y) {
    return x >= MENU_BUTTON_X && x < 184 &&
           y >= 0 && y < HEADER_HEIGHT;
}

int exploreAreaAt(int x, int y, float scroll, uint8_t visibleAreaCount) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int contentY = static_cast<int>(y - MENU_CONTENT_TOP + scroll);
    if (contentY < 0) return -1;
    int index = contentY / EXPLORE_ROW_HEIGHT;
    int count = std::min<int>(visibleAreaCount, Game::EXPLORE_AREA_COUNT);
    return index < count ? index : -1;
}

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    const uint16_t muted = rgb(137, 155, 158);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, top, canvas.width(), bottom - top);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, "EXPLORE", rgb(115, 226, 183));
        drawMenuIcon(canvas);
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    int scroll = static_cast<int>(std::lround(model.scroll));
    int count = std::min<int>(model.visibleAreaCount,
                              Game::EXPLORE_AREA_COUNT);
    for (int index = 0; index < count; ++index) {
        int y = MENU_CONTENT_TOP + index * EXPLORE_ROW_HEIGHT - scroll;
        if (y + EXPLORE_ROW_HEIGHT <= MENU_CONTENT_TOP || y >= 224) continue;

        bool locked = index > model.unlockedArea;
        bool selected = index == model.selectedArea;
        bool pressed = index == model.pressedArea;
        uint16_t background = pressed
            ? rgb(48, 62, 68)
            : selected && !locked
                ? rgb(31, 52, 52)
                : rgb(23, 33, 41);
        canvas.fillRoundRect(7, y + 3, 170, EXPLORE_ROW_HEIGHT - 6, 4,
                             background);
        canvas.fillRoundRect(11, y + 8, 4, EXPLORE_ROW_HEIGHT - 16, 2,
                             locked ? rgb(73, 83, 89)
                                    : EXPLORE_AREA_COLORS[index]);
        text(canvas, 22, y + 10, EXPLORE_AREA_NAMES[index],
             locked ? muted : ink);

        char levelText[12] = {};
        std::snprintf(levelText, sizeof(levelText), "REC LV %u",
                      ExploreAreaCatalog::recommendedLevel(index));
        text(canvas, 22, y + 27, levelText,
             locked ? rgb(88, 98, 104) : muted);
        if (locked) {
            text(canvas, 133, y + 27, "LOCKED", rgb(116, 126, 130));
        } else if (selected) {
            canvas.fillCircle(164, y + 23, 6, rgb(115, 226, 183));
            canvas.fillCircle(164, y + 23, 2, rgb(20, 39, 40));
        }
    }

    float maxScroll = exploreMaxScroll(model.visibleAreaCount);
    if (maxScroll > 0.0f) {
        int trackHeight = MENU_VIEWPORT_HEIGHT - 12;
        int contentHeight = count * EXPLORE_ROW_HEIGHT;
        int thumbHeight = std::max(24,
            MENU_VIEWPORT_HEIGHT * MENU_VIEWPORT_HEIGHT / contentHeight);
        int thumbTravel = trackHeight - thumbHeight;
        int thumbY = MENU_CONTENT_TOP + 6 + static_cast<int>(
            (model.scroll / maxScroll) * thumbTravel);
        canvas.fillRoundRect(180, thumbY, 3, thumbHeight, 1,
                             rgb(92, 139, 137));
    }
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

bool exploreRouteBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

bool exploreRouteMenuAt(int x, int y) {
    return x >= MENU_BUTTON_X && x < 184 &&
           y >= 0 && y < HEADER_HEIGHT;
}

int exploreRouteExitChoiceAt(int x, int y) {
    if (y < 167 || y >= 204) return -1;
    if (x >= 18 && x < 88) return 0;
    if (x >= 96 && x < 166) return 1;
    return -1;
}

bool exploreRouteMapAt(int x, int y) {
    return x >= 0 && x < 184 && y >= HEADER_HEIGHT && y < 224;
}

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowEnd > HOME_HEADER_HEIGHT) {
        int top = std::max<int>(rowBegin, HOME_HEADER_HEIGHT);
        canvas.setClipRect(0, top, canvas.width(), rowEnd - top);
        canvas.fillRect(0, HOME_HEADER_HEIGHT, canvas.width(),
                        canvas.height() - HOME_HEADER_HEIGHT,
                        ExploreAreaCatalog::fieldColor(model.area));
        if (model.map) {
            drawExploreMap(canvas, *model.map,
                           model.cameraX, model.cameraY,
                           ExploreAreaCatalog::fieldColor(model.area),
                           model.petFrame);
            drawExploreRoutePet(canvas, model);
        }

        canvas.fillRect(0, 200, canvas.width(), 24, rgb(10, 18, 23));
        canvas.drawFastHLine(0, 200, canvas.width(), rgb(68, 99, 101));
        char stepsText[16] = {};
        std::snprintf(stepsText, sizeof(stepsText), "STEPS %u", model.steps);
        text(canvas, 7, 208, stepsText, rgb(226, 238, 233));
        const char* status = model.complete
            ? "ROUTE END"
            : model.walking || model.autoWalk ? "WALKING" : "PAUSED";
        int statusX = 177 - static_cast<int>(std::strlen(status)) * 6;
        text(canvas, statusX, 208, status,
             model.complete ? rgb(248, 210, 105)
                            : rgb(115, 226, 183));

        if (model.exitConfirm) {
            canvas.fillRoundRect(10, 145, 164, 69, 6, rgb(17, 27, 34));
            canvas.drawRoundRect(10, 145, 164, 69, 6,
                                 rgb(82, 117, 117));
            text(canvas, 49, 154, "LEAVE ROUTE", rgb(226, 238, 233));
            canvas.fillRoundRect(18, 167, 70, 37, 4, rgb(36, 54, 61));
            canvas.fillRoundRect(96, 167, 70, 37, 4, rgb(91, 49, 55));
            text(canvas, 36, 182, "STAY", rgb(115, 226, 183));
            text(canvas, 117, 182, "EXIT", rgb(239, 143, 148));
        }
        canvas.clearClipRect();
    }

    if (rowBegin < HOME_HEADER_HEIGHT) {
        int bottom = std::min<int>(rowEnd, HOME_HEADER_HEIGHT);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), HOME_HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HOME_HEADER_HEIGHT - 1,
                        canvas.width(), 1, rgb(56, 87, 89));
        drawBackIcon(canvas);
        const char* name = EXPLORE_AREA_NAMES[
            model.area < Game::EXPLORE_AREA_COUNT ? model.area : 0];
        text(canvas, 36, 8, name, rgb(115, 226, 183));
        drawMenuIcon(canvas);
        canvas.clearClipRect();
    }
}

bool exploreRouteMenuBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int exploreRouteMenuItemAt(int x, int y) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int index = (y - MENU_CONTENT_TOP) / MENU_ROW_HEIGHT;
    return index < AppSceneFlow::exploreMenuItemCount() ? index : -1;
}

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(12, 18, 25));
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, "EXPLORE", rgb(115, 226, 183));
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    for (uint8_t index = 0; index < AppSceneFlow::exploreMenuItemCount();
         ++index) {
        int y = MENU_CONTENT_TOP + index * MENU_ROW_HEIGHT;
        AppSceneFlow::ExploreMenuEntry entry =
            AppSceneFlow::exploreMenuEntry(index);
        uint16_t background = index == model.pressedItem
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        uint16_t labelColor = ink;
        if (entry.item == AppSceneFlow::ExploreMenuItem::END) {
            labelColor = rgb(239, 143, 148);
        } else if (entry.item == AppSceneFlow::ExploreMenuItem::BACK) {
            labelColor = rgb(115, 226, 183);
        }
        canvas.fillRoundRect(6, y + 2, 172, MENU_ROW_HEIGHT - 4, 4,
                             background);
        if (entry.iconIndex < MenuAssets::MAIN_ICON_COUNT) {
            uint16_t offset = FlashStorage::readWord(
                &MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex].offset);
            uint16_t length = FlashStorage::readWord(
                &MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex].length);
            canvas.drawRgb565Rle(
                10, y + 4, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                MenuAssets::MAIN_ICON_RLE, offset, length);
        }
        text(canvas, 58, y + 19, entry.shortLabel, labelColor);
        if (entry.item == AppSceneFlow::ExploreMenuItem::TEAM ||
            entry.item == AppSceneFlow::ExploreMenuItem::BAG) {
            canvas.drawLine(163, y + 19, 168, y + 23,
                            rgb(126, 145, 145));
            canvas.drawLine(168, y + 23, 163, y + 27,
                            rgb(126, 145, 145));
        }
    }
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

namespace {

constexpr int TEAM_CARD_TOP = 32;
constexpr int TEAM_CARD_HEIGHT = 78;
constexpr int TEAM_CARD_GAP = 6;

const char* teamStatusLabel(Game::MajorStatus status) {
    switch (status) {
    case Game::MajorStatus::POISON: return "POISON";
    case Game::MajorStatus::TOXIC: return "TOXIC";
    case Game::MajorStatus::PARALYSIS: return "PARALYSIS";
    case Game::MajorStatus::SLEEP: return "SLEEP";
    case Game::MajorStatus::BURN: return "BURN";
    case Game::MajorStatus::FREEZE: return "FREEZE";
    case Game::MajorStatus::NONE: return "NORMAL";
    }
    return "NORMAL";
}

void drawTeamSprite(Canvas565& canvas, uint16_t speciesId,
                    int centerX, int centerY) {
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        canvas.fillCircle(centerX, centerY, 19, rgb(42, 61, 68));
        text(canvas, centerX - 9, centerY - 3, "MON",
             rgb(115, 226, 183));
        return;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    float scale = std::min(1.0f, std::min(50.0f / std::max(1, width),
                                         54.0f / std::max(1, height)));
    int drawnWidth = static_cast<int>(std::lround(width * scale));
    int drawnHeight = static_cast<int>(std::lround(height * scale));
    if (!PokemonSprites::drawFrameScaled(
            frame, centerX - drawnWidth / 2,
            centerY - drawnHeight / 2, scale)) {
        canvas.fillCircle(centerX, centerY, 19, rgb(42, 61, 68));
    }
}

void drawTeamConfirm(Canvas565& canvas) {
    canvas.fillRoundRect(10, 132, 164, 82, 6, rgb(17, 27, 34));
    canvas.drawRoundRect(10, 132, 164, 82, 6, rgb(82, 117, 117));
    text(canvas, 50, 145, "CHANGE LEADER", rgb(226, 238, 233));
    text(canvas, 62, 160, "SET FIRST", rgb(248, 210, 105));
    canvas.fillRoundRect(18, 174, 70, 36, 4, rgb(36, 54, 61));
    canvas.fillRoundRect(96, 174, 70, 36, 4, rgb(91, 49, 55));
    text(canvas, 39, 188, "YES", rgb(115, 226, 183));
    text(canvas, 113, 188, "CANCEL", rgb(239, 143, 148));
}

}  // namespace

bool teamBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int teamMemberAt(int x, int y, uint8_t teamCount) {
    if (x < 6 || x >= 178 || y < TEAM_CARD_TOP) return -1;
    int pitch = TEAM_CARD_HEIGHT + TEAM_CARD_GAP;
    int index = (y - TEAM_CARD_TOP) / pitch;
    int rowY = TEAM_CARD_TOP + index * pitch;
    if (index < 0 || index >= teamCount || y >= rowY + TEAM_CARD_HEIGHT) {
        return -1;
    }
    return index;
}

int teamConfirmChoiceAt(int x, int y) {
    return itemConfirmChoiceAt(x, y);
}

void renderTeamScreen(Canvas565& canvas, const TeamViewModel& model,
                      uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd || !model.state) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(12, 18, 25));
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, "TEAM", rgb(115, 226, 183));
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    PixelRenderer::canvas().setClipRect(
        0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    uint8_t teamCount = std::min<uint8_t>(model.state->teamCount,
                                           Game::TEAM_CAP);
    for (uint8_t slot = 0; slot < teamCount; ++slot) {
        const Game::MonsterRuntime& monster = model.state->team[slot];
        int y = TEAM_CARD_TOP + slot * (TEAM_CARD_HEIGHT + TEAM_CARD_GAP);
        uint16_t background = model.pressedSlot == slot
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        uint16_t border = slot == 0 ? rgb(115, 226, 183)
                                    : rgb(67, 97, 101);
        canvas.fillRoundRect(6, y, 172, TEAM_CARD_HEIGHT, 5, background);
        canvas.drawRoundRect(6, y, 172, TEAM_CARD_HEIGHT, 5, border);
        drawTeamSprite(canvas, monster.speciesId, 35, y + 36);

        const Species* species = findSpecies(monster.speciesId);
        PixelRenderer::text(64, y + 6,
                            species ? species->name : "MONSTER",
                            rgb(226, 238, 233), 1);
        char level[12];
        std::snprintf(level, sizeof(level), "LV%u", monster.level);
        text(canvas, 64, y + 28, level, rgb(126, 175, 175));
        char hp[20];
        std::snprintf(hp, sizeof(hp), "%u/%u",
                      monster.hpCur, monster.hpMax);
        text(canvas, 172 - static_cast<int>(std::strlen(hp)) * 6,
             y + 28, hp, rgb(226, 238, 233));
        drawHomeHpBar(canvas, 64, y + 42, 104,
                      Game::HomeHud::hpPercent(monster));

        char hunger[10];
        std::snprintf(hunger, sizeof(hunger), "H %u",
                      Game::HomeHud::hungerPercent(monster));
        text(canvas, 64, y + 58, hunger, rgb(248, 210, 105));
        const char* status = teamStatusLabel(monster.majorStatus);
        text(canvas, 172 - static_cast<int>(std::strlen(status)) * 6,
             y + 58, status,
             monster.majorStatus == Game::MajorStatus::NONE
                 ? rgb(126, 145, 145) : rgb(239, 143, 148));
        if (slot == 0) {
            canvas.fillRoundRect(139, y + 5, 31, 14, 3,
                                 rgb(43, 94, 79));
            text(canvas, 143, y + 9, "LEAD", rgb(194, 242, 216));
        } else if (monster.origin != Game::Origin::VISITOR) {
            canvas.drawLine(164, y + 9, 169, y + 13,
                            rgb(115, 226, 183));
            canvas.drawLine(169, y + 13, 164, y + 17,
                            rgb(115, 226, 183));
        }
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    if (model.confirmOpen) drawTeamConfirm(canvas);
    canvas.clearClipRect();
}

int shopCategoryItemAt(int x, int y) {
    if (x < 0 || x >= 184 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int index = (y - MENU_CONTENT_TOP) / MENU_ROW_HEIGHT;
    return index < 4 ? index : -1;
}

void renderShopCategoryScreen(Canvas565& canvas,
                              const ShopCategoryViewModel& model,
                              uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* LABELS[] = {
        "DAILY", "EXPLORE", "SELL", "BACK",
    };
    const uint16_t ink = rgb(226, 238, 233);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(12, 18, 25));
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, "SHOP", rgb(115, 226, 183));
        char coins[16];
        std::snprintf(coins, sizeof(coins), "C%lu",
                      static_cast<unsigned long>(model.coins));
        text(canvas, 178 - static_cast<int>(std::strlen(coins)) * 6,
             8, coins, rgb(248, 210, 105));
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    PixelRenderer::canvas().setClipRect(
        0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));
    canvas.fillRect(0, MENU_CONTENT_TOP, SHOP_RAIL_DIVIDER_X,
                    canvas.height() - MENU_CONTENT_TOP, rgb(17, 24, 31));
    canvas.drawFastVLine(SHOP_RAIL_DIVIDER_X, MENU_CONTENT_TOP,
                         canvas.height() - MENU_CONTENT_TOP,
                         rgb(67, 74, 84));

    int previewIndex = model.pressedItem >= 0
        ? model.pressedItem : static_cast<int>(model.category);
    for (int index = 0; index < 4; ++index) {
        int y = MENU_CONTENT_TOP + index * MENU_ROW_HEIGHT;
        bool selected = index == previewIndex;
        if (selected) {
            canvas.fillRect(4, y + 8, 3, 30, rgb(248, 210, 105));
            canvas.fillRect(8, y + 3, SHOP_RAIL_DIVIDER_X - 10,
                            MENU_ROW_HEIGHT - 6, rgb(31, 42, 49));
        }
        text(canvas, 12, y + 20, LABELS[index],
             index == 3 ? rgb(115, 226, 183)
                        : selected ? rgb(248, 210, 105) : ink);
    }

    if (previewIndex == 3 || !model.state) {
        uint16_t backColor = rgb(115, 226, 183);
        canvas.drawFastHLine(105, 111, 34, backColor);
        canvas.drawLine(105, 111, 116, 100, backColor);
        canvas.drawLine(105, 111, 116, 122, backColor);
        text(canvas, 96, 142, "RETURN", rgb(126, 175, 175));
    } else {
        Game::ShopService::Category category =
            static_cast<Game::ShopService::Category>(previewIndex);
        uint8_t count = category == Game::ShopService::Category::SELL
            ? Game::ShopService::sellItemCount(*model.state)
            : Game::ShopService::buyItemCount(category, *model.state);
        uint8_t shown = std::min<uint8_t>(count, 4);
        for (uint8_t index = 0; index < shown; ++index) {
            Game::ItemId item = category == Game::ShopService::Category::SELL
                ? Game::ShopService::sellItemAt(*model.state, index)
                : Game::ShopService::buyItemAt(category, *model.state, index);
            if (!GameAssets::drawCentered(GameAssets::itemKind(item),
                                          121, 52 + index * 44, 0.9f)) {
                text(canvas, 118, 47 + index * 44, "?",
                     rgb(248, 210, 105));
            }
        }
        if (count == 0) {
            text(canvas, 91, 103, "EMPTY", rgb(126, 145, 145));
        } else if (count > shown) {
            canvas.fillTriangle(116, 213, 126, 213, 121, 218,
                                rgb(126, 175, 175));
        }
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

bool itemListBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

float itemListMaxScroll(uint8_t itemCount) {
    int contentHeight = itemCount * ITEM_ROW_HEIGHT;
    return static_cast<float>(
        std::max(0, contentHeight - (224 - MENU_CONTENT_TOP)));
}

float shopItemScrollForIndex(uint8_t index) {
    return static_cast<float>(index * SHOP_ICON_ROW_HEIGHT);
}

float shopItemMaxScroll(uint8_t itemCount) {
    return itemCount == 0 ? 0.0f : shopItemScrollForIndex(itemCount - 1);
}

int shopSelectedItem(float scroll, uint8_t itemCount) {
    if (itemCount == 0) return -1;
    int index = static_cast<int>(std::lround(
        scroll / static_cast<float>(SHOP_ICON_ROW_HEIGHT)));
    return std::max(0, std::min(index, static_cast<int>(itemCount) - 1));
}

int shopItemAt(int x, int y, float scroll, uint8_t itemCount) {
    if (x < 0 || x >= SHOP_RAIL_DIVIDER_X ||
        y < MENU_CONTENT_TOP || y >= 224 || itemCount == 0) {
        return -1;
    }
    float row = (static_cast<float>(y - SHOP_ICON_CENTER_Y) + scroll) /
                static_cast<float>(SHOP_ICON_ROW_HEIGHT);
    int index = static_cast<int>(std::lround(row));
    if (index < 0 || index >= itemCount) return -1;
    int centerY = SHOP_ICON_CENTER_Y + index * SHOP_ICON_ROW_HEIGHT -
                  static_cast<int>(std::lround(scroll));
    return std::abs(y - centerY) <= SHOP_ICON_ROW_HEIGHT / 2 ? index : -1;
}

bool shopActionAt(int x, int y) {
    return x >= SHOP_ACTION_X && x < SHOP_ACTION_X + SHOP_ACTION_WIDTH &&
           y >= SHOP_ACTION_Y && y < SHOP_ACTION_Y + SHOP_ACTION_HEIGHT;
}

int itemListItemAt(int x, int y, float scroll, uint8_t itemCount) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int contentY = static_cast<int>(y - MENU_CONTENT_TOP + scroll);
    if (contentY < 0) return -1;
    int index = contentY / ITEM_ROW_HEIGHT;
    return index < itemCount ? index : -1;
}

int itemConfirmChoiceAt(int x, int y) {
    if (y < 174 || y >= 210) return -1;
    if (x >= 18 && x < 88) return 0;
    if (x >= 96 && x < 166) return 1;
    return -1;
}

namespace {

Game::ItemId itemForRow(const ItemListViewModel& model, uint8_t index) {
    if (!model.state) return Game::ItemId::COUNT;
    switch (model.mode) {
    case ItemListMode::BAG:
        return Game::ItemInventory::homeBagItemAt(*model.state, index);
    case ItemListMode::BUY:
        return Game::ShopService::buyItemAt(
            model.category, *model.state, index);
    case ItemListMode::SELL:
        return Game::ShopService::sellItemAt(*model.state, index);
    }
    return Game::ItemId::COUNT;
}

void drawItemConfirm(Canvas565& canvas, const ItemListViewModel& model) {
    if (!model.confirmOpen || model.pendingItem == Game::ItemId::COUNT) return;
    canvas.fillRoundRect(10, 132, 164, 82, 6, rgb(17, 27, 34));
    canvas.drawRoundRect(10, 132, 164, 82, 6, rgb(82, 117, 117));
    const char* action = model.mode == ItemListMode::BAG
        ? "USE ITEM" : model.mode == ItemListMode::SELL
            ? "SELL ITEM" : "BUY ITEM";
    text(canvas, (184 - static_cast<int>(std::strlen(action)) * 6) / 2,
         141, action, rgb(226, 238, 233));
    const char* name = Game::ShopService::shortName(model.pendingItem);
    text(canvas, std::max(16, (184 - static_cast<int>(std::strlen(name)) * 6) / 2),
         157, name, rgb(248, 210, 105));
    canvas.fillRoundRect(18, 174, 70, 36, 4, rgb(36, 54, 61));
    canvas.fillRoundRect(96, 174, 70, 36, 4, rgb(91, 49, 55));
    text(canvas, 39, 188, "YES", rgb(115, 226, 183));
    text(canvas, 113, 188, "CANCEL", rgb(239, 143, 148));
}

}  // namespace

void renderItemListScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(12, 18, 25));
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8,
             model.mode == ItemListMode::BAG ? "BAG" : "SHOP",
             rgb(115, 226, 183));
        if (model.mode != ItemListMode::BAG) {
            char coins[16];
            std::snprintf(coins, sizeof(coins), "C%lu",
                          static_cast<unsigned long>(model.coins));
            text(canvas, 178 - static_cast<int>(std::strlen(coins)) * 6,
                 8, coins, rgb(248, 210, 105));
        }
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    PixelRenderer::canvas().setClipRect(
        0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    if (model.itemCount == 0) {
        text(canvas, 50, 105,
             model.mode == ItemListMode::SELL ? "NOTHING TO SELL"
                                               : "BAG IS EMPTY",
             rgb(126, 145, 145));
    }
    for (uint8_t index = 0; index < model.itemCount; ++index) {
        int y = MENU_CONTENT_TOP + index * ITEM_ROW_HEIGHT -
                static_cast<int>(std::lround(model.scroll));
        if (y + ITEM_ROW_HEIGHT <= MENU_CONTENT_TOP || y >= 224) continue;
        Game::ItemId item = itemForRow(model, index);
        if (item == Game::ItemId::COUNT) continue;
        uint16_t background = index == model.pressedItem
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect(6, y + 2, 172, ITEM_ROW_HEIGHT - 4, 4,
                             background);
        if (!GameAssets::drawCentered(GameAssets::itemKind(item),
                                      27, y + 21, 0.72f)) {
            text(canvas, 24, y + 16, "?", rgb(248, 210, 105));
        }
        char owned[8];
        std::snprintf(owned, sizeof(owned), "X%u",
                      Game::ItemInventory::count(*model.state, item));
        text(canvas, 10, y + 35, owned, rgb(248, 210, 105));
        const char* name = Game::ShopService::shortName(item);
        text(canvas, 48, y + 9, name, rgb(226, 238, 233));
        text(canvas, 48, y + 27,
             Game::ShopService::shortDescription(item),
             rgb(126, 175, 175));
        if (model.mode != ItemListMode::BAG) {
            uint16_t price = model.mode == ItemListMode::SELL
                ? Game::ShopService::sellPrice(item)
                : Game::ShopService::buyPrice(item);
            char priceText[10];
            std::snprintf(priceText, sizeof(priceText), "C%u", price);
            int priceX = 172 - static_cast<int>(std::strlen(priceText)) * 6;
            canvas.fillRect(priceX - 2, y + 5,
                            static_cast<int>(std::strlen(priceText)) * 6 + 4,
                            13, background);
            text(canvas, priceX, y + 8, priceText, rgb(248, 210, 105));
        }
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    drawItemConfirm(canvas, model);
    canvas.clearClipRect();
}

void renderShopItemScreen(Canvas565& canvas,
                          const ItemListViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(12, 18, 25));
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, "SHOP", rgb(115, 226, 183));
        char coins[16];
        std::snprintf(coins, sizeof(coins), "C%lu",
                      static_cast<unsigned long>(model.coins));
        text(canvas, 178 - static_cast<int>(std::strlen(coins)) * 6,
             8, coins, rgb(248, 210, 105));
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    PixelRenderer::canvas().setClipRect(
        0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));
    canvas.fillRect(0, MENU_CONTENT_TOP, SHOP_RAIL_DIVIDER_X,
                    canvas.height() - MENU_CONTENT_TOP, rgb(17, 24, 31));
    canvas.drawFastVLine(SHOP_RAIL_DIVIDER_X, MENU_CONTENT_TOP,
                         canvas.height() - MENU_CONTENT_TOP,
                         rgb(67, 74, 84));

    int selected = shopSelectedItem(model.scroll, model.itemCount);
    for (uint8_t index = 0; index < model.itemCount; ++index) {
        int centerY = SHOP_ICON_CENTER_Y + index * SHOP_ICON_ROW_HEIGHT -
                      static_cast<int>(std::lround(model.scroll));
        if (centerY + 22 < MENU_CONTENT_TOP || centerY - 22 >= 224) continue;
        bool isSelected = index == selected;
        if (isSelected) {
            canvas.fillRect(2, centerY - 21, SHOP_RAIL_DIVIDER_X - 4, 42,
                            rgb(31, 42, 49));
            canvas.fillRect(2, centerY - 12, 3, 24, rgb(248, 210, 105));
        }
        Game::ItemId item = itemForRow(model, index);
        if (!GameAssets::drawCentered(GameAssets::itemKind(item),
                                      SHOP_ICON_CENTER_X, centerY,
                                      isSelected ? 1.0f : 0.82f)) {
            text(canvas, SHOP_ICON_CENTER_X - 3, centerY - 5, "?",
                 isSelected ? rgb(248, 210, 105) : rgb(126, 175, 175));
        }
    }

    if (model.scroll > 0.5f) {
        canvas.fillTriangle(48, 34, 54, 34, 51, 30,
                            rgb(126, 175, 175));
    }
    if (model.scroll < shopItemMaxScroll(model.itemCount) - 0.5f) {
        canvas.fillTriangle(48, 215, 54, 215, 51, 219,
                            rgb(126, 175, 175));
    }

    static constexpr const char* CATEGORY_LABELS[] = {
        "DAILY", "EXPLORE", "SELL",
    };
    text(canvas, 68, 34,
         CATEGORY_LABELS[static_cast<uint8_t>(model.category)],
         rgb(126, 175, 175));
    canvas.drawFastHLine(68, 47, 108, rgb(56, 87, 89));

    if (selected < 0 || !model.state) {
        text(canvas, 78, 98,
             model.mode == ItemListMode::SELL ? "NOTHING" : "NO ITEMS",
             rgb(126, 145, 145));
        if (model.mode == ItemListMode::SELL) {
            text(canvas, 82, 114, "TO SELL", rgb(126, 145, 145));
        }
    } else {
        Game::ItemId item = itemForRow(model, static_cast<uint8_t>(selected));
        const char* name = Game::ShopService::shortName(item);
        text(canvas, 68, 56, name, rgb(226, 238, 233));

        uint16_t price = model.mode == ItemListMode::SELL
            ? Game::ShopService::sellPrice(item)
            : Game::ShopService::buyPrice(item);
        char priceText[16];
        std::snprintf(priceText, sizeof(priceText), "PRICE C%u", price);
        text(canvas, 68, 80, priceText, rgb(248, 210, 105));

        char owned[12];
        std::snprintf(owned, sizeof(owned), "OWNED X%u",
                      Game::ItemInventory::count(*model.state, item));
        text(canvas, 68, 99, owned, rgb(239, 196, 154));
        text(canvas, 68, 124,
             Game::ShopService::shortDescription(item),
             rgb(126, 175, 175));

        uint16_t actionColor = model.pressedItem == selected
            ? rgb(48, 74, 68) : rgb(36, 54, 61);
        canvas.fillRoundRect(SHOP_ACTION_X, SHOP_ACTION_Y,
                             SHOP_ACTION_WIDTH, SHOP_ACTION_HEIGHT,
                             4, actionColor);
        canvas.drawRoundRect(SHOP_ACTION_X, SHOP_ACTION_Y,
                             SHOP_ACTION_WIDTH, SHOP_ACTION_HEIGHT,
                             4, rgb(82, 117, 117));
        const char* action = model.mode == ItemListMode::SELL ? "SELL" : "BUY";
        text(canvas,
             SHOP_ACTION_X +
                 (SHOP_ACTION_WIDTH - static_cast<int>(std::strlen(action)) * 6) / 2,
             SHOP_ACTION_Y + 14, action, rgb(115, 226, 183));
    }

    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    drawItemConfirm(canvas, model);
    canvas.clearClipRect();
}

int roomMenuItemAt(int x, int y) {
    constexpr int rowHeight = 60;
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int index = (y - MENU_CONTENT_TOP) / rowHeight;
    return index < 3 ? index : -1;
}

void renderRoomMenuScreen(Canvas565& canvas, const RoomMenuViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* LABELS[] = {"FOOD", "SHOWER", "BACK"};
    static constexpr const char* DETAILS[] = {
        "ROOM SUPPLIES", "WASH YOUR PET", "RETURN",
    };
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    PixelRenderer::canvas().setClipRect(
        0, rowBegin, canvas.width(), rowEnd - rowBegin);
    canvas.fillRect(0, 0, canvas.width(), canvas.height(), rgb(12, 18, 25));
    canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT, rgb(19, 31, 39));
    canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1, rgb(56, 87, 89));
    drawBackIcon(canvas);
    text(canvas, 36, 8, "ROOM", rgb(115, 226, 183));

    uint16_t foodCount = 0;
    if (model.state) {
        for (uint8_t stock : model.state->room.food) foodCount += stock;
    }
    for (int index = 0; index < 3; ++index) {
        int y = MENU_CONTENT_TOP + index * 60;
        uint16_t background = index == model.pressedItem
            ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect(6, y + 3, 172, 54, 4, background);
        if (index == 0) {
            if (!GameAssets::drawCentered(
                    GameAssets::Kind::ITEM_NORMAL_FOOD, 31, y + 28, 0.9f)) {
                text(canvas, 28, y + 24, "?", rgb(248, 210, 105));
            }
        } else if (index == 1) {
            if (!GameAssets::drawCentered(
                    GameAssets::Kind::SHOWER_MENU_SPRINKLER,
                    31, y + 28, 0.72f)) {
                text(canvas, 28, y + 24, "?", rgb(126, 175, 175));
            }
        } else {
            canvas.drawFastHLine(19, y + 28, 23, rgb(115, 226, 183));
            canvas.drawLine(19, y + 28, 27, y + 20, rgb(115, 226, 183));
            canvas.drawLine(19, y + 28, 27, y + 36, rgb(115, 226, 183));
        }
        text(canvas, 58, y + 13, LABELS[index],
             index == 2 ? rgb(115, 226, 183) : rgb(226, 238, 233));
        text(canvas, 58, y + 32, DETAILS[index], rgb(126, 145, 145));
        if (index == 0) {
            char stock[10];
            std::snprintf(stock, sizeof(stock), "X%u", foodCount);
            text(canvas, 164 - static_cast<int>(std::strlen(stock)) * 6,
                 y + 13, stock, rgb(248, 210, 105));
        }
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

namespace {

constexpr int SHOWER_PET_X = 92;
constexpr int SHOWER_PET_Y = 106;
constexpr int SHOWER_TOOLBAR_Y = 176;
constexpr int SHOWER_TOOL_BUTTON_W = 42;

void drawShowerPet(Canvas565& canvas, uint16_t speciesId) {
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        drawFallbackPet(canvas, SHOWER_PET_X, 158);
        return;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return;
    float scale = std::min(100.0f / width, 112.0f / height);
    scale = std::min(scale, 1.4f);
    int drawWidth = static_cast<int>(width * scale);
    int drawHeight = static_cast<int>(height * scale);
    PokemonSprites::drawFrameScaled(
        frame, SHOWER_PET_X - drawWidth / 2,
        SHOWER_PET_Y - drawHeight / 2, scale, false);
}

void drawShowerBubbles(const ShowerViewModel& model) {
    static constexpr int8_t SPOTS[][2] = {
        {-24, -30}, {20, -25}, {-8, -14}, {25, 0},
        {-25, 4}, {8, 14}, {-16, 29}, {20, 28},
    };
    uint8_t count = std::min<uint8_t>(model.soapProgress, 8);
    if (model.rinseProgress >= 100) return;
    if (model.rinseProgress > 0) {
        count = static_cast<uint8_t>(
            count * (100 - model.rinseProgress) / 100);
    }
    uint8_t stage = model.brushProgress == 0
        ? 0 : std::min<uint8_t>(4, 1 + model.brushProgress / 2);
    for (uint8_t index = 0; index < count; ++index) {
        GameAssets::Kind kind = static_cast<GameAssets::Kind>(
            static_cast<uint16_t>(GameAssets::Kind::SHOWER_BUBBLE_0) + stage);
        GameAssets::drawCentered(kind,
                                 SHOWER_PET_X + SPOTS[index][0],
                                 SHOWER_PET_Y + SPOTS[index][1],
                                 stage >= 3 ? 0.82f : 0.72f);
    }
    if (model.brushProgress >= 8 && model.rinseProgress == 0) {
        GameAssets::drawCenteredAlpha(
            GameAssets::Kind::SHOWER_BUBBLE_5,
            SHOWER_PET_X, SHOWER_PET_Y + 12, 0.9f, 210);
    }
}

void drawShowerToolbar(Canvas565& canvas, const ShowerViewModel& model) {
    static constexpr GameAssets::Kind ICONS[] = {
        GameAssets::Kind::SHOWER_MENU_SOAP,
        GameAssets::Kind::SHOWER_MENU_BRUSH,
        GameAssets::Kind::SHOWER_MENU_SPRINKLER,
        GameAssets::Kind::COUNT,
    };
    static constexpr const char* LABELS[] = {"SOAP", "BRUSH", "RINSE", "EXIT"};
    for (int index = 0; index < 4; ++index) {
        int x = 3 + index * 45;
        bool selected = index == model.pressedItem;
        canvas.fillRoundRect(x, SHOWER_TOOLBAR_Y, SHOWER_TOOL_BUTTON_W, 44, 4,
                             selected ? rgb(48, 74, 68) : rgb(24, 34, 42));
        canvas.drawRoundRect(x, SHOWER_TOOLBAR_Y, SHOWER_TOOL_BUTTON_W, 44, 4,
                             rgb(67, 97, 101));
        if (index < 3) {
            GameAssets::drawCentered(ICONS[index], x + 21,
                                     SHOWER_TOOLBAR_Y + 18, 0.56f);
        } else {
            canvas.drawFastHLine(x + 12, SHOWER_TOOLBAR_Y + 17, 18,
                                 rgb(239, 143, 148));
            canvas.drawLine(x + 12, SHOWER_TOOLBAR_Y + 17,
                            x + 19, SHOWER_TOOLBAR_Y + 10,
                            rgb(239, 143, 148));
            canvas.drawLine(x + 12, SHOWER_TOOLBAR_Y + 17,
                            x + 19, SHOWER_TOOLBAR_Y + 24,
                            rgb(239, 143, 148));
        }
        int labelX = x + (SHOWER_TOOL_BUTTON_W -
                          static_cast<int>(std::strlen(LABELS[index])) * 6) / 2;
        text(canvas, labelX, SHOWER_TOOLBAR_Y + 33, LABELS[index],
             index == 3 ? rgb(239, 143, 148) : rgb(126, 175, 175));
    }
}

void drawShowerSoapPicker(Canvas565& canvas, const ShowerViewModel& model) {
    canvas.fillRoundRect(8, 66, 168, 98, 6, rgb(17, 27, 34));
    canvas.drawRoundRect(8, 66, 168, 98, 6, rgb(82, 117, 117));
    text(canvas, 58, 75, "CHOOSE SOAP", rgb(226, 238, 233));
    for (uint8_t index = 0; index < Game::SOAP_VARIANT_COUNT; ++index) {
        int x = 13 + index * 55;
        uint8_t count = model.state ? model.state->bag.soap[index] : 0;
        canvas.fillRoundRect(x, 94, 49, 59, 4,
                             count > 0 ? rgb(28, 42, 48) : rgb(20, 27, 31));
        GameAssets::Kind kind = static_cast<GameAssets::Kind>(
            static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) + index);
        GameAssets::drawCenteredAlpha(kind, x + 24, 116, 0.82f,
                                      count > 0 ? 255 : 80);
        char stock[8];
        std::snprintf(stock, sizeof(stock), "X%u", count);
        text(canvas, x + 17, 140, stock,
             count > 0 ? rgb(248, 210, 105) : rgb(91, 104, 104));
    }
}

void drawShowerExitConfirm(Canvas565& canvas, const ShowerViewModel& model) {
    canvas.fillRoundRect(10, 116, 164, 98, 6, rgb(17, 27, 34));
    canvas.drawRoundRect(10, 116, 164, 98, 6, rgb(82, 117, 117));
    text(canvas, 43, 129, "FOAM REMAINS", rgb(248, 210, 105));
    text(canvas, 58, 148, "LEAVE BATH?", rgb(226, 238, 233));
    canvas.fillRoundRect(18, 174, 70, 36, 4,
                         model.exitConfirmYes ? rgb(48, 74, 68)
                                              : rgb(36, 54, 61));
    canvas.fillRoundRect(96, 174, 70, 36, 4,
                         !model.exitConfirmYes ? rgb(76, 48, 54)
                                               : rgb(46, 39, 43));
    text(canvas, 39, 188, "LEAVE", rgb(115, 226, 183));
    text(canvas, 116, 188, "STAY", rgb(239, 143, 148));
}

}  // namespace

bool showerBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int showerMenuItemAt(int x, int y) {
    if (x < 3 || x >= 183 || y < SHOWER_TOOLBAR_Y || y >= 224) return -1;
    int index = (x - 3) / 45;
    return index < 4 ? index : -1;
}

int showerSoapItemAt(int x, int y) {
    if (x < 13 || x >= 178 || y < 94 || y >= 153) return -1;
    int index = (x - 13) / 55;
    return index < Game::SOAP_VARIANT_COUNT ? index : -1;
}

int showerExitChoiceAt(int x, int y) {
    if (y < 174 || y >= 210) return -1;
    if (x >= 18 && x < 88) return 0;
    if (x >= 96 && x < 166) return 1;
    return -1;
}

bool showerToolAt(int x, int y, int toolX, int toolY) {
    int dx = x - toolX;
    int dy = y - toolY;
    return dx * dx + dy * dy <= 28 * 28;
}

void renderShowerScreen(Canvas565& canvas, const ShowerViewModel& model,
                        uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    PixelRenderer::canvas().setClipRect(
        0, rowBegin, canvas.width(), rowEnd - rowBegin);
    canvas.fillRect(0, 0, canvas.width(), canvas.height(), rgb(24, 43, 37));
    if (!GameAssets::draw(GameAssets::Kind::SHOWER_BACKGROUND, -28, 28)) {
        canvas.fillRect(0, 28, canvas.width(), 135, rgb(178, 116, 57));
    }
    canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT, rgb(19, 31, 39));
    canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1, rgb(56, 87, 89));
    drawBackIcon(canvas);
    text(canvas, 36, 8, "SHOWER", rgb(115, 226, 183));

    drawShowerPet(canvas, model.speciesId);
    drawShowerBubbles(model);

    if (model.mode == ShowerMode::RINSING) {
        uint8_t alpha = static_cast<uint8_t>(70 + model.rinseProgress / 2);
        PixelRenderer::fillRectAlpha(18, 28, 148, 135,
                                     rgb(74, 190, 232), alpha);
        for (int stream = 0; stream < 10; ++stream) {
            int x = 24 + stream * 15;
            int phase = (model.rinseProgress * 3 + stream * 17) % 44;
            canvas.drawFastVLine(x, 28 + phase, 55,
                                 rgb(184, 236, 249));
        }
        GameAssets::drawCentered(GameAssets::Kind::SHOWER_SPRINKLER,
                                 92, 43, 0.8f);
    }

    canvas.fillRect(0, 164, canvas.width(), 60, rgb(12, 18, 25));
    drawShowerToolbar(canvas, model);

    if (model.mode == ShowerMode::SOAPING ||
        model.mode == ShowerMode::BRUSHING) {
        GameAssets::Kind tool = model.mode == ShowerMode::SOAPING
            ? static_cast<GameAssets::Kind>(
                static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) +
                model.soapIndex)
            : GameAssets::Kind::SHOWER_BRUSH;
        GameAssets::drawCentered(tool, model.toolX, model.toolY,
                                 model.toolDragging ? 1.0f : 0.9f);
        uint8_t progress = model.mode == ShowerMode::SOAPING
            ? model.soapProgress : model.brushProgress;
        canvas.fillRect(48, 167, 88, 5, rgb(39, 45, 50));
        canvas.fillRect(49, 168, 86 * progress / 8, 3,
                        rgb(115, 226, 183));
    }

    if (model.mode == ShowerMode::SOAP_SELECT) {
        drawShowerSoapPicker(canvas, model);
    } else if (model.mode == ShowerMode::COMPLETE) {
        for (uint8_t index = 0; index < model.completionHearts; ++index) {
            drawHeart(canvas, 76 + index * 16, 62, rgb(242, 111, 126));
        }
        text(canvas, 59, 145, "ALL CLEAN", rgb(115, 226, 183));
    } else if (model.mode == ShowerMode::EXIT_CONFIRM) {
        drawShowerExitConfirm(canvas, model);
    }

    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

}  // namespace AmoledV1
