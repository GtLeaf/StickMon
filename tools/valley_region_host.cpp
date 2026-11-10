#include <array>
#include <cassert>
#include <cmath>
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

static void checkForestCenterlines() {
    using ExploreRouteGeometry::pathPoint;
    constexpr int tile = ExploreRouteGeometry::TILE_SIZE;
    unsigned left = 0, right = 0, up = 0, down = 0, bends = 0;
    for (uint32_t seed : {38U, 294U}) {
        for (uint8_t node = 0; node < 6; ++node) {
            const auto& room = ExploreValleyData::room(seed, node);
            const auto road = [&](int x, int y) {
                return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT &&
                    (room.roads[0][y] & (1U << x));
            };
            for (uint8_t entrance = 0; entrance < room.count; ++entrance) {
                Map map{};
                assert(generateValleyRegion(seed, {node, entrance}, false, map));
                for (uint8_t p = 0; p < map.pathCount; ++p) {
                    const auto& path = map.paths[p];
                    if (path.interiorExit) {
                        const auto point = pathPoint(path, path.pointCount - 1);
                        assert(point.x == ExploreRouteGeometry::tileCenter(path.exit.point.x));
                        assert(point.y == ExploreRouteGeometry::tileCenter(path.exit.point.y));
                    } else if (path.nextValleyNode < 6) {
                        Map next{};
                        assert(generateValleyRegion(seed, {path.nextValleyNode, path.nextValleyEntrance}, false, next));
                        const auto a = pathPoint(path, path.pointCount - 1);
                        const auto b = pathPoint(next.paths[0], 0);
                        assert(path.exit.edge == Edge::TOP || path.exit.edge == Edge::BOTTOM
                               ? a.x == b.x : a.y == b.y);
                    }
                    for (uint8_t i = 1; i < path.pointCount; ++i) {
                        const auto a = pathPoint(path, i - 1), b = pathPoint(path, i);
                        assert(a.x != b.x || a.y != b.y);
                        for (int step = 0; step <= 32; ++step) {
                            const float x = a.x + (b.x - a.x) * (step / 32.0f);
                            const float y = a.y + (b.y - a.y) * (step / 32.0f);
                            bool supported = false;
                            for (float dy : {-0.01f, 0.01f}) for (float dx : {-0.01f, 0.01f})
                                supported |= road(int(std::floor((x + dx) / tile)), int(std::floor((y + dy) / tile)));
                            assert(supported);
                        }
                        if (i + 1 == path.pointCount) continue;
                        const auto prev = path.points[i - 1], cell = path.points[i], next = path.points[i + 1];
                        const bool vertical = prev.x == cell.x && next.x == cell.x;
                        const bool horizontal = prev.y == cell.y && next.y == cell.y;
                        if (!vertical && !horizontal) { ++bends; continue; }
                        // Isolated double lanes have exactly one partner throughout
                        // the three-cell sample. Their center is the shared seam.
                        const int dx = vertical, dy = horizontal;
                        const auto flank = [&](Point c, int sign) { return road(c.x + dx * sign, c.y + dy * sign); };
                        for (int sign : {-1, 1}) {
                            if (!flank(prev, sign) || !flank(cell, sign) || !flank(next, sign) ||
                                flank(prev, -sign) || flank(cell, -sign) || flank(next, -sign) ||
                                flank(cell, 2 * sign)) continue;
                            if (vertical) {
                                assert(b.x == ExploreRouteGeometry::tileCenter(cell.x) + sign * tile / 2);
                                ++(sign < 0 ? left : right);
                            } else {
                                assert(b.y == ExploreRouteGeometry::tileCenter(cell.y) + sign * tile / 2);
                                ++(sign < 0 ? up : down);
                            }
                        }
                    }
                }
            }
        }
    }
    assert(left && right && up && down && bends);
    std::printf("forest centerlines: left=%u right=%u up=%u down=%u bends=%u\n", left, right, up, down, bends);
}

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
    checkForestCenterlines();
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
                if (node >= 6) {
                    for (uint8_t p = 0; p < map.pathCount; ++p) {
                        const auto& path = map.paths[p];
                        for (uint8_t i = 0; i < path.pointCount; ++i) {
                            const auto point = ExploreRouteGeometry::pathPoint(path, i);
                            assert(point.x == ExploreRouteGeometry::tileCenter(path.points[i].x));
                            assert(point.y == ExploreRouteGeometry::tileCenter(path.points[i].y));
                        }
                    }
                }
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
        constexpr float scale = AmoledV1::EXPLORE_ROUTE_SCALE;
        constexpr int size = ExploreRouteGeometry::TILE_SIZE * scale;
        constexpr int x = 3 * size, y = 3 * size;
        constexpr uint16_t empty = 0xf81f, actor = 0x07ff;
        canvas.clearClipRect();
        canvas.fillSprite(empty);
        assert(GameAssets::drawExploreTileTo(canvas, tile, x, y, 0, scale));
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
