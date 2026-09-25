#pragma once

#include <cstdint>

namespace RoomRenderer {

bool draw(float cameraY, bool night);
bool drawViewport(int16_t destinationX, int16_t destinationY,
                  uint16_t viewportWidth, uint16_t viewportHeight,
                  int16_t cameraX, int16_t cameraY, bool night,
                  // Scale of world pixels on the target surface. The room
                  // package may have a separate native art scale.
                  uint8_t pixelScale = 1);

}  // namespace RoomRenderer
