#pragma once

#include <cstdint>

#include "game/ExploreMapGenerator.h"
#include "game/ExploreCaveTiles.h"

namespace ExploreRouteGeometry {

constexpr uint16_t TILE_SIZE = 26;

struct WorldPoint {
    float x;
    float y;
};

inline constexpr float tileCenter(uint8_t tile) {
    return tile * TILE_SIZE + TILE_SIZE * 0.5f;
}

// Interior stair portals finish in place instead of walking through the walls.
inline bool interiorStairExit(const ExploreMapGenerator::Map& map,
                              const ExploreMapGenerator::Path& path) {
    if (!path.cellCentered || path.pointCount == 0) return false;
    const auto point = path.points[path.pointCount - 1];
    if (point.x >= ExploreMapGenerator::WIDTH ||
        point.y >= ExploreMapGenerator::HEIGHT) return false;
    const uint16_t tile = map.layers[1]
        [point.y * ExploreMapGenerator::WIDTH + point.x];
    return tile == ExploreCaveTiles::FROST_DOWNWARD_STAIRS ||
           tile == ExploreCaveTiles::FROST_UP_LADDER[1];
}

// Cave doorways are warps. Finish at the authored portal instead of crossing
// the perimeter wall on the way to a screen-edge target.
inline bool interiorPortalExit(const ExploreMapGenerator::Map& map,
                               const ExploreMapGenerator::Path& path) {
    if (map.valleyJourney) return path.interiorExit;
    if (map.areaIndex == ExploreMapGenerator::ANCIENT_WATERFALL_VALLEY_AREA &&
        path.cellCentered && path.pointCount != 0) return true;
    return interiorStairExit(map, path);
}

inline WorldPoint pathPoint(const ExploreMapGenerator::Path& path,
                            uint8_t index) {
    if (path.pointCount == 0) return {0.0f, 0.0f};
    if (index >= path.pointCount) index = path.pointCount - 1;
    const ExploreMapGenerator::Point& point = path.points[index];
    WorldPoint world{tileCenter(point.x), tileCenter(point.y)};
    if (path.pointCount == 1 || path.cellCentered) return world;

    // Roads are two tiles wide. Their centerline therefore sits halfway
    // between the paired cells; at a turn, both axes need that offset.
    bool vertical = false;
    bool horizontal = false;
    if (index > 0) {
        const ExploreMapGenerator::Point& previous = path.points[index - 1];
        vertical = vertical || previous.x == point.x;
        horizontal = horizontal || previous.y == point.y;
    }
    if (index + 1 < path.pointCount) {
        const ExploreMapGenerator::Point& next = path.points[index + 1];
        vertical = vertical || next.x == point.x;
        horizontal = horizontal || next.y == point.y;
    }
    if (vertical) world.x += TILE_SIZE * 0.5f;
    if (horizontal) world.y += TILE_SIZE * 0.5f;
    return world;
}

}  // namespace ExploreRouteGeometry
