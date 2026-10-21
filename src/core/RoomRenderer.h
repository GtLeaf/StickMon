#pragma once

#include <cstdint>

#ifndef STICKMON_ROOM_PIXEL_SCALE
#define STICKMON_ROOM_PIXEL_SCALE 1
#endif

namespace RoomRenderer {

static constexpr uint8_t PIXEL_SCALE = STICKMON_ROOM_PIXEL_SCALE;
static_assert(PIXEL_SCALE > 0, "Room pixel scale must be positive");

struct Viewport {
    int16_t x, y;
    uint16_t width, height;
};

// Center rooms smaller than the screen without stretching their pixels.
Viewport viewport();
// World anchors and room sprites share this scale; UI keeps native pixels.
int16_t clampCameraX(float cameraX);
int16_t clampCameraY(float cameraY);
int16_t screenX(float worldX, float cameraX = 0);
int16_t screenY(float worldY, float cameraY);
bool draw(float cameraY, bool night, float cameraX = 0);
bool drawViewport(int16_t destinationX, int16_t destinationY,
                  uint16_t viewportWidth, uint16_t viewportHeight,
                  int16_t cameraX, int16_t cameraY, bool night,
                  // Scale of world pixels on the target surface. The room
                  // package may have a separate native art scale.
                  uint8_t pixelScale = 1);

}  // namespace RoomRenderer
