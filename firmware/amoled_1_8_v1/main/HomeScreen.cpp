#include "presentation/PokemonShadow.h"
#include "HomeScreen.h"
#include "AmoledGeometry.h"
#include "ui/UiMetrics.h"
#include "ui/UiCommon.h"
#include "ui/RenderCaches.h"
#include "ui/ExploreMapRenderer.h"
#include "ui/BattleSpriteLayout.h"

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
#include "core/FontResource.h"
#include "core/RoomRenderer.h"
#include "core/RoomResource.h"
#include "core/UiStrings.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "game/BondSystem.h"
#include "game/ContactRoster.h"
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
#include "presentation/MenuGrid.h"
#include "presentation/QrCodeGen.h"
#include "presentation/TeamFlag.h"

namespace AmoledV1 {
using UiCommon::rgb;
using UiCommon::textWidth;
using UiCommon::text;
using UiCommon::drawSceneFadeOverlay;
using UiCommon::drawHeaderButton;
using UiCommon::drawBackIcon;
using UiCommon::drawToast;
namespace {

constexpr int HEADER_HEIGHT = HOME_HEADER_HEIGHT;
constexpr int MENU_BUTTON_X = 316;
constexpr int HOME_LOCK_BUTTON_X = 12;
constexpr int HOME_MENU_BUTTON_X = 88;
constexpr int HOME_HUD_BUTTON_Y = 364;
constexpr int HOME_HUD_BUTTON_SIZE = 64;
constexpr int HOME_MONSTER_PANEL_X = 192;
constexpr int HOME_MONSTER_PANEL_W = 164;

void drawHomeHudMessage(Canvas565& canvas, const char* message) {
    // Match the native font widths and keep UTF-8 glyphs intact when wrapping.
    constexpr int maxWidth = AmoledUi::WIDTH - 32;
    char lines[2][96] = {};
    int count = 0;
    for (; count < 2 && *message; ++count) {
        size_t length = 0;
        int width = 0;
        while (*message && *message != '\n') {
            const char* next = message + 1;
            while (*next && (static_cast<uint8_t>(*next) & 0xC0) == 0x80) ++next;
            const size_t bytes = static_cast<size_t>(next - message);
            const int glyphWidth = static_cast<uint8_t>(*message) < 0x80 ? 16 : 32;
            if (width + glyphWidth > maxWidth || length + bytes >= sizeof(lines[0])) break;
            std::memcpy(lines[count] + length, message, bytes);
            length += bytes;
            width += glyphWidth;
            message = next;
        }
        if (*message == '\n') ++message;
        if (count == 1 && *message) {
            while (length && textWidth(lines[count]) + textWidth("...") > maxWidth) {
                --length;
                while (length &&
                       (static_cast<uint8_t>(lines[count][length]) & 0xC0) == 0x80) --length;
                lines[count][length] = '\0';
            }
            std::strcat(lines[count], "...");
        }
    }
    const int glyphHeight = canvas.nativeText() ? FontResource::LARGE_GLYPH_H : 14;
    constexpr int lineGap = 6;
    const int height = count * glyphHeight + (count - 1) * lineGap;
    const int top = HOME_STATUS_TOP + (AmoledUi::HEIGHT - HOME_STATUS_TOP - height) / 2;
    for (int line = 0; line < count; ++line) {
        text(canvas, (AmoledUi::WIDTH - textWidth(lines[line])) / 2,
             top + line * (glyphHeight + lineGap), lines[line], rgb(234, 240, 235));
    }
}

constexpr int MENU_CONTENT_TOP = UiMetrics::PAGE_HEADER_HEIGHT;
constexpr int MENU_VIEWPORT_HEIGHT = 448 - MENU_CONTENT_TOP;
// The computer page is a compact vertical list. Keep its geometry separate
// from the two-column main menu so all four entries fit on one screen.
constexpr int COMPUTER_MENU_ROW_HEIGHT = 86;
constexpr int COMPUTER_MENU_CELL_HEIGHT = 78;
constexpr int ITEM_ROW_HEIGHT = 96;
constexpr float BAG_ITEM_ICON_SCALE = 1.5f;
constexpr int EXPLORE_MENU_PANEL_WIDTH = 120;
constexpr int EXPLORE_MENU_PANEL_ROW_HEIGHT = 60;
constexpr int EXPLORE_MENU_TEXT_INSET =
    (EXPLORE_MENU_PANEL_ROW_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
constexpr int EXPLORE_MENU_PANEL_HEIGHT =
    AppSceneFlow::exploreMenuItemCount() * EXPLORE_MENU_PANEL_ROW_HEIGHT;
constexpr int EXPLORE_MENU_PANEL_TOP =
    (HOME_STATUS_TOP - EXPLORE_MENU_PANEL_HEIGHT) / 2;
constexpr int SHOP_RAIL_DIVIDER_X = SHOP_LEFT_PANEL_WIDTH;
constexpr int SHOP_GRID_LEFT = 122;
constexpr int SHOP_GRID_COLUMN_WIDTH = 116;
constexpr int SHOP_GRID_ROW_HEIGHT = 100;
constexpr int SHOP_SECTION_HEADER_HEIGHT = 48;
constexpr int SHOP_SECTION_GAP = 8;
// Detail actions are drawn directly in the AMOLED framebuffer. The two
// buttons form a centered 140px pair with a 22px bottom margin.
constexpr int SHOP_DETAIL_BUTTON_Y = UiMetrics::DETAIL_BUTTON_Y;
constexpr int SHOP_DETAIL_BUTTON_HEIGHT = UiMetrics::DETAIL_BUTTON_HEIGHT;
constexpr AmoledUi::Rect SHOP_DETAIL_ACTION_RECT = UiMetrics::DETAIL_ACTION_RECT;
constexpr AmoledUi::Rect SHOP_DETAIL_BACK_RECT = UiMetrics::DETAIL_BACK_RECT;

bool drawShopItemCentered(GameAssets::Kind kind, int centerX, int centerY,
                          float scale, uint8_t alpha) {
    // Keep the shop's item anchor and optional detail scaling in one helper.
    const int nativeCenterX = centerX;
    const int nativeCenterY = centerY;
    const float nativeScale = scale;
    return GameAssets::drawCenteredAlpha(kind, nativeCenterX, nativeCenterY,
                                         nativeScale, alpha);
}
constexpr int EXPLORE_ROUTE_HUD_X = 8;
constexpr int EXPLORE_ROUTE_HUD_Y = 6;
constexpr int EXPLORE_ROUTE_HUD_HEIGHT = 46;
constexpr int EXPLORE_ROUTE_BAG_BUTTON_X = HOME_LOCK_BUTTON_X;
constexpr int EXPLORE_ROUTE_MENU_BUTTON_X = HOME_MENU_BUTTON_X;
// AMOLED UI assets contain native 48x48 item frames. Lists use 2x nearest
// neighbour drawing so the source pixels remain crisp on the AMOLED panel.
constexpr float SHOP_GRID_ICON_SCALE = 1.5f;
constexpr float SHOP_DETAIL_ICON_END_SCALE = 1.5f;
constexpr uint32_t EXPLORE_PREVIEW_CYCLE_MS = 2800;
constexpr uint32_t EXPLORE_PREVIEW_MOVE_MS = 500;
constexpr uint32_t EXPLORE_PREVIEW_HOLD_MS =
    EXPLORE_PREVIEW_CYCLE_MS - EXPLORE_PREVIEW_MOVE_MS;
constexpr float EXPLORE_ROUTE_BOSS_SCALE = 2.0f;
constexpr float EXPLORE_ROUTE_BOSS_PATROL_DISTANCE = 5.0f;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_CYCLE_MS = 6000;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS = 1600;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS = 2300;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS = 3800;
constexpr uint32_t EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS = 4500;
constexpr int EXPLORE_PREVIEW_CENTER_Y =
    (EXPLORE_PREVIEW_TOP + EXPLORE_PREVIEW_BOTTOM) / 2;
constexpr int EXPLORE_PREVIEW_SLOT_SPACING = 124;
// Explore previews use the high-resolution FRONT pilot assets directly in
// the AMOLED framebuffer. The larger box makes small species readable while
// retaining a hard bound for wide/tall species in the three-item carousel.
constexpr int EXPLORE_PREVIEW_NATIVE_MAX_WIDTH = 104;
constexpr int EXPLORE_PREVIEW_NATIVE_MAX_HEIGHT = 160;
// The action row is authored directly in the 368x448 AMOLED coordinate
// space. Keep the two physical rectangles explicit so their visual anchor is
// not coupled to the legacy 184x224 layout grid.
constexpr AmoledUi::Rect EXPLORE_START_BUTTON_NATIVE{
    20, (EXPLORE_SELECTOR_BUTTON_TOP) + 4, 156, 48};
constexpr AmoledUi::Rect EXPLORE_BACK_BUTTON_NATIVE{
    192, (EXPLORE_SELECTOR_BUTTON_TOP) + 4, 156, 48};
constexpr int EXPLORE_SELECTOR_DIVIDER_NATIVE_Y =
    (EXPLORE_SELECTOR_BUTTON_TOP);
// The shared explore artwork is authored at 240x135. Cover the portrait
// AMOLED canvas and crop the horizontal sides so the scene fills the page.
constexpr float EXPLORE_BACKGROUND_SCALE = 448.0f / 135.0f;
constexpr int EXPLORE_BACKGROUND_X = -216;
constexpr float BATTLE_BACKGROUND_SCALE = 448.0f / 135.0f;
constexpr int BATTLE_BACKGROUND_X = -214;
constexpr int BATTLE_FOOTER_Y = 352;
constexpr int BATTLE_FOOTER_HEIGHT = 448 - BATTLE_FOOTER_Y;

uint16_t exploreRouteBossPatrolPermille(uint32_t phase) {
    if (phase < EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS) return 0;
    if (phase < EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS) {
        return static_cast<uint16_t>(
            (phase - EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS) * 1000U /
            (EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS -
             EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS));
    }
    if (phase < EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS) return 1000;
    if (phase < EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS) {
        return static_cast<uint16_t>(
            1000U -
            (phase - EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS) * 1000U /
                (EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS -
                 EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS));
    }
    return 0;
}

PokemonSprites::WalkDirection exploreRouteDirectionForDelta(
    float dx, float dy, PokemonSprites::WalkDirection fallback) {
    if (std::fabs(dx) < 0.01f && std::fabs(dy) < 0.01f) return fallback;
    if (std::fabs(dx) >= std::fabs(dy)) {
        return dx >= 0.0f ? PokemonSprites::WalkDirection::RIGHT
                          : PokemonSprites::WalkDirection::LEFT;
    }
    return dy >= 0.0f ? PokemonSprites::WalkDirection::DOWN
                      : PokemonSprites::WalkDirection::UP;
}

float exploreRouteAirOffsetY(uint16_t speciesId, uint32_t nowMs,
                             float scale) {
    const PokemonMotion::AirProfile air =
        PokemonMotion::airProfileForSpecies(speciesId);
    if (air.height <= 0.0f) return 0.0f;
    const float phase = static_cast<float>((nowMs + speciesId * 97U) % 1600U) *
                        0.00392699f;
    return (air.height + std::sin(phase) * air.bobAmplitude) * scale;
}

bool drawBattleBackgroundLayer(Canvas565& canvas,
                               PixelCache565& battleBackgroundCache,
                               GameAssets::Kind kind,
                               uint16_t rowBegin, uint16_t rowEnd
#if STICKMON_ENABLE_DEBUG_FEATURES
                               , uint32_t* assetDrawUs, uint32_t* cacheStoreUs
#endif
                               ) {
    const RenderCacheKey key{static_cast<uint32_t>(kind), 0,
                             canvas.physicalWidth(), canvas.physicalHeight(),
                             canvas.byteSwapped()};
    if (battleBackgroundCache.copyRowsTo(canvas, key, rowBegin, rowEnd)) return true;
    battleBackgroundCache.invalidate();

#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t drawStartedUs = assetDrawUs ? Platform::clock().micros() : 0;
#endif
    bool drawn = GameAssets::draw(
        kind, BATTLE_BACKGROUND_X, 0, BATTLE_BACKGROUND_SCALE);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (assetDrawUs) *assetDrawUs = Platform::clock().micros() - drawStartedUs;
#endif
    if (!drawn) {
        canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), rgb(12, 18, 25));
    }

#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t cacheStartedUs = cacheStoreUs ? Platform::clock().micros() : 0;
#endif
    if (drawn && rowBegin == 0 && rowEnd == AmoledUi::HEIGHT) {
        if (uint16_t* pixels = battleBackgroundCache.begin(key, Platform::memory())) {
            std::memcpy(pixels, canvas.rawPixels(), battleBackgroundCache.allocatedBytes());
            battleBackgroundCache.commit();
        }
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (cacheStoreUs) *cacheStoreUs = Platform::clock().micros() - cacheStartedUs;
#endif
    return drawn;
}

bool drawExploreBackgroundLayer(Canvas565& canvas,
                                PixelCache565& exploreBackgroundCache,
                                uint16_t rowBegin,
                                uint16_t rowEnd) {
    const RenderCacheKey key{static_cast<uint32_t>(GameAssets::Kind::EXPLORE_MENU_BACKGROUND), 0,
                             canvas.physicalWidth(), canvas.physicalHeight(),
                             canvas.byteSwapped()};
    if (exploreBackgroundCache.copyRowsTo(canvas, key, rowBegin, rowEnd)) return true;
    exploreBackgroundCache.invalidate();

    bool drawn = GameAssets::draw(GameAssets::Kind::EXPLORE_MENU_BACKGROUND,
                                  EXPLORE_BACKGROUND_X, 0,
                                  EXPLORE_BACKGROUND_SCALE);
    if (!drawn) {
        canvas.fillRect(
            (0), (0),
            (AmoledUi::WIDTH),
            (AmoledUi::HEIGHT),
            rgb(12, 18, 25));
    }

    // Cache the static tint together with the artwork. Partial-frame fallback
    // must only blend the restored rows, otherwise the tint accumulates.
    const int bandEdges[] = {0, EXPLORE_SELECTOR_TOP_HEIGHT,
                            EXPLORE_SELECTOR_BUTTON_TOP, AmoledUi::HEIGHT};
    const uint8_t bandAlpha[] = {148, 96, 178};
    for (int band = 0; band < 3; ++band) {
        const int top = std::max<int>(rowBegin, bandEdges[band]);
        const int bottom = std::min<int>(rowEnd, bandEdges[band + 1]);
        if (top >= bottom) continue;
        PixelRenderer::fillRectAlpha(
            0, (top),
            (AmoledUi::WIDTH),
            (bottom - top), rgb(8, 17, 22), bandAlpha[band]);
    }
    if (drawn && rowBegin == 0 && rowEnd == AmoledUi::HEIGHT) {
        if (uint16_t* pixels = exploreBackgroundCache.begin(key, Platform::memory())) {
            std::memcpy(pixels, canvas.rawPixels(), exploreBackgroundCache.allocatedBytes());
            exploreBackgroundCache.commit();
        }
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

#if STICKMON_HAS_CLAW
// Draws a Wi-Fi pairing QR (scanning it joins the setup hotspot directly) and
// returns the bottom Y of the QR block, so the caller can flow text below it.
// When the portal credentials are unavailable no QR is drawn and the reserved
// top position is returned instead.
int drawClawQr(Canvas565& canvas, const char* ssid, const char* password) {
    constexpr int QUIET_MODULES = 4;  // Spec-mandated quiet zone.
    constexpr int QR_MODULE = 6;
    constexpr int QR_TOP = 60;
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
    const int blockX = (AmoledUi::WIDTH - block) / 2;
    canvas.fillRect((blockX), (QR_TOP), (block), (block), rgb(255, 255, 255));
    const uint16_t black = rgb(0, 0, 0);
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            if (!modules[row * size + column]) continue;
            canvas.fillRect(((blockX + (column + QUIET_MODULES) * QR_MODULE)), ((QR_TOP + (row + QUIET_MODULES) * QR_MODULE)), (QR_MODULE), (QR_MODULE), black);
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
        canvas.fillRoundRect((x), (4), (CLAW_TAB_WIDTH), (HEADER_HEIGHT - 8), (6), active ? rgb(42, 61, 68) : rgb(24, 34, 42));
        const char* label = tab == 0 ? Ui::Amoled::CLAW_TAB_CONNECT
                                     : Ui::Amoled::CLAW_TAB_LOG;
        text(canvas, x + (CLAW_TAB_WIDTH - textWidth(label)) / 2, 8, label,
             active ? rgb(115, 226, 183) : rgb(126, 145, 145));
    }
}
#endif

void drawHomeHudButton(Canvas565& canvas, int x) {
    canvas.fillRoundRect((x), (HOME_HUD_BUTTON_Y), (HOME_HUD_BUTTON_SIZE), (HOME_HUD_BUTTON_SIZE), (8), rgb(27, 43, 51));
    canvas.drawRoundRect((x), (HOME_HUD_BUTTON_Y), (HOME_HUD_BUTTON_SIZE), (HOME_HUD_BUTTON_SIZE), (8), rgb(67, 97, 101));
}

void drawHomeLockIcon(Canvas565& canvas, bool enabled = true) {
    drawHomeHudButton(canvas, HOME_LOCK_BUTTON_X);
    const uint16_t color = enabled ? rgb(222, 234, 229) : rgb(87, 106, 109);
    const uint16_t background = rgb(27, 43, 51);
    const int x = HOME_LOCK_BUTTON_X;
    const int y = HOME_HUD_BUTTON_Y;
    // Reference SVG silhouette, rebuilt with a uniform 3px stroke and no bitmap.
    canvas.fillRoundRect(x + 23, y + 13, 18, 24, 9, color);
    canvas.fillRoundRect(x + 26, y + 16, 12, 18, 6, background);
    canvas.fillRoundRect(x + 16, y + 26, 32, 25, 6, color);
    canvas.fillRoundRect(x + 19, y + 29, 26, 19, 3, background);
    canvas.fillRoundRect(x + 30, y + 34, 4, 10, 2, color);
}

void drawHudMenuIcon(Canvas565& canvas, int x) {
    drawHomeHudButton(canvas, x);
    const uint16_t color = rgb(222, 234, 229);
    for (int row = 0; row < 3; ++row) {
        canvas.fillRoundRect(x + 16, HOME_HUD_BUTTON_Y + 20 + row * 10,
                             32, 4, 2, color);
    }
}

void drawHomeMenuIcon(Canvas565& canvas) {
    drawHudMenuIcon(canvas, HOME_MENU_BUTTON_X);
}

void drawHomeRecallIcon(Canvas565& canvas) {
    drawHomeHudButton(canvas, HOME_MENU_BUTTON_X);
    const uint16_t color = rgb(222, 234, 229);
    const int x = HOME_MENU_BUTTON_X;
    const int y = HOME_HUD_BUTTON_Y;
    canvas.drawLine(x + 25, y + 25, x + 37, y + 15, color);
    canvas.drawLine(x + 37, y + 15, x + 49, y + 25, color);
    canvas.drawRect(x + 28, y + 25, 18, 16, color);
    canvas.drawFastHLine(x + 35, y + 30, 5, color);
    canvas.drawLine(x + 46, y + 46, x + 22, y + 46, color);
    canvas.drawLine(x + 22, y + 46, x + 22, y + 37, color);
    canvas.drawLine(x + 22, y + 37, x + 17, y + 42, color);
    canvas.drawLine(x + 22, y + 37, x + 27, y + 42, color);
}

void drawExploreBagIcon(Canvas565& canvas) {
    drawHomeHudButton(canvas, EXPLORE_ROUTE_BAG_BUTTON_X);
    constexpr uint8_t bagIconIndex = 3;
    const uint16_t offset = FlashStorage::readWord(
        &MenuAssets::MAIN_ICON_FRAMES[bagIconIndex].offset);
    const uint16_t length = FlashStorage::readWord(
        &MenuAssets::MAIN_ICON_FRAMES[bagIconIndex].length);
    // The source frame is 40x40. Keep a small, even inset inside the 64px
    // HUD button so the backpack reads at the same visual weight as the
    // other native HUD controls.
    constexpr float scale = 1.4f;
    constexpr int iconSize = 56;
    PixelRenderer::drawRgb565RleScaled(
        EXPLORE_ROUTE_BAG_BUTTON_X +
            (HOME_HUD_BUTTON_SIZE - iconSize) / 2,
        HOME_HUD_BUTTON_Y + (HOME_HUD_BUTTON_SIZE - iconSize) / 2,
        MenuAssets::FRAME_W, MenuAssets::FRAME_H,
        MenuAssets::MAIN_ICON_RLE, offset, length, scale);
}

void drawExploreMenuIcon(Canvas565& canvas) {
    drawHudMenuIcon(canvas, EXPLORE_ROUTE_MENU_BUTTON_X);
}

void drawHeart(Canvas565& canvas, int x, int y, uint16_t color,
               bool whiteOutline = false) {
    if (whiteOutline) {
        constexpr uint16_t outline = 0xFFFF;
        canvas.fillCircle((x - 4), (y - 2), (7), outline);
        canvas.fillCircle((x + 4), (y - 2), (7), outline);
        canvas.fillTriangle((x - 11), (y), (x + 11), (y),
                            (x), (y + 13), outline);
    }
    canvas.fillCircle((x - 4), (y - 2), (6), color);
    canvas.fillCircle((x + 4), (y - 2), (6), color);
    canvas.fillTriangle((x - 10), (y), (x + 10), (y), (x), (y + 12), color);
}

void drawHeartBurst(Canvas565& canvas, int x, int y, uint16_t ageMs) {
    constexpr uint16_t burstColor = 0xFFFF;
    int phase = std::min<int>(8, ageMs / 50);
    int radius = 10 + phase * 4;
    int length = 6 + (phase * 2);
    canvas.drawLine((x - radius), (y), (x - radius - length), (y), burstColor);
    canvas.drawLine((x + radius), (y), (x + radius + length), (y), burstColor);
    canvas.drawLine((x), (y - radius), (x), (y - radius - length), burstColor);
    canvas.drawLine((x), (y + radius), (x), (y + radius + length), burstColor);
    if (phase >= 2) {
        canvas.drawLine((x - radius + 2), (y - radius + 2), (x - radius - length + 4), (y - radius - length + 4), burstColor);
        canvas.drawLine((x + radius - 2), (y + radius - 2), (x + radius + length - 4), (y + radius + length - 4), burstColor);
    }
}

void drawPlant(Canvas565& canvas) {
    const uint16_t pot = rgb(197, 104, 76);
    const uint16_t leaf = rgb(60, 139, 94);
    canvas.fillRoundRect((36), (210), (44), (30), (6), pot);
    canvas.fillEllipse((48), (204), (10), (26), leaf);
    canvas.fillEllipse((68), (198), (12), (30), rgb(76, 165, 105));
    canvas.fillEllipse((58), (184), (10), (26), rgb(92, 182, 116));
}

void drawFoodContent(Canvas565& canvas, int centerX, int centerY) {
    if (GameAssets::drawCentered(GameAssets::Kind::ITEM_NORMAL_FOOD,
                                 centerX, centerY - 6, 1.0f)) {
        return;
    }
    canvas.fillEllipse((centerX), (centerY), (18), (8), rgb(176, 113, 62));
    canvas.fillEllipse((centerX - 6), (centerY - 4), (8), (4), rgb(221, 155, 77));
    canvas.fillEllipse((centerX + 8), (centerY - 2), (6), (4), rgb(205, 91, 66));
}

void drawBowl(Canvas565& canvas, bool filled) {
    const uint16_t rim = rgb(222, 229, 218);
    const uint16_t bowl = rgb(76, 137, 166);
    canvas.fillEllipse((290), (286), (34), (12), rgb(42, 55, 60));
    canvas.drawFastHLine((256), (286), (70), rim);
    canvas.fillRoundRect((262), (288), (58), (22), (10), bowl);
    canvas.drawFastHLine((270), (308), (42), rgb(43, 88, 112));
    if (filled) drawFoodContent(canvas, 290, 286);
}

void drawPet(Canvas565& canvas, const HomeViewModel& model,
             AmoledUi::Rect* visibleBounds = nullptr);
#if STICKMON_ENABLE_DEBUG_FEATURES
uint16_t blendShadowRgb565(uint16_t background, uint16_t color,
                           uint8_t alpha);

constexpr int DEBUG_CONTACT_BUBBLE_Y = 262;
constexpr int DEBUG_CONTACT_BUBBLE_YES_X = 108;
constexpr int DEBUG_CONTACT_BUBBLE_NO_X = 260;
constexpr int DEBUG_CONTACT_BUBBLE_RADIUS = 38;
constexpr int DEBUG_CONTACT_BUBBLE_HIT_RADIUS = 52;

// Two white bubbles pop up over the room while the bottom HUD fades out and
// shows the visitor message instead.
void drawDebugContactPrompt(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.debugContactChoiceVisible) return;
    const float progress =
        static_cast<float>(model.debugContactPromptFade) / 255.0f;
    if (progress <= 0.0f) return;
    const float clamped = std::min(1.0f, progress * 1.15f);
    const float eased = 1.0f - (1.0f - clamped) * (1.0f - clamped) *
                            (1.0f - clamped);
    const int radius = static_cast<int>(DEBUG_CONTACT_BUBBLE_RADIUS * eased);
    if (radius <= 0) return;
    const uint16_t bubble = rgb(248, 248, 244);
    const uint16_t edge = rgb(122, 134, 130);
    const uint16_t ink = rgb(38, 48, 52);
    const int centers[] = {DEBUG_CONTACT_BUBBLE_YES_X,
                           DEBUG_CONTACT_BUBBLE_NO_X};
    const char* labels[] = {Ui::ContactVisit::YES, Ui::ContactVisit::NO};
    for (int index = 0; index < 2; ++index) {
        if (model.debugContactSelectedChoice >= 0 &&
            model.debugContactSelectedChoice != index) {
            continue;
        }
        canvas.fillCircle(centers[index], DEBUG_CONTACT_BUBBLE_Y, radius,
                          bubble);
        canvas.drawCircle(centers[index], DEBUG_CONTACT_BUBBLE_Y, radius,
                          edge);
        if (progress > 0.55f) {
            text(canvas,
                 centers[index] - textWidth(labels[index]) / 2,
                 DEBUG_CONTACT_BUBBLE_Y - 8, labels[index], ink);
        }
    }
}

void drawDebugContactGuest(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.debugContactActive || model.debugContactSpeciesId == 0) return;
    HomeViewModel guest = model;
    guest.speciesId = model.debugContactSpeciesId;
    guest.petCenterX = 270;
    guest.petGroundY = 302;
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
            canvas.drawPixel(
                (x),
                (y),
                blendShadowRgb565(
                    canvas.readPixel((x),
                                     (y)),
                    color, alpha));
        }
    }
}

void drawDebugLight(Canvas565& canvas, const HomeViewModel& model) {
    if (model.debugLightSource == 0) return;
    int lightX = model.petCenterX;
    int lightY = model.petGroundY - 64;
    switch (model.debugLightSource) {
    case 1: lightX = 88; lightY = HOME_ROOM_TOP + 60; break;
    case 2: lightX = AmoledUi::WIDTH / 2; lightY = HOME_ROOM_TOP + 40; break;
    case 3: lightX = AmoledUi::WIDTH - 88; lightY = HOME_ROOM_TOP + 60; break;
    case 4: lightX = 68; lightY = HOME_ROOM_TOP + HOME_ROOM_HEIGHT / 2; break;
    case 5: lightX = AmoledUi::WIDTH - 68;
            lightY = HOME_ROOM_TOP + HOME_ROOM_HEIGHT / 2; break;
    default: break;
    }
    if (model.night) {
        fillRadialDebugLight(canvas, lightX, lightY, 116, 84,
                             rgb(255, 210, 128), 88);
        fillRadialDebugLight(canvas, lightX, lightY, 224, 152,
                             rgb(255, 151, 92), 30);
    } else {
        fillRadialDebugLight(canvas, lightX, lightY, 136, 92,
                             rgb(255, 226, 150), 30);
        fillRadialDebugLight(canvas, lightX, lightY, 236, 152,
                             rgb(255, 176, 96), 10);
    }
    canvas.fillCircle((lightX), (lightY), (6), rgb(255, 236, 158));
    canvas.drawCircle((lightX), (lightY), (10), rgb(255, 169, 79));
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
        return (static_cast<int>(polygon[index].x) - model.cameraX) * AmoledUi::RESOURCE_SCALE;
    };
    auto pointY = [&](uint8_t index) {
        return HOME_ROOM_TOP + (static_cast<int>(polygon[index].y) -
               model.cameraY) * AmoledUi::RESOURCE_SCALE;
    };
    int previousX = pointX(0);
    int previousY = pointY(0);
    for (uint8_t i = 1; i <= count; ++i) {
        uint8_t index = i == count ? 0 : i;
        int x = pointX(index);
        int y = pointY(index);
        canvas.drawLine((previousX - 2), (previousY), (x - 2), (y), outline);
        canvas.drawLine((previousX + 2), (previousY), (x + 2), (y), outline);
        canvas.drawLine((previousX), (previousY - 2), (x), (y - 2), outline);
        canvas.drawLine((previousX), (previousY + 2), (x), (y + 2), outline);
        canvas.drawLine((previousX), (previousY), (x), (y), red);
        canvas.fillCircle((x), (y), (4), red);
        previousX = x;
        previousY = y;
    }
}

void drawDebugTalkPoints(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.debugTalkPointsVisible) return;

    const int leftX = (model.debugTalkCompanionX - model.cameraX) *
        AmoledUi::RESOURCE_SCALE;
    const int rightX = (model.debugTalkMainX - model.cameraX) *
        AmoledUi::RESOURCE_SCALE;
    const int leftY = HOME_ROOM_TOP +
        (model.debugTalkCompanionY - model.cameraY) *
            AmoledUi::RESOURCE_SCALE;
    const int rightY = HOME_ROOM_TOP +
        (model.debugTalkMainY - model.cameraY) *
            AmoledUi::RESOURCE_SCALE;
    const uint16_t outline = rgb(12, 18, 25);
    const uint16_t left = rgb(102, 205, 240);
    const uint16_t right = rgb(248, 210, 105);
    const uint16_t welcome = rgb(235, 104, 184);

    canvas.drawFastHLine(leftX - 12, leftY, 25, outline);
    canvas.drawFastVLine(leftX, leftY - 12, 25, outline);
    canvas.fillCircle(leftX, leftY, 7, left);
    canvas.drawCircle(leftX, leftY, 10, outline);
    canvas.drawFastHLine(rightX - 12, rightY, 25, outline);
    canvas.drawFastVLine(rightX, rightY - 12, 25, outline);
    canvas.fillCircle(rightX, rightY, 7, right);
    canvas.drawCircle(rightX, rightY, 10, outline);

    const int welcomeX = model.debugWelcomeCenterX;
    const int welcomeY = model.debugWelcomeGroundY;
    canvas.drawFastHLine(welcomeX - 12, welcomeY, 25, outline);
    canvas.drawFastVLine(welcomeX, welcomeY - 12, 25, outline);
    canvas.fillCircle(welcomeX, welcomeY, 7, welcome);
    canvas.drawCircle(welcomeX, welcomeY, 10, outline);
}
#endif

void drawFallbackPet(Canvas565& canvas, int centerX, int groundY,
                     bool drawShadow = true) {
    const int dx = centerX - 184;
    const int dy = groundY - 302;
    const uint16_t shadow = rgb(39, 57, 63);
    const uint16_t body = rgb(79, 185, 140);
    const uint16_t bodyLight = rgb(126, 218, 166);
    const uint16_t belly = rgb(224, 238, 194);
    const uint16_t dark = rgb(22, 38, 44);
    const uint16_t cheek = rgb(242, 112, 103);

    if (drawShadow) {
        canvas.fillEllipse((184 + dx), (298 + dy), (62), (16), shadow);
    }
    canvas.fillTriangle((138 + dx), (188 + dy), (158 + dx), (148 + dy), (172 + dx), (198 + dy), body);
    canvas.fillTriangle((196 + dx), (198 + dy), (212 + dx), (148 + dy), (232 + dx), (190 + dy), body);
    canvas.fillEllipse((184 + dx), (232 + dy), (60), (70), body);
    canvas.fillEllipse((184 + dx), (256 + dy), (40), (42), belly);
    canvas.fillEllipse((168 + dx), (208 + dy), (10), (14), dark);
    canvas.fillEllipse((202 + dx), (208 + dy), (10), (14), dark);
    canvas.fillRect((166 + dx), (204 + dy), (4), (4), bodyLight);
    canvas.fillRect((200 + dx), (204 + dy), (4), (4), bodyLight);
    canvas.fillEllipse((150 + dx), (230 + dy), (12), (8), cheek);
    canvas.fillEllipse((218 + dx), (230 + dy), (12), (8), cheek);
    canvas.drawFastHLine((176 + dx), (232 + dy), (18), dark);
    canvas.drawPixel((174 + dx), (230 + dy), dark);
    canvas.drawPixel((194 + dx), (230 + dy), dark);
    canvas.fillRoundRect((134 + dx), (264 + dy), (32), (20), (8), body);
    canvas.fillRoundRect((204 + dx), (264 + dy), (32), (20), (8), body);
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

void fillRoundRectAlpha(Canvas565& canvas, int x, int y, int width, int height,
                        int radius, uint16_t color, uint8_t alpha) {
    if (width <= 0 || height <= 0 || radius < 0 || alpha == 0) return;
    radius = std::min(radius, std::min(width, height) / 2);
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            int cornerX = px < x + radius ? x + radius
                          : px >= x + width - radius ? x + width - radius - 1
                                                     : px;
            int cornerY = py < y + radius ? y + radius
                          : py >= y + height - radius ? y + height - radius - 1
                                                       : py;
            int dx = px - cornerX;
            int dy = py - cornerY;
            if (dx * dx + dy * dy > radius * radius) continue;
            uint16_t background = canvas.readPixel(
                (px), (py));
            canvas.drawPixel(
                (px), (py),
                blendShadowRgb565(background, color, alpha));
        }
    }
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
            uint16_t background = canvas.readPixel((px), (py));
            canvas.drawPixel((px), (py), blendShadowRgb565(background, color, alpha));
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
            uint16_t background = canvas.readPixel((px), (py));
            canvas.drawPixel((px), (py), blendShadowRgb565(background, color, alpha));
        }
    }
}

struct HomePetLayout {
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    int x = 0, y = 0, width = 0, height = 0;
    AmoledUi::Rect bounds{};
};

PokemonSprites::FrameBounds cachedPetFrameBounds(
        const PokemonSprites::SpriteFrame* frame) {
    struct Entry {
        const PokemonSprites::SpriteFrame* frame = nullptr;
        uint16_t species = 0, kind = 0;
        PokemonSprites::FrameBounds bounds{};
    };
    // Sprite pixels are immutable. Cache per animation frame so focus queries
    // do not scan the sprite's RLE data on every display-loop iteration.
    static Entry entries[16];
    const uint16_t species = FlashStorage::readWord(&frame->speciesId);
    const uint16_t kind = FlashStorage::readWord(&frame->kind);
    Entry& entry = entries[kind % 16];
    if (entry.frame != frame || entry.species != species || entry.kind != kind) {
        entry = {frame, species, kind, PokemonSprites::frameVisibleBounds(frame)};
    }
    return entry.bounds;
}

HomePetLayout homePetLayout(const HomeViewModel& model) {
    HomePetLayout layout;
    if (!model.petVisible) return layout;
    const int renderGroundY = model.petGroundY + model.petRenderOffsetY;
    layout.bounds = {model.petCenterX - 60, renderGroundY - 154, 120, 154};
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
    if (!frame) return layout;
    layout.frame = frame;
    layout.flipX = flipX;
    layout.width = FlashStorage::readByte(&frame->width) * AmoledUi::RESOURCE_SCALE;
    layout.height = FlashStorage::readByte(&frame->height) * AmoledUi::RESOURCE_SCALE;
    layout.x = model.petCenterX - layout.width / 2;
    const int groundOffset = static_cast<int>(std::lround(
        PokemonSprites::frameGroundOffsetY(frame) * AmoledUi::RESOURCE_SCALE));
    layout.y = renderGroundY - layout.height / 2 - groundOffset;
    const auto bounds = cachedPetFrameBounds(frame);
    const int sourceWidth = layout.width / AmoledUi::RESOURCE_SCALE;
    const int left = flipX ? sourceWidth - bounds.left - bounds.width : bounds.left;
    layout.bounds = {layout.x + left * AmoledUi::RESOURCE_SCALE,
                     layout.y + bounds.top * AmoledUi::RESOURCE_SCALE,
                     bounds.width * AmoledUi::RESOURCE_SCALE,
                     bounds.height * AmoledUi::RESOURCE_SCALE};
    return layout;
}

void drawPet(Canvas565& canvas, const HomeViewModel& model,
             AmoledUi::Rect* visibleBounds) {
    if (!model.petVisible) return;
    const int renderGroundY = model.petGroundY + model.petRenderOffsetY;
    const auto layout = homePetLayout(model);
    const auto* frame = layout.frame;
    if (visibleBounds) *visibleBounds = layout.bounds;
    if (!frame) {
        drawFallbackPet(canvas, model.petCenterX, renderGroundY);
        return;
    }
    const int width = layout.width;
    const int height = layout.height;
    const PokemonMotion::AirProfile air =
        PokemonMotion::airProfileForSpecies(model.speciesId);
    const bool floating = air.height > 0.0f;
    const auto shadow = PokemonShadow::atGround(
        frame, model.petCenterX, model.petGroundY,
        AmoledUi::RESOURCE_SCALE, layout.flipX);
    int radiusX = shadow.radiusX;
    int radiusY = shadow.radiusY;
    if (model.night) {
        radiusX = (radiusX * 11 + 10) / 10;
        radiusY = (radiusY * 11 + 10) / 10;
    }
    const uint16_t shadowColor = model.night
        ? rgb(18, 16, 24) : rgb(36, 29, 24);
    const uint8_t outerAlpha = model.night
        ? (floating ? 84 : 122) : (floating ? 68 : 116);
    const uint8_t coreAlpha = model.night
        ? (floating ? 0 : 92) : (floating ? 0 : 86);
    fillSoftShadow(canvas, shadow.x, shadow.y, radiusX, radiusY,
                   shadowColor, outerAlpha);
    if (!floating) {
        fillShadowCore(canvas, shadow.x, shadow.y,
                       std::max(10, radiusX / 2),
                       std::max(4, radiusY / 2), shadowColor, coreAlpha);
    }
    if (!PokemonSprites::drawFrameScaled(
            frame, layout.x, layout.y, 2.0f, layout.flipX)) {
        drawFallbackPet(canvas, model.petCenterX, renderGroundY);
        if (visibleBounds) {
            *visibleBounds = {model.petCenterX - 60, renderGroundY - 154, 120, 154};
        }
    }
}

void drawHomeCompanion(Canvas565& canvas, const HomeViewModel& model) {
    if (!model.companionVisible || model.companionSpeciesId == 0) return;
    HomeViewModel companion = model;
    // The companion has its own visibility lifecycle. In particular, the
    // leader disappears after crossing the door while the companion still
    // needs to walk across the room.
    companion.petVisible = true;
    companion.speciesId = model.companionSpeciesId;
    companion.petCenterX = model.companionCenterX;
    companion.petGroundY = model.companionGroundY;
    companion.petRenderOffsetY = model.companionRenderOffsetY;
    companion.petFrame = model.companionFrame;
    companion.petDirection = model.companionDirection;
    companion.petAction = model.companionAction;
    companion.petLongMove = model.companionLongMove;
    companion.petResting = model.companionResting;
    companion.showHearts = false;
    companion.moodBurstAgeMs = 0;
    drawPet(canvas, companion);
}

void drawHomeHpBar(Canvas565& canvas, int x, int y, int width,
                   uint8_t percent, bool fainted = false) {
    canvas.fillRect((x), (y), (width), (12), rgb(39, 45, 50));
    int filled = percent > 0
        ? std::max(2, (width - 4) * percent / 100) : 0;
    uint16_t fillColor = fainted ? rgb(156, 174, 181)
        : percent > 50 ? rgb(92, 222, 112)
        : (percent > 20 ? rgb(246, 204, 72) : rgb(232, 80, 84));
    if (filled > 0) canvas.fillRect((x + 2), (y + 2), (filled), (8), fillColor);
    canvas.drawRect((x), (y), (width), (12), rgb(220, 224, 218));
}

void drawBattleHpBar(Canvas565& canvas, int x, int y, int width,
                     uint8_t percent) {
    canvas.fillRect((x), (y), (width), (12), rgb(39, 45, 50));
    int filled = (width - 4) * percent / 100;
    uint16_t fillColor = percent > 50
        ? rgb(92, 222, 112)
        : (percent > 20 ? rgb(246, 204, 72) : rgb(232, 80, 84));
    if (filled > 0) canvas.fillRect((x + 2), (y + 2), (filled), (8), fillColor);
    canvas.drawRect((x), (y), (width), (12), rgb(0, 0, 0));
}

void drawBattleExperienceBar(Canvas565& canvas, int x, int y, int width,
                             uint8_t percent) {
    text(canvas, x, y - 10, "EXP", rgb(35, 118, 184));
    const int barX = x + 48;
    const int barWidth = std::max(1, width - 48);
    canvas.fillRect(barX, y, barWidth, 12, rgb(39, 45, 50));
    const int filled = (barWidth - 4) * percent / 100;
    if (filled > 0) {
        canvas.fillRect(barX + 2, y + 2, filled, 8,
                        rgb(35, 118, 184));
    }
    canvas.drawRect(barX, y, barWidth, 12, rgb(0, 0, 0));
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
    int x = static_cast<int>(std::lround((point.x - model.cameraX) * AmoledUi::RESOURCE_SCALE));
    int y = EXPLORE_ROUTE_MAP_TOP + static_cast<int>(std::lround(
        (point.y - model.cameraY) * AmoledUi::RESOURCE_SCALE)) +
        EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y - 12;
    if (x < -26 || x >= AmoledUi::WIDTH + 26 ||
        y < EXPLORE_ROUTE_MAP_TOP - 26 ||
        y >= EXPLORE_ROUTE_MAP_BOTTOM + 26) return;

    canvas.fillEllipse((x), (y + 12), (14), (4), rgb(55, 68, 59));
    if (GameAssets::drawCentered(
            GameAssets::Kind::EXPLORE_PICKUP_BALL, x, y - 8, 2.0f)) {
        return;
    }
    if (GameAssets::drawCentered(
            GameAssets::Kind::ITEM_POKE_BALL, x, y - 8, 1.24f)) {
        return;
    }
    canvas.fillCircle((x), (y - 8), (14), rgb(224, 69, 65));
    for (int row = 0; row <= 12; ++row) {
        int halfWidth = static_cast<int>(std::sqrt(
            196.0f - static_cast<float>(row * row)));
        canvas.drawFastHLine((x - halfWidth), (y - 8 + row), (halfWidth * 2 + 2), 0xFFFF);
    }
    canvas.drawCircle((x), (y - 8), (14), rgb(35, 39, 44));
    canvas.drawFastHLine((x - 14), (y - 8), (28), rgb(35, 39, 44));
    canvas.fillCircle((x), (y - 8), (4), 0xFFFF);
    canvas.drawCircle((x), (y - 8), (4), rgb(35, 39, 44));
}

void drawExploreRouteShadow(Canvas565& canvas,
                            const PokemonSprites::SpriteFrame* frame,
                            int centerX, int spriteTopY,
                            int drawWidth, int drawHeight,
                            float scale, bool flipX) {
    if (!frame || drawWidth <= 0 || drawHeight <= 0 || scale <= 0.0f) {
        return;
    }

    const int groundY = spriteTopY + drawHeight / 2 +
        static_cast<int>(std::lround(
            PokemonSprites::frameGroundOffsetY(frame) * scale));
    const auto shadow = PokemonShadow::atGround(frame, centerX, groundY, scale, flipX);
    const int radiusX = shadow.radiusX, radiusY = shadow.radiusY;
    const uint16_t shadowColor = rgb(27, 48, 48);

    // A low-opacity radial layer gives the edge a soft falloff. A smaller,
    // darker core keeps the contact point readable without a hard silhouette.
    fillSoftShadow(canvas, shadow.x, shadow.y,
                   radiusX, radiusY, shadowColor, 92);
    fillShadowCore(canvas, shadow.x, shadow.y,
                   std::max(8, radiusX * 2 / 3),
                   std::max(2, radiusY / 2), shadowColor, 58);
}

struct ExploreRouteBossWorldPose {
    ExploreRouteGeometry::WorldPoint point;
    float tangentX;
    float tangentY;
};

ExploreRouteBossWorldPose exploreRouteBossWorldPose(
    const ExploreMapGenerator::Path& path, uint8_t bossIndex,
    uint32_t animationNowMs) {
    ExploreRouteBossWorldPose pose{
        ExploreRouteGeometry::pathPoint(path, bossIndex), 0.0f, 1.0f};
    if (bossIndex + 1 < path.pointCount) {
        const ExploreRouteGeometry::WorldPoint next =
            ExploreRouteGeometry::pathPoint(path, bossIndex + 1);
        pose.tangentX = next.x - pose.point.x;
        pose.tangentY = next.y - pose.point.y;
    }
    const float tangentLength = std::hypot(pose.tangentX, pose.tangentY);
    if (tangentLength > 0.01f) {
        pose.tangentX /= tangentLength;
        pose.tangentY /= tangentLength;
    }
    const float patrolOffset = EXPLORE_ROUTE_BOSS_PATROL_DISTANCE *
        exploreRouteBossPatrolPermille(
            animationNowMs % EXPLORE_ROUTE_BOSS_PATROL_CYCLE_MS) / 1000.0f;
    pose.point.x += pose.tangentX * patrolOffset;
    pose.point.y += pose.tangentY * patrolOffset;
    return pose;
}

void drawExploreRouteBoss(Canvas565& canvas,
                          const ExploreRouteViewModel& model,
                          uint16_t rowBegin, uint16_t rowEnd) {
    if (!model.bossPending || model.bossSpeciesId == 0 || !model.map ||
        model.pathIndex >= model.map->pathCount) {
        return;
    }
    const ExploreMapGenerator::Path& path = model.map->paths[model.pathIndex];
    if (model.bossIndex >= path.pointCount) return;

    const ExploreRouteBossWorldPose pose = exploreRouteBossWorldPose(
        path, model.bossIndex, model.animationNowMs);
    const ExploreRouteGeometry::WorldPoint point = pose.point;

    const uint32_t patrolPhase =
        model.animationNowMs % EXPLORE_ROUTE_BOSS_PATROL_CYCLE_MS;
    const bool movingOut =
        patrolPhase >= EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS &&
        patrolPhase < EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS;
    const bool movingBack =
        patrolPhase >= EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS &&
        patrolPhase < EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS;
    const bool patrolMoving = movingOut || movingBack;
    const bool facesOutward =
        patrolPhase >= EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS &&
        patrolPhase < EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS;
    const PokemonSprites::WalkDirection direction =
        exploreRouteDirectionForDelta(
            facesOutward ? pose.tangentX : -pose.tangentX,
            facesOutward ? pose.tangentY : -pose.tangentY,
            PokemonSprites::WalkDirection::DOWN);

    PokemonSprites::WalkingAnimation animation{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    if (PokemonSprites::walkingAnimation(
            model.bossSpeciesId, direction,
            animation) && animation.frameCount > 0) {
        uint8_t frameIndex = 0;
        if (patrolMoving) {
            const uint32_t moveStarted = movingOut
                ? EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS
                : EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS;
            const uint16_t moveDuration = static_cast<uint16_t>(movingOut
                ? EXPLORE_ROUTE_BOSS_PATROL_OUT_END_MS -
                      EXPLORE_ROUTE_BOSS_PATROL_OUT_START_MS
                : EXPLORE_ROUTE_BOSS_PATROL_RETURN_END_MS -
                      EXPLORE_ROUTE_BOSS_PATROL_RETURN_START_MS);
            const uint32_t moveElapsed = patrolPhase - moveStarted;
            frameIndex = PokemonMotion::movementFrame(
                PokemonMotion::behaviorForSpecies(model.bossSpeciesId),
                animation.frameCount, moveElapsed, moveElapsed, moveDuration);
        }
        frame = PokemonSprites::findSpeciesSprite(
            model.bossSpeciesId,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(animation.base) + frameIndex));
        flipX = animation.flipX;
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            model.bossSpeciesId, PokemonSprites::SpriteKind::ICON_0);
    }
    if (!frame) return;

    constexpr float scale = EXPLORE_ROUTE_BOSS_SCALE;
    int width = std::max(1, static_cast<int>(std::lround(
        FlashStorage::readByte(&frame->width) * scale)));
    int height = std::max(1, static_cast<int>(std::lround(
        FlashStorage::readByte(&frame->height) * scale)));
    int centerX = static_cast<int>(std::lround((point.x - model.cameraX) * AmoledUi::RESOURCE_SCALE));
    int groundCenterY = EXPLORE_ROUTE_MAP_TOP +
        static_cast<int>(std::lround((point.y - model.cameraY) * AmoledUi::RESOURCE_SCALE)) +
        EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y;
    const int groundOffset = static_cast<int>(std::lround(
        PokemonSprites::frameGroundOffsetY(frame) * scale));
    const int spriteGroundTopY = groundCenterY - height / 2 - groundOffset;
    const int bodyTopY = spriteGroundTopY - static_cast<int>(std::lround(
        exploreRouteAirOffsetY(model.bossSpeciesId, model.animationNowMs,
                               scale)));
    int x = centerX - width / 2;
    int y = bodyTopY;
    if (x + width < -8 || x >= AmoledUi::WIDTH + 8 ||
        y + height < EXPLORE_ROUTE_MAP_TOP - 32 ||
        y >= EXPLORE_ROUTE_MAP_BOTTOM + 16) {
        return;
    }

    drawExploreRouteShadow(canvas, frame, centerX, spriteGroundTopY,
                           width, height, scale, flipX);
    PokemonSprites::drawFrameScaled(frame, x, y, scale, flipX);
    const auto shadow = PokemonShadow::atGround(frame, centerX, groundCenterY, scale, flipX);
    drawExploreRouteMapVegetationForActor(
        canvas, model, exploreRouteActorFootWorldY(point.y), std::min(x, shadow.x - shadow.radiusX),
        std::min(y, shadow.y - shadow.radiusY),
        std::max(x + width, shadow.x + shadow.radiusX + 1),
        std::max(y + height, shadow.y + shadow.radiusY + 1), rowBegin, rowEnd);
}

AmoledUi::Rect exploreActorBounds(uint16_t speciesId, float worldX, float worldY,
                                  uint8_t walkDirection, uint8_t petFrame,
                                  bool walking, uint32_t animationNowMs,
                                  int16_t cameraX, int16_t cameraY) {
    auto direction = static_cast<PokemonSprites::WalkDirection>(
        walkDirection <=
                static_cast<uint8_t>(PokemonSprites::WalkDirection::RIGHT)
            ? walkDirection
            : static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN));
    PokemonSprites::WalkingAnimation animation{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    if (PokemonSprites::walkingAnimation(
            speciesId, direction, animation) &&
        animation.frameCount > 0) {
        uint8_t frameIndex = walking
            ? static_cast<uint8_t>(petFrame % animation.frameCount)
            : 0;
        auto kind = static_cast<PokemonSprites::SpriteKind>(
            static_cast<uint16_t>(animation.base) + frameIndex);
        frame = PokemonSprites::findSpeciesSprite(speciesId, kind);
        flipX = animation.flipX;
    }

    constexpr float scale = AmoledUi::RESOURCE_SCALE;
    const int x = static_cast<int>(std::lround(worldX * scale)) - cameraX * AmoledUi::RESOURCE_SCALE;
    const int ground = EXPLORE_ROUTE_MAP_TOP + static_cast<int>(std::lround(worldY * scale)) -
        cameraY * AmoledUi::RESOURCE_SCALE + EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y;
    if (!frame) return {x - 63, ground - 155, 127, 169};
    const int width = static_cast<int>(std::lround(FlashStorage::readByte(&frame->width) * scale));
    const int height = static_cast<int>(std::lround(FlashStorage::readByte(&frame->height) * scale));
    const int offset = static_cast<int>(std::lround(PokemonSprites::frameGroundOffsetY(frame) * scale));
    const int lift = static_cast<int>(std::lround(exploreRouteAirOffsetY(speciesId, animationNowMs, scale)));
    const int drawY = ground - height / 2 - offset - lift;
    // Include the complete frame, the soft shadow and rounding at both edges.
    const auto shadow = PokemonShadow::atGround(frame, x, ground, scale, flipX);
    const int left = std::min(x - width / 2, shadow.x - shadow.radiusX) - 1;
    const int top = std::min(drawY, shadow.y - shadow.radiusY) - 1;
    const int right = std::max(x - width / 2 + width, shadow.x + shadow.radiusX + 1) + 1;
    const int bottom = std::max(drawY + height, shadow.y + shadow.radiusY + 1) + 1;
    return {left, top, right - left, bottom - top};
}

AmoledUi::Rect unionExploreBounds(AmoledUi::Rect a, AmoledUi::Rect b) {
    const int left = std::min(a.x, b.x), top = std::min(a.y, b.y);
    const int right = std::max(a.x + a.width, b.x + b.width);
    const int bottom = std::max(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

void drawExploreRouteActor(Canvas565& canvas, uint16_t speciesId,
                           float worldX, float worldY,
                           uint8_t walkDirection, uint8_t petFrame,
                           bool walking, uint32_t animationNowMs,
                           bool companion,
                           int16_t cameraX, int16_t cameraY,
                           const ExploreRouteViewModel& model,
                           uint16_t rowBegin, uint16_t rowEnd) {
    const bool airborne = ExploreEcology::locomotionForSpecies(speciesId) !=
        ExploreEcology::Locomotion::GROUND;
    auto direction = static_cast<PokemonSprites::WalkDirection>(
        walkDirection <=
                static_cast<uint8_t>(PokemonSprites::WalkDirection::RIGHT)
            ? walkDirection
            : static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN));
    PokemonSprites::WalkingAnimation animation{};
    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    if (PokemonSprites::walkingAnimation(
            speciesId, direction, animation) &&
        animation.frameCount > 0) {
        uint8_t frameIndex = walking
            ? static_cast<uint8_t>(petFrame % animation.frameCount)
            : 0;
        auto kind = static_cast<PokemonSprites::SpriteKind>(
            static_cast<uint16_t>(animation.base) + frameIndex);
        frame = PokemonSprites::findSpeciesSprite(speciesId, kind);
        flipX = animation.flipX;
    }

    constexpr float scale = static_cast<float>(AmoledUi::RESOURCE_SCALE);
    const int screenX = static_cast<int>(std::lround(
        worldX * AmoledUi::RESOURCE_SCALE)) - cameraX * AmoledUi::RESOURCE_SCALE;
    const int routeGroundY = EXPLORE_ROUTE_MAP_TOP +
        static_cast<int>(std::lround(worldY * AmoledUi::RESOURCE_SCALE)) -
        cameraY * AmoledUi::RESOURCE_SCALE + EXPLORE_ROUTE_ACTOR_GROUND_OFFSET_Y;
    if (!frame) {
#if STICKMON_ENABLE_DEBUG_FEATURES && STICKMON_ENABLE_TRACE_LOGS
        Platform::logf(
            "[AmoledExploreDiag] draw missing species=%u dir=%u walking=%u "
            "world=(%.2f,%.2f) camera=(%d,%d) screenCenter=(%d,%d)\n",
            static_cast<unsigned>(speciesId),
            static_cast<unsigned>(walkDirection), walking ? 1U : 0U,
            static_cast<double>(worldX), static_cast<double>(worldY),
            static_cast<int>(cameraX), static_cast<int>(cameraY), screenX,
            routeGroundY);
#endif
        // The fallback art is authored around a ground anchor. Keep its feet
        // on the same shifted ground anchor as regular walking frames.
        drawFallbackPet(canvas, screenX, routeGroundY);
        drawExploreRouteMapVegetationForActor(
            canvas, model, exploreRouteActorFootWorldY(worldY), screenX - 62, routeGroundY - 154,
            screenX + 63, routeGroundY + 13, rowBegin, rowEnd, !airborne, airborne);
        return;
    }
    const int width = static_cast<int>(std::lround(
        FlashStorage::readByte(&frame->width) * scale));
    const int height = static_cast<int>(std::lround(
        FlashStorage::readByte(&frame->height) * scale));
    // Align the feet with the pickup shadow's ground anchor below the route
    // centerline. The encoded offset describes how far the frame's
    // ground is below its pixel center; floating species lift only the body.
    const int groundOffset = static_cast<int>(std::lround(
        PokemonSprites::frameGroundOffsetY(frame) * scale));
    const int spriteGroundTopY = routeGroundY - height / 2 - groundOffset;
    const int bodyLift = static_cast<int>(std::lround(
        exploreRouteAirOffsetY(speciesId, animationNowMs, scale)));
    const int drawY = spriteGroundTopY - bodyLift;
#if STICKMON_ENABLE_DEBUG_FEATURES && STICKMON_ENABLE_TRACE_LOGS
    struct RouteActorLogState {
        uint32_t lastMs = 0;
        uint8_t direction = 0xFF;
        bool walking = false;
    };
    static RouteActorLogState leaderLog;
    static RouteActorLogState companionLog;
    RouteActorLogState& logState = companion ? companionLog : leaderLog;
    // The renderer is called for partial dirty bands as well as full frames.
    // Log state changes immediately, then sample positions every 500ms.
    const bool stateChanged = logState.direction != walkDirection ||
                              logState.walking != walking;
    if (stateChanged || logState.lastMs == 0 ||
        animationNowMs - logState.lastMs >= 500U) {
        const char* directionName = "other";
        if (walkDirection == static_cast<uint8_t>(
                PokemonSprites::WalkDirection::LEFT)) {
            directionName = "left";
        } else if (walkDirection == static_cast<uint8_t>(
                         PokemonSprites::WalkDirection::RIGHT)) {
            directionName = "right";
        } else if (walkDirection == static_cast<uint8_t>(
                         PokemonSprites::WalkDirection::UP)) {
            directionName = "up";
        } else if (walkDirection == static_cast<uint8_t>(
                         PokemonSprites::WalkDirection::DOWN)) {
            directionName = "down";
        }
        const int frameKind = static_cast<int>(
            static_cast<uint16_t>(animation.base) +
            (walking ? petFrame % animation.frameCount : 0));
        const int frameGroundOffset = PokemonSprites::frameGroundOffsetY(frame);
        Platform::logf(
            "[AmoledExploreDiag] draw role=%s species=%u dir=%u(%s) "
            "walking=%u frame=%d size=%dx%d groundOffsetRaw=%d "
            "groundOffsetPx=%d "
            "world=(%.2f,%.2f) camera=(%d,%d) ground=(%d,%d) "
            "bodyLift=%d groundTop=%d shadow=(%d,%d) "
            "rect=(%d,%d,%d,%d)\n",
            companion ? "companion" : "leader",
            static_cast<unsigned>(speciesId), static_cast<unsigned>(walkDirection),
            directionName, walking ? 1U : 0U, frameKind, width, height,
            frameGroundOffset, groundOffset, static_cast<double>(worldX),
            static_cast<double>(worldY), static_cast<int>(cameraX),
            static_cast<int>(cameraY), screenX, routeGroundY, bodyLift,
            spriteGroundTopY, screenX, routeGroundY - 1,
            screenX - width / 2, drawY, width, height);
        logState.lastMs = animationNowMs;
        logState.direction = walkDirection;
        logState.walking = walking;
    }
#endif
    drawExploreRouteShadow(canvas, frame, screenX,
                           spriteGroundTopY, width, height, scale, flipX);
    const bool spriteDrawn = PokemonSprites::drawFrameScaled(
        frame, screenX - width / 2, drawY, scale, flipX);
    if (!spriteDrawn) {
        drawFallbackPet(canvas, screenX, routeGroundY);
    }
    const auto shadow = PokemonShadow::atGround(frame, screenX, routeGroundY, scale, flipX);
    drawExploreRouteMapVegetationForActor(
        canvas, model, exploreRouteActorFootWorldY(worldY),
        std::min(screenX - width / 2, spriteDrawn ? shadow.x - shadow.radiusX : screenX - 62),
        std::min(drawY, spriteDrawn ? shadow.y - shadow.radiusY : routeGroundY - 154),
        std::max(screenX - width / 2 + width,
                 spriteDrawn ? shadow.x + shadow.radiusX + 1 : screenX + 63),
        std::max(drawY + height, spriteDrawn ? shadow.y + shadow.radiusY + 1 : routeGroundY + 13),
        rowBegin, rowEnd, !airborne, airborne);
}

void drawExploreRoutePet(Canvas565& canvas,
                         const ExploreRouteViewModel& model,
                         uint16_t rowBegin, uint16_t rowEnd) {
    drawExploreRouteActor(canvas, model.speciesId, model.worldX, model.worldY,
                          model.walkDirection, model.petFrame, model.walking,
                          model.animationNowMs, false,
                          model.cameraX, model.cameraY, model, rowBegin, rowEnd);
}

void drawExploreRouteCompanion(Canvas565& canvas,
                               const ExploreRouteViewModel& model,
                               uint16_t rowBegin, uint16_t rowEnd) {
    if (!model.companionVisible || model.companionSpeciesId == 0) return;
    drawExploreRouteActor(
        canvas, model.companionSpeciesId, model.companionWorldX,
        model.companionWorldY, model.companionWalkDirection,
        model.companionFrame, model.companionWalking, model.animationNowMs,
        true,
        model.cameraX, model.cameraY, model, rowBegin, rowEnd);
}

constexpr float WILDLIFE_SCALE = 1.5f;

const PokemonSprites::SpriteFrame* wildlifeFrame(const ExploreEcology::Pose& pose,
                                                  bool& flipX) {
    PokemonSprites::PetAnimationProfile profile{};
    if (PokemonSprites::petAnimationProfile(pose.speciesId, profile)) {
        const bool sleeping = pose.activity == ExploreEcology::Activity::DOZE ||
            (pose.cover == ExploreEcology::Cover::GRASS &&
             (pose.activity == ExploreEcology::Activity::HIDE ||
              pose.activity == ExploreEcology::Activity::BURROW));
        const bool flapping = pose.airborne || pose.activity == ExploreEcology::Activity::TAKEOFF ||
                              pose.activity == ExploreEcology::Activity::LAND;
        if (sleeping || (!pose.walking && !flapping)) {
            constexpr uint8_t directions[] = {0, 2, 4, 6};
            uint8_t direction = directions[pose.direction % 4];
            flipX = !sleeping && direction == 6 && profile.mirrorRightDirections;
            if (flipX) direction = 2;
            const uint8_t count = sleeping ? profile.sleepingFrames : profile.idleFrames;
            if (count) {
                const auto kind = static_cast<PokemonSprites::SpriteKind>(
                    static_cast<uint16_t>(sleeping ? profile.sleepingBase : profile.idleBase) +
                    (sleeping ? 0 : direction * count) + pose.frame % count);
                if (const auto* frame = PokemonSprites::findCachedSpeciesSprite(pose.speciesId, kind))
                    return frame;
            }
        }
    }
    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(pose.speciesId,
            static_cast<PokemonSprites::WalkDirection>(pose.direction % 4), animation) &&
        animation.frameCount) {
        flipX = animation.flipX;
        const auto kind = static_cast<PokemonSprites::SpriteKind>(
            static_cast<uint16_t>(animation.base) +
            ((pose.airborne || pose.walking || pose.activity == ExploreEcology::Activity::TAKEOFF ||
              pose.activity == ExploreEcology::Activity::LAND) ? pose.frame % animation.frameCount : 0));
        if (const auto* frame = PokemonSprites::findCachedSpeciesSprite(pose.speciesId, kind))
            return frame;
    }
    flipX = false;
    return PokemonSprites::findCachedSpeciesSprite(
        pose.speciesId, PokemonSprites::SpriteKind::ICON_0);
}

AmoledUi::Rect wildlifeBounds(const ExploreRouteViewModel& model, uint8_t index) {
    if (index >= model.wildlifeCount || index >= ExploreEcology::CAPACITY) return {};
    const auto& pose = model.wildlife[index];
    if (pose.absent) return {};
    const int x = int(std::lround((pose.x - model.cameraX) * AmoledUi::RESOURCE_SCALE));
    const int ground = int(std::lround((pose.y - model.cameraY) * AmoledUi::RESOURCE_SCALE));
    if (pose.hidden) return {x - 16, ground - 17, 33, 24};
    bool flipX = false;
    const auto* frame = wildlifeFrame(pose, flipX);
    if (!frame) return {x, ground, 0, 0};
    const int width = int(std::lround(FlashStorage::readByte(&frame->width) * WILDLIFE_SCALE));
    const int height = int(std::lround(FlashStorage::readByte(&frame->height) * WILDLIFE_SCALE));
    const int offset = int(std::lround(PokemonSprites::frameGroundOffsetY(frame) * WILDLIFE_SCALE));
    const int bodyX = x + pose.sway * AmoledUi::RESOURCE_SCALE;
    const int bodyTop = ground - height / 2 - offset - pose.lift * AmoledUi::RESOURCE_SCALE;
    const int bodyBottom = bodyTop + height;
    const auto shadow = PokemonShadow::atGround(frame, bodyX, ground, WILDLIFE_SCALE, flipX);
    const int left = std::min({bodyX - width / 2, x - 26, shadow.x - shadow.radiusX}) - 1;
    const int top = std::min({bodyTop - 14, ground - 19, shadow.y - shadow.radiusY}) - 1;
    const int right = std::max({bodyX - width / 2 + width, x + 27, shadow.x + shadow.radiusX + 1}) + 1;
    const int bottom = std::max({bodyBottom, ground + 7, shadow.y + shadow.radiusY + 1}) + 1;
    return {left, top, right - left, bottom - top};
}

void drawWildlifeRipple(Canvas565& canvas, int x, int y, int radius,
                        uint16_t color) {
    constexpr int segments = 20;
    int previousX = x + radius, previousY = y;
    for (int i = 1; i <= segments; ++i) {
        const float angle = i * 6.2831853f / segments;
        const int nextX = x + int(std::lround(std::cos(angle) * radius));
        const int nextY = y + int(std::lround(std::sin(angle) * 4));
        canvas.drawLine(previousX, previousY, nextX, nextY, color);
        previousX = nextX;
        previousY = nextY;
    }
}

void drawWildlifeHabitatHint(Canvas565& canvas, const ExploreEcology::Pose& pose,
                             int x, int ground) {
    const uint8_t phase = pose.frame % 3;
    if (pose.surface == ExploreEcology::Surface::WATER) {
        drawWildlifeRipple(canvas, x, ground, 10 + phase * 3, rgb(171, 217, 225));
        if (phase) canvas.fillCircle(x + 3, ground - 7, 2, rgb(195, 233, 239));
    } else if (pose.surface == ExploreEcology::Surface::ROCK ||
               pose.surface == ExploreEcology::Surface::ICE) {
        const bool ice = pose.surface == ExploreEcology::Surface::ICE;
        const uint16_t color = ice ? rgb(183, 219, 231) : rgb(146, 137, 125);
        const int spread = phase * 3;
        canvas.fillEllipse(x, ground + 2, 10 + spread, 2, color);
        if (phase) {
            canvas.fillCircle(x - 5 - spread, ground - 4, 2, color);
            canvas.fillCircle(x + 5 + spread, ground - 7, 2, color);
        }
    } else {
        const int bend = phase == 1 ? -3 : (phase == 2 ? 3 : 0);
        for (int blade = -1; blade <= 1; ++blade) {
            canvas.drawLine(x + blade * 6, ground + 2,
                x + blade * 9 + bend, ground - 10 - (blade == 0 ? 4 : 0),
                rgb(68, 126, 75));
            canvas.drawLine(x + blade * 6 + 1, ground + 2,
                x + blade * 9 + bend + 1, ground - 9 - (blade == 0 ? 4 : 0),
                rgb(110, 163, 85));
        }
    }
}

void drawExploreWildlife(Canvas565& canvas, const ExploreRouteViewModel& model,
                         uint8_t index, uint16_t rowBegin, uint16_t rowEnd) {
    const auto bounds = wildlifeBounds(model, index);
    if (bounds.width == 0 || bounds.x + bounds.width <= 0 || bounds.x >= AmoledUi::WIDTH ||
        bounds.y + bounds.height <= rowBegin || bounds.y >= rowEnd) return;
    const auto& pose = model.wildlife[index];
    const int x = int(std::lround((pose.x - model.cameraX) * AmoledUi::RESOURCE_SCALE));
    const int ground = int(std::lround((pose.y - model.cameraY) * AmoledUi::RESOURCE_SCALE));
    if (pose.hidden) {
        drawWildlifeHabitatHint(canvas, pose, x, ground);
    } else {
        bool flipX = false;
        const auto* frame = wildlifeFrame(pose, flipX);
        if (!frame) return;
        const int width = int(std::lround(FlashStorage::readByte(&frame->width) * WILDLIFE_SCALE));
        const int height = int(std::lround(FlashStorage::readByte(&frame->height) * WILDLIFE_SCALE));
        const int baseTop = ground - height / 2 - int(std::lround(
            PokemonSprites::frameGroundOffsetY(frame) * WILDLIFE_SCALE));
        const int top = baseTop - pose.lift * AmoledUi::RESOURCE_SCALE;
        const int bodyX = x + pose.sway * AmoledUi::RESOURCE_SCALE;
        if (pose.airborne) {
            const auto shadow = PokemonShadow::atGround(
                frame, bodyX, ground, WILDLIFE_SCALE, flipX);
            fillSoftShadow(canvas, shadow.x, shadow.y, shadow.radiusX,
                           shadow.radiusY, rgb(62, 83, 83), 68);
        }
        else if (pose.surface == ExploreEcology::Surface::WATER)
            drawWildlifeRipple(canvas, x, ground, 18, rgb(144, 201, 217));
        else drawExploreRouteShadow(canvas, frame, bodyX, baseTop, width, height, WILDLIFE_SCALE, flipX);
        PokemonSprites::drawFrameScaled(frame, bodyX - width / 2, top, WILDLIFE_SCALE, flipX);
        if (pose.activity == ExploreEcology::Activity::DOZE) {
            // A small pixel Z moves with the breathing pose.
            const int zx = bodyX + 8, zy = top - 9;
            const uint16_t color = rgb(208, 226, 231);
            canvas.drawFastHLine(zx, zy, 6, color);
            canvas.drawLine(zx + 5, zy, zx, zy + 5, color);
            canvas.drawFastHLine(zx, zy + 5, 6, color);
        }
        if (pose.activity == ExploreEcology::Activity::SPLASH ||
            pose.activity == ExploreEcology::Activity::GATHER) {
            drawWildlifeRipple(canvas, x, ground, 18 + pose.frame % 3 * 3, rgb(185, 224, 233));
            if (pose.activity == ExploreEcology::Activity::SPLASH) {
                const int spread = 12 + pose.frame % 3 * 3;
                canvas.fillCircle(x - spread, ground - 7, 2, rgb(194, 231, 239));
                canvas.fillCircle(x + spread, ground - 11, 2, rgb(194, 231, 239));
            }
        }
    }
    drawExploreRouteMapVegetationForActor(canvas, model, pose.y,
        bounds.x, bounds.y, bounds.x + bounds.width, bounds.y + bounds.height,
        rowBegin, rowEnd, !pose.airborne, pose.airborne,
        pose.surface == ExploreEcology::Surface::WATER);
}

void drawExploreRouteParkActors(Canvas565& canvas,
                                const ExploreRouteViewModel& model,
                                uint16_t rowBegin, uint16_t rowEnd) {
    ExploreTileSemantics::ActorDepth actors[3 + ExploreEcology::CAPACITY] = {
        {ExploreTileSemantics::ActorRole::LEADER, exploreRouteActorFootWorldY(model.worldY)}};
    uint8_t actorCount = 1;
    if (model.companionVisible && model.companionSpeciesId != 0) {
        actors[actorCount++] = {
            ExploreTileSemantics::ActorRole::COMPANION,
            exploreRouteActorFootWorldY(model.companionWorldY)};
    }
    if (model.bossPending && model.bossSpeciesId != 0 && model.map &&
        model.pathIndex < model.map->pathCount) {
        const ExploreMapGenerator::Path& path = model.map->paths[model.pathIndex];
        if (model.bossIndex < path.pointCount) {
            actors[actorCount++] = {ExploreTileSemantics::ActorRole::BOSS,
                exploreRouteActorFootWorldY(exploreRouteBossWorldPose(
                    path, model.bossIndex, model.animationNowMs).point.y)};
        }
    }
    for (uint8_t i = 0; i < model.wildlifeCount && i < ExploreEcology::CAPACITY; ++i) {
        actors[actorCount++] = {
            ExploreTileSemantics::ActorRole::WILDLIFE, model.wildlife[i].y, i,
            model.wildlife[i].surface == ExploreEcology::Surface::WATER &&
                !model.wildlife[i].airborne};
    }
    ExploreTileSemantics::sortActorsByGroundDepth(actors, actorCount);
    for (uint8_t index = 0; index < actorCount; ++index) {
        switch (actors[index].role) {
        case ExploreTileSemantics::ActorRole::WILDLIFE:
            drawExploreWildlife(canvas, model, actors[index].index, rowBegin, rowEnd);
            break;
        case ExploreTileSemantics::ActorRole::BOSS:
            drawExploreRouteBoss(canvas, model, rowBegin, rowEnd);
            break;
        case ExploreTileSemantics::ActorRole::COMPANION:
            drawExploreRouteCompanion(canvas, model, rowBegin, rowEnd);
            break;
        case ExploreTileSemantics::ActorRole::LEADER:
            drawExploreRoutePet(canvas, model, rowBegin, rowEnd);
            break;
        }
    }
}

bool exploreRouteBossBehindPlayer(const ExploreRouteViewModel& model) {
    if (!model.bossPending || !model.map ||
        model.pathIndex >= model.map->pathCount) {
        return false;
    }
    const ExploreMapGenerator::Path& path = model.map->paths[model.pathIndex];
    if (model.bossIndex >= path.pointCount) return false;
    return ExploreRouteGeometry::pathPoint(path, model.bossIndex).y <=
           model.worldY;
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
        std::min(static_cast<float>(EXPLORE_PREVIEW_NATIVE_MAX_WIDTH) / width,
                 static_cast<float>(EXPLORE_PREVIEW_NATIVE_MAX_HEIGHT) / height));
    int drawnWidth = std::max(1, static_cast<int>(std::lround(width * scale)));
    int drawnHeight = std::max(1, static_cast<int>(std::lround(height * scale)));
    int x = centerX - drawnWidth / 2;
    int y = centerY - drawnHeight / 2;
    // PokemonSprites draws through PixelRenderer's bound canvas rather than
    // the page canvas reference, so switch the actual sprite target here.
    Canvas565& spriteCanvas = PixelRenderer::canvas();
    const uint8_t previousAssetScale = spriteCanvas.assetScale();
    spriteCanvas.setAssetScale(1);
    if (hidden) {
        PokemonSprites::drawFrameSilhouette(
            frame, x, y, rgb(5, 10, 14));
    } else {
        PokemonSprites::drawFrameScaled(frame, x, y, scale, false);
    }
    spriteCanvas.setAssetScale(previousAssetScale);
}

void drawExplorePreviewSlot(Canvas565& canvas,
                            const ExploreViewModel& model, uint8_t index,
                            int centerX) {
    if (index >= model.previewPool.count) return;
    drawExplorePreviewMember(canvas, model.previewFrames[index], centerX,
                             (EXPLORE_PREVIEW_CENTER_Y),
                             model.previewHidden[index]);
}

}  // namespace

AmoledUi::Rect exploreRouteWildlifeBounds(const ExploreRouteViewModel& model, uint8_t index) {
    return wildlifeBounds(model, index);
}

AmoledUi::Rect exploreRouteActorBounds(const ExploreRouteViewModel& model) {
    auto bounds = exploreActorBounds(model.speciesId, model.worldX, model.worldY,
        model.walkDirection, model.petFrame, model.walking, model.animationNowMs,
        model.cameraX, model.cameraY);
    if (model.companionVisible && model.companionSpeciesId != 0) {
        bounds = unionExploreBounds(bounds, exploreActorBounds(
            model.companionSpeciesId, model.companionWorldX, model.companionWorldY,
            model.companionWalkDirection, model.companionFrame, model.companionWalking,
            model.animationNowMs, model.cameraX, model.cameraY));
    }
    return bounds;
}

AmoledUi::Rect homePetVisibleBounds(const HomeViewModel& model) {
    return homePetLayout(model).bounds;
}

void drawHatchHeart(Canvas565& canvas, int x, int y) {
    drawHeart(canvas, x, y, rgb(239, 103, 113), true);
}

void renderHomeScreen(Canvas565& canvas, const HomeViewModel& model,
                      uint16_t rowBegin, uint16_t rowEnd) {
    renderHomeScreenRegion(canvas, model, rowBegin, rowEnd, 0, AmoledUi::WIDTH);
}

void renderHomeScreenRegion(Canvas565& canvas, const HomeViewModel& model,
                            uint16_t rowBegin, uint16_t rowEnd,
                            uint16_t columnBegin, uint16_t columnEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd, columnBegin, columnEnd);
    if (rowBegin >= rowEnd) return;

    if (rowBegin < HOME_HEADER_HEIGHT) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, HOME_HEADER_HEIGHT);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        UiCommon::drawHeader(canvas, HEADER_HEIGHT);
        constexpr int MOOD_HEART_START_X = 20;
        constexpr int MOOD_HEART_GAP = 28;
        constexpr int headerCenterY = HOME_HEADER_HEIGHT / 2;
        // The heart spans y - 4 through y + 6 around its drawing anchor.
        constexpr int heartAnchorY = headerCenterY - 2;
        const int clockHeight = canvas.nativeText() ? 32 : 14;
        for (uint8_t index = 0; index < model.moodHearts; ++index) {
            drawHeart(canvas, MOOD_HEART_START_X + index * MOOD_HEART_GAP,
                      heartAnchorY, rgb(239, 103, 113), true);
        }
        if (model.moodBurstHeart < 5 && model.moodBurstAgeMs > 0) {
            drawHeartBurst(
                canvas,
                MOOD_HEART_START_X + model.moodBurstHeart * MOOD_HEART_GAP,
                headerCenterY, model.moodBurstAgeMs);
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
        text(canvas, 232, (HOME_HEADER_HEIGHT - clockHeight) / 2,
             clockText, ink);
        HudRenderer::drawBattery(canvas, 320, headerCenterY - 16 / 2, model.batteryPercent);
        pageClip.reset();
    }

    if (rowBegin < HOME_STATUS_TOP && rowEnd > HOME_ROOM_TOP) {
        int top = std::max<int>(rowBegin, HOME_ROOM_TOP);
        int bottom = std::min<int>(rowEnd, HOME_STATUS_TOP);
        pageClip.setRect((0), (top), (HOME_ROOM_WIDTH), (bottom - top));
        bool roomDrawn = RoomRenderer::drawViewport(
            0, HOME_ROOM_TOP, AmoledUi::WIDTH, HOME_ROOM_HEIGHT,
            model.cameraX, model.cameraY, model.night,
            AmoledUi::RESOURCE_SCALE);
        if (!roomDrawn) {
            canvas.fillRect((0), (HEADER_HEIGHT), (AmoledUi::WIDTH), (200), rgb(218, 204, 173));
            canvas.fillRect((0), (248), (AmoledUi::WIDTH), (96), rgb(122, 86, 68));
            for (int y = 254; y < 344; y += 16) {
                canvas.drawFastHLine((0), (y), (AmoledUi::WIDTH), rgb(105, 72, 58));
            }
            for (int x = 16; x < AmoledUi::WIDTH; x += 48) {
                canvas.drawFastVLine((x), (248), (96), rgb(112, 77, 61));
            }

            canvas.fillRoundRect((34), (72), (110), (94), (6), rgb(67, 74, 75));
            canvas.fillRect((42), (80), (94), (78), rgb(93, 174, 196));
            canvas.fillCircle((116), (100), (14), rgb(246, 213, 116));
            canvas.fillRect((42), (136), (94), (22), rgb(101, 153, 119));
            canvas.drawFastVLine((88), (80), (78), rgb(67, 74, 75));
            canvas.drawFastHLine((42), (118), (94), rgb(67, 74, 75));

            canvas.fillRoundRect((264), (88), (74), (124), (6), rgb(92, 69, 59));
            canvas.fillRect((272), (100), (58), (8), rgb(184, 133, 81));
            canvas.fillRect((272), (142), (58), (8), rgb(184, 133, 81));
            canvas.fillRect((278), (118), (12), (24), rgb(87, 140, 157));
            canvas.fillRect((294), (114), (14), (28), rgb(205, 104, 83));
            canvas.fillRect((312), (122), (12), (20), rgb(220, 183, 91));
            canvas.fillRoundRect((280), (166), (42), (36), (6), rgb(65, 113, 105));
            drawPlant(canvas);
            canvas.fillEllipse((184), (302), (106), (34), rgb(49, 112, 111));
            canvas.fillEllipse((184), (298), (90), (24), rgb(71, 151, 139));
        }
        // Food is part of the room surface. Draw it after the room art but
        // before every actor so a pet standing over the bowl occludes it.
        if (roomDrawn) {
            if (model.bowlFilled) {
                drawFoodContent(canvas, model.bowlCenterX, model.bowlCenterY);
            }
        } else {
            drawBowl(canvas, model.bowlFilled);
        }
        if (model.companionVisible &&
            model.companionGroundY < model.petGroundY) {
            drawHomeCompanion(canvas, model);
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugPairChaseActive &&
            model.debugPairGroundY < model.petGroundY) {
            drawDebugPairChaser(canvas, model);
        }
#endif
        AmoledUi::Rect petBounds{};
        drawPet(canvas, model, model.showHearts ? &petBounds : nullptr);
        if (model.companionVisible &&
            model.companionGroundY >= model.petGroundY) {
            drawHomeCompanion(canvas, model);
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugPairChaseActive &&
            model.debugPairGroundY >= model.petGroundY) {
            drawDebugPairChaser(canvas, model);
        }
        drawDebugContactGuest(canvas, model);
#endif
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugLightSource != 0) {
            drawDebugLight(canvas, model);
        }
        drawDebugWalkBoundary(canvas, model);
        drawDebugTalkPoints(canvas, model);
        if (model.debugContactPromptFade > 0) {
            drawDebugContactPrompt(canvas, model);
        }
#endif
        if (model.showHearts && model.petVisible && petBounds.height > 0) {
            // Keep both hearts close to the visible head, including transparent
            // canvas padding, mirrored frames and body-only hop offsets.
            const int centerX = std::clamp(petBounds.x + petBounds.width / 2, 28,
                                           AmoledUi::WIDTH - 28);
            const int heartY = std::max(HOME_ROOM_TOP + 22, petBounds.y - 20);
            drawHeart(canvas, centerX - 14, heartY, rgb(239, 103, 113), true);
            drawHeart(canvas, centerX + 14, heartY - 6, rgb(239, 103, 113), true);
        }
        pageClip.reset();
    }

    if (rowEnd > HOME_STATUS_TOP) {
        int top = std::max<int>(rowBegin, HOME_STATUS_TOP);
        int bottom = rowEnd;
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        canvas.fillRect((0), (344), (AmoledUi::WIDTH), (104), rgb(18, 27, 35));
        canvas.drawFastHLine((0), (344), (AmoledUi::WIDTH), rgb(71, 108, 108));
        if (model.toast && *model.toast) {
            drawHomeHudMessage(canvas, model.toast);
        } else {
            drawHomeLockIcon(canvas, !model.hatching);
            if (model.visitAway) drawHomeRecallIcon(canvas);
            else drawHomeMenuIcon(canvas);

            canvas.fillRoundRect((HOME_MONSTER_PANEL_X), (352), (HOME_MONSTER_PANEL_W), (88), (8), rgb(24, 34, 42));
            canvas.drawRoundRect((HOME_MONSTER_PANEL_X), (352), (HOME_MONSTER_PANEL_W), (88), (8), rgb(72, 83, 98));
            if (model.visitAway) {
                text(canvas, HOME_MONSTER_PANEL_X + 2, 376,
                     Ui::Social::VISIT_AWAY_HUD, rgb(226, 238, 233));
            } else if (model.hatching) {
                HudRenderer::drawHungerIcon(canvas, HOME_MONSTER_PANEL_X + 14,
                                            360, 0, AmoledUi::RESOURCE_SCALE);
                char countdown[12];
                std::snprintf(countdown, sizeof(countdown), "%02u:%02u",
                    model.hatchRemainingSeconds / 60, model.hatchRemainingSeconds % 60);
                const int height = canvas.nativeText() ? 32 : 14;
                text(canvas, HOME_MONSTER_PANEL_X + 58 +
                             (90 - textWidth(countdown)) / 2,
                     374 - height / 2, countdown, rgb(115, 226, 183));
                text(canvas, HOME_MONSTER_PANEL_X +
                             (HOME_MONSTER_PANEL_W - textWidth(Ui::Hatch::TITLE)) / 2,
                     400, Ui::Hatch::TITLE, rgb(151, 168, 166));
            } else {
                uint8_t count = std::min<uint8_t>(model.monsterCount,
                                                  Game::TEAM_CAP);
                for (uint8_t index = 0; index < count; ++index) {
                    int rowY = 354 + index * 42;
                    HudRenderer::drawHungerIcon(
                        canvas, HOME_MONSTER_PANEL_X + 14, rowY + 6,
                        model.monsters[index].hunger, AmoledUi::RESOURCE_SCALE);
                    if (model.monsters[index].hpKnown) {
                        drawHomeHpBar(canvas, HOME_MONSTER_PANEL_X + 58,
                                      rowY + 14, 90,
                                      model.monsters[index].fainted
                                          ? model.monsters[index].faintRest
                                          : model.monsters[index].hp,
                                      model.monsters[index].fainted);
                    } else {
                        text(canvas, HOME_MONSTER_PANEL_X + 72, rowY + 5,
                             "HP --", rgb(151, 168, 166));
                    }
                }
            }
        }
#if STICKMON_ENABLE_DEBUG_FEATURES
        if (model.debugContactPromptFade > 0) {
            // The visitor prompt owns the bottom strip: fade the HUD widgets
            // into the strip background and show the visitor message there.
            const uint16_t strip = rgb(18, 27, 35);
            PixelRenderer::fillRectAlpha(0, 344, AmoledUi::WIDTH, 104,
                                         strip, model.debugContactPromptFade);
            if (model.debugContactPromptText) {
                const uint16_t ink = blendShadowRgb565(
                    strip, rgb(226, 238, 233), model.debugContactPromptFade);
                const char* message = model.debugContactPromptText;
                const char* newline = std::strchr(message, '\n');
                if (!newline) {
                    text(canvas,
                         (AmoledUi::WIDTH - textWidth(message)) / 2,
                         384, message, ink);
                } else {
                    // Two-line prompt (32px glyphs): centered as a 68px block
                    // inside the 344..448 strip.
                    char firstLine[28];
                    const size_t firstLength = std::min<size_t>(
                        static_cast<size_t>(newline - message),
                        sizeof(firstLine) - 1);
                    std::memcpy(firstLine, message, firstLength);
                    firstLine[firstLength] = '\0';
                    const char* secondLine = newline + 1;
                    text(canvas,
                         (AmoledUi::WIDTH - textWidth(firstLine)) / 2,
                         362, firstLine, ink);
                    text(canvas,
                         (AmoledUi::WIDTH - textWidth(secondLine)) / 2,
                         398, secondLine, ink);
                }
            }
        }
#endif
        pageClip.reset();
    }
    if (model.recallConfirm && rowBegin < HOME_STATUS_TOP &&
        rowEnd > HOME_ROOM_TOP) {
        const int top = std::max<int>(rowBegin, HOME_ROOM_TOP);
        const int bottom = std::min<int>(rowEnd, HOME_STATUS_TOP);
        pageClip.setRect(0, top, AmoledUi::WIDTH, bottom - top);
        canvas.fillRoundRect(34, 150, 300, 148, 10, rgb(20, 31, 38));
        canvas.drawRoundRect(34, 150, 300, 148, 10, rgb(112, 151, 151));
        text(canvas, (AmoledUi::WIDTH - textWidth(Ui::Social::RECALL_CONFIRM)) / 2,
             174, Ui::Social::RECALL_CONFIRM, rgb(226, 238, 233));
        canvas.fillRoundRect(54, 226, 116, 56, 8, rgb(46, 106, 92));
        canvas.fillRoundRect(198, 226, 116, 56, 8, rgb(47, 65, 73));
        text(canvas, 80, 238, Ui::Social::RECALL, rgb(226, 238, 233));
        text(canvas, 224, 238, Ui::Social::RECALL_CANCEL,
             rgb(226, 238, 233));
        pageClip.reset();
    }
    drawSceneFadeOverlay(canvas, model.fadeAlpha, rowBegin, rowEnd);
}

int recallConfirmChoiceAt(int x, int y) {
    if (y < 226 || y >= 282) return -1;
    if (x >= 54 && x < 170) return 0;
    if (x >= 198 && x < 314) return 1;
    return -1;
}

HomeHitTarget homeHitTargetAt(int x, int y, int petCenterX,
                              int petGroundY, int bowlCenterX,
                              int bowlCenterY) {
    if (y >= HOME_STATUS_TOP && y < 448) {
        if (x >= HOME_MENU_BUTTON_X - 8 &&
            x < HOME_MENU_BUTTON_X + HOME_HUD_BUTTON_SIZE + 8) {
            return HomeHitTarget::MENU;
        }
        if (x >= HOME_LOCK_BUTTON_X - 8 &&
            x < HOME_LOCK_BUTTON_X + HOME_HUD_BUTTON_SIZE + 8) {
            return HomeHitTarget::LOCK;
        }
    }
    if (x >= bowlCenterX - 40 && x < bowlCenterX + 40 &&
        y >= bowlCenterY - 36 && y < bowlCenterY + 36) {
        return HomeHitTarget::BOWL;
    }
    if (x >= petCenterX - 76 && x < petCenterX + 76 &&
        y >= petGroundY - 164 && y < petGroundY + 10) {
        return HomeHitTarget::PET;
    }
    return HomeHitTarget::NONE;
}

float mainMenuMaxScroll() {
    return MenuGrid::mainLayout(AmoledUi::WIDTH, AmoledUi::HEIGHT, 2).maxScroll(MAIN_MENU_ITEM_COUNT);
}

bool mainMenuBackAt(int, int) {
    return false;
}

int mainMenuItemAt(int x, int y, float scroll) {
    return MenuGrid::mainLayout(AmoledUi::WIDTH, AmoledUi::HEIGHT, 2).itemAt(x, y, scroll, MAIN_MENU_ITEM_COUNT);
}

#if STICKMON_ENABLE_DEBUG_FEATURES
int debugContactChoiceAt(int x, int y) {
    const int dy = y - DEBUG_CONTACT_BUBBLE_Y;
    const int yesDx = x - DEBUG_CONTACT_BUBBLE_YES_X;
    if (yesDx * yesDx + dy * dy <=
        DEBUG_CONTACT_BUBBLE_HIT_RADIUS * DEBUG_CONTACT_BUBBLE_HIT_RADIUS) {
        return 0;
    }
    const int noDx = x - DEBUG_CONTACT_BUBBLE_NO_X;
    if (noDx * noDx + dy * dy <=
        DEBUG_CONTACT_BUBBLE_HIT_RADIUS * DEBUG_CONTACT_BUBBLE_HIT_RADIUS) {
        return 1;
    }
    return -1;
}
#endif

void renderMainMenu(Canvas565& canvas, const MenuViewModel& model,
                    uint16_t rowBegin, uint16_t rowEnd) {
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT, UiMetrics::PAGE_BACKGROUND);
    MenuGrid::mainMenu(canvas, MenuGrid::mainLayout(AmoledUi::WIDTH, AmoledUi::HEIGHT, 2),
                       MAIN_MENU_ITEM_COUNT, model.scroll, model.pressedItem);
    drawToast(canvas, model.toast);
}

#if STICKMON_ENABLE_DEBUG_FEATURES
namespace {

constexpr int DEBUG_CONTENT_TOP = UiMetrics::PAGE_HEADER_HEIGHT;
constexpr int DEBUG_ROW_HEIGHT = 64;
constexpr int DEBUG_TEXT_Y_OFFSET = 16;
constexpr int DEBUG_ROW_LEFT = 12;
constexpr int DEBUG_ROW_WIDTH = 344;
constexpr int DEBUG_POPUP_X = 20;
constexpr int DEBUG_POPUP_Y = 84;
constexpr int DEBUG_POPUP_W = 328;
constexpr int DEBUG_POPUP_H = 280;
constexpr int DEBUG_POPUP_CONTROL_TEXT_OFFSET = 16;

uint8_t debugItemCount(DebugViewModel::Category category) {
    switch (category) {
    case DebugViewModel::Category::MONSTER: return 4;
    case DebugViewModel::Category::RESOURCE: return 2;
    case DebugViewModel::Category::ENV: return 4;
    case DebugViewModel::Category::ROOM: return 3;
    case DebugViewModel::Category::MOTION: return 5;
    case DebugViewModel::Category::BATTLE: return 3;
    case DebugViewModel::Category::CONTACT_EVENT: return 4;
    case DebugViewModel::Category::TOUCH_TEST: return 1;
    case DebugViewModel::Category::ROOT:
    default: return 9;
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
        return Ui::Debug::AMOLED_ENV_ITEMS[index];
    case DebugViewModel::Category::ROOM:
        return Ui::Debug::ROOM_ITEMS[index];
    case DebugViewModel::Category::MOTION:
        return Ui::Debug::MOTION_ITEMS[index];
    case DebugViewModel::Category::BATTLE:
        return Ui::Debug::BATTLE_ITEMS[index];
    case DebugViewModel::Category::CONTACT_EVENT:
        return Ui::Debug::CONTACT_EVENT_ITEMS[index];
    case DebugViewModel::Category::TOUCH_TEST:
        return Ui::BACK;
    case DebugViewModel::Category::ROOT:
    default:
        return index == 7 ? Ui::Debug::TOUCH_TEST
             : index == 8 ? Ui::BACK : Ui::Debug::ROOT_ITEMS[index];
    }
}

const char* debugCategoryTitle(DebugViewModel::Category category) {
    switch (category) {
    case DebugViewModel::Category::MONSTER: return Ui::Debug::CATEGORY_MONSTER;
    case DebugViewModel::Category::RESOURCE: return Ui::Debug::CATEGORY_RESOURCE;
    case DebugViewModel::Category::ENV: return Ui::Debug::CATEGORY_ENV;
    case DebugViewModel::Category::ROOM: return Ui::Debug::ROOM;
    case DebugViewModel::Category::MOTION: return Ui::Debug::CATEGORY_MOTION;
    case DebugViewModel::Category::BATTLE: return Ui::Debug::CATEGORY_BATTLE;
    case DebugViewModel::Category::CONTACT_EVENT:
        return Ui::Debug::CATEGORY_CONTACT_EVENT;
    case DebugViewModel::Category::TOUCH_TEST: return Ui::Debug::TOUCH_TEST;
    case DebugViewModel::Category::ROOT:
    default: return Ui::DEBUG;
    }
}

void drawDebugPopup(Canvas565& canvas, const DebugViewModel& model) {
    canvas.fillRoundRect((DEBUG_POPUP_X), (DEBUG_POPUP_Y), (DEBUG_POPUP_W), (DEBUG_POPUP_H), (12), rgb(17, 27, 34));
    canvas.drawRoundRect((DEBUG_POPUP_X), (DEBUG_POPUP_Y), (DEBUG_POPUP_W), (DEBUG_POPUP_H), (12), rgb(115, 226, 183));
    if (model.popup == DebugViewModel::Popup::BED_STYLE ||
        model.popup == DebugViewModel::Popup::RUG_STYLE) {
        const bool bed = model.popup == DebugViewModel::Popup::BED_STYLE;
        const char* title = bed ? Ui::Debug::BED : Ui::Debug::RUG;
        text(canvas, DEBUG_POPUP_X + 24, DEBUG_POPUP_Y + 12, title, rgb(226, 238, 233));
        const uint8_t selected = bed ? model.bedStyle : model.rugStyle;
        for (uint8_t style = 1; style < Ui::Debug::FURNITURE_STYLE_COUNT; ++style) {
            const int y = DEBUG_POPUP_Y + 48 + (style - 1) * 44;
            canvas.fillRoundRect(DEBUG_POPUP_X + 20, y, DEBUG_POPUP_W - 40, 40, 8, rgb(36, 54, 61));
            if (selected == style) {
                canvas.drawLine(DEBUG_POPUP_X + 32, y + 19, DEBUG_POPUP_X + 39, y + 26, rgb(115, 226, 183));
                canvas.drawLine(DEBUG_POPUP_X + 39, y + 26, DEBUG_POPUP_X + 51, y + 12, rgb(115, 226, 183));
            }
            text(canvas, DEBUG_POPUP_X + 62, y + 4, Ui::Debug::FURNITURE_STYLES[style], rgb(226, 238, 233));
        }
        text(canvas, DEBUG_POPUP_X + 132, DEBUG_POPUP_Y + 232, Ui::BACK, rgb(248, 210, 105));
        return;
    }
    const bool timePopup = model.popup == DebugViewModel::Popup::SET_TIME;
    const char* title = timePopup ? Ui::Debug::TARGET_TIME : Ui::Debug::INPUT_ID;
    text(canvas, DEBUG_POPUP_X + (DEBUG_POPUP_W - textWidth(title)) / 2,
         DEBUG_POPUP_Y + 24, title, rgb(226, 238, 233));

    const uint8_t digitCount = timePopup ? 4 : 3;
    for (uint8_t index = 0; index < digitCount; ++index) {
        int x = DEBUG_POPUP_X + 36 + index * 68;
        bool focused = index == model.focus;
        canvas.fillRoundRect((x), (DEBUG_POPUP_Y + 68), (52), (64), (8), focused ? rgb(42, 61, 68) : rgb(24, 34, 42));
        canvas.drawRoundRect((x), (DEBUG_POPUP_Y + 68), (52), (64), (8), focused ? rgb(248, 210, 105) : rgb(67, 97, 101));
        char digit[2] = {static_cast<char>('0' + model.digits[index]), '\0'};
        text(canvas, x + 18,
             DEBUG_POPUP_Y + 68 + DEBUG_POPUP_CONTROL_TEXT_OFFSET, digit,
             focused ? rgb(248, 210, 105) : rgb(226, 238, 233));
    }
    canvas.fillRoundRect((DEBUG_POPUP_X + 36), (DEBUG_POPUP_Y + 176), (120), (64), (8), rgb(36, 54, 61));
    canvas.fillRoundRect((DEBUG_POPUP_X + 172), (DEBUG_POPUP_Y + 176), (120), (64), (8), rgb(91, 49, 55));
    text(canvas, DEBUG_POPUP_X + 74,
         DEBUG_POPUP_Y + 176 + DEBUG_POPUP_CONTROL_TEXT_OFFSET, Ui::Debug::YES,
         rgb(115, 226, 183));
    text(canvas, DEBUG_POPUP_X + 192,
         DEBUG_POPUP_Y + 176 + DEBUG_POPUP_CONTROL_TEXT_OFFSET,
         Ui::Debug::CANCEL,
         rgb(239, 143, 148));
}

}  // namespace

float debugMaxScroll(DebugViewModel::Category category) {
    return static_cast<float>(std::max(
        0, static_cast<int>(debugItemCount(category)) * DEBUG_ROW_HEIGHT -
            (AmoledUi::HEIGHT - DEBUG_CONTENT_TOP)));
}

bool debugBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

int debugItemAt(int x, int y, DebugViewModel::Category category,
                float scroll) {
    if (x < DEBUG_ROW_LEFT || x >= DEBUG_ROW_LEFT + DEBUG_ROW_WIDTH ||
        y < DEBUG_CONTENT_TOP || y >= 448) return -1;
    int contentY = y - DEBUG_CONTENT_TOP + static_cast<int>(std::lround(scroll));
    int index = contentY / DEBUG_ROW_HEIGHT;
    return index >= 0 && index < debugItemCount(category) ? index : -1;
}

int debugFurnitureStyleAt(int x, int y) {
    if (x < DEBUG_POPUP_X + 20 || x >= DEBUG_POPUP_X + DEBUG_POPUP_W - 20) return 0;
    for (unsigned style = 1; style < Ui::Debug::FURNITURE_STYLE_COUNT; ++style) {
        const int top = DEBUG_POPUP_Y + 48 + (style - 1) * 44;
        if (y >= top && y < top + 40) return style;
    }
    return 0;
}

bool debugFurnitureCancelAt(int x, int y) {
    return x < DEBUG_POPUP_X || x >= DEBUG_POPUP_X + DEBUG_POPUP_W ||
        y < DEBUG_POPUP_Y || y >= DEBUG_POPUP_Y + 224;
}

int debugPopupChoiceAt(int x, int y) {
    if (y < DEBUG_POPUP_Y + 164 || y >= DEBUG_POPUP_Y + DEBUG_POPUP_H) return -1;
    if (x >= DEBUG_POPUP_X + 20 && x < DEBUG_POPUP_X + 164) return 0;
    if (x >= DEBUG_POPUP_X + 164 && x < DEBUG_POPUP_X + DEBUG_POPUP_W - 16) return 1;
    return -1;
}

int debugPopupDigitAt(int x, int y, uint8_t digitCount) {
    if (y < DEBUG_POPUP_Y + 56 || y >= DEBUG_POPUP_Y + 148) return -1;
    for (uint8_t index = 0; index < digitCount; ++index) {
        int left = DEBUG_POPUP_X + 36 + index * 68;
        if (x >= left && x < left + 52) return index;
    }
    return -1;
}

void renderDebugScreen(Canvas565& canvas, const DebugViewModel& model,
                       uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    canvas.fillRect((0), (0), (AmoledUi::WIDTH), (AmoledUi::HEIGHT), UiMetrics::PAGE_BACKGROUND);
    if (model.category == DebugViewModel::Category::TOUCH_TEST) {
        const TouchTest::State empty;
        const auto& state = model.touchTest ? *model.touchTest : empty;
        text(canvas, 120, 82, Ui::Debug::TOUCH_TEST, rgb(115, 226, 183));
        const bool nativeText = canvas.nativeText();
        canvas.setNativeText(false);
        char buffer[80];
        for (int i = 0; i < TouchTest::COUNT; ++i) {
            const auto target = TouchTest::TARGETS[i];
            const uint16_t color = i == state.count ? rgb(248, 210, 105)
                : i < state.count ? rgb(115, 226, 183) : rgb(65, 78, 85);
            canvas.drawCircle(target.x, target.y, 12, color);
            canvas.drawFastHLine(target.x - 18, target.y, 37, color);
            canvas.drawFastVLine(target.x, target.y - 18, 37, color);
            std::snprintf(buffer, sizeof(buffer), "%d:%d,%d", i + 1, target.x, target.y);
            text(canvas, std::clamp(target.x - 36, 4, 292),
                 target.y == 416 ? 394 : target.y + 20, buffer, color);
        }
        if (state.count < TouchTest::COUNT) {
            const auto target = TouchTest::TARGETS[state.count];
            std::snprintf(buffer, sizeof(buffer), "%d/9  TARGET %d: (%d,%d)",
                          state.count, state.count + 1, target.x, target.y);
        } else {
            std::snprintf(buffer, sizeof(buffer), "9/9  COMPLETE");
        }
        text(canvas, 16, 122, buffer, rgb(248, 210, 105));
        const auto& sample = state.pending;
        std::snprintf(buffer, sizeof(buffer), "DOWN %d,%d   UP %s", sample.down.x,
                      sample.down.y, state.lastWasUp ? "" : "--");
        if (state.lastWasUp) std::snprintf(buffer, sizeof(buffer), "DOWN %d,%d   UP %d,%d",
            sample.down.x, sample.down.y, sample.up.x, sample.up.y);
        text(canvas, 16, 142, state.lastValid ? buffer : "DOWN --   UP --", rgb(226, 238, 233));
        if (state.lastValid) {
            const int index = std::min(state.lastWasUp ? std::max(0, state.count - 1)
                                                     : state.count, TouchTest::COUNT - 1);
            const auto target = TouchTest::TARGETS[index];
            std::snprintf(buffer, sizeof(buffer), "LIVE %d,%d   dx=%+d dy=%+d",
                          state.last.x, state.last.y, state.last.x - target.x, state.last.y - target.y);
            text(canvas, 16, 162, buffer, rgb(239, 143, 148));
            std::snprintf(buffer, sizeof(buffer), "GESTURE x:%d..%d y:%d..%d", sample.minimum.x,
                          sample.maximum.x, sample.minimum.y, sample.maximum.y);
            text(canvas, 16, 182, buffer, rgb(170, 185, 185));
        }
        float dx, dy, maxError;
        state.statistics(dx, dy, maxError);
        std::snprintf(buffer, sizeof(buffer), "DOWN mean dx=%+.1f dy=%+.1f", dx, dy);
        text(canvas, 16, 274, buffer, rgb(226, 238, 233));
        std::snprintf(buffer, sizeof(buffer), "DOWN max error=%.1f px", maxError);
        text(canvas, 16, 294, buffer, rgb(226, 238, 233));
        text(canvas, 16, 314, "DOWN", rgb(239, 143, 148));
        text(canvas, 64, 314, "/ UP", rgb(102, 205, 240));
        text(canvas, 112, 314, " 368x448", rgb(170, 185, 185));
        canvas.setNativeText(nativeText);
        for (const auto rect : {TouchTest::CLEAR_RECT, TouchTest::BACK_RECT}) {
            canvas.fillRect(rect.x, rect.y, rect.width, rect.height, rgb(36, 54, 61));
        }
        text(canvas, TouchTest::CLEAR_RECT.x + 26, TouchTest::CLEAR_RECT.y + 8,
             "重测", rgb(226, 238, 233));
        text(canvas, TouchTest::BACK_RECT.x + 26, TouchTest::BACK_RECT.y + 8,
             Ui::BACK, rgb(226, 238, 233));
        for (int i = 0; i < state.count; ++i) {
            const auto& point = state.samples[i];
            canvas.fillCircle(point.down.x, point.down.y, 3, rgb(239, 143, 148));
            canvas.drawCircle(point.up.x, point.up.y, 6, rgb(102, 205, 240));
        }
        if (state.pressed) canvas.drawCircle(state.last.x, state.last.y, 5, rgb(239, 143, 148));
        pageClip.reset();
        return;
    }
    UiCommon::drawPageHeader(canvas, debugCategoryTitle(model.category));

    uint8_t count = debugItemCount(model.category);
    int scroll = static_cast<int>(std::lround(model.scroll));
    for (uint8_t index = 0; index < count; ++index) {
        int y = DEBUG_CONTENT_TOP + index * DEBUG_ROW_HEIGHT - scroll;
        if (y + DEBUG_ROW_HEIGHT <= DEBUG_CONTENT_TOP || y >= 448) continue;
        bool selected = index == model.cursor;
        uint16_t background = selected ? rgb(42, 61, 68) : rgb(24, 34, 42);
        canvas.fillRoundRect((DEBUG_ROW_LEFT), (y + 4), (DEBUG_ROW_WIDTH), (DEBUG_ROW_HEIGHT - 10), (8), background);
        if (selected) canvas.fillRect((DEBUG_ROW_LEFT), (y + 14), (6), (DEBUG_ROW_HEIGHT - 30), rgb(248, 210, 105));
        text(canvas, DEBUG_ROW_LEFT + 20, y + DEBUG_TEXT_Y_OFFSET,
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
        } else if (model.category == DebugViewModel::Category::ROOM && index < 2) {
            value = Ui::Debug::FURNITURE_STYLES[index == 0 ? model.bedStyle : model.rugStyle];
        } else if (model.category == DebugViewModel::Category::MOTION && index == 0) {
            value = model.tiltEnabled ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (model.category == DebugViewModel::Category::MOTION && index == 1) {
            value = model.boundaryVisible ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (model.category == DebugViewModel::Category::MOTION && index == 2) {
            value = model.talkPointsVisible ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (model.category == DebugViewModel::Category::BATTLE && index == 1) {
            value = model.battleBoundsVisible ? Ui::Settings::ON : Ui::Settings::OFF;
        } else if (model.category == DebugViewModel::Category::ROOT && index == 6) {
            value = model.touchDisplayEnabled ? Ui::Settings::ON
                                              : Ui::Settings::OFF;
        }
        if (value) {
            text(canvas, 348 - textWidth(value), y + DEBUG_TEXT_Y_OFFSET, value,
                 selected ? rgb(255, 218, 178) : rgb(126, 175, 175));
        }
    }
    if (model.popup != DebugViewModel::Popup::NONE) drawDebugPopup(canvas, model);
    drawToast(canvas, model.toast);
    pageClip.reset();
}
#endif

#include "ui/ExploreScreens.inc"

void drawTeamSprite(Canvas565& canvas, uint16_t speciesId,
                    int centerX, int centerY);

const char* communicationStateLabel(
    Communication::VisitSessionService::State state) {
    using State = Communication::VisitSessionService::State;
    switch (state) {
    case State::HOSTING: return Ui::HOSTING;
    case State::SEARCHING: return Ui::SEARCHING;
    case State::ROOM_LIST: return Ui::Social::SELECT_ROOM;
    case State::JOINING: return Ui::Social::WAIT_HOST_DECISION;
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
    canvas.fillRoundRect((20), (y), (328), (72), (8), rgb(24, 38, 45));
    canvas.drawRoundRect((20), (y), (328), (72), (8), rgb(68, 100, 102));
    text(canvas, (368 - textWidth(label)) / 2, y + 28, label, color);
}

}  // namespace

void renderCommunicationScreen(Canvas565& canvas,
                               const CommunicationViewModel& model,
                               uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    using State = Communication::VisitSessionService::State;

    if (rowBegin < MENU_CONTENT_TOP) {
        int bottom = std::min<int>(rowEnd, MENU_CONTENT_TOP);
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (bottom - rowBegin));
        UiCommon::drawPageHeader(canvas, Ui::SOCIAL);
        pageClip.reset();
    }
    if (rowEnd <= MENU_CONTENT_TOP) return;
    int top = std::max<int>(rowBegin, MENU_CONTENT_TOP);
    pageClip.setRect((0), (top), (AmoledUi::WIDTH), (rowEnd - top));
    canvas.fillRect((0), (MENU_CONTENT_TOP), (AmoledUi::WIDTH), (AmoledUi::HEIGHT - MENU_CONTENT_TOP), UiMetrics::PAGE_BACKGROUND);

    if (model.state == State::IDLE) {
        drawCommunicationButton(canvas, 84, Ui::Amoled::HOST,
                                rgb(115, 226, 183));
        drawCommunicationButton(canvas, 180, Ui::Amoled::SEARCH,
                                rgb(226, 238, 233));
    } else if (model.state == State::SEARCHING ||
               model.state == State::ROOM_LIST) {
        const char* title = communicationStateLabel(model.state);
        if (model.error && std::strcmp(model.error, "JOIN DECLINED") == 0) {
            title = Ui::Social::JOIN_REJECTED;
        } else if (model.error &&
                   std::strcmp(model.error, "JOIN TIMEOUT") == 0) {
            title = Ui::Social::JOIN_TIMED_OUT;
        }
        text(canvas, 28, 84, title,
             rgb(115, 226, 183));
        if (model.roomCount == 0) {
            text(canvas, 96, 172, Ui::Amoled::NO_ROOM, rgb(151, 168, 166));
        } else {
            for (uint8_t index = 0; index < model.roomCount; ++index) {
                int y = 108 + index * 80;
                canvas.fillRoundRect((16), (y), (336), (64), (8), rgb(24, 38, 45));
                char room[24] = {};
                std::snprintf(room, sizeof(room), Ui::Social::ROOM_ROW_FMT,
                              static_cast<unsigned>(model.rooms[index].roomId));
                text(canvas, 36, y + 24, room,
                     rgb(226, 238, 233));
                text(canvas, 276, y + 24, Ui::Amoled::JOIN,
                     rgb(115, 226, 183));
            }
        }
    } else if (model.state == State::HOSTING) {
        text(canvas, 108, 84, Ui::Amoled::WAITING, rgb(115, 226, 183));
        char room[24] = {};
        std::snprintf(room, sizeof(room), Ui::Social::ROOM_ROW_FMT,
                      static_cast<unsigned>(model.roomId));
        text(canvas, (368 - textWidth(room)) / 2, 132, room,
             rgb(226, 238, 233));
    } else if (model.state == State::WAITING_HOST_DECISION) {
        text(canvas, 84, 84, Ui::Amoled::INCOMING, rgb(248, 210, 105));
        drawCommunicationButton(canvas, 132, Ui::Amoled::ACCEPT,
                                rgb(115, 226, 183));
        drawCommunicationButton(canvas, 224, Ui::Amoled::DECLINE,
                                rgb(239, 143, 148));
    } else if (model.state == State::JOINING ||
               model.state == State::SYNCING ||
               model.state == State::WAITING_ACCEPT) {
        text(canvas, 112, 96, communicationStateLabel(model.state),
             rgb(115, 226, 183));
        if (model.remote.known) {
            drawTeamSprite(canvas, model.remote.speciesId, 184, 252);
        }
    } else if (model.state == State::ACTIVE ||
               model.state == State::ENDING) {
        if (model.remote.known) {
            drawTeamSprite(canvas, model.remote.speciesId, 184, 200);
            char level[8] = {};
            std::snprintf(level, sizeof(level), "LV%u", model.remote.level);
            text(canvas, 148, 252, level, rgb(226, 238, 233));
            char value[8] = {};
            text(canvas, 40, 292, Ui::HUNGER, rgb(151, 168, 166));
            std::snprintf(value, sizeof(value), "%u", model.remote.satiety);
            text(canvas, 110, 292, value, rgb(115, 226, 183));
            text(canvas, 188, 292, Ui::MOOD, rgb(151, 168, 166));
            std::snprintf(value, sizeof(value), "%u", model.remote.mood);
            text(canvas, 258, 292, value, rgb(115, 226, 183));
            text(canvas, 92, 324, Ui::Amoled::AFFECTION,
                 rgb(151, 168, 166));
            std::snprintf(value, sizeof(value), "%u", model.remote.affection);
            text(canvas, 178, 324, value, rgb(248, 210, 105));
            text(canvas, 72, 348, Ui::Amoled::VISITING,
                 rgb(226, 238, 233));
            char remain[8] = {};
            std::snprintf(remain, sizeof(remain), "%us", model.remainSec);
            text(canvas, 184, 348, remain, rgb(115, 226, 183));
        }
        drawCommunicationButton(canvas, 368,
                                model.state == State::ENDING
                                    ? Ui::Amoled::ENDING : Ui::Amoled::EXIT,
                                rgb(239, 143, 148));
    } else {
        text(canvas, 116, 96, communicationStateLabel(model.state),
             rgb(239, 143, 148));
        if (model.error) text(canvas, 64, 152, model.error, rgb(239, 143, 148));
        drawCommunicationButton(canvas, 264, Ui::BACK, rgb(115, 226, 183));
    }
    pageClip.reset();
}

bool communicationBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

int communicationItemAt(int x, int y,
                        const CommunicationViewModel& model) {
    if (x < 16 || x >= 356 || y < MENU_CONTENT_TOP || y >= 448) return -1;
    using State = Communication::VisitSessionService::State;
    if (model.state == State::IDLE) {
        if (y >= 84 && y < 156) return 0;
        if (y >= 180 && y < 252) return 1;
    } else if (model.state == State::ROOM_LIST) {
        if (y >= 108 && y < 108 + model.roomCount * 80) {
            return (y - 108) / 80;
        }
    } else if (model.state == State::WAITING_HOST_DECISION) {
        if (y >= 132 && y < 204) return 0;
        if (y >= 224 && y < 296) return 1;
    } else if (model.state == State::ACTIVE ||
               model.state == State::ENDING) {
        if (y >= 368) return 0;
    } else if (model.state == State::FAILED ||
               model.state == State::ENDED) {
        if (y >= 264 && y < 336) return 0;
    }
    return -1;
}

#include "ui/TeamScreens.inc"

#include "ui/ItemShopScreens.inc"

#include "ui/ComputerScreens.inc"

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

int battleItemAt(int x, int y, BattleViewModel::Phase phase) {
    if (x < 0 || x >= 368 || y < BATTLE_FOOTER_Y || y >= 448) return -1;
    if (phase == BattleViewModel::Phase::SWITCH_SELECT) {
        return x < 184 ? 0 : 1;
    }
    if (phase == BattleViewModel::Phase::BAG_SELECT) {
        return std::min(3, x * 4 / AmoledUi::WIDTH);
    }
    if (phase == BattleViewModel::Phase::FRIENDSHIP) {
        return x < 184 ? 0 : 1;
    }
    if (phase == BattleViewModel::Phase::VICTORY) return -1;
    if (phase != BattleViewModel::Phase::ACTION) return 0;
    return std::min(3, x * 4 / AmoledUi::WIDTH);
}

bool battleBackAt(int x, int y) {
    return x >= 308 && x < 368 && y >= 0 && y < HEADER_HEIGHT;
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
            int x = centerX - 32 + i * 30;
            int y = groundY - 16 - ((pulse + i) % 4) * 8;
            int radius = 4 + ((pulse + i) & 1);
            canvas.fillCircle((x), (y), (radius + 2), outline);
            canvas.fillCircle((x), (y), (radius), color);
        }
        break;
    }
    case Game::MajorStatus::PARALYSIS: {
        const uint16_t color = rgb(255, 215, 55);
        const int jitter = (pulse & 1) ? 4 : 0;
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * (46 + jitter);
            int y = groundY - 70 + (side > 0 ? 10 : 0);
            canvas.drawLine((x), (y), (x - side * 10), (y + 12), outline);
            canvas.drawLine((x - side * 10), (y + 12), ((x + (side * 2))), (y + 12), outline);
            canvas.drawLine(((x + (side * 2))), (y + 12), (x - side * 8), (y + 26), outline);
            canvas.drawLine((x), (y), (x - side * 8), (y + 12), color);
            canvas.drawLine((x - side * 8), (y + 12), (x + side * 4), (y + 12), color);
            canvas.drawLine((x + side * 4), (y + 12), (x - side * 6), (y + 26), color);
        }
        break;
    }
    case Game::MajorStatus::SLEEP: {
        const uint16_t color = rgb(116, 169, 232);
        int rise = static_cast<int>((nowMs / 240U) % 3U);
        PixelRenderer::textOutlined((centerX + 36), (groundY - 90 - rise * 4), "Z", color, outline, 1);
        PixelRenderer::textOutlined((centerX + 56), (groundY - 110 - rise * 4), "Z", color, outline, 1);
        break;
    }
    case Game::MajorStatus::BURN: {
        const int sway = (pulse & 1) ? 2 : -1;
        const uint16_t red = rgb(226, 76, 42);
        const uint16_t yellow = rgb(255, 191, 46);
        for (int side = -1; side <= 1; side += 2) {
            int x = centerX + side * 38;
            int y = groundY - 20;
            canvas.fillTriangle((x - 10), (y + 14), (x + 10), (y + 14), ((x + sway * (side * 2))), (y - 14), outline);
            canvas.fillTriangle((x - 8), (y + 12), (x + 8), (y + 12), ((x + sway * (side * 2))), (y - 12), red);
            canvas.fillTriangle((x - 4), (y + 10), (x + 4), (y + 10), ((x - sway * (side * 2))), (y), yellow);
        }
        break;
    }
    case Game::MajorStatus::FREEZE: {
        const uint16_t color = rgb(116, 219, 239);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX - 40 + i * 40;
            int y = groundY - 26 - ((pulse + i) & 1) * 10;
            canvas.drawLine((x - 8), (y), (x + 8), (y), outline);
            canvas.drawLine((x), (y - 8), (x), (y + 8), outline);
            canvas.drawLine((x - 6), (y - 6), (x + 6), (y + 6), outline);
            canvas.drawLine((x - 6), (y + 6), (x + 6), (y - 6), outline);
            canvas.drawLine((x - 6), (y), (x + 6), (y), color);
            canvas.drawLine((x), (y - 6), (x), (y + 6), color);
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
                std::cos(pointAngle) * 34.0f));
            int y = groundY - 100 + static_cast<int>(std::lround(
                std::sin(pointAngle) * 8.0f));
            canvas.fillCircle((x), (y), (6), outline);
            canvas.fillCircle((x), (y), (4), colors[i]);
        }
    }
    if (state.bindTurns > 0) {
        const uint16_t color = rgb(205, 154, 86);
        int y = groundY - 44 + ((pulse & 1) ? 2 : -2);
        canvas.drawFastHLine((centerX - 48), (y), (96), outline);
        canvas.drawFastHLine((centerX - 48), (y + 10), (96), outline);
        canvas.drawFastHLine((centerX - 46), (y + 2), (92), color);
        canvas.drawFastHLine((centerX - 46), (y + 8), (92), color);
    }
    if (state.yawnTurns > 0 && majorStatus != Game::MajorStatus::SLEEP) {
        const uint16_t color = rgb(181, 202, 225);
        int drift = static_cast<int>(((nowMs / 260U) % 6U) * 2);
        for (uint8_t i = 0; i < 3; ++i) {
            int x = centerX + 30 + i * 12;
            int y = groundY - 90 - i * 4 - drift;
            canvas.fillCircle((x), (y), (4), outline);
            canvas.fillCircle((x), (y), (2), color);
        }
    }
}

int drawBattleSprite(Canvas565& canvas, uint16_t speciesId,
                     int centerX, int groundY, int maxWidth, int maxHeight,
                     bool back, AmoledUi::Rect* drawnBounds = nullptr) {
    if (drawnBounds) *drawnBounds = {centerX - 40, groundY - 96, 80, 96};
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, back ? PokemonSprites::SpriteKind::BACK
                            : PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        drawFallbackPet(canvas, centerX, groundY, false);
        return 0;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return 0;
    const uint8_t encodedPadding =
        FlashStorage::readByte(&frame->groundPaddingPlusOne);
    const int groundPadding = encodedPadding > 0 ? encodedPadding - 1 : 0;
    const auto layout = back
        ? fitBattleSprite(width, height, groundPadding, centerX, groundY,
                          maxWidth, maxHeight, PLAYER_SPRITE_TARGET_HEIGHT)
        : fitBattleFrontSprite(speciesId, width, height, groundPadding,
                               centerX, groundY, maxWidth, maxHeight);
    if (drawnBounds) {
        *drawnBounds = layout.rect;
    }
    PokemonSprites::drawFrameScaled(
        frame, layout.rect.x, layout.rect.y, layout.scale, false);
    return layout.verticalOffset;
}

void drawBattleStatusMoveEffect(const BattleViewModel& model,
                                const AmoledUi::Rect& playerBounds,
                                const AmoledUi::Rect& wildBounds) {
    if (!model.animationStatusEffect || model.animationFrame < 2 ||
        model.animationFrame > 5) return;
    const uint8_t frame = model.animationFrame - 2;
    const auto& from = model.animationAttackerWild ? wildBounds : playerBounds;
    const auto& to = model.animationAttackerWild ? playerBounds : wildBounds;
    const int direction = model.animationAttackerWild ? -1 : 1;
    const int sourceX = from.x + from.width / 2 + direction * from.width / 5;
    const int sourceY = from.y + from.height / 3;
    int x = sourceX, y = sourceY;
    float scale = 0.85f;
    if (model.animationFx != GameAssets::BattleFx::LEER) {
        const int progress = (frame + 1) * 25;
        const int targetX = to.x + to.width / 2;
        const int targetY = to.y + to.height / 2;
        const int startY = model.animationFx == GameAssets::BattleFx::SAND_ATTACK
            ? from.y + from.height - 12 : sourceY;
        // Route particles through the gap above the player's name/HP panel.
        const int middleX = 156;
        const int middleY = std::min(from.y, to.y) +
            std::min(from.height, to.height) * 2 / 3;
        if (progress <= 50) {
            x = sourceX + (middleX - sourceX) * progress / 50;
            y = startY + (middleY - startY) * progress / 50;
        } else {
            x = middleX + (targetX - middleX) * (progress - 50) / 50;
            y = middleY + (targetY - middleY) * (progress - 50) / 50;
        }
        scale = 0.65f + frame * 0.18f;
    }
    GameAssets::drawCentered(
        GameAssets::battleFxKind(model.animationFx, frame), x, y, scale);
}

void drawBattleStatusIcon(Canvas565& canvas, Game::MajorStatus status,
                          int x, int y) {
    GameAssets::Kind kind = GameAssets::statusKind(status);
    if (kind != GameAssets::Kind::COUNT) {
        GameAssets::draw(kind, x, y, 2.0f);
    }
}

void drawBattleHitEffect(Canvas565& canvas, int centerX, int centerY,
                         uint8_t animationFrame) {
    if (animationFrame == 0) return;
    const uint16_t outline = rgb(43, 39, 44);
    const uint16_t flash = animationFrame == 2
        ? rgb(239, 143, 148) : rgb(248, 210, 105);
    int radius = animationFrame == 2 ? 30 : 20;
    canvas.drawCircle((centerX), (centerY), (radius + 4), outline);
    canvas.drawCircle((centerX), (centerY), (radius), flash);
    canvas.drawLine((centerX - radius - 10), (centerY), (centerX - radius + 2), (centerY), flash);
    canvas.drawLine((centerX + radius - 2), (centerY), (centerX + radius + 10), (centerY), flash);
    canvas.drawLine((centerX), (centerY - radius - 10), (centerX), (centerY - radius + 2), flash);
    canvas.drawLine((centerX), (centerY + radius - 2), (centerX), (centerY + radius + 10), flash);
    if (animationFrame == 2) {
        canvas.fillCircle((centerX - 18), (centerY - 16), (4), flash);
        canvas.fillCircle((centerX + 20), (centerY + 14), (4), flash);
    }
}

bool drawBattleFxEffect(int centerX, int centerY, GameAssets::BattleFx fx,
                        uint8_t frame) {
    const GameAssets::Kind kind = GameAssets::battleFxKind(fx, frame);
    return GameAssets::drawCentered(kind, centerX, centerY, 1.35f);
}

void drawBattleSceneText(int x, int y, const char* value) {
    PixelRenderer::textOutlined((x), (y), value, rgb(25, 31, 40), rgb(241, 242, 232), 1);
}

void drawBattleFooter(Canvas565& canvas) {
    PixelRenderer::fillRectAlpha((0), (BATTLE_FOOTER_Y), (AmoledUi::WIDTH), (BATTLE_FOOTER_HEIGHT), rgb(204, 204, 204), 153);
    canvas.drawRect((0), (BATTLE_FOOTER_Y), (AmoledUi::WIDTH), (BATTLE_FOOTER_HEIGHT), rgb(74, 91, 75));
}

void drawBattleBackIcon(Canvas565& canvas) {
    drawHeaderButton(canvas, 316);
    const uint16_t color = rgb(255, 255, 255);
    UiCommon::drawBackChevron(canvas, 334, 24, 12, color);
}

void drawBattleFooterChoice(Canvas565& canvas, int index, int count,
                            const char* label, bool pressed, bool enabled) {
    if (count <= 0 || index < 0 || index >= count) return;
    int left = index * AmoledUi::WIDTH / count;
    int right = (index + 1) * AmoledUi::WIDTH / count;
    if (pressed) {
        PixelRenderer::fillRectAlpha((left + 2), (BATTLE_FOOTER_Y + 2), (right - left - 2), (BATTLE_FOOTER_HEIGHT - 4), rgb(54, 111, 94), 96);
        canvas.fillRect((left + 8), (BATTLE_FOOTER_Y + 6), ((std::max(1, right - left - 8))), (4), rgb(62, 150, 124));
    }
    if (index > 0) {
        canvas.drawFastVLine((left), (BATTLE_FOOTER_Y + 18), (BATTLE_FOOTER_HEIGHT - 36), rgb(105, 116, 108));
    }
    const uint16_t color = enabled ? rgb(25, 31, 40) : rgb(102, 108, 108);
    int labelX = left + (right - left - textWidth(label)) / 2;
    text(canvas, labelX, BATTLE_FOOTER_Y + 38, label, color);
}

}  // namespace

bool prepareBattleBackground(Canvas565& canvas, PixelCache565& backgroundCache,
                             GameAssets::Kind kind) {
    const RenderCacheKey key{static_cast<uint32_t>(kind), 0,
                             canvas.physicalWidth(), canvas.physicalHeight(),
                             canvas.byteSwapped()};
    if (backgroundCache.matches(key)) return true;
#if STICKMON_ENABLE_DEBUG_FEATURES
    const uint32_t startedUs = Platform::clock().micros();
#endif
    drawBattleBackgroundLayer(canvas, backgroundCache, kind, 0, AmoledUi::HEIGHT
#if STICKMON_ENABLE_DEBUG_FEATURES
                              , nullptr, nullptr
#endif
                              );
#if STICKMON_ENABLE_DEBUG_FEATURES
    Platform::logf("[BattleBgPrewarm] ready=%u elapsed=%lu us\n",
                   backgroundCache.matches(key) ? 1U : 0U,
                   static_cast<unsigned long>(Platform::clock().micros() - startedUs));
#endif
    return backgroundCache.matches(key);
}

void renderBattleScreen(Canvas565& canvas, const BattleViewModel& model,
                        PixelCache565& backgroundCache,
                        uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;
    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
#if STICKMON_ENABLE_DEBUG_FEATURES
    static bool coldBattleProfiled = false;
    static bool warmBattleProfiled = false;
    const RenderCacheKey backgroundKey{
        static_cast<uint32_t>(model.battleBackground), 0,
        canvas.physicalWidth(), canvas.physicalHeight(), canvas.byteSwapped()};
    const bool cacheHit = backgroundCache.matches(backgroundKey);
    const bool profileBattle = rowBegin == 0 && rowEnd == AmoledUi::HEIGHT &&
        (cacheHit ? !warmBattleProfiled : !coldBattleProfiled);
    uint32_t stamps[11] = {};
    uint32_t backgroundAssetUs = 0;
    uint32_t backgroundCacheUs = 0;
    if (profileBattle) stamps[0] = Platform::clock().micros();
#endif
    drawBattleBackgroundLayer(
        canvas, backgroundCache, model.battleBackground, rowBegin, rowEnd
#if STICKMON_ENABLE_DEBUG_FEATURES
        , profileBattle ? &backgroundAssetUs : nullptr,
          profileBattle ? &backgroundCacheUs : nullptr
#endif
        );
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[1] = Platform::clock().micros();
#endif
    const Species* wild = findSpecies(model.wildSpeciesId);
    const Species* player = findSpecies(model.playerSpeciesId);
    static constexpr int WILD_NAME_X = 17;
    static constexpr int WILD_LEVEL_X = 164;
    static constexpr int PLAYER_NAME_X = 184;
    static constexpr int PLAYER_LEVEL_X = 300;
    static constexpr int NAME_LEVEL_GAP = 8;
    if (wild) {
        const int nameX = std::min(
            WILD_NAME_X,
            WILD_LEVEL_X - NAME_LEVEL_GAP - textWidth(wild->name));
        drawBattleSceneText(std::max(4, nameX), 16, wild->name);
    }
    char level[10];
    std::snprintf(level, sizeof(level), "LV%u", model.wildLevel);
    drawBattleSceneText(WILD_LEVEL_X, 16, level);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[2] = Platform::clock().micros();
#endif
    static constexpr int WILD_GROUND_Y = 229;
    static constexpr int PLAYER_GROUND_Y = 344;
    int wildX = 272;
    int playerX = 80;
    if (model.animationActive && !GameAssets::battleFxIsStatus(model.animationFx)) {
        static constexpr int LUNGE[] = {0, 12, 26, 38, 26, 12, 0};
        uint8_t frame = std::min<uint8_t>(model.animationFrame, 6);
        int lunge = LUNGE[frame];
        if (model.animationAttackerWild) wildX -= lunge;
        else playerX += lunge;
        if (model.animationHit && frame >= 3 && frame <= 5) {
            int shake = (frame & 1U) ? -8 : 8;
            if (model.animationAttackerWild) playerX += shake;
            else wildX += shake;
        }
    }
    playerX += model.playerSwitchOffsetX;
    AmoledUi::Rect wildBounds{}, playerBounds{};
    const int wildVerticalOffset = drawBattleSprite(
        canvas, model.wildSpeciesId, wildX, WILD_GROUND_Y,
        WILD_SPRITE_AREA_WIDTH, BATTLE_SPRITE_AREA_HEIGHT, false, &wildBounds);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[3] = Platform::clock().micros();
#endif
    drawBattleStatusIcon(canvas, model.wildStatus, 12, 66);
    drawBattleHpBar(canvas, 44, 72, 128, model.wildHp);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[4] = Platform::clock().micros();
#endif

    if (player) {
        const int nameX = std::min(
            PLAYER_NAME_X,
            PLAYER_LEVEL_X - NAME_LEVEL_GAP - textWidth(player->name));
        drawBattleSceneText(std::max(4, nameX), 234, player->name);
    }
    std::snprintf(level, sizeof(level), "LV%u", model.playerLevel);
    drawBattleSceneText(PLAYER_LEVEL_X, 234, level);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[5] = Platform::clock().micros();
#endif
    const int playerVerticalOffset = drawBattleSprite(
        canvas, model.playerSpeciesId, playerX, PLAYER_GROUND_Y,
        PLAYER_SPRITE_AREA_WIDTH, BATTLE_SPRITE_AREA_HEIGHT, true, &playerBounds);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[6] = Platform::clock().micros();
#endif
    if (model.showPlayerExperience) {
        drawBattleExperienceBar(canvas, 184, 290, 168,
                                model.playerExperience);
    } else {
        drawBattleStatusIcon(canvas, model.playerStatus, 184, 284);
        drawBattleHpBar(canvas, 216, 290, 136, model.playerHp);
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[7] = Platform::clock().micros();
#endif

#if STICKMON_ENABLE_DEBUG_FEATURES
    if (model.debugDrawBounds) {
        canvas.drawRect(wildX - WILD_SPRITE_AREA_WIDTH / 2,
                        WILD_GROUND_Y - BATTLE_SPRITE_AREA_HEIGHT,
                        WILD_SPRITE_AREA_WIDTH, BATTLE_SPRITE_AREA_HEIGHT,
                        rgb(255, 32, 32));
        canvas.drawRect(playerX - PLAYER_SPRITE_AREA_WIDTH / 2,
                        PLAYER_GROUND_Y - BATTLE_SPRITE_AREA_HEIGHT,
                        PLAYER_SPRITE_AREA_WIDTH, BATTLE_SPRITE_AREA_HEIGHT,
                        rgb(0, 220, 255));
    }
#endif

    uint32_t nowMs = Platform::clock().millis();
    if (model.wildHp > 0) {
        drawBattleConditionEffects(canvas, wildX, WILD_GROUND_Y - wildVerticalOffset,
                                   model.wildStatus,
                                   model.wildBattleState, nowMs);
    }
    if (model.playerHp > 0) {
        drawBattleConditionEffects(canvas, playerX, PLAYER_GROUND_Y - playerVerticalOffset,
                                   model.playerStatus,
                                   model.playerBattleState, nowMs);
    }
    if (model.thrownItem != Game::ItemId::COUNT) {
        const float progress = std::clamp(model.itemThrowProgress, 0.0f, 1.0f);
        const float fromX = playerBounds.x + playerBounds.width / 2.0f;
        const float fromY = std::min(320.0f, playerBounds.y + playerBounds.height / 2.0f);
        const float toX = wildBounds.x + wildBounds.width / 2.0f;
        const float toY = wildBounds.y + wildBounds.height / 2.0f;
        const int x = static_cast<int>(std::lround(fromX + (toX - fromX) * progress));
        const int y = static_cast<int>(std::lround(fromY + (toY - fromY) * progress -
                                                  240.0f * progress * (1.0f - progress)));
        GameAssets::drawCentered(GameAssets::itemKind(model.thrownItem), x, y,
                                 0.8f - 0.3f * progress);
    }
    if (model.animationActive) {
        drawBattleStatusMoveEffect(model, playerBounds, wildBounds);
    }
    if (model.animationActive && model.animationHit) {
        int hitX = model.animationAttackerWild ? playerX : wildX;
        int hitY = model.animationAttackerWild
                       ? PLAYER_GROUND_Y - playerVerticalOffset - 44
                       : wildBounds.y + wildBounds.height / 2;
        if (model.animationFrame >= 2 && model.animationFrame <= 5) {
            const uint8_t fxFrame = static_cast<uint8_t>(model.animationFrame - 2);
            if (!drawBattleFxEffect(hitX, hitY, model.animationFx, fxFrame)) {
                drawBattleHitEffect(canvas, hitX, hitY,
                                    static_cast<uint8_t>(std::min<uint8_t>(
                                        2, fxFrame + 1)));
            }
        }
    }
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[8] = Platform::clock().micros();
#endif

    drawBattleFooter(canvas);
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) stamps[9] = Platform::clock().micros();
#endif
    if (model.phase == BattleViewModel::Phase::BAG_SELECT ||
        model.phase == BattleViewModel::Phase::SWITCH_SELECT) {
        drawBattleBackIcon(canvas);
    }

    if (model.logCount > 0) {
        for (uint8_t line = 0; line < model.logCount && line < 2; ++line) {
            if (!model.logLines[line]) continue;
            text(canvas, 20, BATTLE_FOOTER_Y + 14 + line * 36,
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
    } else if (model.phase == BattleViewModel::Phase::DEFEAT) {
        drawBattleFooterChoice(canvas, 0, 1, Ui::Amoled::REST_HOME,
                               model.pressedItem == 0, true);
    }
    pageClip.reset();
#if STICKMON_ENABLE_DEBUG_FEATURES
    if (profileBattle) {
        stamps[10] = Platform::clock().micros();
        Platform::logf(
            "[BattleDrawPerf] cacheHit=%u background=%lu asset=%lu cache=%lu "
            "wildText=%lu "
            "wildSprite=%lu wildHud=%lu playerText=%lu playerSprite=%lu "
            "playerHud=%lu effects=%lu footerBase=%lu footerItems=%lu "
            "total=%lu us\n",
            cacheHit ? 1U : 0U,
            static_cast<unsigned long>(stamps[1] - stamps[0]),
            static_cast<unsigned long>(backgroundAssetUs),
            static_cast<unsigned long>(backgroundCacheUs),
            static_cast<unsigned long>(stamps[2] - stamps[1]),
            static_cast<unsigned long>(stamps[3] - stamps[2]),
            static_cast<unsigned long>(stamps[4] - stamps[3]),
            static_cast<unsigned long>(stamps[5] - stamps[4]),
            static_cast<unsigned long>(stamps[6] - stamps[5]),
            static_cast<unsigned long>(stamps[7] - stamps[6]),
            static_cast<unsigned long>(stamps[8] - stamps[7]),
            static_cast<unsigned long>(stamps[9] - stamps[8]),
            static_cast<unsigned long>(stamps[10] - stamps[9]),
            static_cast<unsigned long>(stamps[10] - stamps[0]));
        (cacheHit ? warmBattleProfiled : coldBattleProfiled) = true;
    }
#endif
}

namespace {

constexpr int SHOWER_PET_X = 184;
constexpr int SHOWER_PET_Y = 212;
constexpr int SHOWER_TOOLBAR_Y = 352;
constexpr int SHOWER_TOOL_BUTTON_W = 116;
constexpr int SHOWER_TOOL_BUTTON_PITCH = 120;

void drawShowerPet(Canvas565& canvas, const ShowerViewModel& model) {
    const uint16_t speciesId = model.speciesId;
    const PokemonSprites::SpriteFrame* frame =
        PokemonSprites::findSpeciesSprite(
            speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) {
        drawFallbackPet(canvas, SHOWER_PET_X, 316);
        return;
    }
    int width = FlashStorage::readByte(&frame->width);
    int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return;
    float scale = std::min(200.0f / width, 224.0f / height);
    scale = std::min(scale, 2.8f);
    int drawWidth = static_cast<int>(width * scale);
    int drawHeight = static_cast<int>(height * scale);
    PokemonSprites::drawFrameScaled(
        frame, SHOWER_PET_X - drawWidth / 2,
        SHOWER_PET_Y - drawHeight / 2 + model.petOffsetY, scale, model.petFlip);
}

void drawShowerBubbles(const ShowerViewModel& model) {
    for (uint8_t stage = 0; stage < 4; ++stage) {
        for (uint8_t index = 0; index < model.foamCount; ++index) {
            const ShowerFoamState& foam = model.foam[index];
            if (!foam.active || foam.stage != stage) continue;
            const auto kind = static_cast<GameAssets::Kind>(
                static_cast<uint16_t>(GameAssets::Kind::SHOWER_BUBBLE_0) + stage);
            GameAssets::drawCentered(kind, static_cast<int>(foam.x), static_cast<int>(foam.y), 2.0f);
        }
    }
    if (model.atmosphereAlpha > 0) {
        GameAssets::drawCenteredAlpha(GameAssets::Kind::SHOWER_BUBBLE_5,
            SHOWER_PET_X, model.groundY - 40, 2.0f, model.atmosphereAlpha);
    }
}

void drawShowerToolbar(Canvas565& canvas, const ShowerViewModel& model) {
    static constexpr GameAssets::Kind ICONS[] = {
        GameAssets::Kind::SHOWER_MENU_SOAP,
        GameAssets::Kind::SHOWER_MENU_BRUSH,
        GameAssets::Kind::SHOWER_MENU_SPRINKLER,
    };
    static constexpr const char* LABELS[] = {
        Ui::Amoled::SOAP, Ui::Amoled::BRUSH, Ui::Amoled::RINSE,
    };
    for (int index = 0; index < 3; ++index) {
        int x = 6 + index * SHOWER_TOOL_BUTTON_PITCH;
        bool selected = index == model.pressedItem;
        canvas.fillRoundRect((x), (SHOWER_TOOLBAR_Y), (SHOWER_TOOL_BUTTON_W), (88), (8), selected ? rgb(48, 74, 68) : rgb(24, 34, 42));
        canvas.drawRoundRect((x), (SHOWER_TOOLBAR_Y), (SHOWER_TOOL_BUTTON_W), (88), (8), rgb(67, 97, 101));
        GameAssets::drawCentered(ICONS[index],
                                 x + SHOWER_TOOL_BUTTON_W / 2,
                                 SHOWER_TOOLBAR_Y + 36, 1.12f);
        int labelX = x + (SHOWER_TOOL_BUTTON_W - textWidth(LABELS[index])) / 2;
        text(canvas, labelX, SHOWER_TOOLBAR_Y + 62, LABELS[index],
             rgb(126, 175, 175));
    }
}

void drawShowerSoapPicker(Canvas565& canvas, const ShowerViewModel& model) {
    canvas.fillRoundRect((16), (132), (336), (196), (12), rgb(17, 27, 34));
    canvas.drawRoundRect((16), (132), (336), (196), (12), rgb(82, 117, 117));
    text(canvas, 116, 150, Ui::Amoled::CHOOSE_SOAP, rgb(226, 238, 233));
    for (uint8_t index = 0; index < Game::SOAP_VARIANT_COUNT; ++index) {
        int x = 26 + index * 110;
        uint8_t count = model.state ? model.state->bag.soap[index] : 0;
        canvas.fillRoundRect((x), (188), (98), (118), (8), count > 0 ? rgb(28, 42, 48) : rgb(20, 27, 31));
        GameAssets::Kind kind = static_cast<GameAssets::Kind>(
            static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) + index);
        GameAssets::drawCenteredAlpha(kind, x + 48, 232, 1.64f,
                                      count > 0 ? 255 : 80);
        char stock[8];
        std::snprintf(stock, sizeof(stock), "X%u", count);
        text(canvas, x + 34, 280, stock,
             count > 0 ? rgb(248, 210, 105) : rgb(91, 104, 104));
    }
}

void drawShowerExitConfirm(Canvas565& canvas, const ShowerViewModel& model) {
    canvas.fillRoundRect((20), (232), (328), (196), (12), rgb(17, 27, 34));
    canvas.drawRoundRect((20), (232), (328), (196), (12), rgb(82, 117, 117));
    text(canvas, 86, 258, Ui::Amoled::FOAM_REMAINS,
         rgb(248, 210, 105));
    text(canvas, 116, 296, Ui::Amoled::LEAVE_BATH,
         rgb(226, 238, 233));
    canvas.fillRoundRect((36), (348), (140), (72), (8), model.exitConfirmYes ? rgb(48, 74, 68)
                                              : rgb(36, 54, 61));
    canvas.fillRoundRect((192), (348), (140), (72), (8), !model.exitConfirmYes ? rgb(76, 48, 54)
                                               : rgb(46, 39, 43));
    text(canvas, 78, 376, Ui::Amoled::EXIT, rgb(115, 226, 183));
    text(canvas, 232, 376, Ui::Amoled::STAY, rgb(239, 143, 148));
}

}  // namespace

bool showerBackAt(int x, int y) {
    return UiCommon::pageHeaderBackAt(x, y);
}

int showerMenuItemAt(int x, int y) {
    if (y < SHOWER_TOOLBAR_Y || y >= 440) return -1;
    for (int index = 0; index < 3; ++index) {
        const int left = 6 + index * SHOWER_TOOL_BUTTON_PITCH;
        if (x >= left && x < left + SHOWER_TOOL_BUTTON_W) return index;
    }
    return -1;
}

int showerSoapItemAt(int x, int y) {
    if (x < 26 || x >= 356 || y < 188 || y >= 306) return -1;
    int index = (x - 26) / 110;
    return index < Game::SOAP_VARIANT_COUNT ? index : -1;
}

int showerExitChoiceAt(int x, int y) {
    if (y < 348 || y >= 420) return -1;
    if (x >= 36 && x < 176) return 0;
    if (x >= 192 && x < 332) return 1;
    return -1;
}

bool showerToolAt(int x, int y, int toolX, int toolY) {
    int dx = x - toolX;
    int dy = y - toolY;
    return dx * dx + dy * dy <= 56 * 56;
}

int showerPetGroundY(uint16_t speciesId) {
    const auto* frame = PokemonSprites::findSpeciesSprite(speciesId, PokemonSprites::SpriteKind::FRONT);
    if (!frame) return 316;
    const int width = FlashStorage::readByte(&frame->width);
    const int height = FlashStorage::readByte(&frame->height);
    if (width <= 0 || height <= 0) return 316;
    const float scale = std::min(2.8f, std::min(200.0f / width, 224.0f / height));
    return std::min(324, SHOWER_PET_Y + static_cast<int>(height * scale) / 2);
}

namespace {
void drawShowerStatic(Canvas565& canvas, const ShowerViewModel& model,
                      PixelCache565* cache, uint16_t begin, uint16_t end) {
    const RenderCacheKey key{static_cast<uint32_t>(GameAssets::Kind::SHOWER_BACKGROUND), 0,
        canvas.physicalWidth(), canvas.physicalHeight(), canvas.byteSwapped()};
    if (cache && cache->copyRectTo(canvas, key, model.columnBegin, begin, model.columnEnd, end)) return;
    canvas.fillRect(0, 0, AmoledUi::WIDTH, AmoledUi::HEIGHT, rgb(24, 43, 37));
    if (!GameAssets::draw(GameAssets::Kind::SHOWER_BACKGROUND, -56, UiMetrics::PAGE_HEADER_HEIGHT, 2.0f)) {
        canvas.fillRect(0, UiMetrics::PAGE_HEADER_HEIGHT, AmoledUi::WIDTH,
            328 - UiMetrics::PAGE_HEADER_HEIGHT, rgb(178, 116, 57));
    }
    UiCommon::drawPageHeader(canvas, Ui::Amoled::WASH_PET);
    canvas.fillRect(0, 328, AmoledUi::WIDTH, 120, UiMetrics::PAGE_BACKGROUND);
    ShowerViewModel idle = model;
    idle.pressedItem = -1;
    drawShowerToolbar(canvas, idle);
    if (cache && begin == 0 && end == AmoledUi::HEIGHT &&
        model.columnBegin == 0 && model.columnEnd == AmoledUi::WIDTH &&
        !cache->matches(key)) {
        if (auto* pixels = cache->begin(key, Platform::memory())) {
            std::memcpy(pixels, canvas.rawPixels(), cache->allocatedBytes());
            cache->commit();
        }
    }
}

void drawShowerFloats(Canvas565& canvas, const ShowerViewModel& model) {
    for (uint8_t i = 0; i < model.floatCount; ++i) {
        const auto& floater = model.floats[i];
        if (!floater.active) continue;
        const uint32_t age = model.nowMs - floater.bornMs;
        if (age >= 1100) continue;
        const int rise = age * 48 / 1100;
        const uint8_t alpha = age <= 500 ? 255 : static_cast<uint8_t>((1100 - age) * 255 / 600);
        if (floater.heartCount > 0) {
            const uint8_t count = std::min<uint8_t>(floater.heartCount, 3);
            for (uint8_t index = 0; index < count; ++index) {
                const int x = SHOWER_PET_X - 6 + index * 24 - (count - 1) * 12;
                const int y = 148 - rise - (index % 2) * 6;
                drawHeart(canvas, x, y,
                    rgb(242 * alpha / 255, 111 * alpha / 255, 126 * alpha / 255));
            }
        } else {
            char message[16];
            std::snprintf(message, sizeof(message), Ui::Shower::EXP_GAIN_FMT,
                          static_cast<unsigned>(floater.experience));
            text(canvas, 216, 244 - rise, message,
                 rgb(241 * alpha / 255, 242 * alpha / 255, 232 * alpha / 255));
        }
    }
}

void renderShowerPage(Canvas565& canvas, const ShowerViewModel& model,
                      PixelCache565* cache, PixelCache565* petCache,
                      uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd, model.columnBegin, model.columnEnd);
    if (rowBegin >= rowEnd) return;

    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    drawShowerStatic(canvas, model, cache, rowBegin, rowEnd);

    pageClip.setRect(0, UiMetrics::PAGE_HEADER_HEIGHT, AmoledUi::WIDTH,
                     328 - UiMetrics::PAGE_HEADER_HEIGHT);
    const RenderCacheKey petKey{model.speciesId, model.petFlip ? 1U : 0U,
        canvas.physicalWidth(), canvas.physicalHeight(), canvas.byteSwapped()};
    const uint16_t petTop = std::max<uint16_t>(rowBegin, UiMetrics::PAGE_HEADER_HEIGHT);
    const uint16_t petBottom = std::min<uint16_t>(rowEnd, 328);
    const bool petCached = model.petOffsetY == 0 && petCache &&
        petCache->copyRectTo(canvas, petKey, model.columnBegin, petTop, model.columnEnd, petBottom);
    if (!petCached) {
        drawShowerPet(canvas, model);
        if (model.petOffsetY == 0 && petCache && rowBegin == 0 && rowEnd == AmoledUi::HEIGHT &&
            model.columnBegin == 0 && model.columnEnd == AmoledUi::WIDTH) {
            if (auto* pixels = petCache->begin(petKey, Platform::memory())) {
                std::memcpy(pixels, canvas.rawPixels(), petCache->allocatedBytes());
                petCache->commit();
            }
        }
    }
    drawShowerBubbles(model);

    if (model.mode == ShowerMode::RINSING) {
        uint8_t alpha = static_cast<uint8_t>(70 + model.rinseProgress / 2);
        PixelRenderer::fillRectAlpha((36), (UiMetrics::PAGE_HEADER_HEIGHT),
                                     (296),
                                     (328 - UiMetrics::PAGE_HEADER_HEIGHT),
                                     rgb(74, 190, 232), alpha);
        for (int stream = 0; stream < 10; ++stream) {
            int x = 48 + stream * 30;
            int phase = (model.elapsedMs / 3 + stream * 17) % 88;
            canvas.drawFastVLine((x),
                                 ((UiMetrics::PAGE_HEADER_HEIGHT + phase)),
                                 (110), rgb(184, 236, 249));
        }
        for (uint8_t i = 0; i < model.waterCount; ++i) {
            const auto& drop = model.water[i];
            if (!drop.active) continue;
            canvas.drawFastVLine(static_cast<int>(drop.x), static_cast<int>(drop.y),
                drop.vy > 360 ? 16 : 10, rgb(220, 250, 255));
        }
        GameAssets::drawCentered(GameAssets::Kind::SHOWER_SPRINKLER,
                                 184, 106, 1.6f);
    }

    pageClip.reset();
    if (model.pressedItem >= 0) drawShowerToolbar(canvas, model);

    if (model.mode == ShowerMode::SOAPING ||
        model.mode == ShowerMode::BRUSHING) {
        GameAssets::Kind tool = model.mode == ShowerMode::SOAPING
            ? static_cast<GameAssets::Kind>(
                static_cast<uint16_t>(GameAssets::Kind::SHOWER_SOAP_0) +
                model.soapIndex)
            : GameAssets::Kind::SHOWER_BRUSH;
        GameAssets::drawCentered(tool, model.toolX, model.toolY,
                                 model.toolDragging ? 2.0f : 1.8f);
    }

    if (model.mode == ShowerMode::SOAP_SELECT) {
        drawShowerSoapPicker(canvas, model);
    } else if (model.mode == ShowerMode::COMPLETE) {
        bool rewardHeartsFloating = false;
        for (uint8_t index = 0; index < model.floatCount; ++index) {
            const auto& floater = model.floats[index];
            rewardHeartsFloating = rewardHeartsFloating ||
                (floater.active && floater.heartCount > 0 && model.nowMs - floater.bornMs < 1100);
        }
        if (!rewardHeartsFloating) {
            for (uint8_t index = 0; index < model.completionHearts; ++index) {
                drawHeart(canvas, 152 + index * 32, 124, rgb(242, 111, 126));
            }
        }
        text(canvas, 118, 290, Ui::Amoled::ALL_CLEAN,
             rgb(115, 226, 183));
    } else if (model.mode == ShowerMode::EXIT_CONFIRM) {
        drawShowerExitConfirm(canvas, model);
    }

    drawShowerFloats(canvas, model);
    drawToast(canvas, model.toast);
    pageClip.reset();
}
}  // namespace

void renderShowerScreen(Canvas565& canvas, const ShowerViewModel& model,
                        uint16_t rowBegin, uint16_t rowEnd) {
    renderShowerPage(canvas, model, nullptr, nullptr, rowBegin, rowEnd);
}

void renderShowerScreenCached(Canvas565& canvas, const ShowerViewModel& model,
                              PixelCache565& backgroundCache,
                              uint16_t rowBegin, uint16_t rowEnd,
                              PixelCache565* petCache) {
    renderShowerPage(canvas, model, &backgroundCache, petCache, rowBegin, rowEnd);
}

}  // namespace AmoledV1
