#pragma once
#include <algorithm>
#include "presentation/PixelRenderer.h"

namespace FurnitureArt {
inline constexpr int FRAME_WORLD_X = 193, FRAME_WORLD_SIZE = 21;
inline constexpr int WALL_PHOTO_SIZE = 13;
// The upper-left corner lies 5 native (2x) pixels above the wall-aligned position.
inline constexpr float FRAME_ART_Y = 45.5f;

inline int frameTop(int artScale, int displayScale) {
    const int artRow = static_cast<int>(FRAME_ART_Y * artScale + 0.5f);
    // First display row that samples this source row, including on legacy 1x room art.
    return (artRow * displayScale + artScale - 1) / artScale;
}

// Distances are measured inward from a rail on a 112 px reference frame.
inline uint16_t railColor(int depth, bool lit) {
    if (depth < 2) return lit ? PixelRenderer::rgb(142, 90, 53) : PixelRenderer::rgb(94, 59, 36);
    if (depth < 5) return lit ? PixelRenderer::rgb(232, 181, 116) : PixelRenderer::rgb(147, 91, 51);
    if (depth < 10) return lit ? PixelRenderer::rgb(199, 139, 80) : PixelRenderer::rgb(176, 113, 64);
    // The inner bevel turns toward the picture, reversing the lighting.
    if (depth < 13) return lit ? PixelRenderer::rgb(128, 78, 45) : PixelRenderer::rgb(216, 161, 98);
    if (depth < 15) return lit ? PixelRenderer::rgb(102, 66, 42) : PixelRenderer::rgb(156, 105, 65);
    return PixelRenderer::rgb(242, 228, 197);
}

inline uint16_t framePixel(int x, int y, int size) {
    const int lightEdge = std::min(x, y);
    const int darkEdge = size - 1 - std::max(x, y);
    const int depth = std::min(lightEdge, darkEdge) * 112 / size;
    if (depth >= 20) return PixelRenderer::rgb(228, 232, 208);
    return railColor(depth, lightEdge <= darkEdge);
}

inline void drawFrame(Canvas565& canvas, int cx, int cy, int size = 112) {
    if (size <= 0) return;
    const int x = cx - size / 2, y = cy - size / 2;
    const int side = std::max(1, size / 112), bottom = std::max(1, size / 56);
    canvas.fillRect(x + side, y + bottom, size, size, PixelRenderer::rgb(92, 57, 35));
    const int inset = (20 * size + 111) / 112;
    canvas.fillRect(x + inset, y + inset, size - 2 * inset, size - 2 * inset,
                    PixelRenderer::rgb(228, 232, 208));
    for (int i = 0; i < inset; ++i) {
        const int depth = i * 112 / size, span = size - 2 * i;
        const uint16_t light = railColor(depth, true), dark = railColor(depth, false);
        canvas.drawFastHLine(x + i, y + size - 1 - i, span, dark);
        canvas.drawFastVLine(x + size - 1 - i, y + i, span, dark);
        canvas.drawFastHLine(x + i, y + i, span, light);
        canvas.drawFastVLine(x + i, y + i, span, light);
    }
}
}
