#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "game/ExploreIceSlide.h"
#include "game/ExploreRouteGeometry.h"

namespace {

using ExploreMapGenerator::Map;
using ExploreMapGenerator::Path;
using ExploreMapGenerator::Point;

void setIce(Map& map, uint8_t x, uint8_t y) {
    map.layers[0][y * ExploreMapGenerator::WIDTH + x] = 4504;
}

Path makeStraightPath() {
    Path path;
    path.pointCount = 6;
    for (uint8_t index = 0; index < path.pointCount; ++index) {
        path.points[index] = {static_cast<uint8_t>(index + 1), 4};
    }
    return path;
}

void checkGeneratedCaveFootprints() {
    using namespace ExploreMapGenerator;
    unsigned maps = 0;
    unsigned samples = 0;
    for (unsigned seed = 0; seed < 256; ++seed) {
        for (unsigned edge = 0; edge < 4; ++edge) {
            for (uint8_t level = 0; level < 3; ++level) {
                for (bool ladder : {false, true}) {
                    Map map;
                    assert(generate(seed, static_cast<Edge>(edge),
                        FROST_CRYSTAL_CAVE_AREA, map, {level, 3, ladder}));
                    ++maps;
                    for (uint8_t p = 0; p < map.pathCount; ++p) {
                        const Path& path = map.paths[p];
                        assert(path.cellCentered);
                        assert(ExploreIceSlide::routeCrossesIceStraight(map, path));
                        for (uint8_t i = 0; i < path.pointCount; ++i) {
                            const auto a = ExploreRouteGeometry::pathPoint(path, i);
                            assert(a.x == path.points[i].x * 26 + 13);
                            assert(a.y == path.points[i].y * 26 + 13);
                            const auto b = ExploreRouteGeometry::pathPoint(path, i + 1);
                            // Cover both Stick's ground anchor and AMOLED's
                            // additional nine world pixels, including transit.
                            for (int footOffset : {0, 9}) {
                                for (int step = 0; step <= 8; ++step) {
                                    const int x = static_cast<int>((a.x + (b.x - a.x) * step / 8) / 26);
                                    const int y = static_cast<int>((a.y + (b.y - a.y) * step / 8 + footOffset) / 26);
                                    assert(x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT);
                                    const int cell = y * WIDTH + x;
                                    const uint16_t ground = map.layers[0][cell];
                                    assert(ground == 4511 || ground == 4501 ||
                                        (ground >= 4780 && ground <= 4794) ||
                                        isSmoothIceTile(ground));
                                    for (uint8_t layer = 1; layer < LAYER_COUNT; ++layer) {
                                        const uint16_t tile = map.layers[layer][cell];
                                        assert(tile == 0 || (tile >= 4741 && tile <= 4773));
                                    }
                                    ++samples;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Path outdoor = makeStraightPath();
    const auto outdoorCenter = ExploreRouteGeometry::pathPoint(outdoor, 2);
    assert(outdoorCenter.x == 3 * 26 + 13 && outdoorCenter.y == 5 * 26);
    std::printf("frost geometry: %u maps, %u safe movement samples\n", maps, samples);
}

}  // namespace

int main() {
    Map map;
    Path path = makeStraightPath();
    setIce(map, 2, 4);
    setIce(map, 3, 4);
    setIce(map, 4, 4);

    int8_t dx = 0;
    int8_t dy = 0;
    assert(ExploreIceSlide::begins(map, path, 0, dx, dy));
    assert(dx == 1 && dy == 0);
    assert(ExploreIceSlide::continues(map, path, 1, dx, dy));
    assert(ExploreIceSlide::continues(map, path, 3, dx, dy));
    assert(!ExploreIceSlide::continues(map, path, 4, dx, dy));
    assert(ExploreIceSlide::routeCrossesIceStraight(map, path));

    path.points[3] = {3, 5};
    assert(!ExploreIceSlide::routeCrossesIceStraight(map, path));

    path = makeStraightPath();
    assert(ExploreIceSlide::nearestNonIceIndex(map, path, 2, 1, 4) == 4);
    assert(ExploreIceSlide::nearestNonIceIndex(map, path, 3, 2, 5) == 4);
    assert(ExploreIceSlide::nearestNonIceIndex(map, path, 2, 1, 3) ==
           ExploreIceSlide::INVALID_INDEX);

    map.layers[0][4 * ExploreMapGenerator::WIDTH + 5] = 4506;
    assert(ExploreIceSlide::isCrackedIce(map, path, 4));
    assert(ExploreIceSlide::nearestNonIceIndex(map, path, 4, 4, 5) == 5);
    ExploreIceSlide::breakIce(map, path, 4);
    assert(map.layers[0][4 * ExploreMapGenerator::WIDTH + 5] ==
           ExploreCaveTiles::FROST_BROKEN_ICE_HOLE);
    map.layers[0][4 * ExploreMapGenerator::WIDTH + 6] = 4511;
    assert(ExploreIceSlide::landingIndex(map, path, 7) == 0);

    Path landingPath = makeStraightPath();
    for (uint8_t index = 1; index < 4; ++index) {
        map.layers[0][4 * ExploreMapGenerator::WIDTH + index + 1] = 4511;
    }
    assert(ExploreIceSlide::landingIndex(map, landingPath, 7) != 0);
    for (uint16_t tile = 4780; tile <= 4794; ++tile) {
        for (uint8_t index = 1; index < 4; ++index)
            map.layers[0][4 * ExploreMapGenerator::WIDTH + index + 1] = tile;
        assert(ExploreIceSlide::landingIndex(map, landingPath, 7) != 0);
    }
    checkGeneratedCaveFootprints();
    return 0;
}
