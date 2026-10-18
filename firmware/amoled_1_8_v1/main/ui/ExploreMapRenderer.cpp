#include "ui/ExploreMapRenderer.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>

#include "assets/GameAssets.h"
#include "core/BuildConfig.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "platform/api/PlatformServices.h"
#include "presentation/Canvas565.h"
#include "presentation/PixelRenderer.h"
#include "ui/UiCommon.h"
#include "ui/RenderCaches.h"
#include "ui/models/ScreenModels.h"

#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
#include "esp_timer.h"
#endif

namespace AmoledV1 {

using UiCommon::rgb;

namespace {

constexpr int EXPLORE_ROUTE_WORLD_WIDTH =
    ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE;
constexpr int EXPLORE_ROUTE_WORLD_HEIGHT =
    ExploreMapGenerator::HEIGHT * ExploreRouteGeometry::TILE_SIZE;
constexpr int EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH =
    EXPLORE_ROUTE_WORLD_WIDTH * AmoledUi::RESOURCE_SCALE;
constexpr int EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT =
    EXPLORE_ROUTE_WORLD_HEIGHT * AmoledUi::RESOURCE_SCALE;

bool isExploreActorForegroundTile(uint16_t tileId) {
    return tileId == 4772;
}

constexpr int GRASS_SIZE = ExploreRouteGeometry::TILE_SIZE;
constexpr uint16_t GRASS_TRANSPARENT = 0xF81F;
struct GrassPixels {
    uint16_t tiles[3][GRASS_SIZE * GRASS_SIZE]{};
    uint16_t floor[64]{};
    uint8_t floorCount = 0;
    bool ready = false;
    bool isFloor(uint16_t color) const {
        return std::find(floor, floor + floorCount, color) != floor + floorCount;
    }
    bool load() {
        if (ready) return true;
        Canvas565 tile;
        uint16_t scratch[GRASS_SIZE * GRASS_SIZE];
        floorCount = 0;
        // Sparse ground tiles identify floor colors without inventing a mask
        // or decoding artwork in the animation hot path.
        tile.attach({scratch, GRASS_SIZE, GRASS_SIZE, false});
        for (uint16_t id = 385; id <= 389; ++id) {
            if (!GameAssets::drawExploreTileTo(tile, id, 0, 0, 0, 1)) return false;
            for (uint16_t color : scratch)
                if (!isFloor(color) && floorCount < 64) floor[floorCount++] = color;
        }
        const uint16_t ids[] = {390, 399, 391};
        for (int i = 0; i < 3; ++i) {
            tile.attach({tiles[i], GRASS_SIZE, GRASS_SIZE, false});
            tile.fillSprite(GRASS_TRANSPARENT);
            if (!GameAssets::drawExploreTileTo(tile, ids[i], 0, 0, 0, 1)) return false;
        }
        ready = true;
        return true;
    }
};

ExploreGrass::Frame grassFrame(const ExploreRouteViewModel& model, int cell, uint16_t id) {
    if (!model.grass) return {};
    if (id == 391 && cell + ExploreMapGenerator::WIDTH < ExploreMapGenerator::CELL_COUNT &&
        model.map->layers[0][cell + ExploreMapGenerator::WIDTH] == 399)
        cell += ExploreMapGenerator::WIDTH;
    return model.grass->frame(cell);
}

// Both the ground pass and body cover sample the same deformed artwork.
// Leaves part around the contact, overshoot briefly, then return to rest.
void drawGrass(Canvas565& canvas, uint16_t id, int x, int y,
               ExploreGrass::Frame frame, bool cover) {
    static GrassPixels grass;
    if (!grass.load()) return;
    constexpr int scale = AmoledUi::RESOURCE_SCALE;
    const int index = id == 391 ? 2 : id == 399 ? 1 : 0;
    const int bend = ExploreGrass::Simulation::bend(frame);
    for (int row = 0; row < GRASS_SIZE; ++row) {
        if (cover && id == 390 && row < 10) continue;
        const int spread = id == 391 ? bend * (2 * GRASS_SIZE - 1 - row) / (2 * GRASS_SIZE - 1)
            : id == 399 ? bend * (GRASS_SIZE - 1 - row) / (2 * GRASS_SIZE - 1)
                        : bend * (GRASS_SIZE - 1 - row) / (GRASS_SIZE - 1);
        for (int column = 0; column < GRASS_SIZE; ++column) {
            const int source = column - (column < frame.contactX ? -spread : spread);
            const uint16_t color = source < 0 || source >= GRASS_SIZE
                ? (id == 391 ? GRASS_TRANSPARENT : grass.floor[0])
                : grass.tiles[index][row * GRASS_SIZE + source];
            if (color == GRASS_TRANSPARENT || (cover && id != 391 && grass.isFloor(color)))
                continue;
            canvas.fillRect(x + column * scale, y + row * scale, scale, scale, color);
        }
    }
}

#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
struct ExploreMapPerf {
    int64_t windowStartedUs = 0;
    uint32_t frames = 0;
    uint32_t cacheHits = 0;
    uint32_t rows = 0;
    uint64_t baseTotalUs = 0;
    uint32_t baseMaxUs = 0;
    uint64_t animationTotalUs = 0;
    uint32_t animationMaxUs = 0;

    void record(bool cacheHit, uint16_t rowCount, uint32_t baseUs,
                uint32_t animationUs) {
        const int64_t nowUs = esp_timer_get_time();
        if (windowStartedUs == 0) windowStartedUs = nowUs;
        ++frames;
        if (cacheHit) ++cacheHits;
        rows += rowCount;
        baseTotalUs += baseUs;
        baseMaxUs = std::max(baseMaxUs, baseUs);
        animationTotalUs += animationUs;
        animationMaxUs = std::max(animationMaxUs, animationUs);
        if (nowUs - windowStartedUs < 1000000 || frames == 0) return;

        Platform::logf(
            "[ExploreMapPerf] frames=%lu cacheHit=%lu rows(avg)=%lu "
            "base(avg/max)=%lu/%lu us anim(avg/max)=%lu/%lu us\n",
            static_cast<unsigned long>(frames),
            static_cast<unsigned long>(cacheHits),
            static_cast<unsigned long>(rows / frames),
            static_cast<unsigned long>(baseTotalUs / frames),
            static_cast<unsigned long>(baseMaxUs),
            static_cast<unsigned long>(animationTotalUs / frames),
            static_cast<unsigned long>(animationMaxUs));
        *this = ExploreMapPerf{};
        windowStartedUs = nowUs;
    }
};

ExploreMapPerf exploreMapPerf;
#endif

void drawExploreTileFallback(Canvas565& canvas, uint16_t tileId,
                             int x, int y, uint8_t layer,
                             uint16_t fieldColor, int outputScale =
                                 AmoledUi::RESOURCE_SCALE) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE;
    auto scaled = [outputScale](int value) { return value * outputScale; };
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
        canvas.fillRect(x, y, scaled(tileSize),
                        scaled(tileSize), color);
        return;
    }
    if (ExploreMapGenerator::isForestTile(tileId)) {
        canvas.fillRect(x + scaled(3), y + scaled(3),
                        scaled(tileSize - 6), scaled(tileSize - 3),
                        rgb(24, 73, 42));
    } else if (tileId >= 4500 && tileId <= 4544) {
        canvas.fillTriangle(x + scaled(5), y + scaled(22),
                            x + scaled(13), y + scaled(4),
                            x + scaled(21), y + scaled(22),
                            rgb(112, 198, 232));
    }
}

RenderCacheKey exploreWorldKey(Canvas565& canvas, const ExploreRouteViewModel& model) {
    return {model.map->seed, model.area, EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH,
            EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT, canvas.byteSwapped()};
}

bool prepareExploreRouteWorldCache(Canvas565& canvas,
                                   const ExploreRouteViewModel& model,
                                   PixelCache565& exploreRouteWorldCache) {
    if (!model.map) return false;
    const RenderCacheKey key = exploreWorldKey(canvas, model);
    if (exploreRouteWorldCache.matches(key)) return true;
    uint16_t* pixels = exploreRouteWorldCache.begin(key, Platform::memory());
    if (!pixels) return false;

    const Platform::FrameBuffer565 worldFrameBuffer{
        pixels, EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH,
        EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT, canvas.byteSwapped()};
    Canvas565 worldCanvas;
    worldCanvas.attach(worldFrameBuffer);
    worldCanvas.setCoordinateScale(1);
    worldCanvas.setLayoutScale(1);
    worldCanvas.setAssetScale(1);
    const uint16_t fieldColor = ExploreAreaCatalog::fieldColor(model.area);
    worldCanvas.fillSprite(fieldColor);

    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT;
         ++layer) {
        for (uint8_t tileY = 0; tileY < ExploreMapGenerator::HEIGHT;
             ++tileY) {
            for (uint8_t tileX = 0; tileX < ExploreMapGenerator::WIDTH;
                 ++tileX) {
                uint16_t tileId = model.map->layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (tileId == 0 || isExploreActorForegroundTile(tileId)) continue;
                int x = tileX * tileSize;
                int y = tileY * tileSize;
                if (!GameAssets::drawExploreTileTo(
                        worldCanvas, tileId, x, y, 0, AmoledUi::RESOURCE_SCALE)) {
                    drawExploreTileFallback(
                        worldCanvas, tileId, x, y, layer, fieldColor,
                        AmoledUi::RESOURCE_SCALE);
                }
            }
        }
    }

    exploreRouteWorldCache.commit();
    return true;
}

bool drawExploreRouteWorldViewport(Canvas565& canvas,
                                   const ExploreRouteViewModel& model,
                                   PixelCache565& exploreRouteWorldCache,
                                   uint16_t rowBegin, uint16_t rowEnd,
                                   bool* worldCacheHit = nullptr) {
    const bool cacheWasReady = model.map &&
        exploreRouteWorldCache.matches(exploreWorldKey(canvas, model));
    if (canvas.physicalWidth() != AmoledUi::WIDTH ||
        canvas.physicalHeight() != AmoledUi::HEIGHT ||
        !prepareExploreRouteWorldCache(canvas, model, exploreRouteWorldCache)) {
        if (worldCacheHit) *worldCacheHit = false;
        return false;
    }

    if (worldCacheHit) {
        *worldCacheHit = cacheWasReady;
    }

    const int top = std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP);
    const int bottom = std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM);
    const int left = std::min<int>(model.renderColumnBegin, AmoledUi::WIDTH);
    const int right = std::clamp<int>(model.renderColumnEnd, left, AmoledUi::WIDTH);
    for (int row = top; row < bottom; ++row) {
        const uint16_t* source = exploreRouteWorldCache.data() +
            static_cast<size_t>(model.cameraY * AmoledUi::RESOURCE_SCALE + row) *
                EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH +
            model.cameraX * AmoledUi::RESOURCE_SCALE + left;
        uint16_t* firstOutput = canvas.rawPixels() +
            static_cast<size_t>(row) *
                canvas.physicalWidth() + left;
        std::memcpy(firstOutput, source,
                    static_cast<size_t>(right - left) * sizeof(uint16_t));
    }
    return true;
}

void drawExploreMap(Canvas565& canvas,
                    const ExploreMapGenerator::Map& map,
                    int cameraX, int cameraY, uint16_t fieldColor,
                    uint8_t animationFrame) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    cameraX *= AmoledUi::RESOURCE_SCALE;
    cameraY *= AmoledUi::RESOURCE_SCALE;
    int firstX = std::max(0, cameraX / tileSize);
    int firstY = std::max(0, cameraY / tileSize);
    int lastX = std::min<int>(
        ExploreMapGenerator::WIDTH - 1,
        (cameraX + AmoledUi::WIDTH - 1) / tileSize);
    int lastY = std::min<int>(
        ExploreMapGenerator::HEIGHT - 1,
        (cameraY + (AmoledUi::HEIGHT - EXPLORE_ROUTE_MAP_TOP) - 1) /
            tileSize);

    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT;
         ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                uint16_t tileId = map.layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (tileId == 0 || isExploreActorForegroundTile(tileId)) continue;
                int x = tileX * tileSize - cameraX;
                int y = EXPLORE_ROUTE_MAP_TOP + tileY * tileSize - cameraY;
                if (!GameAssets::drawExploreTile(
                        tileId, x, y, animationFrame, AmoledUi::RESOURCE_SCALE)) {
                    drawExploreTileFallback(
                        canvas, tileId, x, y, layer, fieldColor, 1);
                }
            }
        }
    }
}

void drawExploreMapForeground(Canvas565& canvas,
                              const ExploreMapGenerator::Map& map,
                              int cameraX, int cameraY,
                              uint16_t fieldColor,
                              uint16_t rowBegin, uint16_t rowEnd) {
    constexpr int tileSize =
        ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    cameraX *= AmoledUi::RESOURCE_SCALE;
    cameraY *= AmoledUi::RESOURCE_SCALE;
    int firstX = std::max(0, cameraX / tileSize);
    int firstY = std::max(0, cameraY / tileSize);
    int lastX = std::min<int>(
        ExploreMapGenerator::WIDTH - 1,
        (cameraX + AmoledUi::WIDTH - 1) / tileSize);
    int lastY = std::min<int>(
        ExploreMapGenerator::HEIGHT - 1,
        (cameraY + (AmoledUi::HEIGHT - EXPLORE_ROUTE_MAP_TOP) - 1) /
            tileSize);

    for (int tileY = firstY; tileY <= lastY; ++tileY) {
        int y = EXPLORE_ROUTE_MAP_TOP + tileY * tileSize - cameraY;
        if (y + tileSize <= rowBegin || y >= rowEnd) continue;
        for (int tileX = firstX; tileX <= lastX; ++tileX) {
            uint16_t tileId = map.layers[2]
                [tileY * ExploreMapGenerator::WIDTH + tileX];
            if (tileId == 0 || !isExploreActorForegroundTile(tileId)) continue;
            int x = tileX * tileSize - cameraX;
            if (!GameAssets::drawExploreTile(
                    tileId, x, y, 0, AmoledUi::RESOURCE_SCALE)) {
                drawExploreTileFallback(
                    canvas, tileId, x, y, 2, fieldColor);
            }
        }
    }
}

void drawExploreMapAnimations(Canvas565& canvas,
                              const ExploreMapGenerator::Map& map,
                              int cameraX, int cameraY,
                              uint8_t animationFrame,
                              uint16_t rowBegin, uint16_t rowEnd,
                              uint16_t fieldColor) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    cameraX *= AmoledUi::RESOURCE_SCALE;
    cameraY *= AmoledUi::RESOURCE_SCALE;
    int firstX = std::max(0, cameraX / tileSize);
    int firstY = std::max(0, cameraY / tileSize);
    int lastX = std::min<int>(
        ExploreMapGenerator::WIDTH - 1,
        (cameraX + AmoledUi::WIDTH - 1) / tileSize);
    int lastY = std::min<int>(
        ExploreMapGenerator::HEIGHT - 1,
        (cameraY + (AmoledUi::HEIGHT - EXPLORE_ROUTE_MAP_TOP) - 1) /
            tileSize);

    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT;
         ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            int y = EXPLORE_ROUTE_MAP_TOP + tileY * tileSize - cameraY;
            if (y + tileSize <= rowBegin || y >= rowEnd) continue;
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                uint16_t tileId = map.layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (isExploreActorForegroundTile(tileId) ||
                    !GameAssets::isExploreTileAnimated(tileId)) continue;
                int x = tileX * tileSize - cameraX;
                if (!GameAssets::drawExploreTile(
                        tileId, x, y, animationFrame, AmoledUi::RESOURCE_SCALE)) {
                    drawExploreTileFallback(
                        canvas, tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
}

void drawExploreGrassAnimations(Canvas565& canvas, const ExploreRouteViewModel& model,
                                uint16_t rowBegin, uint16_t rowEnd) {
    if (!model.grass) return;
    constexpr int size = GRASS_SIZE * AmoledUi::RESOURCE_SCALE;
    const auto& map = *model.map;
    for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        const int x = (cell % ExploreMapGenerator::WIDTH * GRASS_SIZE - model.cameraX) * AmoledUi::RESOURCE_SCALE;
        const int y = EXPLORE_ROUTE_MAP_TOP +
            (cell / ExploreMapGenerator::WIDTH * GRASS_SIZE - model.cameraY) * AmoledUi::RESOURCE_SCALE;
        if (x + size <= model.renderColumnBegin || x >= model.renderColumnEnd ||
            y + size <= rowBegin || y >= rowEnd) continue;
        const auto base = grassFrame(model, cell, map.layers[0][cell]);
        const auto top = grassFrame(model, cell, 391);
        const bool movingBase = base.phase && ExploreTileSemantics::denseGrass(map.layers[0][cell]);
        const bool movingTop = top.phase && map.layers[1][cell] == 391;
        if (!movingBase && !movingTop) continue;
        // Recompose this cell in layer order, clearing the original transparent
        // grass top before drawing its displaced contour.
        for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
            const uint16_t id = map.layers[layer][cell];
            if (!id || isExploreActorForegroundTile(id)) continue;
            if ((layer == 0 && movingBase) || (layer == 1 && movingTop)) {
                drawGrass(canvas, id, x, y, layer ? top : base, false);
            } else if (!GameAssets::drawExploreTileTo(canvas, id, x, y,
                        model.mapFrame, AmoledUi::RESOURCE_SCALE)) {
                drawExploreTileFallback(canvas, id, x, y, layer, ExploreAreaCatalog::fieldColor(model.area));
            }
        }
    }
}

}  // namespace

bool drawExploreRouteMapLayer(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& exploreRouteWorldCache,
                                   uint16_t rowBegin, uint16_t rowEnd) {
    if (!model.map) return false;
#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
    const int64_t baseStartedUs = esp_timer_get_time();
    const uint16_t renderedRows = static_cast<uint16_t>(std::max(
        0, std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM) -
               std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP)));
#endif

    const int mapTop = EXPLORE_ROUTE_MAP_TOP;
    const int mapBottom = EXPLORE_ROUTE_MAP_BOTTOM;
    bool worldCacheHit = false;
    if (drawExploreRouteWorldViewport(
            canvas, model, exploreRouteWorldCache, rowBegin, rowEnd, &worldCacheHit)) {
#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
        const int64_t animationStartedUs = esp_timer_get_time();
#endif
        drawExploreMapAnimations(
            canvas, *model.map, model.cameraX, model.cameraY, model.mapFrame,
            rowBegin, rowEnd, ExploreAreaCatalog::fieldColor(model.area));
        drawExploreGrassAnimations(canvas, model, rowBegin, rowEnd);
#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
        if (model.walking) {
            const int64_t finishedUs = esp_timer_get_time();
            exploreMapPerf.record(
                worldCacheHit, renderedRows,
                static_cast<uint32_t>(animationStartedUs - baseStartedUs),
                static_cast<uint32_t>(finishedUs - animationStartedUs));
        }
#endif
        return true;
    }

    canvas.fillRect(
        0, mapTop, AmoledUi::WIDTH, mapBottom - mapTop,
        ExploreAreaCatalog::fieldColor(model.area));
    drawExploreMap(canvas, *model.map, model.cameraX, model.cameraY,
                   ExploreAreaCatalog::fieldColor(model.area), 0);

#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
    const int64_t animationStartedUs = esp_timer_get_time();
#endif
    drawExploreMapAnimations(
        canvas, *model.map, model.cameraX, model.cameraY, model.mapFrame,
        rowBegin, rowEnd, ExploreAreaCatalog::fieldColor(model.area));
    drawExploreGrassAnimations(canvas, model, rowBegin, rowEnd);
#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
    if (model.walking) {
        const int64_t finishedUs = esp_timer_get_time();
        exploreMapPerf.record(
            false, renderedRows,
            static_cast<uint32_t>(animationStartedUs - baseStartedUs),
            static_cast<uint32_t>(finishedUs - animationStartedUs));
    }
#endif
    return true;
}

void drawExploreRouteMapForegroundLayer(Canvas565& canvas,
                                        const ExploreRouteViewModel& model,
                                        uint16_t rowBegin, uint16_t rowEnd) {
    if (!model.map) return;
    drawExploreMapForeground(
        canvas, *model.map, model.cameraX, model.cameraY,
        ExploreAreaCatalog::fieldColor(model.area), rowBegin, rowEnd);
}

void drawExploreRouteMapVegetationForActor(
    Canvas565& canvas, const ExploreRouteViewModel& model,
    float actorFootWorldY, int actorLeft, int actorTop,
    int actorRight, int actorBottom, uint16_t rowBegin, uint16_t rowEnd,
    bool denseGrassCover, bool airborne, bool swimming) {
    if (!model.map) return;
    const int mapTop = std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP);
    const int mapBottom = std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM);
    const int left = std::max<int>(model.renderColumnBegin, actorLeft);
    const int top = std::max(mapTop, actorTop);
    const int right = std::min<int>(model.renderColumnEnd, actorRight);
    const int bottom = std::min(mapBottom, actorBottom);
    if (left >= right || top >= bottom) return;

    Canvas565& assets = PixelRenderer::canvas();
    canvas.setClipRect(left, top, right - left, bottom - top);
    if (&assets != &canvas) {
        assets.setClipRect(left, top, right - left, bottom - top);
    }
    constexpr int tileSize =
        ExploreRouteGeometry::TILE_SIZE * AmoledUi::RESOURCE_SCALE;
    const int cameraX = model.cameraX * AmoledUi::RESOURCE_SCALE;
    const int cameraY = model.cameraY * AmoledUi::RESOURCE_SCALE;
    const int firstX = std::max(0, (cameraX + left) / tileSize);
    const int firstY = std::max(0,
        (cameraY + top - EXPLORE_ROUTE_MAP_TOP) / tileSize);
    const int lastX = std::min<int>(ExploreMapGenerator::WIDTH - 1,
        (cameraX + right - 1) / tileSize);
    const int lastY = std::min<int>(ExploreMapGenerator::HEIGHT - 1,
        (cameraY + bottom - EXPLORE_ROUTE_MAP_TOP - 1) / tileSize);
    const uint16_t fieldColor = ExploreAreaCatalog::fieldColor(model.area);
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer) {
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                const uint16_t tileId = model.map->layers[layer]
                    [tileY * ExploreMapGenerator::WIDTH + tileX];
                if (airborne && ExploreTileSemantics::foregroundDepthOffsetRows(tileId) <= 1)
                    continue;
                const int x = tileX * tileSize - cameraX;
                const int y = EXPLORE_ROUTE_MAP_TOP + tileY * tileSize - cameraY;
                const int cell = tileY * ExploreMapGenerator::WIDTH + tileX;
                if (layer == 0 && denseGrassCover && ExploreTileSemantics::denseGrass(tileId) &&
                    actorFootWorldY < (tileY + 1) * GRASS_SIZE) {
                    drawGrass(canvas, tileId, x, y, grassFrame(model, cell, tileId), true);
                }
                const bool bridgeCover = swimming && !airborne && layer == 2 &&
                    ExploreTileSemantics::bridgeDeck(tileId);
                if (!bridgeCover && !ExploreTileSemantics::objectOccludes(
                        tileId, layer, tileY, actorFootWorldY,
                        ExploreRouteGeometry::TILE_SIZE)) continue;
                if (tileId == 391) {
                    drawGrass(canvas, tileId, x, y, grassFrame(model, cell, tileId), true);
                    continue;
                }
                if (!GameAssets::drawExploreTileTo(
                        canvas, tileId, x, y, model.mapFrame,
                        AmoledUi::RESOURCE_SCALE)) {
                    drawExploreTileFallback(canvas, tileId, x, y, layer, fieldColor);
                }
            }
        }
    }
    canvas.setClipRect(model.renderColumnBegin, mapTop,
        model.renderColumnEnd - model.renderColumnBegin, mapBottom - mapTop);
    if (&assets != &canvas) {
        assets.setClipRect(model.renderColumnBegin, mapTop,
        model.renderColumnEnd - model.renderColumnBegin, mapBottom - mapTop);
    }
}

}  // namespace AmoledV1
