#include <cassert>

#include "core/RoomNavigator.h"

void Home::Route::clear() {
    count = 0;
    index = 0;
}

int main() {
    const RoomResource::Point walkArea[] = {
        {10, 10}, {100, 10}, {100, 40},
        {50, 40}, {50, 100}, {10, 100},
    };
    RoomNavigator::Request request;
    request.polygon = walkArea;
    request.polygonCount =
        static_cast<uint8_t>(sizeof(walkArea) / sizeof(walkArea[0]));
    request.bounds = {10.0f, 10.0f, 100.0f, 100.0f};
    request.geometry.groundOffsetY = 0.0f;
    request.geometry.footprint = {2.0f, 2.0f};
    request.startX = 5.0f;
    request.startY = 80.0f;
    request.goalX = 90.0f;
    request.goalY = 20.0f;

    int16_t parent[RoomNavigator::MAX_NODES] = {};
    uint16_t queue[RoomNavigator::MAX_NODES] = {};
    RoomNavigator::Scratch scratch = {
        parent, queue, RoomNavigator::MAX_NODES,
    };
    Home::Route route;

    request.allowOutsideStart = false;
    assert(!RoomNavigator::build(request, route, scratch));

    request.allowOutsideStart = true;
    assert(RoomNavigator::build(request, route, scratch));
    assert(route.count >= 2);
    assert(RoomMovementArea::containsFootprint(
        walkArea, request.polygonCount,
        route.x[0], route.y[0], request.geometry.footprint));
    assert(route.x[route.count - 1] == request.goalX);
    assert(route.y[route.count - 1] == request.goalY);
    return 0;
}
