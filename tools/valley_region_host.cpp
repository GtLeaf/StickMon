#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GameAssets.h"
#include "game/ExploreEcology.h"
#include "game/ExploreRouteGeometry.h"
#include "game/ExploreTileSemantics.h"
#include "game/ExploreValleyData.h"
#include "platform/desktop/DesktopPlatform.h"
#include "presentation/Canvas565.h"
#include "ui/ExploreMapRenderer.h"
#include "ui/models/ScreenModels.h"
#define private public
#include "app/ExploreSession.h"
#undef private

using namespace ExploreMapGenerator;

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--fingerprints") == 0) {
        for (uint32_t seed = 0; seed < 32; ++seed) {
            for (uint32_t offset : {0U, 256U, 0xffffffe0U}) {
                for (uint8_t node = 0; node < 14; ++node) {
                    const auto& room = ExploreValleyData::room(seed + offset, node);
                    for (uint8_t entry = 0; entry < room.count; ++entry) {
                        if ((node == 10 && entry >= 2) || (node == 13 && entry < 2)) continue;
                        for (bool final : {false, true}) {
                            Map map;
                            assert(generateValleyRegion(seed + offset, {node, entry}, final, map));
                            std::printf("%u %u %u %u %08x\n", seed + offset, node, entry, final, fingerprint(map));
                        }
                    }
                }
            }
        }
        return 0;
    }
    assert(argc == 2 || argc == 3);
    DesktopPlatform desktop(argv[1]);
    Platform::bind(desktop.serviceBundle());
    assert(desktop.begin() && Platform::resources().mount() && GameAssets::begin());
    std::array<uint16_t, 416 * 312> pixels{};
    Canvas565 canvas;
    canvas.attach({pixels.data(), 416, 312, false});
    unsigned maps = 0, bridges = 0, trees = 0;
    for (uint32_t seed = 0; seed < 32; ++seed) {
        for (uint32_t offset : {0U, 256U}) {
            for (uint8_t node = 0; node < 14; ++node) {
                const uint8_t entry = node == 13 ? 2 : 0;
                Map map;
                assert(generateValleyRegion(seed + offset, {node, entry}, false, map));
                ++maps;
                ExploreEcology::Simulation ecology;
                ecology.reset(map, 0, {}, 0, 0);
                for (uint8_t frame = 0; frame < 8; ++frame) {
                    canvas.fillSprite(0);
                    for (uint8_t layer = 0; layer < 3; ++layer) {
                        for (uint16_t cell = 0; cell < 192; ++cell) {
                            const auto tile = map.layers[layer][cell];
                            if (!tile) continue;
                            if (!GameAssets::drawExploreTileTo(canvas, tile,
                                cell % 16 * 26, cell / 16 * 26, frame, 1)) {
                                std::fprintf(stderr, "missing tile=%u node=%u layer=%u\n", tile, node, layer);
                                return 1;
                            }
                            if (const auto* data = ExploreValleyData::tile(tile)) {
                                assert(GameAssets::isExploreTileAnimated(tile) == (data->frames > 1));
                                if (data->depth && layer) {
                                    const float ground = (cell / 16 + data->depth) * 26;
                                    assert(ExploreTileSemantics::objectOccludes(tile, layer, cell / 16, ground - .01f, 26));
                                    assert(!ExploreTileSemantics::objectOccludes(tile, layer, cell / 16, ground, 26));
                                }
                                if (data->flags & 32) ++trees;
                                if (data->flags & 16) {
                                    ++bridges;
                                    assert(ExploreTileSemantics::bridgeDeck(tile));
                                    if (ecology.surfaceAt(cell % 16, cell / 16) == ExploreEcology::Surface::WATER) {
                                        assert(ecology.canOccupy(129, cell % 16, cell / 16));
                                        assert(!ecology.canOccupy(7, cell % 16, cell / 16));
                                    }
                                }
                            }
                        }
                    }
                    if (argc == 3 && seed == 6 && offset == 256 && frame == 0) {
                        const auto path = std::string(argv[2]) + "/region-" + std::to_string(node) + ".rgb565";
                        FILE* out = std::fopen(path.c_str(), "wb");
                        assert(out && std::fwrite(pixels.data(), 2, pixels.size(), out) == pixels.size());
                        std::fclose(out);
                    }
                }
            }
        }
    }
    assert(bridges && trees);
    // Test actual clipped foreground restoration on each new canopy/rock/deck.
    std::vector<uint16_t> screen(AmoledUi::WIDTH * AmoledUi::HEIGHT);
    canvas.attach({screen.data(), AmoledUi::WIDTH, AmoledUi::HEIGHT, false});
    for (uint16_t tile = 9000; ExploreValleyData::tile(tile); ++tile) {
        const auto& spec = *ExploreValleyData::tile(tile);
        const bool deck = spec.flags & 16;
        if (!spec.depth && !deck) continue;
        Map sample{};
        sample.areaIndex = 5;
        const uint8_t layer = deck ? 2 : 1;
        sample.layers[layer][3 * 16 + 3] = tile;
        AmoledV1::ExploreRouteViewModel model{};
        model.map = &sample;
        constexpr int x = 156, y = 156, size = 52;
        constexpr uint16_t empty = 0xf81f, actor = 0x07ff;
        canvas.clearClipRect();
        canvas.fillSprite(empty);
        assert(GameAssets::drawExploreTileTo(canvas, tile, x, y, 0, 2));
        const auto original = screen;
        unsigned restored = 0;
        for (bool behind : {true, false}) {
            canvas.clearClipRect();
            canvas.fillSprite(actor);
            const float ground = (3 + spec.depth) * 26;
            AmoledV1::drawExploreRouteMapVegetationForActor(canvas, model,
                behind ? ground - .01f : ground,
                x+4, y+4, x+size-4, y+size-4, 0,
                AmoledV1::EXPLORE_ROUTE_MAP_BOTTOM, false, false, deck && behind);
            for (int py = 0; py < AmoledUi::HEIGHT; ++py) {
                for (int px = 0; px < AmoledUi::WIDTH; ++px) {
                    const auto i = py * AmoledUi::WIDTH + px;
                    const bool cover = behind && px >= x+4 && px < x+size-4 &&
                        py >= y+4 && py < y+size-4 && original[i] != empty;
                    assert(screen[i] == (cover ? original[i] : actor));
                    restored += cover;
                }
            }
        }
        assert(restored);
    }
    canvas.clearClipRect();
    // Drive the real AMOLED transfer logic at the quota boundary. The three
    // internal bridge transitions must not consume additional map blocks.
    for (uint8_t quota : {7, 8, 9}) {
        Game::GameState game{};
        AmoledV1::ExploreSession session;
        auto& s = session.state_;
        s.exploreRouteMapBlockCount = quota;
        s.exploreRouteMapBlock = quota - 2;
        s.exploreRouteExpeditionSeed = 38;
        s.exploreRoutePendingValley = {0, 0};
        assert(session.generateMap(game, 5));
        const std::array<uint8_t, 8> sequence = {1, 2, 5, 10, 11, 13, 12, 255};
        // Full journey gets a fresh allowance; quota behavior is tested below.
        s.exploreRouteMapBlock = 0;
        s.exploreRouteMapBlockCount = 9;
        assert(session.generateMap(game, 5));
        for (const auto target : sequence) {
            bool found = false;
            for (uint8_t p = 0; p < s.exploreRouteMap.pathCount; ++p) {
                if (s.exploreRouteMap.paths[p].nextValleyNode != target) continue;
                s.exploreRoutePath = p;
                s.exploreRouteIndex = s.exploreRouteMap.paths[p].pointCount - 1;
                found = true;
                const auto block = s.exploreRouteMapBlock;
                const bool internal = internalValleyTransfer(s.exploreRouteMap, p);
                if (target == 255) {
                    assert(!session.finishAtEnd(game) && s.exploreRouteComplete);
                } else {
                    assert(session.finishAtEnd(game));
                    assert(s.exploreRouteMap.valleyNode == target);
                    assert(s.exploreRouteMapBlock == block + (internal ? 0 : 1));
                }
                break;
            }
            assert(found);
        }
        session = AmoledV1::ExploreSession{};
        auto& last = session.state_;
        last.exploreRouteMapBlockCount = quota;
        last.exploreRouteMapBlock = quota - 1;
        last.exploreRouteExpeditionSeed = 38;
        last.exploreRoutePendingValley = {10, 0};
        assert(session.generateMap(game, 5));
        for (uint8_t target : {11, 13, 12}) {
            assert(last.exploreRouteMap.pathCount == 1);
            assert(last.exploreRouteMap.paths[0].nextValleyNode == target);
            last.exploreRouteIndex = last.exploreRouteMap.paths[0].pointCount - 1;
            assert(session.finishAtEnd(game));
            assert(last.exploreRouteMapBlock == quota - 1 && !last.exploreRouteComplete);
        }
        last.exploreRouteIndex = last.exploreRouteMap.paths[0].pointCount - 1;
        assert(!session.finishAtEnd(game) && last.exploreRouteComplete);
    }
    std::printf("valley-region=ok maps=%u animated-frames=%u journey=ok quota=ok\n", maps, maps * 8);
}
