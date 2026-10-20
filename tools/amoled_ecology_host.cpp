#include "AmoledApp.h"
#include "HomeScreen.h"
#include "core/SaveCodec.h"
#include "game/GameRandom.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "ui/ExploreMapRenderer.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace AmoledV1 {
namespace {
int randomCalls = 0;
uint32_t countingRandom(uint32_t minimum, uint32_t) {
    ++randomCalls;
    return minimum;
}
class WaterCacheAllocator : public Platform::IMemoryAllocator {
public:
    WaterCacheAllocator(Platform::IMemoryAllocator& backing, bool fail)
        : backing_(backing), fail_(fail) {}
    void* allocate(size_t bytes, bool external) override {
        return fail_ && bytes >= 368 * 448 * 2 ? nullptr : backing_.allocate(bytes, external);
    }
    void release(void* memory) override { backing_.release(memory); }
    size_t externalFree() const override { return backing_.externalFree(); }
private:
    Platform::IMemoryAllocator& backing_;
    bool fail_;
};
std::vector<uint16_t> pixels(Canvas565& canvas) {
    return {canvas.rawPixels(), canvas.rawPixels() + canvas.pixelCount()};
}
void writePreviewPixels(const std::vector<uint16_t>& frame, const char* name,
                        const char* outputVariable = "AMOLED_ECOLOGY_OUTPUT") {
    const char* directory = std::getenv(outputVariable);
    if (!directory || !*directory) return;
    const std::string path = std::string(directory) + "/" + name + ".ppm";
    FILE* file = std::fopen(path.c_str(), "wb");
    assert(file);
    std::fprintf(file, "P6\n368 448\n255\n");
    for (uint16_t pixel : frame) {
        const uint8_t rgb[] = {
            static_cast<uint8_t>(((pixel >> 11) & 31) * 255 / 31),
            static_cast<uint8_t>(((pixel >> 5) & 63) * 255 / 63),
            static_cast<uint8_t>((pixel & 31) * 255 / 31)};
        std::fwrite(rgb, 1, sizeof(rgb), file);
    }
    std::fclose(file);
}
void writePreview(Canvas565& canvas, const char* name) {
    if (std::getenv("AMOLED_ECOLOGY_OUTPUT")) writePreviewPixels(pixels(canvas), name);
}
}

struct AmoledEcologyTestAccess {
    static void checkWildlifeGrassApp(DesktopPlatform& desktop) {
        for (uint16_t species : {uint16_t(161), uint16_t(41)}) {
          for (bool hidden : {false, true}) {
            AmoledApp app;
            app.begin(desktop.millis());
            assert(app.startExploreRoute(desktop.millis()));
            app.expeditionFade.reset();
            app.gameState.teamCount = 1;
            app.gameState.team[0].speciesId = 161;
            app.explorePresenter_.state_.exploreRouteWorldX = app.explorePresenter_.state_.exploreRouteWorldY = -1000;
            auto& map = app.exploreSession_.state_.exploreRouteMap;
            map = {};
            map.seed = 773;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 390);
            map.pathCount = 1;
            map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            ExplorePool::Pool pool{};
            pool.count = 1;
            pool.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
            app.explorePresenter_.state_.exploreEcology.reset(map, 0, pool, hidden ? 1 : 0, desktop.millis());
            for (uint32_t seed = 774; app.explorePresenter_.state_.exploreEcology.hasGroup() && seed < 838; ++seed) {
                map.seed = seed;
                app.explorePresenter_.state_.exploreEcology.reset(map, 0, pool, hidden ? 1 : 0, desktop.millis());
            }
            assert(!app.explorePresenter_.state_.exploreEcology.hasGroup() && app.explorePresenter_.state_.exploreEcology.count() == 1);
            app.explorePresenter_.state_.exploreGrass.reset(desktop.millis());
            app.updateExploreEcology(desktop.millis());
            bool feedback = false;
            for (int tick = 0; tick < 300; ++tick) {
                desktop.advanceMs(100);
                app.updateExploreEcology(desktop.millis());
                for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell)
                    feedback |= app.explorePresenter_.state_.exploreGrass.frame(cell).phase != 0;
            }
            assert(feedback == (species == 161 && !hidden));
          }
        }
        std::puts("wildlife grass: real grounded wildlife triggers rebound; hidden and airborne actors do not");
    }
    static void checkWaterBridge(DesktopPlatform& desktop, Canvas565& canvas) {
        using ExploreTileSemantics::ActorDepth;
        using ExploreTileSemantics::ActorRole;
        ActorDepth order[] = {{ActorRole::LEADER, 100}, {ActorRole::COMPANION, 90},
            {ActorRole::BOSS, 80}, {ActorRole::WILDLIFE, 150, 0, true},
            {ActorRole::WILDLIFE, 160, 1, true}};
        ExploreTileSemantics::sortActorsByGroundDepth(order, 5);
        assert(order[0].belowBridge && order[0].index == 0);
        assert(order[1].belowBridge && order[1].index == 1);
        assert(order[2].role == ActorRole::BOSS && order[4].role == ActorRole::LEADER);
        ExploreMapGenerator::Map map{};
        assert(ExploreMapGenerator::generate(12, ExploreMapGenerator::Edge::RIGHT, 1, map));
        int bridgeY = -1, riverX = -1;
        for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
            if (map.layers[2][cell] == 1627 && map.layers[0][cell] == 1097) {
                bridgeY = cell / 16; riverX = cell % 16; break;
            }
        }
        assert(bridgeY >= 0 && riverX >= 0);
        const uint16_t species[] = {129, 130, 161, 133, 278};
        PokemonSprites::preloadDynamicSpecies(species, 5);
        int compared = 0, visibleFrames = 0, walkerPixels = 0;
        for (bool cached : {false, true}) {
          auto& original = desktop.serviceBundle();
          WaterCacheAllocator allocator(original.memory, !cached);
          Platform::Services services{original.lifecycle, original.clock, original.logger,
              original.input, original.display, original.audio, original.imu, original.power,
              original.blobs, original.resources, allocator, original.peers};
          Platform::bind(services);
          for (int cameraOffset : {0, 13}) {
            ExploreRouteViewModel model{};
            model.area = 1;
            model.map = &map;
            model.speciesId = 161;
            model.cameraX = std::clamp(riverX * 26 - 92, 0, 232);
            model.cameraY = cameraOffset;
            model.worldX = model.worldY = -1000;
            PixelCache565 cache;
            // Record the real atlas opacity, including transparent plank gaps.
            canvas.fillSprite(0xF81F);
            for (int cell = 0; cell < ExploreMapGenerator::CELL_COUNT; ++cell) {
                const uint16_t tile = map.layers[2][cell];
                if (!ExploreTileSemantics::bridgeDeck(tile)) continue;
                assert(GameAssets::drawExploreTileTo(canvas, tile,
                    (cell % 16 * 26 - model.cameraX) * 2,
                    EXPLORE_ROUTE_MAP_TOP + (cell / 16 * 26 - model.cameraY) * 2, 0, 2));
            }
            const auto deck = pixels(canvas);
            renderExploreRouteScreen(canvas, model, cache);
            assert((cache.data() != nullptr) == cached);
            const auto empty = pixels(canvas);
            for (bool walker : {false, true}) {
              model.worldX = walker ? ExploreRouteGeometry::tileCenter(riverX) : -1000;
              model.worldY = walker ? (bridgeY + 0.5f) * 26 : -1000;
              model.wildlifeCount = 0;
              renderExploreRouteScreen(canvas, model, cache);
              const auto baseline = pixels(canvas);
              if (walker) {
                  for (size_t i = 0; i < deck.size(); ++i)
                      walkerPixels += deck[i] != 0xF81F && baseline[i] != empty[i];
              }
              model.wildlifeCount = 1;
              for (uint16_t fish : {uint16_t(129), uint16_t(130)}) {
                for (bool hidden : {false, true}) {
                  for (int step = 0; step <= 20; ++step) {
                    auto& pose = model.wildlife[0];
                    pose = {};
                    pose.speciesId = fish;
                    pose.x = ExploreRouteGeometry::tileCenter(riverX);
                    pose.y = bridgeY * 26 - 20 + step * 5;
                    pose.surface = ExploreEcology::Surface::WATER;
                    pose.hidden = hidden;
                    pose.frame = step % 3;
                    pose.activity = ExploreEcology::Activity::SPLASH;
                    pose.lift = step % 5;
                    renderExploreRouteScreen(canvas, model, cache);
                    const auto full = pixels(canvas);
                    bool visible = false;
                    for (size_t i = 0; i < deck.size(); ++i) {
                        if (deck[i] != 0xF81F) assert(full[i] == baseline[i]);
                        else visible |= full[i] != baseline[i];
                    }
                    visibleFrames += visible;
                    canvas.fillSprite(0xF81F);
                    for (int y = 0; y < 448; y += 17)
                        renderExploreRouteScreen(canvas, model, cache, y, std::min(y + 17, 448));
                    assert(pixels(canvas) == full);
                    // A moving fish must also erase its old pixels in a narrow dirty rectangle.
                    const auto old = exploreRouteWildlifeBounds(model, 0);
                    pose.y += 3;
                    const auto next = exploreRouteWildlifeBounds(model, 0);
                    model.renderColumnBegin = std::max(0, std::min(old.x, next.x));
                    model.renderColumnEnd = std::min(368, std::max(old.x + old.width, next.x + next.width));
                    renderExploreRouteScreen(canvas, model, cache,
                        std::max(0, std::min(old.y, next.y)),
                        std::min(EXPLORE_ROUTE_MAP_BOTTOM, std::max(old.y + old.height, next.y + next.height)));
                    const auto partial = pixels(canvas);
                    model.renderColumnBegin = 0; model.renderColumnEnd = 368;
                    renderExploreRouteScreen(canvas, model, cache);
                    assert(pixels(canvas) == partial);
                    ++compared;
                    if (cached && cameraOffset == 0 && !hidden && step % 4 == 0) {
                        char name[64];
                        std::snprintf(name, sizeof(name), "fish-%u-%s-%02d", fish,
                            walker ? "bridge-walker" : "river", step);
                        writePreviewPixels(partial, name, "AMOLED_WATER_OUTPUT");
                    }
                  }
                }
              }
            }
          }
          Platform::bind(original);
        }
        assert(visibleFrames > 0 && walkerPixels > 0 && compared == 672);
        std::printf("bridge: %d fish/ripple/splash/hidden/walker frames, atlas opacity, cameras, dirty rectangles and cache fallback verified\n", compared);
    }
    static void checkArea(DesktopPlatform& desktop, Canvas565& canvas, uint8_t area) {
        AmoledApp app;
        app.begin(desktop.millis());
        for (auto& count : app.gameState.explorePoolRerollCounts) count = 1;
        app.selectedExploreArea = area;
        GameRandom::setRangeProvider(countingRandom);
        assert(app.startExploreRoute(desktop.millis()));
        assert(app.selectedExploreArea == area && app.explorePresenter_.state_.exploreEcology.count() > 0);
        for (uint8_t entry = 0; entry < app.exploreSession_.state_.exploreRoutePool.count; ++entry) {
            const auto* family = ExploreEcology::groupFamily(app.exploreSession_.state_.exploreRoutePool.entries[entry].speciesId);
            if (!family) continue;
            for (uint8_t i = 0; i < family->count; ++i) {
                PokemonSprites::WalkingAnimation animation{};
                assert(PokemonSprites::walkingAnimation(family->species[i],
                    PokemonSprites::WalkDirection::DOWN, animation));
                assert(PokemonSprites::findCachedSpeciesSprite(family->species[i], animation.base));
            }
        }
        app.exploreSession_.state_.exploreRouteBossPending = false;
        app.exploreSession_.state_.exploreRouteBossScheduled = false;
        const auto mapFingerprint = ExploreMapGenerator::fingerprint(app.exploreSession_.state_.exploreRouteMap);
        const auto state = app.gameState;
        const auto history = app.encounterHistory;
        const int callsBefore = randomCalls;
        // Observe from a real route point near the first wild actor.
        const auto target = app.explorePresenter_.state_.exploreEcology.pose(0);
        const auto& path = app.exploreSession_.state_.exploreRouteMap.paths[app.exploreSession_.state_.exploreRoutePath];
        float nearest = 1e9f;
        for (uint8_t p = 0; p < path.pointCount; ++p) {
            const auto point = ExploreRouteGeometry::pathPoint(path, p);
            const float dx = point.x - target.x, dy = point.y - target.y;
            if (dx * dx + dy * dy < nearest) {
                nearest = dx * dx + dy * dy;
                app.explorePresenter_.state_.exploreRouteWorldX = point.x;
                app.explorePresenter_.state_.exploreRouteWorldY = point.y;
            }
        }
        app.explorePresenter_.updateCamera();
        app.requestFullRender();
        app.render(canvas);
        app.markRendered();
        char preview[48];
        std::snprintf(preview, sizeof(preview), "area-%u-ecology", area);
        writePreview(canvas, preview);
        int redraws = 0;
        for (int tick = 0; tick < 120; ++tick) {
            desktop.advanceMs(100);
            app.updateExploreEcology(desktop.millis());
            if (!app.needsRender()) continue;
            ++redraws;
            assert(app.renderRowEnd() <= EXPLORE_ROUTE_MAP_BOTTOM);
            assert(app.renderRowEnd() - app.renderRowBegin() < EXPLORE_ROUTE_MAP_BOTTOM);
            app.render(canvas);
            const auto partial = pixels(canvas);
            app.requestFullRender();
            app.render(canvas);
            const auto full = pixels(canvas);
            if (partial != full) {
                for (size_t p = 0; p < partial.size(); ++p) {
                    if (partial[p] == full[p]) continue;
                    std::fprintf(stderr, "ecology tick=%d: partial mismatch at %zu,%zu (%04x/%04x)\n",
                                 tick, p % 368, p / 368, partial[p], full[p]);
                    break;
                }
                assert(false);
            }
            app.markRendered();
        }
        assert(redraws > 5); // Continuous fliers can legitimately redraw every tick.
        assert(randomCalls == callsBefore);
        assert(std::memcmp(&state, &app.gameState, sizeof(state)) == 0);
        assert(std::memcmp(&history, &app.encounterHistory, sizeof(history)) == 0);
        assert(mapFingerprint == ExploreMapGenerator::fingerprint(app.exploreSession_.state_.exploreRouteMap));
        ExploreEcology::Pose paused[ExploreEcology::CAPACITY];
        for (uint8_t i = 0; i < app.explorePresenter_.state_.exploreEcology.count(); ++i)
            paused[i] = app.explorePresenter_.state_.exploreEcology.pose(i);
        for (int reason = 0; reason < 4; ++reason) {
            app.exploreSession_.state_.exploreRoutePaused = reason == 0;
            app.exploreSession_.state_.exploreRouteComplete = reason == 1;
            app.sceneFlow.enter(reason == 2 ? AppSceneFlow::Scene::BATTLE
                : reason == 3 ? AppSceneFlow::Scene::EXPLORE_MENU
                              : AppSceneFlow::Scene::EXPLORE_ROUTE);
            desktop.advanceMs(10000);
            app.updateExploreEcology(desktop.millis());
            for (uint8_t i = 0; i < app.explorePresenter_.state_.exploreEcology.count(); ++i)
                assert(ExploreEcology::samePose(paused[i], app.explorePresenter_.state_.exploreEcology.pose(i)));
            assert(!app.needsRender());
        }
        // Camera changes affect screen position, not map population or location.
        auto model = app.exploreRouteModel();
        const auto world = model.wildlife[0];
        const auto originalBounds = exploreRouteWildlifeBounds(model, 0);
        model.cameraX += 10;
        model.cameraY += 10;
        const auto shiftedBounds = exploreRouteWildlifeBounds(model, 0);
        assert(shiftedBounds.x == originalBounds.x - 20);
        assert(shiftedBounds.y == originalBounds.y - 20);
        assert(ExploreEcology::samePose(world, app.explorePresenter_.state_.exploreEcology.pose(0)));

        // Exercise unknown/known species and depth overlap in every dirty band.
        model = app.exploreRouteModel();
        model.complete = false;
        model.exitConfirm = false;
        model.wildlifeCount = 2;
        model.wildlife[0] = {16, model.worldX - 20, model.worldY + 6, 1, 1, true, false};
        model.wildlife[1] = {133, model.worldX + 22, model.worldY - 10, 0, 2, false, true};
        PixelCache565 cache;
        for (auto surface : {ExploreEcology::Surface::GRASS, ExploreEcology::Surface::ROCK,
                             ExploreEcology::Surface::ICE, ExploreEcology::Surface::WATER}) {
            for (bool hidden : {true, false}) {
                model.wildlife[1].hidden = hidden;
                model.wildlife[1].surface = surface;
                for (const auto& pose : model.wildlife) {
                    const uint16_t id = pose.speciesId;
                    PokemonSprites::preloadDynamicSpecies(&id, 1);
                }
                for (uint8_t frame = 0; frame < 3; ++frame) {
                    model.wildlife[1].frame = frame;
                    renderExploreRouteScreen(canvas, model, cache);
                    const auto full = pixels(canvas);
                    if (frame == 2) {
                        std::snprintf(preview, sizeof(preview), "area-%u-surface-%u-%s", area,
                            unsigned(surface), hidden ? "hidden" : "revealed");
                        writePreview(canvas, preview);
                    }
                    canvas.fillSprite(0xF81F);
                    for (int y = 0; y < 448; y += 7)
                        renderExploreRouteScreen(canvas, model, cache, y, std::min(y + 7, 448));
                    assert(pixels(canvas) == full);
                    const auto bounds = exploreRouteWildlifeBounds(model, 1);
                    model.wildlife[1].frame = (frame + 1) % 3;
                    model.renderColumnBegin = std::max(0, bounds.x);
                    model.renderColumnEnd = std::min(int(AmoledUi::WIDTH), bounds.x + bounds.width);
                    renderExploreRouteScreen(canvas, model, cache,
                        std::max(0, bounds.y), std::min(EXPLORE_ROUTE_MAP_BOTTOM, bounds.y + bounds.height));
                    const auto partial = pixels(canvas);
                    model.renderColumnBegin = 0;
                    model.renderColumnEnd = AmoledUi::WIDTH;
                    renderExploreRouteScreen(canvas, model, cache);
                    assert(pixels(canvas) == partial);
                }
            }
        }
        GameRandom::setRangeProvider(nullptr);
        std::printf("AMOLED ecology area %u: %d partial frames, pause, camera, depth and gameplay isolation verified\n", area, redraws);
    }
    static void checkCover(Canvas565& canvas) {
        struct Object { uint16_t tile; uint8_t layer; int rows; };
        const Object objects[] = {{391, 1, 2}, {804, 1, 3}, {812, 1, 2},
            {820, 1, 1}, {804, 2, 3}, {818, 0, 1}, {859, 1, 1}, {1231, 2, 1},
            {1506, 2, 2}, {1514, 2, 1}, {1532, 2, 1}, {4507, 1, 1},
            {4541, 2, 2}, {4542, 1, 1}, {4532, 1, 3}, {4540, 1, 1}};
        constexpr uint16_t emptyColor = 0xF81F, actorColor = 0x07FF;
        constexpr int tileX = 3, tileY = 3, tileSize = 52;
        for (uint8_t area = 0; area < 6; ++area) {
          for (const auto& object : objects) {
            ExploreMapGenerator::Map map{};
            map.areaIndex = area;
            map.seed = 1000 + area * 100 + object.tile;
            map.layers[object.layer][tileY * ExploreMapGenerator::WIDTH + tileX] = object.tile;
            ExploreRouteViewModel model{};
            model.map = &map;
            model.area = area;
            model.worldX = model.worldY = -1000;
            const int x = tileX * tileSize, y = tileY * tileSize;
            canvas.fillSprite(emptyColor);
            assert(GameAssets::drawExploreTileTo(canvas, object.tile, x, y, 0, 2));
            const auto tile = pixels(canvas);
            const float ground = (tileY + object.rows) * 26;
            for (bool behind : {true, false}) {
                canvas.fillSprite(actorColor);
                drawExploreRouteMapVegetationForActor(canvas, model, ground - (behind ? 0.01f : 0),
                    x + 2, y + 2, x + tileSize - 2, y + tileSize - 2, 0, EXPLORE_ROUTE_MAP_BOTTOM);
                const auto restored = pixels(canvas);
                unsigned count = 0;
                for (int py = y; py < y + tileSize; ++py) {
                  for (int px = x; px < x + tileSize; ++px) {
                    const auto index = py * 368 + px;
                    const bool opaque = behind && tile[index] != emptyColor &&
                        px >= x + 2 && px < x + tileSize - 2 &&
                        py >= y + 2 && py < y + tileSize - 2;
                    assert(restored[index] == (opaque ? tile[index] : actorColor));
                    count += opaque;
                  }
                }
                if (behind) assert(count > 0);
            }
          }
        }
        // Dense ground grass must preserve gaps between blades and everything
        // above their top, while restoring colors from the original map tile.
        for (uint16_t grass : {uint16_t(390), uint16_t(399)}) {
            ExploreMapGenerator::Map map{};
            map.layers[0][tileY * ExploreMapGenerator::WIDTH + tileX] = grass;
            ExploreRouteViewModel model{};
            model.map = &map;
            canvas.fillSprite(emptyColor);
            assert(GameAssets::drawExploreTileTo(canvas, grass, tileX * tileSize, tileY * tileSize, 0, 2));
            const auto terrain = pixels(canvas);
            canvas.fillSprite(actorColor);
            const int ground = tileY * tileSize + 26;
            drawExploreRouteMapVegetationForActor(canvas, model, ground / 2.0f,
                tileX * tileSize, tileY * tileSize, (tileX + 1) * tileSize, (tileY + 1) * tileSize,
                0, EXPLORE_ROUTE_MAP_BOTTOM, true);
            unsigned restored = 0, gaps = 0;
            const auto output = pixels(canvas);
            for (int y = tileY * tileSize; y < (tileY + 1) * tileSize; ++y) {
                for (int x = tileX * tileSize; x < (tileX + 1) * tileSize; ++x) {
                    const int index = y * 368 + x;
                    if (grass == 390 && y < tileY * tileSize + 20)
                        assert(output[index] == actorColor);
                    if (output[index] == actorColor) ++gaps;
                    else { assert(output[index] == terrain[index]); ++restored; }
                }
            }
            assert(restored > 0 && gaps > 0);
        }
        std::puts("cover: all six areas preserve grass gaps and restore real tree/shrub/rock contours only behind their ground depth");
    }
    static void checkDaylightFrames(Canvas565& canvas) {
        int compared = 0;
        for (uint8_t area = 0; area < 6; ++area) {
            ExploreMapGenerator::Map map{};
            assert(ExploreMapGenerator::generate(12152 + area, ExploreMapGenerator::Edge::LEFT, area, map));
            const uint32_t fingerprint = ExploreMapGenerator::fingerprint(map);
            const auto& path = map.paths[0];
            const auto point = ExploreRouteGeometry::pathPoint(path, path.pointCount / 2);
            ExploreRouteViewModel model{};
            model.map = &map;
            model.area = area;
            model.speciesId = 161;
            model.worldX = point.x; model.worldY = point.y;
            model.cameraX = std::clamp(int(point.x) - 92, 0, 232);
            model.cameraY = std::clamp(int(point.y) - 86, 0, 140);
            model.wildlifeCount = 1;
            model.wildlife[0] = {285, point.x + 30, point.y + 10, 1, 0, false, false};
            const uint16_t species[] = {161, 285};
            PokemonSprites::preloadDynamicSpecies(species, 2);
            PixelCache565 cache;
            renderExploreRouteScreen(canvas, model, cache);
            const auto day = pixels(canvas);
            for (uint16_t minute : {uint16_t(300), uint16_t(720), uint16_t(1140), uint16_t(1320)}) {
                model.daylight = ExploreDaylight::at(minute, area);
                renderExploreRouteScreen(canvas, model, cache);
                const auto full = pixels(canvas);
                assert(std::equal(day.begin() + EXPLORE_ROUTE_MAP_BOTTOM * 368, day.end(),
                                  full.begin() + EXPLORE_ROUTE_MAP_BOTTOM * 368));
                if (area == ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA || minute == 720)
                    assert(day == full);
                else assert(day != full);
                char name[64];
                std::snprintf(name, sizeof(name), "daylight-area-%u-%04u", area, minute);
                writePreview(canvas, name);
                canvas.fillSprite(0xF81F);
                for (int row = 0; row < 448; row += 11)
                    renderExploreRouteScreen(canvas, model, cache, row, std::min(row + 11, 448));
                assert(full == pixels(canvas));
                for (int tick = 0; tick < 12; ++tick) {
                    const auto old = exploreRouteWildlifeBounds(model, 0);
                    model.wildlife[0].x += tick % 2 ? -3 : 3;
                    model.wildlife[0].frame = tick % 3;
                    const auto current = exploreRouteWildlifeBounds(model, 0);
                    model.renderColumnBegin = std::max(0, std::min(old.x, current.x));
                    model.renderColumnEnd = std::min(368, std::max(old.x + old.width, current.x + current.width));
                    renderExploreRouteScreen(canvas, model, cache, std::max(0, std::min(old.y, current.y)),
                        std::min(EXPLORE_ROUTE_MAP_BOTTOM, std::max(old.y + old.height, current.y + current.height)));
                    const auto partial = pixels(canvas);
                    model.renderColumnBegin = 0; model.renderColumnEnd = 368;
                    renderExploreRouteScreen(canvas, model, cache);
                    assert(partial == pixels(canvas));
                    ++compared;
                }
                // World tint must be applied before solid dialog panels/text.
                model.complete = true;
                renderExploreRouteScreen(canvas, model, cache);
                const auto tinted = pixels(canvas);
                model.daylight = {};
                renderExploreRouteScreen(canvas, model, cache);
                const auto neutral = pixels(canvas);
                for (int y = 148; y < 256; ++y)
                    for (int x = 54; x < 314; ++x)
                        assert(tinted[y * 368 + x] == neutral[y * 368 + x]);
                model.complete = false;
                model.wildlife[0].frame = 0;
            }
            assert(fingerprint == ExploreMapGenerator::fingerprint(map));
        }
        std::printf("daylight render: %d dirty/full motion frames, six maps/four times, neutral caches, bands, HUD and dialogs verified\n", compared);
    }
    static void checkDaylightApp(DesktopPlatform& desktop, Canvas565& canvas) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.selectedExploreArea = 0;
        app.gameState.gameMinutesTotal = 1140;
        app.gameClock.set(desktop.millis(), 1140);
        assert(app.startExploreRoute(desktop.millis()));
        app.expeditionFade.reset();
        assert(app.exploreRouteModel().daylight == ExploreDaylight::at(1140, 0));
        app.render(canvas); app.markRendered();
        app.updateClockAndCare(desktop.millis());
        app.markRendered();
        const auto setTime = [&](uint32_t minute) {
            app.gameClock.set(desktop.millis(), minute);
            app.updateClockAndCare(desktop.millis());
        };
        setTime(1141);
        assert(app.exploreRouteModel().daylight == ExploreDaylight::at(1141, 0));
        assert(app.needsRender() && app.renderRowEnd() == EXPLORE_ROUTE_MAP_BOTTOM);
        app.render(canvas); const auto partial = pixels(canvas);
        app.requestFullRender(); app.render(canvas);
        assert(partial == pixels(canvas)); app.markRendered();
        app.exploreSession_.state_.exploreRoutePaused = true;
        setTime(1150);
        assert(app.exploreRouteModel().daylight == ExploreDaylight::at(1141, 0));
        assert(app.renderRowEnd() <= HOME_HEADER_HEIGHT);
        app.markRendered();
        app.exploreSession_.state_.exploreRoutePaused = false;
        setTime(1150);
        assert(app.exploreRouteModel().daylight == ExploreDaylight::at(1150, 0));
        assert(app.renderRowEnd() == EXPLORE_ROUTE_MAP_BOTTOM);
        app.markRendered();
        setTime(1320); app.markRendered();
        setTime(1321);
        assert(app.renderRowEnd() <= HOME_HEADER_HEIGHT); // Stable night adds no map damage.
        app.markRendered();
        app.updateClockAndCare(desktop.millis()); assert(!app.needsRender());
        app.selectedExploreArea = ExploreMapGenerator::FROST_CRYSTAL_CAVE_AREA;
        setTime(1322); assert(app.exploreRouteModel().daylight == ExploreDaylight::Tone{});
        app.markRendered();
        setTime(300); assert(app.exploreRouteModel().daylight == ExploreDaylight::Tone{});
        std::puts("daylight app: entry time, clock-driven map-only damage, paused/resumed tone, stable idle and cave exception verified");
    }
    static void checkGrassApp(DesktopPlatform& desktop, Canvas565& canvas) {
        AmoledApp app;
        app.begin(desktop.millis());
        assert(app.startExploreRoute(desktop.millis()));
        app.expeditionFade.reset();
        app.exploreSession_.state_.exploreRouteBossPending = app.exploreSession_.state_.exploreRouteBossScheduled = false;
        app.exploreSession_.state_.exploreRouteMap = {};
        auto& map = app.exploreSession_.state_.exploreRouteMap;
        map.seed = 918273;
        std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 385);
        constexpr int cell = 4 * ExploreMapGenerator::WIDTH + 3;
        map.layers[0][cell] = 399;
        map.layers[1][cell - ExploreMapGenerator::WIDTH] = 391;
        app.explorePresenter_.state_.exploreEcology.reset(map, 0, {}, 0, desktop.millis());
        app.gameState.teamCount = 1;
        app.gameState.team[0].speciesId = 161;
        app.gameState.activeSlot = 0;
        app.explorePresenter_.state_.exploreRouteWorldX = 81;
        app.explorePresenter_.state_.exploreRouteWorldY = 107;
        app.explorePresenter_.state_.exploreRouteCameraX = 0;
        app.explorePresenter_.state_.exploreRouteCameraY = 13;
        app.updateExploreEcology(desktop.millis());
        app.requestFullRender(); app.render(canvas); app.markRendered();
        const auto state = app.gameState;
        const int callsBefore = randomCalls;
        app.explorePresenter_.state_.exploreRouteWorldX += 6;
        desktop.advanceMs(80);
        app.updateExploreEcology(desktop.millis());
        assert(app.explorePresenter_.state_.exploreGrass.frame(cell).phase == 1 && app.needsRender());
        // Draw the actor's move once; following ticks exercise grass-only damage.
        app.requestFullRender(); app.render(canvas); app.markRendered();
        for (int tick = 0; tick < 10; ++tick) {
            desktop.advanceMs(90);
            app.updateExploreEcology(desktop.millis());
            if (app.needsRender()) {
                assert(app.renderRowEnd() <= EXPLORE_ROUTE_MAP_BOTTOM);
                app.render(canvas);
                const auto partial = pixels(canvas);
                app.requestFullRender(); app.render(canvas);
                assert(partial == pixels(canvas));
                app.markRendered();
            }
        }
        assert(app.explorePresenter_.state_.exploreGrass.frame(cell).phase == 0 && !app.needsRender());
        app.explorePresenter_.state_.exploreRouteWorldX += 6;
        desktop.advanceMs(80); app.updateExploreEcology(desktop.millis());
        const auto frame = app.explorePresenter_.state_.exploreGrass.frame(cell);
        assert(frame.phase == 1);
        app.requestFullRender(); app.render(canvas); app.markRendered();
        app.exploreSession_.state_.exploreRoutePaused = true;
        desktop.advanceMs(10000); app.updateExploreEcology(desktop.millis());
        assert(app.explorePresenter_.state_.exploreGrass.frame(cell) == frame && !app.needsRender());
        app.exploreSession_.state_.exploreRoutePaused = false;
        desktop.advanceMs(90); app.updateExploreEcology(desktop.millis());
        assert(app.explorePresenter_.state_.exploreGrass.frame(cell).phase == 2);
        assert(std::memcmp(&state, &app.gameState, sizeof(state)) == 0);
        assert(randomCalls == callsBefore);
        // Species using flying/floating movement cannot create grass feedback.
        for (uint16_t species : {uint16_t(41), uint16_t(278)}) {
            app.gameState.team[0].speciesId = species;
            app.explorePresenter_.state_.exploreGrass.reset(desktop.millis());
            app.updateExploreEcology(desktop.millis());
            app.explorePresenter_.state_.exploreRouteWorldX = 81;
            desktop.advanceMs(100); app.updateExploreEcology(desktop.millis());
            app.explorePresenter_.state_.exploreRouteWorldX += 8;
            desktop.advanceMs(100); app.updateExploreEcology(desktop.millis());
            assert(app.explorePresenter_.state_.exploreGrass.frame(cell).phase == 0);
        }
        std::puts("grass app: grass-only damage, settle, pause, flight and gameplay isolation verified");
    }
    static void checkGrassFrames(Canvas565& canvas) {
        int compared = 0;
        for (bool high : {false, true}) {
          for (int camera : {0, 13}) {
           for (uint16_t minute : {uint16_t(720), uint16_t(1140), uint16_t(1320)}) {
            ExploreMapGenerator::Map map{};
            map.seed = 99123 + high + camera;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 385);
            for (int x = 1; x <= 6; ++x) {
                map.layers[0][4 * ExploreMapGenerator::WIDTH + x] = high ? 399 : 390;
                if (high) map.layers[1][3 * ExploreMapGenerator::WIDTH + x] = 391;
            }
            ExploreGrass::Simulation grass;
            ExploreRouteViewModel model{};
            model.map = &map;
            model.daylight = ExploreDaylight::at(minute, 0);
            model.grass = &grass;
            model.worldX = model.worldY = -1000;
            model.cameraX = camera;
            model.cameraY = 20;
            model.wildlifeCount = 2;
            model.wildlife[0] = {286, 40, 116, 2, 0, false, false};
            model.wildlife[1] = {285, 18, 116, 2, 0, false, false};
            const uint16_t species[] = {286, 285};
            PokemonSprites::preloadDynamicSpecies(species, 2);
            PixelCache565 cache;
            ExploreGrass::Contact contacts[2] = {{40, 116, true}, {18, 116, true}};
            grass.update(map, 0, true, contacts, 2);
            renderExploreRouteScreen(canvas, model, cache);
            const auto original = pixels(canvas);
            bool changed = false;
            for (int tick = 1; tick <= 54; ++tick) {
                ExploreGrass::Frame before[ExploreMapGenerator::CELL_COUNT];
                for (int c = 0; c < ExploreMapGenerator::CELL_COUNT; ++c) before[c] = grass.frame(c);
                auto previous = exploreRouteWildlifeBounds(model, 0);
                const auto young = exploreRouteWildlifeBounds(model, 1);
                int left = std::min(previous.x, young.x), top = std::min(previous.y, young.y);
                int right = std::max(previous.x + previous.width, young.x + young.width);
                int bottom = std::max(previous.y + previous.height, young.y + young.height);
                if (tick <= 34) {
                    for (int i = 0; i < 2; ++i) {
                        contacts[i].x += 4;
                        model.wildlife[i].x = contacts[i].x;
                        model.wildlife[i].frame = (tick / 2) % 3;
                    }
                }
                grass.update(map, tick * 80, true, contacts, 2);
                for (int c = 0; c < ExploreMapGenerator::CELL_COUNT; ++c) {
                    if (before[c] == grass.frame(c)) continue;
                    changed = true;
                    const int x = (c % ExploreMapGenerator::WIDTH * 26 - model.cameraX) * 2;
                    const int y = (c / ExploreMapGenerator::WIDTH * 26 - model.cameraY) * 2;
                    left = std::min(left, x); right = std::max(right, x + 52);
                    top = std::min(top, y - (high ? 52 : 0)); bottom = std::max(bottom, y + 52);
                }
                for (int i = 0; i < 2; ++i) {
                    const auto bounds = exploreRouteWildlifeBounds(model, i);
                    left = std::min(left, bounds.x); right = std::max(right, bounds.x + bounds.width);
                    top = std::min(top, bounds.y); bottom = std::max(bottom, bounds.y + bounds.height);
                }
                model.renderColumnBegin = std::max(0, left);
                model.renderColumnEnd = std::min(368, right);
                renderExploreRouteScreen(canvas, model, cache, std::max(0, top),
                    std::min(EXPLORE_ROUTE_MAP_BOTTOM, bottom));
                const auto partial = pixels(canvas);
                model.renderColumnBegin = 0; model.renderColumnEnd = 368;
                renderExploreRouteScreen(canvas, model, cache);
                assert(partial == pixels(canvas));
                ++compared;
                if (camera == 0 && minute == 1320 && tick <= 44) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "grass-%s-%02d", high ? "high" : "short", tick);
                    writePreview(canvas, name);
                }
            }
            assert(changed && original != pixels(canvas));
            for (int c = 0; c < ExploreMapGenerator::CELL_COUNT; ++c) assert(grass.frame(c).phase == 0);
            // A settled simulation must match the original static artwork.
            const auto settled = pixels(canvas);
            model.grass = nullptr;
            renderExploreRouteScreen(canvas, model, cache);
            assert(settled == pixels(canvas));
           }
          }
        }
        std::printf("grass: %d ordinary/high grass pair frames match partial/full rendering and settle to static artwork\n", compared);
    }
    static void checkCoverFrames(Canvas565& canvas) {
        int compared = 0;
        for (uint8_t area = 0; area < 6; ++area) {
          for (uint8_t object = 0; object < 4; ++object) {
            ExploreMapGenerator::Map map{};
            map.seed = 18000 + area * 10 + object;
            map.areaIndex = area;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 385);
            const int cell = 3 * ExploreMapGenerator::WIDTH + 3;
            int groundRow = 4;
            if (object == 0) {
                groundRow = 6;
                for (int row = 0; row < 3; ++row)
                    for (int column = 0; column < 2; ++column)
                        map.layers[1][cell + row * ExploreMapGenerator::WIDTH + column] = 804 + row * 8 + column;
            } else if (object == 1) map.layers[2][cell] = 1231;
            else if (object == 2) map.layers[1][cell] = 859;
            else {
                for (int row = 2; row <= 5; ++row)
                    for (int x = 1; x < 7; ++x)
                        map.layers[0][row * ExploreMapGenerator::WIDTH + x] = 390;
            }
            ExploreRouteViewModel model{};
            model.map = &map;
            model.area = area;
            model.cameraY = 40;
            model.worldX = model.worldY = -1000;
            model.wildlifeCount = 1;
            PixelCache565 cache;
            for (uint16_t species : {uint16_t(161), uint16_t(41)}) {
                PokemonSprites::preloadDynamicSpecies(&species, 1);
                auto& pose = model.wildlife[0];
                pose = {};
                pose.speciesId = species;
                pose.airborne = species == 41;
                pose.lift = pose.airborne ? 8 : 0;
                pose.x = 50;
                pose.y = groundRow * 26 - 20;
                renderExploreRouteScreen(canvas, model, cache);
                for (int tick = 0; tick < 30; ++tick) {
                    const auto oldBounds = exploreRouteWildlifeBounds(model, 0);
                    pose.x = 50 + tick * 3;
                    pose.y = groundRow * 26 - 20 + tick * 1.2f;
                    if (object == 3) pose.y = groundRow * 26 - 20 + tick * 4.3f;
                    pose.frame = tick % 3;
                    pose.direction = 3;
                    const auto nextBounds = exploreRouteWildlifeBounds(model, 0);
                    model.renderColumnBegin = std::max(0, std::min(oldBounds.x, nextBounds.x));
                    model.renderColumnEnd = std::min(int(AmoledUi::WIDTH),
                        std::max(oldBounds.x + oldBounds.width, nextBounds.x + nextBounds.width));
                    const int top = std::max(0, std::min(oldBounds.y, nextBounds.y));
                    const int bottom = std::min(EXPLORE_ROUTE_MAP_BOTTOM,
                        std::max(oldBounds.y + oldBounds.height, nextBounds.y + nextBounds.height));
                    renderExploreRouteScreen(canvas, model, cache, top, bottom);
                    const auto partial = pixels(canvas);
                    model.renderColumnBegin = 0;
                    model.renderColumnEnd = AmoledUi::WIDTH;
                    renderExploreRouteScreen(canvas, model, cache);
                    assert(pixels(canvas) == partial);
                    ++compared;
                    if (area == 0 && species == 161 && (tick == 10 || tick == 20)) {
                        char name[64];
                        std::snprintf(name, sizeof(name), "cover-%u-%s", object, tick == 10 ? "behind" : "front");
                        writePreview(canvas, name);
                    }
                }
            }
          }
        }
        std::printf("cover: %d ground/air partial frames across four cover types and six areas verified\n", compared);
    }
    static void checkRenderedEpisodes(Canvas565& canvas) {
        using ExploreEcology::Activity;
        bool seen[ExploreEcology::ACTIVITY_COUNT]{};
        int partialFrames = 0;
        for (uint16_t species : {uint16_t(285), uint16_t(172), uint16_t(161), uint16_t(10), uint16_t(41), uint16_t(380), uint16_t(129)}) {
            ExploreMapGenerator::Map map{};
            map.seed = 7;
            map.areaIndex = species == 129 ? 1 : 0;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, species == 129 ? 1097 : 390);
            map.pathCount = 1;
            map.paths[0].pointCount = 14;
            for (uint8_t x = 0; x < ExploreMapGenerator::WIDTH; ++x) {
                map.layers[0][6 * ExploreMapGenerator::WIDTH + x] = species == 129 ? 1627 : 538;
                map.layers[0][7 * ExploreMapGenerator::WIDTH + x] = species == 129 ? 1643 : 554;
                if (x > 0 && x < 15) map.paths[0].points[x - 1] = {x, 6};
            }
            ExplorePool::Pool pool{};
            pool.count = 1;
            pool.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
            ExploreEcology::Simulation sim;
            sim.reset(map, 0, pool, 0, 0);
            for (uint32_t seed = 8; sim.hasGroup() && seed < 72; ++seed) {
                map.seed = seed;
                sim.reset(map, 0, pool, 0, 0);
            }
            assert(!sim.hasGroup() && sim.count() == 1);
            PokemonSprites::preloadDynamicSpecies(&species, 1);
            const auto origin = sim.pose(0);
            ExploreRouteViewModel model{};
            model.map = &map;
            model.area = map.areaIndex;
            model.wildlifeCount = 1;
            model.wildlife[0] = origin;
            model.worldX = origin.x;
            model.worldY = 7 * 26;
            model.cameraX = std::clamp(int(origin.x) - 92, 0, 416 - 184);
            model.cameraY = std::clamp(int(origin.y) - 86, 0, 312 - 172);
            PixelCache565 cache;
            renderExploreRouteScreen(canvas, model, cache);
            bool capture = false, captured = false, shyRetreated = false, captureComplete = false;
            std::vector<std::vector<uint16_t>> captureFrames;
            const Activity captureTarget = species == 285 ? Activity::WAKE :
                species == 172 ? Activity::APPROACH : species == 41 ? Activity::FLY :
                species == 129 ? Activity::SPLASH : Activity::EMERGE;
            const auto capturePose = [&]() {
                if (captured || !std::getenv("AMOLED_ECOLOGY_OUTPUT")) return;
                const Activity activity = model.wildlife[0].activity;
                if (!capture && ExploreEcology::isEpisode(activity)) capture = true;
                if (!capture) return;
                captureFrames.push_back(pixels(canvas));
                captureComplete |= activity == captureTarget;
                const bool finished = activity == Activity::REST || activity == Activity::CRUISE;
                if (!finished && captureFrames.size() < 100) return;
                if (finished && captureComplete) {
                    for (size_t frame = 0; frame < captureFrames.size(); ++frame) {
                        char name[64];
                        std::snprintf(name, sizeof(name), "clip-%u-%03u", species, unsigned(frame));
                        writePreviewPixels(captureFrames[frame], name);
                    }
                    captured = true;
                }
                capture = false;
                captureComplete = false;
                captureFrames.clear();
            };
            for (uint32_t ms = 200; ms <= 90000; ms += 200) {
                float px = -100, py = -100;
                if ((species == 161 || species == 10) && ms < 30000 && !shyRetreated) {
                    px = sim.pose(0).x + 18; py = sim.pose(0).y;
                }
                if (species == 172) { px = origin.x + 45; py = origin.y; }
                if (species == 285 && sim.pose(0).activity == Activity::DOZE && ms % 2000 == 0) {
                    px = origin.x; py = origin.y - 15;
                }
                const auto before = model.wildlife[0];
                const auto oldBounds = exploreRouteWildlifeBounds(model, 0);
                sim.update(ms, true, px, py, {0, 0, 416, 312});
                model.wildlife[0] = sim.pose(0);
                const auto& pose = model.wildlife[0];
                if (pose.activity == Activity::HIDE) shyRetreated = true;
                const auto nextBounds = exploreRouteWildlifeBounds(model, 0);
                if (ExploreEcology::samePose(before, pose)) {
                    capturePose();
                    continue;
                }
                const int left = std::max(0, std::min(oldBounds.x, nextBounds.x));
                const int right = std::min(int(AmoledUi::WIDTH),
                    std::max(oldBounds.x + oldBounds.width, nextBounds.x + nextBounds.width));
                const int top = std::max(0, std::min(oldBounds.y, nextBounds.y));
                const int bottom = std::min(EXPLORE_ROUTE_MAP_BOTTOM,
                    std::max(oldBounds.y + oldBounds.height, nextBounds.y + nextBounds.height));
                model.renderColumnBegin = left;
                model.renderColumnEnd = right;
                renderExploreRouteScreen(canvas, model, cache, top, bottom);
                const auto partial = pixels(canvas);
                model.renderColumnBegin = 0;
                model.renderColumnEnd = AmoledUi::WIDTH;
                renderExploreRouteScreen(canvas, model, cache);
                if (pixels(canvas) != partial) {
                    std::fprintf(stderr, "episode render species=%u phase=%u previous=%u at %u\n",
                        species, unsigned(pose.activity), unsigned(before.activity), ms);
                    assert(false);
                }
                ++partialFrames;
                const unsigned activity = unsigned(pose.activity);
                if (!seen[activity]) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "episode-%u-species-%u", activity, species);
                    writePreview(canvas, name);
                    seen[activity] = true;
                }
                capturePose();
            }
        }
        for (Activity activity : {Activity::DOZE, Activity::WAKE, Activity::APPROACH,
            Activity::RETURN, Activity::FLEE, Activity::HIDE, Activity::PEEK,
            Activity::TAKEOFF, Activity::FLY, Activity::LAND, Activity::GATHER,
            Activity::JUMP, Activity::SPLASH, Activity::BURROW, Activity::EMERGE}) {
            if (!seen[unsigned(activity)]) std::fprintf(stderr, "episode render missing phase %u\n", unsigned(activity));
            assert(seen[unsigned(activity)]);
        }
        std::printf("AMOLED episodes: %d partial frames across sleep, cover, approach, flight, water and their transitions verified\n", partialFrames);
    }
    static void checkObservationPacing(Canvas565& canvas) {
        uint32_t worstStillMs = 0, worstFirstMs = 0;
        for (uint16_t species : {uint16_t(16), uint16_t(74), uint16_t(161),
                uint16_t(172), uint16_t(285), uint16_t(129)}) {
          for (bool isolated : {false, true}) {
            ExploreMapGenerator::Map map{};
            map.seed = 7;
            map.areaIndex = species == 129 ? 1 : 0;
            const uint16_t terrain = species == 129 ? 1097 : 385;
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, isolated ? 64 : terrain);
            map.layers[0][4 * ExploreMapGenerator::WIDTH + 4] = terrain;
            map.pathCount = 1;
            map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            ExplorePool::Pool pool{};
            pool.count = 1;
            pool.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
            ExploreEcology::Simulation sim;
            sim.reset(map, 0, pool, 0, 0);
            for (uint32_t seed = 8; sim.hasGroup() && seed < 72; ++seed) {
                map.seed = seed;
                sim.reset(map, 0, pool, 0, 0);
            }
            assert(!sim.hasGroup() && sim.count() == 1);
            const auto origin = sim.pose(0);
            PokemonSprites::preloadDynamicSpecies(&species, 1);
            ExploreRouteViewModel model{};
            model.map = &map;
            model.area = map.areaIndex;
            model.worldX = model.worldY = -1000; // Only wildlife can change pixels.
            model.cameraX = std::clamp(int(origin.x) - 92, 0, 416 - 184);
            model.cameraY = std::clamp(int(origin.y) - 86, 0, 312 - 172);
            model.wildlifeCount = 1;
            model.wildlife[0] = origin;
            PixelCache565 cache;
            renderExploreRouteScreen(canvas, model, cache);
            auto previous = pixels(canvas);
            uint32_t stillMs = 0;
            bool changed = false;
            for (uint32_t ms = 200; ms <= 90000; ms += 200) {
                sim.update(ms, true, origin.x + 18, origin.y,
                    {float(model.cameraX), float(model.cameraY), 184, 172});
                model.wildlife[0] = sim.pose(0);
                renderExploreRouteScreen(canvas, model, cache);
                auto next = pixels(canvas);
                if (previous != next) {
                    if (!changed) worstFirstMs = std::max(worstFirstMs, ms);
                    changed = true;
                    stillMs = 0;
                } else stillMs += 200;
                if (stillMs > 5000) {
                    std::fprintf(stderr, "observation stalled species=%u isolated=%u phase=%u at=%u unchanged=%u\n",
                        species, isolated, unsigned(sim.pose(0).activity), ms, stillMs);
                    assert(false);
                }
                worstStillMs = std::max(worstStillMs, stillMs);
                previous.swap(next);
                if (isolated && species == 16 && ms <= 12000) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "observation-%03u", unsigned(ms / 200 - 1));
                    writePreview(canvas, name);
                }
            }
            assert(changed);
          }
        }
        assert(worstFirstMs <= 2200);
        std::printf("observation: 12 open/blocked habitats, 90s each; actual pixels first change <=%ums, longest still <=%ums\n",
            worstFirstMs, worstStillMs);
    }
    static void checkWideFlightFrames(Canvas565& canvas) {
        using ExploreEcology::Activity;
        int partialFrames = 0, disappearances = 0, arrivals = 0;
        for (uint16_t species : {uint16_t(278), uint16_t(279)}) {
          for (int side = 0; side < 2; ++side) {
            ExploreMapGenerator::Map map{};
            map.seed = 1;
            map.areaIndex = 1;
            map.pathCount = 1;
            map.paths[0].pointCount = 14;
            for (uint8_t x = 1; x <= 14; ++x) map.paths[0].points[x - 1] = {x, 6};
            std::fill_n(map.layers[0], ExploreMapGenerator::CELL_COUNT, 385);
            for (int y = 0; y < 12; ++y) {
                for (int x = 6; x <= 8; ++x) map.layers[0][y * 16 + x] = 1097;
                map.layers[1][y * 16 + 10] = y == 5 ? 0 : 1185;
            }
            for (int x = 0; x < 16; ++x) map.layers[0][6 * 16 + x] = 538;
            for (int y = 0; y < 3; ++y)
                for (int x = 0; x < 2; ++x) map.layers[1][(2 + y) * 16 + 2 + x] = 804 + y * 8 + x;
            map.layers[2][4 * 16 + 5] = 1231;
            ExplorePool::Pool pool{};
            pool.count = 1;
            pool.entries[0] = {species, 1, ExplorePool::Rarity::COMMON};
            ExploreEcology::Simulation sim;
            sim.reset(map, 0, pool, 0, 0);
            for (uint32_t seed = 8; sim.hasGroup() && seed < 72; ++seed) {
                map.seed = seed;
                sim.reset(map, 0, pool, 0, 0);
            }
            assert(!sim.hasGroup() && sim.count() == 1);
            PokemonSprites::preloadDynamicSpecies(&species, 1);
            ExploreRouteViewModel model{};
            model.map = &map;
            model.area = 1;
            model.worldX = model.worldY = -1000;
            model.cameraX = side ? 232 : 0;
            model.cameraY = side ? 140 : 0;
            model.wildlifeCount = 1;
            model.wildlife[0] = sim.pose(0);
            PixelCache565 cache;
            renderExploreRouteScreen(canvas, model, cache);
            for (uint32_t ms = 200; ms <= 180000; ms += 200) {
                const auto before = model.wildlife[0];
                const auto oldBounds = exploreRouteWildlifeBounds(model, 0);
                sim.update(ms, true, -100, -100, {float(model.cameraX), float(model.cameraY), 184, 172});
                model.wildlife[0] = sim.pose(0);
                const auto pose = model.wildlife[0];
                const auto nextBounds = exploreRouteWildlifeBounds(model, 0);
                int left = 368, right = 0, top = EXPLORE_ROUTE_MAP_BOTTOM, bottom = 0;
                for (const auto bounds : {oldBounds, nextBounds}) {
                    if (bounds.width == 0 || bounds.x >= 368 || bounds.x + bounds.width <= 0 ||
                        bounds.y >= EXPLORE_ROUTE_MAP_BOTTOM || bounds.y + bounds.height <= 0) continue;
                    left = std::min(left, std::max(0, bounds.x));
                    right = std::max(right, std::min(368, bounds.x + bounds.width));
                    top = std::min(top, std::max(0, bounds.y));
                    bottom = std::max(bottom, std::min(EXPLORE_ROUTE_MAP_BOTTOM, bounds.y + bounds.height));
                }
                if (right > left && bottom > top) {
                    model.renderColumnBegin = left; model.renderColumnEnd = right;
                    renderExploreRouteScreen(canvas, model, cache, top, bottom);
                    ++partialFrames;
                }
                const auto partial = pixels(canvas);
                model.renderColumnBegin = 0; model.renderColumnEnd = 368;
                renderExploreRouteScreen(canvas, model, cache);
                assert(pixels(canvas) == partial);
                if (pose.absent != before.absent) {
                    auto worldModel = model;
                    worldModel.cameraX = worldModel.cameraY = 0;
                    worldModel.wildlife[0].absent = false;
                    const auto envelope = exploreRouteWildlifeBounds(worldModel, 0);
                    assert(envelope.x + envelope.width <= 0 || envelope.x >= 832 ||
                        envelope.y + envelope.height <= 0 || envelope.y >= 624);
                    if (pose.absent) ++disappearances; else ++arrivals;
                }
                if (pose.absent) {
                    assert(nextBounds.width == 0);
                    const auto absent = pixels(canvas);
                    model.wildlifeCount = 0;
                    renderExploreRouteScreen(canvas, model, cache);
                    assert(pixels(canvas) == absent);
                    model.wildlifeCount = 1;
                }
                if (species == 278 && side == 0 && ms <= 20000) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "gull-%03u", unsigned(ms / 200 - 1));
                    writePreview(canvas, name);
                }
            }
          }
        }
        assert(disappearances >= 4 && arrivals >= 4 && partialFrames > 400);
        std::printf("wide flight render: %d partial frames, %d full-body departures and %d edge reentries, two species/cameras, absent pixels and cover verified\n",
            partialFrames, disappearances, arrivals);
    }
    static void checkGroupApp(DesktopPlatform& desktop, Canvas565& canvas) {
        int rendered = 0;
        for (const auto& family : ExploreEcology::GROUP_FAMILIES) {
          const auto& species = family.species;
          const uint16_t adult = species[0];
          for (uint8_t composition = family.count == 3 ? 1 : 3;
               composition < (1U << family.count); ++composition) {
          uint16_t selected[ExploreEcology::CAPACITY]{};
          uint8_t selectedCount = 0;
          for (uint8_t i = 0; i < family.count; ++i)
              if (composition & (1U << i)) selected[selectedCount++] = species[i];
          for (uint8_t poolVariant = 0; poolVariant <= family.count; ++poolVariant) {
            const bool watery = false;
            AmoledApp app;
            app.begin(desktop.millis());
            app.selectedExploreArea = adult == 286 ? 2 : 1;
            GameRandom::setRangeProvider(countingRandom);
            assert(app.startExploreRoute(desktop.millis()));
            app.exploreSession_.state_.exploreRouteBossPending = app.exploreSession_.state_.exploreRouteBossScheduled = false;
            ExplorePool::Pool pool{};
            pool.count = poolVariant == family.count ? family.count : 1;
            for (uint8_t i = 0; i < pool.count; ++i)
                pool.entries[i] = {species[poolVariant == family.count ? i : poolVariant], 1, ExplorePool::Rarity::COMMON};
            const auto poolBefore = pool;
            bool found = false;
            for (uint32_t seed = 1; seed <= 512; ++seed) {
                if (!ExploreMapGenerator::generate(seed, ExploreMapGenerator::Edge(seed % 4),
                    app.selectedExploreArea, app.exploreSession_.state_.exploreRouteMap)) continue;
                app.exploreSession_.state_.exploreRoutePath = seed % app.exploreSession_.state_.exploreRouteMap.pathCount;
                app.explorePresenter_.state_.exploreEcology.reset(app.exploreSession_.state_.exploreRouteMap, app.exploreSession_.state_.exploreRoutePath, pool, 0, desktop.millis());
                bool uncovered = true;
                for (uint8_t i = 0; i < app.explorePresenter_.state_.exploreEcology.count(); ++i)
                    uncovered &= app.explorePresenter_.state_.exploreEcology.pose(i).cover == ExploreEcology::Cover::NONE;
                bool matches = app.explorePresenter_.state_.exploreEcology.count() == selectedCount;
                for (uint8_t i = 0; matches && i < selectedCount; ++i)
                    matches &= app.explorePresenter_.state_.exploreEcology.pose(i).speciesId == selected[i];
                if (app.explorePresenter_.state_.exploreEcology.hasFamily() && matches && uncovered &&
                    (app.explorePresenter_.state_.exploreEcology.pose(0).surface == ExploreEcology::Surface::WATER) == watery) {
                    found = true; break;
                }
            }
            assert(found);
            assert(app.explorePresenter_.state_.exploreEcology.count() == selectedCount);
            assert(app.explorePresenter_.state_.exploreEcology.hasGroup() == (selectedCount > 1));
            for (uint8_t i = 0; i < selectedCount; ++i)
                assert(app.explorePresenter_.state_.exploreEcology.pose(i).speciesId == selected[i]);
            assert(std::memcmp(&poolBefore, &pool, sizeof(pool)) == 0);
            assert(PokemonSprites::preloadDynamicSpecies(species, family.count));
            for (uint8_t i = 0; i < family.count; ++i) {
                const uint16_t member = species[i];
                for (uint8_t direction = 0; direction < 4; ++direction) {
                    PokemonSprites::WalkingAnimation animation{};
                    assert(PokemonSprites::walkingAnimation(member,
                        static_cast<PokemonSprites::WalkDirection>(direction), animation));
                    assert(animation.frameCount >= 2);
                    for (uint8_t frame = 0; frame < animation.frameCount; ++frame)
                        assert(PokemonSprites::findCachedSpeciesSprite(member,
                            static_cast<PokemonSprites::SpriteKind>(unsigned(animation.base) + frame)));
                }
            }
            const auto state = app.gameState;
            const auto history = app.encounterHistory;
            const int callsBefore = randomCalls;
            const auto origin = app.explorePresenter_.state_.exploreEcology.pose(0);
            app.explorePresenter_.state_.exploreRouteWorldX = origin.x;
            app.explorePresenter_.state_.exploreRouteWorldY = origin.y + 65;
            app.explorePresenter_.updateCamera();
            app.explorePresenter_.state_.exploreRouteCameraX = std::clamp(int(origin.x) - 92, 0, 416 - 184);
            app.explorePresenter_.state_.exploreRouteCameraY = std::clamp(int(origin.y) - 86, 0, 312 - 172);
            app.requestFullRender(); app.render(canvas); app.markRendered();
            for (unsigned tick = 0; tick < 350; ++tick) {
                if (tick == 75) {
                    const auto leader = app.explorePresenter_.state_.exploreEcology.pose(0);
                    app.explorePresenter_.state_.exploreRouteWorldX = leader.x + 20;
                    app.explorePresenter_.state_.exploreRouteWorldY = leader.y;
                    app.requestFullRender(); app.render(canvas); app.markRendered();
                } else if (tick == 100) {
                    app.explorePresenter_.state_.exploreRouteWorldX = -1000; app.explorePresenter_.state_.exploreRouteWorldY = -1000;
                    app.requestFullRender(); app.render(canvas); app.markRendered();
                }
                desktop.advanceMs(100);
                app.updateExploreEcology(desktop.millis());
                if (!app.needsRender()) continue;
                assert(app.renderRowEnd() <= EXPLORE_ROUTE_MAP_BOTTOM);
                app.render(canvas);
                const auto partial = pixels(canvas);
                app.requestFullRender(); app.render(canvas);
                assert(pixels(canvas) == partial);
                app.markRendered();
                ++rendered;
                if (tick % 2 == 0 && tick < 280) {
                    char name[64];
                    std::snprintf(name, sizeof(name), "group-%u-mask-%u-%s-%03u", adult, composition,
                        watery ? "water" : "ground", tick / 2);
                    writePreview(canvas, name);
                }
            }
            assert(std::memcmp(&state, &app.gameState, sizeof(state)) == 0);
            assert(std::memcmp(&history, &app.encounterHistory, sizeof(history)) == 0);
            assert(callsBefore == randomCalls);
            GameRandom::setRangeProvider(nullptr);
          }
          }
        }
        assert(rendered > 400);
        std::printf("groups app: %d real dirty frames across all seven Marill compositions, other pairs and single-member/all-member pools; follow, reunion, player, cover and gameplay isolation verified\n", rendered);
    }
    static void checkWideFlightApp(DesktopPlatform& desktop, Canvas565& canvas) {
        AmoledApp app;
        app.begin(desktop.millis());
        app.selectedExploreArea = 1;
        GameRandom::setRangeProvider(countingRandom);
        assert(app.startExploreRoute(desktop.millis()));
        app.exploreSession_.state_.exploreRouteBossPending = app.exploreSession_.state_.exploreRouteBossScheduled = false;
        ExplorePool::Pool pool{};
        pool.count = 1;
        pool.entries[0] = {278, 1, ExplorePool::Rarity::COMMON};
        app.explorePresenter_.state_.exploreEcology.reset(app.exploreSession_.state_.exploreRouteMap, app.exploreSession_.state_.exploreRoutePath, pool, 0, desktop.millis());
        assert(app.explorePresenter_.state_.exploreEcology.count() == 1);
        const uint16_t species = 278;
        PokemonSprites::preloadDynamicSpecies(&species, 1);
        const auto state = app.gameState;
        const auto history = app.encounterHistory;
        const int callsBefore = randomCalls;
        app.requestFullRender(); app.render(canvas); app.markRendered();
        int rendered = 0, absentTicks = 0;
        for (unsigned tick = 0; tick < 1800; ++tick) {
            desktop.advanceMs(100);
            app.updateExploreEcology(desktop.millis());
            if (app.explorePresenter_.state_.exploreEcology.pose(0).absent) {
                ++absentTicks;
                assert(!app.needsRender());
            }
            if (!app.needsRender()) continue;
            assert(app.renderRowEnd() <= EXPLORE_ROUTE_MAP_BOTTOM);
            app.render(canvas);
            const auto partial = pixels(canvas);
            app.requestFullRender(); app.render(canvas);
            assert(pixels(canvas) == partial);
            app.markRendered();
            ++rendered;
        }
        assert(rendered > 100 && absentTicks > 100);
        assert(std::memcmp(&state, &app.gameState, sizeof(state)) == 0);
        assert(std::memcmp(&history, &app.encounterHistory, sizeof(history)) == 0);
        assert(callsBefore == randomCalls);
        GameRandom::setRangeProvider(nullptr);
        std::printf("wide flight app: %d real dirty frames, %d absent ticks with no redraw; gameplay state/random isolation verified\n",
            rendered, absentTicks);
    }
};
}

void checkExploreEcology(DesktopPlatform& desktop, Canvas565& canvas) {
    for (uint8_t area = 0; area < Game::EXPLORE_AREA_COUNT; ++area)
        AmoledV1::AmoledEcologyTestAccess::checkArea(desktop, canvas, area);
    AmoledV1::AmoledEcologyTestAccess::checkCover(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkDaylightFrames(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkDaylightApp(desktop, canvas);
    AmoledV1::AmoledEcologyTestAccess::checkGrassApp(desktop, canvas);
    AmoledV1::AmoledEcologyTestAccess::checkGrassFrames(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkCoverFrames(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkRenderedEpisodes(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkObservationPacing(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkWideFlightFrames(canvas);
    AmoledV1::AmoledEcologyTestAccess::checkWideFlightApp(desktop, canvas);
    AmoledV1::AmoledEcologyTestAccess::checkGroupApp(desktop, canvas);
}

void checkExploreWater(DesktopPlatform& desktop, Canvas565& canvas) {
    AmoledV1::AmoledEcologyTestAccess::checkWildlifeGrassApp(desktop);
    AmoledV1::AmoledEcologyTestAccess::checkWaterBridge(desktop, canvas);
}
