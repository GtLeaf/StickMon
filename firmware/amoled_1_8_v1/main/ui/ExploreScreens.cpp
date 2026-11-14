#include "game/ExploreDestinations.h"
#include "ui/common/MarketView.h"
#include "ui/ExploreScreens.h"
#if STICKMON_EXPLORE_POSTCARD
#include "ui/ExplorePostcardUi.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "assets/GameAssets.h"
#include "assets/MenuAssets.h"
#include "assets/PokemonMotion.h"
#include "assets/PokemonSprites.h"
#include "core/AppSceneFlow.h"
#include "core/FontResource.h"
#include "core/UiStrings.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreEcology.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "platform/api/FlashStorage.h"
#include "platform/api/PlatformServices.h"
#include "presentation/PixelRenderer.h"
#include "presentation/PokemonShadow.h"
#include "ui/ExploreMapRenderer.h"
#include "ui/RenderCaches.h"
#include "ui/UiCommon.h"
#include "ui/UiMetrics.h"

namespace AmoledV1 {

using UiCommon::rgb;
using UiCommon::text;
using UiCommon::textWidth;
using UiCommon::drawToast;
using UiCommon::drawSceneFadeOverlay;
using UiCommon::drawHomeHpBar;
using UiCommon::drawHomeHudButton;
using UiCommon::drawHudMenuIcon;
using UiCommon::blendShadowRgb565;
using UiCommon::fillSoftShadow;
using UiCommon::fillShadowCore;
using UiCommon::drawFallbackPet;

namespace {

constexpr int HEADER_HEIGHT = UiMetrics::HOME_HEADER_HEIGHT;
constexpr int HOME_STATUS_TOP = UiMetrics::HOME_STATUS_TOP;
constexpr int HOME_LOCK_BUTTON_X = UiMetrics::HOME_LOCK_BUTTON_X;
constexpr int HOME_MENU_BUTTON_X = UiMetrics::HOME_MENU_BUTTON_X;
constexpr int HOME_HUD_BUTTON_Y = UiMetrics::HOME_HUD_BUTTON_Y;
constexpr int HOME_HUD_BUTTON_SIZE = UiMetrics::HOME_HUD_BUTTON_SIZE;
constexpr int HOME_MONSTER_PANEL_X = UiMetrics::HOME_MONSTER_PANEL_X;
constexpr int HOME_MONSTER_PANEL_W = UiMetrics::HOME_MONSTER_PANEL_W;

constexpr int EXPLORE_MENU_PANEL_WIDTH = 120;
constexpr int EXPLORE_MENU_PANEL_ROW_HEIGHT = 60;
constexpr int EXPLORE_MENU_TEXT_INSET =
    (EXPLORE_MENU_PANEL_ROW_HEIGHT - FontResource::LARGE_GLYPH_H) / 2;
constexpr int EXPLORE_MENU_PANEL_HEIGHT =
    AppSceneFlow::exploreMenuItemCount() * EXPLORE_MENU_PANEL_ROW_HEIGHT;
constexpr int EXPLORE_MENU_PANEL_TOP =
    (HOME_STATUS_TOP - EXPLORE_MENU_PANEL_HEIGHT) / 2;

constexpr int EXPLORE_ROUTE_HUD_X = 8;
constexpr int EXPLORE_ROUTE_HUD_Y = 6;
constexpr int EXPLORE_ROUTE_HUD_HEIGHT = 46;
constexpr int EXPLORE_ROUTE_BAG_BUTTON_X = HOME_LOCK_BUTTON_X;
constexpr int EXPLORE_ROUTE_MENU_BUTTON_X = HOME_MENU_BUTTON_X;

constexpr uint32_t EXPLORE_PREVIEW_CYCLE_MS = 2800;
constexpr uint32_t EXPLORE_PREVIEW_MOVE_MS = 500;
constexpr uint32_t EXPLORE_PREVIEW_HOLD_MS =
    EXPLORE_PREVIEW_CYCLE_MS - EXPLORE_PREVIEW_MOVE_MS;
constexpr float EXPLORE_ROUTE_BOSS_SCALE = float(EXPLORE_ROUTE_SCALE);
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


void fillRoundRectAlpha(Canvas565& canvas, int x, int y, int width, int height,
                        int radius, uint16_t color, uint8_t alpha) {
    if (width <= 0 || height <= 0 || radius < 0 || alpha == 0) return;
    if (canvas.blendRoundRectAlphaNative(x, y, width, height, radius, color, alpha)) return;
    radius = std::min(radius, std::min(width, height) / 2);
    for (int py = y; py < y + height; ++py) {
        const int cornerY = py < y + radius ? y + radius
                            : py >= y + height - radius ? y + height - radius - 1
                                                         : py;
        const int dy = py - cornerY;
        int inset = 0;
        while (inset < radius &&
               (radius - inset) * (radius - inset) + dy * dy > radius * radius) {
            ++inset;
        }
        const int left = x + inset;
        const int span = width - inset * 2;
        for (int px = left; px < left + span; ++px) {
            uint16_t background = canvas.readPixel(
                (px), (py));
            canvas.drawPixel(
                (px), (py),
                blendShadowRgb565(background, color, alpha));
        }
    }
}


PokemonShadow::Anchor routeSpriteAnchor(const ExploreMapGenerator::Map* map,
                                        const PokemonSprites::SpriteFrame* frame,
                                        float worldX, float worldY, int cameraX, int cameraY,
                                        float scale, bool flipX) {
    const int x = exploreRouteScreen(worldX, cameraX);
    const int y = exploreRouteActorGroundScreenY(map, worldY, cameraY);
    return frame && ExploreRouteGeometry::hasValleyForestCenterline(map)
        ? PokemonShadow::spriteAnchorForShadow(frame, x, y, scale, flipX)
        : PokemonShadow::Anchor{x, y};
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
    int x = exploreRouteScreen(point.x, model.cameraX);
    int y = exploreRouteActorGroundScreenY(model.map, point.y, model.cameraY) - exploreRoutePixel(6);
    if (x < -26 || x >= AmoledUi::WIDTH + 26 ||
        y < EXPLORE_ROUTE_MAP_TOP - 26 ||
        y >= EXPLORE_ROUTE_MAP_BOTTOM + 26) return;

    canvas.fillEllipse(x, y + exploreRoutePixel(6),
        exploreRoutePixel(7), exploreRoutePixel(2), rgb(55, 68, 59));
    if (GameAssets::drawCentered(
            GameAssets::Kind::EXPLORE_PICKUP_BALL, x, y - exploreRoutePixel(4),
            EXPLORE_ROUTE_SCALE, true)) {
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
        canvas.drawFastHLine(x - halfWidth, y - 4 + row, halfWidth * 2 + 1, 0xFFFF);
    }
    canvas.drawCircle(x, y - 4, 7, rgb(35, 39, 44));
    canvas.drawFastHLine(x - 7, y - 4, 14, rgb(35, 39, 44));
    canvas.fillCircle(x, y - 4, 2, 0xFFFF);
    canvas.drawCircle(x, y - 4, 2, rgb(35, 39, 44));
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
                          uint16_t rowBegin, uint16_t rowEnd,
                          AmoledUi::Rect* boundsOnly = nullptr) {
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
    const auto [centerX, groundCenterY] = routeSpriteAnchor(
        model.map, frame, point.x, point.y, model.cameraX, model.cameraY, scale, flipX);
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

    const auto shadow = PokemonShadow::atGround(frame, centerX, groundCenterY, scale, flipX);
    if (boundsOnly) {
        const int left = std::min(x, shadow.x - shadow.radiusX) - 1;
        const int top = std::min(y, shadow.y - shadow.radiusY) - 1;
        *boundsOnly = {left, top,
            std::max(x + width, shadow.x + shadow.radiusX + 1) + 1 - left,
            std::max(y + height, shadow.y + shadow.radiusY + 1) + 1 - top};
        return;
    }
    drawExploreRouteShadow(canvas, frame, centerX, spriteGroundTopY,
                           width, height, scale, flipX);
    PokemonSprites::drawFrameScaled(frame, x, y, scale, flipX, true);
    drawExploreRouteMapVegetationForActor(
        canvas, model, exploreRouteActorFootWorldY(model.map, point.y), std::min(x, shadow.x - shadow.radiusX),
        std::min(y, shadow.y - shadow.radiusY),
        std::max(x + width, shadow.x + shadow.radiusX + 1),
        std::max(y + height, shadow.y + shadow.radiusY + 1), rowBegin, rowEnd);
}

AmoledUi::Rect exploreActorBounds(uint16_t speciesId, float worldX, float worldY,
                                  uint8_t walkDirection, uint8_t petFrame,
                                  bool walking, uint32_t animationNowMs,
                                  int16_t cameraX, int16_t cameraY,
                                  const ExploreMapGenerator::Map* map) {
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

    constexpr float scale = EXPLORE_ROUTE_SCALE;
    const auto [x, ground] = routeSpriteAnchor(map, frame, worldX, worldY, cameraX, cameraY, scale, flipX);
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

    constexpr float scale = static_cast<float>(EXPLORE_ROUTE_SCALE);
    const auto [screenX, routeGroundY] = routeSpriteAnchor(
        model.map, frame, worldX, worldY, cameraX, cameraY, scale, flipX);
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
            canvas, model, exploreRouteActorFootWorldY(model.map, worldY), screenX - 62, routeGroundY - 154,
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
        frame, screenX - width / 2, drawY, scale, flipX, true);
    if (!spriteDrawn) {
        drawFallbackPet(canvas, screenX, routeGroundY);
    }
    const auto shadow = PokemonShadow::atGround(frame, screenX, routeGroundY, scale, flipX);
    drawExploreRouteMapVegetationForActor(
        canvas, model, exploreRouteActorFootWorldY(model.map, worldY),
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

constexpr float WILDLIFE_SCALE = 0.8f * EXPLORE_ROUTE_SCALE;

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
    const int x = exploreRouteScreen(pose.x, model.cameraX);
    const int ground = exploreRouteScreen(pose.y, model.cameraY);
    if (pose.hidden) return {x - exploreRoutePixel(16), ground - exploreRoutePixel(17),
        exploreRoutePixel(33), exploreRoutePixel(24)};
    bool flipX = false;
    const auto* frame = wildlifeFrame(pose, flipX);
    if (!frame) return {x, ground, 0, 0};
    const int width = int(std::lround(FlashStorage::readByte(&frame->width) * WILDLIFE_SCALE));
    const int height = int(std::lround(FlashStorage::readByte(&frame->height) * WILDLIFE_SCALE));
    const int offset = int(std::lround(PokemonSprites::frameGroundOffsetY(frame) * WILDLIFE_SCALE));
    const int bodyX = x + exploreRoutePixel(pose.sway);
    const int bodyTop = ground - height / 2 - offset - exploreRoutePixel(pose.lift);
    const int bodyBottom = bodyTop + height;
    const auto shadow = PokemonShadow::atGround(frame, bodyX, ground, WILDLIFE_SCALE, flipX);
    const int left = std::min({bodyX - width / 2, x - exploreRoutePixel(26), shadow.x - shadow.radiusX}) - 1;
    const int top = std::min({bodyTop - exploreRoutePixel(14), ground - exploreRoutePixel(19), shadow.y - shadow.radiusY}) - 1;
    const int right = std::max({bodyX - width / 2 + width, x + exploreRoutePixel(27), shadow.x + shadow.radiusX + 1}) + 1;
    const int bottom = std::max({bodyBottom, ground + exploreRoutePixel(7), shadow.y + shadow.radiusY + 1}) + 1;
    return {left, top, right - left, bottom - top};
}

void drawWildlifeRipple(Canvas565& canvas, int x, int y, int radius,
                        uint16_t color) {
    constexpr int segments = 20;
    int previousX = x + radius, previousY = y;
    for (int i = 1; i <= segments; ++i) {
        const float angle = i * 6.2831853f / segments;
        const int nextX = x + int(std::lround(std::cos(angle) * radius));
        const int nextY = y + int(std::lround(std::sin(angle) * exploreRoutePixel(2)));
        canvas.drawLine(previousX, previousY, nextX, nextY, color);
        previousX = nextX;
        previousY = nextY;
    }
}

void drawWildlifeHabitatHint(Canvas565& canvas, const ExploreEcology::Pose& pose,
                             int x, int ground) {
    const uint8_t phase = pose.frame % 3;
    if (pose.surface == ExploreEcology::Surface::WATER) {
        drawWildlifeRipple(canvas, x, ground, 5 + phase * 2, rgb(171, 217, 225));
        if (phase) canvas.fillCircle(x + 2, ground - 4, 1, rgb(195, 233, 239));
    } else if (pose.surface == ExploreEcology::Surface::ROCK ||
               pose.surface == ExploreEcology::Surface::ICE) {
        const bool ice = pose.surface == ExploreEcology::Surface::ICE;
        const uint16_t color = ice ? rgb(183, 219, 231) : rgb(146, 137, 125);
        const int spread = phase * 2;
        canvas.fillEllipse(x, ground + 1, 5 + spread, 1, color);
        if (phase) {
            canvas.fillCircle(x - 3 - spread, ground - 2, 1, color);
            canvas.fillCircle(x + 3 + spread, ground - 4, 1, color);
        }
    } else {
        const int bend = phase == 1 ? -2 : (phase == 2 ? 2 : 0);
        for (int blade = -1; blade <= 1; ++blade) {
            canvas.drawLine(x + blade * 3, ground + 1,
                x + blade * 5 + bend, ground - 5 - (blade == 0 ? 2 : 0),
                rgb(68, 126, 75));
            canvas.drawLine(x + blade * 3 + 1, ground + 1,
                x + blade * 5 + bend + 1, ground - 4 - (blade == 0 ? 2 : 0),
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
    const int x = exploreRouteScreen(pose.x, model.cameraX);
    const int ground = exploreRouteScreen(pose.y, model.cameraY);
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
        const int top = baseTop - exploreRoutePixel(pose.lift);
        const int bodyX = x + exploreRoutePixel(pose.sway);
        if (pose.airborne) {
            const auto shadow = PokemonShadow::atGround(
                frame, bodyX, ground, WILDLIFE_SCALE, flipX);
            fillSoftShadow(canvas, shadow.x, shadow.y, shadow.radiusX,
                           shadow.radiusY, rgb(62, 83, 83), 68);
        }
        else if (pose.surface == ExploreEcology::Surface::WATER)
            drawWildlifeRipple(canvas, x, ground, exploreRoutePixel(9), rgb(144, 201, 217));
        else drawExploreRouteShadow(canvas, frame, bodyX, baseTop, width, height, WILDLIFE_SCALE, flipX);
        PokemonSprites::drawFrameScaled(frame, bodyX - width / 2, top, WILDLIFE_SCALE, flipX, true);
        if (pose.activity == ExploreEcology::Activity::DOZE) {
            // A small pixel Z moves with the breathing pose.
            const int zx = bodyX + exploreRoutePixel(8), zy = top - exploreRoutePixel(9);
            const uint16_t color = rgb(208, 226, 231);
            canvas.drawFastHLine(zx, zy, 6, color);
            canvas.drawLine(zx + 5, zy, zx, zy + 5, color);
            canvas.drawFastHLine(zx, zy + 5, 6, color);
        }
        if (pose.activity == ExploreEcology::Activity::SPLASH ||
            pose.activity == ExploreEcology::Activity::GATHER) {
            drawWildlifeRipple(canvas, x, ground, exploreRoutePixel(9 + pose.frame % 3 * 2), rgb(185, 224, 233));
            if (pose.activity == ExploreEcology::Activity::SPLASH) {
                const int spread = exploreRoutePixel(6 + pose.frame % 3 * 2);
                canvas.fillCircle(x - spread, ground - 4, 1, rgb(194, 231, 239));
                canvas.fillCircle(x + spread, ground - 6, 1, rgb(194, 231, 239));
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
        {ExploreTileSemantics::ActorRole::LEADER, exploreRouteActorFootWorldY(model.map, model.worldY)}};
    uint8_t actorCount = 1;
    if (model.companionVisible && model.companionSpeciesId != 0) {
        actors[actorCount++] = {
            ExploreTileSemantics::ActorRole::COMPANION,
            exploreRouteActorFootWorldY(model.map, model.companionWorldY)};
    }
    if (model.bossPending && model.bossSpeciesId != 0 && model.map &&
        model.pathIndex < model.map->pathCount) {
        const ExploreMapGenerator::Path& path = model.map->paths[model.pathIndex];
        if (model.bossIndex < path.pointCount) {
            actors[actorCount++] = {ExploreTileSemantics::ActorRole::BOSS,
                exploreRouteActorFootWorldY(model.map, exploreRouteBossWorldPose(
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
        model.cameraX, model.cameraY, model.map);
    if (model.companionVisible && model.companionSpeciesId != 0) {
        bounds = unionExploreBounds(bounds, exploreActorBounds(
            model.companionSpeciesId, model.companionWorldX, model.companionWorldY,
            model.companionWalkDirection, model.companionFrame, model.companionWalking,
            model.animationNowMs, model.cameraX, model.cameraY, model.map));
    }
    return bounds;
}

// Explore area, route, and explore menu rendering.
int exploreAreaAt(int x, int y, uint8_t selectedArea,
                  uint8_t visibleAreaCount) {
    if constexpr (EXPLORE_POSTCARD) {
        if (y<146 || y>=206) return -1;
        int target = x>=10 && x<64 ? int(selectedArea)-1 : x>=304 && x<358 ? int(selectedArea)+1 : -1;
        return target>=0 && target<visibleAreaCount ? target : -1;
    }
    if (y < 0 || y >= EXPLORE_SELECTOR_TOP_HEIGHT) return -1;
    int count = std::min<int>(visibleAreaCount, ExploreDestinations::COUNT);
    if (count <= 0) return -1;
    int index = selectedArea + static_cast<int>(std::lround(
        (x - EXPLORE_SELECTOR_CENTER_X) /
        static_cast<float>(EXPLORE_SELECTOR_AREA_SPACING)));
    if (index < 0 || index >= count) return -1;
    int expectedX = EXPLORE_SELECTOR_CENTER_X +
        (index - static_cast<int>(selectedArea)) *
            EXPLORE_SELECTOR_AREA_SPACING;
    return std::abs(x - expectedX) <= 68 ? index : -1;
}

bool exploreStartAt(int x, int y) {
    if constexpr (EXPLORE_POSTCARD) return AmoledUi::Rect{20,388,328,48}.contains(x,y);
    return EXPLORE_START_BUTTON_NATIVE.contains(x, y);
}

bool exploreSelectionBackAt(int x, int y) {
    if constexpr (EXPLORE_POSTCARD) return UiCommon::pageHeaderBackAt(x,y);
    return EXPLORE_BACK_BUTTON_NATIVE.contains(x, y);
}

void drawExploreNativeButtonLabel(Canvas565& canvas,
                                  const AmoledUi::Rect& button,
                                  const char* label, uint16_t color) {
    const int labelWidth = textWidth(label);
    PixelRenderer::text(
        canvas, button.x + (button.width - labelWidth) / 2, button.y + 8,
        label, color, 1);
}

void renderExploreLegacyScreen(Canvas565& canvas, const ExploreViewModel& model,
                         PixelCache565& backgroundCache,
                         uint16_t rowBegin, uint16_t rowEnd) {
    const uint16_t ink = rgb(226, 238, 233);
    const uint16_t muted = rgb(137, 155, 158);
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
    drawExploreBackgroundLayer(canvas, backgroundCache, rowBegin, rowEnd);
    pageClip.reset();

    const int count = std::min<int>(model.visibleAreaCount,
                                    model.destinationMenu ? ExploreDestinations::COUNT : Game::EXPLORE_AREA_COUNT);
    const int centerX = EXPLORE_SELECTOR_CENTER_X;
    const int selectorBottom = EXPLORE_SELECTOR_TOP_HEIGHT;
    const int buttonTop = EXPLORE_SELECTOR_BUTTON_TOP;
    const bool selectedLocked = model.destinationMenu ? !model.destinationUnlocked[model.selectedArea] : model.selectedArea > model.unlockedArea;

    if (rowBegin < selectorBottom) {
        int top = rowBegin;
        int bottom = std::min<int>(rowEnd, selectorBottom);
        pageClip.setRect((0),
                           (top),
                           (AmoledUi::WIDTH),
                           (bottom - top));
        if (count > 0) {
            for (int index = 0; index < count; ++index) {
                float offset = static_cast<float>(index) -
                               model.areaAnimCursor;
                if (std::fabs(offset) > 1.35f) continue;
                int x = centerX + static_cast<int>(std::lround(
                    offset * EXPLORE_SELECTOR_AREA_SPACING));
                bool active = std::fabs(offset) < 0.5f;
                uint16_t color = active ? ink : muted;
                const auto id = model.destinationMenu ? model.destinationIds[index] : index;
                const char* name = id == ExploreDestinations::MARKET ? Ui::MarketView::NAME : EXPLORE_AREA_NAMES[id < Game::EXPLORE_AREA_COUNT ? id : 0];
                text(canvas, x - textWidth(name) / 2, 44, name, color);
                if (active) {
                    canvas.drawFastHLine(
                        (x - 44),
                        (112),
                        (88),
                        ink);
                }
            }
        }
        canvas.drawFastHLine((16),
                             (selectorBottom - 2),
                             (336),
                             rgb(77, 105, 106));
        pageClip.reset();
    }

    if (rowEnd > selectorBottom && rowBegin < buttonTop) {
        int top = std::max<int>(rowBegin, selectorBottom);
        int bottom = std::min<int>(rowEnd, buttonTop);
        pageClip.setRect((0),
                           (top),
                           (AmoledUi::WIDTH),
                           (bottom - top));
        const int previewCenterX = AmoledUi::WIDTH / 2;
    const char* title = model.marketSelected ? Ui::MarketView::NAME : selectedLocked
        ? Ui::Explore::AREA_LOCKED
        : model.previewPool.count > 0 && ExplorePool::poolHasRare(
              model.previewPool)
            ? Ui::Explore::MASS_OUTBREAK
            : Ui::Explore::HABITAT_MONSTERS;
    text(canvas, previewCenterX - textWidth(title) / 2, selectorBottom + 16,
         title, selectedLocked ? rgb(137, 155, 158) : ink);

    pageClip.setRect((0),
                       ((std::max(top, EXPLORE_PREVIEW_TOP))),
                       (AmoledUi::WIDTH),
                       ((std::max(0,
                           std::min(bottom, EXPLORE_PREVIEW_BOTTOM) -
                           std::max(top, EXPLORE_PREVIEW_TOP)))));
    if (model.marketSelected) {
        char countdown[64];
        const char* message = Ui::MarketText::OPEN_TODAY;
        if (model.marketDaysUntilOpen > 0) {
            std::snprintf(countdown, sizeof(countdown), Ui::MarketText::NEXT_OPEN,
                          static_cast<unsigned long>(model.marketDaysUntilOpen));
            message = countdown;
        }
        text(canvas, previewCenterX - textWidth(message) / 2,
             EXPLORE_PREVIEW_CENTER_Y - 16, message, selectedLocked ? muted : ink);
    } else if (selectedLocked) {
        const char* message = Ui::Explore::DEFEAT_PREVIOUS_BOSS;
        text(canvas, previewCenterX - textWidth(message) / 2,
             EXPLORE_PREVIEW_CENTER_Y - 16, message, muted);
    } else if (model.previewPool.count > 0) {
        uint8_t poolCount = model.previewPool.count;
        uint32_t elapsed = Platform::clock().millis() -
                           model.previewStartedAt;
        uint8_t current = static_cast<uint8_t>(
            (elapsed / EXPLORE_PREVIEW_CYCLE_MS + 1) % poolCount);
        uint32_t cycleElapsed = elapsed % EXPLORE_PREVIEW_CYCLE_MS;
        float progress = cycleElapsed <= EXPLORE_PREVIEW_HOLD_MS
            ? 0.0f
            : (cycleElapsed - EXPLORE_PREVIEW_HOLD_MS) /
                  static_cast<float>(EXPLORE_PREVIEW_MOVE_MS);
        progress = std::min(1.0f, progress);
        progress = progress * progress * (3.0f - 2.0f * progress);

        const int previewCenterNativeX =
            (previewCenterX);
        if (poolCount == 1) {
            drawExplorePreviewSlot(canvas, model, current, previewCenterNativeX);
        } else {
            // Keep both edge members until they have fully crossed the clip.
            // All slots share one translation, including at cycle rollover.
            const int translation = static_cast<int>(std::lround(
                progress * EXPLORE_PREVIEW_SLOT_SPACING));
            for (int slot = -1; slot <= 2; ++slot) {
                const int x = previewCenterNativeX +
                    slot * EXPLORE_PREVIEW_SLOT_SPACING - translation;
                if (x + EXPLORE_PREVIEW_NATIVE_MAX_WIDTH / 2 <= 0 ||
                    x - EXPLORE_PREVIEW_NATIVE_MAX_WIDTH / 2 >= AmoledUi::WIDTH) {
                    continue;
                }
                const uint8_t index = static_cast<uint8_t>(
                    (current + poolCount + slot) % poolCount);
                drawExplorePreviewSlot(canvas, model, index, x);
            }
        }
    }
    pageClip.reset();

    }

    if (rowEnd > buttonTop && rowBegin < AmoledUi::HEIGHT) {
        int top = std::max<int>(rowBegin, buttonTop);
        pageClip.setRect((0),
                           (top),
                           (AmoledUi::WIDTH),
                           (rowEnd - top));
        canvas.drawFastHLine((16),
                             EXPLORE_SELECTOR_DIVIDER_NATIVE_Y,
                             (336),
                             rgb(77, 105, 106));
        const uint16_t startFill = selectedLocked
            ? rgb(78, 91, 96) : rgb(42, 61, 68);
        canvas.fillRoundRect(EXPLORE_START_BUTTON_NATIVE.x,
                             EXPLORE_START_BUTTON_NATIVE.y,
                             EXPLORE_START_BUTTON_NATIVE.width,
                             EXPLORE_START_BUTTON_NATIVE.height, 8, startFill);
        canvas.drawRoundRect(EXPLORE_START_BUTTON_NATIVE.x,
                             EXPLORE_START_BUTTON_NATIVE.y,
                             EXPLORE_START_BUTTON_NATIVE.width,
                             EXPLORE_START_BUTTON_NATIVE.height, 8,
                             selectedLocked ? rgb(78, 91, 96)
                                            : rgb(115, 226, 183));
        canvas.fillRoundRect(EXPLORE_BACK_BUTTON_NATIVE.x,
                             EXPLORE_BACK_BUTTON_NATIVE.y,
                             EXPLORE_BACK_BUTTON_NATIVE.width,
                             EXPLORE_BACK_BUTTON_NATIVE.height, 8,
                             rgb(27, 43, 51));
        canvas.drawRoundRect(EXPLORE_BACK_BUTTON_NATIVE.x,
                             EXPLORE_BACK_BUTTON_NATIVE.y,
                             EXPLORE_BACK_BUTTON_NATIVE.width,
                             EXPLORE_BACK_BUTTON_NATIVE.height, 8,
                             rgb(67, 97, 101));
        drawExploreNativeButtonLabel(
            canvas, EXPLORE_START_BUTTON_NATIVE,
            model.marketSelected && selectedLocked ? Ui::MarketText::CLOSED : Ui::Explore::DEPART,
            selectedLocked ? rgb(126, 145, 145) : rgb(226, 238, 233));
        drawExploreNativeButtonLabel(canvas, EXPLORE_BACK_BUTTON_NATIVE,
                                     Ui::BACK, rgb(226, 238, 233));
        pageClip.reset();
    }
    drawToast(canvas, model.toast);
    pageClip.reset();
}

void renderExploreScreen(Canvas565& canvas, const ExploreViewModel& model,
                         PixelCache565& cache,uint16_t rowBegin,uint16_t rowEnd) {
#if STICKMON_EXPLORE_POSTCARD
    UiCommon::PageClip clip(canvas,rowBegin,rowEnd);
    if(rowBegin>=rowEnd)return;
    renderExplorePostcard(canvas,model,rowBegin,rowEnd);
    drawToast(canvas,model.toast);
#else
    renderExploreLegacyScreen(canvas,model,cache,rowBegin,rowEnd);
#endif
}

bool exploreRouteBackAt(int x, int y) {
    (void)x;
    (void)y;
    return false;
}

bool exploreRouteBagAt(int x, int y) {
    return x >= EXPLORE_ROUTE_BAG_BUTTON_X - 8 &&
           x < EXPLORE_ROUTE_BAG_BUTTON_X + HOME_HUD_BUTTON_SIZE + 8 &&
           y >= HOME_STATUS_TOP && y < AmoledUi::HEIGHT;
}

bool exploreRouteMenuAt(int x, int y) {
    return x >= EXPLORE_ROUTE_MENU_BUTTON_X - 8 &&
           x < EXPLORE_ROUTE_MENU_BUTTON_X + HOME_HUD_BUTTON_SIZE + 8 &&
           y >= HOME_STATUS_TOP && y < AmoledUi::HEIGHT;
}

int exploreRouteExitChoiceAt(int x, int y) {
    if (y < 334 || y >= 408) return -1;
    if (x >= 36 && x < 176) return 0;
    if (x >= 192 && x < 332) return 1;
    return -1;
}

bool exploreRouteMapAt(int x, int y) {
    return x >= 0 && x < AmoledUi::WIDTH &&
           y >= EXPLORE_ROUTE_MAP_TOP && y < EXPLORE_ROUTE_MAP_BOTTOM;
}

void drawExploreRouteHud(Canvas565& canvas,
                         const ExploreRouteViewModel& model) {
    const uint16_t panelColor = rgb(8, 17, 22);
    const char* name = EXPLORE_AREA_NAMES[
        model.area < Game::EXPLORE_AREA_COUNT ? model.area : 0];
    const int namePanelWidth = std::min(
        AmoledUi::WIDTH - EXPLORE_ROUTE_HUD_X * 2,
        textWidth(name) + 14);
    fillRoundRectAlpha(canvas, EXPLORE_ROUTE_HUD_X, EXPLORE_ROUTE_HUD_Y,
                       namePanelWidth, EXPLORE_ROUTE_HUD_HEIGHT, 14,
                       panelColor, 168);
    canvas.drawRoundRect(
        (EXPLORE_ROUTE_HUD_X),
        (EXPLORE_ROUTE_HUD_Y),
        (namePanelWidth),
        (EXPLORE_ROUTE_HUD_HEIGHT),
        (14), rgb(87, 121, 119));
    text(canvas, EXPLORE_ROUTE_HUD_X + 14, EXPLORE_ROUTE_HUD_Y + 6,
         name, rgb(226, 238, 233));
}

void drawExploreRouteBottomHud(Canvas565& canvas,
                               const ExploreRouteViewModel& model) {
    canvas.fillRect(
        (0),
        (HOME_STATUS_TOP),
        (AmoledUi::WIDTH),
        (AmoledUi::HEIGHT - HOME_STATUS_TOP),
        rgb(0, 0, 0));
    canvas.drawFastHLine(
        (0),
        (HOME_STATUS_TOP),
        (AmoledUi::WIDTH), rgb(71, 108, 108));
    drawExploreBagIcon(canvas);
    drawExploreMenuIcon(canvas);

    canvas.fillRoundRect(
        (HOME_MONSTER_PANEL_X),
        (352),
        (HOME_MONSTER_PANEL_W),
        (88), (8),
        rgb(0, 0, 0));
    canvas.drawRoundRect(
        (HOME_MONSTER_PANEL_X),
        (352),
        (HOME_MONSTER_PANEL_W),
        (88), (8),
        rgb(72, 83, 98));

    uint8_t count = model.state
        ? std::min<uint8_t>(model.state->teamCount, 2) : 0;
    for (uint8_t index = 0; index < count; ++index) {
        const Game::MonsterRuntime& monster = model.state->team[index];
        uint8_t percent = monster.hpMax == 0 ? 0
            : static_cast<uint8_t>(std::min<uint32_t>(
                100, static_cast<uint32_t>(monster.hpCur) * 100 /
                    monster.hpMax));
        const int rowY = 354 + index * 42;
        drawHomeHpBar(canvas, HOME_MONSTER_PANEL_X + 14, rowY + 14,
                      98, percent);
        GameAssets::Kind status = GameAssets::statusKind(monster.majorStatus);
        if (status != GameAssets::Kind::COUNT) {
            GameAssets::draw(status, HOME_MONSTER_PANEL_X + 126, rowY + 2, 2.0f);
        }
    }
}

void renderExploreRouteScreen(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& mapCache,
                              uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd,
                                model.renderColumnBegin, model.renderColumnEnd);
    if (rowBegin >= rowEnd) return;

    if (rowBegin < EXPLORE_ROUTE_MAP_BOTTOM &&
        rowEnd > EXPLORE_ROUTE_MAP_TOP) {
        int top = std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP);
        int bottom = std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        ExploreRouteTonePass tonePass;
        if (model.map) {
            if (model.daylight == ExploreDaylight::Tone{128, 152, 192}) {
                tonePass.include(model, exploreRouteActorBounds(model));
                for (uint8_t i = 0; i < model.wildlifeCount && i < ExploreEcology::CAPACITY; ++i)
                    tonePass.include(model, wildlifeBounds(model, i));
                AmoledUi::Rect bossBounds{};
                drawExploreRouteBoss(canvas, model, top, bottom, &bossBounds);
                tonePass.include(model, bossBounds);
                if (model.pickupAvailable && model.pathIndex < model.map->pathCount &&
                    model.pickupIndex < model.map->paths[model.pathIndex].pointCount) {
                    const auto point = ExploreRouteGeometry::pathPoint(
                        model.map->paths[model.pathIndex], model.pickupIndex);
                    const int x = exploreRouteScreen(point.x, model.cameraX);
                    const int y = exploreRouteActorGroundScreenY(model.map, point.y, model.cameraY) -
                        exploreRoutePixel(6);
                    tonePass.include(model, {x - 40, y - 48, 80, 72});
                }
            }
            drawExploreRouteMapLayer(canvas, model, mapCache, top, bottom, &tonePass);
            drawExploreRoutePickup(canvas, model);
            if (model.map->areaIndex == ExploreMapGenerator::TALL_GRASS_PARK_AREA ||
                model.map->areaIndex == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA ||
                model.map->areaIndex == ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA ||
                model.wildlifeCount > 0) {
                drawExploreRouteParkActors(canvas, model, top, bottom);
            } else {
                bool bossBehindPlayer = exploreRouteBossBehindPlayer(model);
                if (bossBehindPlayer) drawExploreRouteBoss(canvas, model, top, bottom);
                if (model.companionVisible &&
                    model.companionWorldY <= model.worldY) {
                    drawExploreRouteCompanion(canvas, model, top, bottom);
                }
                drawExploreRoutePet(canvas, model, top, bottom);
                if (model.companionVisible &&
                    model.companionWorldY > model.worldY) {
                    drawExploreRouteCompanion(canvas, model, top, bottom);
                }
                if (!bossBehindPlayer) drawExploreRouteBoss(canvas, model, top, bottom);
            }
        } else {
            canvas.fillRect(
                (0),
                (EXPLORE_ROUTE_MAP_TOP),
                (AmoledUi::WIDTH),
                ((EXPLORE_ROUTE_MAP_BOTTOM -
                                       EXPLORE_ROUTE_MAP_TOP)),
                ExploreAreaCatalog::fieldColor(model.area));
        }
        if (tonePass.active) tonePass.finish(canvas, model);
        else canvas.multiplyRectRgb(0, top, AmoledUi::WIDTH, bottom - top,
                model.daylight.red, model.daylight.green, model.daylight.blue);
        pageClip.reset();
    }

    if (model.complete && rowBegin < 268 && rowEnd > 136) {
        int top = std::max<int>(rowBegin, 136);
        int bottom = std::min<int>(rowEnd, 268);
        pageClip.setRect(
            (0), (top),
            (AmoledUi::WIDTH),
            (bottom - top));
        canvas.fillRoundRect(
            (40), (136),
            (288), (132),
            (12), rgb(17, 27, 34));
        canvas.drawRoundRect(
            (40), (136),
            (288), (132),
            (12), rgb(82, 117, 117));
        text(canvas,
             (AmoledUi::WIDTH -
              textWidth(Ui::Explore::RESULT_END)) / 2,
             160, Ui::Explore::RESULT_END, rgb(248, 210, 105));
        text(canvas,
             (AmoledUi::WIDTH -
              textWidth(Ui::Explore::ANY_KEY_RETURN)) / 2,
             212, Ui::Explore::ANY_KEY_RETURN, rgb(226, 238, 233));
        pageClip.reset();
    }

    if (rowBegin < EXPLORE_ROUTE_HUD_Y + EXPLORE_ROUTE_HUD_HEIGHT &&
        rowEnd > EXPLORE_ROUTE_HUD_Y) {
        int top = std::max<int>(rowBegin, EXPLORE_ROUTE_HUD_Y);
        int bottom = std::min<int>(
            rowEnd, EXPLORE_ROUTE_HUD_Y + EXPLORE_ROUTE_HUD_HEIGHT);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        drawExploreRouteHud(canvas, model);
        pageClip.reset();
    }

    if (rowEnd > HOME_STATUS_TOP) {
        int top = std::max<int>(rowBegin, HOME_STATUS_TOP);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (rowEnd - top));
        drawExploreRouteBottomHud(canvas, model);
        pageClip.reset();
    }

    if (model.exitConfirm &&
        rowBegin < 428 && rowEnd > 290) {
        int top = std::max<int>(rowBegin, 290);
        int bottom = std::min<int>(rowEnd, 428);
        pageClip.setRect((0), (top), (AmoledUi::WIDTH), (bottom - top));
        if (model.exitConfirm) {
            canvas.fillRoundRect((20), (290), (328), (138), (12), rgb(17, 27, 34));
            canvas.drawRoundRect((20), (290), (328), (138), (12), rgb(82, 117, 117));
            text(canvas, 98, 308, Ui::Amoled::LEAVE_ROUTE,
                 rgb(226, 238, 233));
            canvas.fillRoundRect((36), (334), (140), (74), (8), rgb(36, 54, 61));
            canvas.fillRoundRect((192), (334), (140), (74), (8), rgb(91, 49, 55));
            text(canvas, 72, 364, Ui::Amoled::STAY, rgb(115, 226, 183));
            text(canvas, 234, 364, Ui::Amoled::EXIT, rgb(239, 143, 148));
        }
        pageClip.reset();
    }

    if (model.toast && rowBegin < UiCommon::TOAST_BOTTOM &&
        rowEnd > UiCommon::TOAST_TOP) {
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
        drawToast(canvas, model.toast);
        pageClip.reset();
    }
    drawSceneFadeOverlay(canvas, model.fadeAlpha, rowBegin, rowEnd);
}

bool exploreRouteMenuBackAt(int x, int y) {
    return x >= 0 && x < 60 && y >= 0 && y < HEADER_HEIGHT;
}

int exploreRouteMenuItemAt(int x, int y) {
    const int panelX = AmoledUi::WIDTH - EXPLORE_MENU_PANEL_WIDTH;
    if (x < panelX || x >= AmoledUi::WIDTH ||
        y < EXPLORE_MENU_PANEL_TOP ||
        y >= EXPLORE_MENU_PANEL_TOP + EXPLORE_MENU_PANEL_HEIGHT) {
        return -1;
    }
    int index = (y - EXPLORE_MENU_PANEL_TOP) /
                EXPLORE_MENU_PANEL_ROW_HEIGHT;
    return index < AppSceneFlow::exploreMenuItemCount() ? index : -1;
}

void renderExploreMenuScreen(Canvas565& canvas,
                             const ExploreMenuViewModel& model,
                             uint16_t rowBegin, uint16_t rowEnd) {
    rowBegin = std::min<uint16_t>(rowBegin, AmoledUi::HEIGHT);
    rowEnd = std::min<uint16_t>(rowEnd, AmoledUi::HEIGHT);
    UiCommon::PageClip pageClip(canvas, rowBegin, rowEnd);
    if (rowBegin >= rowEnd) return;

    const int panelX = AmoledUi::WIDTH - EXPLORE_MENU_PANEL_WIDTH;
    pageClip.setRect((panelX), (rowBegin), (EXPLORE_MENU_PANEL_WIDTH), (rowEnd - rowBegin));
    canvas.fillRoundRect(panelX, EXPLORE_MENU_PANEL_TOP,
                         EXPLORE_MENU_PANEL_WIDTH, EXPLORE_MENU_PANEL_HEIGHT,
                         8, rgb(20, 25, 32));
    canvas.drawRoundRect(panelX, EXPLORE_MENU_PANEL_TOP,
                         EXPLORE_MENU_PANEL_WIDTH, EXPLORE_MENU_PANEL_HEIGHT,
                         8, rgb(190, 200, 205));

    for (uint8_t index = 0; index < AppSceneFlow::exploreMenuItemCount();
         ++index) {
        int y = EXPLORE_MENU_PANEL_TOP +
                index * EXPLORE_MENU_PANEL_ROW_HEIGHT;
        if (y + EXPLORE_MENU_PANEL_ROW_HEIGHT <= rowBegin || y >= rowEnd) {
            continue;
        }
        bool selected = index == model.cursor;
        bool pressed = index == model.pressedItem;
        if (pressed) {
            canvas.fillRoundRect(panelX + 4, y + 4,
                                 EXPLORE_MENU_PANEL_WIDTH - 8,
                                 EXPLORE_MENU_PANEL_ROW_HEIGHT - 8,
                                 4, rgb(33, 43, 48));
        }
        if (selected) {
            canvas.fillRect(panelX + 10,
                            y + (EXPLORE_MENU_PANEL_ROW_HEIGHT - 36) / 2,
                            6, 36, rgb(255, 216, 72));
        }
        uint16_t color = selected ? rgb(255, 216, 72)
                                  : rgb(235, 239, 232);
        text(canvas, panelX + 26, y + EXPLORE_MENU_TEXT_INSET,
             Ui::Explore::SIDE_MENU_ITEMS[index], color);
    }
    pageClip.reset();
    if (model.toast && rowBegin < UiCommon::TOAST_BOTTOM &&
        rowEnd > UiCommon::TOAST_TOP) {
        pageClip.setRect((0), (rowBegin), (AmoledUi::WIDTH), (rowEnd - rowBegin));
        drawToast(canvas, model.toast);
        pageClip.reset();
    }
}

}  // namespace AmoledV1
