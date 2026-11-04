#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "assets/GameAssets.h"
#include "game/ExploreAncientValleyLayouts.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "app/ExplorePresenter.h"
#include "ui/ExploreMapRenderer.h"
#include "ui/models/ScreenModels.h"

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin() && Platform::resources().mount());
    assert(GameAssets::begin());
    std::array<uint16_t, 416 * 312> pixels{};
    Canvas565 canvas;
    canvas.attach({pixels.data(), 416, 312, false});
    for (uint8_t edge = 0; edge < 4; ++edge) {
        ExploreMapGenerator::Map map;
        assert(ExploreMapGenerator::generate(1, ExploreMapGenerator::Edge(edge), 5, map));
        for (uint8_t branch = 0; branch < map.pathCount; ++branch) {
            const auto& path = map.paths[branch];
            assert(ExploreRouteGeometry::interiorPortalExit(map, path));
            for (uint8_t index = 0; index < path.pointCount; ++index) {
                const auto p = path.points[index];
                const auto world = ExploreRouteGeometry::pathPoint(path, index);
                assert(world.x == (p.x + 0.5f) * 26 && world.y == (p.y + 0.5f) * 26);
            }
            AmoledV1::ExploreSession::State route{};
            route.exploreRouteMap = map;
            route.exploreRoutePath = branch;
            route.exploreRouteIndex = path.pointCount - 1;
            Game::GameState game{};
            AmoledV1::ExplorePresenter presenter;
            presenter.resetMap(route, game, 5, 0, 1000);
            const auto before = presenter.state();
            presenter.beginExit(route, true, 1000);
            const auto after = presenter.state();
            assert(after.exploreRouteTargetX == before.exploreRouteWorldX);
            assert(after.exploreRouteTargetY == before.exploreRouteWorldY);
            assert(after.exploreRouteFollowerTargetX == before.exploreRouteFollowerWorldX);
            assert(after.exploreRouteFollowerTargetY == before.exploreRouteFollowerWorldY);
            assert(!after.exploreRouteFollowerMoving);
        }
        for (uint8_t frame = 0; frame < 8; ++frame) {
            canvas.fillSprite(0);
            for (uint8_t layer = 0; layer < 3; ++layer) {
                for (uint16_t cell = 0; cell < 192; ++cell) {
                    const uint16_t tile = map.layers[layer][cell];
                    if (!tile) continue;
                    assert(GameAssets::drawExploreTileTo(canvas, tile,
                        cell % 16 * 26, cell / 16 * 26, frame, 1));
                    if (ExploreAncientValley::waterTile(tile))
                        assert(GameAssets::isExploreTileAnimated(tile));
                    const auto source = ExploreAncientValley::caveSource(tile);
                    if (layer && (source == 521 || source == 529 || source == 516)) {
                        const float ground = (cell / 16 + 1) * 26;
                        assert(ExploreTileSemantics::objectOccludes(tile, layer, cell / 16, ground - 1, 26));
                        assert(!ExploreTileSemantics::objectOccludes(tile, layer, cell / 16, ground, 26));
                    }
                }
            }
            if (argc == 3) {
                const auto name = std::string(argv[2]) + "/native-" + std::to_string(edge) + "-" + std::to_string(frame) + ".rgb565";
                FILE* file = std::fopen(name.c_str(), "wb");
                assert(file && std::fwrite(pixels.data(), sizeof(uint16_t), pixels.size(), file) == pixels.size());
                std::fclose(file);
            }
        }
    }
    // Exercise the real foreground renderer with clipped actor rectangles.
    // Both northern doorway pieces must share the lower doorway's depth.
    std::vector<uint16_t> screen(AmoledUi::WIDTH * AmoledUi::HEIGHT);
    canvas.attach({screen.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
    for (uint16_t tile = 4800; tile <= 4835; ++tile) {
        const int rows = ExploreAncientValley::depthRows(tile);
        if (!rows) continue;
        ExploreMapGenerator::Map map{};
        map.areaIndex = ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA;
        map.layers[1][3 * 16 + 3] = tile;
        AmoledV1::ExploreRouteViewModel model{};
        model.map = &map;
        constexpr int x = 156, y = 156, size = 52;
        constexpr uint16_t empty = 0xf81f, actor = 0x07ff;
        canvas.fillSprite(empty);
        assert(GameAssets::drawExploreTileTo(canvas, tile, x, y, 0, 2));
        const auto tilePixels = screen;
        unsigned restored = 0;
        for (float depth : {(3 + rows) * 26 - .01f, float((3 + rows) * 26)}) {
            canvas.clearClipRect();
            canvas.fillSprite(actor);
            AmoledV1::drawExploreRouteMapVegetationForActor(canvas, model, depth,
                x+4, y+4, x+size-4, y+size-4,
                0, AmoledV1::EXPLORE_ROUTE_MAP_BOTTOM);
            for (int py = 0; py < AmoledUi::HEIGHT; ++py) {
                for (int px = 0; px < AmoledUi::WIDTH; ++px) {
                    const auto i = py * AmoledUi::WIDTH + px;
                    const bool cover = depth < (3 + rows) * 26 &&
                        px >= x+4 && px < x+size-4 && py >= y+4 && py < y+size-4 &&
                        tilePixels[i] != empty;
                    assert(screen[i] == (cover ? tilePixels[i] : actor));
                    restored += cover;
                }
            }
        }
        assert(restored);
    }
    std::puts("ancient-valley=ok layouts=4 frames=32 exits=8");
}
