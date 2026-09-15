#include "ui/ExploreMapRenderer.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "assets/GameAssets.h"
#include "core/BuildConfig.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreRouteGeometry.h"
#include "platform/api/PlatformServices.h"
#include "presentation/Canvas565.h"
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
                if (tileId == 0) continue;
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
    for (int row = top; row < bottom; ++row) {
        const uint16_t* source = exploreRouteWorldCache.data() +
            static_cast<size_t>(model.cameraY * AmoledUi::RESOURCE_SCALE + row) *
                EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH +
            model.cameraX * AmoledUi::RESOURCE_SCALE;
        uint16_t* firstOutput = canvas.rawPixels() +
            static_cast<size_t>(row) *
                canvas.physicalWidth();
        std::memcpy(firstOutput, source,
                    static_cast<size_t>(AmoledUi::WIDTH) * sizeof(uint16_t));
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
                if (tileId == 0) continue;
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
                if (!GameAssets::isExploreTileAnimated(tileId)) continue;
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

}  // namespace AmoledV1
