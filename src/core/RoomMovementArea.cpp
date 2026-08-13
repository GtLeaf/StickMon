#include "core/RoomMovementArea.h"

#include <cmath>

namespace RoomMovementArea {

bool containsPoint(const RoomResource::Point* polygon, uint8_t pointCount,
                   float x, float y) {
    if (!polygon || pointCount < 3) return true;

    bool inside = false;
    uint8_t previous = static_cast<uint8_t>(pointCount - 1);
    for (uint8_t current = 0; current < pointCount; ++current) {
        const RoomResource::Point& a = polygon[current];
        const RoomResource::Point& b = polygon[previous];
        bool crosses = ((a.y > y) != (b.y > y)) &&
                       (x < static_cast<float>(b.x - a.x) *
                                    (y - a.y) / (b.y - a.y) + a.x);
        if (crosses) inside = !inside;
        previous = current;
    }
    return inside;
}

bool containsFootprint(const RoomResource::Point* polygon, uint8_t pointCount,
                       float footX, float footY,
                       const Footprint& footprint) {
    float diagonalX = footprint.radiusX * 0.70f;
    float diagonalY = footprint.radiusY * 0.70f;
    return containsPoint(polygon, pointCount, footX, footY) &&
           containsPoint(polygon, pointCount,
                         footX - footprint.radiusX, footY) &&
           containsPoint(polygon, pointCount,
                         footX + footprint.radiusX, footY) &&
           containsPoint(polygon, pointCount,
                         footX, footY - footprint.radiusY) &&
           containsPoint(polygon, pointCount,
                         footX - diagonalX, footY - diagonalY) &&
           containsPoint(polygon, pointCount,
                         footX + diagonalX, footY - diagonalY);
}

bool segmentInsideFootprint(const RoomResource::Point* polygon,
                            uint8_t pointCount,
                            float fromFootX, float fromFootY,
                            float toFootX, float toFootY,
                            const Footprint& footprint,
                            bool allowOutsideStart,
                            float sampleStep) {
    float dx = toFootX - fromFootX;
    float dy = toFootY - fromFootY;
    float distance = std::sqrt(dx * dx + dy * dy);
    if (sampleStep <= 0.0f) sampleStep = 3.0f;
    uint16_t steps = static_cast<uint16_t>(std::ceil(distance / sampleStep));
    if (steps == 0) {
        return containsFootprint(
            polygon, pointCount, toFootX, toFootY, footprint);
    }

    bool entered = containsFootprint(
        polygon, pointCount, fromFootX, fromFootY, footprint);
    if (!entered && !allowOutsideStart) return false;
    for (uint16_t step = 1; step <= steps; ++step) {
        float progress = static_cast<float>(step) / steps;
        bool inside = containsFootprint(
            polygon, pointCount,
            fromFootX + dx * progress,
            fromFootY + dy * progress,
            footprint);
        if (inside) {
            entered = true;
        } else if (entered) {
            return false;
        }
    }
    return entered;
}

}  // namespace RoomMovementArea
