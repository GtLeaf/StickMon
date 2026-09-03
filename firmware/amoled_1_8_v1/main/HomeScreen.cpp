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
#include "assets/PokemonMotion.h"
#include "core/AppSceneFlow.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreRouteGeometry.h"
#include "game/HomeHud.h"
#include "game/ItemInventory.h"
#include "game/MoveManagementService.h"
#include "game/Species.h"
#include "game/TeamRoster.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"
#include "presentation/Canvas565.h"
#include "presentation/HudRenderer.h"
#include "presentation/PixelRenderer.h"
#include "presentation/QrCodeGen.h"

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
constexpr int MENU_CELL_WIDTH = 82;
constexpr int MENU_CELL_HEIGHT = 76;
constexpr int MENU_ROW_HEIGHT = 82;
constexpr int MENU_GRID_LEFT = 4;
constexpr int MENU_GRID_GAP = 8;
constexpr int MENU_VIEWPORT_HEIGHT = 224 - MENU_CONTENT_TOP;
constexpr int MAIN_MENU_VIEWPORT_HEIGHT = 224 - MAIN_MENU_CONTENT_TOP;
// The computer page is a compact vertical list. Keep its geometry separate
// from the two-column main menu so all four entries fit on one screen.
constexpr int COMPUTER_MENU_ROW_HEIGHT = 43;
constexpr int COMPUTER_MENU_CELL_HEIGHT = 39;
constexpr int ITEM_ROW_HEIGHT = 48;
constexpr int EXPLORE_MENU_PANEL_WIDTH = 60;
constexpr int EXPLORE_MENU_PANEL_ROW_TOP = 9;
constexpr int EXPLORE_MENU_PANEL_ROW_HEIGHT = 30;
constexpr int SHOP_RAIL_DIVIDER_X = SHOP_LEFT_PANEL_WIDTH;
constexpr int SHOP_GRID_LEFT = 61;
constexpr int SHOP_GRID_COLUMN_WIDTH = 58;
constexpr int SHOP_GRID_ROW_HEIGHT = 50;
constexpr int SHOP_SECTION_HEADER_HEIGHT = 24;
constexpr int SHOP_SECTION_GAP = 4;
constexpr int SHOP_DETAIL_BUTTON_Y = 174;
constexpr int SHOP_DETAIL_BUTTON_HEIGHT = 36;
// Product cells show the 36x36 source art at native logical size. AMOLED's
// 2x canvas mapping presents it at 72x72 physical pixels.
constexpr float SHOP_GRID_ICON_SCALE = 1.0f;
constexpr float SHOP_DETAIL_ICON_END_SCALE = 1.4f;
constexpr uint32_t EXPLORE_PREVIEW_CYCLE_MS = 2800;
constexpr uint32_t EXPLORE_PREVIEW_MOVE_MS = 500;
constexpr uint32_t EXPLORE_PREVIEW_HOLD_MS =
    EXPLORE_PREVIEW_CYCLE_MS - EXPLORE_PREVIEW_MOVE_MS;
constexpr int EXPLORE_PREVIEW_CENTER_Y = 126;
constexpr int EXPLORE_PREVIEW_GAP = 5;
constexpr int EXPLORE_PREVIEW_MAX_WIDTH = 34;
constexpr int EXPLORE_PREVIEW_MAX_HEIGHT = 56;
// The shared explore artwork is authored at 240x135. Cover the portrait
// AMOLED canvas and crop the horizontal sides so the scene fills the page.
constexpr float EXPLORE_BACKGROUND_SCALE = 224.0f / 135.0f;
constexpr int EXPLORE_BACKGROUND_X = -108;
constexpr float BATTLE_BACKGROUND_SCALE = 224.0f / 135.0f;
constexpr int BATTLE_BACKGROUND_X = -107;
constexpr int BATTLE_FOOTER_Y = 176;
constexpr int BATTLE_FOOTER_HEIGHT = 224 - BATTLE_FOOTER_Y;

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue);

uint16_t* battleBackgroundCache = nullptr;
size_t battleBackgroundCachePixels = 0;
GameAssets::Kind cachedBattleBackground = GameAssets::Kind::COUNT;
int cachedBattleBackgroundWidth = 0;
int cachedBattleBackgroundHeight = 0;

bool drawBattleBackgroundLayer(Canvas565& canvas, GameAssets::Kind kind,
                               uint16_t rowBegin, uint16_t rowEnd) {
    const size_t pixels = static_cast<size_t>(canvas.physicalWidth()) *
                          canvas.physicalHeight();
    bool cacheMatches = battleBackgroundCache &&
        battleBackgroundCachePixels == pixels &&
        cachedBattleBackground == kind &&
        cachedBattleBackgroundWidth == canvas.physicalWidth() &&
        cachedBattleBackgroundHeight == canvas.physicalHeight();
    if (cacheMatches) {
        const int scale = canvas.coordinateScale();
        const size_t rowBytes = static_cast<size_t>(canvas.physicalWidth()) *
                                sizeof(uint16_t);
        for (uint16_t row = rowBegin * scale; row < rowEnd * scale; ++row) {
            std::memcpy(canvas.rawPixels() +
                            static_cast<size_t>(row) * canvas.physicalWidth(),
                        battleBackgroundCache +
                            static_cast<size_t>(row) * canvas.physicalWidth(),
                        rowBytes);
        }
        return true;
    }

    bool drawn = GameAssets::draw(
        kind, BATTLE_BACKGROUND_X, 0, BATTLE_BACKGROUND_SCALE);
    if (!drawn) {
        canvas.fillRect(0, 0, canvas.width(), canvas.height(),
                        rgb(12, 18, 25));
    }

    bool fullFrame = rowBegin == 0 && rowEnd == canvas.height();
    if (!fullFrame) return drawn;
    if (battleBackgroundCachePixels != pixels) {
        if (battleBackgroundCache) {
            Platform::memory().release(battleBackgroundCache);
        }
        battleBackgroundCache = static_cast<uint16_t*>(
            Platform::memory().allocate(pixels * sizeof(uint16_t), true));
        battleBackgroundCachePixels = battleBackgroundCache ? pixels : 0;
    }
    if (battleBackgroundCache) {
        std::memcpy(battleBackgroundCache, canvas.rawPixels(),
                    pixels * sizeof(uint16_t));
        cachedBattleBackground = kind;
        cachedBattleBackgroundWidth = canvas.physicalWidth();
        cachedBattleBackgroundHeight = canvas.physicalHeight();
    }
    return drawn;
}

constexpr const char* EXPLORE_AREA_NAMES[Game::EXPLORE_AREA_COUNT] = {
    Ui::Explore::GRASS_PATH,
    Ui::Explore::CREEK_SLOPE,
    Ui::Explore::TALL_GRASS_PARK,
    Ui::Explore::FROST_CRYSTAL_CAVE,
    Ui::Explore::MIST_FOREST_PATH,
    Ui::Explore::ANCIENT_WATERFALL_VALLEY,
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

const Glyph* findGlyphExact(char value) {
    for (const Glyph& glyph : FONT) {
        if (glyph.value == value) return &glyph;
    }
    return nullptr;
}

bool containsNonAscii(const char* value) {
    if (!value) return false;
    for (const uint8_t* it = reinterpret_cast<const uint8_t*>(value);
         *it; ++it) {
        if (*it >= 0x80) return true;
    }
    return false;
}

int textWidth(const char* value) {
    if (!value) return 0;
    const bool hasNonAscii = containsNonAscii(value);
    bool compactSupported = !hasNonAscii;
    if (compactSupported) {
        for (const uint8_t* it = reinterpret_cast<const uint8_t*>(value);
             *it; ++it) {
            if (!findGlyphExact(static_cast<char>(*it))) {
                compactSupported = false;
                break;
            }
        }
    }
    if (!hasNonAscii && PixelRenderer::canvas().coordinateScale() >= 2) {
        return static_cast<int>(std::strlen(value)) * 8;
    }
    if (!compactSupported && !hasNonAscii) {
        return static_cast<int>(std::strlen(value)) * 8;
    }
    int width = 0;
    const uint8_t* it = reinterpret_cast<const uint8_t*>(value);
    while (*it) {
        if (*it < 0x80) {
            width += 8;
            ++it;
            continue;
        }
        width += 16;
        ++it;
        while (*it && (*it & 0xC0) == 0x80) ++it;
    }
    return width;
}

void text(Canvas565& canvas, int x, int y, const char* value,
          uint16_t color, int scale = 1) {
    if (!value || scale <= 0) return;
    // AMOLED uses the native 32px font for both CJK and ASCII. The legacy
    // 5x7 path remains for the 1x Stick S3 canvas.
    if (canvas.coordinateScale() >= 2) {
        PixelRenderer::text(canvas, x, y, value, color, 1);
        return;
    }
    bool compactSupported = true;
    for (const uint8_t* it = reinterpret_cast<const uint8_t*>(value);
         *it; ++it) {
        if (*it >= 0x80 || !findGlyphExact(static_cast<char>(*it))) {
            compactSupported = false;
            break;
        }
    }
    if (containsNonAscii(value) || !compactSupported) {
        PixelRenderer::text(canvas, x, y, value, color, 1);
        return;
    }
    int cursor = x;
    for (const char* it = value; *it; ++it) {
        const Glyph* glyph = findGlyphExact(*it);
        if (!glyph) continue;
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

#if STICKMON_HAS_CLAW
// Draws a Wi-Fi pairing QR (scanning it joins the setup hotspot directly) and
// returns the bottom Y of the QR block, so the caller can flow text below it.
// When the portal credentials are unavailable no QR is drawn and the reserved
// top position is returned instead.
int drawClawQr(Canvas565& canvas, const char* ssid, const char* password) {
    constexpr int QUIET_MODULES = 4;  // Spec-mandated quiet zone.
    constexpr int QR_MODULE = 3;
    constexpr int QR_TOP = 30;
    if (!ssid || !ssid[0] || !password || !password[0]) return QR_TOP;
    // The SSID/password alphabets never contain the reserved characters
    // (\ ; , : "), so the payload needs no escaping.
    char payload[96];
    std::snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;",
                  ssid, password);
    uint8_t modules[Stickmon::QrCodeGen::MAX_MATRIX_BYTES];
    const int size =
        Stickmon::QrCodeGen::encode(payload, modules, sizeof(modules));
    if (size <= 0) return QR_TOP;
    const int block = (size + 2 * QUIET_MODULES) * QR_MODULE;
    const int blockX = (canvas.width() - block) / 2;
    canvas.fillRect(blockX, QR_TOP, block, block, rgb(255, 255, 255));
    const uint16_t black = rgb(0, 0, 0);
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            if (!modules[row * size + column]) continue;
            canvas.fillRect(blockX + (column + QUIET_MODULES) * QR_MODULE,
                            QR_TOP + (row + QUIET_MODULES) * QR_MODULE,
                            QR_MODULE, QR_MODULE, black);
        }
    }
    return QR_TOP + block;
}

uint16_t clawLogLevelColor(Stickmon::ClawStatusLog::Level level) {
    switch (level) {
    case Stickmon::ClawStatusLog::Level::OK: return rgb(115, 226, 183);
    case Stickmon::ClawStatusLog::Level::WARN: return rgb(248, 210, 105);
    case Stickmon::ClawStatusLog::Level::ERROR: return rgb(240, 110, 110);
    default: return rgb(226, 238, 233);
    }
}

// Maps the raw esp-claw QR login phase token to a short UI label.
const char* clawWechatPhaseText(const char* phase, bool persisted) {
    if (persisted) return Ui::Amoled::CLAW_WECHAT_SAVED;
    if (std::strcmp(phase, "waiting_scan") == 0) {
        return Ui::Amoled::CLAW_WECHAT_WAITING;
    }
    if (std::strcmp(phase, "scanned") == 0) {
        return Ui::Amoled::CLAW_WECHAT_SCANNED;
    }
    if (std::strcmp(phase, "confirmed") == 0) {
        return Ui::Amoled::CLAW_WECHAT_LOGGED_IN;
    }
    if (std::strcmp(phase, "expired") == 0) {
        return Ui::Amoled::CLAW_WECHAT_EXPIRED;
    }
    if (std::strcmp(phase, "cancelled") == 0) {
        return Ui::Amoled::CLAW_WECHAT_CANCELLED;
    }
    if (std::strcmp(phase, "error") == 0) {
        return Ui::Amoled::CLAW_WECHAT_FAILED;
    }
    return Ui::Amoled::CLAW_WECHAT_IDLE;
}

uint16_t clawWechatPhaseColor(const char* phase, bool persisted) {
    if (persisted || std::strcmp(phase, "confirmed") == 0) {
        return rgb(115, 226, 183);
    }
    if (std::strcmp(phase, "error") == 0) return rgb(240, 110, 110);
    if (std::strcmp(phase, "expired") == 0) return rgb(248, 210, 105);
    return rgb(126, 145, 145);
}

void drawClawTabs(Canvas565& canvas, bool logView) {
    for (int tab = 0; tab < 2; ++tab) {
        const bool active = (tab == 1) == logView;
        const int x = tab == 0 ? CLAW_TAB_CONNECT_LEFT : CLAW_TAB_LOG_LEFT;
        canvas.fillRoundRect(x, 2, CLAW_TAB_WIDTH, HEADER_HEIGHT - 4, 3,
                             active ? rgb(42, 61, 68) : rgb(24, 34, 42));
        const char* label = tab == 0 ? Ui::Amoled::CLAW_TAB_CONNECT
                                     : Ui::Amoled::CLAW_TAB_LOG;
        text(canvas, x + (CLAW_TAB_WIDTH - textWidth(label)) / 2, 4, label,
             active ? rgb(115, 226, 183) : rgb(126, 145, 145));
    }
}
#endif

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
    int width = textWidth(value) + 14;
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

void drawHeartBurst(Canvas565& canvas, int x, int y, uint16_t ageMs) {
    constexpr uint16_t burstColor = 0xFFFF;
    int phase = std::min<int>(8, ageMs / 50);
    int radius = 5 + phase * 2;
    int length = 3 + phase;
    canvas.drawLine(x - radius, y, x - radius - length, y, burstColor);
    canvas.drawLine(x + radius, y, x + radius + length, y, burstColor);
    canvas.drawLine(x, y - radius, x, y - radius - length, burstColor);
    canvas.drawLine(x, y + radius, x, y + radius + length, burstColor);
    if (phase >= 2) {
        canvas.drawLine(x - radius + 1, y - radius + 1,
                        x - radius - length + 2, y - radius - length + 2,
                        burstColor);
        canvas.drawLine(x + radius - 1, y + radius - 1,
                        x + radius + length - 2, y + radius + length - 2,
                        burstColor);
    }
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

#if STICKMON_ENABLE_DEBUG_FEATURES
void drawPet(Canvas565& canvas, const HomeViewModel& model);
uint16_t blendShadowRgb565(uint16_t background, uint16_t color,
                           uint8_t alpha);

constexpr int DEBUG_CONTACT_PROMPT_X = 8;
constexpr int DEBUG_CONTACT_PROMPT_Y = 112;
constexpr int DEBUG_CONTACT_PROMPT_W = 168;
constexpr int DEBUG_CONTACT_PROMPT_H = 58;

void drawDebugContactPrompt(Canvas565& canvas) {
    canvas.fillRoundRect(DEBUG_CONTACT_PROMPT_X, DEBUG_CONTACT_PROMPT_Y,
                         DEBUG_CONTACT_PROMPT_W, DEBUG_CONTACT_PROMPT_H, 5,
                         rgb(17, 27, 34));
    canvas.drawRoundRect(DEBUG_CONTACT_PROMPT_X, DEBUG_CONTACT_PROMPT_Y,
                         DEBUG_CONTACT_PROMPT_W, DEBUG_CONTACT_PROMPT_H, 5,
                         rgb(115, 226, 183));
    text(canvas, DEBUG_CONTACT_PROMPT_X +
             (DEBUG_CONTACT_PROMPT_W - textWidth(Ui::ContactVisit::KNOCK)) / 2,
         DEBUG_CONTACT_PROMPT_Y + 9, Ui::ContactVisit::KNOCK,
         rgb(226, 238, 233));
    text(canvas, DEBUG_CONTACT_PROMPT_X + 47, DEBUG_CONTACT_PROMPT_Y + 36,
         Ui::ContactVisit::YES, rgb(248, 210, 105));
    text(canvas, DEBUG_CONTACT_PROMPT_X + 112, DEBUG_CONTACT_PROMPT_Y + 36,
         Ui::ContactVisit::NO, rgb(239, 143, 148));
}

void drawDebugContactGuest(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.debugContactActive || model.debugContactSpeciesId == 0) return;
    HomeViewModel guest = model;
    guest.speciesId = model.debugContactSpeciesId;
    guest.petCenterX = 135;
    guest.petGroundY = 151;
    guest.petDirection = PokemonSprites::WalkDirection::LEFT;
    guest.petAction = HomeViewModel::PetVisualAction::IDLE;
    guest.petResting = false;
    guest.showHearts = false;
    drawPet(canvas, guest);
}

void drawDebugPairChaser(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.debugPairChaseActive || model.debugPairSpeciesId == 0) return;
    HomeViewModel chaser = model;
    chaser.speciesId = model.debugPairSpeciesId;
    chaser.petCenterX = model.debugPairCenterX;
    chaser.petGroundY = model.debugPairGroundY;
    chaser.petDirection = model.debugPairDirection;
    chaser.petFrame = model.debugPairFrame;
    chaser.petAction = HomeViewModel::PetVisualAction::WALKING;
    chaser.petResting = false;
    chaser.showHearts = false;
    drawPet(canvas, chaser);
}

void fillRadialDebugLight(Canvas565& canvas, int centerX, int centerY,
                          int radiusX, int radiusY, uint16_t color,
                          uint8_t maxAlpha) {
    if (radiusX <= 0 || radiusY <= 0 || maxAlpha == 0) return;
    for (int y = centerY - radiusY; y <= centerY + radiusY; ++y) {
        float dy = static_cast<float>(y - centerY) / radiusY;
        for (int x = centerX - radiusX; x <= centerX + radiusX; ++x) {
            float dx = static_cast<float>(x - centerX) / radiusX;
            float distanceSq = dx * dx + dy * dy;
            if (distanceSq > 1.0f) continue;
            uint8_t alpha = static_cast<uint8_t>(
                maxAlpha * (1.0f - distanceSq));
            if (alpha == 0) continue;
            canvas.drawPixel(x, y,
                             blendShadowRgb565(canvas.readPixel(x, y),
                                               color, alpha));
        }
    }
}

void drawDebugLight(Canvas565& canvas, const HomeViewModel& model) {
    if (model.debugLightSource == 0) return;
    int lightX = model.petCenterX;
    int lightY = model.petGroundY - 32;
    switch (model.debugLightSource) {
    case 1: lightX = 44; lightY = HOME_ROOM_TOP + 30; break;
    case 2: lightX = canvas.width() / 2; lightY = HOME_ROOM_TOP + 20; break;
    case 3: lightX = canvas.width() - 44; lightY = HOME_ROOM_TOP + 30; break;
    case 4: lightX = 34; lightY = HOME_ROOM_TOP + HOME_ROOM_HEIGHT / 2; break;
    case 5: lightX = canvas.width() - 34;
            lightY = HOME_ROOM_TOP + HOME_ROOM_HEIGHT / 2; break;
    default: break;
    }
    if (model.night) {
        fillRadialDebugLight(canvas, lightX, lightY, 58, 42,
                             rgb(255, 210, 128), 88);
        fillRadialDebugLight(canvas, lightX, lightY, 112, 76,
                             rgb(255, 151, 92), 30);
    } else {
        fillRadialDebugLight(canvas, lightX, lightY, 68, 46,
                             rgb(255, 226, 150), 30);
        fillRadialDebugLight(canvas, lightX, lightY, 118, 76,
                             rgb(255, 176, 96), 10);
    }
    canvas.fillCircle(lightX, lightY, 3, rgb(255, 236, 158));
    canvas.drawCircle(lightX, lightY, 5, rgb(255, 169, 79));
}

void drawDebugWalkBoundary(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.debugBoundaryVisible) return;
    RoomResource& room = RoomResource::ins();
    uint8_t count = room.walkPolygonCount();
    const RoomResource::Point* polygon = room.walkPolygon();
    if (count < 2 || !polygon) return;

    const uint16_t outline = rgb(0, 0, 0);
    const uint16_t red = rgb(255, 32, 32);
    auto pointX = [&](uint8_t index) {
        return static_cast<int>(polygon[index].x) - model.cameraX;
    };
    auto pointY = [&](uint8_t index) {
        return HOME_ROOM_TOP + static_cast<int>(polygon[index].y) -
               model.cameraY;
    };
    int previousX = pointX(0);
    int previousY = pointY(0);
    for (uint8_t i = 1; i <= count; ++i) {
        uint8_t index = i == count ? 0 : i;
        int x = pointX(index);
        int y = pointY(index);
        canvas.drawLine(previousX - 1, previousY, x - 1, y, outline);
        canvas.drawLine(previousX + 1, previousY, x + 1, y, outline);
        canvas.drawLine(previousX, previousY - 1, x, y - 1, outline);
        canvas.drawLine(previousX, previousY + 1, x, y + 1, outline);
        canvas.drawLine(previousX, previousY, x, y, red);
        canvas.fillCircle(x, y, 2, red);
        previousX = x;
        previousY = y;
    }
}
#endif

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

uint8_t shadowRgb565R(uint16_t color) {
    return static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
}

uint8_t shadowRgb565G(uint16_t color) {
    return static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
}

uint8_t shadowRgb565B(uint16_t color) {
    return static_cast<uint8_t>((color & 0x1F) * 255 / 31);
}

uint16_t blendShadowRgb565(uint16_t background, uint16_t color,
                           uint8_t alpha) {
    uint8_t inverse = static_cast<uint8_t>(255 - alpha);
    return rgb(
        static_cast<uint8_t>((shadowRgb565R(color) * alpha +
                              shadowRgb565R(background) * inverse) / 255),
        static_cast<uint8_t>((shadowRgb565G(color) * alpha +
                              shadowRgb565G(background) * inverse) / 255),
        static_cast<uint8_t>((shadowRgb565B(color) * alpha +
                              shadowRgb565B(background) * inverse) / 255));
}

void fillSoftShadow(Canvas565& canvas, int centerX, int centerY,
                    int radiusX, int radiusY, uint16_t color,
                    uint8_t maxAlpha) {
    if (radiusX <= 0 || radiusY <= 0 || maxAlpha == 0) return;
    for (int py = centerY - radiusY; py <= centerY + radiusY; ++py) {
        float dy = static_cast<float>(py - centerY) / radiusY;
        for (int px = centerX - radiusX; px <= centerX + radiusX; ++px) {
            float dx = static_cast<float>(px - centerX) / radiusX;
            float distanceSq = dx * dx + dy * dy;
            if (distanceSq > 1.0f) continue;
            uint8_t alpha = static_cast<uint8_t>(
                maxAlpha * (1.0f - distanceSq));
            if (alpha == 0) continue;
            uint16_t background = canvas.readPixel(px, py);
            canvas.drawPixel(px, py,
                             blendShadowRgb565(background, color, alpha));
        }
    }
}

void fillShadowCore(Canvas565& canvas, int centerX, int centerY,
                    int radiusX, int radiusY, uint16_t color,
                    uint8_t alpha) {
    if (radiusX <= 0 || radiusY <= 0 || alpha == 0) return;
    for (int py = centerY - radiusY; py <= centerY + radiusY; ++py) {
        float dy = static_cast<float>(py - centerY) / radiusY;
        for (int px = centerX - radiusX; px <= centerX + radiusX; ++px) {
            float dx = static_cast<float>(px - centerX) / radiusX;
            if (dx * dx + dy * dy > 1.0f) continue;
            uint16_t background = canvas.readPixel(px, py);
            canvas.drawPixel(px, py,
                             blendShadowRgb565(background, color, alpha));
        }
    }
}

void drawPet(Canvas565& canvas, const HomeViewModel& model) {
    PokemonSprites::PetAnimationProfile profile{};
    bool hasProfile = PokemonSprites::petAnimationProfile(
        model.speciesId, profile);
    PokemonSprites::WalkDirection direction = model.petDirection;
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    if (hasProfile) {
        if (model.petResting) {
            const uint8_t frameCount = profile.sleepingFrames > 0
                ? profile.sleepingFrames : 1;
            frame = PokemonSprites::findSpeciesSprite(
                model.speciesId,
                static_cast<PokemonSprites::SpriteKind>(
                    static_cast<uint16_t>(profile.sleepingBase) +
                    model.petFrame % frameCount));
        } else if (model.petAction == HomeViewModel::PetVisualAction::IDLE) {
            uint16_t directionIndex = 0;
            switch (direction) {
            case PokemonSprites::WalkDirection::LEFT:
                directionIndex = 2;
                break;
            case PokemonSprites::WalkDirection::UP:
                directionIndex = 4;
                break;
            case PokemonSprites::WalkDirection::RIGHT:
                directionIndex = profile.mirrorRightDirections ? 2 : 6;
                flipX = profile.mirrorRightDirections;
                break;
            case PokemonSprites::WalkDirection::DOWN:
            default:
                directionIndex = 0;
                break;
            }
            uint16_t kindValue = static_cast<uint16_t>(profile.idleBase) +
                directionIndex * profile.idleFrames +
                (profile.idleFrames > 0
                    ? model.petFrame % profile.idleFrames
                    : 0);
            frame = PokemonSprites::findSpeciesSprite(
                model.speciesId,
                static_cast<PokemonSprites::SpriteKind>(kindValue));
        } else {
            PokemonSprites::WalkingAnimation animation{};
            if (PokemonSprites::walkingAnimation(
                    model.speciesId, direction, animation) &&
                animation.frameCount > 0) {
                uint8_t frameIndex = 0;
                if (model.petAction == HomeViewModel::PetVisualAction::STOPPING) {
                    if (profile.motionMode ==
                            PokemonSprites::PetMotionMode::PINGPONG &&
                        profile.walkingFrames >= 2) {
                        frameIndex = model.petFrame == 0 ? 1 : 0;
                    } else if (profile.motionMode ==
                                   PokemonSprites::PetMotionMode::START_HOLD_END &&
                               profile.walkingFrames >= 3) {
                        frameIndex = profile.walkingFrames - 1;
                    }
                } else if (profile.motionMode ==
                               PokemonSprites::PetMotionMode::START_HOLD_END &&
                           profile.walkingFrames >= 3) {
                    frameIndex = model.petFrame == 0 ? 0 : 1;
                } else if (profile.motionMode ==
                               PokemonSprites::PetMotionMode::PINGPONG &&
                           profile.walkingFrames == 3 && !model.petLongMove) {
                    frameIndex = model.petFrame % 2;
                } else {
                    frameIndex = model.petFrame % animation.frameCount;
                }
                auto kind = static_cast<PokemonSprites::SpriteKind>(
                    static_cast<uint16_t>(animation.base) + frameIndex);
                frame = PokemonSprites::findSpeciesSprite(
                    model.speciesId, kind);
                flipX = animation.flipX;
            }
        }
    }
    if (!frame) {
        drawFallbackPet(canvas, model.petCenterX, model.petGroundY);
        return;
    }

    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    const int x = model.petCenterX - width / 2;
    const int y = model.petGroundY - height;
    const PokemonMotion::AirProfile air =
        PokemonMotion::airProfileForSpecies(model.speciesId);
    const bool floating = air.height > 0.0f;
    int radiusX = std::clamp(
        static_cast<int>(width * (floating ? 0.25f : 0.44f)),
        floating ? 10 : 16, floating ? 22 : 40);
    int radiusY = std::clamp(
        static_cast<int>(height * (floating ? 0.055f : 0.14f)),
        floating ? 3 : 6, floating ? 6 : 14);
    if (model.night) {
        radiusX = (radiusX * 11 + 5) / 10;
        radiusY = (radiusY * 11 + 5) / 10;
    }
    const int shadowY = model.petGroundY - height / 2 +
        PokemonSprites::frameGroundOffsetY(frame);
    const uint16_t shadowColor = model.night
        ? rgb(18, 16, 24) : rgb(36, 29, 24);
    const uint8_t outerAlpha = model.night
        ? (floating ? 84 : 122) : (floating ? 68 : 116);
    const uint8_t coreAlpha = model.night
        ? (floating ? 0 : 92) : (floating ? 0 : 86);
    fillSoftShadow(canvas, model.petCenterX, shadowY, radiusX, radiusY,
                   shadowColor, outerAlpha);
    if (!floating) {
        fillShadowCore(canvas, model.petCenterX, shadowY,
                       std::max(5, radiusX / 2),
                       std::max(2, radiusY / 2), shadowColor, coreAlpha);
    }
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

void drawBattleHpBar(Canvas565& canvas, int x, int y, int width,
                     uint8_t percent) {
    canvas.fillRect(x, y, width, 6, rgb(39, 45, 50));
    int filled = (width - 2) * percent / 100;
    uint16_t fillColor = percent > 50
        ? rgb(92, 222, 112)
        : (percent > 20 ? rgb(246, 204, 72) : rgb(232, 80, 84));
    if (filled > 0) canvas.fillRect(x + 1, y + 1, filled, 4, fillColor);
    canvas.drawRect(x, y, width, 6, rgb(0, 0, 0));
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

void drawExploreRoutePickup(Canvas565& canvas,
                            const ExploreRouteViewModel& model) {
    if (!model.pickupAvailable || !model.map ||
        model.pathIndex >= model.map->pathCount) return;
    const ExploreMapGenerator::Path& path =
        model.map->paths[model.pathIndex];
    if (model.pickupIndex >= path.pointCount) return;

    ExploreRouteGeometry::WorldPoint point =
        ExploreRouteGeometry::pathPoint(path, model.pickupIndex);
    int x = static_cast<int>(std::lround(point.x)) - model.cameraX;
    int y = static_cast<int>(std::lround(point.y)) - model.cameraY + 3;
    if (x < -13 || x >= canvas.width() + 13 ||
        y < HOME_HEADER_HEIGHT - 13 || y >= canvas.height() + 13) return;

    canvas.fillEllipse(x, y + 6, 7, 2, rgb(55, 68, 59));
    if (GameAssets::drawCentered(
            GameAssets::Kind::EXPLORE_PICKUP_BALL, x, y - 4)) {
        return;
    }
    if (GameAssets::drawCentered(
            GameAssets::Kind::ITEM_POKE_BALL, x, y - 4, 0.62f)) {
        return;
    }
    canvas.fillCircle(x, y - 4, 7, rgb(224, 69, 65));
    for (int row = 0; row <= 6; ++row) {
        int halfWidth = static_cast<int>(std::sqrt(
            49.0f - static_cast<float>(row * row)));
        canvas.drawFastHLine(x - halfWidth, y - 4 + row,
                             halfWidth * 2 + 1, 0xFFFF);
    }
    canvas.drawCircle(x, y - 4, 7, rgb(35, 39, 44));
    canvas.drawFastHLine(x - 7, y - 4, 14, rgb(35, 39, 44));
    canvas.fillCircle(x, y - 4, 2, 0xFFFF);
    canvas.drawCircle(x, y - 4, 2, rgb(35, 39, 44));
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

void drawExplorePreviewMember(Canvas565& canvas,
                              const PokemonSprites::SpriteFrame* frame,
                              int centerX, int centerY, bool hidden) {
    if (!frame) {
        canvas.drawCircle(centerX, centerY, 10, rgb(70, 88, 91));
        canvas.drawPixel(centerX, centerY, rgb(248, 210, 105));
        return;
    }

    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return;
    float scale = std::min(
        1.0f,
        std::min(static_cast<float>(EXPLORE_PREVIEW_MAX_WIDTH) / width,
                 static_cast<float>(EXPLORE_PREVIEW_MAX_HEIGHT) / height));
    int drawnWidth = std::max(1, static_cast<int>(std::lround(width * scale)));
    int drawnHeight = std::max(1, static_cast<int>(std::lround(height * scale)));
    int x = centerX - drawnWidth / 2;
    int y = centerY - drawnHeight / 2;
    if (hidden) {
        PokemonSprites::drawFrameSilhouette(
            frame, x, y, rgb(5, 10, 14));
    } else {
        PokemonSprites::drawFrameScaled(frame, x, y, scale, false);
    }
}

void drawExplorePreviewSlot(Canvas565& canvas,
                            const ExploreViewModel& model, uint8_t index,
                            int centerX) {
    if (index >= model.previewPool.count) return;
    drawExplorePreviewMember(canvas, model.previewFrames[index], centerX,
                             EXPLORE_PREVIEW_CENTER_Y,
                             model.previewHidden[index]);
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
        constexpr int MOOD_HEART_START_X = 10;
        constexpr int MOOD_HEART_GAP = 14;
        for (uint8_t index = 0; index < model.moodHearts; ++index) {
            drawHeart(canvas, MOOD_HEART_START_X + index * MOOD_HEART_GAP,
                      13, rgb(239, 103, 113));
        }
        if (model.moodBurstHeart < 5 && model.moodBurstAgeMs > 0) {
            drawHeartBurst(
                canvas,
                MOOD_HEART_START_X + model.moodBurstHeart * MOOD_HEART_GAP,
                13, model.moodBurstAgeMs);
        }
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
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugPairChaseActive &&
            model.debugPairGroundY < model.petGroundY) {
            drawDebugPairChaser(canvas, model);
        }
#endif
        drawPet(canvas, model);
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugPairChaseActive &&
            model.debugPairGroundY >= model.petGroundY) {
            drawDebugPairChaser(canvas, model);
        }
        drawDebugContactGuest(canvas, model);
#endif
        if (roomDrawn) {
            if (model.bowlFilled) {
                drawFoodContent(canvas, model.bowlCenterX, model.bowlCenterY);
            }
        } else {
            drawBowl(canvas, model.bowlFilled);
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugLightSource != 0) {
            drawDebugLight(canvas, model);
        }
        drawDebugWalkBoundary(canvas, model);
        if (model.debugContactPrompt) {
            drawDebugContactPrompt(canvas);
        }
#endif
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
    int rowCount = (MAIN_MENU_ITEM_COUNT + 1) / 2;
    int contentHeight = rowCount * MENU_ROW_HEIGHT;
    return static_cast<float>(
        std::max(0, contentHeight - MAIN_MENU_VIEWPORT_HEIGHT));
}

bool mainMenuBackAt(int, int) {
    return false;
}

int mainMenuItemAt(int x, int y, float scroll) {
    if (x < MENU_GRID_LEFT ||
        x >= MENU_GRID_LEFT + 2 * MENU_CELL_WIDTH + MENU_GRID_GAP ||
        y < MAIN_MENU_CONTENT_TOP || y >= 224) {
        return -1;
    }
    int contentY = static_cast<int>(y - MAIN_MENU_CONTENT_TOP + scroll);
    if (contentY < 0) return -1;
    int row = contentY / MENU_ROW_HEIGHT;
    int cellY = contentY % MENU_ROW_HEIGHT;
    if (cellY >= MENU_CELL_HEIGHT) return -1;

    int localX = x - MENU_GRID_LEFT;
    int column = localX / (MENU_CELL_WIDTH + MENU_GRID_GAP);
    int cellX = localX % (MENU_CELL_WIDTH + MENU_GRID_GAP);
    if (column >= 2 || cellX >= MENU_CELL_WIDTH) return -1;

    int index = row * 2 + column;
    return index < MAIN_MENU_ITEM_COUNT ? index : -1;
}

#if STICKMON_ENABLE_DEBUG_FEATURES
int debugContactChoiceAt(int x, int y) {
    if (y < DEBUG_CONTACT_PROMPT_Y + 28 ||
        y >= DEBUG_CONTACT_PROMPT_Y + DEBUG_CONTACT_PROMPT_H ||
        x < DEBUG_CONTACT_PROMPT_X ||
        x >= DEBUG_CONTACT_PROMPT_X + DEBUG_CONTACT_PROMPT_W) {
        return -1;
    }
    return x < DEBUG_CONTACT_PROMPT_X + DEBUG_CONTACT_PROMPT_W / 2 ? 0 : 1;
}
#endif

void renderMainMenu(Canvas565& canvas, const MenuViewModel& model,
                    uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    int contentTop = std::max<int>(rowBegin, MAIN_MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MAIN_MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MAIN_MENU_CONTENT_TOP, rgb(12, 18, 25));

    int scroll = static_cast<int>(std::lround(model.scroll));
    int rowCount = (MAIN_MENU_ITEM_COUNT + 1) / 2;
    for (int row = 0; row < rowCount; ++row) {
        int y = MAIN_MENU_CONTENT_TOP + row * MENU_ROW_HEIGHT - scroll;
        if (y + MENU_ROW_HEIGHT <= MAIN_MENU_CONTENT_TOP || y >= 224) continue;

        for (int column = 0; column < 2; ++column) {
            int index = row * 2 + column;
            if (index >= MAIN_MENU_ITEM_COUNT) break;
            int x = MENU_GRID_LEFT + column * (MENU_CELL_WIDTH + MENU_GRID_GAP);
            AppSceneFlow::MainMenuEntry entry =
                AppSceneFlow::mainMenuEntry(
                    static_cast<uint8_t>(index),
                    STICKMON_ENABLE_DEBUG_FEATURES != 0);
            bool pressed = index == model.pressedItem;
            uint16_t background = pressed ? rgb(42, 61, 68)
                                          : rgb(24, 34, 42);
            uint16_t border = pressed ? rgb(115, 226, 183)
                                      : rgb(56, 75, 84);
            canvas.fillRoundRect(x, y + 1, MENU_CELL_WIDTH,
                                 MENU_CELL_HEIGHT, 4, background);
            canvas.drawRoundRect(x, y + 1, MENU_CELL_WIDTH,
                                 MENU_CELL_HEIGHT, 4, border);
            if (entry.iconIndex < MenuAssets::MAIN_ICON_COUNT) {
                uint16_t offset = FlashStorage::readWord(
                    &MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex].offset);
                uint16_t length = FlashStorage::readWord(
                    &MenuAssets::MAIN_ICON_FRAMES[entry.iconIndex].length);
                canvas.drawRgb565Rle(
                    x + (MENU_CELL_WIDTH - MenuAssets::FRAME_W) / 2,
                    y + 7, MenuAssets::FRAME_W, MenuAssets::FRAME_H,
                    MenuAssets::MAIN_ICON_RLE, offset, length);
            }
            text(canvas, x + (MENU_CELL_WIDTH - textWidth(entry.shortLabel)) / 2,
                 y + 54, entry.shortLabel, ink);
        }
    }
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

#if STICKMON_ENABLE_DEBUG_FEATURES
namespace {

constexpr int DEBUG_CONTENT_TOP = 28;
constexpr int DEBUG_ROW_HEIGHT = 32;
constexpr int DEBUG_TEXT_Y_OFFSET = 8;
constexpr int DEBUG_ROW_LEFT = 6;
constexpr int DEBUG_ROW_WIDTH = 172;
constexpr int DEBUG_POPUP_X = 10;
constexpr int DEBUG_POPUP_Y = 42;
constexpr int DEBUG_POPUP_W = 164;
constexpr int DEBUG_POPUP_H = 140;
constexpr int DEBUG_POPUP_CONTROL_TEXT_OFFSET = 8;

uint8_t debugItemCount(DebugViewModel::Category category) {
    switch (category) {
    case DebugViewModel::Category::MONSTER: return 4;
    case DebugViewModel::Category::RESOURCE: return 2;
    case DebugViewModel::Category::ENV: return 3;
    case DebugViewModel::Category::MOTION: return 4;
    case DebugViewModel::Category::BATTLE: return 3;
    case DebugViewModel::Category::CONTACT_EVENT: return 4;
    case DebugViewModel::Category::ROOT:
    default: return 7;
    }
}

const char* debugItemLabel(DebugViewModel::Category category, uint8_t index) {
    if (index >= debugItemCount(category)) return Ui::BACK;
    switch (category) {
    case DebugViewModel::Category::MONSTER:
        return Ui::Debug::MONSTER_ITEMS[index];
    case DebugViewModel::Category::RESOURCE:
        return Ui::Debug::RESOURCE_ITEMS[index];
    case DebugViewModel::Category::ENV:
        return Ui::Debug::ENV_ITEMS[index];
    case DebugViewModel::Category::MOTION:
        return Ui::Debug::MOTION_ITEMS[index];
    case DebugViewModel::Category::BATTLE:
        return Ui::Debug::BATTLE_ITEMS[index];
    case DebugViewModel::Category::CONTACT_EVENT:
        return Ui::Debug::CONTACT_EVENT_ITEMS[index];
    case DebugViewModel::Category::ROOT:
    default:
        return Ui::Debug::ROOT_ITEMS[index];
    }
}

const char* debugCategoryTitle(DebugViewModel::Category category) {
    switch (category) {
    case DebugViewModel::Category::MONSTER: return Ui::Debug::CATEGORY_MONSTER;
    case DebugViewModel::Category::RESOURCE: return Ui::Debug::CATEGORY_RESOURCE;
    case DebugViewModel::Category::ENV: return Ui::Debug::CATEGORY_ENV;
    case DebugViewModel::Category::MOTION: return Ui::Debug::CATEGORY_MOTION;
    case DebugViewModel::Category::BATTLE: return Ui::Debug::CATEGORY_BATTLE;
    case DebugViewModel::Category::CONTACT_EVENT:
        return Ui::Debug::CATEGORY_CONTACT_EVENT;
    case DebugViewModel::Category::ROOT:
    default: return Ui::DEBUG;
    }
}

void drawDebugPopup(Canvas565& canvas, const DebugViewModel& model) {
    canvas.fillRoundRect(DEBUG_POPUP_X, DEBUG_POPUP_Y, DEBUG_POPUP_W,
                         DEBUG_POPUP_H, 6, rgb(17, 27, 34));
    canvas.drawRoundRect(DEBUG_POPUP_X, DEBUG_POPUP_Y, DEBUG_POPUP_W,
                         DEBUG_POPUP_H, 6, rgb(115, 226, 183));
    const bool timePopup = model.popup == DebugViewModel::Popup::SET_TIME;
    const char* title = timePopup ? Ui::Debug::TARGET_TIME : Ui::Debug::INPUT_ID;
    text(canvas, DEBUG_POPUP_X + (DEBUG_POPUP_W - textWidth(title)) / 2,
         DEBUG_POPUP_Y + 12, title, rgb(226, 238, 233));

    const uint8_t digitCount = timePopup ? 4 : 3;
    for (uint8_t index = 0; index < digitCount; ++index) {
        int x = DEBUG_POPUP_X + 18 + index * 34;
        bool focused = index == model.focus;
        canvas.fillRoundRect(x, DEBUG_POPUP_Y + 34, 26, 32, 4,
                             focused ? rgb(42, 61, 68) : rgb(24, 34, 42));
        canvas.drawRoundRect(x, DEBUG_POPUP_Y + 34, 26, 32, 4,
                             focused ? rgb(248, 210, 105) : rgb(67, 97, 101));
        char digit[2] = {static_cast<char>('0' + model.digits[index]), '\0'};
        text(canvas, x + 9,
             DEBUG_POPUP_Y + 34 + DEBUG_POPUP_CONTROL_TEXT_OFFSET, digit,
             focused ? rgb(248, 210, 105) : rgb(226, 238, 233));
    }
    canvas.fillRoundRect(DEBUG_POPUP_X + 18, DEBUG_POPUP_Y + 88, 60, 32, 4,
                         rgb(36, 54, 61));
    canvas.fillRoundRect(DEBUG_POPUP_X + 86, DEBUG_POPUP_Y + 88, 60, 32, 4,
                         rgb(91, 49, 55));
    text(canvas, DEBUG_POPUP_X + 37,
         DEBUG_POPUP_Y + 88 + DEBUG_POPUP_CONTROL_TEXT_OFFSET, Ui::Debug::YES,
         rgb(115, 226, 183));
    text(canvas, DEBUG_POPUP_X + 96,
         DEBUG_POPUP_Y + 88 + DEBUG_POPUP_CONTROL_TEXT_OFFSET,
         Ui::Debug::CANCEL,
         rgb(239, 143, 148));
}

}  // namespace

float debugMaxScroll(DebugViewModel::Category category) {
    return static_cast<float>(std::max(
        0, static_cast<int>(debugItemCount(category)) * DEBUG_ROW_HEIGHT -
            (224 - DEBUG_CONTENT_TOP)));
}

bool debugBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < DEBUG_CONTENT_TOP;
}

int debugItemAt(int x, int y, DebugViewModel::Category category,
                float scroll) {
    if (x < DEBUG_ROW_LEFT || x >= DEBUG_ROW_LEFT + DEBUG_ROW_WIDTH ||
        y < DEBUG_CONTENT_TOP || y >= 224) return -1;
    int contentY = y - DEBUG_CONTENT_TOP + static_cast<int>(std::lround(scroll));
    int index = contentY / DEBUG_ROW_HEIGHT;
    return index >= 0 && index < debugItemCount(category) ? index : -1;
}

int debugPopupChoiceAt(int x, int y) {
    if (y < DEBUG_POPUP_Y + 82 || y >= DEBUG_POPUP_Y + DEBUG_POPUP_H) return -1;
    if (x >= DEBUG_POPUP_X + 10 && x < DEBUG_POPUP_X + 82) return 0;
    if (x >= DEBUG_POPUP_X + 82 && x < DEBUG_POPUP_X + DEBUG_POPUP_W - 8) return 1;
    return -1;
}

int debugPopupDigitAt(int x, int y, uint8_t digitCount) {
    if (y < DEBUG_POPUP_Y + 28 || y >= DEBUG_POPUP_Y + 74) return -1;
    for (uint8_t index = 0; index < digitCount; ++index) {
        int left = DEBUG_POPUP_X + 18 + index * 34;
        if (x >= left && x < left + 26) return index;
    }
    return -1;
}

void renderDebugScreen(Canvas565& canvas, const DebugViewModel& model,
                       uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    canvas.fillRect(0, 0, canvas.width(), canvas.height(), rgb(12, 18, 25));
    canvas.fillRect(0, 0, canvas.width(), DEBUG_CONTENT_TOP,
                    rgb(19, 31, 39));
    canvas.fillRect(0, DEBUG_CONTENT_TOP - 1, canvas.width(), 1,
                    rgb(56, 87, 89));
    drawBackIcon(canvas);
    text(canvas, 36, 8, debugCategoryTitle(model.category),
         rgb(115, 226, 183));

    uint8_t count = debugItemCount(model.category);
    int scroll = static_cast<int>(std::lround(model.scroll));
    for (uint8_t index = 0; index < count; ++index) {
        int y = DEBUG_CONTENT_TOP + index * DEBUG_ROW_HEIGHT - scroll;
        if (y + DEBUG_ROW_HEIGHT <= DEBUG_CONTENT_TOP || y >= 224) continue;
        bool selected = index == model.cursor;
        uint16_t background = selected ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect(DEBUG_ROW_LEFT, y + 2, DEBUG_ROW_WIDTH,
                             DEBUG_ROW_HEIGHT - 5, 4, background);
        if (selected) canvas.fillRect(DEBUG_ROW_LEFT, y + 7, 3,
                                      DEBUG_ROW_HEIGHT - 15, rgb(248, 210, 105));
        text(canvas, DEBUG_ROW_LEFT + 10, y + DEBUG_TEXT_Y_OFFSET,
             debugItemLabel(model.category, index),
             selected ? rgb(248, 210, 105) : rgb(226, 238, 233));

        const char* value = nullptr;
        char buffer[24] = {};
        if (model.category == DebugViewModel::Category::MONSTER && index == 1 &&
            model.state && model.state->teamCount > 0) {
            std::snprintf(buffer, sizeof(buffer), Ui::Common::LEVEL_FMT,
                          model.state->team[0].level);
            value = buffer;
        } else if (model.category == DebugViewModel::Category::MONSTER && index == 2 &&
                   model.state) {
            uint16_t species = model.state->teamCount > 0
                ? model.state->team[0].speciesId : 0;
            std::snprintf(buffer, sizeof(buffer), Ui::Debug::CURRENT_ID_FMT,
                          species);
            value = buffer;
        } else if (model.category == DebugViewModel::Category::ENV && index == 0) {
            value = model.currentTime;
        } else if (model.category == DebugViewModel::Category::ENV && index == 1) {
            value = model.lightSource;
        } else if (model.category == DebugViewModel::Category::MOTION && index == 0) {
            value = model.tiltEnabled ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (model.category == DebugViewModel::Category::MOTION && index == 1) {
            value = model.boundaryVisible ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (model.category == DebugViewModel::Category::BATTLE && index == 1) {
            value = model.battleBoundsVisible ? Ui::Settings::ON : Ui::Settings::OFF;
        }
        if (value) {
            text(canvas, 174 - textWidth(value), y + DEBUG_TEXT_Y_OFFSET, value,
                 selected ? rgb(255, 218, 178) : rgb(126, 175, 175));
        }
    }
    if (model.popup != DebugViewModel::Popup::NONE) drawDebugPopup(canvas, model);
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}
#endif

bool exploreBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

bool exploreMenuAt(int x, int y) {
    return x >= MENU_BUTTON_X && x < 184 &&
           y >= 0 && y < HEADER_HEIGHT;
}

int exploreAreaAt(int x, int y, uint8_t selectedArea,
                  uint8_t visibleAreaCount) {
    if (x < 2 || x >= EXPLORE_SELECTOR_LEFT_WIDTH ||
        y < HEADER_HEIGHT || y >= 224) return -1;
    int count = std::min<int>(visibleAreaCount, Game::EXPLORE_AREA_COUNT);
    if (count <= 0) return -1;
    int index = selectedArea + static_cast<int>(std::lround(
        (y - EXPLORE_SELECTOR_CENTER_Y) /
        static_cast<float>(EXPLORE_SELECTOR_AREA_SPACING)));
    if (index < 0 || index >= count) return -1;
    int expectedY = EXPLORE_SELECTOR_CENTER_Y +
        (index - static_cast<int>(selectedArea)) *
            EXPLORE_SELECTOR_AREA_SPACING;
    return std::abs(y - expectedY) <= 15 ? index : -1;
}

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    const uint16_t muted = rgb(137, 155, 158);
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    if (!GameAssets::draw(GameAssets::Kind::EXPLORE_MENU_BACKGROUND,
                          EXPLORE_BACKGROUND_X, 0,
                          EXPLORE_BACKGROUND_SCALE)) {
        canvas.fillRect(0, 0, canvas.width(), canvas.height(),
                        rgb(12, 18, 25));
    }
    canvas.clearClipRect();

    if (rowBegin < HEADER_HEIGHT) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, HEADER_HEIGHT);
        canvas.setClipRect(0, top, canvas.width(), bottom - top);
        canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, Ui::EXPLORE, rgb(115, 226, 183));
        drawMenuIcon(canvas);
        canvas.clearClipRect();
    }

    if (rowEnd <= HEADER_HEIGHT) return;
    int contentTop = std::max<int>(rowBegin, HEADER_HEIGHT);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);

    const int leftWidth = EXPLORE_SELECTOR_LEFT_WIDTH;
    const int centerY = EXPLORE_SELECTOR_CENTER_Y;
    const int count = std::min<int>(model.visibleAreaCount,
                                    Game::EXPLORE_AREA_COUNT);
    PixelRenderer::fillRectAlpha(
        0, HEADER_HEIGHT, leftWidth - 1,
        canvas.height() - HEADER_HEIGHT, rgb(8, 17, 22), 132);
    canvas.drawFastVLine(leftWidth - 1, HEADER_HEIGHT + 4,
                         canvas.height() - HEADER_HEIGHT - 8,
                         rgb(77, 105, 106));

    for (int index = 0; index < count; ++index) {
        float offset = static_cast<float>(index) - model.areaAnimCursor;
        if (std::fabs(offset) > 2.25f) continue;
        int y = centerY + static_cast<int>(std::lround(
            offset * EXPLORE_SELECTOR_AREA_SPACING));
        bool active = std::fabs(offset) < 0.5f;
        bool locked = index > model.unlockedArea;
        bool pressed = index == model.pressedArea;
        uint16_t color = locked
            ? rgb(78, 91, 96)
            : active
                ? rgb(248, 210, 105)
                : rgb(170, 185, 181);
        if (pressed) color = rgb(115, 226, 183);
        if (active) {
            canvas.fillRoundRect(5, y - 8, 3, 16, 1,
                                 locked ? rgb(78, 91, 96)
                                        : rgb(248, 210, 105));
        }
        int textX = (leftWidth - textWidth(EXPLORE_AREA_NAMES[index])) / 2;
        text(canvas, textX, y - 8, EXPLORE_AREA_NAMES[index], color);
    }

    int rightCenterX = leftWidth + (canvas.width() - leftWidth) / 2;
    bool selectedLocked = model.selectedArea > model.unlockedArea;
    const char* title = selectedLocked
        ? Ui::Explore::AREA_LOCKED
        : model.previewPool.count > 0 && ExplorePool::poolHasRare(
              model.previewPool)
            ? Ui::Explore::MASS_OUTBREAK
            : Ui::Explore::HABITAT_MONSTERS;
    text(canvas, rightCenterX - textWidth(title) / 2, HEADER_HEIGHT + 7,
         title, selectedLocked ? rgb(137, 155, 158) : ink);
    canvas.drawFastHLine(leftWidth + 8, HEADER_HEIGHT + 26,
                         canvas.width() - leftWidth - 16,
                         rgb(77, 105, 106));

    canvas.setClipRect(leftWidth, HEADER_HEIGHT + 28,
                       canvas.width() - leftWidth,
                       canvas.height() - HEADER_HEIGHT - 28);
    if (selectedLocked) {
        const char* message = Ui::Explore::DEFEAT_PREVIOUS_BOSS;
        text(canvas, rightCenterX - textWidth(message) / 2,
             EXPLORE_PREVIEW_CENTER_Y - 8, message, muted);
    } else if (model.previewPool.count > 0) {
        uint8_t poolCount = model.previewPool.count;
        uint32_t elapsed = Platform::clock().millis() -
                           model.previewStartedAt;
        uint8_t current = static_cast<uint8_t>(
            (elapsed / EXPLORE_PREVIEW_CYCLE_MS + 1) % poolCount);
        uint8_t next = static_cast<uint8_t>((current + 1) % poolCount);
        uint8_t previous = static_cast<uint8_t>(
            (current + poolCount - 1) % poolCount);
        uint32_t cycleElapsed = elapsed % EXPLORE_PREVIEW_CYCLE_MS;
        float progress = cycleElapsed <= EXPLORE_PREVIEW_HOLD_MS
            ? 0.0f
            : (cycleElapsed - EXPLORE_PREVIEW_HOLD_MS) /
                  static_cast<float>(EXPLORE_PREVIEW_MOVE_MS);
        progress = std::min(1.0f, progress);
        progress = progress * progress * (3.0f - 2.0f * progress);

        auto frameWidth = [](const PokemonSprites::SpriteFrame* frame) {
            return frame ? static_cast<int>(FlashStorage::readByte(
                &frame->width)) : EXPLORE_PREVIEW_MAX_WIDTH;
        };
        auto scaledWidth = [&](const PokemonSprites::SpriteFrame* frame) {
            int width = frameWidth(frame);
            int height = frame ? static_cast<int>(FlashStorage::readByte(
                &frame->height)) : EXPLORE_PREVIEW_MAX_HEIGHT;
            float scale = std::min(
                1.0f,
                std::min(static_cast<float>(EXPLORE_PREVIEW_MAX_WIDTH) /
                             std::max(1, width),
                         static_cast<float>(EXPLORE_PREVIEW_MAX_HEIGHT) /
                             std::max(1, height)));
            return std::max(1, static_cast<int>(std::lround(width * scale)));
        };
        int currentWidth = scaledWidth(model.previewFrames[current]);
        int nextWidth = scaledWidth(model.previewFrames[next]);
        int previousWidth = scaledWidth(model.previewFrames[previous]);
        int currentX = rightCenterX;
        int nextX = rightCenterX + currentWidth / 2 +
                    EXPLORE_PREVIEW_GAP + nextWidth / 2;
        int previousX = rightCenterX - currentWidth / 2 -
                        EXPLORE_PREVIEW_GAP - previousWidth / 2;
        if (progress <= 0.0f) {
            drawExplorePreviewSlot(canvas, model, previous, previousX);
            drawExplorePreviewSlot(canvas, model, current, currentX);
            drawExplorePreviewSlot(canvas, model, next, nextX);
        } else {
            uint8_t entering = static_cast<uint8_t>((next + 1) % poolCount);
            int enteringWidth = scaledWidth(model.previewFrames[entering]);
            int currentNewX = rightCenterX - nextWidth / 2 -
                              EXPLORE_PREVIEW_GAP - currentWidth / 2;
            int nextNewX = rightCenterX;
            int enteringOldX = canvas.width() + enteringWidth / 2 + 4;
            int enteringNewX = rightCenterX + nextWidth / 2 +
                               EXPLORE_PREVIEW_GAP + enteringWidth / 2;
            int currentAnimatedX = currentX + static_cast<int>(std::lround(
                (currentNewX - currentX) * progress));
            int nextAnimatedX = nextX + static_cast<int>(std::lround(
                (nextNewX - nextX) * progress));
            int enteringAnimatedX = enteringOldX +
                static_cast<int>(std::lround(
                    (enteringNewX - enteringOldX) * progress));
            drawExplorePreviewSlot(canvas, model, current,
                                   currentAnimatedX);
            drawExplorePreviewSlot(canvas, model, next, nextAnimatedX);
            drawExplorePreviewSlot(canvas, model, entering,
                                   enteringAnimatedX);
        }
    }
    canvas.clearClipRect();

    char levelText[24] = {};
    std::snprintf(levelText, sizeof(levelText), Ui::Amoled::REC_LEVEL_FMT,
                  ExploreAreaCatalog::recommendedLevel(model.selectedArea));
    text(canvas, rightCenterX - textWidth(levelText) / 2, 204, levelText,
         selectedLocked ? rgb(88, 98, 104) : rgb(137, 175, 166));
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

int exploreRoutePromptChoiceAt(int x, int y) {
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
            drawExploreRoutePickup(canvas, model);
            drawExploreRoutePet(canvas, model);
        }

        canvas.fillRect(0, 200, canvas.width(), 24, rgb(10, 18, 23));
        canvas.drawFastHLine(0, 200, canvas.width(), rgb(68, 99, 101));
        char stepsText[16] = {};
        std::snprintf(stepsText, sizeof(stepsText), Ui::Amoled::STEPS_FMT,
                      model.steps);
        text(canvas, 7, 208, stepsText, rgb(226, 238, 233));
        const char* status = model.complete
            ? Ui::Amoled::ROUTE_END
            : model.prompt != ExploreRouteViewModel::Prompt::NONE
                ? Ui::Amoled::BLOCKED
            : model.sliding ? Ui::Amoled::SLIDING
            : model.walking || model.autoWalk ? Ui::Amoled::WALKING
                                               : Ui::Amoled::PAUSED;
        int statusX = 177 - textWidth(status);
        text(canvas, statusX, 208, status,
             model.complete ? rgb(248, 210, 105)
                            : rgb(115, 226, 183));

        if (model.exitConfirm) {
            canvas.fillRoundRect(10, 145, 164, 69, 6, rgb(17, 27, 34));
            canvas.drawRoundRect(10, 145, 164, 69, 6,
                                 rgb(82, 117, 117));
            text(canvas, 49, 154, Ui::Amoled::LEAVE_ROUTE,
                 rgb(226, 238, 233));
            canvas.fillRoundRect(18, 167, 70, 37, 4, rgb(36, 54, 61));
            canvas.fillRoundRect(96, 167, 70, 37, 4, rgb(91, 49, 55));
            text(canvas, 36, 182, Ui::Amoled::STAY, rgb(115, 226, 183));
            text(canvas, 117, 182, Ui::Amoled::EXIT, rgb(239, 143, 148));
        }
        if (model.prompt != ExploreRouteViewModel::Prompt::NONE) {
            canvas.fillRoundRect(10, 145, 164, 69, 6, rgb(17, 27, 34));
            canvas.drawRoundRect(10, 145, 164, 69, 6,
                                 rgb(82, 117, 117));
            text(canvas, 43, 154,
                 model.prompt == ExploreRouteViewModel::Prompt::PUZZLE
                     ? Ui::Amoled::PATH_PUZZLE : Ui::Amoled::PATH_BLOCKED,
                 rgb(226, 238, 233));
            canvas.fillRoundRect(18, 167, 70, 37, 4, rgb(36, 54, 61));
            canvas.fillRoundRect(96, 167, 70, 37, 4, rgb(91, 49, 55));
            text(canvas, 33, 182,
                 model.prompt == ExploreRouteViewModel::Prompt::PUZZLE
                     ? Ui::Amoled::SOLVE : Ui::Amoled::OPEN,
                 rgb(115, 226, 183));
            text(canvas, 117, 182, Ui::Amoled::TURN, rgb(239, 143, 148));
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
    int panelX = 184 - EXPLORE_MENU_PANEL_WIDTH;
    if (x < panelX || x >= 184 || y < EXPLORE_MENU_PANEL_ROW_TOP ||
        y >= EXPLORE_MENU_PANEL_ROW_TOP +
                 AppSceneFlow::exploreMenuItemCount() *
                     EXPLORE_MENU_PANEL_ROW_HEIGHT) {
        return -1;
    }
    int index = (y - EXPLORE_MENU_PANEL_ROW_TOP) /
                EXPLORE_MENU_PANEL_ROW_HEIGHT;
    return index < AppSceneFlow::exploreMenuItemCount() ? index : -1;
}

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;

    const int panelX = canvas.width() - EXPLORE_MENU_PANEL_WIDTH;
    canvas.setClipRect(panelX, rowBegin, EXPLORE_MENU_PANEL_WIDTH,
                       rowEnd - rowBegin);
    canvas.fillRect(panelX, 0, EXPLORE_MENU_PANEL_WIDTH, canvas.height(),
                    rgb(20, 25, 32));
    canvas.drawRect(panelX, 0, EXPLORE_MENU_PANEL_WIDTH, canvas.height(),
                    rgb(190, 200, 205));

    for (uint8_t index = 0; index < AppSceneFlow::exploreMenuItemCount();
         ++index) {
        int y = EXPLORE_MENU_PANEL_ROW_TOP +
                index * EXPLORE_MENU_PANEL_ROW_HEIGHT;
        if (y + EXPLORE_MENU_PANEL_ROW_HEIGHT <= rowBegin || y >= rowEnd) {
            continue;
        }
        bool selected = index == model.cursor;
        bool pressed = index == model.pressedItem;
        if (pressed) {
            canvas.fillRect(panelX + 2, y + 2,
                            EXPLORE_MENU_PANEL_WIDTH - 4,
                            EXPLORE_MENU_PANEL_ROW_HEIGHT - 4,
                            rgb(33, 43, 48));
        }
        if (selected) {
            canvas.fillRect(panelX + 5, y, 3, 18, rgb(255, 216, 72));
        }
        uint16_t color = selected ? rgb(255, 216, 72)
                                  : rgb(235, 239, 232);
        text(canvas, panelX + 13, y,
             Ui::Explore::SIDE_MENU_ITEMS[index], color);
    }
    canvas.clearClipRect();
    if (model.toast && rowBegin < 167 && rowEnd > 149) {
        canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
        drawToast(canvas, model.toast);
        canvas.clearClipRect();
    }
}

namespace {

void drawTeamSprite(Canvas565& canvas, uint16_t speciesId,
                    int centerX, int centerY);

const char* communicationStateLabel(
    Communication::VisitSessionService::State state) {
    using State = Communication::VisitSessionService::State;
    switch (state) {
    case State::HOSTING: return Ui::HOSTING;
    case State::SEARCHING: return Ui::SEARCHING;
    case State::JOINING: return Ui::Amoled::JOINING;
    case State::WAITING_HOST_DECISION: return Ui::Amoled::INCOMING;
    case State::SYNCING: return Ui::Amoled::SYNCING;
    case State::WAITING_ACCEPT: return Ui::Amoled::WAIT_ACCEPT;
    case State::ACTIVE: return Ui::Amoled::VISITING;
    case State::ENDING: return Ui::Amoled::ENDING;
    case State::FAILED: return Ui::Amoled::FAILED;
    case State::ENDED: return Ui::Amoled::ENDED;
    case State::IDLE: return Ui::SOCIAL;
    }
    return Ui::SOCIAL;
}

void drawCommunicationButton(Canvas565& canvas, int y, const char* label,
                             uint16_t color) {
    canvas.fillRoundRect(10, y, 164, 36, 4, rgb(24, 38, 45));
    canvas.drawRoundRect(10, y, 164, 36, 4, rgb(68, 100, 102));
    text(canvas, (184 - textWidth(label)) / 2, y + 14, label, color);
}

}  // namespace

void renderCommunicationScreen(Canvas565& canvas,
                               const CommunicationViewModel& model,
                               uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;
    using State = Communication::VisitSessionService::State;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        canvas.setClipRect(0, rowBegin, canvas.width(), bottom - rowBegin);
        canvas.fillRect(0, 0, canvas.width(), MENU_CONTENT_TOP,
                        rgb(19, 31, 39));
        canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                        rgb(56, 87, 89));
        drawBackIcon(canvas);
        text(canvas, 36, 8, Ui::SOCIAL, rgb(115, 226, 183));
        canvas.clearClipRect();
    }
    if (rowEnd <= MENU_CONTENT_TOP) return;
    int top = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, top, canvas.width(), rowEnd - top);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    if (model.state == State::IDLE) {
        drawCommunicationButton(canvas, 42, Ui::Amoled::HOST,
                                rgb(115, 226, 183));
        drawCommunicationButton(canvas, 90, Ui::Amoled::SEARCH,
                                rgb(226, 238, 233));
    } else if (model.state == State::SEARCHING ||
               model.state == State::JOINING) {
        text(canvas, 14, 36, communicationStateLabel(model.state),
             rgb(115, 226, 183));
        if (model.roomCount == 0) {
            text(canvas, 48, 86, Ui::Amoled::NO_ROOM, rgb(151, 168, 166));
        } else {
            for (uint8_t index = 0; index < model.roomCount; ++index) {
                int y = 54 + index * 40;
                canvas.fillRoundRect(8, y, 168, 32, 4,
                                     index == 0 && model.state == State::JOINING
                                         ? rgb(47, 68, 73) : rgb(24, 38, 45));
                text(canvas, 18, y + 12, Ui::Amoled::VISIT_ROOM,
                     rgb(226, 238, 233));
                text(canvas, 138, y + 12, Ui::Amoled::JOIN,
                     rgb(115, 226, 183));
            }
        }
    } else if (model.state == State::HOSTING) {
        text(canvas, 54, 42, Ui::Amoled::WAITING, rgb(115, 226, 183));
        text(canvas, 40, 66, Ui::Amoled::ROOM_OPEN, rgb(226, 238, 233));
    } else if (model.state == State::WAITING_HOST_DECISION) {
        text(canvas, 42, 38, Ui::Amoled::INCOMING, rgb(248, 210, 105));
        drawCommunicationButton(canvas, 66, Ui::Amoled::ACCEPT,
                                rgb(115, 226, 183));
        drawCommunicationButton(canvas, 112, Ui::Amoled::DECLINE,
                                rgb(239, 143, 148));
    } else if (model.state == State::SYNCING ||
               model.state == State::WAITING_ACCEPT) {
        text(canvas, 56, 48, communicationStateLabel(model.state),
             rgb(115, 226, 183));
        if (model.remote.known) {
            drawTeamSprite(canvas, model.remote.speciesId, 92, 126);
        }
    } else if (model.state == State::ACTIVE ||
               model.state == State::ENDING) {
        if (model.remote.known) {
            drawTeamSprite(canvas, model.remote.speciesId, 92, 100);
            char level[8] = {};
            std::snprintf(level, sizeof(level), "LV%u", model.remote.level);
            text(canvas, 74, 126, level, rgb(226, 238, 233));
            text(canvas, 26, 146, Ui::HUNGER, rgb(151, 168, 166));
            text(canvas, 72, 146, Ui::MOOD, rgb(151, 168, 166));
            text(canvas, 30, 158, "HP", rgb(115, 226, 183));
            text(canvas, 78, 158, Ui::Amoled::AFFECTION,
                 rgb(248, 210, 105));
            text(canvas, 30, 170, Ui::Amoled::VISITING,
                 rgb(226, 238, 233));
            char remain[8] = {};
            std::snprintf(remain, sizeof(remain), "%us", model.remainSec);
            text(canvas, 78, 170, remain, rgb(115, 226, 183));
        }
        drawCommunicationButton(canvas, 184,
                                model.state == State::ENDING
                                    ? Ui::Amoled::ENDING : Ui::Amoled::EXIT,
                                rgb(239, 143, 148));
    } else {
        text(canvas, 58, 48, communicationStateLabel(model.state),
             rgb(239, 143, 148));
        if (model.error) text(canvas, 32, 76, model.error, rgb(239, 143, 148));
        drawCommunicationButton(canvas, 132, Ui::BACK, rgb(115, 226, 183));
    }
    canvas.clearClipRect();
}

bool communicationBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int communicationItemAt(int x, int y,
                        const CommunicationViewModel& model) {
    if (x < 8 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    using State = Communication::VisitSessionService::State;
    if (model.state == State::IDLE) {
        if (y >= 42 && y < 78) return 0;
        if (y >= 90 && y < 126) return 1;
    } else if (model.state == State::SEARCHING ||
               model.state == State::JOINING) {
        if (y >= 54 && y < 54 + model.roomCount * 40) {
            return (y - 54) / 40;
        }
    } else if (model.state == State::WAITING_HOST_DECISION) {
        if (y >= 66 && y < 102) return 0;
        if (y >= 112 && y < 148) return 1;
    } else if (model.state == State::ACTIVE ||
               model.state == State::ENDING) {
        if (y >= 184) return 0;
    } else if (model.state == State::FAILED ||
               model.state == State::ENDED) {
        if (y >= 132 && y < 168) return 0;
    }
    return -1;
}

namespace {

constexpr int TEAM_CARD_TOP = 32;
constexpr int TEAM_CARD_HEIGHT = 78;
constexpr int TEAM_CARD_GAP = 6;

const char* teamStatusLabel(Game::MajorStatus status) {
    switch (status) {
    case Game::MajorStatus::POISON: return Ui::Status::STATUS_POISON;
    case Game::MajorStatus::TOXIC: return Ui::Status::STATUS_TOXIC;
    case Game::MajorStatus::PARALYSIS: return Ui::Status::STATUS_PARALYSIS;
    case Game::MajorStatus::SLEEP: return Ui::Status::STATUS_SLEEP;
    case Game::MajorStatus::BURN: return Ui::Status::STATUS_BURN;
    case Game::MajorStatus::FREEZE: return Ui::Status::STATUS_FREEZE;
    case Game::MajorStatus::NONE: return Ui::Status::STATUS_OK;
    }
    return Ui::Status::STATUS_OK;
}

void drawTeamSprite(Canvas565& canvas, uint16_t speciesId,
                    int centerX, int centerY) {
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        canvas.fillCircle(centerX, centerY, 19, rgb(42, 61, 68));
        text(canvas, centerX - 16, centerY - 8, Ui::Amoled::MONSTER,
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
    text(canvas, 50, 145, Ui::Amoled::CHANGE_LEADER,
         rgb(226, 238, 233));
    text(canvas, 62, 160, Ui::Amoled::SET_FIRST, rgb(248, 210, 105));
    canvas.fillRoundRect(18, 174, 70, 36, 4, rgb(36, 54, 61));
    canvas.fillRoundRect(96, 174, 70, 36, 4, rgb(91, 49, 55));
    text(canvas, 39, 188, Ui::Amoled::YES, rgb(115, 226, 183));
    text(canvas, 113, 188, Ui::BACK, rgb(239, 143, 148));
}

const char* moveSlotLabel(uint8_t slot) {
    switch (slot) {
    case 0: return Ui::Amoled::BATTLE_BASIC;
    case 1: return Ui::Amoled::BATTLE_SPECIAL_1;
    case 2: return Ui::Amoled::BATTLE_SPECIAL_2;
    default: return Ui::Amoled::MOVES;
    }
}

void drawMoveSummary(Canvas565& canvas, const Game::GameState& state,
                     uint8_t teamSlot, uint8_t moveSlot) {
    if (teamSlot >= state.teamCount || teamSlot >= Game::TEAM_CAP) return;
    const Game::MonsterRuntime& monster = state.team[teamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (!species) return;
    const MoveInfo* move = Game::MoveManagementService::learnedMove(
        *species, monster, moveSlot);
    if (!move) {
        text(canvas, 8, 34, Ui::Amoled::EMPTY_MOVE_SLOT,
             rgb(126, 145, 145));
        return;
    }
    text(canvas, 8, 34, move->name, rgb(248, 210, 105));
    char detail[64] = {};
    std::snprintf(detail, sizeof(detail), Ui::Amoled::MOVE_DETAIL_FMT,
                  move->power, move->accuracy,
                  monster.moveProficiency[moveSlot]);
    text(canvas, 8, 44, detail, rgb(126, 175, 175));
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

bool teamMovesButtonAt(int x, int y, uint8_t teamSlot) {
    constexpr int pitch = TEAM_CARD_HEIGHT + TEAM_CARD_GAP;
    int rowY = TEAM_CARD_TOP + static_cast<int>(teamSlot) * pitch;
    return x >= 120 && x < 176 && y >= rowY + 45 && y < rowY + 75;
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
        text(canvas, 36, 8, Ui::TEAM, rgb(115, 226, 183));
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
                            species ? species->name : Ui::Amoled::MONSTER,
                            rgb(226, 238, 233), 1);
        char level[12];
        std::snprintf(level, sizeof(level), "LV%u", monster.level);
        text(canvas, 64, y + 28, level, rgb(126, 175, 175));
        char hp[20];
        std::snprintf(hp, sizeof(hp), "%u/%u",
                      monster.hpCur, monster.hpMax);
        text(canvas, 172 - textWidth(hp),
             y + 28, hp, rgb(226, 238, 233));
        drawHomeHpBar(canvas, 64, y + 42, 104,
                      Game::HomeHud::hpPercent(monster));

        char hunger[10];
        std::snprintf(hunger, sizeof(hunger), Ui::Amoled::HUNGER_FMT,
                      Game::HomeHud::hungerPercent(monster));
        text(canvas, 64, y + 58, hunger, rgb(248, 210, 105));
        const char* status = teamStatusLabel(monster.majorStatus);
        text(canvas, 172 - textWidth(status),
             y + 58, status,
             monster.majorStatus == Game::MajorStatus::NONE
                 ? rgb(126, 145, 145) : rgb(239, 143, 148));
        if (slot == 0) {
            canvas.fillRoundRect(139, y + 5, 31, 14, 3,
                                 rgb(43, 94, 79));
            text(canvas, 143, y + 9, Ui::Amoled::LEAD,
                 rgb(194, 242, 216));
        } else if (monster.origin != Game::Origin::VISITOR) {
            canvas.drawLine(164, y + 9, 169, y + 13,
                            rgb(115, 226, 183));
            canvas.drawLine(169, y + 13, 164, y + 17,
                            rgb(115, 226, 183));
        }
        canvas.fillRoundRect(122, y + 48, 48, 22, 3,
                             model.pressedSlot == slot
                                 ? rgb(48, 74, 68) : rgb(36, 54, 61));
        text(canvas, 131, y + 56, Ui::Amoled::MOVES,
             rgb(115, 226, 183));
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    if (model.confirmOpen) drawTeamConfirm(canvas);
    canvas.clearClipRect();
}

bool teamMovesBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int teamMovesItemAt(int x, int y, TeamMovesViewModel::Mode mode,
                    uint8_t recallCount) {
    if (x < 6 || x >= 178 || y < 52 || y >= 224) return -1;
    int index = (y - 52) / 31;
    if (mode == TeamMovesViewModel::Mode::MANAGE) {
        return index < 5 ? index : -1;
    }
    if (mode == TeamMovesViewModel::Mode::RECALL_SELECT) {
        return index < static_cast<int>(recallCount) + 1 ? index : -1;
    }
    return index < 3 ? index : -1;
}

void renderTeamMovesScreen(Canvas565& canvas,
                           const TeamMovesViewModel& model,
                           uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd || !model.state ||
        model.teamSlot >= model.state->teamCount) return;

    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    canvas.fillRect(0, 0, canvas.width(), canvas.height(), rgb(12, 18, 25));
    canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT, rgb(19, 31, 39));
    canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1,
                    rgb(56, 87, 89));
    drawBackIcon(canvas);
    text(canvas, 36, 8, Ui::Amoled::MOVES, rgb(115, 226, 183));

    const Game::MonsterRuntime& monster = model.state->team[model.teamSlot];
    const Species* species = findSpecies(monster.speciesId);
    if (species) text(canvas, 8, 27, species->name, rgb(226, 238, 233));
    char scales[18] = {};
    std::snprintf(scales, sizeof(scales), Ui::Amoled::HEART_COUNT_FMT,
                  Game::ItemInventory::count(
                      *model.state, Game::ItemId::HEART_SCALE));
    text(canvas, 122, 27, scales, rgb(248, 210, 105));

    uint8_t detailSlot = 0xFF;
    if (model.mode == TeamMovesViewModel::Mode::MANAGE &&
        model.selectedItem < Game::MOVE_SLOT_COUNT) {
        detailSlot = model.selectedItem;
    } else if (model.mode == TeamMovesViewModel::Mode::RECALL_REPLACE &&
               model.selectedItem < 2) {
        detailSlot = static_cast<uint8_t>(model.selectedItem + 1);
    }
    if (detailSlot != 0xFF) drawMoveSummary(
        canvas, *model.state, model.teamSlot, detailSlot);

    uint8_t rowCount = 0;
    if (model.mode == TeamMovesViewModel::Mode::MANAGE) {
        rowCount = 5;
    } else if (model.mode == TeamMovesViewModel::Mode::RECALL_SELECT) {
        rowCount = static_cast<uint8_t>(model.recallCount + 1);
    } else {
        rowCount = 3;
    }
    for (uint8_t index = 0; index < rowCount; ++index) {
        int y = 52 + index * 31;
        bool selected = index == model.selectedItem;
        canvas.fillRoundRect(6, y, 172, 27, 4,
                             selected ? rgb(42, 61, 68)
                                      : rgb(24, 34, 42));
        const char* label = Ui::BACK;
        uint16_t color = rgb(115, 226, 183);
        if (model.mode == TeamMovesViewModel::Mode::MANAGE) {
            if (index < Game::MOVE_SLOT_COUNT) {
                label = moveSlotLabel(index);
                const MoveInfo* move = species
                    ? Game::MoveManagementService::learnedMove(
                          *species, monster, index) : nullptr;
                if (move) label = move->name;
                else color = rgb(91, 104, 104);
            } else if (index == 3) {
                label = Ui::Amoled::RECALL_MOVE;
                color = Game::ItemInventory::count(
                    *model.state, Game::ItemId::HEART_SCALE) > 0
                    ? rgb(115, 226, 183) : rgb(91, 104, 104);
            }
        } else if (model.mode == TeamMovesViewModel::Mode::RECALL_SELECT) {
            if (index < model.recallCount) {
                const MoveInfo* move = findMove(model.recallIds[index]);
                label = move ? move->name : Ui::Amoled::MOVES;
            }
        } else if (index < 2) {
            label = index == 0 ? Ui::Amoled::REPLACE_SPECIAL_1
                               : Ui::Amoled::REPLACE_SPECIAL_2;
        }
        text(canvas, 14, y + 10, label, color);
    }
    drawToast(canvas, model.toast);
    if (model.forgetConfirmOpen) {
        canvas.fillRoundRect(10, 132, 164, 82, 6, rgb(17, 27, 34));
        canvas.drawRoundRect(10, 132, 164, 82, 6, rgb(82, 117, 117));
        const MoveInfo* move = species
            ? Game::MoveManagementService::learnedMove(
                  *species, monster, model.forgetSlot) : nullptr;
        text(canvas, 27, 145, Ui::Amoled::FORGET_MOVE,
             rgb(226, 238, 233));
        if (move) text(canvas, 27, 158, move->name, rgb(248, 210, 105));
        canvas.fillRoundRect(18, 174, 70, 36, 4, rgb(36, 54, 61));
        canvas.fillRoundRect(96, 174, 70, 36, 4, rgb(91, 49, 55));
        text(canvas, 39, 188, Ui::Amoled::YES, rgb(115, 226, 183));
        text(canvas, 113, 188, Ui::BACK, rgb(239, 143, 148));
    }
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

namespace {

int shopGridRows(uint8_t itemCount) {
    return (static_cast<int>(itemCount) + 1) / 2;
}

int shopExploreSectionTop(uint8_t dailyItemCount) {
    return SHOP_SECTION_HEADER_HEIGHT +
           shopGridRows(dailyItemCount) * SHOP_GRID_ROW_HEIGHT +
           SHOP_SECTION_GAP;
}

int shopGridContentHeight(ShopViewModel::Mode mode,
                          uint8_t dailyItemCount,
                          uint8_t exploreItemCount,
                          uint8_t itemCount) {
    if (mode == ShopViewModel::Mode::SELL) {
        return SHOP_SECTION_HEADER_HEIGHT +
               shopGridRows(itemCount) * SHOP_GRID_ROW_HEIGHT +
               SHOP_SECTION_GAP;
    }
    return shopExploreSectionTop(dailyItemCount) +
           SHOP_SECTION_HEADER_HEIGHT +
           shopGridRows(exploreItemCount) * SHOP_GRID_ROW_HEIGHT +
           SHOP_SECTION_GAP;
}

bool shopGridItemCenter(int index, float scroll, ShopViewModel::Mode mode,
                        uint8_t dailyItemCount, uint8_t itemCount,
                        int& centerX, int& centerY) {
    (void)itemCount;
    if (index < 0) return false;
    int sectionIndex = index;
    int sectionTop = SHOP_SECTION_HEADER_HEIGHT;
    if (mode == ShopViewModel::Mode::BUY && index >= dailyItemCount) {
        sectionIndex -= dailyItemCount;
        sectionTop = shopExploreSectionTop(dailyItemCount) +
                     SHOP_SECTION_HEADER_HEIGHT;
    }
    int column = sectionIndex % 2;
    int row = sectionIndex / 2;
    centerX = SHOP_GRID_LEFT + column * SHOP_GRID_COLUMN_WIDTH +
              SHOP_GRID_COLUMN_WIDTH / 2;
    centerY = MENU_CONTENT_TOP + sectionTop +
              row * SHOP_GRID_ROW_HEIGHT + SHOP_GRID_ROW_HEIGHT / 2 -
              static_cast<int>(std::lround(scroll));
    return true;
}

}  // namespace

int shopMenuItemAt(int x, int y) {
    if (x < 0 || x >= SHOP_RAIL_DIVIDER_X) return -1;
    for (int index = 0; index < 3; ++index) {
        int top = 44 + index * 54;
        if (y >= top && y < top + 42) return index;
    }
    return -1;
}

int shopGridItemAt(int x, int y, float scroll,
                   ShopViewModel::Mode mode, uint8_t dailyItemCount,
                   uint8_t exploreItemCount, uint8_t itemCount) {
    (void)exploreItemCount;
    if (x < SHOP_GRID_LEFT || x >= 180 ||
        y < MENU_CONTENT_TOP || y >= 224) {
        return -1;
    }
    for (uint8_t index = 0; index < itemCount; ++index) {
        int centerX = 0;
        int centerY = 0;
        if (!shopGridItemCenter(index, scroll, mode, dailyItemCount,
                                itemCount, centerX, centerY)) {
            continue;
        }
        if (std::abs(x - centerX) <= SHOP_GRID_COLUMN_WIDTH / 2 - 2 &&
            std::abs(y - centerY) <= SHOP_GRID_ROW_HEIGHT / 2 - 2) {
            return index;
        }
    }
    return -1;
}

float shopGridMaxScroll(ShopViewModel::Mode mode, uint8_t dailyItemCount,
                        uint8_t exploreItemCount, uint8_t itemCount) {
    int contentHeight = shopGridContentHeight(
        mode, dailyItemCount, exploreItemCount, itemCount);
    return static_cast<float>(
        std::max(0, contentHeight - (224 - MENU_CONTENT_TOP)));
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
        ? Ui::Amoled::USE : model.mode == ItemListMode::SELL
            ? Ui::Amoled::SELL : Ui::Amoled::BUY;
    text(canvas, (184 - textWidth(action)) / 2,
         141, action, rgb(226, 238, 233));
    const char* name = Game::ShopService::shortName(model.pendingItem);
    text(canvas, std::max(16, (184 - textWidth(name)) / 2),
         157, name, rgb(248, 210, 105));
    canvas.fillRoundRect(18, 174, 70, 36, 4, rgb(36, 54, 61));
    canvas.fillRoundRect(96, 174, 70, 36, 4, rgb(91, 49, 55));
    text(canvas, 39, 188, Ui::Amoled::YES, rgb(115, 226, 183));
    text(canvas, 113, 188, Ui::BACK, rgb(239, 143, 148));
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
             model.mode == ItemListMode::BAG ? Ui::BAG : Ui::SHOP,
             rgb(115, 226, 183));
        if (model.mode != ItemListMode::BAG) {
            char coins[16];
            std::snprintf(coins, sizeof(coins), "C%lu",
                          static_cast<unsigned long>(model.coins));
            text(canvas, 178 - textWidth(coins),
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
             model.mode == ItemListMode::SELL ? Ui::Amoled::NOTHING_TO_SELL
                                               : Ui::Amoled::NOTHING,
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
            int priceX = 172 - textWidth(priceText);
            canvas.fillRect(priceX - 2, y + 5,
                            textWidth(priceText) + 4,
                            13, background);
            text(canvas, priceX, y + 8, priceText, rgb(248, 210, 105));
        }
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    drawItemConfirm(canvas, model);
    canvas.clearClipRect();
}

namespace {

Game::ItemId shopItemForIndex(const ShopViewModel& model, uint8_t index) {
    if (!model.state || index >= model.itemCount) return Game::ItemId::COUNT;
    if (model.mode == ShopViewModel::Mode::SELL) {
        return Game::ShopService::sellItemAt(*model.state, index);
    }
    if (index < model.dailyItemCount) {
        return Game::ShopService::buyItemAt(
            Game::ShopService::Category::DAILY, *model.state, index);
    }
    return Game::ShopService::buyItemAt(
        Game::ShopService::Category::EXPLORE, *model.state,
        static_cast<uint8_t>(index - model.dailyItemCount));
}

uint16_t shopFadeColor(uint16_t foreground, uint8_t alpha,
                       uint16_t background = 0) {
    uint8_t inverse = static_cast<uint8_t>(255 - alpha);
    uint8_t red = static_cast<uint8_t>(
        ((((foreground >> 11) & 0x1F) * alpha) +
         (((background >> 11) & 0x1F) * inverse)) / 255);
    uint8_t green = static_cast<uint8_t>(
        ((((foreground >> 5) & 0x3F) * alpha) +
         (((background >> 5) & 0x3F) * inverse)) / 255);
    uint8_t blue = static_cast<uint8_t>(
        (((foreground & 0x1F) * alpha) +
         ((background & 0x1F) * inverse)) / 255);
    return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

}  // namespace

void renderShopScreen(Canvas565& canvas, const ShopViewModel& model,
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
        text(canvas, 36, 8, Ui::SHOP, rgb(115, 226, 183));
        char coins[16];
        std::snprintf(coins, sizeof(coins), "C%lu",
                      static_cast<unsigned long>(model.coins));
        text(canvas, 178 - textWidth(coins), 8, coins,
             rgb(248, 210, 105));
        canvas.clearClipRect();
    }

    if (rowEnd <= MENU_CONTENT_TOP) return;
    int contentTop = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    canvas.setClipRect(0, contentTop, canvas.width(), rowEnd - contentTop);
    PixelRenderer::canvas().setClipRect(
        0, contentTop, canvas.width(), rowEnd - contentTop);
    canvas.fillRect(0, MENU_CONTENT_TOP, canvas.width(),
                    canvas.height() - MENU_CONTENT_TOP, rgb(12, 18, 25));

    const bool detailOpen = model.detailItem != Game::ItemId::COUNT;
    const bool showRail = !detailOpen;
    if (showRail) {
        canvas.fillRect(0, MENU_CONTENT_TOP, SHOP_RAIL_DIVIDER_X,
                        canvas.height() - MENU_CONTENT_TOP, rgb(17, 24, 31));
        canvas.drawFastVLine(SHOP_RAIL_DIVIDER_X, MENU_CONTENT_TOP,
                             canvas.height() - MENU_CONTENT_TOP,
                             rgb(67, 74, 84));

        static constexpr const char* MENU_LABELS[] = {
            Ui::Amoled::BUY, Ui::Amoled::SELL, Ui::Amoled::LEAVE,
        };
        int selectedMenu = model.mode == ShopViewModel::Mode::BUY ? 0 : 1;
        for (int index = 0; index < 3; ++index) {
            int x = 5;
            int y = 44 + index * 54;
            bool selected = index == selectedMenu;
            if (selected) {
                canvas.fillRect(x, y + 20, 3, 8, rgb(248, 210, 105));
            }
            const char* label = MENU_LABELS[index];
            text(canvas, x + (46 - textWidth(label)) / 2, y + 16, label,
                 index == 2 ? rgb(115, 226, 183)
                            : selected ? rgb(248, 210, 105)
                                       : rgb(194, 210, 207));
        }
    }

    if (!detailOpen) {
        int scrollPixels = static_cast<int>(std::lround(model.scroll));
        auto drawSectionHeader = [&](int localY, const char* label) {
        int y = MENU_CONTENT_TOP + localY - scrollPixels;
        if (y + SHOP_SECTION_HEADER_HEIGHT <= MENU_CONTENT_TOP || y >= 224) {
            return;
        }
        canvas.fillRect(SHOP_GRID_LEFT, y, 116,
                        SHOP_SECTION_HEADER_HEIGHT, rgb(20, 31, 38));
        text(canvas, SHOP_GRID_LEFT + 4, y + 3, label,
             rgb(126, 175, 175));
        canvas.drawFastHLine(SHOP_GRID_LEFT + 4,
                             y + SHOP_SECTION_HEADER_HEIGHT - 1,
                             108, rgb(56, 87, 89));
        };
        if (model.mode == ShopViewModel::Mode::BUY) {
            drawSectionHeader(0, Ui::Shop::CATEGORY_DAILY);
            drawSectionHeader(shopExploreSectionTop(model.dailyItemCount),
                              Ui::Shop::CATEGORY_EXPLORE);
        } else {
            drawSectionHeader(0, Ui::BAG);
        }

        for (uint8_t index = 0; index < model.itemCount; ++index) {
        int centerX = 0;
        int centerY = 0;
        if (!shopGridItemCenter(index, model.scroll, model.mode,
                                model.dailyItemCount, model.itemCount,
                                centerX, centerY) ||
            centerY + SHOP_GRID_ROW_HEIGHT / 2 < MENU_CONTENT_TOP ||
            centerY - SHOP_GRID_ROW_HEIGHT / 2 >= 224) {
            continue;
        }
        bool pressed = index == model.pressedItem;
        canvas.fillRoundRect(centerX - 27, centerY - 23, 54, 46, 4,
                             pressed ? rgb(42, 61, 68) : rgb(24, 34, 42));
        Game::ItemId item = shopItemForIndex(model, index);
        if (!GameAssets::drawCenteredAlpha(
                GameAssets::itemKind(item), centerX, centerY,
                SHOP_GRID_ICON_SCALE,
                255)) {
            text(canvas, centerX - 3, centerY - 7, "?",
                 rgb(248, 210, 105));
        }
        }

        if (model.itemCount == 0) {
            const char* empty = model.mode == ShopViewModel::Mode::SELL
                ? Ui::Amoled::NOTHING_TO_SELL : Ui::Amoled::NOTHING;
            text(canvas, 67, 108, empty, rgb(126, 145, 145));
        }
    }

    if (detailOpen) {
        int originX = 40;
        int originY = 72;
        int iconX = originX;
        int iconY = originY;
        float iconScale = SHOP_DETAIL_ICON_END_SCALE;
        if (!GameAssets::drawCenteredAlpha(
                GameAssets::itemKind(model.detailItem), iconX, iconY,
                iconScale, 255)) {
            text(canvas, iconX - 3, iconY - 5, "?", rgb(248, 210, 105));
        }

        constexpr uint8_t detailAlpha = 255;
        const uint16_t detailBackground = rgb(12, 18, 25);
        const char* name = Game::ShopService::shortName(model.detailItem);
        text(canvas, 84, 42, name,
             shopFadeColor(rgb(226, 238, 233), detailAlpha,
                           detailBackground));
        char owned[16];
        std::snprintf(owned, sizeof(owned), Ui::Shop::OWNED_FMT,
                      model.state ? Game::ItemInventory::count(
                                        *model.state, model.detailItem) : 0);
        text(canvas, 84, 68, owned,
             shopFadeColor(rgb(239, 196, 154), detailAlpha,
                           detailBackground));
        char price[20];
        std::snprintf(price, sizeof(price),
                      model.mode == ShopViewModel::Mode::SELL
                          ? Ui::Shop::SELL_PRICE_FMT : Ui::Shop::PRICE_FMT,
                      model.mode == ShopViewModel::Mode::SELL
                          ? Game::ShopService::sellPrice(model.detailItem)
                          : Game::ShopService::buyPrice(model.detailItem));
        text(canvas, 84, 94, price,
             shopFadeColor(rgb(248, 210, 105), detailAlpha,
                           detailBackground));
        text(canvas, 12, 124,
             Game::ShopService::shortDescription(model.detailItem),
             shopFadeColor(rgb(126, 175, 175), detailAlpha,
                           detailBackground));

        uint16_t actionFill = shopFadeColor(
            model.pressedDetailAction == 0 ? rgb(48, 74, 68)
                                           : rgb(36, 54, 61),
            detailAlpha, detailBackground);
        uint16_t backFill = shopFadeColor(
            model.pressedDetailAction == 1 ? rgb(91, 49, 55)
                                           : rgb(46, 37, 44),
            detailAlpha, detailBackground);
        canvas.fillRoundRect(18, SHOP_DETAIL_BUTTON_Y, 70,
                             SHOP_DETAIL_BUTTON_HEIGHT, 4, actionFill);
        canvas.fillRoundRect(96, SHOP_DETAIL_BUTTON_Y, 70,
                             SHOP_DETAIL_BUTTON_HEIGHT, 4, backFill);
        const char* action = model.mode == ShopViewModel::Mode::SELL
            ? Ui::Amoled::SELL : Ui::Amoled::BUY;
        text(canvas, 18 + (70 - textWidth(action)) / 2,
             SHOP_DETAIL_BUTTON_Y + 14, action,
             shopFadeColor(rgb(115, 226, 183), detailAlpha,
                           detailBackground));
        text(canvas, 96 + (70 - textWidth(Ui::BACK)) / 2,
             SHOP_DETAIL_BUTTON_Y + 14, Ui::BACK,
             shopFadeColor(rgb(239, 143, 148), detailAlpha,
                           detailBackground));
    }

    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
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
    static constexpr const char* LABELS[] = {
        Ui::Room::FOOD_ITEM, Ui::Amoled::WASH_PET, Ui::BACK,
    };
    static constexpr const char* DETAILS[] = {
        Ui::Amoled::ROOM_SUPPLIES, Ui::Amoled::WASH_PET, Ui::Amoled::RETURN,
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
    text(canvas, 36, 8, Ui::ROOM, rgb(115, 226, 183));

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
            text(canvas, 164 - textWidth(stock),
                 y + 13, stock, rgb(248, 210, 105));
        }
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

int roomFoodItemAt(int x, int y) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int index = (y - MENU_CONTENT_TOP) / 27;
    return index < Game::ROOM_FOOD_COUNT ? index : -1;
}

bool roomFoodBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

void renderRoomFoodScreen(Canvas565& canvas, const RoomFoodViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* NAMES[] = {
        Ui::NORMAL_FOOD, Ui::TASTY_FOOD, Ui::SWEET_FOOD, Ui::SPICY_FOOD,
        Ui::SOUR_FOOD, Ui::BITTER_FOOD, Ui::DRY_FOOD,
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
    text(canvas, 36, 8, Ui::FOOD, rgb(115, 226, 183));

    for (uint8_t index = 0; index < Game::ROOM_FOOD_COUNT; ++index) {
        int y = MENU_CONTENT_TOP + index * 27;
        bool selected = index == model.selectedFood;
        bool pressed = index == model.pressedItem;
        uint16_t background = pressed ? rgb(48, 74, 68)
                                      : (selected ? rgb(31, 49, 53)
                                                  : rgb(20, 29, 36));
        canvas.fillRoundRect(6, y + 2, 172, 25, 3, background);
        if (selected) canvas.fillRect(9, y + 7, 3, 15, rgb(248, 210, 105));
        Game::ItemId item = Game::itemIdForFoodIndex(index);
        GameAssets::drawCentered(GameAssets::itemKind(item), 25, y + 14, 0.48f);
        text(canvas, 42, y + 9, NAMES[index],
             selected ? rgb(248, 210, 105) : rgb(226, 238, 233));
        char stock[10];
        uint8_t count = model.state ? model.state->room.food[index] : 0;
        std::snprintf(stock, sizeof(stock), "X%u", count);
        text(canvas, 148, y + 9, stock,
             count > 0 ? rgb(115, 226, 183) : rgb(91, 104, 104));
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

bool computerBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int clawTabAt(int x, int y) {
    if (y < 0 || y >= HEADER_HEIGHT) return -1;
    if (x >= CLAW_TAB_CONNECT_LEFT &&
        x < CLAW_TAB_CONNECT_LEFT + CLAW_TAB_WIDTH) {
        return 0;
    }
    if (x >= CLAW_TAB_LOG_LEFT && x < CLAW_TAB_LOG_LEFT + CLAW_TAB_WIDTH) {
        return 1;
    }
    return -1;
}

int computerItemAt(int x, int y, ComputerViewModel::Page page,
                   float storageScroll, uint8_t storageCount,
                   bool clawEnabled) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    if (page == ComputerViewModel::Page::MENU) {
        int row = (y - MENU_CONTENT_TOP) / COMPUTER_MENU_ROW_HEIGHT;
#if STICKMON_HAS_CLAW
        return row < 4 ? row : -1;
#else
        return row < 3 ? row : -1;
#endif
    }
    if (page == ComputerViewModel::Page::AI_HOSTING) {
#if STICKMON_HAS_CLAW
        const int row = (y - MENU_CONTENT_TOP) / COMPUTER_MENU_ROW_HEIGHT;
        const int rowCount = clawEnabled ? 4 : 3;
        return row < rowCount ? row : -1;
#else
        return -1;
#endif
    }
    if (page != ComputerViewModel::Page::STORAGE || storageCount == 0) {
        return -1;
    }
    int contentY = y - MENU_CONTENT_TOP +
                   static_cast<int>(std::lround(storageScroll));
    int row = contentY / 43;
    return row >= 0 && row < storageCount ? row : -1;
}

void drawAiToggle(Canvas565& canvas, int centerX, int centerY, bool on,
                  bool pressed) {
    const uint16_t track = on ? rgb(62, 124, 105) : rgb(60, 72, 80);
    const uint16_t knob = on ? rgb(226, 255, 234) : rgb(168, 181, 184);
    canvas.fillRoundRect(centerX - 18, centerY - 7, 36, 14, 7, track);
    if (pressed) {
        canvas.drawRoundRect(centerX - 18, centerY - 7, 36, 14, 7,
                             rgb(248, 210, 105));
    }
    canvas.fillCircle(centerX + (on ? 10 : -10), centerY, 6, knob);
}

void renderComputerScreen(Canvas565& canvas, const ComputerViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
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
    const char* title = model.page == ComputerViewModel::Page::STATUS
        ? Ui::Amoled::STATUS_PAGE
        : model.page == ComputerViewModel::Page::AI_HOSTING
            ? Ui::Amoled::AI_HOSTING
        : model.page == ComputerViewModel::Page::CLAW_SETUP
            ? Ui::Amoled::BACKEND : Ui::COMPUTER;
    text(canvas, 36, 8, title, rgb(115, 226, 183));
#if STICKMON_HAS_CLAW
    if (model.page == ComputerViewModel::Page::CLAW_SETUP) {
        drawClawTabs(canvas, model.clawLogView);
    }
#endif

    if (model.page == ComputerViewModel::Page::MENU) {
        static constexpr const char* ITEMS[] = {
            Ui::Amoled::STATUS_PAGE, Ui::Amoled::STORAGE_PAGE,
#if STICKMON_HAS_CLAW
            Ui::Amoled::AI_HOSTING, Ui::BACK,
#else
            Ui::BACK,
#endif
        };
        constexpr int ITEM_COUNT = sizeof(ITEMS) / sizeof(ITEMS[0]);
        for (int index = 0; index < ITEM_COUNT; ++index) {
            int y = MENU_CONTENT_TOP + index * COMPUTER_MENU_ROW_HEIGHT;
            bool selected = index == model.pressedItem;
            canvas.fillRoundRect(6, y + 2, 172, COMPUTER_MENU_CELL_HEIGHT, 4,
                                 selected ? rgb(42, 61, 68) : rgb(24, 34, 42));
            text(canvas, 20, y + 13, ITEMS[index],
                 index == ITEM_COUNT - 1 ? rgb(115, 226, 183)
                                         : rgb(226, 238, 233));
            if (index == 1 && model.state) {
                char count[12];
                std::snprintf(count, sizeof(count), "%u/20",
                              model.state->storageCount);
                text(canvas, 136, y + 13, count, rgb(248, 210, 105));
            }
        }
    } else if (model.page == ComputerViewModel::Page::AI_HOSTING) {
#if STICKMON_HAS_CLAW
        static constexpr int AI_ROW_COUNT = 4;
        int rowCount = model.clawEnabled ? AI_ROW_COUNT : 3;
        const char* labels[AI_ROW_COUNT] = {
            Ui::Amoled::WIFI, Ui::Amoled::ESP_CLAW,
            Ui::Amoled::BACKEND, Ui::BACK,
        };
        const bool values[AI_ROW_COUNT] = {
            model.wifiEnabled, model.clawEnabled, false, false,
        };
        for (int index = 0; index < rowCount; ++index) {
            int y = MENU_CONTENT_TOP + index * COMPUTER_MENU_ROW_HEIGHT;
            bool selected = index == model.pressedItem;
            canvas.fillRoundRect(6, y + 2, 172, COMPUTER_MENU_CELL_HEIGHT, 4,
                                 selected ? rgb(42, 61, 68)
                                          : rgb(24, 34, 42));
            text(canvas, 20, y + 13, labels[index],
                 index == rowCount - 1 ? rgb(115, 226, 183)
                                       : rgb(226, 238, 233));
            if (index < 2) {
                drawAiToggle(canvas, 142, y + 21, values[index], selected);
            }
        }
#endif
    } else if (model.page == ComputerViewModel::Page::STATUS) {
        uint8_t count = model.state
            ? Game::TeamRoster::memberCount(*model.state) : 0;
        if (count == 0) {
            text(canvas, 52, 102, Ui::Amoled::TEAM_EMPTY,
                 rgb(126, 145, 145));
        }
        for (uint8_t index = 0; index < count && index < 2; ++index) {
            const Game::MonsterRuntime& monster = model.state->team[index];
            const Species* species = findSpecies(monster.speciesId);
            int y = MENU_CONTENT_TOP + index * 78;
            canvas.fillRoundRect(6, y + 3, 172, 70, 4, rgb(24, 34, 42));
            if (species) {
                const PokemonSprites::SpriteFrame* frame =
                    PokemonSprites::findSpeciesSprite(
                        monster.speciesId, PokemonSprites::SpriteKind::ICON_0);
                if (frame) PokemonSprites::drawFrameScaled(
                    frame, 14, y + 13, 0.62f, false);
                text(canvas, 50, y + 10, species->name, rgb(226, 238, 233));
            }
            char line[28];
            std::snprintf(line, sizeof(line), Ui::Amoled::STATUS_LINE_FMT,
                          monster.level, monster.hpCur, monster.hpMax);
            text(canvas, 50, y + 30, line, rgb(126, 175, 175));
            std::snprintf(line, sizeof(line), Ui::Amoled::HUNGER_FMT,
                          Game::HomeHud::hungerPercent(monster));
            text(canvas, 50, y + 50, line, rgb(248, 210, 105));
        }
    } else if (model.page == ComputerViewModel::Page::STORAGE) {
        uint8_t count = model.state ? model.state->storageCount : 0;
        if (count == 0) {
            text(canvas, 48, 102, Ui::Amoled::STORAGE_EMPTY,
                 rgb(126, 145, 145));
        }
        for (uint8_t index = 0; index < count && index < Game::STORAGE_CAP;
             ++index) {
            int y = MENU_CONTENT_TOP + index * 43 -
                    static_cast<int>(std::lround(model.storageScroll));
            if (y + 43 <= MENU_CONTENT_TOP || y >= 224) continue;
            const Game::MonsterRuntime& monster = model.state->storage[index];
            const Species* species = findSpecies(monster.speciesId);
            bool selected = index == model.pressedItem;
            if (selected) canvas.fillRoundRect(6, y + 2, 172, 39, 4,
                                               rgb(42, 61, 68));
            if (species) {
                const PokemonSprites::SpriteFrame* frame =
                    PokemonSprites::findSpeciesSprite(
                        monster.speciesId, PokemonSprites::SpriteKind::ICON_0);
                if (frame) PokemonSprites::drawFrameScaled(frame, 14, y + 6,
                                                             0.5f, false);
                text(canvas, 42, y + 7, species->name,
                     selected ? rgb(248, 210, 105) : rgb(226, 238, 233));
            }
            char status[24];
            std::snprintf(status, sizeof(status), Ui::Amoled::STATUS_LINE_FMT,
                          monster.level, monster.hpCur, monster.hpMax);
            text(canvas, 42, y + 25, status, rgb(126, 175, 175));
        }
        int maxScroll = std::max(
            0, static_cast<int>(count) * 43 - (224 - MENU_CONTENT_TOP));
        if (model.storageScroll > 0.5f) {
            canvas.fillTriangle(164, 34, 170, 34, 167, 30,
                                rgb(126, 175, 175));
        }
        if (model.storageScroll < maxScroll - 0.5f) {
            canvas.fillTriangle(164, 215, 170, 215, 167, 219,
                                rgb(126, 175, 175));
        }
#if STICKMON_HAS_CLAW
    } else if (!model.clawLogView) {
        const uint16_t labelColor = rgb(115, 226, 183);
        const uint16_t valueColor = rgb(226, 238, 233);
        const char* ip = model.clawIp && model.clawIp[0]
            ? model.clawIp : "192.168.4.1";
        const bool portalReady = model.clawSsid && model.clawSsid[0] &&
                                 model.clawPassword && model.clawPassword[0];
        const char* ssid = portalReady ? model.clawSsid : "NOT READY";
        const char* password = portalReady ? model.clawPassword : "NOT READY";
        const int contentBottom = portalReady
            ? drawClawQr(canvas, model.clawSsid, model.clawPassword)
            : drawClawQr(canvas, nullptr, nullptr);
        // textWidth() matches the shared font advances (8 per ASCII char,
        // 16 per CJK char), so the URL stays centered regardless of content.
        char address[32];
        std::snprintf(address, sizeof(address), "http://%s", ip);
        const int urlY = contentBottom + 6;
        const int addressX =
            std::max(0, (canvas.width() - textWidth(address)) / 2);
        PixelRenderer::text(canvas, addressX, urlY, address, valueColor);
        const int wifiY = urlY + 20;
        const int passY = wifiY + 20;
        PixelRenderer::text(canvas, 4, wifiY, Ui::Amoled::CLAW_WIFI,
                            labelColor);
        PixelRenderer::text(canvas,
                            4 + textWidth(Ui::Amoled::CLAW_WIFI) + 8, wifiY,
                            ssid, valueColor);
        PixelRenderer::text(canvas, 4, passY, Ui::Amoled::CLAW_PASSWORD,
                            labelColor);
        PixelRenderer::text(canvas,
                            4 + textWidth(Ui::Amoled::CLAW_PASSWORD) + 8,
                            passY, password, valueColor);
    } else {
        // Log view: keep each service status on its own row above the log.
        // The log window starts below all three rows so values cannot overlap.
        const bool staConnected = model.clawStaConnected;
        text(canvas, 4, 26,
             staConnected ? Ui::Amoled::CLAW_WIFI_ON : Ui::Amoled::CLAW_WIFI_OFF,
             staConnected ? rgb(115, 226, 183) : rgb(248, 210, 105));
        text(canvas, 4, 42, Ui::Amoled::CLAW_WECHAT_LABEL,
             rgb(115, 226, 183));
        text(canvas, 52, 42,
             clawWechatPhaseText(model.clawWechatPhase,
                                 model.clawWechatPersisted),
             clawWechatPhaseColor(model.clawWechatPhase,
                                  model.clawWechatPersisted));
        text(canvas, 4, 58, Ui::Amoled::ESP_CLAW,
             rgb(115, 226, 183));
        text(canvas, 84, 58,
             model.clawStarted ? Ui::Amoled::CLAW_AGENT_ON
                               : Ui::Amoled::CLAW_AGENT_OFF,
             model.clawStarted ? rgb(115, 226, 183) : rgb(248, 210, 105));
        canvas.drawRect(4, CLAW_LOG_TOP, 176, CLAW_LOG_HEIGHT,
                        rgb(56, 87, 89));
        if (model.clawLogCount == 0 || !model.clawLog) {
            text(canvas, 60, CLAW_LOG_TOP + (CLAW_LOG_HEIGHT - 16) / 2,
                 Ui::Amoled::CLAW_LOG_EMPTY,
                 rgb(126, 145, 145));
        } else {
            canvas.setClipRect(5, CLAW_LOG_TOP + 1, 174, CLAW_LOG_HEIGHT - 2);
            PixelRenderer::canvas().setClipRect(5, CLAW_LOG_TOP + 1, 174,
                                                CLAW_LOG_HEIGHT - 2);
            const int scroll =
                std::max(0, static_cast<int>(std::lround(model.clawLogScroll)));
            const size_t first =
                static_cast<size_t>(scroll / CLAW_LOG_ROW_HEIGHT);
            for (size_t i = first; i < model.clawLogCount; ++i) {
                const int y = CLAW_LOG_TOP + 4 +
                    static_cast<int>(i) * CLAW_LOG_ROW_HEIGHT - scroll;
                if (y >= CLAW_LOG_TOP + CLAW_LOG_HEIGHT) break;
                text(canvas, 10, y, model.clawLog[i].text,
                     clawLogLevelColor(model.clawLog[i].level));
            }
            canvas.setClipRect(0, rowBegin, canvas.width(),
                               rowEnd - rowBegin);
            PixelRenderer::canvas().setClipRect(0, rowBegin, canvas.width(),
                                                rowEnd - rowBegin);
            const int maxScroll = std::max(
                0, static_cast<int>(model.clawLogCount) * CLAW_LOG_ROW_HEIGHT -
                       CLAW_LOG_VIEWPORT);
            if (model.clawLogScroll > 0.5f) {
                canvas.fillTriangle(164, CLAW_LOG_TOP + 8, 170,
                                    CLAW_LOG_TOP + 8, 167, CLAW_LOG_TOP + 4,
                                    rgb(126, 175, 175));
            }
            if (model.clawLogScroll < maxScroll - 0.5f) {
                canvas.fillTriangle(164, 211, 170, 211, 167, 215,
                                    rgb(126, 175, 175));
            }
        }
#endif
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

bool settingsBackAt(int x, int y) {
    return x >= 0 && x < 30 && y >= 0 && y < HEADER_HEIGHT;
}

int settingsItemAt(int x, int y) {
    if (x < 6 || x >= 178 || y < MENU_CONTENT_TOP || y >= 224) return -1;
    int index = (y - MENU_CONTENT_TOP) / 32;
    return index < 6 ? index : -1;
}

void drawSettingsSlider(Canvas565& canvas, int y, uint8_t value,
                        uint8_t minimum, uint8_t maximum, bool pressed) {
    const int trackY = y + SETTINGS_SLIDER_OFFSET_Y;
    const int trackWidth = SETTINGS_SLIDER_RIGHT - SETTINGS_SLIDER_LEFT;
    const int range = std::max<int>(1, maximum - minimum);
    const int clamped = std::clamp<int>(value, minimum, maximum);
    const int knobX = SETTINGS_SLIDER_LEFT +
        (clamped - minimum) * trackWidth / range;
    const uint16_t track = pressed ? rgb(64, 83, 88) : rgb(48, 63, 70);
    const uint16_t active = pressed ? rgb(115, 226, 183) : rgb(83, 184, 157);
    canvas.fillRoundRect(SETTINGS_SLIDER_LEFT, trackY - 4,
                         trackWidth, 8, 4, track);
    if (knobX > SETTINGS_SLIDER_LEFT) {
        canvas.fillRoundRect(SETTINGS_SLIDER_LEFT, trackY - 4,
                             knobX - SETTINGS_SLIDER_LEFT, 8, 4, active);
    }
    canvas.fillCircle(knobX, trackY, pressed ? 7 : 6, rgb(226, 238, 233));
    canvas.fillCircle(knobX, trackY, pressed ? 4 : 3, active);
}

void renderSettingsScreen(Canvas565& canvas, const SettingsViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    static constexpr const char* LABELS[] = {
        Ui::BRIGHTNESS, Ui::Amoled::VOLUME, Ui::Amoled::GAME_SPEED,
        Ui::Amoled::POWER_SAVE, Ui::Amoled::VOICE_CALL, Ui::BACK,
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
    text(canvas, 36, 8, Ui::SETTINGS, rgb(115, 226, 183));
    for (int index = 0; index < 6; ++index) {
        int y = MENU_CONTENT_TOP + index * 32;
        bool pressed = index == model.pressedItem;
        if (pressed) canvas.fillRect(6, y + 2, 172, 28, rgb(42, 61, 68));
        text(canvas, 14, y + 10, LABELS[index],
             index == 5 ? rgb(115, 226, 183) : rgb(226, 238, 233));
        char value[20] = {};
        if (model.state) {
            const auto& settings = model.state->settings;
            if (index == 0) {
                drawSettingsSlider(canvas, y, model.brightness, 32, 255,
                                   pressed);
            }
            if (index == 1) {
                drawSettingsSlider(canvas, y, model.volume, 0, 100, pressed);
            }
            if (index == 2) std::snprintf(value, sizeof(value), "%ux",
                                          1U << std::min<uint8_t>(settings.speedIndex, 3));
            if (index == 3) {
                static constexpr const char* POWER[] = {
                    Ui::Amoled::IDLE_30S, Ui::Amoled::IDLE_2MIN,
                    Ui::Amoled::IDLE_5MIN, Ui::Amoled::IDLE_10MIN,
                    Ui::Settings::IDLE_NEVER,
                };
                std::snprintf(value, sizeof(value), "%s",
                              POWER[std::min<uint8_t>(settings.idleTimeoutIndex, 4)]);
            }
            if (index == 4) std::snprintf(value, sizeof(value), "%s",
                                          settings.voiceCallEnabled
                                              ? Ui::Amoled::VALUE_ON
                                              : Ui::Amoled::VALUE_OFF);
        }
        if (value[0]) text(canvas, 142, y + 10, value, rgb(248, 210, 105));
    }
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

int progressionItemAt(int x, int y, ProgressionViewModel::Mode mode) {
    if (mode == ProgressionViewModel::Mode::MOVE_REPLACE) {
        if (y >= 106 && y < 142) return 1;
        if (y >= 146 && y < 182) return 2;
    }
    return x >= 20 && x < 164 && y >= 174 && y < 214 ? 0 : -1;
}

void renderProgressionScreen(Canvas565& canvas,
                             const ProgressionViewModel& model,
                             uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;
    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    PixelRenderer::canvas().setClipRect(
        0, rowBegin, canvas.width(), rowEnd - rowBegin);
    canvas.fillRect(0, 0, canvas.width(), canvas.height(), rgb(12, 18, 25));
    canvas.fillRect(0, 0, canvas.width(), HEADER_HEIGHT, rgb(19, 31, 39));
    canvas.fillRect(0, HEADER_HEIGHT - 1, canvas.width(), 1, rgb(56, 87, 89));
    text(canvas, 26, 8, Ui::Amoled::GROWTH, rgb(115, 226, 183));
    const Game::MonsterRuntime* monster = model.state &&
        model.teamSlot < Game::TEAM_CAP ? &model.state->team[model.teamSlot] : nullptr;
    const Species* species = monster ? findSpecies(monster->speciesId) : nullptr;
    if (species) {
        const PokemonSprites::SpriteFrame* frame =
            PokemonSprites::findSpeciesSprite(species->id, PokemonSprites::SpriteKind::FRONT);
        if (frame) PokemonSprites::drawFrameScaled(frame, 52, 44, 0.75f, false);
    }
    const char* title = Ui::Amoled::LEVEL_UP;
    if (model.mode == ProgressionViewModel::Mode::EVOLUTION) {
        title = Ui::Common::EVOLUTION_TITLE;
    }
    if (model.mode == ProgressionViewModel::Mode::MOVE_LEARN) {
        title = Ui::Amoled::NEW_MOVE;
    }
    if (model.mode == ProgressionViewModel::Mode::MOVE_REPLACE) {
        title = Ui::Amoled::REPLACE_MOVE;
    }
    text(canvas, 72, 40, title, rgb(248, 210, 105));
    if (model.mode == ProgressionViewModel::Mode::LEVEL_UP) {
        char line[32];
        std::snprintf(line, sizeof(line), Ui::Amoled::LEVEL_FMT,
                      model.level);
        text(canvas, 76, 112, line, rgb(226, 238, 233));
    } else if (model.mode == ProgressionViewModel::Mode::EVOLUTION) {
        const Species* target = findSpecies(model.toSpeciesId);
        text(canvas, 54, 112, target ? target->name : Ui::Amoled::READY,
             rgb(226, 238, 233));
        text(canvas, 42, 132, Ui::Amoled::TAP_TO_EVOLVE,
             rgb(126, 175, 175));
    } else {
        const MoveInfo* move = findMove(model.moveId);
        text(canvas, 44, 112, move ? move->name : Ui::Status::MOVE_UNKNOWN,
             rgb(226, 238, 233));
        if (model.mode == ProgressionViewModel::Mode::MOVE_REPLACE) {
            text(canvas, 34, 134, Ui::Amoled::CHOOSE_OLD_MOVE,
                 rgb(126, 175, 175));
            canvas.fillRoundRect(20, 106, 144, 32, 4,
                                 model.pressedItem == 1 ? rgb(48, 74, 68) : rgb(24, 34, 42));
            canvas.fillRoundRect(20, 146, 144, 32, 4,
                                 model.pressedItem == 2 ? rgb(48, 74, 68) : rgb(24, 34, 42));
            const MoveInfo* move2 = findMove(model.oldMove2);
            const MoveInfo* move3 = findMove(model.oldMove3);
            text(canvas, 30, 117, move2 ? move2->name : Ui::EMPTY,
                 rgb(226, 238, 233));
            text(canvas, 30, 157, move3 ? move3->name : Ui::EMPTY,
                 rgb(226, 238, 233));
        }
    }
    canvas.fillRoundRect(20, 174, 144, 40, 4,
                         model.pressedItem == 0 ? rgb(48, 74, 68) : rgb(24, 34, 42));
    text(canvas, 65, 190,
         model.mode == ProgressionViewModel::Mode::MOVE_REPLACE
             ? Ui::Amoled::SKIP : Ui::Amoled::CONTINUE,
         rgb(115, 226, 183));
    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

int battleItemAt(int x, int y, BattleViewModel::Phase phase) {
    if (x < 0 || x >= 184 || y < BATTLE_FOOTER_Y || y >= 224) return -1;
    if (phase == BattleViewModel::Phase::SWITCH_SELECT) {
        return x < 92 ? 0 : 1;
    }
    if (phase == BattleViewModel::Phase::BAG_SELECT) {
        return std::min(3, x * 4 / 184);
    }
    if (phase == BattleViewModel::Phase::FRIENDSHIP) {
        return x < 92 ? 0 : 1;
    }
    if (phase != BattleViewModel::Phase::ACTION) return 0;
    return std::min(3, x * 4 / 184);
}

bool battleBackAt(int x, int y) {
    return x >= 154 && x < 184 && y >= 0 && y < HEADER_HEIGHT;
}

const char* battleItemLabel(Game::ItemId item) {
    switch (item) {
    case Game::ItemId::POTION: return Ui::Amoled::ITEM_POTION;
    case Game::ItemId::SUPER_POTION: return Ui::Amoled::ITEM_SUPER;
    case Game::ItemId::MAX_POTION: return Ui::Amoled::ITEM_MAX;
    case Game::ItemId::FULL_RESTORE: return Ui::Amoled::ITEM_RESTORE;
    case Game::ItemId::FULL_HEAL: return Ui::Amoled::ITEM_FULL_HEAL;
    case Game::ItemId::REVIVE: return Ui::Amoled::ITEM_REVIVE;
    case Game::ItemId::ANTIDOTE: return Ui::Amoled::ITEM_ANTIDOTE;
    case Game::ItemId::PARALYZE_HEAL: return Ui::Amoled::ITEM_PARALYZE;
    case Game::ItemId::AWAKENING: return Ui::Amoled::ITEM_SLEEP;
    case Game::ItemId::BURN_HEAL: return Ui::Amoled::ITEM_BURN;
    case Game::ItemId::ICE_HEAL: return Ui::Amoled::ITEM_FREEZE;
    default: return Ui::Amoled::ITEM;
    }
}

namespace {

void drawBattleConditionEffects(Canvas565& canvas, int centerX, int groundY,
                                Game::MajorStatus majorStatus,
                                const BattleSystem::BattleActorState& state,
                                uint32_t nowMs) {
    const uint8_t pulse = static_cast<uint8_t>((nowMs / 180U) % 4U);
    const uint16_t outline = rgb(48, 45, 55);
    switch (majorStatus) {
    case Game::MajorStatus::POISON:
    case Game::MajorStatus::TOXIC: {
        const uint16_t color = majorStatus == Game::MajorStatus::TOXIC
            ? rgb(151, 68, 190) : rgb(185, 91, 204);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 16 + i * 15;
            int y = groundY - 8 - ((pulse + i) % 4) * 4;
            int radius = 2 + ((pulse + i) & 1);
            canvas.fillCircle(x, y, radius + 1, outline);
            canvas.fillCircle(x, y, radius, color);
        }
        break;
    }
    case Game::MajorStatus::PARALYSIS: {
        const uint16_t color = rgb(255, 215, 55);
        const int jitter = (pulse & 1) ? 2 : 0;
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * (23 + jitter);
            int y = groundY - 35 + (side > 0 ? 5 : 0);
            canvas.drawLine(x, y, x - side * 5, y + 6, outline);
            canvas.drawLine(x - side * 5, y + 6, x + side, y + 6, outline);
            canvas.drawLine(x + side, y + 6, x - side * 4, y + 13, outline);
            canvas.drawLine(x, y, x - side * 4, y + 6, color);
            canvas.drawLine(x - side * 4, y + 6, x + side * 2, y + 6, color);
            canvas.drawLine(x + side * 2, y + 6, x - side * 3, y + 13, color);
        }
        break;
    }
    case Game::MajorStatus::SLEEP: {
        const uint16_t color = rgb(116, 169, 232);
        int rise = static_cast<int>((nowMs / 240U) % 3U);
        PixelRenderer::textOutlined(centerX + 18, groundY - 45 - rise * 2,
                                    "Z", color, outline, 1);
        PixelRenderer::textOutlined(centerX + 28, groundY - 55 - rise * 2,
                                    "Z", color, outline, 1);
        break;
    }
    case Game::MajorStatus::BURN: {
        const int sway = (pulse & 1) ? 2 : -1;
        const uint16_t red = rgb(226, 76, 42);
        const uint16_t yellow = rgb(255, 191, 46);
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * 19;
            int y = groundY - 10;
            canvas.fillTriangle(x - 5, y + 7, x + 5, y + 7,
                                x + sway * side, y - 7, outline);
            canvas.fillTriangle(x - 4, y + 6, x + 4, y + 6,
                                x + sway * side, y - 6, red);
            canvas.fillTriangle(x - 2, y + 5, x + 2, y + 5,
                                x - sway * side, y, yellow);
        }
        break;
    }
    case Game::MajorStatus::FREEZE: {
        const uint16_t color = rgb(116, 219, 239);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 20 + i * 20;
            int y = groundY - 13 - ((pulse + i) & 1) * 5;
            canvas.drawLine(x - 4, y, x + 4, y, outline);
            canvas.drawLine(x, y - 4, x, y + 4, outline);
            canvas.drawLine(x - 3, y - 3, x + 3, y + 3, outline);
            canvas.drawLine(x - 3, y + 3, x + 3, y - 3, outline);
            canvas.drawLine(x - 3, y, x + 3, y, color);
            canvas.drawLine(x, y - 3, x, y + 3, color);
        }
        break;
    }
    default:
        break;
    }

    if (state.confusionTurns > 0) {
        const uint16_t colors[] = {rgb(255, 215, 55), rgb(239, 119, 116),
                                   rgb(111, 211, 221)};
        float angle = nowMs * 0.006f;
        for (uint8_t i = 0; i < 3; ++i) {
            float pointAngle = angle + i * 2.0943951f;
            int x = centerX + static_cast<int>(std::lround(
                std::cos(pointAngle) * 17.0f));
            int y = groundY - 50 + static_cast<int>(std::lround(
                std::sin(pointAngle) * 4.0f));
            canvas.fillCircle(x, y, 3, outline);
            canvas.fillCircle(x, y, 2, colors[i]);
        }
    }
    if (state.bindTurns > 0) {
        const uint16_t color = rgb(205, 154, 86);
        int y = groundY - 22 + ((pulse & 1) ? 1 : -1);
        canvas.drawFastHLine(centerX - 24, y, 48, outline);
        canvas.drawFastHLine(centerX - 24, y + 5, 48, outline);
        canvas.drawFastHLine(centerX - 23, y + 1, 46, color);
        canvas.drawFastHLine(centerX - 23, y + 4, 46, color);
    }
    if (state.yawnTurns > 0 && majorStatus != Game::MajorStatus::SLEEP) {
        const uint16_t color = rgb(181, 202, 225);
        int drift = static_cast<int>((nowMs / 260U) % 3U);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX + 15 + i * 6;
            int y = groundY - 45 - i * 2 - drift;
            canvas.fillCircle(x, y, 2, outline);
            canvas.fillCircle(x, y, 1, color);
        }
    }
}

void drawBattleSprite(Canvas565& canvas, uint16_t speciesId,
                      int centerX, int groundY, int maxWidth, int maxHeight,
                      bool back) {
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, back ? PokemonSprites::SpriteKind::BACK
                            : PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        drawFallbackPet(canvas, centerX, groundY);
        return;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return;
    float scale = std::min(static_cast<float>(maxWidth) / width,
                           static_cast<float>(maxHeight) / height);
    scale = std::min(1.0f, scale);
    int drawWidth = std::max(1, static_cast<int>(width * scale));
    int drawHeight = std::max(1, static_cast<int>(height * scale));
    canvas.fillEllipse(centerX, groundY, std::max(12, drawWidth / 2 - 4),
                       5, rgb(27, 48, 48));
    PokemonSprites::drawFrameScaled(
        frame, centerX - drawWidth / 2, groundY - drawHeight, scale, false);
}

void drawBattleStatusIcon(Canvas565& canvas, Game::MajorStatus status,
                          int x, int y) {
    GameAssets::Kind kind = GameAssets::statusKind(status);
    if (kind != GameAssets::Kind::COUNT) {
        GameAssets::draw(kind, x, y);
    }
}

void drawBattleHitEffect(Canvas565& canvas, int centerX, int centerY,
                         uint8_t animationFrame, uint16_t damage) {
    if (animationFrame == 0) return;
    const uint16_t outline = rgb(43, 39, 44);
    const uint16_t flash = animationFrame == 2
        ? rgb(239, 143, 148) : rgb(248, 210, 105);
    int radius = animationFrame == 2 ? 15 : 10;
    canvas.drawCircle(centerX, centerY, radius + 2, outline);
    canvas.drawCircle(centerX, centerY, radius, flash);
    canvas.drawLine(centerX - radius - 5, centerY,
                    centerX - radius + 1, centerY, flash);
    canvas.drawLine(centerX + radius - 1, centerY,
                    centerX + radius + 5, centerY, flash);
    canvas.drawLine(centerX, centerY - radius - 5,
                    centerX, centerY - radius + 1, flash);
    canvas.drawLine(centerX, centerY + radius - 1,
                    centerX, centerY + radius + 5, flash);
    if (animationFrame == 2) {
        canvas.fillCircle(centerX - 9, centerY - 8, 2, flash);
        canvas.fillCircle(centerX + 10, centerY + 7, 2, flash);
    }
    if (damage > 0) {
        char damageText[8] = {};
        std::snprintf(damageText, sizeof(damageText), "-%u", damage);
        text(canvas, centerX - textWidth(damageText) / 2,
             centerY - radius - 13, damageText, flash);
    }
}

void drawBattleSceneText(int x, int y, const char* value) {
    PixelRenderer::textOutlined(x, y, value, rgb(25, 31, 40),
                                rgb(241, 242, 232), 1);
}

void drawBattleFooter(Canvas565& canvas) {
    PixelRenderer::fillRectAlpha(
        0, BATTLE_FOOTER_Y, canvas.width(), BATTLE_FOOTER_HEIGHT,
        rgb(204, 204, 204), 153);
    canvas.drawRect(0, BATTLE_FOOTER_Y, canvas.width(),
                    BATTLE_FOOTER_HEIGHT, rgb(74, 91, 75));
}

void drawBattleBackIcon(Canvas565& canvas) {
    drawHeaderButton(canvas, 158);
    const uint16_t color = rgb(222, 234, 229);
    canvas.drawLine(174, 7, 167, 12, color);
    canvas.drawLine(167, 12, 174, 17, color);
}

void drawBattleFooterChoice(Canvas565& canvas, int index, int count,
                            const char* label, bool pressed, bool enabled) {
    if (count <= 0 || index < 0 || index >= count) return;
    int left = index * canvas.width() / count;
    int right = (index + 1) * canvas.width() / count;
    if (pressed) {
        PixelRenderer::fillRectAlpha(left + 1, BATTLE_FOOTER_Y + 1,
                                    right - left - 1,
                                    BATTLE_FOOTER_HEIGHT - 2,
                                    rgb(54, 111, 94), 96);
        canvas.fillRect(left + 4, BATTLE_FOOTER_Y + 3,
                        std::max(1, right - left - 8), 2,
                        rgb(62, 150, 124));
    }
    if (index > 0) {
        canvas.drawFastVLine(left, BATTLE_FOOTER_Y + 9,
                            BATTLE_FOOTER_HEIGHT - 18,
                            rgb(105, 116, 108));
    }
    const uint16_t color = enabled ? rgb(25, 31, 40) : rgb(102, 108, 108);
    int labelX = left + (right - left - textWidth(label)) / 2;
    text(canvas, labelX, BATTLE_FOOTER_Y + 19, label, color);
}

}  // namespace

void renderBattleScreen(Canvas565& canvas, const BattleViewModel& model,
                        uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, canvas.height());
    rowEnd = std::min<uint16_t>(rowEnd, canvas.height());
    if (rowBegin >= rowEnd) return;
    canvas.setClipRect(0, rowBegin, canvas.width(), rowEnd - rowBegin);
    PixelRenderer::canvas().setClipRect(
        0, rowBegin, canvas.width(), rowEnd - rowBegin);
    drawBattleBackgroundLayer(
        canvas, model.battleBackground, rowBegin, rowEnd);
    const Species* wild = findSpecies(model.wildSpeciesId);
    const Species* player = findSpecies(model.playerSpeciesId);
    if (wild) drawBattleSceneText(6, 8, wild->name);
    char level[10];
    std::snprintf(level, sizeof(level), "LV%u", model.wildLevel);
    drawBattleSceneText(58, 8, level);
    int wildX = 136;
    int playerX = 40;
    if (model.animationActive) {
        static constexpr int LUNGE[] = {0, 6, 13, 19, 13, 6, 0};
        uint8_t frame = std::min<uint8_t>(model.animationFrame, 6);
        int lunge = LUNGE[frame];
        if (model.animationAttackerWild) wildX -= lunge;
        else playerX += lunge;
        if (model.animationHit && frame >= 3 && frame <= 5) {
            int shake = (frame & 1U) ? -4 : 4;
            if (model.animationAttackerWild) playerX += shake;
            else wildX += shake;
        }
    }
    drawBattleSprite(canvas, model.wildSpeciesId, wildX, 117, 92, 112, false);
    drawBattleStatusIcon(canvas, model.wildStatus, 6, 25);
    drawBattleHpBar(canvas, 22, 36, 64, model.wildHp);

    if (player) drawBattleSceneText(92, 117, player->name);
    std::snprintf(level, sizeof(level), "LV%u", model.playerLevel);
    drawBattleSceneText(150, 117, level);
    drawBattleSprite(canvas, model.playerSpeciesId, playerX, 190, 82, 116, true);
    drawBattleStatusIcon(canvas, model.playerStatus, 92, 134);
    drawBattleHpBar(canvas, 108, 145, 68, model.playerHp);

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (model.debugDrawBounds) {
        canvas.drawRect(wildX - 46, 117 - 112, 92, 112,
                        rgb(255, 32, 32));
        canvas.drawRect(playerX - 41, 190 - 116, 82, 116,
                        rgb(0, 220, 255));
    }
#endif

    uint32_t nowMs = Platform::clock().millis();
    if (model.wildHp > 0) {
        drawBattleConditionEffects(canvas, wildX, 117, model.wildStatus,
                                   model.wildBattleState, nowMs);
    }
    if (model.playerHp > 0) {
        drawBattleConditionEffects(canvas, playerX, 190, model.playerStatus,
                                   model.playerBattleState, nowMs);
    }
    if (model.animationActive && model.animationHit) {
        int hitX = model.animationAttackerWild ? playerX : wildX;
        int hitY = model.animationAttackerWild ? 168 : 96;
        if (model.animationFrame >= 3 && model.animationFrame <= 5) {
            drawBattleHitEffect(
                canvas, hitX, hitY,
                static_cast<uint8_t>(model.animationFrame - 2),
                model.animationDamage);
        }
    }

    drawBattleFooter(canvas);
    if (model.phase == BattleViewModel::Phase::BAG_SELECT ||
        model.phase == BattleViewModel::Phase::SWITCH_SELECT) {
        drawBattleBackIcon(canvas);
    }

    if (model.logCount > 0) {
        for (uint8_t line = 0; line < model.logCount && line < 2; ++line) {
            if (!model.logLines[line]) continue;
            text(canvas, 10, BATTLE_FOOTER_Y + 7 + line * 18,
                 model.logLines[line], rgb(25, 31, 40));
        }
    } else if (model.phase == BattleViewModel::Phase::ACTION) {
        static constexpr const char* ACTIONS[] = {
            Ui::Explore::CMD_BATTLE, Ui::Explore::CMD_BAG,
            Ui::Explore::CMD_SWITCH, Ui::Explore::CMD_FLEE,
        };
        for (int index = 0; index < 4; ++index) {
            bool pressed = index == model.pressedItem;
            drawBattleFooterChoice(canvas, index, 4, ACTIONS[index],
                                   pressed, true);
        }
    } else if (model.phase == BattleViewModel::Phase::BAG_SELECT) {
        for (int index = 0; index < 4; ++index) {
            bool available = index < model.battleBagCount;
            bool pressed = index == model.pressedItem;
            const char* label = available
                ? battleItemLabel(model.battleBagItems[index])
                : Ui::EMPTY;
            drawBattleFooterChoice(canvas, index, 4, label, pressed,
                                   available);
        }
    } else if (model.phase == BattleViewModel::Phase::SWITCH_SELECT) {
        for (int index = 0; index < 2; ++index) {
            bool available = model.state && index < model.teamCount;
            const Game::MonsterRuntime* member = available
                ? &model.state->team[index] : nullptr;
            const Species* species = member ? findSpecies(member->speciesId) : nullptr;
            bool healthy = member && !member->fainted && member->hpCur > 0;
            bool current = static_cast<uint8_t>(index) == model.activeSlot;
            bool pressed = index == model.pressedItem;
            const char* label = current ? Ui::Amoled::ACTIVE :
                                !available ? Ui::EMPTY :
                                !healthy ? Ui::Amoled::FAINT :
                                (species ? species->name : Ui::Amoled::MONSTER);
            drawBattleFooterChoice(canvas, index, 2, label, pressed,
                                   healthy && !current);
        }
    } else if (model.phase == BattleViewModel::Phase::FRIENDSHIP) {
        if (model.friendshipPrompt == BattleViewModel::FriendshipPrompt::OFFER) {
            static constexpr const char* LABELS[] = {
                Ui::Amoled::YES, Ui::Amoled::NO,
            };
            for (int index = 0; index < 2; ++index) {
                bool pressed = index == model.pressedItem;
                drawBattleFooterChoice(canvas, index, 2, LABELS[index],
                                       pressed, true);
            }
        } else if (model.friendshipPrompt ==
                   BattleViewModel::FriendshipPrompt::TEAM) {
            static constexpr const char* LABELS[] = {
                Ui::Amoled::ADD, Ui::Amoled::LATER,
            };
            for (int index = 0; index < 2; ++index) {
                bool pressed = index == model.pressedItem;
                drawBattleFooterChoice(canvas, index, 2, LABELS[index],
                                       pressed, true);
            }
        } else {
            drawBattleFooterChoice(canvas, 0, 1, Ui::Amoled::CONTINUE,
                                   model.pressedItem == 0, true);
        }
    } else {
        const char* label = model.phase == BattleViewModel::Phase::VICTORY
            ? Ui::Amoled::CONTINUE : Ui::Amoled::REST_HOME;
        drawBattleFooterChoice(canvas, 0, 1, label,
                               model.pressedItem == 0, true);
    }
    PixelRenderer::canvas().clearClipRect();
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
    static constexpr const char* LABELS[] = {
        Ui::Amoled::SOAP, Ui::Amoled::BRUSH, Ui::Amoled::RINSE,
        Ui::Amoled::EXIT,
    };
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
        int labelX = x + (SHOWER_TOOL_BUTTON_W - textWidth(LABELS[index])) / 2;
        text(canvas, labelX, SHOWER_TOOLBAR_Y + 33, LABELS[index],
             index == 3 ? rgb(239, 143, 148) : rgb(126, 175, 175));
    }
}

void drawShowerSoapPicker(Canvas565& canvas, const ShowerViewModel& model) {
    canvas.fillRoundRect(8, 66, 168, 98, 6, rgb(17, 27, 34));
    canvas.drawRoundRect(8, 66, 168, 98, 6, rgb(82, 117, 117));
    text(canvas, 58, 75, Ui::Amoled::CHOOSE_SOAP, rgb(226, 238, 233));
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
    text(canvas, 43, 129, Ui::Amoled::FOAM_REMAINS,
         rgb(248, 210, 105));
    text(canvas, 58, 148, Ui::Amoled::LEAVE_BATH,
         rgb(226, 238, 233));
    canvas.fillRoundRect(18, 174, 70, 36, 4,
                         model.exitConfirmYes ? rgb(48, 74, 68)
                                              : rgb(36, 54, 61));
    canvas.fillRoundRect(96, 174, 70, 36, 4,
                         !model.exitConfirmYes ? rgb(76, 48, 54)
                                               : rgb(46, 39, 43));
    text(canvas, 39, 188, Ui::Amoled::EXIT, rgb(115, 226, 183));
    text(canvas, 116, 188, Ui::Amoled::STAY, rgb(239, 143, 148));
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
    text(canvas, 36, 8, Ui::Amoled::WASH_PET, rgb(115, 226, 183));

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
        text(canvas, 59, 145, Ui::Amoled::ALL_CLEAN,
             rgb(115, 226, 183));
    } else if (model.mode == ShowerMode::EXIT_CONFIRM) {
        drawShowerExitConfirm(canvas, model);
    }

    PixelRenderer::canvas().clearClipRect();
    drawToast(canvas, model.toast);
    canvas.clearClipRect();
}

}  // namespace AmoledV1
