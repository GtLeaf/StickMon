#include "ui/common/ExploreRouteUiContent.h"
#include "ui/common/MarketView.h"
#include "ui/common/ExploreRouteOverlay.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include "assets/GameAssets.h"
#include "assets/PokemonMotion.h"
#include "core/AppSceneFlow.h"
#include "core/MathUtil.h"
#include "core/UiStrings.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreEditorTiles.h"
#include "game/ExploreTileSemantics.h"
#include "platform/api/PlatformServices.h"
#include "presentation/ExploreRouteMotion.h"
#include "presentation/PixelRenderer.h"
#include "presentation/PokemonShadow.h"

namespace Ui::ExploreRouteUiContent {
using ExploreRouteMotion::directionForDelta;
namespace {
uint8_t objectAnimationFrame(uint16_t id, uint8_t frame) {
    const auto* tile = ExploreEditorTiles::tile(id);
    return tile && tile->frames > 1 ? frame / 2 : frame;
}
static constexpr uint32_t AREA_PREVIEW_CYCLE_MS = ExploreRouteTiming::PREVIEW_CYCLE_MS;
static constexpr uint32_t AREA_PREVIEW_MOVE_MS = ExploreRouteTiming::PREVIEW_MOVE_MS;
static constexpr uint32_t AREA_PREVIEW_HOLD_MS = ExploreRouteTiming::PREVIEW_HOLD_MS;
static constexpr int EXPLORE_TILE_SIZE = ExploreRouteGeometry::TILE_SIZE;
static constexpr int EXPLORE_MAP_W = 16 * EXPLORE_TILE_SIZE;
static constexpr int EXPLORE_MAP_H = 12 * EXPLORE_TILE_SIZE;
static constexpr int ROUTE_PICKUP_VISUAL_OFFSET_Y = 6;
static constexpr float ROUTE_BOSS_PATROL_DISTANCE = 5.0f;
static constexpr uint32_t ROUTE_BOSS_PATROL_CYCLE_MS = 6000;
static constexpr uint32_t ROUTE_BOSS_PATROL_OUT_START_MS = 1600;
static constexpr uint32_t ROUTE_BOSS_PATROL_OUT_END_MS = 2300;
static constexpr uint32_t ROUTE_BOSS_PATROL_RETURN_START_MS = 3800;
static constexpr uint32_t ROUTE_BOSS_PATROL_RETURN_END_MS = 4500;
static constexpr uint8_t EXPLORE_HUD_ALPHA = 150;
static constexpr uint8_t EXPLORE_MENU_ITEM_COUNT = AppSceneFlow::exploreMenuItemCount();

float routeAirOffsetY(uint16_t speciesId, uint32_t nowMs, float scale) {
    const PokemonMotion::AirProfile air =
        PokemonMotion::airProfileForSpecies(speciesId);
    if (air.height <= 0.0f) return 0.0f;
    float phase = static_cast<float>((nowMs + speciesId * 97U) % 1600U) *
                  0.00392699f;
    return (air.height + sinf(phase) * air.bobAmplitude) * scale;
}

constexpr uint16_t routeBossPatrolPhasePermille(uint32_t phase) {
    return phase < ROUTE_BOSS_PATROL_OUT_START_MS
        ? 0
        : (phase < ROUTE_BOSS_PATROL_OUT_END_MS
            ? static_cast<uint16_t>(
                (phase - ROUTE_BOSS_PATROL_OUT_START_MS) * 1000U /
                (ROUTE_BOSS_PATROL_OUT_END_MS - ROUTE_BOSS_PATROL_OUT_START_MS))
            : (phase < ROUTE_BOSS_PATROL_RETURN_START_MS
                ? 1000
                : (phase < ROUTE_BOSS_PATROL_RETURN_END_MS
                    ? static_cast<uint16_t>(
                        1000U -
                        (phase - ROUTE_BOSS_PATROL_RETURN_START_MS) * 1000U /
                        (ROUTE_BOSS_PATROL_RETURN_END_MS -
                         ROUTE_BOSS_PATROL_RETURN_START_MS))
                    : 0)));
}

constexpr uint16_t routeBossPatrolPermille(uint32_t nowMs) {
    return routeBossPatrolPhasePermille(nowMs % ROUTE_BOSS_PATROL_CYCLE_MS);
}

constexpr bool routeBossPatrolFacesOutward(uint32_t phase) {
    return phase >= ROUTE_BOSS_PATROL_OUT_START_MS &&
           phase < ROUTE_BOSS_PATROL_RETURN_START_MS;
}

static_assert(routeBossPatrolPermille(0) == 0 &&
                  routeBossPatrolPermille(ROUTE_BOSS_PATROL_OUT_END_MS) == 1000 &&
                  routeBossPatrolPermille(ROUTE_BOSS_PATROL_RETURN_END_MS) == 0,
              "route boss patrol must leave and return to its anchor");
static_assert(!routeBossPatrolFacesOutward(0) &&
                  routeBossPatrolFacesOutward(ROUTE_BOSS_PATROL_OUT_START_MS) &&
                  routeBossPatrolFacesOutward(ROUTE_BOSS_PATROL_OUT_END_MS) &&
                  !routeBossPatrolFacesOutward(ROUTE_BOSS_PATROL_RETURN_START_MS),
              "route boss must hold its last movement direction at each endpoint");

int uiTextWidth(const char* value) {
    if (!value) return 0;
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

void drawAreaPreviewFrame(const PokemonSprites::SpriteFrame* frame, int centerX, int centerY, bool silhouette) {
    if (!frame) return;

    int x = centerX - static_cast<int>(frame->width) / 2;
    int y = centerY - static_cast<int>(frame->height) / 2;
    if (silhouette) {
        PokemonSprites::drawFrameSilhouette(frame, x, y, 0x0000);
    } else {
        PokemonSprites::drawFrame(frame, x, y);
    }
}

ExploreRouteGeometry::WorldPoint routePathPointWorld(const ExploreMapGenerator::Path& path, uint8_t index) {
    return ExploreRouteGeometry::pathPoint(path, index);
}

using RouteWorldPoint = ExploreRouteGeometry::WorldPoint;
float routeGroundOffsetY(const ExploreMapGenerator::Map* map,
                         const ExploreMapGenerator::Path& path, const ExploreRouteLayout& layout) {
    if (ExploreRouteGeometry::hasValleyForestCenterline(map)) return 0;
    return EXPLORE_TILE_SIZE * (path.cellCentered ? 1.0f : 2.0f) * layout.routeGroundBias;
}

struct RouteBossWorldPose { RouteWorldPoint point; float tangentX, tangentY; };

RouteBossWorldPose routeBossWorldPose(const ExploreMapGenerator::Path& path, uint8_t index, uint32_t nowMs) {
    RouteBossWorldPose pose{routePathPointWorld(path, index), 0.0f, 1.0f};
    if (index + 1 < path.pointCount) {
        const RouteWorldPoint next = routePathPointWorld(path, index + 1);
        pose.tangentX = next.x - pose.point.x;
        pose.tangentY = next.y - pose.point.y;
    }
    const float tangentLength = hypotf(pose.tangentX, pose.tangentY);
    if (tangentLength > 0.01f) {
        pose.tangentX /= tangentLength;
        pose.tangentY /= tangentLength;
    }
    const float patrolOffset = ROUTE_BOSS_PATROL_DISTANCE *
        routeBossPatrolPermille(nowMs) / 1000.0f;
    pose.point.x += pose.tangentX * patrolOffset;
    pose.point.y += pose.tangentY * patrolOffset;
    return pose;
}

void drawGeneratedTileFallback(uint16_t tileId, int x, int y, uint8_t layer, uint16_t fieldColor) {
    auto& canvas = PixelRenderer::canvas();
    if (tileId >= 4500 && tileId <= 4544) {
        if (layer == 0) {
            uint16_t color = PixelRenderer::rgb(181, 218, 232);
            if (tileId == 4511) color = PixelRenderer::rgb(203, 202, 218);
            else if (tileId == 4501) color = PixelRenderer::rgb(224, 235, 235);
            else if (tileId == 4504 || (tileId >= 4524 && tileId <= 4531)) {
                color = PixelRenderer::rgb(112, 217, 235);
            }
            canvas.fillRect(x, y, EXPLORE_TILE_SIZE, EXPLORE_TILE_SIZE, color);
        } else {
            canvas.fillTriangle(x + 5, y + 22, x + 13, y + 4, x + 21, y + 22,
                                PixelRenderer::rgb(112, 198, 232));
        }
        return;
    }
    if (layer == 0) {
        uint16_t color = fieldColor;
        if (ExploreMapGenerator::isWaterTile(tileId)) color = PixelRenderer::rgb(40, 105, 173);
        else if (ExploreMapGenerator::isRoadTile(tileId)) color = PixelRenderer::rgb(232, 211, 135);
        else if (tileId == 390) color = PixelRenderer::rgb(55, 139, 75);
        else if (ExploreMapGenerator::isForestTile(tileId)) color = PixelRenderer::rgb(29, 91, 51);
        canvas.fillRect(x, y, EXPLORE_TILE_SIZE, EXPLORE_TILE_SIZE, color);
        return;
    }
    if (ExploreMapGenerator::isForestTile(tileId)) {
        canvas.fillRect(x + 3, y + 3, EXPLORE_TILE_SIZE - 6, EXPLORE_TILE_SIZE - 3,
                        PixelRenderer::rgb(24, 73, 42));
    }
}

void drawGeneratedMapViewport(const ExploreMapGenerator::Map& generated, int cameraX, int cameraY, uint16_t fieldColor, uint8_t animationFrame, const ExploreRouteLayout& layout) {
    int firstX = MathUtil::max(0, cameraX / static_cast<int>(EXPLORE_TILE_SIZE));
    int firstY = layout.extendMapEdges ? static_cast<int>(std::floor(float(cameraY) / EXPLORE_TILE_SIZE)) :
        MathUtil::max(0, cameraY / static_cast<int>(EXPLORE_TILE_SIZE));
    int lastX = MathUtil::min<int>(ExploreMapGenerator::WIDTH - 1,
                         (cameraX + layout.width - 1) / EXPLORE_TILE_SIZE);
    int lastY = (cameraY + layout.height - 1) / EXPLORE_TILE_SIZE;
    if (!layout.extendMapEdges) lastY = MathUtil::min<int>(ExploreMapGenerator::HEIGHT - 1, lastY);
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                const int sourceY = MathUtil::clamp(tileY, 0, int(ExploreMapGenerator::HEIGHT) - 1);
                uint16_t tileId = generated.layers[layer][sourceY * ExploreMapGenerator::WIDTH + tileX];
                if (tileId == 0) continue;
                int x = tileX * EXPLORE_TILE_SIZE - cameraX;
                int y = tileY * EXPLORE_TILE_SIZE - cameraY;
                if (!GameAssets::drawExploreTile(tileId, x, y, objectAnimationFrame(tileId, animationFrame))) {
                    drawGeneratedTileFallback(tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
}

void drawGeneratedMapVegetationForActor(const ExploreMapGenerator::Map& generated, int cameraX, int cameraY, uint16_t fieldColor, uint8_t animationFrame, float actorFootWorldY, int actorLeft, int actorTop, int actorRight, int actorBottom, const ExploreRouteLayout& layout) {
    const int left = MathUtil::max(0, actorLeft);
    const int top = MathUtil::max(0, actorTop);
    const int right = MathUtil::min<int>(layout.width, actorRight);
    const int bottom = MathUtil::min<int>(layout.height, actorBottom);
    if (left >= right || top >= bottom) return;

    auto& canvas = PixelRenderer::canvas();
    canvas.setClipRect(left, top, right - left, bottom - top);
    const int firstX = MathUtil::max(0, (cameraX + left) / EXPLORE_TILE_SIZE);
    const int firstY = MathUtil::max(0, (cameraY + top) / EXPLORE_TILE_SIZE);
    const int lastX = MathUtil::min<int>(ExploreMapGenerator::WIDTH - 1,
        (cameraX + right - 1) / EXPLORE_TILE_SIZE);
    const int lastY = MathUtil::min<int>(ExploreMapGenerator::HEIGHT - 1,
        (cameraY + bottom - 1) / EXPLORE_TILE_SIZE);
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                const uint16_t tileId = generated.layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (!ExploreTileSemantics::objectOccludes(
                        tileId, layer, tileY, actorFootWorldY,
                        EXPLORE_TILE_SIZE)) continue;
                const int x = tileX * EXPLORE_TILE_SIZE - cameraX;
                const int y = tileY * EXPLORE_TILE_SIZE - cameraY;
                if (!GameAssets::drawExploreTile(tileId, x, y, objectAnimationFrame(tileId, animationFrame))) {
                    drawGeneratedTileFallback(tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
    canvas.clearClipRect();
}

void drawPreviewMember(const ExploreRouteFrame& view, uint8_t index,
                       const PokemonSprites::SpriteFrame* frame,
                       int centerX, int centerY) {
    drawAreaPreviewFrame(frame, centerX, centerY, view.areaPreviewHidden[index]);
}

}  // namespace

void drawMapTile(uint16_t tileId, int x, int y, uint8_t layer,
                 uint16_t fieldColor, uint8_t animationFrame) {
    if (!GameAssets::drawExploreTile(tileId, x, y, objectAnimationFrame(tileId, animationFrame)))
        drawGeneratedTileFallback(tileId, x, y, layer, fieldColor);
}

void drawRouteCover(const ExploreRouteFrame& view, int cameraX, int cameraY,
                    float groundY, int left, int top, int right, int bottom,
                    const ExploreRouteLayout& layout) {
    if (view.overlay) {
        view.overlay->drawCover(view, groundY, left, top, right, bottom,
            cameraX, cameraY, layout);
    } else {
        drawGeneratedMapVegetationForActor(*view.generatedMap, cameraX, cameraY,
            view.fieldColor, static_cast<uint8_t>((view.nowMs / 140U) % 8U),
            groundY, left, top, right, bottom, layout);
    }
}

void drawAreaMenu(const ExploreRouteFrame& view, ExploreRouteVisualState& visual, const ExploreRouteLayout& layout) {
    auto& c = PixelRenderer::canvas();
    bool backgroundDrawn = GameAssets::drawBattleBackground(
        GameAssets::Kind::EXPLORE_MENU_BACKGROUND);
    if (!visual.areaBackgroundTraced) {
        visual.areaBackgroundTraced = true;
        Platform::logf(
            "[ExploreMenuBg] kind=%u drawn=%u assets=%u cursor=%u phase=%u\n",
            static_cast<unsigned>(GameAssets::Kind::EXPLORE_MENU_BACKGROUND),
            backgroundDrawn ? 1U : 0U, GameAssets::available() ? 1U : 0U,
            view.areaCursor, static_cast<unsigned>(view.phaseDebug));
    }
    if (!backgroundDrawn) {
        c.fillRect(0, 0, layout.width, layout.height, 0x0000);
    }
    const int LEFT_W = ExploreRouteLayout::AREA_LIST_WIDTH;
    const int PREVIEW_CENTER_X =
        LEFT_W + (layout.width - LEFT_W) / 2;

    const int PREVIEW_CENTER_Y =
        26 + (layout.height - 26) / 2;
    const int PREVIEW_GAP = 10;
    uint8_t visibleAreaCount = view.visibleAreaCount;
    uint8_t count = static_cast<uint8_t>(visibleAreaCount + 1);
    c.setClipRect(0, 0, LEFT_W, layout.height);
    for (uint8_t i = 0; i < count; ++i) {
        float offset = static_cast<float>(i) - view.areaAnimCursor;
        if (fabsf(offset) > ExploreRouteLayout::AREA_VISIBLE_DISTANCE) continue;
        int y = layout.areaRowCenter(i, view.areaAnimCursor);
        bool active = fabsf(offset) < 0.5f;
        uint8_t areaIndex = i < visibleAreaCount ? i : ExploreRouteFrame::AREA_CAP;
        const char* name = areaIndex < ExploreRouteFrame::AREA_CAP
            ? view.areaNames[areaIndex] : Ui::BACK;
        bool unlocked = areaIndex >= ExploreRouteFrame::AREA_CAP ||
            view.areaUnlocked[areaIndex];
        bool specialActive =
            unlocked && areaIndex < ExploreRouteFrame::AREA_CAP &&
            view.areaSpecialActive[areaIndex];
        uint16_t color = !unlocked
            ? PixelRenderer::rgb(82, 88, 96)
            : specialActive
            ? PixelRenderer::rgb(72, 220, 255)
            : (active
                ? PixelRenderer::rgb(255, 216, 72)
                : PixelRenderer::rgb(156, 164, 176));
        if (active) {
            c.fillRect(5, y - 6, 3, 12,
                       unlocked
                           ? PixelRenderer::rgb(255, 216, 72)
                           : PixelRenderer::rgb(82, 88, 96));
        }
        int textX = (LEFT_W - uiTextWidth(name)) / 2 + 4;
        PixelRenderer::text(textX, y - 8, name, color, 1);
    }
    c.clearClipRect();

    c.drawFastVLine(LEFT_W, 4, layout.height - 8,
                    PixelRenderer::rgb(241, 242, 232));

    bool selectedAreaUnlocked = view.areaCursor < ExploreRouteFrame::AREA_CAP &&
        view.areaUnlocked[view.areaCursor];
    const char* title = view.marketPreview ? Ui::MarketView::NAME : view.areaCursor < ExploreRouteFrame::AREA_CAP &&
                                !selectedAreaUnlocked
                            ? Ui::Explore::AREA_LOCKED
                            : view.areaCursor < ExploreRouteFrame::AREA_CAP &&
                                view.areaPreviewHasRare
                            ? Ui::Explore::MASS_OUTBREAK
                            : Ui::Explore::HABITAT_MONSTERS;
    int titleX = PREVIEW_CENTER_X - uiTextWidth(title) / 2;
    PixelRenderer::text(titleX, 4, title, PixelRenderer::rgb(241, 242, 232), 1);
    c.drawFastHLine(LEFT_W + 8, 24, layout.width - LEFT_W - 16,
                    PixelRenderer::rgb(241, 242, 232));

    if (view.areaCursor >= ExploreRouteFrame::AREA_CAP) return;
    if (!selectedAreaUnlocked) {
        const char* message = Ui::Explore::DEFEAT_PREVIOUS_BOSS;
        int messageX = PREVIEW_CENTER_X - uiTextWidth(message) / 2;
        PixelRenderer::text(messageX, PREVIEW_CENTER_Y - 8, message,
                            PixelRenderer::rgb(156, 164, 176), 1);
        return;
    }

    const uint8_t poolCount = view.areaPreviewCount;
    if (poolCount == 0) return;
    uint32_t elapsed = view.nowMs - view.areaPreviewStartedAt;

    uint8_t currentPreview = static_cast<uint8_t>(
        (elapsed / AREA_PREVIEW_CYCLE_MS + 1) % poolCount);
    uint8_t nextPreview =
        static_cast<uint8_t>((currentPreview + 1) % poolCount);
    uint8_t prevPreview = static_cast<uint8_t>(
        (currentPreview + poolCount - 1) % poolCount);
    uint32_t cycleElapsed = elapsed % AREA_PREVIEW_CYCLE_MS;
    float progress = cycleElapsed <= AREA_PREVIEW_HOLD_MS
        ? 0.0f
        : (cycleElapsed - AREA_PREVIEW_HOLD_MS) /
              static_cast<float>(AREA_PREVIEW_MOVE_MS);
    progress = MathUtil::min(1.0f, progress);
    progress = progress * progress * (3.0f - 2.0f * progress);

    auto frameWidth = [](const PokemonSprites::SpriteFrame* frame) {
        return frame ? static_cast<int>(frame->width) : 0;
    };
    c.setClipRect(LEFT_W + 1, 26,
                  layout.width - LEFT_W - 1,
                  layout.height - 26);
    if (progress <= 0.0f) {

        const PokemonSprites::SpriteFrame* cur = view.areaPreviewFrames[currentPreview];
        const PokemonSprites::SpriteFrame* prev = view.areaPreviewFrames[prevPreview];
        const PokemonSprites::SpriteFrame* next = view.areaPreviewFrames[nextPreview];
        int leftX = PREVIEW_CENTER_X - frameWidth(cur) / 2 - PREVIEW_GAP -
                    frameWidth(prev) / 2;
        int rightX = PREVIEW_CENTER_X + frameWidth(cur) / 2 + PREVIEW_GAP +
                     frameWidth(next) / 2;
        drawPreviewMember(view, prevPreview, prev, leftX,
                          PREVIEW_CENTER_Y);
        drawPreviewMember(view, currentPreview, cur,
                          PREVIEW_CENTER_X, PREVIEW_CENTER_Y);
        drawPreviewMember(view, nextPreview, next, rightX,
                          PREVIEW_CENTER_Y);
    } else {

        const PokemonSprites::SpriteFrame* cur = view.areaPreviewFrames[currentPreview];
        const PokemonSprites::SpriteFrame* next = view.areaPreviewFrames[nextPreview];
        uint8_t enteringPreview =
            static_cast<uint8_t>((nextPreview + 1) % poolCount);
        const PokemonSprites::SpriteFrame* entering =
            view.areaPreviewFrames[enteringPreview];
        int curW = frameWidth(cur);
        int nextW = frameWidth(next);
        int enteringW = frameWidth(entering);

        int nextOldX = PREVIEW_CENTER_X + curW / 2 + PREVIEW_GAP + nextW / 2;
        int curOldX = PREVIEW_CENTER_X;
        int enteringOldX = layout.width + enteringW / 2 + 4;

        int nextNewX = PREVIEW_CENTER_X;
        int curNewX = PREVIEW_CENTER_X - nextW / 2 - PREVIEW_GAP - curW / 2;
        int enteringNewX =
            PREVIEW_CENTER_X + nextW / 2 + PREVIEW_GAP + enteringW / 2;
        int nextX = nextOldX +
                    static_cast<int>(roundf((nextNewX - nextOldX) * progress));
        int curX = curOldX +
                   static_cast<int>(roundf((curNewX - curOldX) * progress));
        int enteringX = enteringOldX +
                        static_cast<int>(
                            roundf((enteringNewX - enteringOldX) * progress));
        drawPreviewMember(view, currentPreview, cur, curX,
                          PREVIEW_CENTER_Y);
        drawPreviewMember(view, nextPreview, next, nextX,
                          PREVIEW_CENTER_Y);
        drawPreviewMember(view, enteringPreview, entering,
                          enteringX, PREVIEW_CENTER_Y);
    }
    c.clearClipRect();
}

void drawWalking(const ExploreRouteFrame& view, ExploreRouteVisualState& visual, const ExploreRouteLayout& layout) {
    if (!view.generatedMap || !view.leader) return;
    auto& c = PixelRenderer::canvas();
    // Center maps smaller than the viewport. Optional decorative edge rows
    // keep their native tile size without changing the playable map.
    const int cameraX = layout.width > EXPLORE_MAP_W ? (int(EXPLORE_MAP_W) - layout.width) / 2 :
        MathUtil::clamp(static_cast<int>(roundf(view.routeWorldX)) - layout.width / 2,
                       0, static_cast<int>(EXPLORE_MAP_W) - layout.width);
    const int cameraY = layout.height > EXPLORE_MAP_H ? (int(EXPLORE_MAP_H) - layout.height) / 2 :
        MathUtil::clamp(static_cast<int>(roundf(view.routeWorldY)) - layout.height / 2,
                       0, static_cast<int>(EXPLORE_MAP_H) - layout.height);
    if (layout.width > EXPLORE_MAP_W || layout.height > EXPLORE_MAP_H)
        c.fillRect(0, 0, layout.width, layout.height, view.fieldColor);

    const uint32_t nowMs = view.nowMs;
    uint8_t mapAnimationFrame = static_cast<uint8_t>((nowMs / 140U) % 8U);
    drawGeneratedMapViewport(
        (*view.generatedMap), cameraX, cameraY, view.fieldColor, mapAnimationFrame, layout);

    if (view.overlay) view.overlay->drawGround(view, cameraX, cameraY, layout);
    drawRoutePickup(view, cameraX, cameraY, layout);

    const Species& activeSpecies = *view.leader;
    const auto& path = view.generatedMap->paths[view.currentRoutePath];
    const float groundOffsetY = routeGroundOffsetY(view.generatedMap, path, layout);
    if (view.overlay || (*view.generatedMap).areaIndex == ExploreMapGenerator::TALL_GRASS_PARK_AREA ||
        (*view.generatedMap).areaIndex == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA ||
        (*view.generatedMap).areaIndex == ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA) {
        ExploreTileSemantics::ActorDepth actors[6] = {
            {ExploreTileSemantics::ActorRole::LEADER, view.routeWorldY + groundOffsetY}};
        uint8_t actorCount = 1;
        if ((view.follower != nullptr)) {
            actors[actorCount++] = {
                ExploreTileSemantics::ActorRole::COMPANION, view.routeFollowerWorldY + groundOffsetY};
        }
        if (view.routeBossPending) {
            const ExploreMapGenerator::Path& path = (*view.generatedMap).paths[view.currentRoutePath];
            if (view.routeBossIndex < path.pointCount) {
                actors[actorCount++] = {ExploreTileSemantics::ActorRole::BOSS,
                    routeBossWorldPose(path, view.routeBossIndex, nowMs).point.y + groundOffsetY};
            }
        }
        if (view.overlay) actorCount = view.overlay->appendActors(actors, actorCount, 6);
        ExploreTileSemantics::sortActorsByGroundDepth(actors, actorCount);
        for (uint8_t index = 0; index < actorCount; ++index) {
            switch (actors[index].role) {
            case ExploreTileSemantics::ActorRole::WILDLIFE:
                view.overlay->drawWildlife(view, actors[index].index, cameraX, cameraY, layout);
                break;
            case ExploreTileSemantics::ActorRole::BOSS:
                drawRouteBoss(view, cameraX, cameraY, layout);
                break;
            case ExploreTileSemantics::ActorRole::COMPANION:
                drawRouteMonster(view, visual, (*view.follower),
                    view.routeFollowerWorldX, view.routeFollowerWorldY,
                    view.routeFollowerWalkDirection, true, cameraX, cameraY, layout);
                break;
            case ExploreTileSemantics::ActorRole::LEADER:
                drawRouteMonster(view, visual, activeSpecies, view.routeWorldX, view.routeWorldY,
                    view.routeWalkDirection, false, cameraX, cameraY, layout);
                break;
            }
        }
    } else {
        bool bossBehindTeam = false;
        if (view.routeBossPending) {
            const ExploreMapGenerator::Path& path = (*view.generatedMap).paths[view.currentRoutePath];
            if (view.routeBossIndex < path.pointCount) {
                bossBehindTeam =
                    routePathPointWorld(path, view.routeBossIndex).y <= view.routeWorldY;
            }
        }
        if (bossBehindTeam) drawRouteBoss(view, cameraX, cameraY, layout);
        if ((view.follower != nullptr)) {
            const Species& followerSpecies = (*view.follower);
            if (view.routeFollowerWorldY <= view.routeWorldY) {
                drawRouteMonster(view, visual, followerSpecies, view.routeFollowerWorldX,
                                 view.routeFollowerWorldY, view.routeFollowerWalkDirection,
                                 true, cameraX, cameraY, layout);
                drawRouteMonster(view, visual, activeSpecies, view.routeWorldX, view.routeWorldY,
                                 view.routeWalkDirection, false, cameraX, cameraY, layout);
            } else {
                drawRouteMonster(view, visual, activeSpecies, view.routeWorldX, view.routeWorldY,
                                 view.routeWalkDirection, false, cameraX, cameraY, layout);
                drawRouteMonster(view, visual, followerSpecies, view.routeFollowerWorldX,
                                 view.routeFollowerWorldY, view.routeFollowerWalkDirection,
                                 true, cameraX, cameraY, layout);
            }
        } else {
            drawRouteMonster(view, visual, activeSpecies, view.routeWorldX, view.routeWorldY,
                             view.routeWalkDirection, false, cameraX, cameraY, layout);
        }
        if (view.routeBossPending && !bossBehindTeam) drawRouteBoss(view, cameraX, cameraY, layout);
    }

    const auto daylight = view.daylight;
    c.multiplyRectRgb(0, 0, layout.width, layout.height,
        daylight.red, daylight.green, daylight.blue);

    if (view.overlay && view.overlay->drawHud(view, layout)) return;

    const uint16_t panel = PixelRenderer::rgb(8, 10, 14);
    int hudW = MathUtil::max(60, uiTextWidth(view.mapName) + 12);
    int hudX = layout.width - hudW - 4;
    PixelRenderer::fillRectAlpha(
        hudX, 2, hudW, 22, panel, EXPLORE_HUD_ALPHA);
    c.drawRect(hudX, 2, hudW, 22, PixelRenderer::rgb(72, 83, 98));
    PixelRenderer::text(
        hudX + 6, 5, view.mapName, PixelRenderer::rgb(245, 246, 232), 1);
    if (view.exploreMenuOpen) drawExploreMenu(view, layout);
}

void drawRouteBoss(const ExploreRouteFrame& view, int cameraX, int cameraY, const ExploreRouteLayout& layout) {
    if (!view.generatedMap) return;
    const uint32_t nowMs = view.nowMs;
    if (!view.routeBossPending) return;
    const ExploreMapGenerator::Path& path = (*view.generatedMap).paths[view.currentRoutePath];
    if (view.routeBossIndex >= path.pointCount) return;

    const Species* species = view.boss;
    if (!species) return;

    const RouteBossWorldPose pose = routeBossWorldPose(path, view.routeBossIndex, nowMs);
    const float tangentX = pose.tangentX;
    const float tangentY = pose.tangentY;
    uint32_t patrolPhase = nowMs % ROUTE_BOSS_PATROL_CYCLE_MS;
    bool movingOut = patrolPhase >= ROUTE_BOSS_PATROL_OUT_START_MS &&
                     patrolPhase < ROUTE_BOSS_PATROL_OUT_END_MS;
    bool movingBack = patrolPhase >= ROUTE_BOSS_PATROL_RETURN_START_MS &&
                      patrolPhase < ROUTE_BOSS_PATROL_RETURN_END_MS;
    bool patrolMoving = movingOut || movingBack;
    float worldX = pose.point.x;
    float worldY = pose.point.y + routeGroundOffsetY(view.generatedMap, path, layout);

    uint8_t walkDirection = static_cast<uint8_t>(PokemonSprites::WalkDirection::DOWN);
    if (routeBossPatrolFacesOutward(patrolPhase)) {
        walkDirection = directionForDelta(tangentX, tangentY, walkDirection);
    } else {
        walkDirection = directionForDelta(-tangentX, -tangentY, walkDirection);
    }

    const PokemonSprites::SpriteFrame* frame = nullptr;
    bool flipX = false;
    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(
            species->id,
            static_cast<PokemonSprites::WalkDirection>(walkDirection),
            animation)) {
        uint8_t frameIndex = 0;
        if (patrolMoving) {
            uint32_t moveStarted = movingOut ? ROUTE_BOSS_PATROL_OUT_START_MS
                                             : ROUTE_BOSS_PATROL_RETURN_START_MS;
            uint16_t moveDuration = static_cast<uint16_t>(
                movingOut
                    ? ROUTE_BOSS_PATROL_OUT_END_MS - ROUTE_BOSS_PATROL_OUT_START_MS
                    : ROUTE_BOSS_PATROL_RETURN_END_MS - ROUTE_BOSS_PATROL_RETURN_START_MS);
            uint32_t moveElapsed = patrolPhase - moveStarted;
            frameIndex = PokemonMotion::movementFrame(
                PokemonMotion::behaviorForSpecies(species->id),
                animation.frameCount, moveElapsed, moveElapsed, moveDuration);
        }
        frame = PokemonSprites::findSpeciesSprite(
            species->id,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(animation.base) + frameIndex));
        flipX = animation.flipX;
    }
    if (!frame) {
        frame = PokemonSprites::findSpeciesSprite(
            species->id, PokemonSprites::SpriteKind::ICON_0);
    }
    if (!frame) return;

    const float scale = layout.actorScale;
    int drawW = MathUtil::max<int>(1, static_cast<int>(roundf(frame->width * scale)));
    int drawH = MathUtil::max<int>(1, static_cast<int>(roundf(frame->height * scale)));
    int centerX = static_cast<int>(roundf(worldX)) - cameraX;
    float airOffsetY = routeAirOffsetY(species->id, view.nowMs, scale);
    // Route points describe the road centerline. Match the AMOLED renderer and
    // anchor the artwork by its authored feet, rather than by its pixel center;
    // otherwise horizontal two-cell roads place the feet on the lower edge.
    const bool centeredGroundAnchor = layout.feetOnRouteCenter ||
        (*view.generatedMap).areaIndex == ExploreMapGenerator::TALL_GRASS_PARK_AREA ||
        (*view.generatedMap).areaIndex == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA ||
        (*view.generatedMap).areaIndex == ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA;
    const int groundOffset = centeredGroundAnchor ? static_cast<int>(roundf(
        PokemonSprites::frameGroundOffsetY(frame) * scale)) : 0;
    int centerY = static_cast<int>(roundf(worldY - airOffsetY)) - cameraY - groundOffset;
    int shadowGroundY = static_cast<int>(roundf(worldY)) - cameraY -
        groundOffset + static_cast<int>(roundf(PokemonSprites::frameGroundOffsetY(frame) * scale));
    if (ExploreRouteGeometry::hasValleyForestCenterline(view.generatedMap)) {
        const auto anchor = PokemonShadow::spriteAnchorForShadow(frame, centerX, shadowGroundY, scale, flipX);
        centerY += anchor.y - shadowGroundY;
        centerX = anchor.x;
        shadowGroundY = anchor.y;
    }
    int drawX = centerX - drawW / 2;
    int drawY = centerY - drawH / 2;
    if (drawX + drawW < -8 || drawX >= layout.width + 8 ||
        drawY + drawH < -16 || drawY >= layout.height + 8) {
        return;
    }

    auto& c = PixelRenderer::canvas();

    const auto shadow = PokemonShadow::atGround(frame, centerX, shadowGroundY, scale, flipX);
    c.fillEllipse(shadow.x, shadow.y, shadow.radiusX, shadow.radiusY,
                  PixelRenderer::rgb(68, 87, 74));
    PokemonSprites::drawFrameScaled(frame, drawX, drawY, scale, flipX);
    const int actorTop = MathUtil::min(drawY, shadow.y - shadow.radiusY);
    const int actorBottom = MathUtil::max(drawY + drawH, shadow.y + shadow.radiusY + 1);
    drawRouteCover(view, cameraX, cameraY, worldY, MathUtil::min(drawX, shadow.x - shadow.radiusX), actorTop,
        MathUtil::max(drawX + drawW, shadow.x + shadow.radiusX + 1), actorBottom, layout);
}

void drawRouteMonster(const ExploreRouteFrame& view, ExploreRouteVisualState& visual, const Species& species, float worldX, float worldY, uint8_t walkDirection, bool follower, int cameraX, int cameraY, const ExploreRouteLayout& layout) {
    if (!view.generatedMap) return;
    worldY += routeGroundOffsetY(view.generatedMap, view.generatedMap->paths[view.currentRoutePath], layout);
    auto& c = PixelRenderer::canvas();
    const PokemonSprites::SpriteFrame* marker = nullptr;
    bool markerFlipX = false;
    int8_t poseOffsetX = 0;
    int8_t poseOffsetY = 0;
    const PokemonMotion::Behavior motion =
        PokemonMotion::behaviorForSpecies(species.id);
    uint8_t* visualWalkDirection = follower
        ? &visual.routeFollowerVisualWalkDirection
        : &visual.routeVisualWalkDirection;
    bool animationActive = false;
    uint32_t movementElapsedMs = 0;
    uint32_t stepElapsedMs = 0;
    uint16_t stepDurationMs = 1;

    if (view.routeMoving && (!follower || view.routeFollowerMoving)) {
        uint32_t animationNow = view.exploreMenuOpen ? view.exploreMenuOpenedAt : view.nowMs;
        uint32_t elapsed = animationNow - view.routeMoveStarted;
        uint16_t followerDelayMs = view.iceSliding ? 0 : view.routeFollowerDelayMs;
        if (!follower || elapsed > followerDelayMs) {
            if (follower) elapsed -= followerDelayMs;
            animationActive = true;
            stepDurationMs = MathUtil::max<uint16_t>(
                1, follower ? view.routeFollowerMoveDurationMs
                            : view.routeLeaderMoveDurationMs);
            stepElapsedMs = elapsed;
            movementElapsedMs = elapsed;
            if (view.exiting) {
                stepDurationMs = MathUtil::max<uint16_t>(
                    1, (follower ? view.followerExitStepDurationMs : view.leaderExitStepDurationMs));
                stepElapsedMs %= stepDurationMs;
            } else {
                stepElapsedMs = MathUtil::min<uint32_t>(stepElapsedMs, stepDurationMs - 1);
                uint32_t completedSteps = view.routeIndex > 0 ? view.routeIndex - 1 : 0;
                if (follower) {
                    uint8_t followerTargetIndex = view.routeFollowerIndex;
                    completedSteps = followerTargetIndex > 0
                        ? followerTargetIndex - 1
                        : 0;
                }
                movementElapsedMs = completedSteps * motion.stepDurationMs +
                                    stepElapsedMs;
            }
        }
    }

    if (motion.mode != PokemonMotion::Mode::SLITHER || !animationActive) {
        *visualWalkDirection = walkDirection;
    } else {
        uint16_t cycleMs = PokemonMotion::cycleDurationMs(
            motion, PokemonMotion::PlaybackContext::ROUTE);
        uint8_t motionPhase = PokemonMotion::slitherPhaseIndex(
            movementElapsedMs, cycleMs);
        if (PokemonMotion::slitherDirectionChangeSafe(motionPhase)) {
            *visualWalkDirection = walkDirection;
        }
    }

    PokemonSprites::WalkingAnimation animation{};
    if (PokemonSprites::walkingAnimation(
            species.id,
            static_cast<PokemonSprites::WalkDirection>(*visualWalkDirection),
            animation)) {
        uint8_t frameIndex = 0;
        if (animationActive) {
            if (motion.mode == PokemonMotion::Mode::SLITHER) {
                uint8_t registrationDirection = 0;
                switch (static_cast<PokemonSprites::WalkDirection>(
                            *visualWalkDirection)) {
                case PokemonSprites::WalkDirection::DOWN:
                    registrationDirection = 0;
                    break;
                case PokemonSprites::WalkDirection::LEFT:
                    registrationDirection = 2;
                    break;
                case PokemonSprites::WalkDirection::UP:
                    registrationDirection = 4;
                    break;
                case PokemonSprites::WalkDirection::RIGHT:
                    registrationDirection = 6;
                    break;
                case PokemonSprites::WalkDirection::DOWN_LEFT: registrationDirection = 1; break;
                case PokemonSprites::WalkDirection::UP_LEFT: registrationDirection = 3; break;
                case PokemonSprites::WalkDirection::UP_RIGHT: registrationDirection = 5; break;
                case PokemonSprites::WalkDirection::DOWN_RIGHT: registrationDirection = 7; break;
                }
                PokemonMotion::Pose pose = PokemonMotion::slitherPose(
                    motion, animation.frameCount, movementElapsedMs,
                    PokemonMotion::cycleDurationMs(
                        motion, PokemonMotion::PlaybackContext::ROUTE),
                    registrationDirection);
                frameIndex = pose.frameIndex;
                poseOffsetX = pose.offsetX;
                poseOffsetY = pose.offsetY;
            } else {
                frameIndex = PokemonMotion::movementFrame(
                    motion, animation.frameCount, movementElapsedMs,
                    stepElapsedMs, stepDurationMs);
            }
        }
        marker = PokemonSprites::findSpeciesSprite(
            species.id,
            static_cast<PokemonSprites::SpriteKind>(
                static_cast<uint16_t>(animation.base) + frameIndex));
        markerFlipX = animation.flipX;
    }
    if (!marker) {
        poseOffsetX = 0;
        poseOffsetY = 0;
        marker = PokemonSprites::findSpeciesSprite(
            species.id, PokemonSprites::SpriteKind::ICON_0);
    }
    if (marker) {
        const float scale = layout.actorScale;
        int markerW = static_cast<int>(marker->width * scale);
        int markerH = static_cast<int>(marker->height * scale);
        int markerX = static_cast<int>(roundf(worldX)) - cameraX - markerW / 2;
        float airOffsetY = routeAirOffsetY(species.id, view.nowMs, scale);
        const bool centeredGroundAnchor = layout.feetOnRouteCenter ||
            (*view.generatedMap).areaIndex == ExploreMapGenerator::TALL_GRASS_PARK_AREA ||
            (*view.generatedMap).areaIndex == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA ||
            (*view.generatedMap).areaIndex == ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA;
        const int groundOffset = centeredGroundAnchor ? static_cast<int>(roundf(
            PokemonSprites::frameGroundOffsetY(marker) * scale)) : 0;
        int markerY = static_cast<int>(roundf(worldY - airOffsetY)) - cameraY -
            markerH / 2 - groundOffset;
        const int poseX = static_cast<int>(roundf(poseOffsetX * scale));
        const int poseY = static_cast<int>(roundf(poseOffsetY * scale));
        int shadowX = markerX + markerW / 2 + poseX;
        int shadowGroundY = static_cast<int>(roundf(worldY)) - cameraY -
            groundOffset + static_cast<int>(roundf(PokemonSprites::frameGroundOffsetY(marker) * scale)) + poseY;
        if (ExploreRouteGeometry::hasValleyForestCenterline(view.generatedMap)) {
            const auto anchor = PokemonShadow::spriteAnchorForShadow(
                marker, shadowX - poseX, shadowGroundY - poseY, scale, markerFlipX);
            markerX += anchor.x - (shadowX - poseX);
            markerY += anchor.y - (shadowGroundY - poseY);
            shadowX = anchor.x;
            shadowGroundY = anchor.y;
        }
        const auto shadow = PokemonShadow::atGround(marker, shadowX, shadowGroundY, scale, markerFlipX);
        c.fillEllipse(shadow.x, shadow.y, shadow.radiusX, shadow.radiusY,
                      PixelRenderer::rgb(68, 87, 74));
        markerX += poseX;
        markerY += poseY;
        PokemonSprites::drawFrameScaled(marker, markerX, markerY, scale, markerFlipX);
        drawRouteCover(view, cameraX, cameraY, worldY, MathUtil::min(markerX, shadow.x - shadow.radiusX),
            MathUtil::min(markerY, shadow.y - shadow.radiusY),
            MathUtil::max(markerX + static_cast<int>(roundf(marker->width * scale)),
                          shadow.x + shadow.radiusX + 1),
            MathUtil::max(markerY + static_cast<int>(roundf(marker->height * scale)),
                          shadow.y + shadow.radiusY + 1), layout);
    }
}

void drawRoutePickup(const ExploreRouteFrame& view, int cameraX, int cameraY, const ExploreRouteLayout& layout) {
    if (!view.generatedMap) return;
    if (!view.routePickupAvailable) return;
    const ExploreMapGenerator::Path& path = (*view.generatedMap).paths[view.currentRoutePath];
    if (view.routePickupIndex >= path.pointCount) return;

    RouteWorldPoint point = routePathPointWorld(path, view.routePickupIndex);
    int x = static_cast<int>(roundf(point.x)) - cameraX;
    // The existing ball artwork is centered ten pixels above its ground shadow.
    // Align that ground with the authored feet instead of retaining its old +12px offset.
    int y = layout.feetOnRouteCenter || ExploreRouteGeometry::hasValleyForestCenterline(view.generatedMap)
        ? static_cast<int>(roundf(point.y + routeGroundOffsetY(view.generatedMap, path, layout))) - cameraY - 6
        : static_cast<int>(roundf(point.y)) - cameraY + ROUTE_PICKUP_VISUAL_OFFSET_Y;
    if (x < -13 || x >= layout.width + 13 ||
        y < -13 || y >= layout.height + 13) {
        return;
    }

    auto& c = PixelRenderer::canvas();
    c.fillEllipse(x, y + 6, 7, 2, PixelRenderer::rgb(55, 68, 59));
    if (GameAssets::drawCentered(
            GameAssets::Kind::EXPLORE_PICKUP_BALL, x, y - 4)) {
        return;
    }
    if (GameAssets::drawCentered(
            GameAssets::Kind::ITEM_POKE_BALL, x, y - 4, 0.62f)) {
        return;
    }
    c.fillCircle(x, y - 4, 7, PixelRenderer::rgb(224, 69, 65));
    for (int row = 0; row <= 6; ++row) {
        int halfWidth = static_cast<int>(
            sqrtf(49.0f - static_cast<float>(row * row)));
        c.drawFastHLine(x - halfWidth, y - 4 + row,
                        halfWidth * 2 + 1, 0xFFFF);
    }
    c.drawCircle(x, y - 4, 7, PixelRenderer::rgb(35, 39, 44));
    c.drawFastHLine(x - 7, y - 4, 14, PixelRenderer::rgb(35, 39, 44));
    c.fillCircle(x, y - 4, 2, 0xFFFF);
    c.drawCircle(x, y - 4, 2, PixelRenderer::rgb(35, 39, 44));
}

void drawExploreMenu(const ExploreRouteFrame& view, const ExploreRouteLayout& layout) {
    static_assert(EXPLORE_MENU_ITEM_COUNT ==
                      sizeof(Ui::Explore::SIDE_MENU_ITEMS) /
                          sizeof(Ui::Explore::SIDE_MENU_ITEMS[0]),
                  "explore side menu labels must match the menu item count");
    auto& c = PixelRenderer::canvas();
    const int PANEL_W = 60;
    const int PANEL_X = layout.width - PANEL_W;
    const int PANEL_Y = 0;
    const int PANEL_H = layout.height;
    const int ROW_H = 30;
    const uint16_t background = PixelRenderer::rgb(20, 25, 32);
    const uint16_t border = PixelRenderer::rgb(190, 200, 205);
    const uint16_t active = PixelRenderer::rgb(255, 216, 72);
    const uint16_t inactive = PixelRenderer::rgb(235, 239, 232);

    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, background);
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, border);
    for (uint8_t i = 0; i < EXPLORE_MENU_ITEM_COUNT; ++i) {
        int y = PANEL_Y + 9 + i * ROW_H;
        bool selected = i == view.exploreMenuCursor;
        if (selected) c.fillRect(PANEL_X + 5, y, 3, 18, active);
        PixelRenderer::text(PANEL_X + 13, y, Ui::Explore::SIDE_MENU_ITEMS[i],
                            selected ? active : inactive, 1);
    }
}

void drawSpecialPrompt(const ExploreRouteFrame& view, const ExploreRouteLayout& layout) {
    auto& c = PixelRenderer::canvas();
    const auto panel = layout.specialPrompt();
    const int PANEL_X = panel.x, PANEL_Y = panel.y;
    const int PANEL_W = panel.width, PANEL_H = panel.height;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(24, 28, 36));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H,
               PixelRenderer::rgb(241, 242, 232));

    const Species* species = view.boss;
    char line[48] = {};
    snprintf(line, sizeof(line), Ui::Explore::SPECIAL_FOUND_FMT,
             species ? species->name : "");
    PixelRenderer::text(
        PANEL_X + (PANEL_W - uiTextWidth(line)) / 2,
        PANEL_Y + 12, line, PixelRenderer::rgb(72, 220, 255), 1);
    PixelRenderer::text(
        PANEL_X + (PANEL_W -
                   uiTextWidth(Ui::Explore::SPECIAL_CHALLENGE_QUESTION)) / 2,
        PANEL_Y + 36, Ui::Explore::SPECIAL_CHALLENGE_QUESTION,
        PixelRenderer::rgb(241, 242, 232), 1);

    uint16_t challengeColor = view.specialChallengeYes
        ? PixelRenderer::rgb(255, 216, 72)
        : PixelRenderer::rgb(156, 164, 176);
    uint16_t bypassColor = view.specialChallengeYes
        ? PixelRenderer::rgb(156, 164, 176)
        : PixelRenderer::rgb(255, 216, 72);
    PixelRenderer::text(PANEL_X + 45, PANEL_Y + 61,
                        Ui::Explore::SPECIAL_CHALLENGE, challengeColor, 1);
    PixelRenderer::text(PANEL_X + 130, PANEL_Y + 61,
                        Ui::Explore::SPECIAL_BYPASS, bypassColor, 1);
}

void drawPickupPrompt(const ExploreRouteFrame& view, const ExploreRouteLayout& layout) {
    auto& c = PixelRenderer::canvas();
    const auto panel = layout.pickupPrompt();
    const int PANEL_X = panel.x, PANEL_Y = panel.y;
    const int PANEL_W = panel.width, PANEL_H = panel.height;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(241, 242, 232));
    if (view.resultMessage && view.resultMessage[0]) {
        PixelRenderer::text(layout.centeredPrompts
                            ? PANEL_X + (PANEL_W - uiTextWidth(view.resultMessage)) / 2
                            : PANEL_X + 14, PANEL_Y + 19, view.resultMessage,
                            PixelRenderer::rgb(255, 216, 72), 1);
    }
}

void drawResult(const ExploreRouteFrame& view, const ExploreRouteLayout& layout) {
    auto& c = PixelRenderer::canvas();
    c.fillRect((layout.width - 156) / 2, (layout.height - 58) / 2, 156, 58, PixelRenderer::rgb(35, 42, 50));
    c.drawRect((layout.width - 156) / 2, (layout.height - 58) / 2, 156, 58, PixelRenderer::rgb(95, 110, 126));
    const char* result = view.resultMessage && view.resultMessage[0]
        ? view.resultMessage
        : view.wildName;
    if (result) {
        PixelRenderer::text((layout.width - (layout.centeredPrompts ? uiTextWidth(result) : 124)) / 2,
                            (layout.height - (layout.centeredPrompts ? 16 : 8)) / 2, result,
                            PixelRenderer::rgb(241, 242, 232), 1);
    }
}

void drawEndPrompt(const ExploreRouteFrame& view, const ExploreRouteLayout& layout) {
    auto& c = PixelRenderer::canvas();
    const auto panel = layout.endPrompt();
    const int PANEL_X = panel.x, PANEL_Y = panel.y;
    const int PANEL_W = panel.width, PANEL_H = panel.height;
    c.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(24, 28, 36));
    c.drawRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, PixelRenderer::rgb(241, 242, 232));
    PixelRenderer::text(layout.centeredPrompts
                        ? PANEL_X + (PANEL_W - uiTextWidth(Ui::Explore::RESULT_END)) / 2 : 88,
                        PANEL_Y + (layout.centeredPrompts ? 10 : 13), Ui::Explore::RESULT_END,
                        PixelRenderer::rgb(255, 216, 72), 1);
    int returnY = PANEL_Y + 39;
    if (view.adventureBondGain > 0) {
        char bondBuf[24];
        snprintf(bondBuf, sizeof(bondBuf), Ui::Explore::BOND_GAIN_FMT,
                 view.adventureBondGain);
        PixelRenderer::text(
            (layout.width - uiTextWidth(bondBuf)) / 2,
            PANEL_Y + (layout.centeredPrompts ? 30 : 33), bondBuf, PixelRenderer::rgb(255, 154, 181), 1);
        returnY = PANEL_Y + (layout.centeredPrompts ? 47 : 50);
    }
    PixelRenderer::text(layout.centeredPrompts
                        ? PANEL_X + (PANEL_W - uiTextWidth(Ui::Explore::ANY_KEY_RETURN)) / 2 : 64,
                        returnY, Ui::Explore::ANY_KEY_RETURN,
                        PixelRenderer::rgb(241, 242, 232), 1);
}

}  // namespace Ui::ExploreRouteUiContent
