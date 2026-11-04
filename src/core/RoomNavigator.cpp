#include "core/RoomNavigator.h"

#include <algorithm>
#include <cmath>

uint16_t RoomNavigator::requiredNodeCapacity(const Bounds& bounds) {
    float width = bounds.maxX - bounds.minX;
    float height = bounds.maxY - bounds.minY;
    if (width < 0.0f || height < 0.0f) return 0;
    uint16_t cols = static_cast<uint16_t>(
        std::ceil(width / CELL_PX) + 1.0f);
    uint16_t rows = static_cast<uint16_t>(
        std::ceil(height / CELL_PX) + 1.0f);
    if (cols < 2 || rows < 2 || cols > MAX_COLS || rows > MAX_ROWS) {
        return 0;
    }
    return static_cast<uint16_t>(cols * rows);
}

bool RoomNavigator::segmentKeepsSpacing(
    float fromX, float fromY, float toX, float toY,
    const Obstacle& obstacle) {
    if (!obstacle.active || obstacle.halfWidth <= 0.0f ||
        obstacle.halfHeight <= 0.0f) return true;

    auto inside = [&](float x, float y) {
        return std::fabs(x - obstacle.x) < obstacle.halfWidth &&
               std::fabs(y - obstacle.footY) < obstacle.halfHeight;
    };
    // Allow an actor that was restored inside another footprint to leave it;
    // otherwise the two actors can remain permanently deadlocked.
    if (inside(fromX, fromY)) return !inside(toX, toY);

    const float dx = toX - fromX;
    const float dy = toY - fromY;
    float tMin = 0.0f;
    float tMax = 1.0f;
    const float delta[2] = {dx, dy};
    const float start[2] = {fromX, fromY};
    const float center[2] = {obstacle.x, obstacle.footY};
    const float extent[2] = {obstacle.halfWidth, obstacle.halfHeight};
    for (uint8_t axis = 0; axis < 2; ++axis) {
        if (std::fabs(delta[axis]) <= 0.0001f) {
            if (std::fabs(start[axis] - center[axis]) >= extent[axis]) {
                return true;
            }
            continue;
        }
        float nearT = (center[axis] - extent[axis] - start[axis]) /
                      delta[axis];
        float farT = (center[axis] + extent[axis] - start[axis]) /
                     delta[axis];
        if (nearT > farT) std::swap(nearT, farT);
        tMin = std::max(tMin, nearT);
        tMax = std::min(tMax, farT);
        if (tMin > tMax) return true;
    }
    // Exact edge contact is valid; only a segment through the open rectangle
    // is blocked.
    return !(tMax > tMin && tMax > 0.0f && tMin < 1.0f);
}

bool RoomNavigator::build(const Request& request, Home::Route& route,
                          const Scratch& scratch) {
    route.clear();
    const uint16_t nodeCount = requiredNodeCapacity(request.bounds);
    if (nodeCount == 0 || !scratch.parent || !scratch.queue ||
        scratch.capacity < nodeCount) {
        return false;
    }

    float offsetY = request.geometry.groundOffsetY;
    uint8_t cols = static_cast<uint8_t>(
        std::ceil((request.bounds.maxX - request.bounds.minX) / CELL_PX) +
        1.0f);
    uint8_t rows = static_cast<uint8_t>(nodeCount / cols);
    auto pointAllowed = [&](float x, float y) {
        if (!RoomMovementArea::containsFootprint(
                request.polygon, request.polygonCount,
                x, y + offsetY, request.geometry.footprint)) {
            return false;
        }
        if (!request.obstacle.active) return true;
        float dx = x - request.obstacle.x;
        float dy = y + offsetY - request.obstacle.footY;
        return std::fabs(dx) >= request.obstacle.halfWidth ||
               std::fabs(dy) >= request.obstacle.halfHeight;
    };
    auto segmentAllowed = [&](float fromX, float fromY,
                              float toX, float toY,
                              bool allowOutsideStart = false) {
        return RoomMovementArea::segmentInsideFootprint(
                   request.polygon, request.polygonCount,
                   fromX, fromY + offsetY,
                   toX, toY + offsetY,
                   request.geometry.footprint, allowOutsideStart) &&
               segmentKeepsSpacing(
                   fromX, fromY + offsetY,
                   toX, toY + offsetY, request.obstacle);
    };

    if (!pointAllowed(request.goalX, request.goalY)) return false;
    if (segmentAllowed(request.startX, request.startY,
                       request.goalX, request.goalY,
                       request.allowOutsideStart)) {
        route.x[0] = request.goalX;
        route.y[0] = request.goalY;
        route.count = 1;
        return true;
    }

    auto nodeX = [&](uint16_t node) {
        return request.bounds.minX +
               static_cast<float>(node % cols) * CELL_PX;
    };
    auto nodeY = [&](uint16_t node) {
        return request.bounds.minY +
               static_cast<float>(node / cols) * CELL_PX;
    };

    int16_t startNode = -1;
    int16_t goalNode = -1;
    float bestStart = 1000000.0f;
    float bestGoal = 1000000.0f;
    for (uint16_t node = 0; node < nodeCount; ++node) {
        float x = nodeX(node);
        float y = nodeY(node);
        if (!pointAllowed(x, y)) continue;
        float startDx = x - request.startX;
        float startDy = y - request.startY;
        float startDist = startDx * startDx + startDy * startDy;
        if (startDist < bestStart &&
            segmentAllowed(request.startX, request.startY, x, y,
                           request.allowOutsideStart)) {
            bestStart = startDist;
            startNode = static_cast<int16_t>(node);
        }
        float goalDx = x - request.goalX;
        float goalDy = y - request.goalY;
        float goalDist = goalDx * goalDx + goalDy * goalDy;
        if (goalDist < bestGoal &&
            segmentAllowed(x, y, request.goalX, request.goalY)) {
            bestGoal = goalDist;
            goalNode = static_cast<int16_t>(node);
        }
    }
    if (startNode < 0 || goalNode < 0) return false;

    for (uint16_t node = 0; node < nodeCount; ++node) {
        scratch.parent[node] = -2;
    }
    uint16_t read = 0;
    uint16_t write = 0;
    scratch.queue[write++] = static_cast<uint16_t>(startNode);
    scratch.parent[startNode] = -1;
    static constexpr int8_t DIRECTIONS[8][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1},
    };
    while (read < write && scratch.parent[goalNode] == -2) {
        uint16_t node = scratch.queue[read++];
        int col = node % cols;
        int row = node / cols;
        float fromX = nodeX(node);
        float fromY = nodeY(node);
        for (const auto& direction : DIRECTIONS) {
            int nextCol = col + direction[0];
            int nextRow = row + direction[1];
            if (nextCol < 0 || nextCol >= cols ||
                nextRow < 0 || nextRow >= rows) {
                continue;
            }
            uint16_t next = static_cast<uint16_t>(nextRow * cols + nextCol);
            if (scratch.parent[next] != -2) continue;
            float nextX = nodeX(next);
            float nextY = nodeY(next);
            if (!pointAllowed(nextX, nextY) ||
                !segmentAllowed(fromX, fromY, nextX, nextY)) {
                continue;
            }
            scratch.parent[next] = static_cast<int16_t>(node);
            if (write < scratch.capacity) scratch.queue[write++] = next;
        }
    }
    if (scratch.parent[goalNode] == -2) return false;

    uint16_t pathCount = 0;
    for (int16_t node = goalNode;
         node >= 0 && pathCount < scratch.capacity;
         node = scratch.parent[node]) {
        scratch.queue[pathCount++] = static_cast<uint16_t>(node);
    }
    if (pathCount == 0) return false;

    float fromX = request.startX;
    float fromY = request.startY;
    int cursor = static_cast<int>(pathCount) - 1;
    if (request.allowOutsideStart) {
        uint16_t ingress = scratch.queue[cursor];
        fromX = nodeX(ingress);
        fromY = nodeY(ingress);
        route.x[route.count] = fromX;
        route.y[route.count] = fromY;
        ++route.count;
    }
    while (cursor > 0 && route.count + 1 < Home::ROUTE_CAP) {
        int selected = cursor - 1;
        for (int candidate = 0; candidate < cursor; ++candidate) {
            uint16_t node = scratch.queue[candidate];
            if (segmentAllowed(
                    fromX, fromY, nodeX(node), nodeY(node),
                    false)) {
                selected = candidate;
                break;
            }
        }
        uint16_t node = scratch.queue[selected];
        fromX = nodeX(node);
        fromY = nodeY(node);
        route.x[route.count] = fromX;
        route.y[route.count] = fromY;
        ++route.count;
        cursor = selected;
    }
    if (!segmentAllowed(fromX, fromY,
                        request.goalX, request.goalY,
                        false) ||
        route.count >= Home::ROUTE_CAP) {
        route.clear();
        return false;
    }
    route.x[route.count] = request.goalX;
    route.y[route.count] = request.goalY;
    ++route.count;
    return true;
}
