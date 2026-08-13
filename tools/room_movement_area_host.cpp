#include <cassert>

#include "core/RoomMovementArea.h"

int main() {
    const RoomResource::Point concave[] = {
        {0, 0}, {12, 0}, {12, 12}, {8, 12},
        {8, 4}, {4, 4}, {4, 12}, {0, 12},
    };
    const uint8_t count = static_cast<uint8_t>(
        sizeof(concave) / sizeof(concave[0]));

    assert(RoomMovementArea::containsPoint(concave, count, 2.0f, 8.0f));
    assert(RoomMovementArea::containsPoint(concave, count, 10.0f, 8.0f));
    assert(!RoomMovementArea::containsPoint(concave, count, 6.0f, 8.0f));

    const RoomMovementArea::Footprint compact = {0.5f, 0.5f};
    assert(RoomMovementArea::containsFootprint(
        concave, count, 2.0f, 8.0f, compact));
    assert(!RoomMovementArea::containsFootprint(
        concave, count, 3.8f, 8.0f, compact));

    assert(!RoomMovementArea::segmentInsideFootprint(
        concave, count, 2.0f, 8.0f, 10.0f, 8.0f, compact));
    assert(RoomMovementArea::segmentInsideFootprint(
        concave, count, 2.0f, 8.0f, 2.0f, 2.0f, compact));

    assert(RoomMovementArea::containsPoint(nullptr, 0, 100.0f, 100.0f));
    return 0;
}
