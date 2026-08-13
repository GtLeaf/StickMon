#pragma once

#include <cstdint>

#include "game/ExploreMapGenerator.h"

namespace ExploreIceSlide {

constexpr uint8_t INVALID_INDEX = 0xFF;

inline bool pointIsSmoothIce(const ExploreMapGenerator::Map& map,
                             const ExploreMapGenerator::Point& point) {
    if (point.x >= ExploreMapGenerator::WIDTH ||
        point.y >= ExploreMapGenerator::HEIGHT) {
        return false;
    }
    return ExploreMapGenerator::isSmoothIceTile(
        map.layers[0][point.y * ExploreMapGenerator::WIDTH + point.x]);
}

inline bool indexIsSmoothIce(const ExploreMapGenerator::Map& map,
                             const ExploreMapGenerator::Path& path,
                             uint8_t index) {
    return index < path.pointCount && pointIsSmoothIce(map, path.points[index]);
}

inline bool cardinalDelta(const ExploreMapGenerator::Point& from,
                          const ExploreMapGenerator::Point& to,
                          int8_t& dx, int8_t& dy) {
    int16_t wideDx = static_cast<int16_t>(to.x) - from.x;
    int16_t wideDy = static_cast<int16_t>(to.y) - from.y;
    if ((wideDx == 0 && (wideDy == -1 || wideDy == 1)) ||
        (wideDy == 0 && (wideDx == -1 || wideDx == 1))) {
        dx = static_cast<int8_t>(wideDx);
        dy = static_cast<int8_t>(wideDy);
        return true;
    }
    dx = 0;
    dy = 0;
    return false;
}

inline bool begins(const ExploreMapGenerator::Map& map,
                   const ExploreMapGenerator::Path& path,
                   uint8_t fromIndex, int8_t& dx, int8_t& dy) {
    if (fromIndex + 1 >= path.pointCount ||
        !indexIsSmoothIce(map, path, fromIndex + 1)) {
        return false;
    }
    return cardinalDelta(path.points[fromIndex], path.points[fromIndex + 1], dx, dy);
}

inline bool continues(const ExploreMapGenerator::Map& map,
                      const ExploreMapGenerator::Path& path,
                      uint8_t currentIndex, int8_t dx, int8_t dy) {
    if (!indexIsSmoothIce(map, path, currentIndex) ||
        currentIndex + 1 >= path.pointCount) {
        return false;
    }
    int8_t nextDx = 0;
    int8_t nextDy = 0;
    return cardinalDelta(
               path.points[currentIndex], path.points[currentIndex + 1],
               nextDx, nextDy) &&
           nextDx == dx && nextDy == dy;
}

inline bool routeCrossesIceStraight(const ExploreMapGenerator::Map& map,
                                    const ExploreMapGenerator::Path& path) {
    for (uint8_t index = 0; index < path.pointCount; ++index) {
        if (!indexIsSmoothIce(map, path, index)) continue;
        if (index == 0 || index + 1 >= path.pointCount) return false;
        int8_t incomingDx = 0;
        int8_t incomingDy = 0;
        int8_t outgoingDx = 0;
        int8_t outgoingDy = 0;
        if (!cardinalDelta(path.points[index - 1], path.points[index],
                           incomingDx, incomingDy) ||
            !cardinalDelta(path.points[index], path.points[index + 1],
                           outgoingDx, outgoingDy) ||
            incomingDx != outgoingDx || incomingDy != outgoingDy) {
            return false;
        }
    }
    return true;
}

inline uint8_t nearestNonIceIndex(const ExploreMapGenerator::Map& map,
                                  const ExploreMapGenerator::Path& path,
                                  uint8_t preferred, uint8_t first,
                                  uint8_t last) {
    if (path.pointCount == 0) return INVALID_INDEX;
    if (last >= path.pointCount) last = path.pointCount - 1;
    if (first > last) return INVALID_INDEX;
    if (preferred < first) preferred = first;
    if (preferred > last) preferred = last;
    for (uint8_t distance = 0; distance <= last - first; ++distance) {
        int16_t lower = static_cast<int16_t>(preferred) - distance;
        if (lower >= first &&
            !indexIsSmoothIce(map, path, static_cast<uint8_t>(lower))) {
            return static_cast<uint8_t>(lower);
        }
        uint16_t upper = static_cast<uint16_t>(preferred) + distance;
        if (distance > 0 && upper <= last &&
            !indexIsSmoothIce(map, path, static_cast<uint8_t>(upper))) {
            return static_cast<uint8_t>(upper);
        }
    }
    return INVALID_INDEX;
}

}  // namespace ExploreIceSlide
