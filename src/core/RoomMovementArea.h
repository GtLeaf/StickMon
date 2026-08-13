#pragma once

#include <cstdint>

#include "core/RoomResource.h"

namespace RoomMovementArea {

struct Footprint {
    float radiusX;
    float radiusY;
};

bool containsPoint(const RoomResource::Point* polygon, uint8_t pointCount,
                   float x, float y);
bool containsFootprint(const RoomResource::Point* polygon, uint8_t pointCount,
                       float footX, float footY,
                       const Footprint& footprint);
bool segmentInsideFootprint(const RoomResource::Point* polygon,
                            uint8_t pointCount,
                            float fromFootX, float fromFootY,
                            float toFootX, float toFootY,
                            const Footprint& footprint,
                            bool allowOutsideStart = false,
                            float sampleStep = 3.0f);

}  // namespace RoomMovementArea
