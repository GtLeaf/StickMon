#pragma once

#include <cstdint>

#include "game/ExploreMapGenerator.h"

namespace ExploreRouteGeometry {

constexpr uint16_t TILE_SIZE = 26;

struct WorldPoint {
    float x;
    float y;
};

inline constexpr float tileCenter(uint8_t tile) {
    return tile * TILE_SIZE + TILE_SIZE * 0.5f;
}

inline WorldPoint pathPoint(const ExploreMapGenerator::Path& path,
                            uint8_t index) {
    if (path.pointCount == 0) return {0.0f, 0.0f};
    if (index >= path.pointCount) index = path.pointCount - 1;
    const ExploreMapGenerator::Point& point = path.points[index];
    WorldPoint world{tileCenter(point.x), tileCenter(point.y)};
    if (path.pointCount == 1) return world;

    uint8_t neighborIndex = index == 0 ? 1 : index - 1;
    const ExploreMapGenerator::Point& neighbor = path.points[neighborIndex];
    if (neighbor.x == point.x) world.x += TILE_SIZE * 0.5f;
    return world;
}

}  // namespace ExploreRouteGeometry
