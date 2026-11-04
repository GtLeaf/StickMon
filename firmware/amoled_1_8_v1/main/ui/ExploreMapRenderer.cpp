#include "ui/ExploreMapRenderer.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstring>

#include "assets/GameAssets.h"
#include "core/BuildConfig.h"
#include "game/ExploreAreaCatalog.h"
#include "game/ExploreEditorTiles.h"
#include "game/ExploreLakeTiles.h"
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
    EXPLORE_ROUTE_WORLD_WIDTH * EXPLORE_ROUTE_SCALE;
constexpr int EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT =
    EXPLORE_ROUTE_WORLD_HEIGHT * EXPLORE_ROUTE_SCALE;

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
    constexpr float scale = EXPLORE_ROUTE_SCALE;
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
            const int left = int(std::ceil(column * scale));
            const int top = int(std::ceil(row * scale));
            canvas.fillRect(x + left, y + top,
                int(std::ceil((column + 1) * scale)) - left,
                int(std::ceil((row + 1) * scale)) - top, color);
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
                             uint16_t fieldColor, float outputScale =
                                 EXPLORE_ROUTE_SCALE) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE;
    auto scaled = [outputScale](int value) { return int(std::lround(value * outputScale)); };
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

bool animatedCell(const ExploreMapGenerator::Map& map, int cell) {
    for (uint8_t layer = 0; layer < ExploreMapGenerator::LAYER_COUNT; ++layer)
        if (GameAssets::isExploreTileAnimated(map.layers[layer][cell])) return true;
    return false;
}

RenderCacheKey exploreWorldKey(Canvas565& canvas, const ExploreRouteViewModel& model,
                               bool nightCache) {
    return {model.map->seed, uint32_t(model.area) | (nightCache ? 0x100U : 0U), EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH,
            EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT * (nightCache ? 2 : 1), canvas.byteSwapped()};
}

bool prepareExploreRouteWorldCache(Canvas565& canvas,
                                   const ExploreRouteViewModel& model,
                                   PixelCache565& exploreRouteWorldCache, bool& nightCache) {
    if (!model.map) return false;
    RenderCacheKey key = exploreWorldKey(canvas, model, nightCache);
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

    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
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
                        worldCanvas, tileId, x, y, 0, EXPLORE_ROUTE_SCALE)) {
                    drawExploreTileFallback(
                        worldCanvas, tileId, x, y, layer, fieldColor,
                        EXPLORE_ROUTE_SCALE);
                }
            }
        }
    }

    if (nightCache) {
        // A native 1x map has no duplicate rows. Keep the raw map followed by
        // its night-colored copy in one allocation (half the former 2x cache).
        constexpr size_t planePixels = size_t(EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH) *
                                       EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT;
        std::memcpy(pixels + planePixels, pixels, planePixels * sizeof(uint16_t));
        Canvas565 toneCanvas;
        toneCanvas.attach({pixels + planePixels, EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH,
            EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT, canvas.byteSwapped()});
        toneCanvas.multiplyRectRgb(0, 0, EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH,
            EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT,
            model.daylight.red, model.daylight.green, model.daylight.blue);
    }
    exploreRouteWorldCache.commit();
    return true;
}

bool drawExploreRouteWorldViewport(Canvas565& canvas,
                                   const ExploreRouteViewModel& model,
                                   PixelCache565& exploreRouteWorldCache,
                                   uint16_t rowBegin, uint16_t rowEnd,
                                   bool* worldCacheHit, bool& nightCache,
                                   const ExploreRouteTonePass* tonePass) {
    const bool cacheWasReady = model.map &&
        exploreRouteWorldCache.matches(exploreWorldKey(canvas, model, nightCache));
    if (canvas.physicalWidth() != AmoledUi::WIDTH ||
        canvas.physicalHeight() != AmoledUi::HEIGHT ||
        !prepareExploreRouteWorldCache(canvas, model, exploreRouteWorldCache, nightCache)) {
        if (worldCacheHit) *worldCacheHit = false;
        return false;
    }

    if (worldCacheHit) {
        *worldCacheHit = cacheWasReady;
    }

    const int viewTop = std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP);
    const int viewBottom = std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM);
    const int left = std::min<int>(model.renderColumnBegin, AmoledUi::WIDTH);
    const int right = std::clamp<int>(model.renderColumnEnd, left, AmoledUi::WIDTH);
    const int cameraX = exploreRoutePixel(model.cameraX);
    const int cameraY = exploreRoutePixel(model.cameraY) - EXPLORE_ROUTE_MAP_TOP;
    const int top = std::clamp(-cameraY, viewTop, viewBottom);
    const int bottom = std::clamp(EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT - cameraY, top, viewBottom);
    const int mapLeft = std::clamp(-cameraX, left, right);
    const int mapRight = std::clamp(EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH - cameraX, mapLeft, right);
    // Only decorate the margins; they never become traversable map cells.
    const uint16_t fieldColor = ExploreAreaCatalog::fieldColor(model.area);
    canvas.fillRect(left, viewTop, right - left, top - viewTop, fieldColor);
    canvas.fillRect(left, bottom, right - left, viewBottom - bottom, fieldColor);
    canvas.fillRect(left, top, mapLeft - left, bottom - top, fieldColor);
    canvas.fillRect(mapRight, top, right - mapRight, bottom - top, fieldColor);
    constexpr int size = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
    for (int row = top; row < bottom; ++row) {
        const int worldY = cameraY + row;
        for (int x = mapLeft; x < mapRight;) {
            int end = mapRight;
            int sourceY = worldY;
            if (nightCache) {
                const int cellY = worldY / size;
                int cellX = (exploreRoutePixel(model.cameraX) + x) / size;
                const bool raw = tonePass->cells[cellY * ExploreMapGenerator::WIDTH + cellX];
                // Copy each pixel only once, selecting raw or night rows by cell.
                do { ++cellX; }
                while (cellX < ExploreMapGenerator::WIDTH &&
                       tonePass->cells[cellY * ExploreMapGenerator::WIDTH + cellX] == raw);
                end = std::min(right, cellX * size - exploreRoutePixel(model.cameraX));
                sourceY = worldY + (raw ? 0 : EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT);
            }
            const uint16_t* source = exploreRouteWorldCache.data() +
                static_cast<size_t>(sourceY) * EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH +
                exploreRoutePixel(model.cameraX) + x;
            std::memcpy(canvas.rawPixels() + static_cast<size_t>(row) * canvas.physicalWidth() + x,
                source, static_cast<size_t>(end - x) * sizeof(uint16_t));
            x = end;
        }
    }
    return true;
}

void drawExploreMap(Canvas565& canvas,
                    const ExploreMapGenerator::Map& map,
                    int cameraX, int cameraY, uint16_t fieldColor,
                    uint8_t animationFrame) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
    cameraX = exploreRoutePixel(cameraX);
    cameraY = exploreRoutePixel(cameraY);
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
                        tileId, x, y, animationFrame, EXPLORE_ROUTE_SCALE)) {
                    drawExploreTileFallback(
                        canvas, tileId, x, y, layer, fieldColor);
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
                              uint16_t columnBegin, uint16_t columnEnd,
                              uint16_t fieldColor) {
    constexpr int tileSize = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
    cameraX = exploreRoutePixel(cameraX);
    cameraY = exploreRoutePixel(cameraY);
    const int top = std::max<int>(rowBegin, EXPLORE_ROUTE_MAP_TOP);
    const int bottom = std::min<int>(rowEnd, EXPLORE_ROUTE_MAP_BOTTOM);
    if (columnBegin >= columnEnd || top >= bottom) return;
    const int firstX = std::max(0, (cameraX + columnBegin) / tileSize);
    const int firstY = std::max(0, (cameraY + top - EXPLORE_ROUTE_MAP_TOP) / tileSize);
    const int lastX = std::min<int>(ExploreMapGenerator::WIDTH - 1,
        (cameraX + columnEnd - 1) / tileSize);
    const int lastY = std::min<int>(ExploreMapGenerator::HEIGHT - 1,
        (cameraY + bottom - EXPLORE_ROUTE_MAP_TOP - 1) / tileSize);

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
                // Transparent object frames must replace the cached pose. Rebuild
                // this cell from ground to foreground so no old silhouette leaks
                // through and higher layers remain in front of the animation.
                if (ExploreEditorTiles::tile(tileId) || ExploreLakeTiles::contains(tileId)) {
                    canvas.fillRect(x, y, tileSize, tileSize, fieldColor);
                    for (uint8_t z = 0; z < ExploreMapGenerator::LAYER_COUNT; ++z) {
                        const uint16_t id = map.layers[z][tileY * ExploreMapGenerator::WIDTH + tileX];
                        if (id && !GameAssets::drawExploreTile(id, x, y, animationFrame, EXPLORE_ROUTE_SCALE))
                            drawExploreTileFallback(canvas, id, x, y, z, fieldColor);
                    }
                    continue;
                }
                if (!GameAssets::drawExploreTile(
                        tileId, x, y, animationFrame, EXPLORE_ROUTE_SCALE)) {
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
    constexpr int size = GRASS_SIZE * EXPLORE_ROUTE_SCALE;
    const auto& map = *model.map;
    for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
        const int x = exploreRouteScreen(cell % ExploreMapGenerator::WIDTH * GRASS_SIZE, model.cameraX);
        const int y = EXPLORE_ROUTE_MAP_TOP +
            exploreRouteScreen(cell / ExploreMapGenerator::WIDTH * GRASS_SIZE, model.cameraY);
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
            if (!id) continue;
            if ((layer == 0 && movingBase) || (layer == 1 && movingTop)) {
                drawGrass(canvas, id, x, y, layer ? top : base, false);
            } else if (!GameAssets::drawExploreTileTo(canvas, id, x, y,
                        model.mapFrame, EXPLORE_ROUTE_SCALE)) {
                drawExploreTileFallback(canvas, id, x, y, layer, ExploreAreaCatalog::fieldColor(model.area));
            }
        }
    }
}

}  // namespace

void ExploreRouteTonePass::include(const ExploreRouteViewModel& model, AmoledUi::Rect bounds) {
    constexpr int size = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
    const int left = std::max<int>(model.renderColumnBegin, std::max(0, bounds.x));
    const int top = std::max(EXPLORE_ROUTE_MAP_TOP, bounds.y);
    const int right = std::min<int>(model.renderColumnEnd, std::min(AmoledUi::WIDTH, bounds.x + bounds.width));
    const int bottom = std::min(EXPLORE_ROUTE_MAP_BOTTOM, bounds.y + bounds.height);
    if (left >= right || top >= bottom) return;
    const int x0 = std::max(0, (left + exploreRoutePixel(model.cameraX)) / size);
    const int y0 = std::max(0, (top + exploreRoutePixel(model.cameraY)) / size);
    const int x1 = std::min<int>(ExploreMapGenerator::WIDTH - 1,
        (right - 1 + exploreRoutePixel(model.cameraX)) / size);
    const int y1 = std::min<int>(ExploreMapGenerator::HEIGHT - 1,
        (bottom - 1 + exploreRoutePixel(model.cameraY)) / size);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) cells[y * ExploreMapGenerator::WIDTH + x] = true;
}

void ExploreRouteTonePass::finish(Canvas565& canvas, const ExploreRouteViewModel& model) const {
    constexpr int size = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
    const int cameraX = exploreRoutePixel(model.cameraX);
    const int cameraY = exploreRoutePixel(model.cameraY) - EXPLORE_ROUTE_MAP_TOP;
    const int top = std::clamp(-cameraY, EXPLORE_ROUTE_MAP_TOP, EXPLORE_ROUTE_MAP_BOTTOM);
    const int bottom = std::clamp(EXPLORE_ROUTE_WORLD_PHYSICAL_HEIGHT - cameraY, top, EXPLORE_ROUTE_MAP_BOTTOM);
    const int left = std::clamp(-cameraX, 0, AmoledUi::WIDTH);
    const int right = std::clamp(EXPLORE_ROUTE_WORLD_PHYSICAL_WIDTH - cameraX, left, AmoledUi::WIDTH);
    // Margins use raw backdrop colors and may also contain a departing actor.
    // Tint each margin exactly once, independently of map-cell damage.
    auto tint = [&](int x, int y, int width, int height) {
        canvas.multiplyRectRgb(x, y, width, height,
            model.daylight.red, model.daylight.green, model.daylight.blue);
    };
    tint(0, EXPLORE_ROUTE_MAP_TOP, AmoledUi::WIDTH, top - EXPLORE_ROUTE_MAP_TOP);
    tint(0, bottom, AmoledUi::WIDTH, EXPLORE_ROUTE_MAP_BOTTOM - bottom);
    tint(0, top, left, bottom - top);
    tint(right, top, AmoledUi::WIDTH - right, bottom - top);
    // Merge adjacent cells, and never multiply an overlapping actor/animation twice.
    for (int y = 0; y < ExploreMapGenerator::HEIGHT; ++y) {
        for (int x = 0; x < ExploreMapGenerator::WIDTH;) {
            if (!cells[y * ExploreMapGenerator::WIDTH + x]) { ++x; continue; }
            const int first = x++;
            while (x < ExploreMapGenerator::WIDTH && cells[y * ExploreMapGenerator::WIDTH + x]) ++x;
            canvas.multiplyRectRgb(first * size - exploreRoutePixel(model.cameraX),
                y * size - exploreRoutePixel(model.cameraY),
                (x - first) * size, size,
                model.daylight.red, model.daylight.green, model.daylight.blue);
        }
    }
}

bool drawExploreRouteMapLayer(Canvas565& canvas,
                              const ExploreRouteViewModel& model,
                              PixelCache565& exploreRouteWorldCache,
                                   uint16_t rowBegin, uint16_t rowEnd,
                                   ExploreRouteTonePass* tonePass) {
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
    // Twilight changes each game minute: retain the original path there rather
    // than rebuilding the world repeatedly. Only the stable night tone is cached.
    bool nightCache = tonePass && model.daylight == ExploreDaylight::Tone{128, 152, 192};
    if (tonePass) tonePass->active = false;
    if (nightCache) {
        constexpr int size = ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
        for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
            const int x = exploreRouteScreen(cell % ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE, model.cameraX);
            const int y = exploreRouteScreen(cell / ExploreMapGenerator::WIDTH * ExploreRouteGeometry::TILE_SIZE, model.cameraY);
            if (x + size <= model.renderColumnBegin || x >= model.renderColumnEnd ||
                y + size <= rowBegin || y >= rowEnd) continue;
            const bool grass = model.grass &&
                ((ExploreTileSemantics::denseGrass(model.map->layers[0][cell]) &&
                  grassFrame(model, cell, model.map->layers[0][cell]).phase) ||
                 (model.map->layers[1][cell] == 391 && grassFrame(model, cell, 391).phase));
            tonePass->cells[cell] = tonePass->cells[cell] || animatedCell(*model.map, cell) || grass;
        }
    }
    if (drawExploreRouteWorldViewport(
            canvas, model, exploreRouteWorldCache, rowBegin, rowEnd, &worldCacheHit, nightCache, tonePass)) {
        if (tonePass) tonePass->active = nightCache;
#if defined(ESP_PLATFORM) && STICKMON_ENABLE_DEBUG_FEATURES
        const int64_t animationStartedUs = esp_timer_get_time();
#endif
        drawExploreMapAnimations(
            canvas, *model.map, model.cameraX, model.cameraY, model.mapFrame,
            rowBegin, rowEnd, model.renderColumnBegin, model.renderColumnEnd,
            ExploreAreaCatalog::fieldColor(model.area));
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
        rowBegin, rowEnd, model.renderColumnBegin, model.renderColumnEnd,
        ExploreAreaCatalog::fieldColor(model.area));
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
        ExploreRouteGeometry::TILE_SIZE * EXPLORE_ROUTE_SCALE;
    const int cameraX = exploreRoutePixel(model.cameraX);
    const int cameraY = exploreRoutePixel(model.cameraY);
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
                if (airborne && !ExploreTileSemantics::upwardLadder(tileId, layer) &&
                    ExploreTileSemantics::foregroundDepthOffsetRows(tileId) <= 1)
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
                        EXPLORE_ROUTE_SCALE)) {
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
