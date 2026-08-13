#pragma once

#include <cstdint>

#include "core/RoomMovementArea.h"
#include "core/RoomResource.h"
#include "game/HomeActor.h"

class RoomNavigator {
public:
    static constexpr uint8_t CELL_PX = 8;
    static constexpr uint8_t MAX_COLS = 32;
    static constexpr uint8_t MAX_ROWS = 32;
    static constexpr uint16_t MAX_NODES = MAX_COLS * MAX_ROWS;

    struct Bounds {
        float minX;
        float minY;
        float maxX;
        float maxY;
    };

    struct Obstacle {
        bool active = false;
        float x = 0.0f;
        float footY = 0.0f;
        float minSeparation = 0.0f;
    };

    struct Request {
        const RoomResource::Point* polygon = nullptr;
        uint8_t polygonCount = 0;
        Bounds bounds = {};
        Home::Geometry geometry;
        float startX = 0.0f;
        float startY = 0.0f;
        float goalX = 0.0f;
        float goalY = 0.0f;
        bool allowOutsideStart = false;
        Obstacle obstacle;
    };

    struct Scratch {
        int16_t* parent = nullptr;
        uint16_t* queue = nullptr;
        uint16_t capacity = 0;
    };

    static uint16_t requiredNodeCapacity(const Bounds& bounds);
    static bool build(const Request& request, Home::Route& route,
                      const Scratch& scratch);
    static bool segmentKeepsSpacing(float fromX, float fromY,
                                    float toX, float toY,
                                    const Obstacle& obstacle);
};
