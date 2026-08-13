#include <cassert>

#include "game/ExploreIceSlide.h"

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
    return 0;
}
